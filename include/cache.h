/**
 * @file cache.h
 * @brief High-performance sharded open-addressing hash cache with 16-bit tags,
 * inline storage, lock-free per-slot seqlock concurrency, and an ABA-safe slab allocator.
 */

#ifndef CACHE_H
#define CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Number of shards used to partition the key space. Must be a power of 2. */
#define CACHE_SHARD_COUNT 256

/** Default entry time-to-live in seconds if no TTL override is provided. */
#define CACHE_DEFAULT_TTL 300

/**
 * Opaque handle to the cache instance.
 * All internal fields are hidden to preserve ABI compatibility.
 */
typedef struct cache_s cache_t;

/**
 * Creates and initializes a new lock-free sharded cache.
 *
 * @param capacity Total target capacity in live entries (distributed across shards).
 * @param default_ttl Default time-to-live in seconds (0 defaults to CACHE_DEFAULT_TTL).
 * @return Pointer to initialized cache instance, or NULL on allocation failure.
 * @threadsafety Safe to invoke concurrently with distinct targets.
 */
cache_t* cache_create(size_t capacity, uint32_t default_ttl);

/**
 * Destroys the cache instance and releases all associated memory pools.
 *
 * @param cache Pointer to the cache instance. If NULL, operation is a no-op.
 * @note Outstanding pointers returned by cache_get() become invalid after destruction.
 * @threadsafety Not thread-safe. Must be called when no other threads are accessing the cache.
 */
void cache_destroy(cache_t* cache);

/**
 * Retrieves a snapshot view of a value associated with the given key.
 *
 * Operations are completely lock-free on the read path via per-slot seqlock validation.
 *
 * @param cache The cache handle.
 * @param key Pointer to key string.
 * @param key_len Length of key in bytes (excluding null terminator).
 * @param out_len Pointer to variable receiving value length in bytes. May be NULL.
 * @return Pointer to value payload (thread-local snapshot for inline values), or NULL on miss/expiry.
 * @note Inline values are returned as pointers to thread-local storage consistent at call time.
 * @threadsafety Safe for concurrent use by multiple goroutines/threads.
 */
const void* cache_get(cache_t* cache, const char* key, size_t key_len, size_t* out_len);

/**
 * Deprecated compatibility stub for releasing zero-copy value references.
 *
 * @param ptr Pointer previously returned by cache_get().
 * @note Kept for API backward compatibility; lock-free inline storage requires no reference counts.
 * @threadsafety Safe for concurrent use.
 */
void cache_release(const void* ptr);

/**
 * Inserts or updates an entry in the cache using non-blocking CAS operations.
 *
 * Operates without acquiring shard-level locks. Overflow memory for large payloads
 * is allocated from a pre-allocated per-shard lock-free slab pool.
 *
 * @param cache The cache handle.
 * @param key Pointer to key string.
 * @param key_len Length of key in bytes.
 * @param value Pointer to payload buffer.
 * @param value_len Length of payload buffer in bytes.
 * @param ttl_override Custom time-to-live in seconds (0 uses cache default).
 * @return true on successful insertion/update, false on failure (e.g. key too long or full pool).
 * @threadsafety Safe for concurrent use by multiple threads.
 */
bool cache_set(cache_t* cache, const char* key, size_t key_len, const void* value, size_t value_len,
               uint32_t ttl_override);

/**
 * Atomically invalidates and removes an entry matching key from the cache index.
 *
 * @param cache The cache handle.
 * @param key Null-terminated key string to invalidate.
 * @threadsafety Safe for concurrent use by multiple threads.
 */
void cache_invalidate(cache_t* cache, const char* key);

/**
 * Resets and clears all entries across all cache shards.
 *
 * @param cache The cache handle.
 * @threadsafety Safe for concurrent use.
 */
void cache_clear(cache_t* cache);

/**
 * Returns the current total count of live entries stored across all shards.
 *
 * @param cache The cache instance.
 * @return Total live entry count, or 0 if cache is NULL.
 * @threadsafety Thread-safe via atomic counter aggregation.
 */
size_t get_total_cache_size(cache_t* cache);

/**
 * Returns the maximum configured capacity across all shards.
 *
 * @param cache The cache instance.
 * @return Total capacity, or 0 if cache is NULL.
 * @threadsafety Thread-safe.
 */
size_t get_total_capacity(cache_t* cache);

/**
 * Serializes current active non-expired cache entries to a binary stream.
 *
 * @param cache_ptr Pointer to cache instance.
 * @param filename Target binary output file path.
 * @return true on successful serialization, false on file I/O error.
 * @threadsafety Safe for concurrent use alongside readers and writers.
 */
bool cache_save(cache_t* cache_ptr, const char* filename);

/**
 * Deserializes cache entries from a binary snapshot file into the cache.
 *
 * @param cache_ptr Pointer to target cache instance.
 * @param filename Input binary snapshot file path.
 * @return true on successful parse, false on file open or format error.
 * @threadsafety Safe for concurrent use.
 */
bool cache_load(cache_t* cache_ptr, const char* filename);

/**
 * Advances the internal coarse timestamp used for fast TTL verification.
 *
 * Should be called periodically (e.g., every 100ms) by a background worker thread.
 * @threadsafety Safe for concurrent use across threads.
 */
void cache_tick(void);

/**
 * Flushes thread-local statistics and prints linear-probing distribution metrics to stderr.
 * @threadsafety Safe for concurrent use.
 */
void cache_probe_stats_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_H */
