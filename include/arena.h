/**
 * @file arena.h
 * @brief Ultra-fast and robust memory arena allocator for C
 *
 * This arena allocator provides extremely fast memory allocation from a
 * pre-allocated block, significantly outperforming malloc for typical
 * allocation patterns. It's optimized for scenarios with many small
 * allocations and bulk deallocation.
 *
 * Key features:
 * - Ultra-fast allocation (single atomic operation per allocation)
 * - Zero allocation overhead (no per-allocation headers)
 * - Memory alignment for optimal performance
 * - No fragmentation within an arena
 * - Bulk deallocation through arena_reset or arena_destroy
 * - Support for creating arenas from existing memory buffers
 * - State saving/restoring for temporary allocations
 * - Optimized batch allocations
 * - Thread-safe with minimal contention
 * - Memory prefetching for better cache performance
 *
 * Performance characteristics:
 * - O(1) allocation time
 * - Minimal CPU overhead per allocation
 * - Excellent cache locality
 * - Lock-free concurrent access
 */

#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARENA_ALIGNMENT
#define ARENA_ALIGNMENT 16
#endif

#ifndef ARENA_MIN_SIZE
#define ARENA_MIN_SIZE 1024
#endif

/**
 * @brief Arena allocator structure
 *
 * The internal structure of the arena. Users should treat this as opaque and
 * only interact with it through the provided functions.
 */
typedef struct Arena Arena;

/**
 * @brief Create a new arena with the specified size
 *
 * Allocates a new arena with at least the requested size. The actual size may
 * be adjusted to meet alignment requirements. The arena memory is allocated
 * with proper alignment for optimal performance.
 *
 * @param arena_size The minimum size of the arena in bytes
 * @return Arena* Pointer to the new arena, or NULL on failure
 */
Arena* arena_create(size_t arena_size);

/**
 * @brief Create an arena from an existing memory buffer
 *
 * Creates an arena that uses an existing memory buffer. The arena will not free
 * this buffer when destroyed. The buffer must be properly aligned to
 * ARENA_ALIGNMENT boundaries.
 *
 * @param buffer Pointer to the memory buffer to use (must be aligned)
 * @param buffer_size Size of the buffer in bytes
 * @return Arena* Pointer to the new arena, or NULL on failure
 */
Arena* arena_create_from_buffer(void* buffer, size_t buffer_size);

/**
 * @brief Destroy an arena and free all associated memory
 *
 * Frees all memory associated with the arena. If the arena was created using
 * arena_create_from_buffer(), the original buffer will not be freed.
 *
 * @param arena Pointer to the arena to destroy
 */
void arena_destroy(Arena* arena);

/**
 * @brief Reset an arena to its initial empty state
 *
 * Marks all memory in the arena as unused, without deallocating the underlying
 * memory. This is much faster than destroying and recreating an arena.
 * Thread-safe operation.
 *
 * @param arena Pointer to the arena to reset
 */
void arena_reset(Arena* arena);

/**
 * @brief Allocate memory from the arena
 *
 * Allocates a block of memory from the arena with proper alignment.
 * This is an extremely fast operation - typically just a single atomic
 * increment and bounds check.
 *
 * @param arena Pointer to the arena
 * @param size Number of bytes to allocate
 * @return void* Pointer to the allocated memory, or NULL if the allocation fails
 */
void* arena_alloc(Arena* arena, size_t size);

/**
 * @brief Allocate multiple blocks atomically
 *
 * Allocates multiple blocks of memory in a single atomic operation,
 * which is much more efficient than individual allocations when you
 * need several blocks at once.
 *
 * @param arena Pointer to the arena
 * @param sizes Array of sizes for each allocation
 * @param count Number of allocations to perform
 * @param out_ptrs Array to store the resulting pointers
 * @return bool true if all allocations succeeded, false if insufficient memory
 */
bool arena_alloc_batch(Arena* arena, const size_t sizes[], size_t count, void* out_ptrs[]);

/**
 * @brief Allocate memory for a string and copy the contents
 *
 * Allocates memory for a string and copies the contents into the arena.
 * The resulting string is null-terminated.
 *
 * @param arena Pointer to the arena
 * @param str The string to copy
 * @return char* Pointer to the copied string, or NULL if the allocation fails
 */
char* arena_alloc_string(Arena* arena, const char* str);

/**
 * @brief Allocate memory for an integer and set its value
 *
 * Allocates memory for an integer and sets its value.
 *
 * @param arena Pointer to the arena
 * @param n The integer value to store
 * @return int* Pointer to the allocated integer, or NULL if the allocation fails
 */
int* arena_alloc_int(Arena* arena, int n);

/**
 * @brief Allocate memory for an array
 *
 * Allocates memory for an array of elements with overflow protection.
 * Equivalent to arena_alloc(arena, elem_size * count) but with safety checks.
 *
 * @param arena Pointer to the arena
 * @param elem_size Size of each element in bytes
 * @param count Number of elements to allocate
 * @return void* Pointer to the allocated array, or NULL if the allocation fails
 */
void* arena_alloc_array(Arena* arena, size_t elem_size, size_t count);

/**
 * @brief Allocate zeroed memory from the arena
 *
 * Allocates a block of memory and initializes it to zero.
 *
 * @param arena Pointer to the arena
 * @param size Size in bytes to allocate
 * @return void* Pointer to the allocated memory, or NULL if the allocation fails
 */
void* arena_alloc_zero(Arena* arena, size_t size);

/**
 * @brief Get the current offset in the arena
 *
 * Returns the current offset in the arena, which can be used with
 * arena_restore() to return to this point. Thread-safe operation.
 *
 * @param arena Pointer to the arena
 * @return size_t The current offset
 */
size_t arena_get_offset(Arena* arena);

/**
 * @brief Restore the arena to a previous state
 *
 * Restores the arena to a previous state, effectively deallocating all memory
 * allocated after the specified offset. This operation is thread-safe.
 *
 * @param arena Pointer to the arena
 * @param offset The offset to restore to (obtained from arena_get_offset())
 * @return bool true if successful, false if the offset is invalid
 */
bool arena_restore(Arena* arena, size_t offset);

/**
 * @brief Get the remaining space in the arena
 *
 * Returns the number of bytes that can still be allocated from this arena.
 * Note: This is a snapshot and may change immediately in multi-threaded usage.
 *
 * @param arena Pointer to the arena
 * @return size_t The number of bytes available
 */
size_t arena_remaining(Arena* arena);

/**
 * @brief Check if the arena can fit a certain amount of bytes
 *
 * Checks if an allocation of the specified size would succeed.
 * Note: This is a best-effort check in multi-threaded scenarios.
 *
 * @param arena Pointer to the arena
 * @param size The size in bytes to check
 * @return bool true if the allocation would likely succeed, false otherwise
 */
bool arena_can_fit(Arena* arena, size_t size);

/**
 * @brief Get the total size of the arena
 *
 * Returns the total size of the arena in bytes.
 *
 * @param arena Pointer to the arena
 * @return size_t The total size in bytes
 */
size_t arena_size(const Arena* arena);

/**
 * @brief Get the amount of memory used in the arena
 *
 * Returns the number of bytes currently used in the arena.
 *
 * @param arena Pointer to the arena
 * @return size_t The number of bytes used
 */
size_t arena_used(Arena* arena);

#ifdef __cplusplus
}
#endif

#endif  // ARENA_H
