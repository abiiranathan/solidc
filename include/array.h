#ifndef DARRAY_H
#define DARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARRAY_INIT_CAPACITY 16

#define ARRAY_DEFINE(name, type)                                                                             \
    typedef struct name {                                                                                    \
        type* items;                                                                                         \
        size_t count;                                                                                        \
        size_t capacity;                                                                                     \
    } name;                                                                                                  \
                                                                                                             \
    static inline name* name##_new(void) {                                                                   \
        name* arr = (name*)malloc(sizeof(name));                                                             \
        if (!arr) {                                                                                          \
            perror("malloc");                                                                                \
            exit(1);                                                                                         \
        }                                                                                                    \
        arr->items = (type*)malloc(ARRAY_INIT_CAPACITY * sizeof(type));                                      \
        if (!arr->items) {                                                                                   \
            perror("malloc");                                                                                \
            free(arr);                                                                                       \
            exit(1);                                                                                         \
        }                                                                                                    \
        arr->count    = 0;                                                                                   \
        arr->capacity = ARRAY_INIT_CAPACITY;                                                                 \
        return arr;                                                                                          \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_resize(name* arr, size_t new_capacity) {                                       \
        if (new_capacity < arr->count) {                                                                     \
            arr->count = new_capacity;                                                                       \
        }                                                                                                    \
                                                                                                             \
        type* new_items = (type*)realloc(arr->items, new_capacity * sizeof(*arr->items));                    \
        if (new_items == NULL && new_capacity > 0) {                                                         \
            perror("realloc");                                                                               \
            free(arr->items);                                                                                \
            exit(1);                                                                                         \
        }                                                                                                    \
                                                                                                             \
        arr->items    = new_items;                                                                           \
        arr->capacity = new_capacity;                                                                        \
    }                                                                                                        \
                                                                                                             \
    static inline void name##vec_shrink(name* arr) {                                                         \
        if (arr->capacity > arr->count) {                                                                    \
            name##_resize(arr, arr->count);                                                                  \
        }                                                                                                    \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_append(name* arr, type x) {                                                    \
        if (arr->count >= arr->capacity) {                                                                   \
            size_t new_capacity = arr->capacity == 0 ? ARRAY_INIT_CAPACITY : arr->capacity * 2;              \
            name##_resize(arr, new_capacity);                                                                \
        }                                                                                                    \
        arr->items[arr->count++] = x;                                                                        \
    }                                                                                                        \
                                                                                                             \
    static inline type name##_get(const name* arr, size_t index) {                                           \
        if (index >= arr->count) {                                                                           \
            fprintf(stderr, "Index out of bounds\n");                                                        \
            exit(1);                                                                                         \
        }                                                                                                    \
        return arr->items[index];                                                                            \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_set(name* arr, size_t index, type value) {                                     \
        if (index >= arr->count) {                                                                           \
            fprintf(stderr, "Index out of bounds\n");                                                        \
            exit(1);                                                                                         \
        }                                                                                                    \
        arr->items[index] = value;                                                                           \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_insert(name* arr, size_t index, type value) {                                  \
        if (index > arr->count) {                                                                            \
            fprintf(stderr, "Index out of bounds\n");                                                        \
            return;                                                                                          \
        }                                                                                                    \
        if (arr->count >= arr->capacity) {                                                                   \
            size_t new_capacity = arr->capacity == 0 ? 1 : arr->capacity * 2;                                \
            name##_resize(arr, new_capacity);                                                                \
        }                                                                                                    \
        memmove(&arr->items[index + 1], &arr->items[index], (arr->count - index) * sizeof(type));            \
        arr->items[index] = value;                                                                           \
        arr->count++;                                                                                        \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_remove(name* arr, size_t index) {                                              \
        if (index >= arr->count) {                                                                           \
            fprintf(stderr, "Index out of bounds\n");                                                        \
            return;                                                                                          \
        }                                                                                                    \
        memmove(&arr->items[index], &arr->items[index + 1], (arr->count - index - 1) * sizeof(type));        \
        arr->count--;                                                                                        \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_clear(name* arr) {                                                             \
        memset(arr->items, 0, arr->count * sizeof(*arr->items));                                             \
        arr->count = 0;                                                                                      \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_free(name* arr) {                                                              \
        if (arr->items) {                                                                                    \
            free(arr->items);                                                                                \
            arr->items = NULL;                                                                               \
        }                                                                                                    \
        free(arr);                                                                                           \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_copy(name* dest, name* src) {                                                  \
        name##_resize(dest, src->capacity);                                                                  \
        memcpy(dest->items, src->items, src->count * sizeof(*src->items));                                   \
        dest->count = src->count;                                                                            \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_swap(name* arr1, name* arr2) {                                                 \
        name temp = *arr1;                                                                                   \
        *arr1     = *arr2;                                                                                   \
        *arr2     = temp;                                                                                    \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_reverse(name* arr) {                                                           \
        for (size_t i = 0; i < arr->count / 2; ++i) {                                                        \
            type temp                      = arr->items[i];                                                  \
            arr->items[i]                  = arr->items[arr->count - i - 1];                                 \
            arr->items[arr->count - i - 1] = temp;                                                           \
        }                                                                                                    \
    }                                                                                                        \
                                                                                                             \
    static inline void name##_sort(name* arr, int (*cmp)(const type*, const type*)) {                        \
        qsort(arr->items, arr->count, sizeof(*arr->items), (int (*)(const void*, const void*))cmp);          \
    }

#ifdef __cplusplus
}
#endif

#endif
