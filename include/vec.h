#ifndef SOLIDC_VEC_H
#define SOLIDC_VEC_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

// Function pointer type for custom comparison functions
// The function should return true if the two elements are equal.
// We can't use == to compare the elements because we don't know their type.
typedef bool (*vec_cmp_fn)(const void*, const void*);

// Vector struct
typedef struct vec_t vec_t;

/**
* Creates a new vector with the specified capacity. If capacity is 0, the vector will have an initial capacity of 1024.
* The vector will use the specified comparison function to compare elements.
* */
vec_t* vec_new(size_t capacity, vec_cmp_fn cmp);

// Frees the vector and its data if heap-allocated
void vec_free(vec_t* vec);

// Pushes an element to the end of the vector.
// elem should be a pointer to heap-allocated memory. otherwise, the behavior is undefined.
void vec_push(vec_t* vec, void* elem);

// Removes the last element from the vector
void vec_pop(vec_t* vec);

// Inserts an element at the specified index
void* vec_get(vec_t* vec, size_t index);

// Sets the element at the specified index
// elem should be a pointer to heap-allocated memory. otherwise, the behavior is undefined.
void vec_set(vec_t* vec, size_t index, void* elem);

// Returns the size of the vector
size_t vec_size(vec_t* vec);

// Returns the capacity of the vector
size_t vec_capacity(vec_t* vec);

// Reserves capacity for the vector. If the realloc
// fails, this function returns false.
bool vec_reserve(vec_t* vec, size_t capacity);

// Shrinks the vector to fit its size
void vec_shrink(vec_t* vec);

// Clears the vector
void vec_clear(vec_t* vec);

// Swap the two vectors
void vec_swap(vec_t** vec1, vec_t** vec2);

// Copy the vector
vec_t* vec_copy(vec_t* vec);

// Returns true if the vector contains the element
bool vec_contains(vec_t* vec, void* elem);

// Returns the index of the element in the vector, or -1 if the element is not found
int vec_find_index(vec_t* vec, void* elem);

// Reverse the vector
void vec_reverse(vec_t* vec);

// int comparison function
bool int_cmp(const void* a, const void* b);

// char* comparison function
bool str_cmp(const void* a, const void* b);

// float comparison function
bool float_cmp(const void* a, const void* b);

// double comparison function
bool double_cmp(const void* a, const void* b);

// size_t comparison function
bool size_t_cmp(const void* a, const void* b);

#define vec_foreach(vec, index_var)                                                                \
    do {                                                                                           \
        for (size_t index_var = 0; (index_var) < vec_size(vec); (index_var)++)                     \
    } while (0)

#define vec_foreach_ptr(vec, index, elem)                                                          \
    do {                                                                                           \
        for (size_t index = 0; (index) < vec_size(vec); (index)++)                                 \
            for (int cont = 1; cont; cont = 0)                                                     \
                for (void*(elem) = vec_get(vec, index); cont; cont = 0)                            \
    } while (0)

#if defined(__cplusplus)
}
#endif

#endif  // SOLIDC_VEC_H
