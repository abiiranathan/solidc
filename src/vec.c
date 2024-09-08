#include "../include/vec.h"
#include "../include/arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 1024

typedef struct vec_t {
    void** data;      // array of pointers to elements
    size_t size;      // number of elements in the vector
    size_t capacity;  // capacity of the vector
    vec_cmp_fn cmp;   // comparison function for elements
    Arena* arena;     // Memory allocation arena for vec
} vec_t;

vec_t* vec_new(size_t capacity, vec_cmp_fn cmp) {
    vec_t* vec = (vec_t*)malloc(sizeof(vec_t));
    if (!vec) {
        perror("vec_new");
        return NULL;
    }

    Arena* arena = arena_create(ARENA_DEFAULT_CHUNKSIZE, SYSTEM_MAX_ALIGNMENT);
    if (!arena) {
        fprintf(stderr, "error creating arena allocator\n");
        return NULL;
    }

    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
    vec->cmp = cmp;
    vec->arena = arena;

    if (capacity == 0) {
        capacity = INITIAL_CAPACITY;
    }

    if (!vec_reserve(vec, capacity)) {
        fprintf(stderr, "vec_reserve(): realloc failed\n");
        arena_destroy(arena);
        free(vec);
        return NULL;
    }
    return vec;
}

void vec_free(vec_t* vec) {
    if (!vec)
        return;

    arena_destroy(vec->arena);
    free(vec);
    vec = NULL;
}

void vec_push(vec_t* vec, void* elem) {
    if (!vec_reserve(vec, vec->capacity + 1)) {
        return;
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
        // we are not freeing the old element, as its allocated in the arena.
        // Insert the new element
        vec->data[index] = elem;
    }
}

size_t vec_size(vec_t* vec) {
    return vec->size;
}

size_t vec_capacity(vec_t* vec) {
    return vec->capacity;
}

bool vec_reserve(vec_t* vec, size_t capacity) {
    if (capacity > vec->capacity) {
        vec->capacity = capacity * 2;
        vec->data = (void**)arena_realloc(vec->arena, vec->data, vec->capacity * sizeof(void*));
        if (vec->data == NULL) {
            fprintf(stderr, "vec_reserve(): arena_realloc failed\n");
            return false;
        }
    }
    return true;
}

void vec_shrink(vec_t* vec) {
    if (vec->size < vec->capacity) {
        vec->capacity = vec->size;
        vec->data = (void**)arena_realloc(vec->arena, vec->data, vec->capacity * sizeof(void*));
        if (vec->data == NULL) {
            perror("arena_realloc");
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
    *vec1 = *vec2;
    *vec2 = temp;
}

vec_t* vec_copy(vec_t* vec) {
    vec_t* copy = vec_new(vec->capacity, vec->cmp);
    if (copy != NULL) {
        if (!vec_reserve(copy, vec->capacity)) {
            vec_free(copy);
            return NULL;
        }

        // copy data into the new vec.
        memcpy(copy->data, vec->data, vec->size * sizeof(void*));
        copy->size = vec->size;
    }
    return copy;
}

bool vec_contains(vec_t* vec, void* elem) {
    for (size_t i = 0; i < vec->size; ++i) {
        if (vec->cmp(vec->data[i], elem)) {
            return true;
        }
    }
    return false;
}

int vec_find_index(vec_t* vec, void* elem) {
    for (size_t i = 0; i < vec->size; ++i) {
        if (vec->cmp(vec->data[i], elem)) {
            return (int)i;
        }
    }
    return -1;
}

void vec_reverse(vec_t* vec) {
    size_t i = 0;
    size_t j = vec->size - 1;
    void* temp = NULL;

    while (i < j) {
        temp = vec->data[i];
        vec->data[i] = vec->data[j];
        vec->data[j] = temp;
        i++;
        j--;
    }
}

bool int_cmp(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return *(int*)a == *(int*)b;
}

bool str_cmp(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return strcmp((char*)a, (char*)b) == 0;
}

bool float_cmp(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return *(float*)a == *(float*)b;
}

bool double_cmp(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return *(double*)a == *(double*)b;
}

bool size_t_cmp(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return *(size_t*)a == *(size_t*)b;
}



