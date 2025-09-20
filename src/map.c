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
#define DEFAULT_MAX_LOAD_FACTOR   0.75
#define TOMBSTONE_RATIO_THRESHOLD 0.5  // Rehash when tombstones > 50% of size
#define MIN_CAPACITY              8    // Minimum capacity to avoid frequent resizing

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Optimized map structure with better memory layout
typedef struct hash_map {
    void** keys_values;            // Interleaved keys and values for better cache locality
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

// Fast hash for small keys (up to 8 bytes) - optimized version
static inline uint64_t fast_small_hash(const void* key, size_t size) {
    // Use union to avoid strict aliasing violations
    union {
        uint64_t u64;
        uint32_t u32[2];
        uint16_t u16[4];
        uint8_t u8[8];
    } value = {0};

    // Copy only the needed bytes
    const uint8_t* src = (const uint8_t*)key;
    for (size_t i = 0; i < size && i < sizeof(value); i++) {
        value.u8[i] = src[i];
    }
    return value.u64;
}

// xxHash implementation with small key optimization
static unsigned long xxhash(const void* key, size_t size) {
    if (size <= sizeof(uint64_t)) {
        return fast_small_hash(key, size);
    }
    return XXH3_64bits(key, size);
}

// Helper function to calculate next power of two for better hashing
static size_t next_power_of_two(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if SIZE_MAX > UINT32_MAX
    n |= n >> 32;
#endif
    return n + 1;
}

// Safe capacity growth calculation
static size_t calculate_new_capacity(size_t current) {
    if (current > SIZE_MAX / 2) {
        return SIZE_MAX;
    }
    size_t new_cap = current * 2;
    return (new_cap < current) ? SIZE_MAX : new_cap;
}

// Map creation with better error handling and memory optimization
Map* map_create(const MapConfig* config) {
    if (!config || !config->key_compare || !config->key_len_func) {
        fprintf(stderr, "Invalid configuration\n");
        return NULL;
    }

    size_t capacity =
        MAX(MIN_CAPACITY,
            config->initial_capacity > 0 ? next_power_of_two(config->initial_capacity) : INITIAL_MAP_SIZE);

    if (capacity > SIZE_MAX / 2) {
        fprintf(stderr, "Requested capacity too large\n");
        return NULL;
    }

    float max_load_factor = config->max_load_factor > 0.1f && config->max_load_factor <= 0.95f
                                ? config->max_load_factor
                                : DEFAULT_MAX_LOAD_FACTOR;

    Map* m = (Map*)malloc(sizeof(Map));
    if (!m) {
        perror("Failed to allocate memory for map");
        return NULL;
    }

    // Allocate interleaved keys and values for better cache locality
    m->keys_values = (void**)calloc(capacity * 2, sizeof(void*));
    m->deleted     = (bool*)calloc(capacity, sizeof(bool));

    if (!m->keys_values || !m->deleted) {
        free(m->keys_values);
        free(m->deleted);
        free(m);
        return NULL;
    }

    m->size            = 0;
    m->capacity        = capacity;
    m->tombstone_count = 0;
    m->max_load_factor = max_load_factor;
    m->hash            = config->hash_func ? config->hash_func : xxhash;
    m->key_compare     = config->key_compare;
    m->key_len_func    = config->key_len_func;
    m->key_free        = config->key_free;
    m->value_free      = config->value_free;

    lock_init(&m->lock);
    return m;
}

// Helper to get key pointer from interleaved array
static inline void** get_key_ptr(Map* m, size_t index) {
    return &m->keys_values[index * 2];
}

// Helper to get value pointer from interleaved array
static inline void** get_value_ptr(Map* m, size_t index) {
    return &m->keys_values[index * 2 + 1];
}

// Resize the map with optimized rehashing and error handling
static bool map_resize(Map* m, size_t new_capacity) {
    if (new_capacity <= m->capacity || new_capacity > SIZE_MAX / 2) {
        return false;
    }

    void** new_keys_values = (void**)calloc(new_capacity * 2, sizeof(void*));
    bool* new_deleted      = (bool*)calloc(new_capacity, sizeof(bool));

    if (!new_keys_values || !new_deleted) {
        free(new_keys_values);
        free(new_deleted);
        return false;
    }

    // Save old data
    void** old_keys_values = m->keys_values;
    bool* old_deleted      = m->deleted;
    size_t old_capacity    = m->capacity;

    // Swap in new arrays
    m->keys_values     = new_keys_values;
    m->deleted         = new_deleted;
    m->capacity        = new_capacity;
    size_t old_size    = m->size;
    m->size            = 0;
    m->tombstone_count = 0;

    // Rehash all active entries
    bool success = true;
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_keys_values[i * 2] && !old_deleted[i]) {
            void* key   = old_keys_values[i * 2];
            void* value = old_keys_values[i * 2 + 1];

            // Manually rehash to avoid function call overhead
            size_t key_len     = m->key_len_func(key);
            size_t hash        = m->hash(key, key_len);
            size_t index       = hash & (new_capacity - 1);  // Use bitmask for power-of-two
            size_t hash2       = (hash >> 5) | 1;            // Ensure odd step for probing
            size_t probe_count = 0;

            while (*get_key_ptr(m, index) != NULL) {
                index = (index + hash2) & (new_capacity - 1);
                if (++probe_count >= new_capacity) {
                    success = false;
                    break;
                }
            }

            if (success) {
                *get_key_ptr(m, index)   = key;
                *get_value_ptr(m, index) = value;
                m->size++;
            } else {
                break;
            }
        }
    }

    if (success) {
        // Free old arrays
        free(old_keys_values);
        free(old_deleted);
    } else {
        // Restore original state on failure
        m->keys_values = old_keys_values;
        m->deleted     = old_deleted;
        m->capacity    = old_capacity;
        m->size        = old_size;
        free(new_keys_values);
        free(new_deleted);
        return false;
    }

    return true;
}

size_t map_length(Map* m) {
    return m->size;
}

// Set a key-value pair with optimizations and better error handling
bool map_set(Map* m, void* key, void* value) {
    // Check if we need to resize
    size_t total_entries = m->size + m->tombstone_count;
    if (total_entries >= m->capacity * m->max_load_factor) {
        size_t new_capacity = calculate_new_capacity(m->capacity);
        if (new_capacity <= m->capacity || !map_resize(m, new_capacity)) {
            fprintf(stderr, "Map resize failed\n");
            return false;
        }
    }
    // Check for tombstone cleanup
    else if (m->tombstone_count > 0 &&
             (double)m->tombstone_count / (double)m->size > TOMBSTONE_RATIO_THRESHOLD) {
        if (!map_resize(m, m->capacity)) {  // Same capacity to clean tombstones
            fprintf(stderr, "Map tombstone cleanup failed\n");
            return false;
        }
    }

    size_t key_len         = m->key_len_func(key);
    size_t hash            = m->hash(key, key_len);
    size_t index           = hash & (m->capacity - 1);  // Bitmask for power-of-two capacity
    size_t hash2           = (hash >> 5) | 1;           // Ensure odd step for probing
    size_t first_tombstone = SIZE_MAX;
    size_t probe_count     = 0;

    while (probe_count < m->capacity) {
        void** current_key = get_key_ptr(m, index);

        if (*current_key == NULL) {
            break;  // Found empty slot
        }

        if (m->deleted[index]) {
            if (first_tombstone == SIZE_MAX) {
                first_tombstone = index;
            }
        } else if (m->key_compare(*current_key, key)) {
            // Key exists - update value
            if (m->value_free) {
                m->value_free(*get_value_ptr(m, index));
            }
            *get_value_ptr(m, index) = value;
            return true;
        }

        probe_count++;
        index = (index + hash2) & (m->capacity - 1);
    }

    // Use first tombstone if we found one
    if (first_tombstone != SIZE_MAX) {
        index = first_tombstone;
        m->tombstone_count--;
    }

    // Insert new entry
    *get_key_ptr(m, index)   = key;
    *get_value_ptr(m, index) = value;
    m->deleted[index]        = false;
    m->size++;

    return true;
}

// Get a value by key with optimized probing
void* map_get(Map* m, void* key) {
    size_t key_len     = m->key_len_func(key);
    size_t hash        = m->hash(key, key_len);
    size_t index       = hash & (m->capacity - 1);
    size_t hash2       = (hash >> 5) | 1;
    size_t probe_count = 0;

    while (probe_count < m->capacity) {
        void** current_key = get_key_ptr(m, index);

        if (*current_key == NULL) {
            break;  // End of probe sequence
        }

        if (!m->deleted[index] && m->key_compare(*current_key, key)) {
            return *get_value_ptr(m, index);
        }

        probe_count++;
        index = (index + hash2) & (m->capacity - 1);
    }

    return NULL;
}

// Remove a key-value pair with tombstone optimization
bool map_remove(Map* m, void* key) {
    size_t key_len     = m->key_len_func(key);
    size_t hash        = m->hash(key, key_len);
    size_t index       = hash & (m->capacity - 1);
    size_t hash2       = (hash >> 5) | 1;
    size_t probe_count = 0;

    while (probe_count < m->capacity) {
        void** current_key = get_key_ptr(m, index);

        if (*current_key == NULL) {
            break;  // Key not found
        }

        if (!m->deleted[index] && m->key_compare(*current_key, key)) {
            if (m->key_free) {
                m->key_free(*current_key);
            }
            if (m->value_free) {
                m->value_free(*get_value_ptr(m, index));
            }

            *current_key             = NULL;
            *get_value_ptr(m, index) = NULL;
            m->deleted[index]        = true;
            m->size--;
            m->tombstone_count++;
            return true;
        }

        probe_count++;
        index = (index + hash2) & (m->capacity - 1);
    }

    return false;
}

// Map destruction with proper cleanup
void map_destroy(Map* m) {
    if (!m) return;

    if (m->key_free || m->value_free) {
        for (size_t i = 0; i < m->capacity; i++) {
            if (*get_key_ptr(m, i) && !m->deleted[i]) {
                if (m->key_free) m->key_free(*get_key_ptr(m, i));
                if (m->value_free) m->value_free(*get_value_ptr(m, i));
            }
        }
    }

    free(m->keys_values);
    free(m->deleted);
    lock_free(&m->lock);
    free(m);
}

map_iterator map_iter(Map* m) {
    map_iterator it = {.m = m, .index = 0};
    // Advance to first non-empty, non-deleted entry
    while (it.index < m->capacity && (*get_key_ptr(m, it.index) == NULL || m->deleted[it.index])) {
        it.index++;
    }
    return it;
}

bool map_next(map_iterator* it, void** key, void** value) {
    Map* m = it->m;
    while (it->index < m->capacity) {
        if (*get_key_ptr(m, it->index) != NULL && !m->deleted[it->index]) {
            if (key) *key = *get_key_ptr(m, it->index);
            if (value) *value = *get_value_ptr(m, it->index);
            it->index++;

            // Prepare for next call
            while (it->index < m->capacity && (*get_key_ptr(m, it->index) == NULL || m->deleted[it->index])) {
                it->index++;
            }
            return true;
        }
        it->index++;
    }
    return false;
}

// Thread-safe operations
bool map_set_safe(Map* m, void* key, void* value) {
    lock_acquire(&m->lock);
    bool result = map_set(m, key, value);
    lock_release(&m->lock);
    return result;
}

void* map_get_safe(Map* m, void* key) {
    lock_acquire(&m->lock);
    void* value = map_get(m, key);
    lock_release(&m->lock);
    return value;
}

bool map_remove_safe(Map* m, void* key) {
    lock_acquire(&m->lock);
    bool result = map_remove(m, key);
    lock_release(&m->lock);
    return result;
}

bool map_set_from_array(Map* m, void** keys, void** values, size_t num_keys) {
    lock_acquire(&m->lock);
    bool success = true;

    for (size_t i = 0; i < num_keys; ++i) {
        if (!map_set(m, keys[i], values[i])) {
            success = false;
            break;
        }
    }

    lock_release(&m->lock);
    return success;
}
