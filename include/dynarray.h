/**
 * @file dynarray.h
 * @brief Dynamic array implementation with automatic resizing.
 */

#ifndef DYNARRAY_H
#define DYNARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>  // for bool
#include <stddef.h>   // for size_t
#include <stdint.h>   // for SIZE_MAX

/** Growth factor for capacity expansion (1.5x). Balances memory vs reallocation frequency. */
#define DYNARRAY_GROWTH_NUMERATOR   3
#define DYNARRAY_GROWTH_DENOMINATOR 2

/** Initial capacity when array is first created. */
#define DYNARRAY_INITIAL_CAPACITY 8

/** Threshold for shrinking: shrink when size < capacity/4. */
#define DYNARRAY_SHRINK_THRESHOLD 4

/**
 * Dynamic array structure.
 * Grows automatically on push, shrinks on explicit request or when usage drops below threshold.
 * Thread-unsafe: caller must synchronize concurrent access.
 */
typedef struct {
    /** Pointer to the data buffer. */
    void* data;
    /** Number of elements currently in the array. */
    size_t size;
    /** Current capacity (number of elements that can be stored without reallocation). */
    size_t capacity;
    /** Size of each element in bytes. */
    size_t element_size;
} dynarray_t;

/**
 * Initializes a new dynamic array.
 * @param arr Pointer to the array structure to initialize.
 * @param element_size Size of each element in bytes.
 * @param initial_capacity Initial capacity (0 uses default).
 * @return true on success, false on allocation failure.
 * @note Caller must call dynarray_free() when done.
 */
bool dynarray_init(dynarray_t* arr, size_t element_size, size_t initial_capacity);

/**
 * Frees all resources associated with the dynamic array.
 * @param arr Pointer to the array to free. Safe to pass NULL.
 * @note Array is left in an invalid state after this call.
 */
void dynarray_free(dynarray_t* arr);

/**
 * Appends an element to the end of the array.
 * Grows the array automatically if needed.
 * @param arr Pointer to the array.
 * @param element Pointer to the element to append.
 * @return true on success, false on allocation failure.
 */
bool dynarray_push(dynarray_t* arr, const void* element);

/**
 * Appends @p count contiguous elements to the end of the array in a single
 * operation. Equivalent to calling dynarray_push() @p count times, but
 * performs at most one capacity check, one growth calculation, and one
 * memcpy for the whole batch instead of one per element.
 * @param arr Pointer to the array.
 * @param elements Pointer to the first element of a contiguous block to append.
 * @param count Number of elements to append. A count of 0 is a no-op that
 *        returns true.
 * @return true on success, false on allocation failure (array is left
 *         unmodified on failure).
 * @note @p elements must not overlap the array's internal buffer.
 */
bool dynarray_push_n(dynarray_t* arr, const void* elements, size_t count);

/**
 * Removes and returns the last element from the array.
 * @param arr Pointer to the array.
 * @param out_element Pointer to store the popped element (can be NULL if value not needed).
 * @return true on success, false if array is empty.
 * @note May shrink the array if usage drops below threshold.
 */
bool dynarray_pop(dynarray_t* arr, void* out_element);

/**
 * Gets a mutable pointer to the element at the specified index.
 * @param arr Pointer to the array.
 * @param index Index of the element.
 * @return Pointer to the element, or NULL if index is out of bounds.
 * @note Returned pointer may be invalidated by operations that modify the array.
 */
void* dynarray_get(const dynarray_t* arr, size_t index);

/**
 * Gets a read-only pointer to the element at the specified index.
 * Identical to dynarray_get() but preserves const-correctness for callers
 * that only have a const dynarray_t* and must not write through the result.
 * @param arr Pointer to the array.
 * @param index Index of the element.
 * @return Const pointer to the element, or NULL if index is out of bounds.
 * @note Returned pointer may be invalidated by operations that modify the array.
 */
const void* dynarray_get_const(const dynarray_t* arr, size_t index);

/**
 * Sets the element at the specified index.
 * @param arr Pointer to the array.
 * @param index Index of the element.
 * @param element Pointer to the new element value.
 * @return true on success, false if index is out of bounds.
 */
bool dynarray_set(dynarray_t* arr, size_t index, const void* element);

/**
 * Reserves capacity for at least the specified number of elements.
 * Does not shrink if current capacity is already sufficient.
 * @param arr Pointer to the array.
 * @param new_capacity Desired minimum capacity.
 * @return true on success, false on allocation failure.
 */
bool dynarray_reserve(dynarray_t* arr, size_t new_capacity);

/**
 * Shrinks the array capacity to fit the current size exactly.
 * Useful to reclaim memory after many removals.
 * @param arr Pointer to the array.
 * @return true on success, false on allocation failure.
 */
bool dynarray_shrink_to_fit(dynarray_t* arr);

/**
 * Clears all elements from the array.
 * Does not free the underlying buffer (capacity remains unchanged).
 * @param arr Pointer to the array.
 */
void dynarray_clear(dynarray_t* arr);

/**
 * Returns the number of elements in the array.
 * @param arr Pointer to the array.
 * @return Number of elements.
 */
static inline size_t dynarray_size(const dynarray_t* arr) {
    return arr ? arr->size : 0;
}

/**
 * Returns the current capacity of the array.
 * @param arr Pointer to the array.
 * @return Current capacity.
 */
static inline size_t dynarray_capacity(const dynarray_t* arr) {
    return arr ? arr->capacity : 0;
}

/**
 * Checks if the array is empty.
 * @param arr Pointer to the array.
 * @return true if empty or NULL, false otherwise.
 */
static inline bool dynarray_is_empty(const dynarray_t* arr) {
    return arr == NULL || arr->size == 0;
}

#ifdef __cplusplus
}
#endif

#endif  // DYNARRAY_H
