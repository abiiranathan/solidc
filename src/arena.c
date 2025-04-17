#include "../include/arena.h"
#include "../include/aligned_alloc.h"

#include <stdatomic.h>
#include <stdbool.h>
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
    _Atomic size_t used;                                       // Isolated in its own cache line
    char _padding1[CACHE_LINE_SIZE - sizeof(_Atomic size_t)];  // Pad to fill the cache line
    size_t size;                                               // Arena size (immutable after creation)
    char* base;                                                // Memory buffer (immutable after creation)
    bool owns_memory;                                          // Indicates if the arena owns the memory
} Arena;

typedef struct AllocHeader {
    size_t size;  // Size of the allocation (excluding header)
} AllocHeader;

// Thread-local allocation state
typedef struct {
    char* current;
    size_t remaining;
} ThreadBlock;

static _Thread_local ThreadBlock tl_block = {0, 0};

// Helper function to align sizes and addresses
static inline size_t arena_align_up(size_t n, size_t alignment) {
    return (n + alignment - 1) & ~(alignment - 1);
}

Arena* arena_create(size_t arena_size) {
    if (UNLIKELY(arena_size < ARENA_MIN_SIZE)) {
        arena_size = ARENA_MIN_SIZE;
    }

    arena_size   = arena_align_up(arena_size, ARENA_ALIGNMENT);
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

Arena* arena_create_from_buffer(void* buffer, size_t buffer_size) {
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
    } while (!atomic_compare_exchange_weak_explicit(
        &arena->used, &current, offset, memory_order_release, memory_order_relaxed));

    return true;
}

void* arena_alloc(Arena* arena, size_t size) {
    if (UNLIKELY(!arena || !arena->base || size == 0)) {
        return NULL;
    }

    size_t header_size  = arena_align_up(sizeof(AllocHeader), ARENA_ALIGNMENT);
    size_t aligned_size = header_size + arena_align_up(size, ARENA_ALIGNMENT);

    // Fast path: try thread-local block first
    if (LIKELY(tl_block.remaining >= aligned_size)) {
        char* ptr           = tl_block.current;
        AllocHeader* header = (AllocHeader*)ptr;
        header->size        = size;
        tl_block.current += aligned_size;
        tl_block.remaining -= aligned_size;
        return ptr + header_size;
    }

    // Reserve a new block from the arena
    size_t reserve_size =
        aligned_size > THREAD_BLOCK_SIZE ? arena_align_up(aligned_size, ARENA_ALIGNMENT) : THREAD_BLOCK_SIZE;

    size_t old_used = atomic_fetch_add_explicit(&arena->used, reserve_size, memory_order_acq_rel);
    if (UNLIKELY(old_used + reserve_size > arena->size)) {
        // Try one last attempt using the exact aligned size
        reserve_size = arena_align_up(aligned_size, ARENA_ALIGNMENT);
        old_used     = atomic_fetch_add_explicit(&arena->used, reserve_size, memory_order_acq_rel);
        if (UNLIKELY(old_used + reserve_size > arena->size)) {
            return NULL;  // Out of memory
        }
    }

    // Assign new block to thread-local
    if (LIKELY(old_used < arena->size)) {
        tl_block.current   = arena->base + old_used;
        tl_block.remaining = reserve_size;
    } else {
        return NULL;  // Failsafe check
    }

    // Allocate from the new thread-local block
    char* ptr           = tl_block.current;
    AllocHeader* header = (AllocHeader*)ptr;
    header->size        = size;
    tl_block.current += aligned_size;
    tl_block.remaining -= aligned_size;
    return ptr + header_size;
}

void* arena_realloc(Arena* arena, void* ptr, size_t new_size) {
    if (UNLIKELY(arena == NULL || new_size == 0)) {
        return NULL;
    }

    // behave like realloc.
    if (UNLIKELY(ptr == NULL)) {
        return arena_alloc(arena, new_size);
    }

    size_t header_size  = arena_align_up(sizeof(AllocHeader), ARENA_ALIGNMENT);
    AllocHeader* header = (AllocHeader*)((char*)ptr - header_size);
    size_t old_size     = header->size;

    void* new_ptr = arena_alloc(arena, new_size);
    if (UNLIKELY(new_ptr == NULL)) {
        return NULL;  // Allocation failed
    }

    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    return new_ptr;
}

char* arena_alloc_string(Arena* arena, const char* str) {
    if (UNLIKELY(!arena || !str)) {
        return NULL;
    }

    size_t len = strlen(str);
    if (UNLIKELY(len >= SIZE_MAX)) {
        return NULL;
    }

    char* s = (char*)arena_alloc(arena, len + 1);
    if (UNLIKELY(!s)) {
        return NULL;
    }

    memcpy(s, str, len);
    s[len] = '\0';
    return s;
}

int* arena_alloc_int(Arena* arena, int n) {
    if (UNLIKELY(!arena)) {
        return NULL;
    }

    int* number = (int*)arena_alloc(arena, sizeof(int));
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
    size_t used = atomic_load_explicit(&arena->used, memory_order_acquire);
    return arena->size - used;
}

bool arena_can_fit(Arena* arena, size_t size) {
    if (!arena || size == 0) {
        return false;
    }

    size_t aligned_size = arena_align_up(size, ARENA_ALIGNMENT);
    // Relaxed is fine here as we just need a snapshot
    size_t current_used = atomic_load_explicit(&arena->used, memory_order_relaxed);
    return (current_used <= arena->size && aligned_size <= arena->size - current_used);
}

size_t arena_size(Arena* arena) {
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
bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]) {
    if (UNLIKELY(!arena || !sizes || count == 0 || !out_ptrs)) {
        return false;
    }

    size_t header_size  = arena_align_up(sizeof(AllocHeader), ARENA_ALIGNMENT);
    size_t total_size   = 0;
    size_t* alloc_sizes = arena_alloc(arena, count * sizeof(size_t));
    if (UNLIKELY(alloc_sizes == NULL)) {
        return false;
    }

    // Calculate the aligned block size for each allocation and sum them.
    for (size_t i = 0; i < count; i++) {
        size_t asize   = arena_align_up(sizes[i], ARENA_ALIGNMENT);
        alloc_sizes[i] = header_size + asize;
        total_size += alloc_sizes[i];
    }

    total_size = arena_align_up(total_size, ARENA_ALIGNMENT);

    // Reserve the total_size from the arena in one atomic operation.
    size_t old_used, new_used;
    do {
        old_used = atomic_load_explicit(&arena->used, memory_order_relaxed);
        new_used = old_used + total_size;
        if (new_used > arena->size) {
            return false;  // Not enough space in the arena
        }
    } while (!atomic_compare_exchange_weak_explicit(
        &arena->used, &old_used, new_used, memory_order_release, memory_order_relaxed));

    char* batch_ptr = arena->base + old_used;
    char* current   = batch_ptr;

    // Partition the reserved block for each allocation
    for (size_t i = 0; i < count; i++) {
        AllocHeader* header = (AllocHeader*)current;
        header->size        = sizes[i];
        // The pointer returned to the caller skips past the header.
        out_ptrs[i] = current + header_size;
        current += alloc_sizes[i];
    }
    return true;
}
