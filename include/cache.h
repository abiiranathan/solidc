#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rwlock.h"

// --- Configuration ---

/** Number of locks/shards. Must be power of 2 for fast bitwise modulus. */
#define CACHE_SHARD_COUNT 64

/** Access count threshold before promoting entry in LRU. */
#define CACHE_PROMOTION_THRESHOLD 3

/** Default TTL in seconds. */
#define CACHE_DEFAULT_TTL 300

// --- Data Structures ---

typedef struct cache_entry {
    struct cache_entry* prev;       // LRU list previous
    struct cache_entry* next;       // LRU list next
    struct cache_entry* hash_next;  // Hash collision chain

    time_t expires_at;      // Expiration timestamp
    uint32_t access_count;  // For lazy promotion
    uint32_t hash;          // Saved hash (faster resize/checks)
    size_t key_len;         // Length of key
    size_t value_len;       // Length of value

    // Flexible Array Member (C99)
    // Memory layout: [ data... (key) ] [ \0 ] [ ... (value) ]
    unsigned char data[];
} cache_entry_t;

typedef struct {
    cache_entry_t** buckets;  // Hash table buckets for this shard
    size_t bucket_count;      // Buckets per shard
    cache_entry_t* head;      // LRU head
    cache_entry_t* tail;      // LRU tail
    size_t size;              // Current items in this shard
    size_t capacity;          // Max items per shard
    rwlock_t lock;            // Lock for this specific shard
} cache_shard_t;

typedef struct {
    cache_shard_t shards[CACHE_SHARD_COUNT];  // Array of shards
    uint32_t default_ttl;
} cache_t;

// --- API ---

cache_t* cache_create(size_t total_capacity, uint32_t default_ttl);
void cache_destroy(cache_t* cache);
bool cache_get(cache_t* cache, const char* key, char** out_value, size_t* out_len);
bool cache_set(cache_t* cache, const char* key, const unsigned char* value, size_t value_len, uint32_t ttl_override);
void cache_invalidate(cache_t* cache, const char* key);
void cache_clear(cache_t* cache);

#endif
