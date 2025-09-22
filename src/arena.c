#include "../include/arena.h"
#include "../include/aligned_alloc.h"
#include "../include/lock.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGNMENT_MASK ((size_t)(ARENA_ALIGNMENT - 1))

static inline size_t arena_align_up(size_t n) {
    return (n + ALIGNMENT_MASK) & ~ALIGNMENT_MASK;
}

// Thread-safe arena with mutex
typedef struct Arena {
    size_t allocated;  // Current allocation offset
    size_t size;       // Total arena size
    char* base;        // Memory buffer
    Lock lock;         // Mutex for thread safety
    bool owns_memory;  // Memory ownership flag
} Arena;

// Lock-free arena for single-threaded use
typedef struct UnsafeArena {
    size_t allocated;  // Current allocation offset
    size_t size;       // Total arena size
    char* base;        // Memory buffer
    bool owns_memory;  // Memory ownership flag
} UnsafeArena;

// =============================================================================
// THREAD-SAFE ARENA IMPLEMENTATION
// =============================================================================

static inline Arena* arena_new(void* memory, size_t size, bool owns_memory) {
    Arena* arena = ALIGNED_ALLOC(ARENA_ALIGNMENT, sizeof(Arena));
    if (!arena) {
        perror("malloc arena");
        return NULL;
    }

    lock_init(&arena->lock);
    arena->allocated   = 0;
    arena->size        = size;
    arena->base        = memory;
    arena->owns_memory = owns_memory;
    return arena;
}

Arena* arena_create(size_t arena_size) {
    if (arena_size < ARENA_MIN_SIZE) arena_size = ARENA_MIN_SIZE;

    arena_size   = arena_align_up(arena_size);
    char* memory = ALIGNED_ALLOC(ARENA_ALIGNMENT, arena_size);
    if (!memory) {
        perror("aligned_alloc");
        return NULL;
    }

    Arena* arena = arena_new(memory, arena_size, true);
    if (!arena) {
        ALIGNED_FREE(memory);
        return NULL;
    }
    return arena;
}

Arena* arena_create_from_buffer(void* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < ARENA_MIN_SIZE) return NULL;

    if ((uintptr_t)buffer & ALIGNMENT_MASK) {
        fprintf(stderr,
                "arena_create_from_buffer(): buffer %p is not properly aligned to %lu bytes\n",
                buffer, ARENA_ALIGNMENT);
        return NULL;
    }
    return arena_new(buffer, buffer_size, false);
}

void arena_destroy(Arena* arena) {
    if (!arena) return;

    lock_free(&arena->lock);
    if (arena->owns_memory) ALIGNED_FREE(arena->base);
    ALIGNED_FREE(arena);
}

void arena_reset(Arena* arena) {
    lock_acquire(&arena->lock);
    arena->allocated = 0;
    lock_release(&arena->lock);
}

size_t arena_get_offset(Arena* arena) {
    lock_acquire(&arena->lock);
    size_t offset = arena->allocated;
    lock_release(&arena->lock);
    return offset;
}

bool arena_restore(Arena* arena, size_t offset) {
    lock_acquire(&arena->lock);
    if (offset > arena->allocated) {
        lock_release(&arena->lock);
        return false;
    }
    arena->allocated = offset;
    lock_release(&arena->lock);
    return true;
}

void* arena_alloc(Arena* arena, size_t size) {
    if (size == 0) return NULL;

    const size_t aligned_size = arena_align_up(size);

    lock_acquire(&arena->lock);

    if (arena->allocated + aligned_size > arena->size) {
        lock_release(&arena->lock);
        return NULL;  // out of memory
    }

    char* ptr = arena->base + arena->allocated;
    arena->allocated += aligned_size;

    lock_release(&arena->lock);
    return ptr;
}

bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]) {
    if (!sizes || !count || !out_ptrs) {
        return false;
    }

    // Calculate total size needed
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        total_size += arena_align_up(sizes[i]);
    }

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

    arena->allocated = current_offset;

    lock_release(&arena->lock);
    return true;
}

char* arena_strdup(Arena* arena, const char* str) {
    if (!str) return NULL;

    const size_t len = strlen(str);
    char* s          = arena_alloc(arena, len + 1);
    if (s) {
        memcpy(s, str, len + 1);  // Copy null terminator too
    }
    return s;
}

char* arena_strdupn(Arena* arena, const char* str, size_t length) {
    if (!str) return NULL;

    char* s = arena_alloc(arena, length + 1);
    if (s) {
        memcpy(s, str, length);
        s[length] = '\0';
    }
    return s;
}

int* arena_alloc_int(Arena* arena, int n) {
    int* number = arena_alloc(arena, sizeof(int));
    if (number) {
        *number = n;
    }
    return number;
}

size_t arena_remaining(Arena* arena) {
    lock_acquire(&arena->lock);
    size_t remaining = arena->size - arena->allocated;
    lock_release(&arena->lock);
    return remaining;
}

bool arena_can_fit(Arena* arena, size_t size) {
    if (size == 0) return false;

    const size_t aligned_size = arena_align_up(size);

    lock_acquire(&arena->lock);
    bool can_fit =
        arena->allocated <= arena->size && aligned_size <= (arena->size - arena->allocated);
    lock_release(&arena->lock);
    return can_fit;
}

size_t arena_size(const Arena* arena) {
    return arena->size;
}

size_t arena_used(Arena* arena) {
    lock_acquire(&arena->lock);
    size_t used = arena->allocated;
    lock_release(&arena->lock);
    return used;
}

void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count) {
    if (!elem_size || !count) return NULL;

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

// =============================================================================
// UNSAFE ARENA IMPLEMENTATION (LOCK-FREE)
// =============================================================================

static inline UnsafeArena* unsafe_arena_new(void* memory, size_t size, bool owns_memory) {
    UnsafeArena* arena = ALIGNED_ALLOC(ARENA_ALIGNMENT, sizeof(UnsafeArena));
    if (!arena) {
        perror("malloc unsafe_arena");
        return NULL;
    }

    arena->allocated   = 0;
    arena->size        = size;
    arena->base        = memory;
    arena->owns_memory = owns_memory;
    return arena;
}

UnsafeArena* unsafe_arena_create(size_t arena_size) {
    if (arena_size < ARENA_MIN_SIZE) arena_size = ARENA_MIN_SIZE;

    arena_size   = arena_align_up(arena_size);
    char* memory = ALIGNED_ALLOC(ARENA_ALIGNMENT, arena_size);
    if (!memory) {
        perror("aligned_alloc");
        return NULL;
    }

    UnsafeArena* arena = unsafe_arena_new(memory, arena_size, true);
    if (!arena) {
        ALIGNED_FREE(memory);
        return NULL;
    }
    return arena;
}

UnsafeArena* unsafe_arena_create_from_buffer(void* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < ARENA_MIN_SIZE) return NULL;

    if ((uintptr_t)buffer & ALIGNMENT_MASK) {
        fprintf(
            stderr,
            "unsafe_arena_create_from_buffer(): buffer %p is not properly aligned to %lu bytes\n",
            buffer, ARENA_ALIGNMENT);
        return NULL;
    }
    return unsafe_arena_new(buffer, buffer_size, false);
}

void unsafe_arena_destroy(UnsafeArena* arena) {
    if (!arena) return;

    if (arena->owns_memory) ALIGNED_FREE(arena->base);
    ALIGNED_FREE(arena);
}

void unsafe_arena_reset(UnsafeArena* arena) {
    arena->allocated = 0;
}

size_t unsafe_arena_get_offset(UnsafeArena* arena) {
    return arena->allocated;
}

bool unsafe_arena_restore(UnsafeArena* arena, size_t offset) {
    if (offset > arena->allocated) return false;
    arena->allocated = offset;
    return true;
}

// Ultra-fast allocation - no branches, no locks
void* unsafe_arena_alloc(UnsafeArena* arena, size_t size) {
    if (size == 0) return NULL;

    const size_t aligned_size = arena_align_up(size);

    if (arena->allocated + aligned_size > arena->size) {
        return NULL;  // out of memory
    }

    char* ptr = arena->base + arena->allocated;
    arena->allocated += aligned_size;
    return ptr;
}

bool unsafe_arena_alloc_batch(UnsafeArena* arena, const size_t sizes[], size_t count,
                              void* out_ptrs[]) {
    if (!sizes || !count || !out_ptrs) {
        return false;
    }

    // Calculate total size needed
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        total_size += arena_align_up(sizes[i]);
    }

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

char* unsafe_arena_strdup(UnsafeArena* arena, const char* str) {
    if (!str) return NULL;

    const size_t len = strlen(str);
    char* s          = unsafe_arena_alloc(arena, len + 1);
    if (s) {
        memcpy(s, str, len + 1);  // Copy null terminator too
    }
    return s;
}

char* unsafe_arena_strdupn(UnsafeArena* arena, const char* str, size_t length) {
    if (!str) return NULL;

    char* s = unsafe_arena_alloc(arena, length + 1);
    if (s) {
        memcpy(s, str, length);
        s[length] = '\0';
    }
    return s;
}

int* unsafe_arena_alloc_int(UnsafeArena* arena, int n) {
    int* number = unsafe_arena_alloc(arena, sizeof(int));
    if (number) {
        *number = n;
    }
    return number;
}

size_t unsafe_arena_remaining(UnsafeArena* arena) {
    return arena->size - arena->allocated;
}

bool unsafe_arena_can_fit(UnsafeArena* arena, size_t size) {
    if (size == 0) return false;

    const size_t aligned_size = arena_align_up(size);
    return arena->allocated <= arena->size && aligned_size <= (arena->size - arena->allocated);
}

size_t unsafe_arena_size(const UnsafeArena* arena) {
    return arena->size;
}

size_t unsafe_arena_used(UnsafeArena* arena) {
    return arena->allocated;
}

void* unsafe_arena_alloc_array(UnsafeArena* arena, size_t elem_size, size_t count) {
    if (!elem_size || !count) return NULL;

    // Check for overflow
    if (count > SIZE_MAX / elem_size) return NULL;

    return unsafe_arena_alloc(arena, elem_size * count);
}

void* unsafe_arena_alloc_zero(UnsafeArena* arena, size_t size) {
    void* ptr = unsafe_arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
