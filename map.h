#ifndef A02E572A_DD85_4D77_AC81_41037EDE290A
#define A02E572A_DD85_4D77_AC81_41037EDE290A

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Allow user to customize initial map size and load factor threshold
#ifndef INITIAL_MAP_SIZE
#define INITIAL_MAP_SIZE 64
#endif

#ifndef LOAD_FACTOR_THRESHOLD
#define LOAD_FACTOR_THRESHOLD 0.75
#endif

typedef struct {
    const void* key;
    const void* value;
} entry;

// Function pointer type for the key comparison.
// The function should return true if the keys are equal.
// This is necessary because the map does not know the size of the keys.
// memcmp can be used for byte-wise comparison of keys if the keys are of known size.
typedef bool (*key_compare_fn)(const void*, const void*);

typedef struct map {
    entry* entries;
    size_t size;
    size_t capacity;
    unsigned long (*hash)(const void*);
    key_compare_fn key_compare;
} map;

typedef struct {
    map* m;
    size_t bucket;
    size_t index;
} map_iterator;

#define map_foreach(map, entry_var)                                                                \
    for (size_t i = 0; i < map->size; i++)                                                         \
        for (int cont = 1; cont; cont = 0)                                                         \
            for (entry* entry_var = &map->entries[i]; cont; cont = 0)

// Create a new map iterator
map_iterator* map_iterator_create(map* m);

// Destroy the map iterator
void map_iterator_destroy(map_iterator* iter);

// Check if the map iterator has more elements
int map_iterator_has_next(map_iterator* iter);

// Return the next entry in the map using the iterator
entry map_iterator_next(map_iterator* iter);

// Create a new map and initialize it
map* map_create(size_t initial_capacity, key_compare_fn key_compare);

// Destroy the map and free the memory
void map_destroy(map* m);

// Resize the map to a new capacity and rehash the entries
// This is an expensive operation. Use with caution.
// Consider setting the initial capacity to a large value
// by defining INITIAL_MAP_SIZE before including this file.
void map_resize(map* m, size_t new_capacity);

// Set a key-value pair in the map
// This is idempotent.
void map_set(map* m, const void* key, const void* value);

// Set multiple key-value pairs in the map from arrays
// The keys and values arrays must be of the same length
void map_set_from_array(map* m, const void** keys, const void** values, size_t num_keys);

// Get the value for a key in the map
const void* map_get(map* m, const void* key);

// Get all the keys in the map.
// The number of keys is returned in num_keys
// The user is responsible for freeing the keys array with map_free_keys
const void** map_getkeys(map* m, size_t* num_keys);

// Free the keys array returned by map_getkeys
void map_free_keys(void** keys);

// Remove a key from the map
void map_remove(map* m, const void* key);

// Set the hash function for the map
// The default hash function is djb2_hash
void map_set_hash(map* m, unsigned long (*hash)(const void*));

// Get the number of key-value pairs in the map
size_t map_length(map* m);

// Get the capacity of the map
size_t map_capacity(map* m);

// djb2 hash function.
// http://www.cse.yorku.ca/~oz/hash.html
unsigned long djb2_hash(const void* key);

// Simple DataBase Manager hash.
// http://www.cse.yorku.ca/~oz/hash.html
unsigned long sdbm_hash(const void* key);

#if defined(__cplusplus)
}
#endif

// define MAP_IMPL before including this file to
// generate the implementation
#ifdef MAP_IMPL

// http://www.cse.yorku.ca/~oz/hash.html
unsigned long djb2_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != '\0') {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }

    return hash;
}

// Simple DataBase Manager hash
// http://www.cse.yorku.ca/~oz/hash.html
unsigned long sdbm_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 0;
    int c;
    while ((c = *str++) != '\0') {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

// Implementation of map functions
map* map_create(size_t initial_capacity, key_compare_fn key_compare) {
    if (initial_capacity == 0) {
        initial_capacity = INITIAL_MAP_SIZE;
    }

    if (!key_compare) {
        perror("Key comparison function is required");
        exit(EXIT_FAILURE);
    }

    map* m = (map*)malloc(sizeof(map));
    if (m == NULL) {
        perror("Failed to allocate memory for map");
        exit(EXIT_FAILURE);
    }

    m->entries = (entry*)malloc(sizeof(entry) * initial_capacity);
    if (m->entries == NULL) {
        free(m);
        perror("Failed to allocate memory for map entries");
        exit(EXIT_FAILURE);
    }

    // Initialize the entries
    for (size_t i = 0; i < initial_capacity; i++) {
        m->entries[i].key   = NULL;
        m->entries[i].value = NULL;
    }

    m->size        = 0;
    m->capacity    = initial_capacity;
    m->hash        = djb2_hash;
    m->key_compare = key_compare;
    return m;
}

void map_destroy(map* m) {
    free(m->entries);
    free(m);
}

void map_resize(map* m, size_t new_capacity) {
    // Allocate memory for the new entries
    entry* new_entries = (entry*)malloc(sizeof(entry) * new_capacity);
    if (new_entries == NULL) {
        perror("Failed to allocate memory for new map entries");
        exit(EXIT_FAILURE);
    }

    // Initialize the new entries
    for (size_t i = 0; i < new_capacity; i++) {
        new_entries[i].key   = NULL;
        new_entries[i].value = NULL;
    }

    // Rehash the old entries into the new entries
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].key != NULL) {
            size_t index = m->hash(m->entries[i].key) % new_capacity;
            while (new_entries[index].key != NULL) {
                index = (index + 1) % new_capacity;
            }
            new_entries[index].key   = m->entries[i].key;
            new_entries[index].value = m->entries[i].value;
        }
    }

    // Free the old entries and update the map
    free(m->entries);
    m->entries  = new_entries;
    m->capacity = new_capacity;
}

void map_set(map* m, const void* key, const void* value) {
    if ((double)m->size / m->capacity > LOAD_FACTOR_THRESHOLD) {
        map_resize(m, m->capacity * 2);
    }

    // Find the index to insert the new entry
    size_t index = m->hash(key) % m->capacity;

    // Linear probing to find the next available index
    while (m->entries[index].key != NULL) {
        if (m->key_compare(m->entries[index].key, key)) {
            m->entries[index].value = value;
            return;  // Key already exists with the same value
        }
        // Key collision, continue probing
        index = (index + 1) % m->capacity;
    }

    m->entries[index].key   = key;
    m->entries[index].value = value;
    m->size++;
}

void map_set_from_array(map* m, const void** keys, const void** values, size_t num_keys) {
    for (size_t i = 0; i < num_keys; i++) {
        map_set(m, keys[i], values[i]);
    }
}

const void* map_get(map* m, const void* key) {
    size_t index = m->hash(key) % m->capacity;

    while (m->entries[index].key != NULL) {
        if (m->key_compare(m->entries[index].key, key)) {
            return m->entries[index].value;
        }
        index = (index + 1) % m->capacity;
    }
    return NULL;
}

void map_remove(map* m, const void* key) {
    size_t index = m->hash(key) % m->capacity;
    while (m->entries[index].key != NULL) {
        if (m->key_compare(m->entries[index].key, key)) {
            m->entries[index].key   = NULL;
            m->entries[index].value = NULL;
            m->size--;
            return;
        }
        index = (index + 1) % m->capacity;
    }
}

size_t map_length(map* m) {
    return m->size;
}
size_t map_capacity(map* m) {
    return m->capacity;
}

void map_set_hash(map* m, unsigned long (*hash)(const void*)) {
    m->hash = hash;
}

map_iterator* map_iterator_create(map* m) {
    map_iterator* iter = (map_iterator*)malloc(sizeof(map_iterator));
    if (iter == NULL) {
        perror("Failed to allocate memory for map iterator");
        exit(EXIT_FAILURE);
    }

    iter->m      = m;
    iter->bucket = 0;
    iter->index  = 0;
    return iter;
}

void map_iterator_destroy(map_iterator* iter) {
    if (iter != NULL) {
        free(iter);
        iter = NULL;
    }
}

int map_iterator_has_next(map_iterator* iter) {
    while (iter->bucket < iter->m->capacity) {
        if (iter->m->entries[iter->bucket].key != NULL) {
            return 1;
        }
        iter->bucket++;
    }
    return 0;
}

// Return the next entry in the map using the iterator
entry map_iterator_next(map_iterator* iter) {
    entry e = {NULL, NULL};

    // Iterate over the map entries and return the next entry
    // that has a non-NULL key.
    while (iter->bucket < iter->m->capacity) {
        if (iter->m->entries[iter->bucket].key != NULL) {
            e.key   = iter->m->entries[iter->bucket].key;
            e.value = iter->m->entries[iter->bucket].value;
            iter->bucket++;
            return e;
        }
        iter->bucket++;
    }

    return e;
}

const void** map_getkeys(map* m, size_t* num_keys) {
    size_t count       = 0;
    map_iterator* iter = map_iterator_create(m);
    const void** keys  = (const void**)malloc(sizeof(void*) * m->size);
    if (keys == NULL) {
        perror("Failed to allocate memory for keys");
        exit(EXIT_FAILURE);
    }

    while (map_iterator_has_next(iter)) {
        entry e       = map_iterator_next(iter);
        keys[count++] = e.key;
    }
    map_iterator_destroy(iter);

    if (num_keys != NULL) {
        *num_keys = count;
    }
    return keys;
}

void map_free_keys(void** keys) {
    if (keys != NULL) {
        free(keys);
        keys = NULL;
    }
}

#endif /** MAP_IMPL */

#endif /* A02E572A_DD85_4D77_AC81_41037EDE290A */
