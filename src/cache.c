/**
 * @file cache.c
 * @brief Sharded open-addressing hash cache with CLOCK-based eviction.
 *
 * Optimization: structure-of-arrays slot layout with a compact 1-byte
 * tag array.  The original array-of-structures (AoS) layout stored
 * {uint64_t metadata; cache_entry_t* entry;} co-located, yielding
 * 4 slots per 64-byte cache line.  With 50 % load and 131 072 slots per
 * shard the hot metadata scan touched a 2 MB working set—well beyond the
 * 1 MB L2 cache—so every random probe was a cold DRAM miss.
 *
 * The fix splits the slot array into two parallel arrays:
 *
 *   uint8_t  tags[bucket_count]           — 1-byte tag per slot
 *   cache_entry_t* entries[bucket_count]  — pointer per slot (full metadata
 *                                           now lives only in the entry)
 *
 * A tag packs {EMPTY flag, DELETED flag, 6-bit hash fragment} into one byte,
 * giving 64 slots per cache line—a 16x improvement in the scan density.
 * On the probe fast path only the tag array is touched.  The entry pointer
 * is fetched only on a tag match, which happens in at most 1–2 probes under
 * the measured workload.
 *
 * The 64-bit packed-metadata word {hash:32, key_len:32} is still stored in
 * cache_entry_t (it already was: hash and key_len are entry fields), so the
 * full metadata comparison—needed to eliminate false positives before the
 * key memcmp—is performed once against the fetched entry, not in the loop.
 *
 * Probe-length instrumentation is compiled in when CACHE_PROBE_STATS is
 * defined; it is zero-overhead in release builds.
 */

#include "../include/cache.h"

#include "../include/align.h"
#include "../include/aligned_alloc.h"
#include "../include/spinlock.h"

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__linux__) && defined(MADV_HUGEPAGE)
#include <sys/mman.h>
#endif

/* ---------------------------------------------------------------- constants */

#define CACHE_LINE_SIZE           64         /* bytes per cache line */
#define INITIAL_BUCKET_MULTIPLIER 2          /* target 50 % load factor */
#define CACHE_FILE_MAGIC          0x45484346 /* ASCII "FCHE" in little-endian */
#define CACHE_FILE_VERSION        1

/*
 * Tag byte encoding (8 bits):
 *
 *   bit 7 (0x80) : TAG_EMPTY   — slot never written
 *   bit 6 (0x40) : TAG_DELETED — tombstone (slot was occupied, now free)
 *   bits 5-0     : 6-bit hash fragment for fast reject
 *
 * Invariant: a live slot has both control bits clear (tag & 0xC0 == 0).
 * Using bit 7 for EMPTY and bit 6 for DELETED means a single AND with 0xC0
 * distinguishes all three states with no branch.
 *
 * The 6-bit fragment is derived from the full 32-bit hash (bits 8-13, chosen
 * to avoid overlap with the bits used for bucket index selection).  Because
 * bucket_count is always a power of 2 and INITIAL_BUCKET_MULTIPLIER=2, the
 * bucket index uses at most log2(131072)=17 low bits.  Taking fragment bits
 * from bits 18-23 gives maximum independence from the index selection.
 */
#define TAG_EMPTY          0x80u /* bit 7: slot never used */
#define TAG_DELETED        0x40u /* bit 6: tombstone */
#define TAG_CONTROL_MASK   0xC0u /* bits 7:6 — any set means not live */
#define TAG_FRAGMENT_SHIFT 18u   /* start bit for the 6-bit fragment */
#define TAG_FRAGMENT_MASK  0x3Fu /* 6-bit fragment mask */

/** Branch-prediction hints. */
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

/* ---------------------------------------------------------------- probe-length instrumentation
 *
 * Enabled only when CACHE_PROBE_STATS is defined at compile time.
 * Uses thread-local counters to avoid atomic overhead on the hot path.
 * Call cache_probe_stats_dump() to print accumulated statistics.
 *
 * Zero overhead in release builds: every macro expands to nothing.
 */
#ifdef CACHE_PROBE_STATS
#define PROBE_STAT_BUCKETS 32u

_Thread_local static uint64_t tl_probe_hist[PROBE_STAT_BUCKETS];
_Thread_local static uint64_t tl_probe_total_ops;

static _Atomic uint64_t g_probe_hist[PROBE_STAT_BUCKETS];
static _Atomic uint64_t g_probe_total_ops;

/** Records a single probe-chain length in the thread-local histogram. */
static inline void probe_stat_record(size_t probes) {
    if (unlikely(probes == 0)) return;
    size_t bucket = (probes - 1) < PROBE_STAT_BUCKETS ? (probes - 1) : PROBE_STAT_BUCKETS - 1;
    tl_probe_hist[bucket]++;
    tl_probe_total_ops++;
}

/**
 * Flushes thread-local probe stats into the global atomic counters.
 * Call at thread exit or whenever you want to collect cross-thread stats.
 */
void cache_probe_stats_flush(void) {
    for (size_t i = 0; i < PROBE_STAT_BUCKETS; i++) {
        atomic_fetch_add_explicit(&g_probe_hist[i], tl_probe_hist[i], memory_order_relaxed);
        tl_probe_hist[i] = 0;
    }
    atomic_fetch_add_explicit(&g_probe_total_ops, tl_probe_total_ops, memory_order_relaxed);
    tl_probe_total_ops = 0;
}

/** Prints the global probe-length histogram to stderr. */
void cache_probe_stats_dump(void) {
    cache_probe_stats_flush();
    uint64_t total = atomic_load_explicit(&g_probe_total_ops, memory_order_relaxed);
    fprintf(stderr, "=== find_slot probe-length distribution (%" PRIu64 " ops) ===\n", total);
    double avg = 0.0;
    for (size_t i = 0; i < PROBE_STAT_BUCKETS; i++) {
        uint64_t n = atomic_load_explicit(&g_probe_hist[i], memory_order_relaxed);
        if (n == 0) continue;
        avg += (double)(i + 1) * (double)n;
        fprintf(stderr, "  probes=%2zu : %8" PRIu64 " (%5.2f%%)\n", i + 1, n, 100.0 * (double)n / (double)total);
    }
    if (total) fprintf(stderr, "  avg=%.3f\n", avg / (double)total);
}

#define PROBE_INIT(var)   size_t var = 0
#define PROBE_INC(var)    (var)++
#define PROBE_RECORD(var) probe_stat_record(var)
#else /* !CACHE_PROBE_STATS */
#define PROBE_INIT(var)   (void)0
#define PROBE_INC(var)    (void)0
#define PROBE_RECORD(var) (void)0
#endif /* CACHE_PROBE_STATS */

/* ---------------------------------------------------------------- data structures */

/**
 * @brief A single cache entry: key, value, and metadata in one allocation.
 *
 * Memory layout:
 *   [cache_entry_t header][key_bytes]['\0'][back_pointer][value_bytes]
 *
 * The back_pointer (a cache_entry_t*) stored between key and value allows
 * cache_release() to recover the entry pointer from a raw value pointer
 * without any extra bookkeeping by the caller.
 *
 * Fields are ordered specifically to keep the hot hash and key_len at offset 0
 * for simpler assembly encoding (no base register displacement necessary).
 */
typedef struct ALIGN(CACHE_LINE_SIZE) {
    uint32_t hash;             /**< Full 32-bit FNV-1a hash of the key. */
    uint32_t key_len;          /**< Key length in bytes, excluding null terminator. */
    atomic_int ref_count;      /**< Reference count; freed when it reaches zero. */
    _Atomic uint8_t clock_bit; /**< CLOCK algorithm: 1 = recently used. */
    time_t expires_at;         /**< Absolute UNIX expiry timestamp. */
    size_t value_len;          /**< Value length in bytes. */
    /* Flexible array: [key]['\0'][back_ptr][value] */
#if defined(_MSC_VER) && !defined(__cplusplus)
    unsigned char data[1]; /* MSVC C-mode workaround */
#else
    unsigned char data[]; /* C99/C11 flexible array member */
#endif
} cache_entry_t;

/**
 * @brief Optimized structure-of-arrays slot representation.
 */
typedef struct ALIGN(CACHE_LINE_SIZE) {
    uint8_t* tags;           /**< 1-byte tag per slot; size == bucket_count. */
    cache_entry_t** entries; /**< Entry pointer per slot; size == bucket_count. */
    size_t bucket_count;     /**< Hash table size; always a power of 2. */
    size_t size;             /**< Number of live (non-tombstone) entries. */
    size_t capacity;         /**< Maximum live entries before eviction triggers. */
    size_t tombstone_count;  /**< Number of TAG_DELETED slots. */
    size_t clock_hand;       /**< CLOCK eviction scan position. */
    fast_rwlock_t lock;      /**< Per-shard reader-writer spinlock. */
} aligned_cache_shard_t;

_Static_assert(sizeof(aligned_cache_shard_t) <= 2 * CACHE_LINE_SIZE,
               "Shard struct too large; consider padding or splitting fields");

/** Top-level cache object. */
struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT]; /**< Fixed array of independent shards. */
    uint32_t default_ttl;                            /**< Default TTL in seconds when ttl=0 is passed to cache_set(). */
};

/* ---------------------------------------------------------------- utility functions */

/**
 * Rounds n up to the next power of 2.
 */
static inline size_t next_power_of_2(size_t n) {
    if (n && !(n & (n - 1))) return n;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32; /* Handle 64-bit size constraints */
    return n + 1;
}

/**
 * FNV-1a hash for string keys.
 */
static inline uint32_t hash_key(const char* key, size_t len) {
    uint32_t hash = 2166136261u; /* FNV offset basis */
    const unsigned char* p = (const unsigned char*)key;
    const unsigned char* end = p + len;
    while (p < end) {
        hash ^= *p++;
        hash *= 16777619u; /* FNV prime */
    }
    /* Reserve 0 and 1 as historical control values */
    if (unlikely(hash < 2u)) return hash + 2u;
    return hash;
}

/**
 * Mixes high bits of the hash into low bits to flatten shard distribution.
 */
static inline size_t get_shard_idx(uint32_t hash) {
    hash ^= hash >> 16;
    return hash & (CACHE_SHARD_COUNT - 1);
}

/**
 * Computes the 1-byte tag for a given hash.
 */
static inline uint8_t make_tag(uint32_t hash) {
    return (uint8_t)((hash >> TAG_FRAGMENT_SHIFT) & TAG_FRAGMENT_MASK);
}

/**
 * Returns the value pointer from a cache entry.
 */
static inline const void* value_ptr_from_entry(const cache_entry_t* entry) {
    return (const void*)(entry->data + entry->key_len + 1 + sizeof(cache_entry_t*));
}

/**
 * Recovers the cache_entry_t pointer from a caller-held value pointer.
 */
static inline cache_entry_t* entry_from_value(const void* value_ptr) {
    if (unlikely(!value_ptr)) return NULL;
    cache_entry_t* const* back_ptr = (cache_entry_t* const*)((const char*)value_ptr - sizeof(cache_entry_t*));
    return *back_ptr;
}

/**
 * Increments the entry reference count.
 */
static inline void entry_ref_inc(cache_entry_t* entry) {
    if (entry) atomic_fetch_add_explicit(&entry->ref_count, 1, memory_order_relaxed);
}

/**
 * Decrements the entry reference count and frees the entry if it reaches zero.
 */
static inline void entry_ref_dec(cache_entry_t* entry) {
    if (!entry) return;
    int prev = atomic_fetch_sub_explicit(&entry->ref_count, 1, memory_order_acq_rel);
    if (unlikely(prev == 1)) free(entry);
}

/**
 * Highly optimized key comparison utilizing fixed-size copies.
 * Modern compilers optimize constant-size memcpy blocks into straight register moves.
 */
static inline bool keys_equal(const void* k1, const void* k2, size_t len) {
    if (len == 8) {
        uint64_t u1, u2;
        memcpy(&u1, k1, 8);
        memcpy(&u2, k2, 8);
        return u1 == u2;
    }
    if (len == 4) {
        uint32_t u1, u2;
        memcpy(&u1, k1, 4);
        memcpy(&u2, k2, 4);
        return u1 == u2;
    }
    if (len == 16) {
        uint64_t u1[2], u2[2];
        memcpy(u1, k1, 16);
        memcpy(u2, k2, 16);
        return u1[0] == u2[0] && u1[1] == u2[1];
    }
    if (len == 2) {
        uint16_t u1, u2;
        memcpy(&u1, k1, 2);
        memcpy(&u2, k2, 2);
        return u1 == u2;
    }
    if (len == 1) { return *(const uint8_t*)k1 == *(const uint8_t*)k2; }
    return memcmp(k1, k2, len) == 0;
}

/* ---------------------------------------------------------------- find_slot (optimised) */

/**
 * Locates a slot in the hash table using the compact tag array.
 */
static size_t find_slot(aligned_cache_shard_t* shard, uint32_t hash, const char* key, size_t klen, uint8_t target_tag,
                        bool* found) {
    size_t mask = shard->bucket_count - 1;
    size_t idx = hash & mask; /* initial probe position */
    size_t first_tombstone = SIZE_MAX;
    uint8_t* tags = shard->tags;
    cache_entry_t** entries = shard->entries;

    *found = false;

    PROBE_INIT(probes);

    for (size_t probe_count = 0; probe_count < shard->bucket_count; probe_count++) {
        uint8_t tag = tags[idx];
        PROBE_INC(probes);

        /*
         * Since target_tag is built by masking with 0x3F, it can never have bits 6 or 7 set.
         * Therefore, any tag exactly matching target_tag is guaranteed to be a live entry.
         */
        if (likely(tag == target_tag)) {
            cache_entry_t* entry = entries[idx];

            /* Pack hash:32 and key_len:32 into a single 64-bit load/comparison */
            uint64_t entry_meta;
            memcpy(&entry_meta, &entry->hash, 8);
            uint64_t target_meta = ((uint64_t)klen << 32) | hash;

            if (likely(entry_meta == target_meta) && keys_equal(entry->data, key, klen)) {
                *found = true;
                PROBE_RECORD(probes);
                return idx;
            }
        } else if (tag & TAG_EMPTY) {
            /* Empty slot terminates the probe chain */
            PROBE_RECORD(probes);
            return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
        } else if (tag & TAG_DELETED) {
            /* Tombstone: keep probing, record for potential reuse. */
            if (first_tombstone == SIZE_MAX) first_tombstone = idx;
        }

        idx = (idx + 1) & mask; /* advance: linear probing with power-of-2 wrap */
    }

    /* Table fully probed (should not happen under normal operation). */
    PROBE_RECORD(probes);
    return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
}

/* ---------------------------------------------------------------- compact_shard */

/**
 * Re-hashes all live entries into a fresh pair of tag/entry arrays to
 * eliminate tombstones and restore probe-chain integrity.
 */
static void compact_shard(aligned_cache_shard_t* shard) {
    size_t n = shard->bucket_count;

    uint8_t* new_tags = ALIGNED_ALLOC(CACHE_LINE_SIZE, n * sizeof(uint8_t));
    if (!new_tags) return;

    cache_entry_t** new_entries = ALIGNED_ALLOC(CACHE_LINE_SIZE, n * sizeof(cache_entry_t*));
    if (!new_entries) {
        free(new_tags);
        return;
    }

#if defined(__linux__) && defined(MADV_HUGEPAGE)
    madvise(new_tags, n * sizeof(uint8_t), MADV_HUGEPAGE);
    madvise(new_entries, n * sizeof(cache_entry_t*), MADV_HUGEPAGE);
#endif

    /* Initialize all slots to EMPTY. */
    memset(new_tags, TAG_EMPTY, n * sizeof(uint8_t));
    memset(new_entries, 0, n * sizeof(cache_entry_t*));

    size_t mask = n - 1;
    uint8_t* old_tags = shard->tags;
    cache_entry_t** old_entries = shard->entries;

    for (size_t i = 0; i < n; i++) {
        uint8_t tag = old_tags[i];
        if (tag & TAG_CONTROL_MASK) continue;

        cache_entry_t* entry = old_entries[i];
        uint32_t h = entry->hash;
        uint8_t ntag = make_tag(h);
        size_t idx = h & mask;

        while (!(new_tags[idx] & TAG_EMPTY)) {
            idx = (idx + 1) & mask;
        }
        new_tags[idx] = ntag;
        new_entries[idx] = entry;
    }

    shard->tags = new_tags;
    shard->entries = new_entries;
    shard->tombstone_count = 0;
    shard->clock_hand = 0;

    free(old_tags);
    free(old_entries);
}

/* ---------------------------------------------------------------- clock_evict */

/**
 * Evicts one entry using the CLOCK algorithm (approximate LRU).
 */
static bool clock_evict(aligned_cache_shard_t* shard) {
    size_t scanned = 0;
    size_t mask = shard->bucket_count - 1;
    uint8_t* tags = shard->tags;
    cache_entry_t** entries = shard->entries;

    while (scanned < shard->bucket_count) {
        size_t idx = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;

        uint8_t tag = tags[idx];
        if (tag & TAG_CONTROL_MASK) {
            scanned++;
            continue;
        }

        cache_entry_t* entry = entries[idx];
        uint8_t bit = atomic_load_explicit(&entry->clock_bit, memory_order_relaxed);

        if (bit == 0) {
            tags[idx] = TAG_DELETED;
            entries[idx] = NULL;
            shard->size--;
            shard->tombstone_count++;
            entry_ref_dec(entry);
            return true;
        }

        atomic_store_explicit(&entry->clock_bit, 0, memory_order_relaxed);
        scanned++;
    }

    for (size_t i = 0; i < shard->bucket_count; i++) {
        size_t idx = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;
        if (!(tags[idx] & TAG_CONTROL_MASK)) {
            cache_entry_t* entry = entries[idx];
            tags[idx] = TAG_DELETED;
            entries[idx] = NULL;
            shard->size--;
            shard->tombstone_count++;
            entry_ref_dec(entry);
            return true;
        }
    }
    return false;
}

/* ---------------------------------------------------------------- public API */

/**
 * Creates a new cache with the specified capacity and default TTL.
 */
cache_t* cache_create(size_t capacity, uint32_t default_ttl) {
    struct cache_s* c = calloc(1, sizeof(struct cache_s));
    if (!c) return NULL;

    if (capacity == 0) capacity = 1000;
    size_t shard_cap = capacity / CACHE_SHARD_COUNT;
    if (shard_cap < 1) shard_cap = 1;

    c->default_ttl = default_ttl ? default_ttl : CACHE_DEFAULT_TTL;

    for (size_t i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* s = &c->shards[i];
        s->capacity = shard_cap;

        size_t desired = (size_t)(shard_cap * INITIAL_BUCKET_MULTIPLIER);
        s->bucket_count = next_power_of_2(desired);

        s->tags = ALIGNED_ALLOC(CACHE_LINE_SIZE, s->bucket_count * sizeof(uint8_t));
        if (!s->tags) goto cleanup_error;

        s->entries = ALIGNED_ALLOC(CACHE_LINE_SIZE, s->bucket_count * sizeof(cache_entry_t*));
        if (!s->entries) {
            free(s->tags);
            s->tags = NULL;
            goto cleanup_error;
        }

#if defined(__linux__) && defined(MADV_HUGEPAGE)
        madvise(s->tags, s->bucket_count * sizeof(uint8_t), MADV_HUGEPAGE);
        madvise(s->entries, s->bucket_count * sizeof(cache_entry_t*), MADV_HUGEPAGE);
#endif
        memset(s->tags, TAG_EMPTY, s->bucket_count * sizeof(uint8_t));
        memset(s->entries, 0, s->bucket_count * sizeof(cache_entry_t*));

        fast_rwlock_init(&s->lock);
    }

    return (cache_t*)c;

cleanup_error:
    for (size_t j = 0; j < CACHE_SHARD_COUNT; j++) {
        free(c->shards[j].tags);
        free(c->shards[j].entries);
    }
    free(c);
    return NULL;
}

/**
 * Destroys the cache and frees all associated memory.
 */
void cache_destroy(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* s = &cache->shards[i];
        fast_rwlock_wrlock(&s->lock);
        if (s->tags && s->entries) {
            for (size_t j = 0; j < s->bucket_count; j++) {
                if (!(s->tags[j] & TAG_CONTROL_MASK)) { entry_ref_dec(s->entries[j]); }
            }
        }
        free(s->tags);
        free(s->entries);
        s->tags = NULL;
        s->entries = NULL;
        fast_rwlock_unlock_wr(&s->lock);
    }
    free(cache);
}

/**
 * Retrieves a value from the cache by key (zero-copy).
 */
const void* cache_get(cache_t* cache_ptr, const char* key, size_t klen, size_t* out_len) {
    if (unlikely(!cache_ptr || !key || !klen)) return NULL;

    struct cache_s* cache = (struct cache_s*)cache_ptr;
    uint32_t hash = hash_key(key, klen);
    uint8_t target_tag = make_tag(hash);
    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];

    fast_rwlock_rdlock(&shard->lock);

    bool found;
    size_t found_idx = find_slot(shard, hash, key, klen, target_tag, &found);
    if (!found) {
        fast_rwlock_unlock_rd(&shard->lock);
        return NULL;
    }

    cache_entry_t* entry = shard->entries[found_idx];
    time_t now = time(NULL);

    if (unlikely(now >= entry->expires_at)) {
        fast_rwlock_unlock_rd(&shard->lock);

        /* Upgrade to write lock for removal with double-check logic */
        fast_rwlock_wrlock(&shard->lock);
        bool re_found;
        size_t re_idx = find_slot(shard, hash, key, klen, target_tag, &re_found);
        if (re_found) {
            cache_entry_t* re_entry = shard->entries[re_idx];
            if (now >= re_entry->expires_at) {
                shard->tags[re_idx] = TAG_DELETED;
                shard->entries[re_idx] = NULL;
                shard->size--;
                shard->tombstone_count++;
                entry_ref_dec(re_entry);
            }
        }
        fast_rwlock_unlock_wr(&shard->lock);
        return NULL;
    }

    /* Silent-store optimisation: write the clock bit only if it is clear */
    if (atomic_load_explicit(&entry->clock_bit, memory_order_relaxed) == 0) {
        atomic_store_explicit(&entry->clock_bit, 1, memory_order_relaxed);
    }

    entry_ref_inc(entry);

    if (out_len) *out_len = entry->value_len;

    fast_rwlock_unlock_rd(&shard->lock);
    return value_ptr_from_entry(entry);
}

/**
 * Inserts or updates a key-value pair in the cache.
 */
bool cache_set(cache_t* cache_ptr, const char* key, size_t klen, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr || !key || !klen || !value || !value_len)) return false;

    struct cache_s* cache = (struct cache_s*)cache_ptr;
    uint32_t hash = hash_key(key, klen);
    uint8_t target_tag = make_tag(hash);
    time_t now = time(NULL);

    /*
     * Reverted to fast malloc() to avoid aligned_alloc() overhead in glibc.
     * With hash and key_len moved to offset 0, the hot metadata is guaranteed 
     * to align to a 16-byte boundary and cannot span across a cache-line split.
     */
    size_t alloc_sz = offsetof(cache_entry_t, data) + klen + 1 + sizeof(void*) + value_len;
    cache_entry_t* new_entry = malloc(alloc_sz);
    if (!new_entry) return false;

    new_entry->hash = hash;
    new_entry->key_len = (uint32_t)klen;
    new_entry->value_len = value_len;
    new_entry->expires_at = now + (time_t)(ttl ? ttl : cache->default_ttl);
    atomic_init(&new_entry->ref_count, 1);
    atomic_init(&new_entry->clock_bit, 1);

    unsigned char* dp = new_entry->data;
    memcpy(dp, key, klen);
    dp[klen] = '\0';
    *(cache_entry_t**)(dp + klen + 1) = new_entry;
    memcpy(dp + klen + 1 + sizeof(cache_entry_t*), value, value_len);

    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];

    /* Speculatively prefetch the entry pointer slot before acquiring the lock */
    size_t mask = shard->bucket_count - 1;
    size_t idx = hash & mask;
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(&shard->entries[idx], 0, 3);
#endif

    fast_rwlock_wrlock(&shard->lock);

    if (unlikely(shard->tombstone_count > (shard->bucket_count * 3) / 4)) { compact_shard(shard); }

    bool found;
    size_t found_idx = find_slot(shard, hash, key, klen, target_tag, &found);

    if (found) {
        cache_entry_t* old = shard->entries[found_idx];
        shard->entries[found_idx] = new_entry;
        entry_ref_dec(old);
    } else {
        if (shard->size >= shard->capacity) {
            if (!clock_evict(shard)) {
                fast_rwlock_unlock_wr(&shard->lock);
                free(new_entry);
                return false;
            }
        }

        if (shard->tags[found_idx] == TAG_DELETED) shard->tombstone_count--;

        shard->tags[found_idx] = target_tag;
        shard->entries[found_idx] = new_entry;
        shard->size++;
    }

    fast_rwlock_unlock_wr(&shard->lock);
    return true;
}

/**
 * Removes a key from the cache.
 */
void cache_invalidate(cache_t* cache_ptr, const char* key) {
    if (!cache_ptr || !key) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t klen = strlen(key);
    uint32_t hash = hash_key(key, klen);
    uint8_t target_tag = make_tag(hash);
    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];

    /* Speculatively prefetch the entry pointer slot before acquiring the lock */
    size_t mask = shard->bucket_count - 1;
    size_t idx = hash & mask;
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(&shard->entries[idx], 0, 3);
#endif

    fast_rwlock_wrlock(&shard->lock);

    bool found;
    size_t found_idx = find_slot(shard, hash, key, klen, target_tag, &found);

    if (found) {
        cache_entry_t* entry = shard->entries[found_idx];
        shard->tags[found_idx] = TAG_DELETED;
        shard->entries[found_idx] = NULL;
        shard->size--;
        shard->tombstone_count++;

        if (unlikely(shard->tombstone_count > (shard->bucket_count * 3) / 4)) { compact_shard(shard); }
        entry_ref_dec(entry);
    }

    fast_rwlock_unlock_wr(&shard->lock);
}

/**
 * Releases a reference to a cached value obtained via cache_get().
 */
void cache_release(const void* ptr) {
    if (!ptr) return;
    entry_ref_dec(entry_from_value(ptr));
}

/**
 * Removes all entries from all shards.
 */
void cache_clear(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* s = &cache->shards[i];
        fast_rwlock_wrlock(&s->lock);
        for (size_t j = 0; j < s->bucket_count; j++) {
            if (!(s->tags[j] & TAG_CONTROL_MASK)) { entry_ref_dec(s->entries[j]); }
        }
        memset(s->tags, TAG_EMPTY, s->bucket_count * sizeof(uint8_t));
        memset(s->entries, 0, s->bucket_count * sizeof(cache_entry_t*));
        s->size = 0;
        s->tombstone_count = 0;
        s->clock_hand = 0;
        fast_rwlock_unlock_wr(&s->lock);
    }
}

/**
 * Returns the total number of live entries across all shards.
 */
size_t get_total_cache_size(cache_t* cache_ptr) {
    if (!cache_ptr) return 0;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        fast_rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].size;
        fast_rwlock_unlock_rd(&cache->shards[i].lock);
    }
    return total;
}

/**
 * Returns the total capacity of the cache across all shards.
 */
size_t get_total_capacity(cache_t* cache_ptr) {
    if (!cache_ptr) return 0;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        fast_rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].capacity;
        fast_rwlock_unlock_rd(&cache->shards[i].lock);
    }
    return total;
}

/* ---------------------------------------------------------------- persistence */

static inline bool file_write_chk(const void* ptr, size_t size, size_t count, FILE* stream) {
    return fwrite(ptr, size, count, stream) == count;
}

static inline bool file_read_chk(void* ptr, size_t size, size_t count, FILE* stream) {
    return fread(ptr, size, count, stream) == count;
}

/**
 * Serialises all non-expired entries to a binary file.
 */
bool cache_save(cache_t* cache_ptr, const char* filename) {
    if (!cache_ptr || !filename) return false;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    uint32_t magic = CACHE_FILE_MAGIC;
    uint32_t version = CACHE_FILE_VERSION;
    uint64_t total_entries = 0;

    if (!file_write_chk(&magic, sizeof(magic), 1, f) || !file_write_chk(&version, sizeof(version), 1, f) ||
        !file_write_chk(&total_entries, sizeof(total_entries), 1, f)) {
        fclose(f);
        return false;
    }

    uint64_t actual_count = 0;
    time_t now = time(NULL);

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* shard = &cache->shards[i];
        fast_rwlock_rdlock(&shard->lock);

        for (size_t j = 0; j < shard->bucket_count; j++) {
            if (shard->tags[j] & TAG_CONTROL_MASK) continue;

            cache_entry_t* entry = shard->entries[j];
            if (!entry || entry->expires_at <= now) continue;

            uint32_t klen = entry->key_len;
            uint64_t vlen = (uint64_t)entry->value_len;
            int64_t expiry = (int64_t)entry->expires_at;

            const void* val_ptr = value_ptr_from_entry(entry);
            if (!val_ptr) continue;

            if (!file_write_chk(&klen, sizeof(klen), 1, f) || !file_write_chk(&vlen, sizeof(vlen), 1, f) ||
                !file_write_chk(&expiry, sizeof(expiry), 1, f) || !file_write_chk(entry->data, 1, klen, f) ||
                !file_write_chk(val_ptr, 1, (size_t)vlen, f)) {
                fast_rwlock_unlock_rd(&shard->lock);
                fclose(f);
                return false;
            }
            actual_count++;
        }

        fast_rwlock_unlock_rd(&shard->lock);
    }

    fseek(f, (long)(sizeof(magic) + sizeof(version)), SEEK_SET);
    if (!file_write_chk(&actual_count, sizeof(actual_count), 1, f)) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

/**
 * Loads entries from a binary file previously written by cache_save().
 */
bool cache_load(cache_t* cache_ptr, const char* filename) {
    if (!cache_ptr || !filename) return false;

    FILE* f = fopen(filename, "rb");
    if (!f) return false;

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t stored_count = 0;

    if (!file_read_chk(&magic, sizeof(magic), 1, f) || !file_read_chk(&version, sizeof(version), 1, f) ||
        !file_read_chk(&stored_count, sizeof(stored_count), 1, f)) {
        fclose(f);
        return false;
    }

    if (magic != CACHE_FILE_MAGIC || version != CACHE_FILE_VERSION) {
        fclose(f);
        return false;
    }

    char key_buf[512];
    char* big_key_buf = NULL;
    void* val_buf = NULL;
    size_t val_buf_cap = 0;
    time_t now = time(NULL);
    bool success = true;

    for (uint64_t i = 0; i < stored_count; i++) {
        uint32_t klen;
        uint64_t vlen;
        int64_t expiry;

        if (!file_read_chk(&klen, sizeof(klen), 1, f) || !file_read_chk(&vlen, sizeof(vlen), 1, f) ||
            !file_read_chk(&expiry, sizeof(expiry), 1, f)) {
            success = false;
            break;
        }

        char* kptr = key_buf;
        if (klen >= sizeof(key_buf)) {
            big_key_buf = realloc(big_key_buf, klen + 1);
            if (!big_key_buf) {
                success = false;
                break;
            }
            kptr = big_key_buf;
        }

        if (vlen > val_buf_cap) {
            void* tmp = realloc(val_buf, vlen);
            if (!tmp) {
                success = false;
                break;
            }
            val_buf = tmp;
            val_buf_cap = vlen;
        }

        if (!file_read_chk(kptr, 1, klen, f) || !file_read_chk(val_buf, 1, (size_t)vlen, f)) {
            success = false;
            break;
        }

        kptr[klen] = '\0';

        if ((time_t)expiry > now) {
            uint32_t remaining_ttl = (uint32_t)((time_t)expiry - now);
            if (!cache_set(cache_ptr, kptr, klen, val_buf, (size_t)vlen, remaining_ttl)) {
                success = false;
                break;
            }
        }
    }

    free(big_key_buf);
    free(val_buf);
    fclose(f);
    return success;
}
