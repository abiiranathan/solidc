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

/** Represents a single memory block in the arena chain. */
typedef struct ArenaBlock {
    size_t head;              // Current allocation offset in this block
    size_t size;              // Total size of this block
    struct ArenaBlock* next;  // Next block in the chain
#if defined(_MSC_VER) && !defined(__cplusplus)
    char memory[1];  // MSVC C mode workaround
#else
    char memory[];  // C99 flexible array member
#endif
} ArenaBlock;

typedef struct Arena {
    ArenaBlock* first_block;    // Head of the block list
    ArenaBlock* current_block;  // Current block for fast allocation
    char* current_base;         // current_block->memory (cached)
    size_t current_head;        // current_block->head (cached)
    size_t current_size;        // current_block->size (cached)
    size_t default_block_size;  // Default size for new blocks
} Arena;

/**
 * Creates a new arena block using a single allocation for metadata and buffer.
 * @param size The size of the memory buffer within the block.
 * @return Pointer to new block on success, NULL on failure.
 */
static ArenaBlock* arena_block_create(size_t size) {
    // Calculate total allocation size: metadata + memory buffer
    const size_t total_size = sizeof(ArenaBlock) + size;

    // Check for overflow
    if (total_size < size) {
        fprintf(stderr, "arena_block_create: size overflow\n");
        return NULL;
    }

    // Single allocation for block + memory
    ArenaBlock* block = ALIGNED_ALLOC(ARENA_ALIGNMENT, total_size);
    if (!block) {
        perror("aligned_alloc");
        return NULL;
    }

    block->head = 0;
    block->size = size;
    block->next = NULL;

    return block;
}

/**
 * Destroys an arena block and all subsequent blocks in the chain.
 * @param block The first block to destroy.
 */
static void arena_block_destroy_chain(ArenaBlock* block) {
    while (block) {
        ArenaBlock* next = block->next;
        ALIGNED_FREE(block);
        block = next;
    }
}

/**
 * Attempts to extend the arena by moving to the next block or allocating a new one.
 * @param arena The arena to extend.
 * @param min_size The minimum size required in the new block.
 * @return true on success, false on allocation failure.
 */
static bool arena_extend(Arena* arena, size_t min_size) {
    ArenaBlock* current = arena->current_block;

    // Check if there's a next block we can reuse (from a previous reset)
    if (current->next != NULL) {
        // Sync current block's head before moving
        current->head = arena->current_head;

        // Move to next block and update cached fields
        arena->current_block = current->next;
        arena->current_base  = current->next->memory;
        arena->current_head  = current->next->head;
        arena->current_size  = current->next->size;

        // Check if the allocation fits in the reused block
        if (arena->current_size - arena->current_head >= min_size) {
            return true;
        }
        // If not, fall through to allocate a new block
    }

    // Sync current block's head before allocating new block
    current->head = arena->current_head;

    // Calculate new block size: max of default size and requested size
    size_t new_block_size = arena->default_block_size;
    if (min_size > new_block_size) {
        new_block_size = arena_align_up(min_size);
    }

    ArenaBlock* new_block = arena_block_create(new_block_size);
    if (!new_block) {
        return false;
    }

    // Chain the new block
    arena->current_block->next = new_block;
    arena->current_block       = new_block;

    // Update cached fields for the new block
    arena->current_base = new_block->memory;
    arena->current_head = 0;
    arena->current_size = new_block->size;

    return true;
}

Arena* arena_create(size_t arena_size) {
    if (arena_size < ARENA_MIN_SIZE) {
        arena_size = ARENA_MIN_SIZE;
    }

    arena_size = arena_align_up(arena_size);

    Arena* arena = malloc(sizeof(Arena));
    if (!arena) {
        perror("malloc arena");
        return NULL;
    }

    ArenaBlock* first_block = arena_block_create(arena_size);
    if (!first_block) {
        free(arena);
        return NULL;
    }

    arena->first_block        = first_block;
    arena->current_block      = first_block;
    arena->current_base       = first_block->memory;
    arena->current_head       = 0;
    arena->current_size       = first_block->size;
    arena->default_block_size = arena_size;
    return arena;
}

Arena* arena_create_from_buffer(void* buffer, size_t buffer_size) {
    // REMOVED: Creating from buffer is error-prone with block chaining
    // since we can't allocate new blocks with user-provided memory.
    // Users should use arena_create() instead.
    (void)buffer;
    (void)buffer_size;
    fprintf(stderr, "arena_create_from_buffer() is not supported with block chaining.\n");
    fprintf(stderr, "Use arena_create() instead.\n");
    return NULL;
}

void arena_destroy(Arena* arena) {
    if (!arena) {
        return;
    }

    arena_block_destroy_chain(arena->first_block);
    free(arena);
}

void arena_reset(Arena* arena) {
    // Reset all blocks to empty state
    ArenaBlock* block = arena->first_block;
    while (block) {
        block->head = 0;
        block       = block->next;
    }

    // Reset current block to first block and sync cached fields
    arena->current_block = arena->first_block;
    arena->current_base  = arena->first_block->memory;
    arena->current_head  = 0;
    arena->current_size  = arena->first_block->size;
}

/**
 * Attempts to allocate from the current block or extends the arena.
 * @param arena The arena to allocate from.
 * @param aligned_size The aligned size to allocate.
 * @return Pointer to allocated memory on success, NULL on failure.
 */
static inline void* alloc_aligned_helper(Arena* arena, size_t aligned_size) {
    if (aligned_size == 0) {
        return NULL;
    }

    ArenaBlock* block = arena->current_block;

    // Try to allocate from current block
    const size_t new_head = block->head + aligned_size;
    if (new_head <= block->size) {
        char* ptr   = block->memory + block->head;
        block->head = new_head;
        return ptr;
    }

    // Need to extend the arena with a new block
    if (!arena_extend(arena, aligned_size)) {
        return NULL;
    }

    // Allocate from the newly created block
    block       = arena->current_block;
    char* ptr   = block->memory;
    block->head = aligned_size;
    return ptr;
}

void* arena_alloc(Arena* arena, size_t size) {
    const size_t aligned_size = arena_align_up(size);
    return alloc_aligned_helper(arena, aligned_size);
}

void* arena_alloc_align(Arena* arena, size_t size, size_t alignment) {
    const size_t aligned_size = arena_align_up_custom(size, alignment);
    return alloc_aligned_helper(arena, aligned_size);
}

bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]) {
    // Calculate total required space
    size_t total_aligned = 0;
    for (size_t i = 0; i < count; i++) {
        const size_t aligned_size = arena_align_up(sizes[i]);
        total_aligned += aligned_size;
    }

    // Fast path: try to allocate all from current block using cached fields
    const size_t new_head = arena->current_head + total_aligned;

    if (new_head <= arena->current_size) {
        // All allocations fit in current block
        size_t current_offset = arena->current_head;
        for (size_t i = 0; i < count; i++) {
            out_ptrs[i] = arena->current_base + current_offset;
            current_offset += arena_align_up(sizes[i]);
        }
        arena->current_head = new_head;
        return true;
    }

    // Slow path: allocate individually (may span multiple blocks)
    for (size_t i = 0; i < count; i++) {
        void* ptr = arena_alloc(arena, sizes[i]);
        if (!ptr) {
            // Allocation failed - batch allocation is atomic, so we fail
            return false;
        }
        out_ptrs[i] = ptr;
    }

    return true;
}

char* arena_strdup(Arena* arena, const char* str) {
    const size_t len = strlen(str);
    char* s          = arena_alloc(arena, len + 1);
    if (s) {
        memcpy(s, str, len + 1);
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

int* arena_alloc_int(Arena* arena, int n) {
    int* number = arena_alloc(arena, sizeof(int));
    if (number) {
        *number = n;
    }
    return number;
}

size_t arena_size(const Arena* arena) {
    // Calculate total capacity across all blocks
    size_t total_size       = 0;
    const ArenaBlock* block = arena->first_block;

    while (block) {
        total_size += block->size;
        block = block->next;
    }

    return total_size;
}

size_t arena_used(Arena* arena) {
    // Calculate total bytes used across all blocks
    size_t total_used = 0;
    ArenaBlock* block = arena->first_block;

    while (block) {
        total_used += block->head;
        block = block->next;
    }

    return total_used;
}

void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count) {
    if (count > SIZE_MAX / elem_size) {
        return NULL;
    }
    return arena_alloc(arena, elem_size * count);
}

void* arena_alloc_zero(Arena* arena, size_t size) {
    void* ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
