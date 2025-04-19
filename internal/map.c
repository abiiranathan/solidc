#include <limits.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XXH_INLINE_ALL
#include <xxhash.h>  // With Mingw: paru -S mingw-w64-xxhash
#include "../include/cmp.h"
#include "map.h"

// Map structure
typedef struct hash_map {
    entry* entries;                                 // Map entries
    uint8_t* deleted_bitmap;                        // Bitmap for deleted entries
    size_t size;                                    // Number of map entries
    size_t capacity;                                // Map capacity
    unsigned long (*hash)(const void*);             // Hash function
    bool (*key_compare)(const void*, const void*);  // Key comparison function
    pthread_mutex_t lock;                           // Lock for thread safety
    bool free_entries;                              // Free keys and values (if heap-allocated)
} Map;

// Hash functions
static inline unsigned long xxhash(const void* key);

// Map creation
Map* map_create(size_t initial_capacity, bool (*key_compare)(const void*, const void*), bool free_entries) {
    if (initial_capacity == 0) {
        initial_capacity = INITIAL_MAP_SIZE;
    }

    if (!key_compare) {
        fprintf(stderr, "Key comparison function is required\n");
        return NULL;
    }

    Map* m = (Map*)malloc(sizeof(Map));
    if (!m) {
        perror("Failed to allocate memory for map");
        return NULL;
    }

    m->entries = (entry*)calloc(initial_capacity, sizeof(entry));
    if (!m->entries) {
        free(m);
        perror("Failed to allocate memory for map entries");
        return NULL;
    }

    m->deleted_bitmap = (uint8_t*)calloc((initial_capacity + 7) / 8, sizeof(uint8_t));
    if (!m->deleted_bitmap) {
        free(m->entries);
        free(m);
        perror("Failed to allocate memory for deleted bitmap");
        return NULL;
    }

    m->size         = 0;
    m->capacity     = initial_capacity;
    m->hash         = xxhash;  // Use xxHash for faster hashing
    m->key_compare  = key_compare;
    m->free_entries = free_entries;

    pthread_mutex_init(&m->lock, NULL);
    return m;
}

entry* map_get_entries(Map* m) {
    return m->entries;
}

// Map destruction
void map_destroy(Map* m) {
    if (!m)
        return;

    if (m->free_entries) {
        for (size_t i = 0; i < m->capacity; i++) {
            if (m->entries[i].key)
                free((void*)m->entries[i].key);
            if (m->entries[i].value)
                free(m->entries[i].value);
        }
    }

    free(m->entries);
    free(m->deleted_bitmap);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

// Set the hash function for the map
void map_set_hash(Map* m, unsigned long (*hash)(const void*)) {
    m->hash = hash;
}

// Resize the map
bool map_resize(Map* m, size_t new_capacity) {
    entry* new_entries = (entry*)calloc(new_capacity, sizeof(entry));
    if (!new_entries) {
        perror("Failed to allocate memory for new map entries");
        return false;
    }

    uint8_t* new_deleted_bitmap = (uint8_t*)calloc((new_capacity + 7) / 8, sizeof(uint8_t));
    if (!new_deleted_bitmap) {
        free(new_entries);
        perror("Failed to allocate memory for new deleted bitmap");
        return false;
    }

    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].key && !(m->deleted_bitmap[i / 8] & (1 << (i % 8)))) {
            size_t index = m->hash(m->entries[i].key) % new_capacity;
            size_t j     = 0;
            while (new_entries[index].key) {
                j++;
                index = (index + j * j) % new_capacity;  // Quadratic probing
            }
            new_entries[index] = m->entries[i];
        }
    }

    free(m->entries);
    free(m->deleted_bitmap);

    m->entries        = new_entries;
    m->deleted_bitmap = new_deleted_bitmap;
    m->capacity       = new_capacity;

    return true;
}

// Set a key-value pair
void map_set(Map* m, void* key, void* value) {
    if ((double)m->size / (double)m->capacity > LOAD_FACTOR_THRESHOLD) {
        if (m->capacity > SIZE_MAX / 2 || !map_resize(m, m->capacity * 2)) {
            fprintf(stderr, "Integer overflow or out of memory\n");
            return;
        }
    }

    size_t index = m->hash(key) % m->capacity;
    size_t i     = 0;
    while (m->entries[index].key && !m->key_compare(m->entries[index].key, key)) {
        i++;
        index = (index + i * i) % m->capacity;  // Quadratic probing
    }

    if (!m->entries[index].key)
        m->size++;
    m->entries[index].key   = key;
    m->entries[index].value = value;
    m->deleted_bitmap[index / 8] &= ~(1 << (index % 8));  // Clear deleted flag
}

// Get a value by key
void* map_get(Map* m, void* key) {
    size_t index = m->hash(key) % m->capacity;
    size_t i     = 0;
    while (m->entries[index].key || (m->deleted_bitmap[index / 8] & (1 << (index % 8)))) {
        if (m->entries[index].key && m->key_compare(m->entries[index].key, key)) {
            return m->entries[index].value;
        }
        i++;
        index = (index + i * i) % m->capacity;  // Quadratic probing
    }
    return NULL;
}

size_t map_length(Map* m) {
    return m->size;
}

size_t map_capacity(Map* m) {
    return m->capacity;
}

// Remove a key-value pair
void map_remove(Map* m, void* key) {
    size_t index = m->hash(key) % m->capacity;
    size_t i     = 0;

    while (m->entries[index].key || (m->deleted_bitmap[index / 8] & (1 << (index % 8)))) {
        if (m->entries[index].key && m->key_compare(m->entries[index].key, key)) {
            if (m->free_entries) {
                free((void*)m->entries[index].key);
                free((void*)m->entries[index].value);
            }

            m->entries[index].key   = NULL;
            m->entries[index].value = NULL;
            m->deleted_bitmap[index / 8] |= (1 << (index % 8));  // Mark as deleted
            m->size--;
            return;
        }
        i++;
        index = (index + i * i) % m->capacity;  // Quadratic probing
    }
}

// Thread-safe operations
void map_set_safe(Map* m, void* key, void* value) {
    pthread_mutex_lock(&m->lock);
    map_set(m, key, value);
    pthread_mutex_unlock(&m->lock);
}

void* map_get_safe(Map* m, void* key) {
    pthread_mutex_lock(&m->lock);
    void* value = map_get(m, key);
    pthread_mutex_unlock(&m->lock);
    return value;
}

void map_remove_safe(Map* m, void* key) {
    pthread_mutex_lock(&m->lock);
    map_remove(m, key);
    pthread_mutex_unlock(&m->lock);
}

// Key comparison functions
bool key_compare_int(const void* a, const void* b) {
    return a && b && *(int*)a == *(int*)b;
}

bool key_compare_char_ptr(const void* a, const void* b) {
    return a && b && strcmp((char*)a, (char*)b) == 0;
}

bool key_compare_float(const void* a, const void* b) {
    return a && b && FLOAT_EQUAL(*(float*)a, *(float*)b);
}

bool key_compare_double(const void* a, const void* b) {
    return a && b && FLOAT_EQUAL(*(double*)a, *(double*)b);
}

// xxHash implementation using XXH64.
unsigned long xxhash(const void* key) {
    return XXH64(key, sizeof(int), 0);
}
