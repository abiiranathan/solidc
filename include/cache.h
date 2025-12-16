/**
 * @file cache.h
 * @brief Cache management utilities for performance optimization.
 */

#ifndef CACHE_H
#define CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CACHE_SHARD_COUNT 32
#define CACHE_DEFAULT_TTL 300

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
 * @param key_len The length of the key.
 * @param out_len Pointer to store the size of the retrieved value.
 * @return Pointer to the value data, or NULL if not found.
 * @note YOU MUST CALL cache_release() on the returned pointer when done.
 */
const void* cache_get(cache_t* cache, const char* key, size_t key_len, size_t* out_len);

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
bool cache_set(cache_t* cache, const char* key, size_t key_len, const void* value, size_t value_len,
               uint32_t ttl_override);

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

/**
 * Serializes the current cache state to a binary file.
 *
 * The operation locks shards one by one, allowing concurrent reads/writes
 * to other shards while saving. The snapshot is not atomic across shards.
 *
 * @param cache_ptr The cache to save.
 * @param filename The output file path.
 * @return true on success, false on I/O error.
 */
bool cache_save(cache_t* cache_ptr, const char* filename);

/**
 * Loads cache entries from a binary file.
 *
 * Entries that are already expired in the file are skipped.
 * Entries loaded will be subject to the current cache's eviction policy
 * (if the file contains more items than the cache capacity).
 *
 * @param cache_ptr The cache to load into.
 * @param filename The input file path.
 * @return true on success, false on I/O error or invalid format.
 */
bool cache_load(cache_t* cache_ptr, const char* filename);

#ifdef __cplusplus
}
#endif

#endif  // CACHE_H
