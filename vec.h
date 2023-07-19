
#ifndef __VEC_H__
#define __VEC_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define VEC_DEFINE(T)                                                                                      \
    typedef struct {                                                                                       \
        T* data;                                                                                           \
        size_t size;                                                                                       \
        size_t capacity;                                                                                   \
    } Vec_##T;                                                                                             \
                                                                                                           \
    /*Allocate a new vector and initialize it. If allocation fails, *vec is NULL */                        \
    void vec_init_##T(Vec_##T** vec) {                                                                     \
        *vec = malloc(sizeof(Vec_##T));                                                                    \
        if (*vec == NULL)                                                                                  \
            return;                                                                                        \
        (*vec)->data     = NULL;                                                                           \
        (*vec)->size     = 0;                                                                              \
        (*vec)->capacity = 0;                                                                              \
    }                                                                                                      \
    /* Resizes the vector to the new size. If reallocation fails, *vec data will be null(returns false) */ \
    bool vec_resize_##T(Vec_##T* vec, size_t new_size) {                                                   \
        if (new_size > vec->capacity) {                                                                    \
            size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity;                                  \
            while (new_capacity < new_size) {                                                              \
                new_capacity *= 2;                                                                         \
            }                                                                                              \
            vec->data = realloc(vec->data, new_capacity * sizeof(T));                                      \
            if (vec->data == NULL) {                                                                       \
                return false;                                                                              \
            }                                                                                              \
            vec->capacity = new_capacity;                                                                  \
        }                                                                                                  \
        vec->size = new_size;                                                                              \
        return true;                                                                                       \
    }                                                                                                      \
    void vec_push_back_##T(Vec_##T* vec, T value) {                                                        \
        if (vec->size == vec->capacity) {                                                                  \
            size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity * 2;                              \
            vec->data           = realloc(vec->data, new_capacity * sizeof(T));                            \
            vec->capacity       = new_capacity;                                                            \
        }                                                                                                  \
        vec->data[vec->size++] = value;                                                                    \
    }                                                                                                      \
    /* Frees memory occupied by vec.    */                                                                 \
    void vec_free_##T(Vec_##T** vec) {                                                                     \
        if (vec == NULL || *vec == NULL)                                                                   \
            return;                                                                                        \
        free((*vec)->data);                                                                                \
        (*vec)->data = NULL;                                                                               \
        free(*vec);                                                                                        \
        *vec = NULL;                                                                                       \
    }                                                                                                      \
    /* Remove element at index from vector. If index is out of bounds, it does nothing    */               \
    void vec_remove_##T(Vec_##T* vec, size_t index) {                                                      \
        if (index >= vec->size) {                                                                          \
            printf("Index out of bounds\n");                                                               \
            return;                                                                                        \
        }                                                                                                  \
        for (size_t i = index; i < vec->size - 1; i++) {                                                   \
            vec->data[i] = vec->data[i + 1];                                                               \
        }                                                                                                  \
        vec->size--;                                                                                       \
    }                                                                                                      \
    /* Insert value at index into vector. If index is out of bounds, it returns false    */                \
    bool vec_insert_##T(Vec_##T* vec, size_t index, T value) {                                             \
        if (index > vec->size) {                                                                           \
            printf("Index out of bounds\n");                                                               \
            return false;                                                                                  \
        }                                                                                                  \
        if (vec->size == vec->capacity) {                                                                  \
            size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity * 2;                              \
            vec->data           = realloc(vec->data, new_capacity * sizeof(T));                            \
            vec->capacity       = new_capacity;                                                            \
        }                                                                                                  \
        for (size_t i = vec->size; i > index; i--) {                                                       \
            vec->data[i] = vec->data[i - 1];                                                               \
        }                                                                                                  \
        vec->data[index] = value;                                                                          \
        vec->size++;                                                                                       \
        return true;                                                                                       \
    }                                                                                                      \
    /* Put element at index into result.    */                                                             \
    bool vec_get_##T(Vec_##T* vec, size_t index, T* result) {                                              \
        if (index >= vec->size) {                                                                          \
            printf("Index out of bounds\n");                                                               \
            return false;                                                                                  \
        }                                                                                                  \
        *result = vec->data[index];                                                                        \
        return true;                                                                                       \
    }                                                                                                      \
    /* Remove last element of vector and put it into result    */                                          \
    bool vec_pop_back_##T(Vec_##T* vec, T* result) {                                                       \
        if (vec->size == 0) {                                                                              \
            return false;                                                                                  \
        }                                                                                                  \
        vec->size--;                                                                                       \
        *result = vec->data[vec->size];                                                                    \
        return true;                                                                                       \
    }                                                                                                      \
    /* Clear vector and resize it to 0 memory    */                                                        \
    void vec_clear_##T(Vec_##T* vec) {                                                                     \
        vec->size = 0;                                                                                     \
        if (vec->capacity > 0) {                                                                           \
            vec->data     = realloc(vec->data, 0);                                                         \
            vec->capacity = 0;                                                                             \
        }                                                                                                  \
    }                                                                                                      \
    /* sort vector using qsort    */                                                                       \
    void vec_sort_##T(Vec_##T* vec,                                                                        \
                      int (*comparator)(const void*, const void*)) {                                       \
        qsort(vec->data, vec->size, sizeof(T), comparator);                                                \
    }

#endif /* __VEC_H__ */
