#include "../include/arena.h"
#include "../include/aligned_alloc.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__  // Covers GCC and Clang
#define LIKELY(expr)         __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr)       __builtin_expect(!!(expr), 0)
#define PREFETCH_READ(addr)  __builtin_prefetch(addr, 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch(addr, 1, 3)
#else
#define LIKELY(expr)         (expr)
#define UNLIKELY(expr)       (expr)
#define PREFETCH_READ(addr)  ((void)0)
#define PREFETCH_WRITE(addr) ((void)0)
#endif

#define ALIGNMENT_MASK ((size_t)(ARENA_ALIGNMENT - 1))

static inline size_t arena_align_up(size_t n) {
    return (n + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;
}

typedef struct Arena {
    atomic_size_t head;  // Current allocation offset
    size_t size;         // Total arena size
    char* base;          // Memory buffer
    bool owns_memory;    // Memory ownership flag
} Arena;

Arena* arena_create(size_t arena_size) {
    if (UNLIKELY(arena_size < ARENA_MIN_SIZE)) {
        arena_size = ARENA_MIN_SIZE;
    }

    arena_size   = arena_align_up(arena_size);
    Arena* arena = malloc(sizeof(Arena));
    if (UNLIKELY(!arena)) {
        perror("malloc arena");
        return NULL;
    }

    char* memory = ALIGNED_ALLOC(ARENA_ALIGNMENT, arena_size);
    if (UNLIKELY(!memory)) {
        perror("aligned_alloc");
        free(arena);
        return NULL;
    }

    atomic_init(&arena->head, 0);
    arena->size        = arena_size;
    arena->base        = memory;
    arena->owns_memory = true;

    // Prefetch the beginning of the arena for immediate allocations
    PREFETCH_WRITE(memory);

    return arena;
}

Arena* arena_create_from_buffer(void* buffer, size_t buffer_size) {
    if (UNLIKELY(!buffer || buffer_size < ARENA_MIN_SIZE)) {
        return NULL;
    }

    if (UNLIKELY((uintptr_t)buffer & ALIGNMENT_MASK)) {
        fprintf(stderr, "arena_create_from_buffer(): buffer %p is not properly aligned\n", buffer);
        return NULL;
    }

    Arena* arena = malloc(sizeof(Arena));
    if (UNLIKELY(!arena)) {
        perror("malloc arena");
        return NULL;
    }

    atomic_init(&arena->head, 0);
    arena->size        = buffer_size;
    arena->base        = buffer;
    arena->owns_memory = false;

    PREFETCH_WRITE(buffer);

    return arena;
}

void arena_destroy(Arena* arena) {
    if (UNLIKELY(!arena)) return;

    if (arena->owns_memory) {
        ALIGNED_FREE(arena->base);
    }
    free(arena);
}

void arena_reset(Arena* arena) {
    if (UNLIKELY(!arena)) return;

    // Simple atomic store with release semantics
    atomic_store_explicit(&arena->head, 0, memory_order_release);

    // Prefetch start of arena for next allocations
    PREFETCH_WRITE(arena->base);
}

size_t arena_get_offset(Arena* arena) {
    if (UNLIKELY(!arena)) return 0;
    return atomic_load_explicit(&arena->head, memory_order_acquire);
}

bool arena_restore(Arena* arena, size_t offset) {
    if (UNLIKELY(!arena)) return false;

    size_t current = atomic_load_explicit(&arena->head, memory_order_relaxed);
    if (UNLIKELY(offset > current)) return false;

    // Simple store - no need for CAS loop
    atomic_store_explicit(&arena->head, offset, memory_order_release);
    return true;
}

// Ultra-fast allocation - single atomic operation, no headers, no thread-local overhead
void* arena_alloc(Arena* arena, size_t size) {
    if (UNLIKELY(!arena || !size)) return NULL;

    const size_t aligned_size = arena_align_up(size);

    // Single atomic fetch_add - this is the only contention point
    const size_t offset =
        atomic_fetch_add_explicit(&arena->head, aligned_size, memory_order_relaxed);

    // Check bounds after allocation attempt
    if (UNLIKELY(offset + aligned_size > arena->size)) {
        // Roll back the allocation
        atomic_fetch_sub_explicit(&arena->head, aligned_size, memory_order_relaxed);
        return NULL;
    }

    char* ptr = arena->base + offset;

    // Prefetch next cache line for subsequent allocations
    PREFETCH_WRITE(ptr + aligned_size);

    return ptr;
}

// Fast batch allocation - single atomic operation for multiple allocations
bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]) {
    if (UNLIKELY(!arena || !sizes || !count || !out_ptrs)) {
        return false;
    }

    // Calculate total size needed
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        total_size += arena_align_up(sizes[i]);
    }

    // Single atomic operation for entire batch
    const size_t base_offset =
        atomic_fetch_add_explicit(&arena->head, total_size, memory_order_relaxed);

    if (UNLIKELY(base_offset + total_size > arena->size)) {
        atomic_fetch_sub_explicit(&arena->head, total_size, memory_order_relaxed);
        return false;
    }

    // Distribute the allocated space
    size_t current_offset = base_offset;
    for (size_t i = 0; i < count; i++) {
        out_ptrs[i] = arena->base + current_offset;
        current_offset += arena_align_up(sizes[i]);
    }

    return true;
}

// Simplified string allocation without headers
char* arena_alloc_string(Arena* arena, const char* str) {
    if (UNLIKELY(!arena || !str)) return NULL;

    const size_t len = strlen(str);
    char* s          = arena_alloc(arena, len + 1);
    if (LIKELY(s)) {
        memcpy(s, str, len + 1);  // Copy null terminator too
    }
    return s;
}

// Simplified int allocation
int* arena_alloc_int(Arena* arena, int n) {
    if (UNLIKELY(!arena)) return NULL;

    int* number = arena_alloc(arena, sizeof(int));
    if (LIKELY(number)) {
        *number = n;
    }
    return number;
}

// Fast remaining space calculation
size_t arena_remaining(Arena* arena) {
    if (UNLIKELY(!arena)) return 0;

    const size_t used = atomic_load_explicit(&arena->head, memory_order_relaxed);
    return arena->size - used;
}

// Fast capacity check
bool arena_can_fit(Arena* arena, size_t size) {
    if (UNLIKELY(!arena || !size)) return false;

    const size_t aligned_size = arena_align_up(size);
    const size_t used         = atomic_load_explicit(&arena->head, memory_order_relaxed);
    return used <= arena->size && aligned_size <= (arena->size - used);
}

size_t arena_size(const Arena* arena) {
    return arena ? arena->size : 0;
}

size_t arena_used(Arena* arena) {
    if (UNLIKELY(!arena)) return 0;
    return atomic_load_explicit(&arena->head, memory_order_relaxed);
}

// Memory-efficient allocation for arrays
void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count) {
    if (UNLIKELY(!arena || !elem_size || !count)) return NULL;

    // Check for overflow
    if (UNLIKELY(count > SIZE_MAX / elem_size)) return NULL;

    return arena_alloc(arena, elem_size * count);
}

// Zero-initialized allocation
void* arena_alloc_zero(Arena* arena, size_t size) {
    void* ptr = arena_alloc(arena, size);
    if (LIKELY(ptr)) {
        memset(ptr, 0, size);
    }
    return ptr;
}
