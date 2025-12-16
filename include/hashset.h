/**
 * @file hashset.h
 * @brief Hash set implementation for unique element storage.
 */

#ifndef __HASHSET_H__
#define __HASHSET_H__

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint64_t
#include <stdlib.h>   // for malloc, free
#include <string.h>   // for memcpy, memset

#if defined(__cplusplus)
extern "C" {
#endif

/** Default initial capacity for hash sets. */
#define HASHSET_DEFAULT_CAPACITY 16

/** Load factor threshold for triggering rehash (75%). */
#define HASHSET_LOAD_FACTOR 0.75

/** Node in a hash set bucket chain. */
typedef struct hashset_node {
    void* key;                 /** Stored key (actual data or pointer). */
    struct hashset_node* next; /** Next node in chain. */
} hashset_node_t;

/** Hash set structure using separate chaining. */
typedef struct {
    hashset_node_t** buckets; /** Array of bucket chains. */
    size_t capacity;          /** Number of buckets. */
    size_t size;              /** Number of elements. */
    size_t key_size;          /** Size of each key in bytes. */

    /** Hash function: computes hash from key data. */
    uint64_t (*hash_fn)(const void* key, size_t key_size);

    /** Equality function: returns true if keys are equal. */
    bool (*equals_fn)(const void* a, const void* b, size_t key_size);
} hashset_t;

/**
 * Default hash function using FNV-1a algorithm.
 * @param key Pointer to key data.
 * @param key_size Size of key in bytes.
 * @return 64-bit hash value.
 */
static inline uint64_t hashset_default_hash(const void* key, size_t key_size) {
    const uint64_t FNV_OFFSET = 14695981039346656037ULL;
    const uint64_t FNV_PRIME  = 1099511628211ULL;

    uint64_t hash              = FNV_OFFSET;
    const unsigned char* bytes = (const unsigned char*)key;

    for (size_t i = 0; i < key_size; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

/**
 * Default equality function using memcmp.
 * @param a First key.
 * @param b Second key.
 * @param key_size Size of keys in bytes.
 * @return True if keys are equal.
 */
static inline bool hashset_default_equals(const void* a, const void* b, size_t key_size) {
    return memcmp(a, b, key_size) == 0;
}

/**
 * Creates a new hash set.
 * @param key_size Size of each key in bytes.
 * @param initial_capacity Initial number of buckets (0 uses default).
 * @param hash_fn Custom hash function (NULL uses default FNV-1a).
 * @param equals_fn Custom equality function (NULL uses memcmp).
 * @return Pointer to new hash set on success, NULL on allocation failure.
 * @note Caller must free using hashset_destroy().
 */
static inline hashset_t* hashset_create(size_t key_size, size_t initial_capacity,
                                        uint64_t (*hash_fn)(const void*, size_t),
                                        bool (*equals_fn)(const void*, const void*, size_t)) {
    if (key_size == 0) {
        return NULL;
    }

    hashset_t* set = (hashset_t*)malloc(sizeof(hashset_t));
    if (set == NULL) {
        return NULL;
    }

    size_t capacity = initial_capacity > 0 ? initial_capacity : HASHSET_DEFAULT_CAPACITY;
    set->buckets    = (hashset_node_t**)calloc(capacity, sizeof(hashset_node_t*));
    if (set->buckets == NULL) {
        free(set);
        return NULL;
    }

    set->capacity  = capacity;
    set->size      = 0;
    set->key_size  = key_size;
    set->hash_fn   = hash_fn != NULL ? hash_fn : hashset_default_hash;
    set->equals_fn = equals_fn != NULL ? equals_fn : hashset_default_equals;

    return set;
}

/**
 * Frees all memory associated with a hash set.
 * @param set The hash set to destroy.
 */
static inline void hashset_destroy(hashset_t* set) {
    if (set == NULL) {
        return;
    }

    // Free all nodes in all buckets
    for (size_t i = 0; i < set->capacity; i++) {
        hashset_node_t* node = set->buckets[i];
        while (node != NULL) {
            hashset_node_t* next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
    }

    free(set->buckets);
    free(set);
}

/**
 * Checks if the set contains a key.
 * @param set The hash set.
 * @param key Pointer to the key data.
 * @return True if key exists in set.
 * @note Time complexity: O(1) average, O(n) worst case.
 */
static inline bool hashset_contains(const hashset_t* set, const void* key) {
    if (set == NULL || key == NULL) {
        return false;
    }

    uint64_t hash = set->hash_fn(key, set->key_size);
    size_t index  = hash % set->capacity;

    hashset_node_t* node = set->buckets[index];
    while (node != NULL) {
        if (set->equals_fn(node->key, key, set->key_size)) {
            return true;
        }
        node = node->next;
    }

    return false;
}

/**
 * Internal function to rehash the set to a new capacity.
 * @param set The hash set.
 * @param new_capacity New bucket count.
 * @return True on success, false on allocation failure.
 */
static inline bool hashset_rehash(hashset_t* set, size_t new_capacity) {
    hashset_node_t** new_buckets = (hashset_node_t**)calloc(new_capacity, sizeof(hashset_node_t*));
    if (new_buckets == NULL) {
        return false;
    }

    // Rehash all existing nodes
    for (size_t i = 0; i < set->capacity; i++) {
        hashset_node_t* node = set->buckets[i];
        while (node != NULL) {
            hashset_node_t* next = node->next;

            // Compute new bucket index
            uint64_t hash    = set->hash_fn(node->key, set->key_size);
            size_t new_index = hash % new_capacity;

            // Insert at head of new bucket
            node->next             = new_buckets[new_index];
            new_buckets[new_index] = node;

            node = next;
        }
    }

    free(set->buckets);
    set->buckets  = new_buckets;
    set->capacity = new_capacity;

    return true;
}

/**
 * Adds a key to the set.
 * @param set The hash set.
 * @param key Pointer to the key data to add (will be copied).
 * @return True if added successfully or already exists, false on allocation failure.
 * @note Time complexity: O(1) average, O(n) worst case.
 */
static inline bool hashset_add(hashset_t* set, const void* key) {
    if (set == NULL || key == NULL) {
        return false;
    }

    // Check if already exists
    if (hashset_contains(set, key)) {
        return true;
    }

    // Check load factor and rehash if needed
    if ((double)(set->size + 1) / (double)set->capacity > HASHSET_LOAD_FACTOR) {
        if (!hashset_rehash(set, set->capacity * 2)) {
            return false;
        }
    }

    // Create new node
    hashset_node_t* node = (hashset_node_t*)malloc(sizeof(hashset_node_t));
    if (node == NULL) {
        return false;
    }

    node->key = malloc(set->key_size);
    if (node->key == NULL) {
        free(node);
        return false;
    }

    memcpy(node->key, key, set->key_size);

    // Insert at head of bucket
    uint64_t hash = set->hash_fn(key, set->key_size);
    size_t index  = hash % set->capacity;

    node->next          = set->buckets[index];
    set->buckets[index] = node;
    set->size++;

    return true;
}

/**
 * Removes a key from the set.
 * @param set The hash set.
 * @param key Pointer to the key data to remove.
 * @return True if removed successfully, false if not found.
 * @note Time complexity: O(1) average, O(n) worst case.
 */
static inline bool hashset_remove(hashset_t* set, const void* key) {
    if (set == NULL || key == NULL) {
        return false;
    }

    uint64_t hash = set->hash_fn(key, set->key_size);
    size_t index  = hash % set->capacity;

    hashset_node_t* node = set->buckets[index];
    hashset_node_t* prev = NULL;

    while (node != NULL) {
        if (set->equals_fn(node->key, key, set->key_size)) {
            // Remove node from chain
            if (prev == NULL) {
                set->buckets[index] = node->next;
            } else {
                prev->next = node->next;
            }

            free(node->key);
            free(node);
            set->size--;
            return true;
        }

        prev = node;
        node = node->next;
    }

    return false;
}

/**
 * Returns the number of elements in the set.
 * @param set The hash set.
 * @return Number of elements.
 */
static inline size_t hashset_size(const hashset_t* set) {
    return set != NULL ? set->size : 0;
}

/**
 * Returns the current bucket capacity.
 * @param set The hash set.
 * @return Number of buckets.
 */
static inline size_t hashset_capacity(const hashset_t* set) {
    return set != NULL ? set->capacity : 0;
}

/**
 * Checks if the set is empty.
 * @param set The hash set.
 * @return True if set has no elements.
 */
static inline bool hashset_isempty(const hashset_t* set) {
    return set == NULL || set->size == 0;
}

/**
 * Removes all elements from the set.
 * @param set The hash set.
 */
static inline void hashset_clear(hashset_t* set) {
    if (set == NULL) {
        return;
    }

    for (size_t i = 0; i < set->capacity; i++) {
        hashset_node_t* node = set->buckets[i];
        while (node != NULL) {
            hashset_node_t* next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
        set->buckets[i] = NULL;
    }

    set->size = 0;
}

/**
 * Computes the union of two sets A and B.
 * @param setA First set.
 * @param setB Second set.
 * @return New set containing all elements in A or B, NULL on failure.
 * @note Caller must free using hashset_destroy().
 */
static inline hashset_t* hashset_union(const hashset_t* setA, const hashset_t* setB) {
    if (setA == NULL || setB == NULL || setA->key_size != setB->key_size) {
        return NULL;
    }

    hashset_t* result = hashset_create(setA->key_size, setA->size + setB->size, setA->hash_fn, setA->equals_fn);
    if (result == NULL) {
        return NULL;
    }

    // Add all elements from setA
    for (size_t i = 0; i < setA->capacity; i++) {
        hashset_node_t* node = setA->buckets[i];
        while (node != NULL) {
            if (!hashset_add(result, node->key)) {
                hashset_destroy(result);
                return NULL;
            }
            node = node->next;
        }
    }

    // Add all elements from setB
    for (size_t i = 0; i < setB->capacity; i++) {
        hashset_node_t* node = setB->buckets[i];
        while (node != NULL) {
            if (!hashset_add(result, node->key)) {
                hashset_destroy(result);
                return NULL;
            }
            node = node->next;
        }
    }

    return result;
}

/**
 * Computes the intersection of two sets A and B.
 * @param setA First set.
 * @param setB Second set.
 * @return New set containing elements in both A and B, NULL on failure.
 * @note Caller must free using hashset_destroy().
 */
static inline hashset_t* hashset_intersection(const hashset_t* setA, const hashset_t* setB) {
    if (setA == NULL || setB == NULL || setA->key_size != setB->key_size) {
        return NULL;
    }

    size_t min_size   = setA->size < setB->size ? setA->size : setB->size;
    hashset_t* result = hashset_create(setA->key_size, min_size, setA->hash_fn, setA->equals_fn);
    if (result == NULL) {
        return NULL;
    }

    // Iterate through smaller set for efficiency
    const hashset_t* smaller = setA->size <= setB->size ? setA : setB;
    const hashset_t* larger  = setA->size <= setB->size ? setB : setA;

    for (size_t i = 0; i < smaller->capacity; i++) {
        hashset_node_t* node = smaller->buckets[i];
        while (node != NULL) {
            if (hashset_contains(larger, node->key)) {
                if (!hashset_add(result, node->key)) {
                    hashset_destroy(result);
                    return NULL;
                }
            }
            node = node->next;
        }
    }

    return result;
}

/**
 * Computes the difference of two sets A and B (A - B).
 * @param setA First set.
 * @param setB Second set.
 * @return New set containing elements in A but not in B, NULL on failure.
 * @note Caller must free using hashset_destroy().
 */
static inline hashset_t* hashset_difference(const hashset_t* setA, const hashset_t* setB) {
    if (setA == NULL || setB == NULL || setA->key_size != setB->key_size) {
        return NULL;
    }

    hashset_t* result = hashset_create(setA->key_size, setA->size, setA->hash_fn, setA->equals_fn);
    if (result == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < setA->capacity; i++) {
        hashset_node_t* node = setA->buckets[i];
        while (node != NULL) {
            if (!hashset_contains(setB, node->key)) {
                if (!hashset_add(result, node->key)) {
                    hashset_destroy(result);
                    return NULL;
                }
            }
            node = node->next;
        }
    }

    return result;
}

/**
 * Computes the symmetric difference of two sets A and B.
 * @param setA First set.
 * @param setB Second set.
 * @return New set containing elements in A or B but not both, NULL on failure.
 * @note Caller must free using hashset_destroy().
 */
static inline hashset_t* hashset_symmetric_difference(const hashset_t* setA, const hashset_t* setB) {
    if (setA == NULL || setB == NULL || setA->key_size != setB->key_size) {
        return NULL;
    }

    hashset_t* result = hashset_create(setA->key_size, setA->size + setB->size, setA->hash_fn, setA->equals_fn);
    if (result == NULL) {
        return NULL;
    }

    // Add elements from A not in B
    for (size_t i = 0; i < setA->capacity; i++) {
        hashset_node_t* node = setA->buckets[i];
        while (node != NULL) {
            if (!hashset_contains(setB, node->key)) {
                if (!hashset_add(result, node->key)) {
                    hashset_destroy(result);
                    return NULL;
                }
            }
            node = node->next;
        }
    }

    // Add elements from B not in A
    for (size_t i = 0; i < setB->capacity; i++) {
        hashset_node_t* node = setB->buckets[i];
        while (node != NULL) {
            if (!hashset_contains(setA, node->key)) {
                if (!hashset_add(result, node->key)) {
                    hashset_destroy(result);
                    return NULL;
                }
            }
            node = node->next;
        }
    }

    return result;
}

/**
 * Checks if setA is a subset of setB.
 * @param setA First set.
 * @param setB Second set.
 * @return True if all elements in setA are in setB.
 */
static inline bool hashset_is_subset(const hashset_t* setA, const hashset_t* setB) {
    if (setA == NULL || setB == NULL || setA->key_size != setB->key_size) {
        return false;
    }

    if (setA->size > setB->size) {
        return false;
    }

    for (size_t i = 0; i < setA->capacity; i++) {
        hashset_node_t* node = setA->buckets[i];
        while (node != NULL) {
            if (!hashset_contains(setB, node->key)) {
                return false;
            }
            node = node->next;
        }
    }

    return true;
}

/**
 * Checks if setA is a proper subset of setB.
 * @param setA First set.
 * @param setB Second set.
 * @return True if setA is a subset of setB and setA != setB.
 */
static inline bool hashset_is_proper_subset(const hashset_t* setA, const hashset_t* setB) {
    if (setA == NULL || setB == NULL) {
        return false;
    }

    return setA->size < setB->size && hashset_is_subset(setA, setB);
}

#if defined(__cplusplus)
}
#endif

#endif /* __HASHSET_H__ */
