/**
 * @file arena.h
 * @brief Arena-based memory allocator for efficient bulk allocations.
 */

#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// CONSTANTS
// =============================================================================

// Alignment for the arena pointers.
#define ARENA_ALIGNMENT 8UL

/** Minimum arena size in bytes */
#define ARENA_MIN_SIZE 1024UL

/**
 * Opaque arena structure.
 *
 * Arena provides fast, bump-pointer allocation with automatic block chaining.
 * Allocations are extremely fast (simple pointer arithmetic) and memory is
 * automatically freed when the arena is destroyed. Individual allocations
 * cannot be freed; instead, use arena_reset() to reclaim all memory at once.
 *
 * Arenas automatically extend by allocating new blocks when the current block
 * is exhausted, allowing growth beyond the initial capacity.
 *
 * @note Not thread-safe. External synchronization required for concurrent access.
 * @see arena_create(), arena_destroy(), arena_alloc(), arena_reset()
 */
typedef struct Arena Arena;

/**
 * Create a new thread-safe arena with the specified size.
 *
 * @param arena_size Size of the arena in bytes (minimum 1024 bytes)
 * @return Pointer to the new Arena, or NULL on failure
 */
Arena* arena_create(size_t arena_size);

/**
 * Destroy the arena and free all associated memory.
 * If the arena owns its memory, the buffer will be freed.
 *
 * @param arena Arena to destroy
 */
void arena_destroy(Arena* arena);

/**
 * Reset the arena to its initial state (all allocations are invalidated).
 *
 * @param arena Arena to reset
 */
void arena_reset(Arena* arena);

/**
 * Allocate memory from the arena with automatic alignment.
 *
 * @param arena Target arena
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* arena_alloc(Arena* arena, size_t size);

/**
 * Allocate memory from the arena with specified alignment.
 *
 * @param arena Target arena
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* arena_alloc_align(Arena* arena, size_t size, size_t alignment);

/**
 * Allocate multiple memory blocks in a single operation.
 *
 * @param arena Target arena
 * @param sizes Array of sizes for each allocation
 * @param count Number of allocations to perform
 * @param out_ptrs Array to store the resulting pointers
 * @return true on success, false if any allocation would fail
 */
bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]);

/**
 * Duplicate a string in the arena.
 *
 * @param arena Target arena
 * @param str String to duplicate
 * @return Pointer to duplicated string, or NULL on failure
 */
char* arena_strdup(Arena* arena, const char* str);

/**
 * Duplicate a string with specified length in the arena.
 *
 * @param arena Target arena
 * @param str String to duplicate
 * @param length Maximum length to copy
 * @return Pointer to duplicated string, or NULL on failure
 */
char* arena_strdupn(Arena* arena, const char* str, size_t length);

/**
 * Allocate and initialize an integer in the arena.
 *
 * @param arena Target arena
 * @param n Integer value to store
 * @return Pointer to allocated integer, or NULL on failure
 */
int* arena_alloc_int(Arena* arena, int n);

/**
 * Get the total size of the arena.
 *
 * @param arena Target arena
 * @return Total arena size in bytes
 */
size_t arena_size(const Arena* arena);

/**
 * Get the number of bytes currently allocated in the arena.
 *
 * @param arena Target arena
 * @return Number of bytes allocated
 */
size_t arena_used(Arena* arena);

/**
 * Allocate an array of elements with overflow checking.
 *
 * @param arena Target arena
 * @param elem_size Size of each element
 * @param count Number of elements
 * @return Pointer to allocated array, or NULL on failure/overflow
 */
void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count);

/**
 * Allocate and zero-initialize memory from the arena.
 *
 * @param arena Target arena
 * @param size Number of bytes to allocate
 * @return Pointer to zero-initialized memory, or NULL on failure
 */
void* arena_alloc_zero(Arena* arena, size_t size);

#ifdef __cplusplus
}
#endif

#endif  // ARENA_H
