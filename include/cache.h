#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Cross-platform Read-Write Lock.
#include "rwlock.h"

/** Maximum cache key length. */
#define CACHE_KEY_MAX_LEN 256

/** Default cache capacity (number of entries). */
#define CACHE_DEFAULT_CAPACITY 1000

/** Default TTL in seconds (5 minutes). */
#define CACHE_DEFAULT_TTL 300

/** Access count threshold before promoting entry in LRU (reduces lock contention). */
#define CACHE_PROMOTION_THRESHOLD 3

/** Represents a cached entry. */
typedef struct cache_entry {
    char key[CACHE_KEY_MAX_LEN];    // Cache key
    unsigned char* value;           // Cached value in bytes
    size_t value_len;               // Length of cached value
    time_t expires_at;              // Expiration timestamp
    uint32_t access_count;          // Access counter for lazy LRU updates
    struct cache_entry* prev;       // LRU list previous
    struct cache_entry* next;       // LRU list next
    struct cache_entry* hash_next;  // Hash collision chain
} cache_entry_t;

/** Response cache with LRU eviction. Thread-safe. */
typedef struct {
    cache_entry_t** buckets;  // Hash table buckets
    size_t bucket_count;      // Number of hash buckets
    cache_entry_t* head;      // LRU list head (most recent)
    cache_entry_t* tail;      // LRU list tail (least recent)
    size_t capacity;          // Maximum number of entries
    size_t size;              // Current number of entries
    uint32_t default_ttl;     // Default TTL in seconds
    rwlock_t lock;            // Read-write lock for thread safety
} cache_t;

/**
 * Creates a new cache.
 * @param capacity Maximum number of cache entries (0 uses default).
 * @param default_ttl Default time-to-live in seconds (0 uses default).
 * @return Pointer to new cache on success, NULL on failure.
 * @note Caller must free with cache_destroy().
 */
cache_t* cache_create(size_t capacity, uint32_t default_ttl);

/**
 * Destroys a cache and frees all resources.
 * @param cache The cache to destroy. Safe to pass NULL.
 */
void cache_destroy(cache_t* cache);

/**
 * Retrieves a cached value.
 * @param cache The cache to query.
 * @param key The cache key.
 * @param out_value Pointer to store the cached value (caller must free). The data is not null
 * terminated.
 * @param out_len Pointer to store the value length.
 * @return true if found and not expired, false otherwise.
 * @note If true is returned, caller must free *out_value.
 * @note Thread-safe. Uses read lock for lookup with lazy LRU updates.
 */
bool cache_get(cache_t* cache, const char* key, char** out_value, size_t* out_len);

/**
 * Stores a value in the cache.
 * @param cache The cache to store in.
 * @param key The cache key.
 * @param value The value to cache (will be copied).
 * @param value_len Length of the value.
 * @param ttl_override Time-to-live override (0 uses default).
 * @return true on success, false on failure.
 * @note Thread-safe. Acquires write lock.
 */
bool cache_set(cache_t* cache, const char* key, const unsigned char* value, size_t value_len,
               uint32_t ttl_override);

/**
 * Invalidates a cache entry.
 * @param cache The cache to modify.
 * @param key The cache key to invalidate.
 * @note Thread-safe. Acquires write lock.
 */
void cache_invalidate(cache_t* cache, const char* key);

/**
 * Clears all entries from the cache.
 * @param cache The cache to clear.
 * @note Thread-safe. Acquires write lock.
 */
void cache_clear(cache_t* cache);

#endif  // __CACHE_H__
