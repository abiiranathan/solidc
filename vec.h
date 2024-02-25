#ifndef D45CFCB0_A7C9_4816_B36D_BBC0724921FA
#define D45CFCB0_A7C9_4816_B36D_BBC0724921FA

#if defined(__cplusplus)
extern "C" {
#endif
// Generic vector type in C
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void* data;
    size_t size;
    size_t capacity;
    size_t elem_size;
} vec_t;

// Create a new vector
vec_t* vec_new(size_t elem_size, size_t capacity);

// Free the vector
void vec_free(vec_t* vec);

// Push an element to the end of the vector
void vec_push(vec_t* vec, void* elem);

// Pop an element from the end of the vector
void vec_pop(vec_t* vec);

// Get the element at the given index
void* vec_get(vec_t* vec, size_t index);

// Set the element at the given index
void vec_set(vec_t* vec, size_t index, void* elem);

// Get the size of the vector
size_t vec_size(vec_t* vec);

// Get the capacity of the vector
size_t vec_capacity(vec_t* vec);

// Reserve space for the vector
void vec_reserve(vec_t* vec, size_t capacity);

// Shrink the vector to fit the size
void vec_shrink(vec_t* vec);

// Clear the vector
void vec_clear(vec_t* vec);

// Swap two vectors
void vec_swap(vec_t** vec1, vec_t** vec2);

// Copy the vector
vec_t* vec_copy(vec_t* vec);

// vec_contains
bool vec_contains(vec_t* vec, void* elem);

// Returns the index of elem in vec or -1 of not found.
int vec_find_index(vec_t* vec, void* elem);

void vec_reverse(vec_t* vec);

#define vec_foreach(vec, index_var)                                                                \
    for (size_t index_var = 0; index_var < vec_size(vec); index_var++)

#define vec_foreach_ptr(vec, index_var, elem_var)                                                  \
    for (size_t index_var = 0; index_var < vec_size(vec); index_var++)                             \
        for (int cont = 1; cont; cont = 0)                                                         \
            for (void* elem_var = (char*)vec->data + (index_var * vec->elem_size); cont; cont = 0)

#if defined(__cplusplus)
}
#endif

// Implementation
#ifdef VEC_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

vec_t* vec_new(size_t elem_size, size_t capacity) {
    vec_t* vec = (vec_t*)malloc(sizeof(vec_t));
    if (vec == NULL) {
        perror("vec_new() failed");
        exit(EXIT_FAILURE);
    }

    vec->data      = NULL;
    vec->size      = 0;
    vec->capacity  = 0;
    vec->elem_size = elem_size;

    if (capacity > 0) {
        vec_reserve(vec, capacity);
    }
    return vec;
}

void vec_free(vec_t* vec) {
    if (!vec)
        return;

    free(vec->data);
    vec->data = NULL;

    free(vec);
    vec = NULL;
}

void vec_push(vec_t* vec, void* elem) {
    if (vec->size == vec->capacity) {
        vec_reserve(vec, vec->capacity == 0 ? 1 : vec->capacity * 2);
    }

    memcpy((char*)vec->data + vec->size * vec->elem_size, elem, vec->elem_size);
    vec->size++;
}

void vec_pop(vec_t* vec) {
    // Pop the last element
    if (vec->size <= 0) {
        return;
    }
    vec->size--;

    // Shrink the vector if the size is less than half the capacity
    if (vec->size <= vec->capacity / 2) {
        vec_shrink(vec);
    }
}

void* vec_get(vec_t* vec, size_t index) {
    if (index < vec->size) {
        return (char*)vec->data + (index * vec->elem_size);
    }
    return NULL;
}

// Set the element at the given index
void vec_set(vec_t* vec, size_t index, void* elem) {
    if (index < vec->size) {
        memcpy((char*)vec->data + (index * vec->elem_size), elem, vec->elem_size);
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
        vec->data     = realloc(vec->data, capacity * vec->elem_size);
        vec->capacity = capacity;

        if (vec->data == NULL) {
            perror("vec_reserve() failed");
            exit(EXIT_FAILURE);
        }
    }
}

void vec_shrink(vec_t* vec) {
    if (vec->size < vec->capacity) {
        vec->data     = realloc(vec->data, (vec->size + 1) * vec->elem_size);
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
    vec_t* copy = vec_new(vec->elem_size, vec->capacity);
    if (copy != NULL) {
        vec_reserve(copy, vec->capacity);
        memcpy(copy->data, vec->data, vec->size * vec->elem_size);
        copy->size = vec->size;
    }
    return copy;
}

bool vec_contains(vec_t* vec, void* elem) {
    for (size_t i = 0; i < vec->size; i++) {
        if (memcmp(vec_get(vec, i), elem, vec->elem_size) == 0) {
            return true;
        }
    }
    return false;
}

int vec_find_index(vec_t* vec, void* elem) {
    for (size_t i = 0; i < vec->size; ++i) {
        if (memcmp(vec_get(vec, i), elem, vec->elem_size) == 0) {
            return (unsigned int)i;
        }
    }
    return -1;
}

void vec_reverse(vec_t* vec) {
    size_t i   = 0;
    size_t j   = vec->size - 1;
    void* temp = malloc(vec->elem_size);
    if (temp == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    while (i < j) {
        memcpy(temp, (char*)vec->data + (i * vec->elem_size), vec->elem_size);
        memcpy((char*)vec->data + (i * vec->elem_size), (char*)vec->data + (j * vec->elem_size),
               vec->elem_size);
        memcpy((char*)vec->data + (j * vec->elem_size), temp, vec->elem_size);
        i++;
        j--;
    }
    free(temp);
}

#endif

#endif /* D45CFCB0_A7C9_4816_B36D_BBC0724921FA */
