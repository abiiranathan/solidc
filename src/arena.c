#include "../include/arena.h"
#include "../include/aligned_alloc.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__  // Covers GCC and Clang
#define LIKELY(expr) __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
#define LIKELY(expr) (expr)
#define UNLIKELY(expr) (expr)
#endif

// Separate 'used' into its own cache line to prevent false sharing
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) Arena {
    atomic_size_t used;                                       // Isolated in its own cache line
    char _padding1[CACHE_LINE_SIZE - sizeof(atomic_size_t)];  // Pad to fill the cache line
    size_t size;                                              // Arena size (immutable after creation)
    char* base;                                               // Memory buffer (immutable after creation)
    bool owns_memory;                                         // Indicates if the arena owns the memory
} Arena;

typedef struct AllocHeader {
    size_t size;  // Size of the allocation (excluding header)
} AllocHeader;

// Thread-local allocation state
typedef struct {
    char* current;
    size_t remaining;
} ThreadBlock;

static _Thread_local ThreadBlock tl_block = {NULL, 0};

#define ALIGNMENT_MASK (ARENA_ALIGNMENT - 1)

// Helper function to align sizes and addresses
static size_t arena_align_up(const size_t n) {
    return (n + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;
}

Arena* arena_create(size_t arena_size) {
    if (UNLIKELY(arena_size < ARENA_MIN_SIZE)) {
        arena_size = ARENA_MIN_SIZE;
    }

    arena_size   = arena_align_up(arena_size);
    Arena* arena = malloc(sizeof(Arena));
    if (UNLIKELY(arena == NULL)) {
        perror("malloc arena");
        return NULL;
    }

    char* memory = ALIGNED_ALLOC(ARENA_ALIGNMENT, arena_size);
    if (UNLIKELY(memory == NULL)) {
        perror("aligned_alloc");
        free(arena);
        return NULL;
    }

    atomic_init(&arena->used, 0);
    arena->size        = arena_size;
    arena->base        = memory;
    arena->owns_memory = true;
    return arena;
}

Arena* arena_create_from_buffer(void* buffer, const size_t buffer_size) {
    if (UNLIKELY(!buffer || buffer_size < ARENA_MIN_SIZE)) {
        return NULL;
    }

    if (UNLIKELY((uintptr_t)buffer % ARENA_ALIGNMENT != 0)) {
        printf("arena_create_from_buffer(): buffer %p is not properly aligned\n", (void*)buffer);
        return NULL;
    }

    Arena* arena = malloc(sizeof(Arena));
    if (UNLIKELY(arena == NULL)) {
        perror("malloc arena");
        return NULL;
    }

    atomic_init(&arena->used, 0);
    arena->size        = buffer_size;
    arena->base        = buffer;
    arena->owns_memory = false;
    return arena;
}

void arena_destroy(Arena* arena) {
    if (UNLIKELY(arena == NULL)) {
        return;
    }

    // No concurrent access after this point is safe
    if (arena->owns_memory) {
        ALIGNED_FREE(arena->base);
    }

    // Reset thread-local state (if applicable)
    // This assumes that thread-local blocks are tied to the arena and should be invalidated.
    // If thread-local blocks are independent, this step may not be necessary.
    tl_block.current   = NULL;
    tl_block.remaining = 0;
    free(arena);
}

void arena_reset(Arena* arena) {
    if (UNLIKELY(arena == NULL)) {
        return;
    }

    tl_block.current   = NULL;
    tl_block.remaining = 0;

    // Release semantics to ensure all prior writes are visible
    atomic_store_explicit(&arena->used, 0, memory_order_release);
}

size_t arena_get_offset(Arena* arena) {
    if (UNLIKELY(arena == NULL)) {
        return 0;
    }

    // Acquire semantics to synchronize with allocations
    return atomic_load_explicit(&arena->used, memory_order_acquire);
}

bool arena_restore(Arena* arena, size_t offset) {
    if (UNLIKELY(arena == NULL)) {
        return false;
    }

    size_t current;
    do {
        current = atomic_load_explicit(&arena->used, memory_order_relaxed);
        if (offset > current) {
            return false;
        }
        // Compare-and-swap with release semantics
    } while (!atomic_compare_exchange_weak_explicit(&arena->used, &current, offset, memory_order_release,
                                                    memory_order_relaxed));

    return true;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void* arena_alloc(Arena* arena, size_t size) {
    if (UNLIKELY(!arena || !arena->base || size == 0)) {
        return NULL;
    }

    const size_t header_size  = arena_align_up(sizeof(AllocHeader));
    const size_t aligned_size = header_size + arena_align_up(size);

    // Fast path: try thread-local block first
    if (LIKELY(tl_block.remaining >= aligned_size)) {
        char* ptr    = tl_block.current;
        auto header  = (AllocHeader*)ptr;
        header->size = size;
        tl_block.current += aligned_size;
        tl_block.remaining -= aligned_size;
        return ptr + header_size;
    }

    // Reserve a new block from the arena. Cap the reserve_size at arena->size.
    size_t reserve_size = aligned_size > THREAD_BLOCK_SIZE ? arena_align_up(aligned_size) : THREAD_BLOCK_SIZE;
    reserve_size        = MIN(reserve_size, arena->size);

    size_t old_used = atomic_fetch_add_explicit(&arena->used, reserve_size, memory_order_acq_rel);
    if (UNLIKELY(old_used + reserve_size > arena->size)) {
        // Try one last attempt using the exact aligned size
        reserve_size = arena_align_up(aligned_size);
        old_used     = atomic_fetch_add_explicit(&arena->used, reserve_size, memory_order_acq_rel);
        if (UNLIKELY(old_used + reserve_size > arena->size)) {
            // Roll back the atomic update if we can't allocate
            atomic_fetch_sub_explicit(&arena->used, reserve_size, memory_order_acq_rel);
            return NULL;  // Out of memory
        }
    }

    // Assign new block to thread-local
    tl_block.current   = arena->base + old_used;
    tl_block.remaining = reserve_size;

    // Allocate from the new thread-local block
    char* ptr    = tl_block.current;
    auto header  = (AllocHeader*)ptr;
    header->size = size;
    tl_block.current += aligned_size;
    tl_block.remaining -= aligned_size;
    return ptr + header_size;
}

void* arena_realloc(Arena* arena, void* ptr, size_t new_size) {
    if (UNLIKELY(!arena || new_size == 0))
        return NULL;
    if (UNLIKELY(!ptr))
        return arena_alloc(arena, new_size);

    size_t header_size  = arena_align_up(sizeof(AllocHeader));
    AllocHeader* header = (AllocHeader*)((char*)ptr - header_size);
    size_t old_size     = header->size;

    void* new_ptr = arena_alloc(arena, new_size);
    if (UNLIKELY(!new_ptr))
        return NULL;

    size_t copy_size = MIN(old_size, new_size);
    memcpy(new_ptr, ptr, copy_size);
    return new_ptr;
}

char* arena_alloc_string(Arena* arena, const char* str) {
    if (UNLIKELY(!arena || !str)) {
        return NULL;
    }

    const size_t len = strlen(str);
    if (UNLIKELY(len >= SIZE_MAX)) {
        return NULL;
    }

    char* s = arena_alloc(arena, len + 1);
    if (UNLIKELY(!s)) {
        return NULL;
    }

    memcpy(s, str, len);
    s[len] = '\0';
    return s;
}

int* arena_alloc_int(Arena* arena, const int n) {
    if (UNLIKELY(!arena)) {
        return NULL;
    }

    int* number = arena_alloc(arena, sizeof(int));
    if (UNLIKELY(!number)) {
        return NULL;
    }

    *number = n;
    return number;
}

size_t arena_remaining(Arena* arena) {
    if (!arena) {
        return 0;
    }

    // Acquire semantics to get latest used value
    const size_t used = atomic_load_explicit(&arena->used, memory_order_acquire);
    return arena->size - used;
}

bool arena_can_fit(Arena* arena, const size_t size) {
    if (!arena || size == 0) {
        return false;
    }

    const size_t aligned_size = arena_align_up(size);
    // Relaxed is fine here as we just need a snapshot
    const size_t current_used = atomic_load_explicit(&arena->used, memory_order_relaxed);
    return current_used <= arena->size && aligned_size <= arena->size - current_used;
}

size_t arena_size(const Arena* arena) {
    return arena ? arena->size : 0;
}

size_t arena_used(Arena* arena) {
    if (!arena) {
        return 0;
    }
    // Acquire semantics to get latest value
    return atomic_load_explicit(&arena->used, memory_order_acquire);
}

/*
 *
 * This function accepts an array 'sizes' containing the requested sizes for each allocation and a 'count'
 * indicating how many allocations the caller wants. It uses one atomic update to reserve a contiguous
 * block of memory sufficient to hold all the allocations (each with its header). It then partitions that
 * block, writes the headers, and returns the pointer to each allocation in the out_ptrs array.
 *
 * Returns:
 *   true  - if all allocations succeeded.
 *   false - if there was insufficient memory in the arena.
 */
bool arena_alloc_batch(Arena* arena, const size_t sizes[], const size_t count, void* out_ptrs[]) {
    if (UNLIKELY(!arena || !sizes || count == 0 || !out_ptrs)) {
        return false;
    }

    const size_t header_size = arena_align_up(sizeof(AllocHeader));
    size_t total_size        = 0;
    size_t* alloc_sizes      = arena_alloc(arena, count * sizeof(size_t));
    if (UNLIKELY(alloc_sizes == NULL)) {
        return false;
    }

    // Calculate the aligned block size for each allocation and sum them.
    for (size_t i = 0; i < count; i++) {
        const size_t size = arena_align_up(sizes[i]);
        alloc_sizes[i]    = header_size + size;
        total_size += alloc_sizes[i];
    }

    total_size = arena_align_up(total_size);

    // Reserve the total_size from the arena in one atomic operation.
    size_t old_used, new_used;
    do {
        old_used = atomic_load_explicit(&arena->used, memory_order_relaxed);
        new_used = old_used + total_size;
        if (new_used > arena->size) {
            return false;  // Not enough space in the arena
        }
    } while (!atomic_compare_exchange_weak_explicit(&arena->used, &old_used, new_used, memory_order_release,
                                                    memory_order_relaxed));

    char* batch_ptr = arena->base + old_used;
    char* current   = batch_ptr;

    // Partition the reserved block for each allocation
    for (size_t i = 0; i < count; i++) {
        auto header  = (AllocHeader*)current;
        header->size = sizes[i];
        // The pointer returned to the caller skips past the header.
        out_ptrs[i] = current + header_size;
        current += alloc_sizes[i];
    }
    return true;
}
