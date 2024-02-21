#ifndef D45CFCB0_A7C9_4816_B36D_BBC0724921FA
#define D45CFCB0_A7C9_4816_B36D_BBC0724921FA

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
void vec_swap(vec_t* vec1, vec_t* vec2);

// Copy the vector
vec_t* vec_copy(vec_t* vec);

// vec_contains
bool vec_contains(vec_t* vec, void* elem);

// vec_find_index
size_t vec_find_index(vec_t* vec, void* elem);

// Implementation
#ifdef VEC_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

vec_t* vec_new(size_t elem_size, size_t capacity) {
    vec_t* vec = malloc(sizeof(vec_t));
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
        return vec->data + (index * vec->elem_size);
    }
    return NULL;
}

// Set the element at the given index
void vec_set(vec_t* vec, size_t index, void* elem) {
    if (index < vec->size) {
        memcpy(vec->data + (index * vec->elem_size), elem, vec->elem_size);
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
        vec->data     = realloc(vec->data, vec->size * vec->elem_size);
        vec->capacity = vec->size;

        if (vec->data == NULL) {
            perror("vec_shrink() failed");
            exit(EXIT_FAILURE);
        }
    }
}

void vec_clear(vec_t* vec) {
    vec->size = 0;
    vec_shrink(vec);
}

void vec_swap(vec_t* vec1, vec_t* vec2) {
    vec_t temp = *vec1;
    *vec1      = *vec2;
    *vec2      = temp;

    // Swap the data pointers
    void* data = vec1->data;
    vec1->data = vec2->data;
    vec2->data = data;

    // Swap the capacity
    size_t capacity = vec1->capacity;
    vec1->capacity  = vec2->capacity;
    vec2->capacity  = capacity;

    // Swap the size
    size_t size = vec1->size;
    vec1->size  = vec2->size;
    vec2->size  = size;

    // Swap the element size
    size_t elem_size = vec1->elem_size;
    vec1->elem_size  = vec2->elem_size;
    vec2->elem_size  = elem_size;
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

size_t vec_find_index(vec_t* vec, void* elem) {
    for (size_t i = 0; i < vec->size; i++) {
        if (memcmp(vec_get(vec, i), elem, vec->elem_size) == 0) {
            return i;
        }
    }
    return -1;
}

#endif

#endif /* D45CFCB0_A7C9_4816_B36D_BBC0724921FA */
