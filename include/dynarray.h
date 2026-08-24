/**
 * @file dynarray.h
 * @brief High-performance dynamic array implementation with automatic resizing.
 */

#ifndef DYNARRAY_H
#define DYNARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>  // for bool
#include <stddef.h>   // for size_t
#include <stdint.h>   // for SIZE_MAX, uint8_t, uint16_t, uint32_t, uint64_t
#include <string.h>   // for memcpy, memmove

/* -------------------------------------------------------------------------- */
/* Compiler Portability & Branch Hints                                       */
/* -------------------------------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__)
#define DYNARRAY_LIKELY(x)   __builtin_expect(!!(x), 1)
#define DYNARRAY_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define DYNARRAY_INLINE      inline __attribute__((always_inline))
#define DYNARRAY_COLD        __attribute__((cold, noinline))
#define DYNARRAY_RESTRICT    __restrict__
#else
#define DYNARRAY_LIKELY(x)   (x)
#define DYNARRAY_UNLIKELY(x) (x)
#define DYNARRAY_INLINE      inline
#define DYNARRAY_COLD
#define DYNARRAY_RESTRICT
#endif

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
    /** Pointer to the data buffer. Marked restrict for compiler vectorization. */
    void* DYNARRAY_RESTRICT data;
    /** Number of elements currently in the array. */
    size_t size;
    /** Current capacity (number of elements that can be stored without reallocation). */
    size_t capacity;
    /** Size of each element in bytes. */
    size_t element_size;
} dynarray_t;

/* -------------------------------------------------------------------------- */
/* Internal Helper Functions                                                  */
/* -------------------------------------------------------------------------- */

/**
 * Fast-path element copy optimized for common scalar sizes.
 * Bypasses libc memcpy overhead for primitive types (1, 2, 4, 8 bytes).
 */
static DYNARRAY_INLINE void dynarray_fast_copy(void* DYNARRAY_RESTRICT dest, const void* DYNARRAY_RESTRICT src,
                                               size_t size) {
    switch (size) {
        case 1:
            *(uint8_t*)dest = *(const uint8_t*)src;
            break;
        case 2:
            *(uint16_t*)dest = *(const uint16_t*)src;
            break;
        case 4:
            *(uint32_t*)dest = *(const uint32_t*)src;
            break;
        case 8:
            *(uint64_t*)dest = *(const uint64_t*)src;
            break;
        default:
            memcpy(dest, src, size);
            break;
    }
}

/** Out-of-line cold reallocation target to keep hot inline paths lightweight. */
DYNARRAY_COLD bool dynarray_grow_slowpath(dynarray_t* arr, size_t min_capacity);

/* -------------------------------------------------------------------------- */
/* Public API                                                                */
/* -------------------------------------------------------------------------- */

/**
 * Initializes a new dynamic array.
 * @param arr Pointer to the array structure to initialize.
 * @param element_size Size of each element in bytes.
 * @param initial_capacity Initial capacity (0 uses default).
 * @return true on success, false on allocation failure.
 */
bool dynarray_init(dynarray_t* arr, size_t element_size, size_t initial_capacity);

/**
 * Frees all resources associated with the dynamic array.
 * @param arr Pointer to the array to free. Safe to pass NULL.
 */
void dynarray_free(dynarray_t* arr);

/**
 * Appends an element to the end of the array.
 * @param arr Pointer to the array.
 * @param element Pointer to the element to append.
 * @return true on success, false on allocation failure.
 */
static DYNARRAY_INLINE bool dynarray_push(dynarray_t* arr, const void* element) {
    if (DYNARRAY_UNLIKELY(arr == NULL || element == NULL)) {
        return false;
    }

    if (DYNARRAY_UNLIKELY(arr->size >= arr->capacity)) {
        // Fast 1.5x capacity growth via bitwise shifts: cap + (cap >> 1)
        size_t new_cap = arr->capacity + (arr->capacity >> 1);
        if (DYNARRAY_UNLIKELY(new_cap <= arr->capacity)) {
            new_cap = arr->capacity + 1;
        }
        if (DYNARRAY_UNLIKELY(!dynarray_grow_slowpath(arr, new_cap))) {
            return false;
        }
    }

    unsigned char* dest = (unsigned char*)arr->data + (arr->size * arr->element_size);
    dynarray_fast_copy(dest, element, arr->element_size);
    arr->size++;

    return true;
}

/**
 * Appends count contiguous elements to the end of the array.
 */
bool dynarray_push_n(dynarray_t* arr, const void* elements, size_t count);

/**
 * Removes and returns the last element from the array.
 */
static DYNARRAY_INLINE bool dynarray_pop(dynarray_t* arr, void* out_element) {
    if (DYNARRAY_UNLIKELY(arr == NULL || arr->size == 0)) {
        return false;
    }

    arr->size--;

    if (out_element != NULL) {
        const unsigned char* src = (const unsigned char*)arr->data + (arr->size * arr->element_size);
        dynarray_fast_copy(out_element, src, arr->element_size);
    }

    // Shrink check on cold path if usage drops below threshold
    if (DYNARRAY_UNLIKELY(arr->capacity > DYNARRAY_INITIAL_CAPACITY &&
                          arr->size < arr->capacity / DYNARRAY_SHRINK_THRESHOLD)) {
        size_t new_capacity = arr->capacity >> 1;  // Faster capacity / 2
        if (new_capacity < DYNARRAY_INITIAL_CAPACITY) {
            new_capacity = DYNARRAY_INITIAL_CAPACITY;
        }
        dynarray_grow_slowpath(arr, new_capacity);
    }

    return true;
}

/**
 * Gets a mutable pointer to the element at the specified index.
 */
static DYNARRAY_INLINE void* dynarray_get(const dynarray_t* arr, size_t index) {
    if (DYNARRAY_UNLIKELY(arr == NULL || index >= arr->size)) {
        return NULL;
    }
    return (unsigned char*)arr->data + (index * arr->element_size);
}

/**
 * Gets a read-only pointer to the element at the specified index.
 */
static DYNARRAY_INLINE const void* dynarray_get_const(const dynarray_t* arr, size_t index) {
    if (DYNARRAY_UNLIKELY(arr == NULL || index >= arr->size)) {
        return NULL;
    }
    return (const unsigned char*)arr->data + (index * arr->element_size);
}

/**
 * Sets the element at the specified index.
 */
static DYNARRAY_INLINE bool dynarray_set(dynarray_t* arr, size_t index, const void* element) {
    if (DYNARRAY_UNLIKELY(arr == NULL || element == NULL || index >= arr->size)) {
        return false;
    }

    unsigned char* dest = (unsigned char*)arr->data + (index * arr->element_size);
    dynarray_fast_copy(dest, element, arr->element_size);

    return true;
}

bool dynarray_reserve(dynarray_t* arr, size_t new_capacity);
bool dynarray_shrink_to_fit(dynarray_t* arr);
void dynarray_clear(dynarray_t* arr);

static inline size_t dynarray_size(const dynarray_t* arr) { return arr ? arr->size : 0; }

static inline size_t dynarray_capacity(const dynarray_t* arr) { return arr ? arr->capacity : 0; }

static inline bool dynarray_is_empty(const dynarray_t* arr) { return arr == NULL || arr->size == 0; }

/* -------------------------------------------------------------------------- */
/* Zero-Overhead Type-Safe Accessor Macros                                   */
/* -------------------------------------------------------------------------- */

/**
 * Direct typed array index macro. Completely bypasses function call overhead
 * and pointer arithmetic calculations for maximum speed in inner loops.
 * @param type Concrete C type (e.g., int, my_struct_t).
 * @param arr Pointer to the array.
 * @param index Element index.
 */
#define DYNARRAY_GET_AS(type, arr, index) (((type*)(arr)->data)[index])

#ifdef __cplusplus
}
#endif

#endif  // DYNARRAY_H
