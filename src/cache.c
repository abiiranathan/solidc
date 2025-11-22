#define _POSIX_C_SOURCE 200809L

#include "cache.h"
#include "rwlock.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CACHE_LINE_SIZE           64
#define INITIAL_BUCKET_MULTIPLIER 2

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

// --- Data Structures ---

typedef struct cache_entry {
    atomic_int ref_count;
    _Atomic uint8_t clock_bit;

    time_t expires_at;
    // Note: 'hash' is technically redundant in the entry now since it lives in the bucket,
    // but we keep it for recovery/verification if needed.
    uint32_t hash;
    uint32_t key_len;
    size_t value_len;

    unsigned char data[];  // [Key] [\0] [BackPtr] [Value]
} cache_entry_t;

// --- OPTIMIZATION: Metadata Packing ---
// sizeof(bucket_t) == 16 bytes.
// Fits 4 buckets perfectly into one 64-byte Cache Line.
typedef struct {
    cache_entry_t* entry;  // 8 bytes (64-bit ptr)
    uint32_t hash;         // 4 bytes
    uint32_t _pad;         // 4 bytes (Explicit padding to reach 16 bytes)
} bucket_t;

static cache_entry_t g_tombstone = {0};

typedef struct ALIGNED_CACHE_LINE {
    bucket_t* buckets;  // Changed from cache_entry_t**
    size_t bucket_count;
    size_t size;
    size_t capacity;
    size_t clock_hand;
    rwlock_t lock;
} aligned_cache_shard_t;

struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT];
    uint32_t default_ttl;
};

// --- Helpers ---

static inline size_t next_power_of_2(size_t n) {
    if (n && !(n & (n - 1))) return n;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

static inline uint32_t hash_key(const char* key, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)(unsigned char)key[i];
        hash *= 16777619u;
    }
    return hash;
}

static inline cache_entry_t* entry_from_value(const void* value_ptr) {
    if (unlikely(!value_ptr)) return NULL;
    cache_entry_t* const* back_ptr = (cache_entry_t* const*)((const char*)value_ptr - sizeof(cache_entry_t*));
    return *back_ptr;
}

static inline const void* value_ptr_from_entry(const cache_entry_t* entry) {
    return (const void*)(entry->data + entry->key_len + 1 + sizeof(cache_entry_t*));
}

static inline void entry_ref_inc(cache_entry_t* entry) {
    if (entry != &g_tombstone) {
        atomic_fetch_add_explicit(&entry->ref_count, 1, memory_order_relaxed);
    }
}

static inline void entry_ref_dec(cache_entry_t* entry) {
    if (entry == &g_tombstone) return;
    int old_val = atomic_fetch_sub_explicit(&entry->ref_count, 1, memory_order_acq_rel);
    if (unlikely(old_val == 1)) {
        free(entry);
    }
}

// --- Logic ---

static size_t find_slot(aligned_cache_shard_t* shard, uint32_t hash, const char* key, size_t klen, bool* found) {
    size_t mask            = shard->bucket_count - 1;
    size_t idx             = hash & mask;
    size_t probe_count     = 0;
    size_t first_tombstone = SIZE_MAX;

    *found = false;

    // Prefetching the array is now extremely effective because we scan linearly
    // and all required data (hashes) are inside the array.

    while (probe_count < shard->bucket_count) {
        // Load bucket struct (16 bytes)
        bucket_t* b          = &shard->buckets[idx];
        cache_entry_t* entry = b->entry;

        if (!entry) {
            return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
        }

        if (entry == &g_tombstone) {
            if (first_tombstone == SIZE_MAX) first_tombstone = idx;
            idx = (idx + 1) & mask;
            probe_count++;
            continue;
        }

        // --- CACHE MISS SAVER ---
        // Check the hash stored in the bucket array FIRST.
        // We only dereference 'entry' if the hash matches.
        if (b->hash == hash) {
            // Now we take the cache miss hit to check the key
            if (entry->key_len == klen && memcmp(entry->data, key, klen) == 0) {
                *found = true;
                return idx;
            }
        }

        idx = (idx + 1) & mask;
        probe_count++;
    }

    return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
}

static bool clock_evict(aligned_cache_shard_t* shard) {
    size_t scanned = 0;
    size_t mask    = shard->bucket_count - 1;

    while (scanned < shard->bucket_count * 2) {
        size_t idx        = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;

        bucket_t* b          = &shard->buckets[idx];
        cache_entry_t* entry = b->entry;

        if (!entry || entry == &g_tombstone) {
            scanned++;
            continue;
        }

        uint8_t bit = atomic_exchange_explicit(&entry->clock_bit, 0, memory_order_relaxed);

        if (bit == 0) {
            // Evict
            b->entry = &g_tombstone;
            b->hash  = 0;  // Clear hash for cleanliness
            shard->size--;
            entry_ref_dec(entry);
            return true;
        }
        scanned++;
    }

    // Force eviction
    bucket_t* b = &shard->buckets[shard->clock_hand];
    if (b->entry && b->entry != &g_tombstone) {
        cache_entry_t* victim = b->entry;
        b->entry              = &g_tombstone;
        b->hash               = 0;
        shard->size--;
        entry_ref_dec(victim);
        return true;
    }

    return false;
}

// --- API ---

cache_t* cache_create(size_t capacity, uint32_t default_ttl) {
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    void* mem = NULL;
    if (posix_memalign(&mem, CACHE_LINE_SIZE, sizeof(struct cache_s)) != 0) return NULL;
    struct cache_s* c = (struct cache_s*)mem;
#elif defined(_WIN32)
    struct cache_s* c = _aligned_malloc(sizeof(struct cache_s), CACHE_LINE_SIZE);
#else
    struct cache_s* c = malloc(sizeof(struct cache_s));
#endif
    if (!c) return NULL;
    memset(c, 0, sizeof(struct cache_s));

    if (capacity == 0) capacity = 1000;
    size_t shard_cap = capacity / CACHE_SHARD_COUNT;
    if (shard_cap < 1) shard_cap = 1;

    c->default_ttl = default_ttl ? default_ttl : CACHE_DEFAULT_TTL;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        c->shards[i].capacity     = shard_cap;
        size_t desired            = (size_t)(shard_cap * INITIAL_BUCKET_MULTIPLIER);
        c->shards[i].bucket_count = next_power_of_2(desired);

        // Allocate contiguous array of bucket structs
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
        void* bmem = NULL;
        if (posix_memalign(&bmem, CACHE_LINE_SIZE, c->shards[i].bucket_count * sizeof(bucket_t)) != 0) {
            // Clean up omitted
            return NULL;
        }
        c->shards[i].buckets = (bucket_t*)bmem;
#else
        c->shards[i].buckets = calloc(c->shards[i].bucket_count, sizeof(bucket_t));
#endif
        // Important: calloc zeroes the pointers and the hashes
        if (!c->shards[i].buckets) return NULL;
        memset(c->shards[i].buckets, 0, c->shards[i].bucket_count * sizeof(bucket_t));

        if (rwlock_init(&c->shards[i].lock) != 0) return NULL;
    }
    return (cache_t*)c;
}

void cache_destroy(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_wrlock(&cache->shards[i].lock);
        for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
            cache_entry_t* entry = cache->shards[i].buckets[j].entry;
            if (entry && entry != &g_tombstone) {
                entry_ref_dec(entry);
            }
        }
        free(cache->shards[i].buckets);
        rwlock_unlock_wr(&cache->shards[i].lock);
        rwlock_destroy(&cache->shards[i].lock);
    }
#ifdef _WIN32
    _aligned_free(cache);
#else
    free(cache);
#endif
}

const void* cache_get(cache_t* cache_ptr, const char* key, size_t* out_len) {
    if (unlikely(!cache_ptr || !key)) return NULL;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    size_t klen                  = strlen(key);
    uint32_t hash                = hash_key(key, klen);
    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    rwlock_rdlock(&shard->lock);

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (!found) {
        rwlock_unlock_rd(&shard->lock);
        return NULL;
    }

    bucket_t* b          = &shard->buckets[idx];
    cache_entry_t* entry = b->entry;

    if (unlikely(time(NULL) >= entry->expires_at)) {
        rwlock_unlock_rd(&shard->lock);
        cache_invalidate(cache_ptr, key);
        return NULL;
    }

    atomic_store_explicit(&entry->clock_bit, 1, memory_order_relaxed);
    entry_ref_inc(entry);

    if (out_len) *out_len = entry->value_len;

    rwlock_unlock_rd(&shard->lock);
    return value_ptr_from_entry(entry);
}

bool cache_set(cache_t* cache_ptr, const char* key, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr || !key || !value)) return false;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    size_t klen   = strlen(key);
    uint32_t hash = hash_key(key, klen);

    size_t alloc_sz          = sizeof(cache_entry_t) + klen + 1 + sizeof(cache_entry_t*) + value_len;
    cache_entry_t* new_entry = malloc(alloc_sz);
    if (unlikely(!new_entry)) return false;

    // Fill Entry
    new_entry->hash       = hash;
    new_entry->key_len    = klen;
    new_entry->value_len  = value_len;
    new_entry->expires_at = time(NULL) + (ttl ? ttl : cache->default_ttl);
    atomic_init(&new_entry->ref_count, 1);
    atomic_init(&new_entry->clock_bit, 1);

    unsigned char* ptr = new_entry->data;
    memcpy(ptr, key, klen);
    ptr[klen]                          = '\0';
    *(cache_entry_t**)(ptr + klen + 1) = new_entry;
    memcpy(ptr + klen + 1 + sizeof(cache_entry_t*), value, value_len);

    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];
    rwlock_wrlock(&shard->lock);

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    bucket_t* b = &shard->buckets[idx];

    if (found) {
        // Replace
        cache_entry_t* old = b->entry;
        b->entry           = new_entry;
        b->hash            = hash;  // ensure hash is set (redundant if same, but safe)
        entry_ref_dec(old);
    } else {
        // Insert
        if (shard->size >= shard->capacity) {
            clock_evict(shard);
            // Re-find slot just in case logic needs it, though usually safe to use idx
            // But strict correctness suggests find_slot again if we want to be paranoid.
            // However, idx is either empty or tombstone. It's safe.
        }
        b->entry = new_entry;
        b->hash  = hash;  // <--- WRITE HASH TO BUCKET
        shard->size++;
    }

    rwlock_unlock_wr(&shard->lock);
    return true;
}

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
        bucket_t* b          = &shard->buckets[idx];
        cache_entry_t* entry = b->entry;
        b->entry             = &g_tombstone;
        b->hash              = 0;  // Clear hash
        shard->size--;
        entry_ref_dec(entry);
    }
    rwlock_unlock_wr(&shard->lock);
}

void cache_release(const void* ptr) {
    if (!ptr) return;
    entry_ref_dec(entry_from_value(ptr));
}

void cache_clear(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_wrlock(&cache->shards[i].lock);
        for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
            cache_entry_t* entry = cache->shards[i].buckets[j].entry;
            if (entry && entry != &g_tombstone) {
                entry_ref_dec(entry);
            }
            cache->shards[i].buckets[j].entry = NULL;
            cache->shards[i].buckets[j].hash  = 0;
        }
        cache->shards[i].size       = 0;
        cache->shards[i].clock_hand = 0;
        rwlock_unlock_wr(&cache->shards[i].lock);
    }
}

size_t get_total_cache_size(cache_t* cache) {
    if (!cache) return 0;
    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].size;
        rwlock_unlock_rd(&cache->shards[i].lock);
    }
    return total;
}

size_t get_total_capacity(cache_t* cache) {
    if (!cache) return 0;
    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].capacity;
        rwlock_unlock_rd(&cache->shards[i].lock);
    }
    return total;
}
