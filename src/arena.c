#include "../include/arena.h"
#include "../include/aligned_alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGNMENT_MASK ((size_t)(ARENA_ALIGNMENT - 1))

static inline size_t arena_align_up(size_t n) {
    return (n + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;
}

static inline size_t arena_align_up_custom(size_t n, size_t alignment) {
    return (n + (alignment - 1)) & ~(alignment - 1);
}

typedef struct Arena {
    char* base;        // Memory buffer
    size_t head;       // Current allocation offset (no longer atomic)
    size_t size;       // Total arena size
    bool owns_memory;  // Memory ownership flag
} Arena;

Arena* arena_create(size_t arena_size) {
    if (arena_size < ARENA_MIN_SIZE) {
        arena_size = ARENA_MIN_SIZE;
    }

    arena_size   = arena_align_up(arena_size);
    Arena* arena = malloc(sizeof(Arena));
    if (!arena) {
        perror("malloc arena");
        return NULL;
    }

    char* memory = ALIGNED_ALLOC(ARENA_ALIGNMENT, arena_size);
    if (!memory) {
        perror("aligned_alloc");
        free(arena);
        return NULL;
    }

    arena->head        = 0;
    arena->size        = arena_size;
    arena->base        = memory;
    arena->owns_memory = true;

    return arena;
}

Arena* arena_create_from_buffer(void* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < ARENA_MIN_SIZE) {
        return NULL;
    }

    if ((uintptr_t)buffer & ALIGNMENT_MASK) {
        fprintf(stderr, "arena_create_from_buffer(): buffer %p is not properly aligned\n", buffer);
        return NULL;
    }

    Arena* arena = malloc(sizeof(Arena));
    if (!arena) {
        perror("malloc arena");
        return NULL;
    }

    arena->head        = 0;
    arena->size        = buffer_size;
    arena->base        = buffer;
    arena->owns_memory = false;

    return arena;
}

void arena_destroy(Arena* arena) {
    if (arena->owns_memory) {
        ALIGNED_FREE(arena->base);
    }
    free(arena);
}

void arena_reset(Arena* arena) {
    arena->head = 0;
}

size_t arena_get_offset(Arena* arena) {
    return arena->head;
}

bool arena_restore(Arena* arena, size_t offset) {
    if (offset > arena->head) return false;

    arena->head = offset;
    return true;
}

static inline void* alloc_aligned_helper(Arena* arena, size_t aligned_size) {
    const size_t new_head = arena->head + aligned_size;
    if (new_head > arena->size) {
        return NULL;
    }
    char* ptr   = arena->base + arena->head;
    arena->head = new_head;
    return ptr;
}

// Ultra-fast allocation - simple pointer arithmetic, no atomic operations
void* arena_alloc(Arena* arena, size_t size) {
    const size_t aligned_size = arena_align_up(size);
    return alloc_aligned_helper(arena, aligned_size);
}

void* arena_alloc_align(Arena* arena, size_t size, size_t alignment) {
    const size_t aligned_size = arena_align_up_custom(size, alignment);
    return alloc_aligned_helper(arena, aligned_size);
}

bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]) {
    size_t current_head = arena->head;

    for (size_t i = 0; i < count; i++) {
        const size_t aligned_size = arena_align_up(sizes[i]);
        const size_t new_head     = current_head + aligned_size;

        // Early exit on overflow
        if (new_head > arena->size) {
            return false;
        }

        out_ptrs[i]  = arena->base + current_head;
        current_head = new_head;
    }

    arena->head = current_head;
    return true;
}

// Simplified string allocation without headers
char* arena_strdup(Arena* arena, const char* str) {
    const size_t len = strlen(str);
    char* s          = arena_alloc(arena, len + 1);
    if (s) {
        memcpy(s, str, len + 1);  // Copy null terminator too
    }
    return s;
}

char* arena_strdupn(Arena* arena, const char* str, size_t length) {
    char* s = arena_alloc(arena, length + 1);
    if (s) {
        memcpy(s, str, length);
        s[length] = '\0';
    }
    return s;
}

// Simplified int allocation
int* arena_alloc_int(Arena* arena, int n) {
    int* number = arena_alloc(arena, sizeof(int));
    if (number) {
        *number = n;
    }
    return number;
}

// Fast remaining space calculation
size_t arena_remaining(Arena* arena) {
    return arena->size - arena->head;
}

// Fast capacity check
bool arena_can_fit(Arena* arena, size_t size) {
    const size_t aligned_size = arena_align_up(size);
    return aligned_size <= (arena->size - arena->head);
}

size_t arena_size(const Arena* arena) {
    return arena->size;
}

size_t arena_used(Arena* arena) {
    return arena->head;
}

// Memory-efficient allocation for arrays
void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count) {
    // Check for overflow
    if (count > SIZE_MAX / elem_size) return NULL;
    return arena_alloc(arena, elem_size * count);
}

// Zero-initialized allocation
void* arena_alloc_zero(Arena* arena, size_t size) {
    void* ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
