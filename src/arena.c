#include "../include/arena.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (arena_size < ARENA_MIN_SIZE) {
        arena_size = ARENA_MIN_SIZE;
    }

    arena_size = arena_align_up(arena_size, ARENA_ALIGNMENT);

    Arena* arena = malloc(sizeof(Arena));
    if (!arena) {
        perror("malloc arena");
        return NULL;
    }

    char* memory = aligned_alloc(ARENA_ALIGNMENT, arena_size);
    if (!memory) {
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
    if (!buffer || buffer_size < ARENA_MIN_SIZE) {
        return NULL;
    }

    if ((uintptr_t)buffer % ARENA_ALIGNMENT != 0) {
        printf("arena_create_from_buffer(): buffer %p is not properly aligned\n", (void*)buffer);
        return NULL;
    }

    Arena* arena = malloc(sizeof(Arena));
    if (!arena) {
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
    if (!arena) {
        return;
    }

    // No concurrent access after this point is safe
    if (arena->owns_memory) {
        free(arena->base);
    }

    // Reset thread-local state (if applicable)
    // This assumes that thread-local blocks are tied to the arena and should be invalidated.
    // If thread-local blocks are independent, this step may not be necessary.
    tl_block.current   = NULL;
    tl_block.remaining = 0;

    free(arena);
}

void arena_reset(Arena* arena) {
    if (!arena) {
        return;
    }

    tl_block.current   = NULL;
    tl_block.remaining = 0;

    // Release semantics to ensure all prior writes are visible
    atomic_store_explicit(&arena->used, 0, memory_order_release);
}

size_t arena_get_offset(Arena* arena) {
    if (!arena) {
        return 0;
    }
    // Acquire semantics to synchronize with allocations
    return atomic_load_explicit(&arena->used, memory_order_acquire);
}

bool arena_restore(Arena* arena, size_t offset) {
    if (!arena) {
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
    if (!arena || size == 0)
        return NULL;

    size_t header_size  = arena_align_up(sizeof(AllocHeader), ARENA_ALIGNMENT);
    size_t aligned_size = header_size + arena_align_up(size, ARENA_ALIGNMENT);

    // Try to allocate from thread-local block first
    if (tl_block.remaining >= aligned_size && tl_block.current >= arena->base &&
        tl_block.current < arena->base + arena->size) {
        char* ptr           = tl_block.current;
        AllocHeader* header = (AllocHeader*)ptr;
        header->size        = size;
        tl_block.current += aligned_size;
        tl_block.remaining -= aligned_size;
        return ptr + header_size;
    }

    // Reserve enough space for at least this allocation
    size_t block_size = aligned_size > THREAD_BLOCK_SIZE ? aligned_size : THREAD_BLOCK_SIZE;
    block_size        = arena_align_up(block_size, ARENA_ALIGNMENT);

    size_t old_used, new_used;
    do {
        old_used = atomic_load_explicit(&arena->used, memory_order_relaxed);
        new_used = old_used + block_size;

        if (new_used > arena->size) {
            // Try exact size as a last resort
            block_size = aligned_size;
            new_used   = old_used + block_size;
            if (new_used > arena->size) {
                return NULL;  // Fail if it doesn’t fit
            }
        }
    } while (!atomic_compare_exchange_weak_explicit(
        &arena->used, &old_used, new_used, memory_order_release, memory_order_relaxed));

    // Initialize thread-local block with the full reserved space
    tl_block.current   = arena->base + old_used;
    tl_block.remaining = block_size;

    // Allocate from the new block
    char* ptr           = tl_block.current;
    AllocHeader* header = (AllocHeader*)ptr;
    header->size        = size;
    tl_block.current += aligned_size;
    tl_block.remaining -= aligned_size;
    return ptr + header_size;
}

void* arena_realloc(Arena* arena, void* ptr, size_t new_size) {
    if (!arena || new_size == 0) {
        return NULL;
    }

    if (!ptr) {
        return arena_alloc(arena, new_size);  // Treat NULL as a new allocation
    }

    size_t header_size  = arena_align_up(sizeof(AllocHeader), ARENA_ALIGNMENT);
    AllocHeader* header = (AllocHeader*)((char*)ptr - header_size);
    size_t old_size     = header->size;

    if (old_size == new_size) {
        return ptr;  // No change in size, return the same pointer
    }

    size_t aligned_old_size = arena_align_up(old_size, ARENA_ALIGNMENT);
    size_t aligned_new_size = arena_align_up(new_size, ARENA_ALIGNMENT);

    // Check if this is the last allocation in the arena
    size_t current_used = atomic_load_explicit(&arena->used, memory_order_acquire);
    char* alloc_end     = (char*)ptr + aligned_old_size;
    char* arena_end     = arena->base + current_used;

    if (alloc_end == arena_end && tl_block.remaining == 0) {
        // Last allocation in the arena, attempt in-place resizing
        size_t old_used, new_used;
        do {
            old_used = atomic_load_explicit(&arena->used, memory_order_relaxed);
            if (old_used != current_used) {
                break;  // Another thread modified the arena, fallback to new allocation
            }

            if (new_size < old_size) {
                // Shrinking in-place
                new_used = current_used - (aligned_old_size - aligned_new_size);
            } else {
                // Expanding in-place
                new_used = current_used + (aligned_new_size - aligned_old_size);
                if (new_used > arena->size) {
                    break;  // Not enough space, fallback to new allocation
                }
            }
        } while (!atomic_compare_exchange_weak_explicit(
            &arena->used, &old_used, new_used, memory_order_release, memory_order_relaxed));

        if (old_used == current_used) {
            header->size = new_size;
            if (new_size < old_size) {
                ((char*)ptr)[new_size - 1] = '\0';  // Ensure null termination
            }
            return ptr;
        }
    }

    // Allocate new memory and copy the existing data
    void* new_ptr = arena_alloc(arena, new_size);
    if (!new_ptr) {
        return NULL;  // Allocation failed
    }

    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);

    if (new_size < old_size) {
        ((char*)new_ptr)[new_size] = '\0';  // Ensure null termination
    }

    return new_ptr;
}

char* arena_alloc_string(Arena* arena, const char* str) {
    if (!arena || !str) {
        return NULL;
    }

    size_t len = strlen(str);
    if (len >= SIZE_MAX) {
        return NULL;
    }

    char* s = (char*)arena_alloc(arena, len + 1);
    if (!s) {
        return NULL;
    }

    memcpy(s, str, len);
    s[len] = '\0';
    return s;
}

int* arena_alloc_int(Arena* arena, int n) {
    if (!arena) {
        return NULL;
    }

    int* number = (int*)arena_alloc(arena, sizeof(int));
    if (!number) {
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
