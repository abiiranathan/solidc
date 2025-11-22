/**
 * @file cache.c
 * @brief High-performance thread-safe LRU cache with CLOCK eviction and metadata packing.
 *
 * This cache implementation uses several advanced techniques:
 * - Sharding for reduced lock contention across multiple threads
 * - Metadata packing to fit hash+keylen in a single 64-bit compare
 * - CLOCK eviction algorithm (approximation of LRU with better cache locality)
 * - Reference counting for safe concurrent access to cached values
 * - Open addressing hash table with linear probing
 * - Cache-line aligned data structures to reduce false sharing
 */

#define _POSIX_C_SOURCE 200809L

#include "cache.h"
#include "rwlock.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CACHE_LINE_SIZE           64  // Size of CPU cache line for alignment
#define INITIAL_BUCKET_MULTIPLIER 2   // Load factor: buckets = capacity * 2

// --- Optimization: Packed Metadata Constants ---
// We use a 64-bit integer to store {Hash:32, KeyLen:32}
// We reserve Hash values 0 and 1 for control flags.
#define HASH_EMPTY   0  // Slot never used
#define HASH_DELETED 1  // Slot previously used, now tombstone
#define HASH_MIN_VAL 2  // Minimum valid hash value (ensures no collision with control flags)

// Branch prediction hints for better CPU pipeline utilization
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

// --- Data Structures ---

/**
 * @brief A single cache entry containing key, value, and metadata.
 *
 * Memory layout:
 * [cache_entry_t][key_bytes]['\0'][back_pointer][value_bytes]
 *
 * The back_pointer allows us to recover the cache_entry_t* from a returned value pointer.
 * This enables reference counting without requiring the caller to track entries.
 */
typedef struct cache_entry {
    atomic_int ref_count;       // Reference count for safe concurrent access
    _Atomic uint8_t clock_bit;  // CLOCK algorithm: 1 = recently used, 0 = candidate for eviction

    time_t expires_at;  // Absolute expiration timestamp
    uint32_t hash;      // Full 32-bit hash of the key
    uint32_t key_len;   // Length of the key string (excluding null terminator)
    size_t value_len;   // Length of the value data

    unsigned char data[];  // Flexible array: [key]['\0'][back_ptr][value]
} cache_entry_t;

/**
 * @brief A cache-line aligned shard containing a portion of the cache.
 *
 * Sharding reduces lock contention by partitioning the cache into CACHE_SHARD_COUNT
 * independent hash tables, each with its own lock.
 */
typedef struct ALIGNED_CACHE_LINE {
    /**
     * Packed metadata array for fast lookups without pointer chasing.
     * Each uint64_t stores: High 32 bits = Hash, Low 32 bits = Key Length
     * This allows single 64-bit comparison instead of loading pointer + comparing fields.
     * Fits 8 metadata slots per 64-byte cache line.
     */
    uint64_t* metadata;

    /**
     * Entry pointers array. Only accessed if metadata matches.
     * Separated from metadata to improve cache utilization during probing.
     */
    cache_entry_t** entries;

    size_t bucket_count;  // Hash table size (always power of 2 for fast modulo)
    size_t size;          // Current number of valid entries
    size_t capacity;      // Maximum entries before eviction triggers
    size_t clock_hand;    // CLOCK algorithm: position for next eviction scan
    rwlock_t lock;        // Reader-writer lock for this shard
} aligned_cache_shard_t;

/**
 * @brief The main cache structure containing all shards.
 */
struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT];  // Fixed array of shards
    uint32_t default_ttl;                             // Default time-to-live in seconds
};

// --- Helpers ---

/**
 * Rounds up to the next power of 2.
 * Required because hash table size must be power of 2 for fast modulo via bitwise AND.
 */
static inline size_t next_power_of_2(size_t n) {
    if (n && !(n & (n - 1))) return n;  // Already power of 2
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

/**
 * FNV-1a hash function for string keys.
 * Returns hash >= HASH_MIN_VAL to avoid collision with control flags.
 */
static inline uint32_t hash_key(const char* key, size_t len) {
    uint32_t hash = 2166136261u;  // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)(unsigned char)key[i];
        hash *= 16777619u;  // FNV prime
    }
    // Ensure hash doesn't collide with HASH_EMPTY or HASH_DELETED
    if (unlikely(hash < HASH_MIN_VAL)) return hash + HASH_MIN_VAL;
    return hash;
}

/**
 * Packs hash and key length into single 64-bit value for fast comparison.
 * This is the core optimization: one 64-bit load + compare instead of pointer chase.
 */
static inline uint64_t pack_meta(uint32_t hash, uint32_t len) {
    return ((uint64_t)hash << 32) | (uint32_t)len;
}

/**
 * Extracts hash from packed metadata.
 */
static inline uint32_t meta_hash(uint64_t meta) {
    return (uint32_t)(meta >> 32);
}

/**
 * Recovers cache_entry_t pointer from a returned value pointer.
 * Uses the back-pointer stored between key and value in the entry's data array.
 */
static inline cache_entry_t* entry_from_value(const void* value_ptr) {
    if (unlikely(!value_ptr)) return NULL;
    // Back up by sizeof(pointer) to find the stored back-pointer
    cache_entry_t* const* back_ptr = (cache_entry_t* const*)((const char*)value_ptr - sizeof(cache_entry_t*));
    return *back_ptr;
}

/**
 * Calculates the value pointer from a cache entry.
 * Value is stored after: [key]['\0'][back_pointer]
 */
static inline const void* value_ptr_from_entry(const cache_entry_t* entry) {
    return (const void*)(entry->data + entry->key_len + 1 + sizeof(cache_entry_t*));
}

/**
 * Increments entry reference count (atomic, relaxed ordering is sufficient).
 */
static inline void entry_ref_inc(cache_entry_t* entry) {
    if (entry) atomic_fetch_add_explicit(&entry->ref_count, 1, memory_order_relaxed);
}

/**
 * Decrements entry reference count and frees if it reaches zero.
 * Uses acq_rel ordering to ensure all prior modifications are visible before free.
 */
static inline void entry_ref_dec(cache_entry_t* entry) {
    if (!entry) return;
    int old_val = atomic_fetch_sub_explicit(&entry->ref_count, 1, memory_order_acq_rel);
    if (unlikely(old_val == 1)) free(entry);
}

/**
 * Finds a slot in the hash table using linear probing.
 *
 * Key optimization: compares packed metadata (hash+keylen) first before touching pointers.
 * Only if metadata matches exactly do we dereference the entry pointer and memcmp the key.
 *
 * @param shard The shard to search in
 * @param hash The hash value of the key
 * @param key The key string
 * @param klen The key length
 * @param found Output: true if key found, false if empty/deleted slot
 * @return Index of the found key or suitable insertion slot
 */
static size_t find_slot(aligned_cache_shard_t* shard, uint32_t hash, const char* key, size_t klen, bool* found) {
    size_t mask            = shard->bucket_count - 1;  // Power-of-2 allows fast modulo
    size_t idx             = hash & mask;
    size_t probe_count     = 0;
    size_t first_tombstone = SIZE_MAX;  // Track first deleted slot for insertion

    uint64_t* metadata = shard->metadata;

    // Pre-compute target metadata: only exact match (hash AND length) is a candidate
    uint64_t target = pack_meta(hash, (uint32_t)klen);

    *found = false;

    while (probe_count < shard->bucket_count) {
        uint64_t meta = metadata[idx];
        uint32_t h    = meta_hash(meta);

        if (h == HASH_EMPTY) {
            // Empty slot: key definitely not in table
            return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
        }

        if (h == HASH_DELETED) {
            // Tombstone: record for potential insertion, but keep probing
            if (first_tombstone == SIZE_MAX) first_tombstone = idx;
        } else if (meta == target) {
            // Metadata match! Now pay the cost of pointer dereference + memcmp
            cache_entry_t* entry = shard->entries[idx];

            // Final verification: compare actual key bytes
            if (likely(memcmp(entry->data, key, klen) == 0)) {
                *found = true;
                return idx;
            }
            // Hash collision: different key with same hash+length, continue probing
        }

        idx = (idx + 1) & mask;  // Linear probing
        probe_count++;
    }

    // Table full or wrapped around
    return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
}

/**
 * Evicts an entry using the CLOCK algorithm (approximation of LRU).
 *
 * CLOCK works like a circular buffer:
 * - Each entry has a "clock bit" set to 1 when accessed
 * - Eviction scans forward, clearing bits (giving second chance)
 * - First entry with bit=0 is evicted
 *
 * This provides LRU-like behavior with better cache locality than true LRU.
 * We scan up to 2*bucket_count to handle worst case, then force evict.
 *
 * @return true if an entry was evicted, false otherwise
 */
static bool clock_evict(aligned_cache_shard_t* shard) {
    size_t scanned     = 0;
    size_t mask        = shard->bucket_count - 1;
    uint64_t* metadata = shard->metadata;

    // Scan up to 2 full cycles to find victim
    while (scanned < shard->bucket_count * 2) {
        size_t idx        = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;  // Advance hand

        uint64_t meta = metadata[idx];
        // Skip empty or deleted slots
        if (meta_hash(meta) < HASH_MIN_VAL) {
            scanned++;
            continue;
        }

        cache_entry_t* entry = shard->entries[idx];
        // Atomically read and clear clock bit
        uint8_t bit = atomic_exchange_explicit(&entry->clock_bit, 0, memory_order_relaxed);

        if (bit == 0) {
            // Clock bit was already 0: evict this entry
            shard->metadata[idx] = pack_meta(HASH_DELETED, 0);
            shard->entries[idx]  = NULL;
            shard->size--;
            entry_ref_dec(entry);
            return true;
        }
        // Clock bit was 1: gave it a second chance, continue
        scanned++;
    }

    // Fallback: force evict at current clock hand position
    size_t idx = shard->clock_hand;
    if (meta_hash(metadata[idx]) >= HASH_MIN_VAL) {
        cache_entry_t* entry = shard->entries[idx];
        shard->metadata[idx] = pack_meta(HASH_DELETED, 0);
        shard->entries[idx]  = NULL;
        shard->size--;
        entry_ref_dec(entry);
        return true;
    }
    return false;
}

/**
 * Creates a new cache with the specified capacity and default TTL.
 *
 * @param capacity Total capacity across all shards (0 = default 1000)
 * @param default_ttl Default time-to-live in seconds (0 = CACHE_DEFAULT_TTL)
 * @return Pointer to cache on success, NULL on allocation failure
 */
cache_t* cache_create(size_t capacity, uint32_t default_ttl) {
    struct cache_s* c = calloc(1, sizeof(struct cache_s));
    if (!c) return NULL;

    if (capacity == 0) capacity = 1000;
    size_t shard_cap = capacity / CACHE_SHARD_COUNT;
    if (shard_cap < 1) shard_cap = 1;

    c->default_ttl = default_ttl ? default_ttl : CACHE_DEFAULT_TTL;

    for (size_t i = 0; i < CACHE_SHARD_COUNT; i++) {
        c->shards[i].capacity = shard_cap;
        // Allocate 2x capacity to reduce collision rate
        size_t desired            = (size_t)(shard_cap * INITIAL_BUCKET_MULTIPLIER);
        c->shards[i].bucket_count = next_power_of_2(desired);

        // Allocate metadata array with cache-line alignment to reduce false sharing
        if (posix_memalign((void**)&c->shards[i].metadata, CACHE_LINE_SIZE,
                           c->shards[i].bucket_count * sizeof(uint64_t)) != 0) {
            return NULL;
        }
        memset(c->shards[i].metadata, 0, c->shards[i].bucket_count * sizeof(uint64_t));

        // Allocate entries array with cache-line alignment
        if (posix_memalign((void**)&c->shards[i].entries, CACHE_LINE_SIZE,
                           c->shards[i].bucket_count * sizeof(cache_entry_t*)) != 0) {
            return NULL;
        }
        memset(c->shards[i].entries, 0, c->shards[i].bucket_count * sizeof(cache_entry_t*));

        rwlock_init(&c->shards[i].lock);
    }
    return (cache_t*)c;
}

/**
 * Destroys the cache and frees all associated memory.
 * Decrements reference counts on all entries.
 */
void cache_destroy(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_wrlock(&cache->shards[i].lock);
        for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
            if (meta_hash(cache->shards[i].metadata[j]) >= HASH_MIN_VAL) {
                entry_ref_dec(cache->shards[i].entries[j]);
            }
        }
        free(cache->shards[i].metadata);
        free(cache->shards[i].entries);
        rwlock_unlock_wr(&cache->shards[i].lock);
        rwlock_destroy(&cache->shards[i].lock);
    }
    free(cache);
}

/**
 * Retrieves a value from the cache by key.
 *
 * The returned pointer is reference-counted. Caller must call cache_release() when done.
 * Automatically checks expiration and invalidates expired entries.
 *
 * @param cache_ptr The cache instance
 * @param key The key string (null-terminated)
 * @param out_len Output: length of the value (can be NULL)
 * @return Pointer to value on success, NULL if not found or expired
 */
const void* cache_get(cache_t* cache_ptr, const char* key, size_t* out_len) {
    if (unlikely(!cache_ptr || !key)) return NULL;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    size_t klen   = strlen(key);
    uint32_t hash = hash_key(key, klen);
    // Select shard using low bits of hash
    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    rwlock_rdlock(&shard->lock);

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (!found) {
        rwlock_unlock_rd(&shard->lock);
        return NULL;
    }

    cache_entry_t* entry = shard->entries[idx];

    // Check expiration
    if (unlikely(time(NULL) >= entry->expires_at)) {
        rwlock_unlock_rd(&shard->lock);
        // Invalidate expired entry (requires write lock)
        cache_invalidate(cache_ptr, key);
        return NULL;
    }

    // Mark as recently used for CLOCK algorithm
    atomic_store_explicit(&entry->clock_bit, 1, memory_order_relaxed);
    entry_ref_inc(entry);  // Increment ref count before returning

    if (out_len) *out_len = entry->value_len;

    rwlock_unlock_rd(&shard->lock);
    return value_ptr_from_entry(entry);
}

/**
 * Inserts or updates a key-value pair in the cache.
 *
 * If the cache is at capacity, triggers CLOCK eviction.
 * Creates a new entry with the specified TTL.
 *
 * @param cache_ptr The cache instance
 * @param key The key string (null-terminated)
 * @param value Pointer to value data
 * @param value_len Length of value data in bytes
 * @param ttl Time-to-live in seconds (0 = use default_ttl)
 * @return true on success, false on allocation failure
 */
bool cache_set(cache_t* cache_ptr, const char* key, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr || !key || !value)) return false;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    size_t klen   = strlen(key);
    uint32_t hash = hash_key(key, klen);

    // Allocate single contiguous block: [entry][key]['\0'][back_ptr][value]
    size_t alloc_sz          = sizeof(cache_entry_t) + klen + 1 + sizeof(cache_entry_t*) + value_len;
    cache_entry_t* new_entry = malloc(alloc_sz);
    if (!new_entry) return false;

    // Initialize entry metadata
    new_entry->hash       = hash;
    new_entry->key_len    = (uint32_t)klen;
    new_entry->value_len  = value_len;
    new_entry->expires_at = time(NULL) + (ttl ? ttl : cache->default_ttl);
    atomic_init(&new_entry->ref_count, 1);  // Initial ref count for cache ownership
    atomic_init(&new_entry->clock_bit, 1);  // Start with "recently used"

    // Copy key, back-pointer, and value into data array
    unsigned char* ptr = new_entry->data;
    memcpy(ptr, key, klen);
    ptr[klen]                          = '\0';
    *(cache_entry_t**)(ptr + klen + 1) = new_entry;  // Store back-pointer
    memcpy(ptr + klen + 1 + sizeof(cache_entry_t*), value, value_len);

    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    rwlock_wrlock(&shard->lock);

    // Evict if at capacity
    if (shard->size >= shard->capacity) {
        clock_evict(shard);
    }

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (found) {
        // Update existing entry
        cache_entry_t* old  = shard->entries[idx];
        shard->entries[idx] = new_entry;
        // Metadata (hash+keylen) is identical, no update needed
        entry_ref_dec(old);  // Release old entry
    } else {
        // Insert new entry
        shard->entries[idx]  = new_entry;
        shard->metadata[idx] = pack_meta(hash, (uint32_t)klen);
        shard->size++;
    }
    rwlock_unlock_wr(&shard->lock);

    return true;
}

/**
 * Removes a key from the cache.
 * Decrements the entry's reference count.
 */
void cache_invalidate(cache_t* cache_ptr, const char* key) {
    if (!cache_ptr || !key) return;
    struct cache_s* cache        = (struct cache_s*)cache_ptr;
    size_t klen                  = strlen(key);
    uint32_t hash                = hash_key(key, klen);
    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    rwlock_wrlock(&shard->lock);
    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (found) {
        cache_entry_t* entry = shard->entries[idx];
        shard->metadata[idx] = pack_meta(HASH_DELETED, 0);  // Mark as tombstone
        shard->entries[idx]  = NULL;
        shard->size--;
        entry_ref_dec(entry);
    }
    rwlock_unlock_wr(&shard->lock);
}

/**
 * Releases a reference to a cached value.
 * Must be called for every pointer returned by cache_get().
 */
void cache_release(const void* ptr) {
    if (!ptr) return;
    entry_ref_dec(entry_from_value(ptr));
}

/**
 * Removes all entries from the cache.
 * Decrements reference counts on all entries.
 */
void cache_clear(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_wrlock(&cache->shards[i].lock);
        for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
            if (meta_hash(cache->shards[i].metadata[j]) >= HASH_MIN_VAL) {
                entry_ref_dec(cache->shards[i].entries[j]);
            }
        }

        // Bulk wipe both arrays
        memset(cache->shards[i].metadata, 0, cache->shards[i].bucket_count * sizeof(uint64_t));
        memset(cache->shards[i].entries, 0, cache->shards[i].bucket_count * sizeof(cache_entry_t*));

        cache->shards[i].size       = 0;
        cache->shards[i].clock_hand = 0;
        rwlock_unlock_wr(&cache->shards[i].lock);
    }
}

/**
 * Returns the total number of entries across all shards.
 * Takes read locks on all shards.
 */
size_t get_total_cache_size(cache_t* cache_ptr) {
    if (!cache_ptr) return 0;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t total          = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].size;
        rwlock_unlock_rd(&cache->shards[i].lock);
    }
    return total;
}

/**
 * Returns the total capacity across all shards.
 * Takes read locks on all shards.
 */
size_t get_total_capacity(cache_t* cache_ptr) {
    if (!cache_ptr) return 0;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t total          = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].capacity;
        rwlock_unlock_rd(&cache->shards[i].lock);
    }
    return total;
}
