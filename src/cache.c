#define _POSIX_C_SOURCE 200809L  // For aligned_alloc if available

#include "cache.h"
#include "rwlock.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- Optimization Configuration ---

#define CACHE_LINE_SIZE 64

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

// Alignment attribute
#if defined(__GNUC__) || defined(__clang__)
#define ALIGNED_CACHE_LINE __attribute__((aligned(CACHE_LINE_SIZE)))
#elif defined(_MSC_VER)
#define ALIGNED_CACHE_LINE __declspec(align(CACHE_LINE_SIZE))
#else
#define ALIGNED_CACHE_LINE
#endif

// --- Data Structures ---

typedef struct cache_entry {
    struct cache_entry* prev;
    struct cache_entry* next;
    struct cache_entry* hash_next;

    time_t expires_at;
    uint32_t hash;

    // Using atomic for thread-safe lazy LRU updates without write lock
    _Atomic uint32_t access_count;

    // Reference Counter: Starts at 1 (Cache owns it). Readers increment.
    atomic_int ref_count;

    size_t key_len;
    size_t value_len;

    // Flexible Array Member: [ Key Bytes ] [ \0 ] [ BackPtr ] [ Value Bytes ]
    unsigned char data[];
} cache_entry_t;

// Shard struct aligned to cache line to prevent false sharing
typedef struct ALIGNED_CACHE_LINE {
    cache_entry_t** buckets;
    size_t bucket_count;
    cache_entry_t* head;
    cache_entry_t* tail;
    size_t size;
    size_t capacity;
    rwlock_t lock;
    // Compiler adds padding here to reach 64-byte alignment
} aligned_cache_shard_t;

struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT];
    uint32_t default_ttl;
};

// --- Helper Functions ---

static inline uint32_t hash_key(const char* key, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)(unsigned char)key[i];
        hash *= 16777619u;
    }
    return hash;
}

// Recovers the entry pointer from the user-facing value pointer
static inline cache_entry_t* entry_from_value(const void* value_ptr) {
    if (unlikely(!value_ptr)) return NULL;
    // Backtrack: The pointer immediately before value contains the entry address
    cache_entry_t* const* back_ptr = (cache_entry_t* const*)((const char*)value_ptr - sizeof(cache_entry_t*));
    return *back_ptr;
}

// Calculates the user-facing value pointer from the entry
static inline const void* value_ptr_from_entry(const cache_entry_t* entry) {
    // Layout: [Entry] ... data -> [Key] [\0] [Entry*] [Value]
    return (const void*)(entry->data + entry->key_len + 1 + sizeof(cache_entry_t*));
}

static inline void entry_ref_inc(cache_entry_t* entry) {
    atomic_fetch_add_explicit(&entry->ref_count, 1, memory_order_relaxed);
}

static inline void entry_ref_dec(cache_entry_t* entry) {
    // Acquire-Release ensures all memory ops on the object are visible before free
    int old_val = atomic_fetch_sub_explicit(&entry->ref_count, 1, memory_order_acq_rel);
    if (unlikely(old_val == 1)) {
        free(entry);
    }
}

// --- LRU Logic (Internal) ---

static void lru_remove(aligned_cache_shard_t* shard, cache_entry_t* entry) {
    if (entry->prev)
        entry->prev->next = entry->next;
    else
        shard->head = entry->next;

    if (entry->next)
        entry->next->prev = entry->prev;
    else
        shard->tail = entry->prev;

    entry->prev = NULL;
    entry->next = NULL;
}

static void lru_add_front(aligned_cache_shard_t* shard, cache_entry_t* entry) {
    entry->next = shard->head;
    entry->prev = NULL;

    if (shard->head) shard->head->prev = entry;
    shard->head = entry;

    if (!shard->tail) shard->tail = entry;
}

static void lru_evict(aligned_cache_shard_t* shard) {
    if (unlikely(!shard->tail)) return;

    cache_entry_t* victim = shard->tail;
    size_t bucket_idx     = victim->hash % shard->bucket_count;

    cache_entry_t** slot = &shard->buckets[bucket_idx];
    while (*slot && *slot != victim) {
        slot = &(*slot)->hash_next;
    }
    if (*slot) *slot = victim->hash_next;

    lru_remove(shard, victim);
    shard->size--;

    // Release cache's ownership (will free if no readers)
    entry_ref_dec(victim);
}

// --- API Implementation ---

cache_t* cache_create(size_t capacity, uint32_t default_ttl) {
// Allocate cache struct aligned to cache line size
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    void* mem = NULL;
    if (posix_memalign(&mem, CACHE_LINE_SIZE, sizeof(struct cache_s)) != 0) return NULL;
    struct cache_s* c = (struct cache_s*)mem;
#elif defined(_WIN32)
    struct cache_s* c = _aligned_malloc(sizeof(struct cache_s), CACHE_LINE_SIZE);
#else
    struct cache_s* c = malloc(sizeof(struct cache_s));  // Fallback
#endif

    if (unlikely(!c)) return NULL;
    memset(c, 0, sizeof(struct cache_s));

    if (capacity == 0) capacity = 1000;
    size_t shard_cap = capacity / CACHE_SHARD_COUNT;
    if (shard_cap < 1) shard_cap = 1;

    c->default_ttl = default_ttl ? default_ttl : CACHE_DEFAULT_TTL;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        c->shards[i].capacity     = shard_cap;
        c->shards[i].bucket_count = (size_t)(shard_cap * 1.5 + 1);
        c->shards[i].buckets      = calloc(c->shards[i].bucket_count, sizeof(cache_entry_t*));

        if (unlikely(!c->shards[i].buckets)) {
            // Cleanup previous shards
            for (int j = 0; j < i; j++) {
                free(c->shards[j].buckets);
                rwlock_destroy(&c->shards[j].lock);
            }
#ifdef _WIN32
            _aligned_free(c);
#else
            free(c);
#endif
            return NULL;
        }

        if (rwlock_init(&c->shards[i].lock) != 0) {
            return NULL;  // Cleanup omitted for brevity
        }
    }
    return (cache_t*)c;
}

void cache_destroy(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_wrlock(&cache->shards[i].lock);

        cache_entry_t* curr = cache->shards[i].head;
        while (curr) {
            cache_entry_t* next = curr->next;
            entry_ref_dec(curr);  // Release cache ownership
            curr = next;
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
    aligned_cache_shard_t* shard = &cache->shards[hash % CACHE_SHARD_COUNT];

    rwlock_rdlock(&shard->lock);

    cache_entry_t* entry = shard->buckets[hash % shard->bucket_count];

    // Hint CPU to prefetch next pointer to reduce stall
    if (entry) __builtin_prefetch(entry->hash_next);

    while (entry) {
        if (entry->hash == hash && entry->key_len == klen) {
            if (memcmp(entry->data, key, klen) == 0) {
                break;
            }
        }
        entry = entry->hash_next;
        if (entry) __builtin_prefetch(entry->hash_next);
    }

    if (!entry) {
        rwlock_unlock_rd(&shard->lock);
        return NULL;
    }

    // Lazy Expiry
    if (unlikely(time(NULL) >= entry->expires_at)) {
        rwlock_unlock_rd(&shard->lock);
        cache_invalidate(cache_ptr, key);
        return NULL;
    }

    // Found: Inc Ref Count (Zero Copy)
    entry_ref_inc(entry);

    if (out_len) *out_len = entry->value_len;

    // Lazy Promotion (Atomic update without write lock)
    uint32_t acc = atomic_fetch_add_explicit(&entry->access_count, 1, memory_order_relaxed) + 1;

    rwlock_unlock_rd(&shard->lock);

    // Promote only if threshold reached
    if (unlikely(acc >= CACHE_PROMOTION_THRESHOLD)) {
        rwlock_wrlock(&shard->lock);
        // Ensure entry wasn't evicted while we upgraded lock
        if (entry->prev || entry->next || shard->head == entry) {
            atomic_store_explicit(&entry->access_count, 0, memory_order_relaxed);
            lru_remove(shard, entry);
            lru_add_front(shard, entry);
        }
        rwlock_unlock_wr(&shard->lock);
    }

    return value_ptr_from_entry(entry);
}

void cache_release(const void* ptr) {
    if (unlikely(!ptr)) return;
    cache_entry_t* entry = entry_from_value(ptr);
    entry_ref_dec(entry);
}

bool cache_set(cache_t* cache_ptr, const char* key, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr || !key || !value)) return false;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    size_t klen   = strlen(key);
    uint32_t hash = hash_key(key, klen);

    // --- OPTIMIZATION: OPTIMISTIC ALLOCATION ---
    // Do heavy work (malloc, memcpy) BEFORE acquiring lock.

    size_t alloc_sz          = sizeof(cache_entry_t) + klen + 1 + sizeof(cache_entry_t*) + value_len;
    cache_entry_t* new_entry = malloc(alloc_sz);
    if (unlikely(!new_entry)) return false;

    // Setup Entry
    new_entry->hash       = hash;
    new_entry->key_len    = klen;
    new_entry->value_len  = value_len;
    new_entry->expires_at = time(NULL) + (ttl ? ttl : cache->default_ttl);
    new_entry->prev       = NULL;
    new_entry->next       = NULL;
    new_entry->hash_next  = NULL;
    atomic_init(&new_entry->ref_count, 1);
    atomic_init(&new_entry->access_count, 0);

    // Write Data: [Key] [\0] [BackPtr] [Value]
    unsigned char* ptr = new_entry->data;
    memcpy(ptr, key, klen);
    ptr[klen] = '\0';

    cache_entry_t** back_ptr = (cache_entry_t**)(ptr + klen + 1);
    *back_ptr                = new_entry;

    void* val_dest = (void*)(ptr + klen + 1 + sizeof(cache_entry_t*));
    memcpy(val_dest, value, value_len);

    // --- CRITICAL SECTION ---
    aligned_cache_shard_t* shard = &cache->shards[hash % CACHE_SHARD_COUNT];
    rwlock_wrlock(&shard->lock);

    // Check collision / Update existing
    size_t bucket_idx       = hash % shard->bucket_count;
    cache_entry_t* existing = shard->buckets[bucket_idx];

    while (existing) {
        if (existing->hash == hash && existing->key_len == klen) {
            if (memcmp(existing->data, key, klen) == 0) {
                // Replace logic
                cache_entry_t** slot = &shard->buckets[bucket_idx];
                while (*slot != existing)
                    slot = &(*slot)->hash_next;
                *slot = existing->hash_next;

                lru_remove(shard, existing);
                shard->size--;
                entry_ref_dec(existing);  // Mark for deletion
                break;
            }
        }
        existing = existing->hash_next;
    }

    if (unlikely(shard->size >= shard->capacity)) {
        lru_evict(shard);
    }

    // Link new entry
    lru_add_front(shard, new_entry);
    new_entry->hash_next       = shard->buckets[bucket_idx];
    shard->buckets[bucket_idx] = new_entry;
    shard->size++;

    rwlock_unlock_wr(&shard->lock);
    // --- END CRITICAL SECTION ---

    return true;
}

void cache_invalidate(cache_t* cache_ptr, const char* key) {
    if (unlikely(!cache_ptr || !key)) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    size_t klen                  = strlen(key);
    uint32_t hash                = hash_key(key, klen);
    aligned_cache_shard_t* shard = &cache->shards[hash % CACHE_SHARD_COUNT];

    rwlock_wrlock(&shard->lock);

    cache_entry_t** slot = &shard->buckets[hash % shard->bucket_count];
    while (*slot) {
        cache_entry_t* e = *slot;
        if (e->hash == hash && e->key_len == klen && memcmp(e->data, key, klen) == 0) {
            *slot = e->hash_next;
            lru_remove(shard, e);
            shard->size--;
            entry_ref_dec(e);
            break;
        }
        slot = &e->hash_next;
    }

    rwlock_unlock_wr(&shard->lock);
}

void cache_clear(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_wrlock(&cache->shards[i].lock);

        cache_entry_t* curr = cache->shards[i].head;
        while (curr) {
            cache_entry_t* next = curr->next;
            entry_ref_dec(curr);
            curr = next;
        }

        memset(cache->shards[i].buckets, 0, cache->shards[i].bucket_count * sizeof(cache_entry_t*));
        cache->shards[i].head = NULL;
        cache->shards[i].tail = NULL;
        cache->shards[i].size = 0;

        rwlock_unlock_wr(&cache->shards[i].lock);
    }
}

/**
 * Returns the total number of entries currently in the cache across all shards.
 * Thread-safe: acquires read locks on all shards.
 * @param cache The cache instance.
 * @return Total number of cached entries, or 0 if cache is NULL.
 */
size_t get_total_cache_size(cache_t* cache) {
    if (!cache) {
        return 0;
    }

    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].size;
        rwlock_unlock_rd(&cache->shards[i].lock);
    }

    return total;
}

/**
 * Returns the total capacity of the cache across all shards.
 * Thread-safe: acquires read locks on all shards.
 * Note: Capacity is set at creation and never changes, but we lock for consistency.
 * @param cache The cache instance.
 * @return Total capacity, or 0 if cache is NULL.
 */
size_t get_total_capacity(cache_t* cache) {
    if (!cache) {
        return 0;
    }

    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].capacity;
        rwlock_unlock_rd(&cache->shards[i].lock);
    }

    return total;
}
