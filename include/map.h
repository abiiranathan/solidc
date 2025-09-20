#ifndef SOLIDC_MAP_H
#define A02E572A_DDD85_4D77_AC81_41037EDE290A

#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./cmp.h"

#ifdef __cplusplus
extern "C" {
#endif

// Allow user to customize initial map size and load factor threshold
#ifndef INITIAL_MAP_SIZE
#define INITIAL_MAP_SIZE (size_t)16
#endif

#ifndef LOAD_FACTOR_THRESHOLD
#define LOAD_FACTOR_THRESHOLD (double)0.75
#endif

// Define alignment for cache line optimization
#define CACHE_LINE_SIZE 64

typedef size_t (*HashFunction)(const void* key, size_t size);
typedef size_t (*KeyLenFunction)(const void* key);
typedef bool (*KeyCmpFunction)(const void* key1, const void* key2);
typedef void (*KeyFreeFunction)(void* key);
typedef void (*ValueFreeFunction)(void* value);

typedef struct {
    size_t initial_capacity;       // Initial size of the map. If zero, the default size
                                   // is used.
    KeyCmpFunction key_compare;    // Required: Key comparison function
    KeyLenFunction key_len_func;   // Required: Key length function
    KeyFreeFunction key_free;      // Optional: Key cleanup function. Default is free from libc.
    ValueFreeFunction value_free;  // Optional: Value cleanup function. Default is
                                   // free from libc.
    float max_load_factor;         // Optional: When to resize (default 0.75)
    HashFunction hash_func;        // Optional: Custom hash function
} MapConfig;

#define MapConfigInt    (&(MapConfig){.key_compare = key_compare_int, .key_len_func = key_len_int})
#define MapConfigFloat  (&(MapConfig){.key_compare = key_compare_float, .key_len_func = key_len_float})
#define MapConfigDouble (&(MapConfig){.key_compare = key_compare_double, .key_len_func = key_len_double})
#define MapConfigStr    (&(MapConfig){.key_compare = key_compare_char_ptr, .key_len_func = key_len_char_ptr})

// Generic map implementation using xxhash as the hash function.
typedef struct hash_map Map;

// Create a new map and initialize it. If the initial capacity is 0, a default
// capacity is used, otherwise its used as it.
// key_compare is a function pointer used to compare 2 keys for equality.
// If free_entries is true, the keys and values are also freed.
Map* map_create(const MapConfig* config) __attribute__((warn_unused_result));

// Destroy the map and free the memory.
void map_destroy(Map* m);

// Set a key-value pair in the map
// This is idempotent. Returns true on success, false on failure.
bool map_set(Map* m, void* key, void* value);

// A thread-safe version of map_set. Returns true on success, false on failure.
bool map_set_safe(Map* m, void* key, void* value);

// Set multiple key-value pairs in the map from arrays.
// This is thread safe. Returns true on success, false on failure.
bool map_set_from_array(Map* m, void** keys, void** values, size_t num_keys);

// Get the value for a key in the map without locking
void* map_get(Map* m, void* key);

// Get the value for a key in the map with locking
void* map_get_safe(Map* m, void* key);

// Remove a key from the map without locking. Returns true if key was found and removed.
bool map_remove(Map* m, void* key);

// Remove a key from the map with locking. Returns true if key was found and removed.
bool map_remove_safe(Map* m, void* key);

// Iterator implementation
typedef struct {
    Map* m;
    size_t index;
} map_iterator;

// Create a map iterator.
map_iterator map_iter(Map* m);

// Advance to the next valid entry in the map.
bool map_next(map_iterator* it, void** key, void** value);

// Get the number of key-value pairs in the map
size_t map_length(Map* m);

// Get the capacity of the map
size_t map_capacity(Map* m);

static inline bool key_compare_int(const void* a, const void* b) {
    return a && b && *(const int*)a == *(const int*)b;
}

static inline bool key_compare_char_ptr(const void* a, const void* b) {
    return a && b && strcmp((const char*)a, (const char*)b) == 0;
}

static inline bool key_compare_float(const void* a, const void* b) {
    return a && b && cmp_float(*(const float*)a, *(const float*)b, (cmp_config_t){.epsilon = FLT_EPSILON});
}

static inline bool key_compare_double(const void* a, const void* b) {
    return a && b && cmp_double(*(const double*)a, *(const double*)b, (cmp_config_t){.epsilon = DBL_EPSILON});
}

// Key Len functions
// ==================================

// Returns sizeof(int).
static inline size_t key_len_int(const void* key) {
    (void)key;
    return sizeof(int);
}

// Returns sizeof(double).
static inline size_t key_len_double(const void* key) {
    (void)key;
    return sizeof(double);
}

// Returns sizeof(float).
static inline size_t key_len_float(const void* key) {
    (void)key;
    return sizeof(float);
}

// Return the strlen of the NULL-terminated key.
static inline size_t key_len_char_ptr(const void* key) {
    return strlen((const char*)key);
}

// Satisfies KeyFreeFunction and ValueFreeFunction.
// Used for keys and values that should not be freed.
static inline void NOFREE(void* ptr) {
    (void)ptr;  // Do nothing
}

#if defined(__cplusplus)
}
#endif

#endif /* SOLIDC_MAP_H */
