#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct Arena Arena;
typedef struct UnsafeArena UnsafeArena;

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

// =============================================================================
// UNSAFE ARENA API (LOCK-FREE, SINGLE-THREADED ONLY)
// =============================================================================

/**
 * Create a new lock-free arena with the specified size.
 * WARNING: Not thread-safe. Use only in single-threaded contexts.
 *
 * @param arena_size Size of the arena in bytes (minimum 1024 bytes)
 * @return Pointer to the new UnsafeArena, or NULL on failure
 */
UnsafeArena* unsafe_arena_create(size_t arena_size);

/**
 * Create a lock-free arena from an existing aligned buffer.
 * The buffer must be aligned to 32-byte boundaries.
 * WARNING: Not thread-safe. Use only in single-threaded contexts.
 *
 * @param buffer Pre-allocated buffer (must be 32-byte aligned)
 * @param buffer_size Size of the buffer in bytes
 * @return Pointer to the new UnsafeArena, or NULL on failure
 */
UnsafeArena* unsafe_arena_create_from_buffer(void* buffer, size_t buffer_size);

/**
 * Destroy the unsafe arena and free all associated memory.
 * If the arena owns its memory, the buffer will be freed.
 *
 * @param arena UnsafeArena to destroy
 */
void unsafe_arena_destroy(UnsafeArena* arena);

/**
 * Reset the unsafe arena to its initial state (all allocations are invalidated).
 * WARNING: Not thread-safe.
 *
 * @param arena UnsafeArena to reset
 */
void unsafe_arena_reset(UnsafeArena* arena);

/**
 * Get the current allocation offset within the unsafe arena.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @return Current allocation offset in bytes
 */
size_t unsafe_arena_get_offset(UnsafeArena* arena);

/**
 * Restore the unsafe arena to a previous allocation offset.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param offset Previous offset to restore to
 * @return true on success, false if offset is invalid
 */
bool unsafe_arena_restore(UnsafeArena* arena, size_t offset);

/**
 * Allocate memory from the unsafe arena with automatic alignment.
 * Ultra-fast allocation with no locking overhead.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* unsafe_arena_alloc(UnsafeArena* arena, size_t size);

/**
 * Allocate multiple memory blocks in a single operation.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param sizes Array of sizes for each allocation
 * @param count Number of allocations to perform
 * @param out_ptrs Array to store the resulting pointers
 * @return true on success, false if any allocation would fail
 */
bool unsafe_arena_alloc_batch(UnsafeArena* arena, const size_t sizes[], size_t count,
                              void* out_ptrs[]);

/**
 * Duplicate a string in the unsafe arena.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param str String to duplicate
 * @return Pointer to duplicated string, or NULL on failure
 */
char* unsafe_arena_strdup(UnsafeArena* arena, const char* str);

/**
 * Duplicate a string with specified length in the unsafe arena.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param str String to duplicate
 * @param length Maximum length to copy
 * @return Pointer to duplicated string, or NULL on failure
 */
char* unsafe_arena_strdupn(UnsafeArena* arena, const char* str, size_t length);

/**
 * Allocate and initialize an integer in the unsafe arena.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param n Integer value to store
 * @return Pointer to allocated integer, or NULL on failure
 */
int* unsafe_arena_alloc_int(UnsafeArena* arena, int n);

/**
 * Get the remaining available space in the unsafe arena.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @return Number of bytes remaining
 */
size_t unsafe_arena_remaining(UnsafeArena* arena);

/**
 * Check if the unsafe arena can fit an allocation of the specified size.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param size Size to check
 * @return true if the allocation would fit, false otherwise
 */
bool unsafe_arena_can_fit(UnsafeArena* arena, size_t size);

/**
 * Get the total size of the unsafe arena.
 *
 * @param arena Target unsafe arena
 * @return Total arena size in bytes
 */
size_t unsafe_arena_size(const UnsafeArena* arena);

/**
 * Get the number of bytes currently allocated in the unsafe arena.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @return Number of bytes allocated
 */
size_t unsafe_arena_used(UnsafeArena* arena);

/**
 * Allocate an array of elements with overflow checking.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param elem_size Size of each element
 * @param count Number of elements
 * @return Pointer to allocated array, or NULL on failure/overflow
 */
void* unsafe_arena_alloc_array(UnsafeArena* arena, size_t elem_size, size_t count);

/**
 * Allocate and zero-initialize memory from the unsafe arena.
 * WARNING: Not thread-safe.
 *
 * @param arena Target unsafe arena
 * @param size Number of bytes to allocate
 * @return Pointer to zero-initialized memory, or NULL on failure
 */
void* unsafe_arena_alloc_zero(UnsafeArena* arena, size_t size);

// =============================================================================
// UTILITY MACROS
// =============================================================================

/**
 * Allocate a single instance of a type from the arena.
 * Thread-safe version.
 */
#define ARENA_ALLOC(arena, type) ((type*)arena_alloc((arena), sizeof(type)))

/**
 * Allocate an array of a specific type from the arena.
 * Thread-safe version.
 */
#define ARENA_ALLOC_ARRAY(arena, type, count)                                                      \
    ((type*)arena_alloc_array((arena), sizeof(type), (count)))

/**
 * Allocate and zero-initialize a single instance of a type from the arena.
 * Thread-safe version.
 */
#define ARENA_ALLOC_ZERO(arena, type) ((type*)arena_alloc_zero((arena), sizeof(type)))

/**
 * Allocate a single instance of a type from the unsafe arena.
 * WARNING: Not thread-safe.
 */
#define UNSAFE_ARENA_ALLOC(arena, type) ((type*)unsafe_arena_alloc((arena), sizeof(type)))

/**
 * Allocate an array of a specific type from the unsafe arena.
 * WARNING: Not thread-safe.
 */
#define UNSAFE_ARENA_ALLOC_ARRAY(arena, type, count)                                               \
    ((type*)unsafe_arena_alloc_array((arena), sizeof(type), (count)))

/**
 * Allocate and zero-initialize a single instance of a type from the unsafe arena.
 * WARNING: Not thread-safe.
 */
#define UNSAFE_ARENA_ALLOC_ZERO(arena, type) ((type*)unsafe_arena_alloc_zero((arena), sizeof(type)))

// =============================================================================
// CONSTANTS
// =============================================================================

/** Memory alignment for arena allocations (32 bytes) */
#define ARENA_ALIGNMENT 32UL

/** Minimum arena size in bytes */
#define ARENA_MIN_SIZE 1024UL

#ifdef __cplusplus
}
#endif

#endif  // ARENA_H
