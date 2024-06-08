#ifndef CC4C048C_C6DD_4C98_B8A8_E71736327690
#define CC4C048C_C6DD_4C98_B8A8_E71736327690

// Macro-based implementation of type-safe vector operations.
// Alternative to vec.h that is a single generic impl using void* pointers;
// This is faster and more robust.
// Heavily uses assertions to validate pointers. Cimpile with -DNDEBUG
// to remove the assertions.

#include "../include/arena.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFINE_VECTOR(Type, TypeName)                                                              \
    typedef struct TypeName##_t {                                                                  \
        Type* data;                                                                                \
        size_t size;                                                                               \
        size_t capacity;                                                                           \
        Arena* arena;                                                                              \
    } TypeName##_t;                                                                                \
                                                                                                   \
    TypeName##_t* TypeName##_create(size_t initial_capacity) {                                     \
        Arena* arena = arena_create(ARENA_DEFAULT_CHUNKSIZE, ARENA_DEFAULT_ALIGNMENT);             \
        assert(arena);                                                                             \
        TypeName##_t* vec = (TypeName##_t*)malloc(sizeof(TypeName##_t));                           \
        if (vec == NULL) {                                                                         \
            arena_destroy(arena);                                                                  \
            return NULL;                                                                           \
        }                                                                                          \
                                                                                                   \
        vec->data = (Type*)arena_alloc(arena, initial_capacity * sizeof(Type));                    \
        if (vec->data == NULL) {                                                                   \
            arena_destroy(arena);                                                                  \
            return NULL;                                                                           \
        }                                                                                          \
                                                                                                   \
        vec->size = 0;                                                                             \
        vec->capacity = initial_capacity;                                                          \
        vec->arena = arena;                                                                        \
                                                                                                   \
        return vec;                                                                                \
    }                                                                                              \
                                                                                                   \
    void TypeName##_destroy(TypeName##_t* vec) {                                                   \
        assert(vec != NULL);                                                                       \
        arena_destroy(vec->arena);                                                                 \
        free(vec);                                                                                 \
    }                                                                                              \
                                                                                                   \
    size_t TypeName##_size(const TypeName##_t* vec) {                                              \
        assert(vec != NULL);                                                                       \
        return vec->size;                                                                          \
    }                                                                                              \
                                                                                                   \
    size_t TypeName##_capacity(const TypeName##_t* vec) {                                          \
        assert(vec != NULL);                                                                       \
        return vec->capacity;                                                                      \
    }                                                                                              \
                                                                                                   \
    bool TypeName##_empty(const TypeName##_t* vec) {                                               \
        assert(vec != NULL);                                                                       \
        return vec->size == 0;                                                                     \
    }                                                                                              \
                                                                                                   \
    void TypeName##_resize(TypeName##_t* vec, size_t new_capacity) {                               \
        assert(vec != NULL);                                                                       \
        assert(new_capacity > 0);                                                                  \
                                                                                                   \
        if (new_capacity == vec->capacity || new_capacity < vec->size) {                           \
            printf("Invalid new capacity\n");                                                      \
            return;                                                                                \
        }                                                                                          \
                                                                                                   \
        Type* new_data = (Type*)arena_realloc(vec->arena, vec->data, new_capacity * sizeof(Type)); \
        if (new_data == NULL) {                                                                    \
            printf("Failed to reallocate memory\n");                                               \
            return;                                                                                \
        }                                                                                          \
                                                                                                   \
        vec->data = new_data;                                                                      \
        vec->capacity = new_capacity;                                                              \
    }                                                                                              \
                                                                                                   \
    void TypeName##_reserve(TypeName##_t* vec, size_t min_capacity) {                              \
        assert(vec != NULL);                                                                       \
                                                                                                   \
        if (min_capacity > vec->capacity) {                                                        \
            TypeName##_resize(vec, min_capacity);                                                  \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void TypeName##_push_back(TypeName##_t* vec, Type elem) {                                      \
        assert(vec != NULL);                                                                       \
                                                                                                   \
        if (vec->size == vec->capacity) {                                                          \
            TypeName##_resize(vec, vec->capacity * 2);                                             \
        }                                                                                          \
                                                                                                   \
        vec->data[vec->size++] = elem;                                                             \
    }                                                                                              \
                                                                                                   \
    void TypeName##_erase(TypeName##_t* vec, size_t index) {                                       \
        assert(vec != NULL);                                                                       \
        assert(index < vec->size);                                                                 \
                                                                                                   \
        memmove(&vec->data[index], &vec->data[index + 1], (vec->size - index - 1) * sizeof(Type)); \
        vec->size--;                                                                               \
    }                                                                                              \
                                                                                                   \
    void TypeName##_pop_back(TypeName##_t* vec) {                                                  \
        assert(vec != NULL);                                                                       \
        assert(vec->size > 0);                                                                     \
                                                                                                   \
        vec->size--;                                                                               \
    }                                                                                              \
                                                                                                   \
    Type TypeName##_at(const TypeName##_t* vec, size_t index) {                                    \
        assert(vec != NULL);                                                                       \
        assert(index < vec->size);                                                                 \
        return vec->data[index];                                                                   \
    }                                                                                              \
                                                                                                   \
    Type TypeName##_front(const TypeName##_t* vec) {                                               \
        assert(vec != NULL);                                                                       \
        assert(vec->size > 0);                                                                     \
        return vec->data[0];                                                                       \
    }                                                                                              \
                                                                                                   \
    Type TypeName##_back(const TypeName##_t* vec) {                                                \
        assert(vec != NULL);                                                                       \
        assert(vec->size > 0);                                                                     \
        return vec->data[vec->size - 1];                                                           \
    }

#endif /* CC4C048C_C6DD_4C98_B8A8_E71736327690 */
