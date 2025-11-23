#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CACHE_SHARD_COUNT         16
#define CACHE_PROMOTION_THRESHOLD 3
#define CACHE_DEFAULT_TTL         300

/**
 * Opaque handle to the cache.
 * Implementation details are hidden in cache.c to allow
 * alignment optimizations without breaking ABI.
 */
typedef struct cache_s cache_t;

/**
 * Creates a new cache.
 * @param capacity Total maximum number of entries (distributed across shards).
 * @param default_ttl Default time-to-live in seconds.
 * @return Pointer to new cache, or NULL on failure.
 */
cache_t* cache_create(size_t capacity, uint32_t default_ttl);

/**
 * Destroys the cache and frees resources.
 * Note: Zero-copy references held by users remain valid until cache_release() is called,
 * even after the cache object is destroyed.
 */
void cache_destroy(cache_t* cache);

/**
 * Zero-Copy Retrieval.
 * @param cache The cache handle.
 * @param key The lookup key.
 * @param out_len Pointer to store the size of the retrieved value.
 * @return Pointer to the value data, or NULL if not found.
 * @note YOU MUST CALL cache_release() on the returned pointer when done.
 */
const void* cache_get(cache_t* cache, const char* key, size_t* out_len);

/**
 * Releases a reference to a zero-copy value.
 * @param ptr The pointer returned by cache_get().
 */
void cache_release(const void* ptr);

/**
 * Stores a value in the cache.
 * @param cache The cache handle.
 * @param key The key.
 * @param value The data to store.
 * @param value_len The length of the data.
 * @param ttl_override Optional TTL in seconds (0 uses default).
 * @return true on success, false on failure.
 */
bool cache_set(cache_t* cache, const char* key, const void* value, size_t value_len, uint32_t ttl_override);

/**
 * Invalidates (removes) an entry from the cache index.
 */
void cache_invalidate(cache_t* cache, const char* key);

/**
 * Clears all entries from the cache index.
 */
void cache_clear(cache_t* cache);

/**
 * Returns the total number of entries currently in the cache across all shards.
 * Thread-safe: acquires read locks on all shards.
 * @param cache The cache instance.
 * @return Total number of cached entries, or 0 if cache is NULL.
 */
size_t get_total_cache_size(cache_t* cache);

/**
 * Returns the total capacity of the cache across all shards.
 * Thread-safe: acquires read locks on all shards.
 * Note: Capacity is set at creation and never changes, but we lock for consistency.
 * @param cache The cache instance.
 * @return Total capacity, or 0 if cache is NULL.
 */
size_t get_total_capacity(cache_t* cache);

#endif  // __CACHE_H__
