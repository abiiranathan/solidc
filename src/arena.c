#include "../include/arena.h"
#include "../include/aligned_alloc.h"

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
    size_t head;       // Current allocation offset (no longer atomic)
    size_t size;       // Total arena size
    char* base;        // Memory buffer
    bool owns_memory;  // Memory ownership flag
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

    arena->head        = 0;
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

    arena->head        = 0;
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

    arena->head = 0;

    // Prefetch start of arena for next allocations
    PREFETCH_WRITE(arena->base);
}

size_t arena_get_offset(Arena* arena) {
    if (UNLIKELY(!arena)) return 0;
    return arena->head;
}

bool arena_restore(Arena* arena, size_t offset) {
    if (UNLIKELY(!arena)) return false;

    if (UNLIKELY(offset > arena->head)) return false;

    arena->head = offset;
    return true;
}

// Ultra-fast allocation - simple pointer arithmetic, no atomic operations
void* arena_alloc(Arena* arena, size_t size) {
    if (UNLIKELY(!arena || !size)) return NULL;

    const size_t aligned_size = arena_align_up(size);

    // Check bounds before allocation
    if (UNLIKELY(arena->head + aligned_size > arena->size)) {
        return NULL;
    }

    char* ptr = arena->base + arena->head;
    arena->head += aligned_size;

    // Prefetch next cache line for subsequent allocations
    PREFETCH_WRITE(ptr + aligned_size);

    return ptr;
}

// Fast batch allocation - simple arithmetic, no atomic operations
bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]) {
    if (UNLIKELY(!arena || !sizes || !count || !out_ptrs)) {
        return false;
    }

    // Calculate total size needed
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        total_size += arena_align_up(sizes[i]);
    }

    // Check bounds
    if (UNLIKELY(arena->head + total_size > arena->size)) {
        return false;
    }

    // Distribute the allocated space
    size_t current_offset = arena->head;
    for (size_t i = 0; i < count; i++) {
        out_ptrs[i] = arena->base + current_offset;
        current_offset += arena_align_up(sizes[i]);
    }

    arena->head += total_size;
    return true;
}

// Simplified string allocation without headers
char* arena_strdup(Arena* arena, const char* str) {
    if (UNLIKELY(!arena || !str)) return NULL;

    const size_t len = strlen(str);
    char* s          = arena_alloc(arena, len + 1);
    if (LIKELY(s)) {
        memcpy(s, str, len + 1);  // Copy null terminator too
    }
    return s;
}

char* arena_strdupn(Arena* arena, const char* str, size_t length) {
    if (UNLIKELY(!arena || !str)) return NULL;

    char* s = arena_alloc(arena, length + 1);
    if (LIKELY(s)) {
        memcpy(s, str, length);
        s[length] = '\0';
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

    return arena->size - arena->head;
}

// Fast capacity check
bool arena_can_fit(Arena* arena, size_t size) {
    if (UNLIKELY(!arena || !size)) return false;

    const size_t aligned_size = arena_align_up(size);
    return aligned_size <= (arena->size - arena->head);
}

size_t arena_size(const Arena* arena) {
    return arena ? arena->size : 0;
}

size_t arena_used(Arena* arena) {
    if (UNLIKELY(!arena)) return 0;
    return arena->head;
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
