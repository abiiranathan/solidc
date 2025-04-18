#ifndef __SET_H__
#define __SET_H__

#include <stdbool.h>
#include <stdlib.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define DEFINE_SET(T)                                                                                                  \
    typedef struct Set_##T {                                                                                           \
        (T) * data;                                                                                                    \
        size_t size;                                                                                                   \
        size_t capacity;                                                                                               \
    } Set_##T;                                                                                                         \
    /* Creates a new heap-allocated set. */                                                                            \
    static inline Set_##T* set_create_##T(size_t initialCapacity) {                                                    \
        Set_##T* set = (Set_##T*)malloc(sizeof(Set_##T));                                                              \
        if (set == NULL) {                                                                                             \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        set->data = (T*)malloc(initialCapacity * sizeof(T));                                                           \
        if (set->data == NULL) {                                                                                       \
            free(set);                                                                                                 \
            return NULL;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        set->size     = 0;                                                                                             \
        set->capacity = initialCapacity;                                                                               \
        return set;                                                                                                    \
    }                                                                                                                  \
    /* Free heap memory used by set and its elements.*/                                                                \
    static inline void set_destroy_##T(Set_##T* set) {                                                                 \
        if (set == NULL) {                                                                                             \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        free(set->data);                                                                                               \
        set->data = NULL;                                                                                              \
        free(set);                                                                                                     \
        set = NULL;                                                                                                    \
    }                                                                                                                  \
    /* Returns true if set contains element. */                                                                        \
    static inline bool set_contains_##T(Set_##T* set, T value) {                                                       \
        if (set == NULL) {                                                                                             \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        for (size_t i = 0; i < set->size; i++) {                                                                       \
            if (set->data[i] == value) {                                                                               \
                return true;                                                                                           \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        return false;                                                                                                  \
    }                                                                                                                  \
    /* Add element to the set. returns true if added successfully or already                                           \
     * exists. */                                                                                                      \
    static inline bool set_add_##T(Set_##T* set, T value) {                                                            \
        if (set == NULL) {                                                                                             \
            return false;                                                                                              \
        }                                                                                                              \
        if (set_contains_##T(set, value)) {                                                                            \
            return true;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        if (set->size == set->capacity) {                                                                              \
            size_t newCapacity = set->capacity * 2;                                                                    \
            (T)* newData       = (T*)realloc(set->data, newCapacity * sizeof(T));                                      \
            if (newData == NULL) {                                                                                     \
                return false;                                                                                          \
            }                                                                                                          \
            set->data     = newData;                                                                                   \
            set->capacity = newCapacity;                                                                               \
        }                                                                                                              \
                                                                                                                       \
        set->data[set->size++] = value;                                                                                \
        return true;                                                                                                   \
    }                                                                                                                  \
    /* Remove element from the set. Returns true if successful. */                                                     \
    static inline bool set_remove_##T(Set_##T* set, T value) {                                                         \
        if (set == NULL) {                                                                                             \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        for (size_t i = 0; i < set->size; i++) {                                                                       \
            if (set->data[i] == value) {                                                                               \
                set->data[i] = set->data[--set->size];                                                                 \
                return true;                                                                                           \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        return false;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    /* Returns number of elements in the set. */                                                                       \
    static inline size_t set_size_##T(Set_##T* set) {                                                                  \
        return set->size;                                                                                              \
    }                                                                                                                  \
    /* Returns number of elements in the set. */                                                                       \
    static inline size_t set_capacity_##T(Set_##T* set) {                                                              \
        return set->capacity;                                                                                          \
    }                                                                                                                  \
    /* returns true if size is 0. */                                                                                   \
    static inline bool set_isempty_##T(Set_##T* set) {                                                                 \
        return set->size == 0;                                                                                         \
    }                                                                                                                  \
    /* Clear the set. */                                                                                               \
    static inline void set_clear_##T(Set_##T* set) {                                                                   \
        for (size_t i = 0; i < set->size; i++) {                                                                       \
            set->data[i] = (T)NULL;                                                                                    \
        }                                                                                                              \
        set->size = 0;                                                                                                 \
    }                                                                                                                  \
    /* Computes the union of two sets A and B, and returns a new set containing                                        \
     * all elements in A and B. */                                                                                     \
    static inline Set_##T* set_union_##T(Set_##T* setA, Set_##T* setB) {                                               \
        Set_##T* unionSet = set_create_##T(setA->size + setB->size);                                                   \
        for (size_t i = 0; i < setA->size; i++) {                                                                      \
            set_add_##T(unionSet, setA->data[i]);                                                                      \
        }                                                                                                              \
        for (size_t i = 0; i < setB->size; i++) {                                                                      \
            set_add_##T(unionSet, setB->data[i]);                                                                      \
        }                                                                                                              \
        return unionSet;                                                                                               \
    }                                                                                                                  \
    /* Computes the intersection of two sets A and B, and returns a new set                                            \
     * containing elements that are in both A and B. */                                                                \
    static inline Set_##T* set_intersection_##T(Set_##T* setA, Set_##T* setB) {                                        \
        Set_##T* intersectionSet = set_create_##T(setA->size < setB->size ? setA->size : setB->size);                  \
        for (size_t i = 0; i < setA->size; i++) {                                                                      \
            if (set_contains_##T(setB, setA->data[i])) {                                                               \
                set_add_##T(intersectionSet, setA->data[i]);                                                           \
            }                                                                                                          \
        }                                                                                                              \
        return intersectionSet;                                                                                        \
    }                                                                                                                  \
    /* Computes the difference of two sets A and B, and returns a new set                                              \
     * containing elements that are in A but not in B. */                                                              \
    static inline Set_##T* set_difference_##T(Set_##T* setA, Set_##T* setB) {                                          \
        Set_##T* differenceSet = set_create_##T(setA->size);                                                           \
        for (size_t i = 0; i < setA->size; i++) {                                                                      \
            if (!set_contains_##T(setB, setA->data[i])) {                                                              \
                set_add_##T(differenceSet, setA->data[i]);                                                             \
            }                                                                                                          \
        }                                                                                                              \
        return differenceSet;                                                                                          \
    }                                                                                                                  \
    /* Computes the symmetric difference of two sets A and B, and returns a new                                        \
     * set containing elements that are in A or B but not in both. */                                                  \
    static inline Set_##T* set_symmetric_difference_##T(Set_##T* setA, Set_##T* setB) {                                \
        Set_##T* symmetricDifferenceSet = set_create_##T(setA->size + setB->size);                                     \
        for (size_t i = 0; i < setA->size; i++) {                                                                      \
            if (!set_contains_##T(setB, setA->data[i])) {                                                              \
                set_add_##T(symmetricDifferenceSet, setA->data[i]);                                                    \
            }                                                                                                          \
        }                                                                                                              \
        for (size_t i = 0; i < setB->size; i++) {                                                                      \
            if (!set_contains_##T(setA, setB->data[i])) {                                                              \
                set_add_##T(symmetricDifferenceSet, setB->data[i]);                                                    \
            }                                                                                                          \
        }                                                                                                              \
        return symmetricDifferenceSet;                                                                                 \
    }                                                                                                                  \
    /* Determines if setA is a subset of setB, and returns true if all elements                                        \
     * in setA are also in setB.*/                                                                                     \
    static inline bool set_isSubset_##T(Set_##T* setA, Set_##T* setB) {                                                \
        for (size_t i = 0; i < setA->size; i++) {                                                                      \
            if (!set_contains_##T(setB, setA->data[i])) {                                                              \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        return true;                                                                                                   \
    }                                                                                                                  \
    /* Determines if setA is a proper subset of setB, and returns true if all                                          \
     * elements in setA are also in setB but setA and setB are not equal. */                                           \
    static inline bool set_isProperSubset_##T(Set_##T* setA, Set_##T* setB) {                                          \
        bool issubset = set_isSubset_##T(setA, setB);                                                                  \
        bool isequal  = setA->size == setB->size;                                                                      \
        if (isequal) {                                                                                                 \
            for (size_t i = 0; i < setA->size; i++) {                                                                  \
                if (setA->data[i] != setB->data[i]) {                                                                  \
                    isequal = false;                                                                                   \
                    break;                                                                                             \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        return issubset && !isequal;                                                                                   \
    }

#if defined(__cplusplus)
}
#endif

#endif /* __SET_H__ */
