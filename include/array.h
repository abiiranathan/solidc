#ifndef __ARRAY__H__
#define __ARRAY__H__

#include <stdbool.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration constants
#define DARRAY_INIT_CAPACITY 16
#define DARRAY_MAX_CAPACITY  (SIZE_MAX / 2)
#define DARRAY_GROWTH_FACTOR 2

// Compiler-specific optimizations
#ifdef __GNUC__
#define DARRAY_LIKELY(x)   __builtin_expect(!!(x), 1)
#define DARRAY_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define DARRAY_INLINE      __attribute__((always_inline)) static inline
#define DARRAY_NOINLINE    __attribute__((noinline))
#define DARRAY_PURE        __attribute__((pure))
#define DARRAY_CONST       __attribute__((const))
#define DARRAY_RESTRICT    __restrict__
#else
#define DARRAY_LIKELY(x)   (x)
#define DARRAY_UNLIKELY(x) (x)
#define DARRAY_INLINE      static inline
#define DARRAY_NOINLINE
#define DARRAY_PURE
#define DARRAY_CONST
#define DARRAY_RESTRICT
#endif

// Error handling
typedef enum {
    DARRAY_OK = 0,
    DARRAY_ERROR_NULL_POINTER,
    DARRAY_ERROR_OUT_OF_BOUNDS,
    DARRAY_ERROR_OUT_OF_MEMORY,
    DARRAY_ERROR_OVERFLOW,
    DARRAY_ERROR_INVALID_ARGUMENT
} darray_error_t;

// Make error reporting optional/controllable
#ifndef DARRAY_DISABLE_ERRORS
#define DARRAY_ERROR(err, msg) fprintf(stderr, "%s:%d:darray error %d: %s\n", __FILE__, __LINE__, err, msg);
#else
#define DARRAY_ERROR(err, msg)
#endif

// Validation macros
#define DARRAY_CHECK_NULL(ptr, err_code)                                                                               \
    do {                                                                                                               \
        if (DARRAY_UNLIKELY(!(ptr))) {                                                                                 \
            DARRAY_ERROR(err_code, "null pointer");                                                                    \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define DARRAY_CHECK_NULL_RET(ptr, err_code, ret_val)                                                                  \
    do {                                                                                                               \
        if (DARRAY_UNLIKELY(!(ptr))) {                                                                                 \
            DARRAY_ERROR(err_code, "null pointer");                                                                    \
            return ret_val;                                                                                            \
        }                                                                                                              \
    } while (0)

#define DARRAY_CHECK_BOUNDS(arr, index)                                                                                \
    do {                                                                                                               \
        if (DARRAY_UNLIKELY((index) >= (arr)->count)) {                                                                \
            DARRAY_ERROR(DARRAY_ERROR_OUT_OF_BOUNDS, "index out of bounds");                                           \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define DARRAY_CHECK_BOUNDS_RET(arr, index, ret_val)                                                                   \
    do {                                                                                                               \
        if (DARRAY_UNLIKELY((index) >= (arr)->count)) {                                                                \
            DARRAY_ERROR(DARRAY_ERROR_OUT_OF_BOUNDS, "index out of bounds");                                           \
            return ret_val;                                                                                            \
        }                                                                                                              \
    } while (0)

// NOLINTBEGIN(bugprone-macro-parentheses)
// Core macro that creates the array code.
#define DARRAY_DEFINE(name, type)                                                                                      \
    typedef struct name {                                                                                              \
        type* DARRAY_RESTRICT items;                                                                                   \
        size_t count;                                                                                                  \
        size_t capacity;                                                                                               \
        uint32_t magic; /* Corruption detection */                                                                     \
    } name;                                                                                                            \
                                                                                                                       \
    static const uint32_t name##_MAGIC = 0xDEADBEEF ^ (uint32_t)(__COUNTER__ + __LINE__);                              \
                                                                                                                       \
    /* Type-safe validation */                                                                                         \
    DARRAY_INLINE bool name##_is_valid(const name* arr) {                                                              \
        return arr && arr->magic == name##_MAGIC && arr->count <= arr->capacity;                                       \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE name* name##_new(void) {                                                                             \
        name* arr = (name*)calloc(1, sizeof(name));                                                                    \
        if (DARRAY_UNLIKELY(!arr)) {                                                                                   \
            DARRAY_ERROR(DARRAY_ERROR_OUT_OF_MEMORY, "failed to allocate array");                                      \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        arr->items = (type*)malloc(DARRAY_INIT_CAPACITY * sizeof(type));                                               \
        if (DARRAY_UNLIKELY(!arr->items)) {                                                                            \
            free(arr);                                                                                                 \
            DARRAY_ERROR(DARRAY_ERROR_OUT_OF_MEMORY, "failed to allocate items");                                      \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        arr->count    = 0;                                                                                             \
        arr->capacity = DARRAY_INIT_CAPACITY;                                                                          \
        arr->magic    = name##_MAGIC;                                                                                  \
        return arr;                                                                                                    \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE name* name##_with_capacity(size_t initial_capacity) {                                                \
        if (DARRAY_UNLIKELY(initial_capacity == 0)) initial_capacity = DARRAY_INIT_CAPACITY;                           \
        if (DARRAY_UNLIKELY(initial_capacity > DARRAY_MAX_CAPACITY)) {                                                 \
            DARRAY_ERROR(DARRAY_ERROR_OVERFLOW, "capacity too large");                                                 \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        name* arr = (name*)calloc(1, sizeof(name));                                                                    \
        if (DARRAY_UNLIKELY(!arr)) {                                                                                   \
            DARRAY_ERROR(DARRAY_ERROR_OUT_OF_MEMORY, "failed to allocate array");                                      \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        size_t byte_size;                                                                                              \
        if (DARRAY_UNLIKELY(ckd_mul(&byte_size, initial_capacity, sizeof(type)))) {                                    \
            free(arr);                                                                                                 \
            DARRAY_ERROR(DARRAY_ERROR_OVERFLOW, "capacity overflow");                                                  \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        arr->items = (type*)malloc(byte_size);                                                                         \
        if (DARRAY_UNLIKELY(!arr->items)) {                                                                            \
            free(arr);                                                                                                 \
            DARRAY_ERROR(DARRAY_ERROR_OUT_OF_MEMORY, "failed to allocate items");                                      \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        arr->count    = 0;                                                                                             \
        arr->capacity = initial_capacity;                                                                              \
        arr->magic    = name##_MAGIC;                                                                                  \
        return arr;                                                                                                    \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_resize(name* arr, size_t new_capacity) {                                                 \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        if (DARRAY_UNLIKELY(!name##_is_valid(arr))) {                                                                  \
            DARRAY_ERROR(DARRAY_ERROR_INVALID_ARGUMENT, "invalid array");                                              \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        if (new_capacity == arr->capacity) return true;                                                                \
                                                                                                                       \
        if (DARRAY_UNLIKELY(new_capacity > DARRAY_MAX_CAPACITY)) {                                                     \
            DARRAY_ERROR(DARRAY_ERROR_OVERFLOW, "capacity too large");                                                 \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        size_t byte_size;                                                                                              \
        if (DARRAY_UNLIKELY(ckd_mul(&byte_size, new_capacity, sizeof(type)))) {                                        \
            DARRAY_ERROR(DARRAY_ERROR_OVERFLOW, "capacity overflow");                                                  \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        type* new_items;                                                                                               \
        if (new_capacity == 0) {                                                                                       \
            free(arr->items);                                                                                          \
            new_items = NULL;                                                                                          \
        } else {                                                                                                       \
            new_items = (type*)realloc(arr->items, byte_size);                                                         \
            if (DARRAY_UNLIKELY(!new_items)) {                                                                         \
                DARRAY_ERROR(DARRAY_ERROR_OUT_OF_MEMORY, "failed to reallocate");                                      \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        arr->items    = new_items;                                                                                     \
        arr->capacity = new_capacity;                                                                                  \
        if (arr->count > new_capacity) {                                                                               \
            arr->count = new_capacity;                                                                                 \
        }                                                                                                              \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_reserve(name* arr, size_t min_capacity) {                                                \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        if (arr->capacity >= min_capacity) return true;                                                                \
                                                                                                                       \
        size_t new_capacity = arr->capacity;                                                                           \
        while (new_capacity < min_capacity) {                                                                          \
            if (DARRAY_UNLIKELY(new_capacity > DARRAY_MAX_CAPACITY / DARRAY_GROWTH_FACTOR)) {                          \
                new_capacity = DARRAY_MAX_CAPACITY;                                                                    \
                break;                                                                                                 \
            }                                                                                                          \
            new_capacity *= DARRAY_GROWTH_FACTOR;                                                                      \
        }                                                                                                              \
                                                                                                                       \
        return name##_resize(arr, new_capacity);                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_shrink_to_fit(name* arr) {                                                               \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        return name##_resize(arr, arr->count);                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_push(name* arr, type value) {                                                            \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        if (DARRAY_UNLIKELY(!name##_is_valid(arr))) {                                                                  \
            DARRAY_ERROR(DARRAY_ERROR_INVALID_ARGUMENT, "invalid array");                                              \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        if (DARRAY_UNLIKELY(arr->count >= arr->capacity)) {                                                            \
            size_t new_capacity = arr->capacity == 0 ? DARRAY_INIT_CAPACITY : arr->capacity * DARRAY_GROWTH_FACTOR;    \
            if (!name##_resize(arr, new_capacity)) return false;                                                       \
        }                                                                                                              \
                                                                                                                       \
        arr->items[arr->count++] = value;                                                                              \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE type name##_get(const name* arr, size_t index) {                                                     \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, (type){0});                                              \
        DARRAY_CHECK_BOUNDS_RET(arr, index, (type){0});                                                                \
        return arr->items[index];                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE type* name##_get_ptr(name* arr, size_t index) {                                                      \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, NULL);                                                   \
        DARRAY_CHECK_BOUNDS_RET(arr, index, NULL);                                                                     \
        return &arr->items[index];                                                                                     \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE const type* name##_get_const_ptr(const name* arr, size_t index) {                                    \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, NULL);                                                   \
        DARRAY_CHECK_BOUNDS_RET(arr, index, NULL);                                                                     \
        return (const type*)&arr->items[index];                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_set(name* arr, size_t index, type value) {                                               \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        DARRAY_CHECK_BOUNDS_RET(arr, index, false);                                                                    \
        arr->items[index] = value;                                                                                     \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_insert(name* arr, size_t index, type value) {                                            \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        if (DARRAY_UNLIKELY(index > arr->count)) {                                                                     \
            DARRAY_ERROR(DARRAY_ERROR_OUT_OF_BOUNDS, "insert index out of bounds");                                    \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        if (DARRAY_UNLIKELY(arr->count >= arr->capacity)) {                                                            \
            size_t new_capacity = arr->capacity == 0 ? DARRAY_INIT_CAPACITY : arr->capacity * DARRAY_GROWTH_FACTOR;    \
            if (!name##_resize(arr, new_capacity)) return false;                                                       \
        }                                                                                                              \
                                                                                                                       \
        if (index < arr->count) {                                                                                      \
            memmove(&arr->items[index + 1], &arr->items[index], (arr->count - index) * sizeof(type));                  \
        }                                                                                                              \
        arr->items[index] = value;                                                                                     \
        arr->count++;                                                                                                  \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_remove(name* arr, size_t index) {                                                        \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        DARRAY_CHECK_BOUNDS_RET(arr, index, false);                                                                    \
                                                                                                                       \
        if (index < arr->count - 1) {                                                                                  \
            memmove(&arr->items[index], &arr->items[index + 1], (arr->count - index - 1) * sizeof(type));              \
        }                                                                                                              \
        arr->count--;                                                                                                  \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_pop(name* arr, type* out_value) {                                                        \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        if (DARRAY_UNLIKELY(arr->count == 0)) return false;                                                            \
                                                                                                                       \
        if (out_value) *out_value = arr->items[arr->count - 1];                                                        \
        arr->count--;                                                                                                  \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE void name##_clear(name* arr) {                                                                       \
        DARRAY_CHECK_NULL(arr, DARRAY_ERROR_NULL_POINTER);                                                             \
        arr->count = 0;                                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE void name##_free(name* arr) {                                                                        \
        if (!arr) return;                                                                                              \
        if (arr->items) {                                                                                              \
            free(arr->items);                                                                                          \
            arr->items = NULL;                                                                                         \
        }                                                                                                              \
        arr->magic = 0; /* Invalidate */                                                                               \
        free(arr);                                                                                                     \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE name* name##_copy(const name* src) {                                                                 \
        DARRAY_CHECK_NULL_RET(src, DARRAY_ERROR_NULL_POINTER, NULL);                                                   \
                                                                                                                       \
        name* dest = name##_with_capacity(src->capacity);                                                              \
        if (!dest) return NULL;                                                                                        \
                                                                                                                       \
        if (src->count > 0) {                                                                                          \
            memcpy(dest->items, src->items, src->count * sizeof(type));                                                \
        }                                                                                                              \
        dest->count = src->count;                                                                                      \
        return dest;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE void name##_swap(name* arr1, name* arr2) {                                                           \
        DARRAY_CHECK_NULL(arr1, DARRAY_ERROR_NULL_POINTER);                                                            \
        DARRAY_CHECK_NULL(arr2, DARRAY_ERROR_NULL_POINTER);                                                            \
                                                                                                                       \
        name temp = *arr1;                                                                                             \
        *arr1     = *arr2;                                                                                             \
        *arr2     = temp;                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE void name##_reverse(name* arr) {                                                                     \
        DARRAY_CHECK_NULL(arr, DARRAY_ERROR_NULL_POINTER);                                                             \
                                                                                                                       \
        if (arr->count < 2) {                                                                                          \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        type* items  = arr->items;                                                                                     \
        size_t left  = 0;                                                                                              \
        size_t right = arr->count - 1;                                                                                 \
                                                                                                                       \
        while (left < right) {                                                                                         \
            type temp    = items[left];                                                                                \
            items[left]  = items[right];                                                                               \
            items[right] = temp;                                                                                       \
            left++;                                                                                                    \
            right--;                                                                                                   \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE bool name##_sort(name* arr, int (*cmp)(const void*, const void*)) {                                  \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, false);                                                  \
        DARRAY_CHECK_NULL_RET(cmp, DARRAY_ERROR_NULL_POINTER, false);                                                  \
                                                                                                                       \
        if (arr->count > 1) {                                                                                          \
            qsort(arr->items, arr->count, sizeof(type), cmp);                                                          \
        }                                                                                                              \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    /* Additional utility functions */                                                                                 \
    DARRAY_PURE DARRAY_INLINE size_t name##_size(const name* arr) {                                                    \
        return arr ? arr->count : 0;                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_PURE DARRAY_INLINE size_t name##_capacity(const name* arr) {                                                \
        return arr ? arr->capacity : 0;                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_PURE DARRAY_INLINE bool name##_empty(const name* arr) {                                                     \
        return !arr || arr->count == 0;                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_PURE DARRAY_INLINE type* name##_data(name* arr) {                                                           \
        return arr ? arr->items : NULL;                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_PURE DARRAY_INLINE const type* name##_const_data(const name* arr) {                                         \
        return (const type*)(arr ? arr->items : NULL);                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    DARRAY_INLINE ptrdiff_t name##_find(const name* arr, const type* value, int (*cmp)(const void*, const void*)) {    \
        DARRAY_CHECK_NULL_RET(arr, DARRAY_ERROR_NULL_POINTER, -1);                                                     \
        DARRAY_CHECK_NULL_RET(value, DARRAY_ERROR_NULL_POINTER, -1);                                                   \
                                                                                                                       \
        for (size_t i = 0; i < arr->count; i++) {                                                                      \
            if (cmp) {                                                                                                 \
                if (cmp(&arr->items[i], value) == 0) return (ptrdiff_t)i;                                              \
            } else {                                                                                                   \
                if (memcmp(&arr->items[i], value, sizeof(type)) == 0) return (ptrdiff_t)i;                             \
            }                                                                                                          \
        }                                                                                                              \
        return -1;                                                                                                     \
    }

// NOLINTEND(bugprone-macro-parentheses)
// Convenience macros for common operations
#define DARRAY_FOR_EACH(arr, var) for (size_t _i = 0; _i < (arr)->count && ((var) = (arr)->items[_i], 1); _i++)

#define DARRAY_FOR_EACH_PTR(arr, var) for (size_t _i = 0; _i < (arr)->count && ((var) = &(arr)->items[_i], 1); _i++)

#ifdef __cplusplus
}
#endif

#endif /* __ARRAY__H__ */
