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

#define ARENA_ALIGNMENT 8UL

/** Minimum arena size in bytes */
#define ARENA_MIN_SIZE 1024UL

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct Arena Arena;

// =============================================================================
// THREAD-SAFE ARENA API
// =============================================================================

/**
 * Create a new thread-safe arena with the specified size.
 *
 * @param arena_size Size of the arena in bytes (minimum 1024 bytes)
 * @return Pointer to the new Arena, or NULL on failure
 */
Arena* arena_create(size_t arena_size);

/**
 * Create a thread-safe arena from an existing aligned buffer.
 * The buffer must be aligned to 32-byte boundaries.
 *
 * @param buffer Pre-allocated buffer (must be 32-byte aligned)
 * @param buffer_size Size of the buffer in bytes
 * @return Pointer to the new Arena, or NULL on failure
 */
Arena* arena_create_from_buffer(void* buffer, size_t buffer_size);

/**
 * Destroy the arena and free all associated memory.
 * If the arena owns its memory, the buffer will be freed.
 *
 * @param arena Arena to destroy
 */
void arena_destroy(Arena* arena);

/**
 * Reset the arena to its initial state (all allocations are invalidated).
 * Thread-safe operation.
 *
 * @param arena Arena to reset
 */
void arena_reset(Arena* arena);

/**
 * Get the current allocation offset within the arena.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @return Current allocation offset in bytes
 */
size_t arena_get_offset(Arena* arena);

/**
 * Restore the arena to a previous allocation offset.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @param offset Previous offset to restore to
 * @return true on success, false if offset is invalid
 */
bool arena_restore(Arena* arena, size_t offset);

/**
 * Allocate memory from the arena with automatic alignment.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* arena_alloc(Arena* arena, size_t size);

/**
 * Allocate multiple memory blocks in a single operation.
 * Thread-safe operation.
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
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @param str String to duplicate
 * @return Pointer to duplicated string, or NULL on failure
 */
char* arena_strdup(Arena* arena, const char* str);

/**
 * Duplicate a string with specified length in the arena.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @param str String to duplicate
 * @param length Maximum length to copy
 * @return Pointer to duplicated string, or NULL on failure
 */
char* arena_strdupn(Arena* arena, const char* str, size_t length);

/**
 * Allocate and initialize an integer in the arena.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @param n Integer value to store
 * @return Pointer to allocated integer, or NULL on failure
 */
int* arena_alloc_int(Arena* arena, int n);

/**
 * Get the remaining available space in the arena.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @return Number of bytes remaining
 */
size_t arena_remaining(Arena* arena);

/**
 * Check if the arena can fit an allocation of the specified size.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @param size Size to check
 * @return true if the allocation would fit, false otherwise
 */
bool arena_can_fit(Arena* arena, size_t size);

/**
 * Get the total size of the arena.
 *
 * @param arena Target arena
 * @return Total arena size in bytes
 */
size_t arena_size(const Arena* arena);

/**
 * Get the number of bytes currently allocated in the arena.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @return Number of bytes allocated
 */
size_t arena_used(Arena* arena);

/**
 * Allocate an array of elements with overflow checking.
 * Thread-safe operation.
 *
 * @param arena Target arena
 * @param elem_size Size of each element
 * @param count Number of elements
 * @return Pointer to allocated array, or NULL on failure/overflow
 */
void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count);

/**
 * Allocate and zero-initialize memory from the arena.
 * Thread-safe operation.
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
