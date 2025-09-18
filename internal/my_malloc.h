/**
 * @file memory_allocator.h
 * @brief Thread-safe lock-free memory allocator with dynamic memory pool
 *
 * This memory allocator provides malloc/free/calloc/realloc functionality with:
 * - Dynamic memory pool expansion (not fixed size)
 * - Thread safety using atomic operations (lock-free design)
 * - Memory alignment support
 * - Efficient free block coalescing
 * - Memory corruption detection
 */

#ifndef MEMORY_ALLOCATOR_H
#define MEMORY_ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Allocate memory of specified size
 *
 * Thread-safe allocation of memory with proper alignment. Returns NULL if
 * the allocation fails.
 *
 * @param size Size of memory to allocate in bytes
 * @return void* Pointer to allocated memory, or NULL if allocation fails
 */
void* my_malloc(size_t size);

/**
 * @brief Free previously allocated memory
 *
 * Thread-safe release of memory. Will detect and report double-free errors
 * and invalid pointers.
 *
 * @param ptr Pointer to memory previously allocated with my_malloc, my_calloc
 * or my_realloc
 */
void my_free(void* ptr);

/**
 * @brief Allocate and zero-initialize an array
 *
 * Allocates memory for an array of nmemb elements of size bytes each and
 * initializes all bytes to zero.
 *
 * @param nmemb Number of elements
 * @param size Size of each element in bytes
 * @return void* Pointer to allocated memory, or NULL if allocation fails
 */
void* my_calloc(size_t nmemb, size_t size);

/**
 * @brief Change the size of previously allocated memory block
 *
 * If ptr is NULL, behaves like my_malloc. If size is 0, behaves like my_free.
 * Otherwise, tries to resize the memory block pointed to by ptr.
 *
 * @param ptr Pointer to previously allocated memory or NULL
 * @param size New size in bytes
 * @return void* Pointer to the newly allocated memory, or NULL if reallocation
 * fails
 */
void* my_realloc(void* ptr, size_t size);

/**
 * @brief Print current memory allocation state (for debugging)
 *
 * Outputs information about each memory block, including address, size,
 * allocation status, and memory pool information.
 */
void print_memory_state(void);

#endif /* MEMORY_ALLOCATOR_H */
