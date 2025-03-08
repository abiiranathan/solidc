#ifndef A02E572A_DD85_4D77_AC81_41037EDE290A
#define A02E572A_DD85_4D77_AC81_41037EDE290A

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Allow user to customize initial map size and load factor threshold
#ifndef INITIAL_MAP_SIZE
#define INITIAL_MAP_SIZE 16
#endif

#ifndef LOAD_FACTOR_THRESHOLD
#define LOAD_FACTOR_THRESHOLD 0.75
#endif

// Define alignment for cache line optimization
#define CACHE_LINE_SIZE 64

// Entry structure with cache line alignment
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) entry {
    const void* key;
    void* value;
} entry;

// Function pointer type for the key comparison.
// The function should return true if the keys are equal.
// This is necessary because the map does not know the size of the keys.
// memcmp can be used for byte-wise comparison of keys if they are of a known size.
typedef bool (*key_compare_fn)(const void*, const void*);

// Generic map implementation using xxhash as the hash function.
typedef struct map map;

// Create a new map and initialize it. If the initial capacity is 0, a default
// capacity is used, otherwise its used as it.
// key_compare is a function pointer used to compare 2 keys for equality.
// If free_entries is true, the keys and values are also freed.
map* map_create(size_t initial_capacity, key_compare_fn key_compare, bool free_entries)
    __attribute__((warn_unused_result()));

// Destroy the map and free the memory.
void map_destroy(map* m);

// Set a key-value pair in the map
// This is idempotent.
void map_set(map* m, void* key, void* value);

// A thread-safe version of map_set.
void map_set_safe(map* m, void* key, void* value);

// Set multiple key-value pairs in the map from arrays
// The keys and values arrays must be of the same length.
// Make sure that the keys and values are allocated on the heap to avoid
// unexpected behavior.
void map_set_from_array(map* m, void** keys, void** values, size_t num_keys);

// Get the value for a key in the map without locking
void* map_get(map* m, void* key);

// Get the value for a key in the map with locking
void* map_get_safe(map* m, void* key);

// Remove a key from the map without locking
void map_remove(map* m, void* key);

// Remove a key from the map with locking
void map_remove_safe(map* m, void* key);

// Set the hash function for the map
// The default hash function is xxhash.
// See https://github.com/Cyan4973/xxHash and https://xxhash.com/
void map_set_hash(map* m, unsigned long (*hash)(const void*));

// Get the number of key-value pairs in the map
size_t map_length(map* m);

// Get the capacity of the map
size_t map_capacity(map* m);

entry* map_get_entries(map* m);

#define map_foreach(map_ptr, e)                                                                    \
    size_t capacity = map_capacity(map_ptr);                                                       \
    entry* entries  = map_get_entries(map_ptr);                                                    \
    for (entry * (e) = entries; (e) < entries + capacity; (e)++)                                   \
        if ((e)->key != NULL)

// Function pointer for comparing 2 integers
bool key_compare_int(const void* a, const void* b);

// compares char* for equality.
bool key_compare_char_ptr(const void* a, const void* b);

// compares floats for equality
bool key_compare_float(const void* a, const void* b);

// compares doubles for equality.
bool key_compare_double(const void* a, const void* b);

#if defined(__cplusplus)
}
#endif

#endif /* A02E572A_DD85_4D77_AC81_41037EDE290A */
