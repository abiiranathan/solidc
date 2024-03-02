#ifndef SOLIDC_VEC_H
#define SOLIDC_VEC_H

#if defined(__cplusplus)
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void** data;
    size_t size;
    size_t capacity;
    size_t elem_size;
    bool heap_allocated;
} vec_t;

vec_t* vec_new(size_t capacity, bool heap_allocated);

void vec_free(vec_t* vec);

void vec_push(vec_t* vec, void* elem);

void vec_pop(vec_t* vec);

void* vec_get(vec_t* vec, size_t index);

void vec_set(vec_t* vec, size_t index, void* elem);

size_t vec_size(vec_t* vec);

size_t vec_capacity(vec_t* vec);

void vec_reserve(vec_t* vec, size_t capacity);

void vec_shrink(vec_t* vec);

void vec_clear(vec_t* vec);

void vec_swap(vec_t** vec1, vec_t** vec2);

vec_t* vec_copy(vec_t* vec);

bool vec_contains(vec_t* vec, void* elem);

int vec_find_index(vec_t* vec, void* elem);

void vec_reverse(vec_t* vec);

#define vec_foreach(vec, index_var)                                                                \
    for (size_t index_var = 0; index_var < vec_size(vec); index_var++)

#define vec_foreach_ptr(vec, index, elem)                                                          \
    for (size_t index = 0; index < vec_size(vec); index++)                                         \
        for (int cont = 1; cont; cont = 0)                                                         \
            for (void* elem = vec_get(vec, index); cont; cont = 0)

#if defined(__cplusplus)
}
#endif

#ifdef VEC_IMPL

vec_t* vec_new(size_t capacity, bool heap_allocated) {
    vec_t* vec = (vec_t*)malloc(sizeof(vec_t));
    if (vec == NULL) {
        perror("vec_new() failed");
        exit(EXIT_FAILURE);
    }

    vec->data           = NULL;
    vec->size           = 0;
    vec->capacity       = 0;
    vec->heap_allocated = heap_allocated;

    if (capacity > 0) {
        vec_reserve(vec, capacity);
    }
    return vec;
}

void vec_free(vec_t* vec) {
    if (!vec)
        return;

    if (vec->heap_allocated) {
        for (size_t i = 0; i < vec->size; i++) {
            free(vec->data[i]);
        }
    }

    free(vec->data);
    vec->data = NULL;

    free(vec);
    vec = NULL;
}

void vec_push(vec_t* vec, void* elem) {
    if (vec->size == vec->capacity) {
        vec_reserve(vec, vec->capacity == 0 ? 1 : vec->capacity * 2);
    }

    vec->data[vec->size++] = elem;
}

void vec_pop(vec_t* vec) {
    if (vec->size > 0) {
        vec->size--;

        if (vec->size <= vec->capacity / 2) {
            vec_shrink(vec);
        }
    }
}

void* vec_get(vec_t* vec, size_t index) {
    if (index < vec->size) {
        return vec->data[index];
    }
    return NULL;
}

void vec_set(vec_t* vec, size_t index, void* elem) {
    if (index < vec->size) {
        vec->data[index] = elem;
    }
}

size_t vec_size(vec_t* vec) {
    return vec->size;
}

size_t vec_capacity(vec_t* vec) {
    return vec->capacity;
}

void vec_reserve(vec_t* vec, size_t capacity) {
    if (capacity > vec->capacity) {
        vec->data     = (void**)realloc(vec->data, capacity * sizeof(void*));
        vec->capacity = capacity;

        if (vec->data == NULL) {
            perror("vec_reserve() failed");
            exit(EXIT_FAILURE);
        }
    }
}

void vec_shrink(vec_t* vec) {
    if (vec->size < vec->capacity) {
        vec->data     = (void**)realloc(vec->data, (vec->size + 1) * sizeof(void*));
        vec->capacity = vec->size;

        if (vec->data == NULL) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
}

void vec_clear(vec_t* vec) {
    vec->size = 0;
    vec_shrink(vec);
}

void vec_swap(vec_t** vec1, vec_t** vec2) {
    vec_t* temp = *vec1;
    *vec1       = *vec2;
    *vec2       = temp;
}

vec_t* vec_copy(vec_t* vec) {
    vec_t* copy = vec_new(vec->capacity, vec->heap_allocated);
    if (copy != NULL) {
        vec_reserve(copy, vec->capacity);
        memcpy(copy->data, vec->data, vec->size * sizeof(void*));
        copy->size = vec->size;
    }
    return copy;
}

bool vec_contains(vec_t* vec, void* elem) {
    for (size_t i = 0; i < vec->size; ++i) {
        if (memcmp(vec_get(vec, i), elem, sizeof(void*)) == 0) {
            return true;
        }
    }
    return false;
}

int vec_find_index(vec_t* vec, void* elem) {
    for (size_t i = 0; i < vec->size; ++i) {
        if (memcmp(vec_get(vec, i), elem, sizeof(void*)) == 0) {
            return i;
        }
    }
    return -1;
}

void vec_reverse(vec_t* vec) {
    size_t i   = 0;
    size_t j   = vec->size - 1;
    void* temp = NULL;
    while (i < j) {
        temp         = vec->data[i];
        vec->data[i] = vec->data[j];
        vec->data[j] = temp;
        i++;
        j--;
    }
}

#endif  // VEC_IMPL

#endif  // SOLIDC_VEC_H