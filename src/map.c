#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XXH_INLINE_ALL
#include <xxhash.h>
#include "../include/cmp.h"
#include "../include/lock.h"
#include "../include/map.h"

// Default maximum load factor
#define DEFAULT_MAX_LOAD_FACTOR 0.75
#define TOMBSTONE_RATIO_THRESHOLD 0.5  // Rehash when tombstones > 50% of size

// Map structure with optimized layout
typedef struct hash_map {
    void** keys;                   // Separate array for keys
    void** values;                 // Separate array for values
    bool* deleted;                 // Separate array for deleted markers
    size_t size;                   // Number of active entries
    size_t capacity;               // Map capacity
    size_t tombstone_count;        // Count of deleted entries
    float max_load_factor;         // Configurable load factor threshold
    HashFunction hash;             // Hash function
    KeyLenFunction key_len_func;   // Key length function
    KeyCmpFunction key_compare;    // Key comparison function
    KeyFreeFunction key_free;      // Key free function (optional)
    ValueFreeFunction value_free;  // Value free function (optional)
    Lock lock;                     // Lock for thread safety
} Map;

// Fast hash for small keys (up to 8 bytes)
// Optimized small key hash - no memcpy, no branches
static inline uint64_t fast_small_hash(const void* key, size_t size) {
    if (size == sizeof(uint64_t)) {
        return *(const uint64_t*)key;
    } else {
        uint64_t hash = 0;
        memcpy(&hash, key, size > sizeof(hash) ? sizeof(hash) : size);
        return hash;
    }
}

// xxHash implementation with small key optimization
unsigned long xxhash(const void* key, size_t size) {
    if (size <= sizeof(uint64_t)) {
        return fast_small_hash(key, size);
    }
    return XXH3_64bits(key, size);
}

// Map creation with more configuration options
Map* map_create(const MapConfig* config) {
    if (!config || !config->key_compare || !config->key_len_func) {
        fprintf(stderr, "Invalid configuration\n");
        return NULL;
    }

    float max_load_factor = DEFAULT_MAX_LOAD_FACTOR;
    size_t capacity       = INITIAL_MAP_SIZE;
    if (config->initial_capacity > 0 && config->initial_capacity < SIZE_MAX) {
        capacity = config->initial_capacity;
    }

    if (max_load_factor <= 0.1f || max_load_factor > 0.95f) {
        max_load_factor = DEFAULT_MAX_LOAD_FACTOR;
    }

    Map* m = (Map*)malloc(sizeof(Map));
    if (!m) {
        perror("Failed to allocate memory for map");
        return NULL;
    }

    m->keys    = (void**)calloc(capacity, sizeof(void*));
    m->values  = (void**)calloc(capacity, sizeof(void*));
    m->deleted = (bool*)calloc(capacity, sizeof(bool));

    if (!m->keys || !m->values || !m->deleted) {
        if (m->keys)
            free(m->keys);
        if (m->values)
            free(m->values);
        if (m->deleted)
            free(m->deleted);
        free(m);
        perror("Failed to allocate memory for map entries");
        return NULL;
    }

    m->size            = 0;
    m->capacity        = capacity;
    m->tombstone_count = 0;
    m->max_load_factor = max_load_factor;
    m->hash            = config->hash_func ? config->hash_func : xxhash;
    m->key_compare     = config->key_compare;
    m->key_len_func    = config->key_len_func;
    m->key_free        = config->key_free ? config->key_free : free;
    m->value_free      = config->value_free ? config->value_free : free;
    lock_init(&m->lock);
    return m;
}

// Safe capacity growth calculation
static size_t calculate_new_capacity(size_t current) {
    if (current > SIZE_MAX / 2) {
        return SIZE_MAX;
    }
    size_t new_cap = current + (current / 2);
    return (new_cap < current) ? SIZE_MAX : new_cap;
}

// Resize the map with optimized rehashing
bool map_resize(Map* m, size_t new_capacity) {
    void** new_keys   = (void**)calloc(new_capacity, sizeof(void*));
    void** new_values = (void**)calloc(new_capacity, sizeof(void*));
    bool* new_deleted = (bool*)calloc(new_capacity, sizeof(bool));

    if (!new_keys || !new_values || !new_deleted) {
        free(new_keys);
        free(new_values);
        free(new_deleted);
        return false;
    }

    size_t old_capacity = m->capacity;
    void** old_keys     = m->keys;
    void** old_values   = m->values;
    bool* old_deleted   = m->deleted;

    // Swap in new arrays
    m->keys            = new_keys;
    m->values          = new_values;
    m->deleted         = new_deleted;
    m->capacity        = new_capacity;
    m->size            = 0;
    m->tombstone_count = 0;

    // Rehash all active entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_keys[i] && !old_deleted[i]) {
            map_set(m, old_keys[i], old_values[i]);
        }
    }

    // Free old arrays
    free(old_keys);
    free(old_values);
    free(old_deleted);

    return true;
}

size_t map_length(Map* m) {
    return m->size;
}

// Set a key-value pair with optimizations
void map_set(Map* m, void* key, void* value) {
    // Check if we need to resize
    float load_factor = (float)(m->size + m->tombstone_count) / (float)m->capacity;
    if (load_factor > m->max_load_factor) {
        size_t new_capacity = calculate_new_capacity(m->capacity);
        if (!map_resize(m, new_capacity)) {
            fprintf(stderr, "Map resize failed\n");
            return;
        }
    }

    // Check for tombstone cleanup
    else if ((double)m->tombstone_count / (double)m->size > TOMBSTONE_RATIO_THRESHOLD) {
        if (!map_resize(m, m->capacity)) {
            fprintf(stderr, "Map tombstone cleanup failed\n");
            return;
        }
    }

    size_t key_len         = m->key_len_func(key);
    size_t hash            = m->hash(key, key_len);
    size_t index           = hash % m->capacity;
    size_t hash2           = (hash % (m->capacity - 1)) + 1;  // For double hashing
    size_t first_tombstone = SIZE_MAX;
    size_t i               = 0;

    while (m->keys[index]) {
        if (m->deleted[index]) {
            if (first_tombstone == SIZE_MAX) {
                first_tombstone = index;
            }
        } else if (m->key_compare(m->keys[index], key)) {
            // Key exists - update value
            if (m->value_free) {
                m->value_free(m->values[index]);
            }
            m->values[index] = value;
            return;
        }

        i++;
        index = (hash + i * hash2) % m->capacity;

        if (i >= m->capacity) {
            fprintf(stderr, "Hashmap probe sequence too long\n");
            return;
        }
    }

    // Use first tombstone if we found one
    if (first_tombstone != SIZE_MAX) {
        index = first_tombstone;
        m->tombstone_count--;
    }

    // Insert new entry
    m->keys[index]    = key;
    m->values[index]  = value;
    m->deleted[index] = false;
    m->size++;
}

// Get a value by key with optimized probing
void* map_get(Map* m, void* key) {
    size_t key_len = m->key_len_func(key);
    size_t hash    = m->hash(key, key_len);
    size_t index   = hash % m->capacity;
    size_t hash2   = (hash % (m->capacity - 1)) + 1;
    size_t i       = 0;

    while (m->keys[index]) {
        if (!m->deleted[index] && m->key_compare(m->keys[index], key)) {
            return m->values[index];
        }
        i++;
        index = (hash + i * hash2) % m->capacity;

        if (i >= m->capacity) {
            break;
        }
    }
    return NULL;
}

// Remove a key-value pair with tombstone optimization
void map_remove(Map* m, void* key) {
    size_t key_len = m->key_len_func(key);
    size_t hash    = m->hash(key, key_len);
    size_t index   = hash % m->capacity;
    size_t hash2   = (hash % (m->capacity - 1)) + 1;
    size_t i       = 0;

    while (m->keys[index]) {
        if (!m->deleted[index] && m->key_compare(m->keys[index], key)) {
            if (m->key_free) {
                m->key_free((void*)m->keys[index]);
            }
            if (m->value_free) {
                m->value_free(m->values[index]);
            }

            m->keys[index]    = NULL;  // Mark as deleted
            m->values[index]  = NULL;
            m->deleted[index] = true;
            m->size--;
            m->tombstone_count++;
            return;
        }
        i++;
        index = (hash + i * hash2) % m->capacity;

        if (i >= m->capacity) {
            return;
        }
    }
}

// Map destruction with proper cleanup
void map_destroy(Map* m) {
    if (!m)
        return;

    if (m->key_free || m->value_free) {
        for (size_t i = 0; i < m->capacity; i++) {
            if (m->keys[i] && !m->deleted[i]) {
                if (m->key_free)
                    m->key_free((void*)m->keys[i]);
                if (m->value_free)
                    m->value_free(m->values[i]);
            }
        }
    }

    free(m->keys);
    free(m->values);
    free(m->deleted);
    lock_free(&m->lock);
    free(m);
}

map_iterator map_iter(Map* m) {
    map_iterator it = {.m = m, .index = 0};
    // Advance to first non-empty, non-deleted entry
    while (it.index < m->capacity && (!m->keys[it.index] || m->deleted[it.index])) {
        it.index++;
    }
    return it;
}

bool map_next(map_iterator* it, void** key, void** value) {
    while (it->index < it->m->capacity) {
        if (it->m->keys[it->index] && !it->m->deleted[it->index]) {
            if (key)
                *key = it->m->keys[it->index];
            if (value)
                *value = it->m->values[it->index];
            it->index++;

            // Prepare for next call
            while (it->index < it->m->capacity && (!it->m->keys[it->index] || it->m->deleted[it->index])) {
                it->index++;
            }
            return true;
        }
        it->index++;
    }
    return false;
}

// Thread-safe operations (unchanged from original)
void map_set_safe(Map* m, void* key, void* value) {
    lock_acquire(&m->lock);
    map_set(m, key, value);
    lock_release(&m->lock);
}

void* map_get_safe(Map* m, void* key) {
    lock_acquire(&m->lock);
    void* value = map_get(m, key);
    lock_release(&m->lock);
    return value;
}

void map_remove_safe(Map* m, void* key) {
    lock_acquire(&m->lock);
    map_remove(m, key);
    lock_release(&m->lock);
}

void map_set_from_array(Map* m, void** keys, void** values, size_t num_keys) {
    lock_acquire(&m->lock);

    for (size_t i = 0; i < num_keys; ++i) {
        map_set(m, keys[i], values[i]);
    }

    lock_release(&m->lock);
}
