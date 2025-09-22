#include "../include/arena.h"
#include "../include/aligned_alloc.h"
#include "../include/lock.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_ALIGNMENT (size_t)32
#define ALIGNMENT_MASK  ((size_t)(ARENA_ALIGNMENT - 1))
#define ARENA_MIN_SIZE  1024

static inline size_t arena_align_up(size_t n) {
    return (n + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;
}

typedef struct Arena {
    size_t allocated;  // Current allocation offset
    size_t size;       // Total arena size
    char* base;        // Memory buffer
    Lock lock;         // Cross-platform mutex (pthread/CriticalSection)
    bool owns_memory;  // Memory ownership flag
    bool thread_safe;  // Whether arena is thread-safe or not.
} Arena;

static inline Arena* arena_new(void* memory, size_t size, bool thread_safe, bool owns_memory) {
    Arena* arena = ALIGNED_ALLOC(ARENA_ALIGNMENT, sizeof(Arena));
    if (!arena) {
        perror("malloc arena");
        return NULL;
    }

    if (thread_safe) {
        lock_init(&arena->lock);
    }

    arena->allocated   = 0;
    arena->size        = size;
    arena->base        = memory;
    arena->owns_memory = owns_memory;
    arena->thread_safe = thread_safe;
    return arena;
}

Arena* arena_create(size_t arena_size, bool thread_safe) {
    if (arena_size < ARENA_MIN_SIZE) arena_size = ARENA_MIN_SIZE;

    arena_size   = arena_align_up(arena_size);
    char* memory = ALIGNED_ALLOC(ARENA_ALIGNMENT, arena_size);
    if (!memory) {
        perror("aligned_alloc");
        return NULL;
    }

    Arena* arena = arena_new(memory, arena_size, thread_safe, true);
    if (!arena) {
        ALIGNED_FREE(memory);
        return NULL;
    }
    return arena;
}

Arena* arena_create_from_buffer(void* buffer, size_t buffer_size, bool thread_safe) {
    if (!buffer || buffer_size < ARENA_MIN_SIZE) return NULL;

    if ((uintptr_t)buffer & ALIGNMENT_MASK) {
        fprintf(stderr,
                "arena_create_from_buffer(): buffer %p is not properly aligned to %lu bytes\n",
                buffer, ARENA_ALIGNMENT);
        return NULL;
    }
    return arena_new(buffer, buffer_size, thread_safe, false);
}

void arena_destroy(Arena* arena) {
    if (!arena) return;

    if (arena->thread_safe) lock_free(&arena->lock);
    if (arena->owns_memory) ALIGNED_FREE(arena->base);
    ALIGNED_FREE(arena);
}

void arena_reset(Arena* arena) {
    if (!arena) return;

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);
        arena->allocated = 0;
        lock_release(&arena->lock);
    } else {
        arena->allocated = 0;
    }
}

size_t arena_get_offset(Arena* arena) {
    if (!arena) return 0;

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);
        size_t offset = arena->allocated;
        lock_release(&arena->lock);
        return offset;
    } else {
        return arena->allocated;
    }
}

bool arena_restore(Arena* arena, size_t offset) {
    if (!arena) return false;

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);
        if (offset > arena->allocated) {
            lock_release(&arena->lock);
            return false;
        }
        arena->allocated = offset;
        lock_release(&arena->lock);
        return true;
    } else {
        if (offset > arena->allocated) return false;
        arena->allocated = offset;
        return true;
    }
}

// Ultra-fast allocation
void* arena_alloc(Arena* arena, size_t size) {
    if (!arena || !size) return NULL;

    const size_t aligned_size = arena_align_up(size);

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);

        if (arena->allocated + aligned_size > arena->size) {
            lock_release(&arena->lock);
            return NULL;  // out of memory
        }

        char* ptr = arena->base + arena->allocated;
        arena->allocated += aligned_size;

        lock_release(&arena->lock);
        return ptr;
    } else {
        if (arena->allocated + aligned_size > arena->size) {
            return NULL;  // out of memory
        }

        char* ptr = arena->base + arena->allocated;
        arena->allocated += aligned_size;
        return ptr;
    }
}

// Fast batch allocation
bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]) {
    if (!arena || !sizes || !count || !out_ptrs) {
        return false;
    }

    // Calculate total size needed
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        total_size += arena_align_up(sizes[i]);
    }

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);

        if (arena->allocated + total_size > arena->size) {
            lock_release(&arena->lock);
            return false;  // OOM
        }

        // Distribute the allocated space
        size_t current_offset = arena->allocated;
        for (size_t i = 0; i < count; i++) {
            out_ptrs[i] = arena->base + current_offset;
            current_offset += arena_align_up(sizes[i]);
        }

        arena->allocated = current_offset;  // Fixed: was missing allocation update

        lock_release(&arena->lock);
        return true;
    } else {
        if (arena->allocated + total_size > arena->size) {
            return false;  // OOM
        }

        // Distribute the allocated space
        size_t current_offset = arena->allocated;
        for (size_t i = 0; i < count; i++) {
            out_ptrs[i] = arena->base + current_offset;
            current_offset += arena_align_up(sizes[i]);
        }

        arena->allocated = current_offset;
        return true;
    }
}

char* arena_strdup(Arena* arena, const char* str) {
    if (!arena || !str) return NULL;

    const size_t len = strlen(str);
    char* s          = arena_alloc(arena, len + 1);
    if (s) {
        memcpy(s, str, len + 1);  // Copy null terminator too
    }
    return s;
}

char* arena_strdupn(Arena* arena, const char* str, size_t length) {
    if (!arena || !str) return NULL;

    char* s = arena_alloc(arena, length + 1);
    if (s) {
        memcpy(s, str, length);
        s[length] = '\0';
    }
    return s;
}

int* arena_alloc_int(Arena* arena, int n) {
    if (!arena) return NULL;

    int* number = arena_alloc(arena, sizeof(int));
    if (number) {
        *number = n;
    }
    return number;
}

size_t arena_remaining(Arena* arena) {
    if (!arena) return 0;

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);
        size_t remaining = arena->size - arena->allocated;
        lock_release(&arena->lock);
        return remaining;
    } else {
        return arena->size - arena->allocated;
    }
}

bool arena_can_fit(Arena* arena, size_t size) {
    if (!arena || !size) return false;

    const size_t aligned_size = arena_align_up(size);

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);
        bool can_fit =
            arena->allocated <= arena->size && aligned_size <= (arena->size - arena->allocated);
        lock_release(&arena->lock);
        return can_fit;
    } else {
        return arena->allocated <= arena->size && aligned_size <= (arena->size - arena->allocated);
    }
}

size_t arena_size(const Arena* arena) {
    return arena ? arena->size : 0;
}

size_t arena_used(Arena* arena) {
    if (!arena) return 0;

    if (arena->thread_safe) {
        lock_acquire(&arena->lock);
        size_t used = arena->allocated;
        lock_release(&arena->lock);
        return used;
    } else {
        return arena->allocated;
    }
}

void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count) {
    if (!arena || !elem_size || !count) return NULL;

    // Check for overflow
    if (count > SIZE_MAX / elem_size) return NULL;

    return arena_alloc(arena, elem_size * count);
}

void* arena_alloc_zero(Arena* arena, size_t size) {
    void* ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
