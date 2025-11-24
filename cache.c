#include "include/spinlock.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

// --- Configuration ---
#define CACHE_LINE_SIZE           64
#define INITIAL_BUCKET_MULTIPLIER 2

// --- Constants ---
#define HASH_EMPTY                0
#define HASH_DELETED              1
#define HASH_MIN_VAL              2
#define CACHE_SHARD_COUNT         16
#define CACHE_PROMOTION_THRESHOLD 3
#define CACHE_DEFAULT_TTL         300

/**
 * Opaque handle to the cache.
 * Implementation details are hidden in cache.c to allow
 * alignment optimizations without breaking ABI.
 */
typedef struct cache_s cache_t;

// Branch hints
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

// --- Globals ---
static volatile _Atomic(time_t) g_current_time;

// --- Data Structures ---

typedef struct cache_entry {
    atomic_int ref_count;           // Hot cache line #1
    atomic_uint_fast8_t clock_bit;  // Keep separate to allow relaxed silent stores
    uint32_t expires_at;            // Absolute timestamp
    uint32_t key_len;
    size_t value_len;
    unsigned char data[];  // [key]['\0'][back_ptr][value]
} cache_entry_t;

typedef struct {
    uint64_t metadata;     // [Hash(32) | KeyLen(32)] - Read Only during Get
    cache_entry_t* entry;  // Pointer to heap entry
} cache_slot_t;

typedef struct ALIGNED_CACHE_LINE {
    cache_slot_t* slots;
    size_t bucket_count;
    size_t size;
    size_t capacity;
    size_t clock_hand;
    fast_rwlock_t lock;
} aligned_cache_shard_t;

struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT];
    uint32_t default_ttl;
};

// --- Helpers ---

void cache_invalidate(cache_t* cache_ptr, const char* key, size_t key_len);

// Constructor to init time
static void __attribute__((constructor)) init_timer() {
    atomic_store_explicit(&g_current_time, time(NULL), memory_order_relaxed);
}

// User should call this periodically (e.g. background thread)
void cache_update_time(void) {
    atomic_store_explicit(&g_current_time, time(NULL), memory_order_relaxed);
}

static inline time_t get_cached_time() {
    return atomic_load_explicit(&g_current_time, memory_order_relaxed);
}

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

// Fast inline hash (Murmur3 mixer style)
// Faster than FNV1a on modern pipelines
static inline uint32_t hash_key(const char* key, size_t len) {
    uint32_t h = (uint32_t)len;
    size_t i   = 0;

    // Duff's device style unrolling for small keys
    // (Simple loop for portability/brevity, compiler vectorizes this well)
    for (; i < len; i++) {
        h ^= (uint64_t)key[i];
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }

    if (unlikely(h < HASH_MIN_VAL)) return h + HASH_MIN_VAL;
    return h;
}

static inline uint64_t pack_meta(uint32_t hash, uint32_t len) {
    return ((uint64_t)hash << 32) | (uint32_t)len;
}

static inline uint32_t meta_hash(uint64_t meta) {
    return (uint32_t)(meta >> 32);
}

static inline const void* value_ptr_from_entry(const cache_entry_t* entry) {
    return (const void*)(entry->data + entry->key_len + 1 + sizeof(cache_entry_t*));
}

static inline cache_entry_t* entry_from_value(const void* value_ptr) {
    if (unlikely(!value_ptr)) return NULL;
    cache_entry_t* const* back_ptr = (cache_entry_t* const*)((const char*)value_ptr - sizeof(cache_entry_t*));
    return *back_ptr;
}

static inline void entry_ref_inc(cache_entry_t* entry) {
    // Relaxed ordering is fine for incrementing; we only need release on decrement
    if (entry) atomic_fetch_add_explicit(&entry->ref_count, 1, memory_order_relaxed);
}

static inline void entry_ref_dec(cache_entry_t* entry) {
    if (!entry) return;
    // release ensures earlier writes (cache_set) are visible before free
    int old_val = atomic_fetch_sub_explicit(&entry->ref_count, 1, memory_order_acq_rel);
    if (unlikely(old_val == 1)) free(entry);
}

// --- Lookup ---

// Hot path: Check slot with exact metadata match
static inline int check_slot(cache_slot_t* slot, uint64_t target_meta, const char* key, size_t klen, bool* found) {
    // 1. Load metadata (cheap integer load)
    uint64_t meta = slot->metadata;
    uint32_t h    = meta_hash(meta);

    if (h == HASH_EMPTY) return 2;    // Stop
    if (h == HASH_DELETED) return 3;  // Continue/Record Tombstone

    // 2. Exact match check (One instruction)
    if (meta == target_meta) {
        // 3. Pointer chase only on metadata match
        cache_entry_t* entry = slot->entry;
        // 4. Memory compare
        if (likely(memcmp(entry->data, key, klen) == 0)) {
            *found = true;
            return 1;
        }
    }
    return 0;
}

static size_t find_slot(aligned_cache_shard_t* shard, uint32_t hash, const char* key, size_t klen, bool* found) {
    size_t mask            = shard->bucket_count - 1;
    size_t idx             = hash & mask;
    size_t first_tombstone = SIZE_MAX;
    uint64_t target        = pack_meta(hash, (uint32_t)klen);

    *found = false;

    // Unrolled probe loop
    // We check 4 slots at a time to maximize pipeline usage
    for (size_t probe = 0; probe <= shard->bucket_count; probe += 4) {
        // Prefetch 2 cache lines ahead (128 bytes)
        __builtin_prefetch(&shard->slots[(idx + 8) & mask], 0, 1);

        for (size_t i = 0; i < 4; i++) {
            size_t curr = (idx + i) & mask;
            int res     = check_slot(&shard->slots[curr], target, key, klen, found);

            if (res == 1) return curr;
            if (res == 2) return (first_tombstone != SIZE_MAX) ? first_tombstone : curr;
            if (res == 3 && first_tombstone == SIZE_MAX) first_tombstone = curr;
        }
        idx = (idx + 4) & mask;
    }

    // Fallback for remaining slots (rare)
    return first_tombstone != SIZE_MAX ? first_tombstone : idx;
}

// --- Eviction ---

static bool clock_evict(aligned_cache_shard_t* shard) {
    size_t mask  = shard->bucket_count - 1;
    size_t scans = 0;
    size_t limit = shard->bucket_count * 2;  // Prevent infinite loops

    while (scans++ < limit) {
        size_t idx        = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;

        uint64_t meta = shard->slots[idx].metadata;
        if (meta_hash(meta) < HASH_MIN_VAL) continue;

        cache_entry_t* entry = shard->slots[idx].entry;

        // Relaxed load is sufficient for heuristic
        uint8_t bit = atomic_load_explicit(&entry->clock_bit, memory_order_relaxed);

        if (bit == 0) {
            // Evict
            shard->slots[idx].metadata = pack_meta(HASH_DELETED, 0);
            shard->slots[idx].entry    = NULL;
            shard->size--;
            entry_ref_dec(entry);  // Release cache's reference
            return true;
        } else {
            // Give second chance
            atomic_store_explicit(&entry->clock_bit, 0, memory_order_relaxed);
        }
    }
    return false;
}

// --- API ---

cache_t* cache_create(size_t capacity, uint32_t default_ttl) {
    struct cache_s* c = calloc(1, sizeof(struct cache_s));
    if (!c) return NULL;

    if (capacity == 0) capacity = 1000;
    size_t shard_cap = capacity / CACHE_SHARD_COUNT;
    if (shard_cap < 1) shard_cap = 1;

    c->default_ttl = default_ttl ? default_ttl : CACHE_DEFAULT_TTL;

    for (size_t i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* s = &c->shards[i];
        s->capacity              = shard_cap;
        s->bucket_count          = next_power_of_2(shard_cap * INITIAL_BUCKET_MULTIPLIER);

        if (posix_memalign((void**)&s->slots, CACHE_LINE_SIZE, s->bucket_count * sizeof(cache_slot_t)) != 0) {
            free(c);
            return NULL;
        }
        memset(s->slots, 0, s->bucket_count * sizeof(cache_slot_t));
        fast_rwlock_init(&s->lock);
    }
    return (cache_t*)c;
}

void cache_destroy(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        if (cache->shards[i].slots) {
            for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
                if (meta_hash(cache->shards[i].slots[j].metadata) >= HASH_MIN_VAL) {
                    entry_ref_dec(cache->shards[i].slots[j].entry);
                }
            }
            free(cache->shards[i].slots);
        }
    }
    free(cache);
}

// New API: Takes key_len directly
const void* cache_get(cache_t* cache_ptr, const char* key, size_t key_len, size_t* out_len) {
    if (unlikely(!cache_ptr)) return NULL;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    uint32_t hash                = hash_key(key, key_len);
    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    fast_rwlock_rdlock(&shard->lock);

    bool found;
    size_t idx = find_slot(shard, hash, key, key_len, &found);

    if (!found) {
        fast_rwlock_unlock_rd(&shard->lock);
        return NULL;
    }

    cache_entry_t* entry = shard->slots[idx].entry;

    // Use cached time (avoids syscall)
    if (unlikely(get_cached_time() >= entry->expires_at)) {
        fast_rwlock_unlock_rd(&shard->lock);
        cache_invalidate(cache_ptr, key, key_len);
        return NULL;
    }

    // Silent Store Optimization:
    // Only write if 0. Reduces cache coherence traffic significantly.
    if (atomic_load_explicit(&entry->clock_bit, memory_order_relaxed) == 0) {
        atomic_store_explicit(&entry->clock_bit, 1, memory_order_relaxed);
    }

    entry_ref_inc(entry);
    if (out_len) *out_len = entry->value_len;

    fast_rwlock_unlock_rd(&shard->lock);
    return value_ptr_from_entry(entry);
}

bool cache_set(cache_t* cache_ptr, const char* key, size_t key_len, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr)) return false;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    uint32_t hash = hash_key(key, key_len);

    // Alloc
    size_t alloc_sz          = sizeof(cache_entry_t) + key_len + 1 + sizeof(cache_entry_t*) + value_len;
    cache_entry_t* new_entry = malloc(alloc_sz);
    if (!new_entry) return false;

    // Init
    atomic_init(&new_entry->ref_count, 1);
    atomic_init(&new_entry->clock_bit, 1);
    new_entry->expires_at = (uint32_t)(get_cached_time() + (ttl ? ttl : cache->default_ttl));
    new_entry->key_len    = (uint32_t)key_len;
    new_entry->value_len  = value_len;

    // Data Layout
    unsigned char* ptr = new_entry->data;
    memcpy(ptr, key, key_len);
    ptr[key_len]                          = '\0';
    *(cache_entry_t**)(ptr + key_len + 1) = new_entry;
    memcpy(ptr + key_len + 1 + sizeof(cache_entry_t*), value, value_len);

    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    fast_rwlock_wrlock(&shard->lock);

    if (shard->size >= shard->capacity) {
        clock_evict(shard);
    }

    bool found;
    size_t idx = find_slot(shard, hash, key, key_len, &found);

    if (found) {
        cache_entry_t* old      = shard->slots[idx].entry;
        shard->slots[idx].entry = new_entry;
        // Metadata stays same (Hash + KeyLen)
        entry_ref_dec(old);
    } else {
        shard->slots[idx].entry    = new_entry;
        shard->slots[idx].metadata = pack_meta(hash, (uint32_t)key_len);
        shard->size++;
    }

    fast_rwlock_unlock_wr(&shard->lock);
    return true;
}

void cache_invalidate(cache_t* cache_ptr, const char* key, size_t key_len) {
    struct cache_s* cache        = (struct cache_s*)cache_ptr;
    uint32_t hash                = hash_key(key, key_len);
    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    fast_rwlock_wrlock(&shard->lock);
    bool found;
    size_t idx = find_slot(shard, hash, key, key_len, &found);
    if (found) {
        cache_entry_t* entry       = shard->slots[idx].entry;
        shard->slots[idx].metadata = pack_meta(HASH_DELETED, 0);
        shard->slots[idx].entry    = NULL;
        shard->size--;
        entry_ref_dec(entry);
    }
    fast_rwlock_unlock_wr(&shard->lock);
}

void cache_release(const void* ptr) {
    if (ptr) entry_ref_dec(entry_from_value(ptr));
}

// Legacy wrapper
const void* cache_get_str(cache_t* c, const char* key, size_t* out_len) {
    return cache_get(c, key, strlen(key), out_len);
}

#include <sys/time.h>

static double get_current_time(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}

static void run_benchmarks(void) {
    printf("\n=================================\n");
    printf("  Performance Benchmarks (Zero-Copy)\n");
    printf("=================================\n");

    const size_t CACHE_SIZE = 100000;
    const size_t NUM_OPS    = 500000;
    const size_t VAL_SIZE   = 1024;  // 1KB

    cache_t* cache = cache_create(CACHE_SIZE, 3600);
    if (!cache) {
        printf("Skipping benchmarks due to cache creation failure.\n");
        return;
    }

    // 1. Sequential Write
    double start    = get_current_time();
    char* dummy_val = malloc(VAL_SIZE);
    memset(dummy_val, 'A', VAL_SIZE);
    dummy_val[VAL_SIZE - 1] = '\0';

    for (size_t i = 0; i < NUM_OPS; i++) {
        char key[32];
        int keylen = snprintf(key, sizeof(key), "key%zu", i % CACHE_SIZE);
        cache_set(cache, key, (size_t)keylen, dummy_val, VAL_SIZE, 0);
    }

    double end = get_current_time();
    printf("Writes: %.0f ops/sec\n", NUM_OPS / (end - start));

    // 2. Sequential Read (Zero Copy)
    start            = get_current_time();
    size_t hit_count = 0;
    for (size_t i = 0; i < NUM_OPS; i++) {
        char key[32];
        int keylen = snprintf(key, sizeof(key), "key%zu", i % CACHE_SIZE);

        size_t len;
        const void* ptr = cache_get(cache, key, (size_t)keylen, &len);
        if (ptr) {
            hit_count++;
            cache_release(ptr);
        }
    }
    end = get_current_time();
    printf("Reads:  %.0f ops/sec (Hits: %zu)\n", NUM_OPS / (end - start), hit_count);

    free(dummy_val);
    cache_destroy(cache);
}

int main() {
    run_benchmarks();
    return 0;
}