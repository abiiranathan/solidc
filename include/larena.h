#ifndef L_ARANA_H_
#define L_ARANA_H_

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file larena.h
 * @brief A high-performance arena allocator for memory management
 */

#ifndef LARENA_H
#define LARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @struct LArena
 * @brief Arena allocator structure that manages a contiguous block of memory
 */
typedef struct LArena {
    char* memory;      ///< Pointer to the allocated memory block
    size_t size;       ///< Total size of the allocated memory
    size_t allocated;  ///< Number of bytes currently allocated
} LArena;

/**
 * @brief Creates a new arena allocator
 * @param size Initial size of the arena in bytes
 * @return Pointer to the new arena, or NULL on failure
 */
LArena* larena_create(size_t size);

/**
 * @brief Allocates memory from the arena
 * @param arena Arena to allocate from
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* larena_alloc(LArena* arena, size_t size);

/*
 * @brief Allocates zero-initialized memory from the arena
 * @param arena Arena to allocate from
 * @param count Number of elements to allocate
 * @param size Size of each element
 * @return Pointer to allocated memory, or NULL on failure
 */
void* larena_calloc(LArena* arena, size_t count, size_t size);

/**
 * @brief Allocates a string copy in the arena
 * @param arena Arena to allocate from
 * @param s Null-terminated string to copy
 * @return Pointer to the new string copy, or NULL on failure
 */
char* larena_alloc_string(LArena* arena, const char* s);

/**
 * @brief Resizes the arena's memory block
 * @param arena Arena to resize
 * @param new_size New size in bytes
 * @return true on success, false on failure
 */
bool larena_resize(LArena* arena, size_t new_size);

/**
 * @brief Resets the arena's allocation pointer, allowing reuse of allocated
 * memory
 * @param arena Arena to reset
 *
 * This function resets the allocation pointer to the start of the arena,
 * effectively "freeing" all previously allocated memory in the arena without
 * actually deallocating the underlying memory. This allows the arena to be
 * reused for new allocations.
 *
 * @note This does not zero or clear the memory - previously allocated content
 * remains until overwritten by new allocations.
 */
static inline void larena_reset(LArena* arena) {
    arena->allocated = 0;
}

/**
 * @brief Destroys an arena and frees all its memory
 * @param arena Arena to destroy
 */
void larena_destroy(LArena* arena);

#endif  // LARENA_H

#ifdef __cplusplus
}
#endif

#endif  // L_ARANA_H_
