/**
 * @file cache.c
 * @brief Sharded open-addressing hash cache with embedded 16-bit tags, inline entry
 * storage, a seqlock-based lock-free read path, single-CAS write path, and a coarse shared clock.
 */

#include "cache.h"

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__linux__) && defined(MADV_HUGEPAGE)
#include <sys/mman.h>
#endif

/* ---------------------------------------------------------------- Constants */

#define CACHE_LINE_SIZE           64         /**< Cache line boundary in bytes. */
#define INITIAL_BUCKET_MULTIPLIER 2          /**< Target 50% load factor baseline. */
#define CACHE_FILE_MAGIC          0x45484346 /**< ASCII "FCHE" in little-endian. */
#define CACHE_FILE_VERSION        3          /**< Lock-free layout version. */

/*
 * Embedded 16-bit Tag Encoding:
 *   bit 15 (0x8000) : TAG_EMPTY    — slot never written
 *   bit 14 (0x4000) : TAG_DELETED  — tombstone (slot was occupied, now free)
 *   bit 13 (0x2000) : TAG_BUSY     — slot currently locked/claimed by a writer
 *   bits 12-0       : 13-bit hash fragment for fast reject
 */
#define TAG_EMPTY          0x8000u
#define TAG_DELETED        0x4000u
#define TAG_BUSY           0x2000u
#define TAG_CONTROL_MASK   0xE000u
#define TAG_FRAGMENT_SHIFT 16u
#define TAG_FRAGMENT_MASK  0x1FFFu

#define INLINE_KEY_MAX     56   /**< Max key bytes inline, excluding null terminator. */
#define INLINE_VALUE_MAX   192  /**< Max value bytes inline. */
#define OVERFLOW_SLAB_SIZE 2048 /**< Fixed overflow block size for slab allocator. */
#define SLAB_NIL           0xFFFFFFFFu

#define CACHE_COARSE_TIME_MAX_STALENESS_SEC 1

/** Branch prediction macros. */
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

/* ---------------------------------------------------------------- Coarse Shared Clock */

static _Atomic time_t g_coarse_now;
static _Atomic time_t g_last_real_time_check;

/** Advances coarse cached timestamp. Safe for concurrent use across threads. */
void cache_tick(void) {
    time_t now = time(NULL);
    atomic_store_explicit(&g_coarse_now, now, memory_order_relaxed);
    atomic_store_explicit(&g_last_real_time_check, now, memory_order_relaxed);
}

/** Returns coarse timestamp, lazily refreshing if stale beyond boundary threshold. */
static inline time_t coarse_now(void) {
    time_t cached = atomic_load_explicit(&g_coarse_now, memory_order_relaxed);
    if (unlikely(cached == 0)) {
        cache_tick();
        return atomic_load_explicit(&g_coarse_now, memory_order_relaxed);
    }
    return cached;
}

/* ---------------------------------------------------------------- Probe Instrumentation */

#ifdef CACHE_PROBE_STATS
#define PROBE_STAT_BUCKETS 32u

_Thread_local static uint64_t tl_probe_hist[PROBE_STAT_BUCKETS];
_Thread_local static uint64_t tl_probe_total_ops;

static _Atomic uint64_t g_probe_hist[PROBE_STAT_BUCKETS];
static _Atomic uint64_t g_probe_total_ops;

static inline void probe_stat_record(size_t probes) {
    if (unlikely(probes == 0)) return;
    size_t bucket = (probes - 1) < PROBE_STAT_BUCKETS ? (probes - 1) : PROBE_STAT_BUCKETS - 1;
    tl_probe_hist[bucket]++;
    tl_probe_total_ops++;
}

void cache_probe_stats_flush(void) {
    for (size_t i = 0; i < PROBE_STAT_BUCKETS; i++) {
        atomic_fetch_add_explicit(&g_probe_hist[i], tl_probe_hist[i], memory_order_relaxed);
        tl_probe_hist[i] = 0;
    }
    atomic_fetch_add_explicit(&g_probe_total_ops, tl_probe_total_ops, memory_order_relaxed);
    tl_probe_total_ops = 0;
}

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
#else
#define PROBE_INIT(var)   (void)0
#define PROBE_INC(var)    (void)0
#define PROBE_RECORD(var) (void)0
#endif

/* ---------------------------------------------------------------- Data Structures */

/** Lock-free Treiber stack freelist node for overflow blocks. */
typedef struct {
    _Atomic uint32_t next; /**< Index of next block in freelist. */
} slab_node_t;

/** ABA-safe array-backed lock-free slab pool. */
typedef struct {
    _Atomic uint64_t head; /**< Packed 32-bit generation counter + 32-bit index. */
    void* buffer;          /**< Base memory block backing pool. */
    slab_node_t* nodes;    /**< Freelist node descriptors. */
    size_t block_size;     /**< Fixed byte size per overflow block. */
    size_t block_count;    /**< Total blocks allocated in pool. */
} lockfree_slab_t;

/**
 * Inline slot record.
 * Contains embedded tag to prevent cross-slot false sharing.
 */
typedef struct {
    _Alignas(CACHE_LINE_SIZE) _Atomic uint32_t seq;   /**< Seqlock counter: ODD while writing. */
    _Atomic uint16_t tag;                             /**< Tag embedded in slot (prevents false sharing). */
    uint16_t reserved;                                /**< Explicit padding field. */
    uint32_t hash;                                    /**< Full 32-bit hash. */
    uint32_t key_len;                                 /**< Key length in bytes. */
    uint32_t value_len;                               /**< Value length in bytes. */
    time_t expires_at;                                /**< Absolute expiry timestamp. */
    _Atomic uint8_t clock_bit;                        /**< CLOCK algorithm bit. */
    bool overflow;                                    /**< True if value is heap/slab backed. */
    char key[INLINE_KEY_MAX + 1];                     /**< Null-terminated key inline. */
    union {
        unsigned char inline_value[INLINE_VALUE_MAX]; /**< Inline storage when !overflow. */
        void* overflow_ptr;                           /**< Overflow pointer when overflow. */
    } value;
} cache_slot_t;

/** Lock-free cache shard. */
typedef struct {
    _Alignas(CACHE_LINE_SIZE) cache_slot_t* slots; /**< Direct array of slots. */
    size_t bucket_count;                           /**< Hash table capacity (power of 2). */
    _Atomic size_t size;                           /**< Live entry count. */
    size_t capacity;                               /**< Live entries limit before eviction. */
    _Atomic size_t tombstone_count;                /**< Active tombstone count. */
    _Atomic size_t clock_hand;                     /**< CLOCK eviction scan hand index. */
    lockfree_slab_t slab;                          /**< Per-shard lock-free slab allocator. */
} aligned_cache_shard_t;

struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT];
    uint32_t default_ttl;
};

/* ---------------------------------------------------------------- Lock-Free Slab Helpers */

#define SLAB_PACK(gen, idx) (((uint64_t)(gen) << 32) | (uint64_t)(idx))
#define SLAB_GEN(head)      ((uint32_t)((head) >> 32))
#define SLAB_IDX(head)      ((uint32_t)((head) & 0xFFFFFFFFu))

/** Initializes array-backed lock-free slab pool. */
static bool slab_init(lockfree_slab_t* slab, size_t block_count, size_t block_size) {
    slab->block_count = block_count;
    slab->block_size = block_size;

    slab->buffer = calloc(block_count, block_size);
    if (!slab->buffer) return false;

    slab->nodes = calloc(block_count, sizeof(slab_node_t));
    if (!slab->nodes) {
        free(slab->buffer);
        slab->buffer = NULL;
        return false;
    }

    for (size_t i = 0; i < block_count; i++) {
        uint32_t next = (i + 1 < block_count) ? (uint32_t)(i + 1) : SLAB_NIL;
        atomic_store_explicit(&slab->nodes[i].next, next, memory_order_relaxed);
    }

    atomic_store_explicit(&slab->head, SLAB_PACK(0, 0), memory_order_relaxed);
    return true;
}

/** Destroys lock-free slab pool. */
static void slab_destroy(lockfree_slab_t* slab) {
    if (!slab) return;
    free(slab->buffer);
    free(slab->nodes);
    slab->buffer = NULL;
    slab->nodes = NULL;
}

/** Allocates a block from the lock-free slab pool using ABA-safe CAS. */
static void* slab_alloc(lockfree_slab_t* slab) {
    uint64_t head = atomic_load_explicit(&slab->head, memory_order_acquire);
    for (;;) {
        uint32_t idx = SLAB_IDX(head);
        uint32_t gen = SLAB_GEN(head);

        if (idx == SLAB_NIL) return NULL;

        uint32_t next = atomic_load_explicit(&slab->nodes[idx].next, memory_order_relaxed);
        uint64_t new_head = SLAB_PACK(gen + 1, next);

        if (atomic_compare_exchange_weak_explicit(&slab->head, &head, new_head, memory_order_release,
                                                  memory_order_acquire)) {
            return (void*)((uintptr_t)slab->buffer + (idx * slab->block_size));
        }
    }
}

/** Returns a block to the lock-free slab pool. */
static void slab_free(lockfree_slab_t* slab, void* ptr) {
    if (!ptr || !slab->buffer) return;

    uintptr_t diff = (uintptr_t)ptr - (uintptr_t)slab->buffer;
    uint32_t idx = (uint32_t)(diff / slab->block_size);

    if (unlikely(idx >= slab->block_count)) {
        free(ptr); /* Fallback for general malloc allocations */
        return;
    }

    uint64_t head = atomic_load_explicit(&slab->head, memory_order_relaxed);
    for (;;) {
        uint32_t gen = SLAB_GEN(head);
        uint32_t old_idx = SLAB_IDX(head);

        atomic_store_explicit(&slab->nodes[idx].next, old_idx, memory_order_relaxed);
        uint64_t new_head = SLAB_PACK(gen + 1, idx);

        if (atomic_compare_exchange_weak_explicit(&slab->head, &head, new_head, memory_order_release,
                                                  memory_order_relaxed)) {
            return;
        }
    }
}

/* ---------------------------------------------------------------- Seqlock & Helper Functions */

static inline size_t next_power_of_2(size_t n) {
    if (n && !(n & (n - 1))) return n;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

static inline uint32_t hash_key(const char* key, size_t len) {
    uint32_t h = (uint32_t)len ^ 0x9e3779b9u;
    const uint8_t* p = (const uint8_t*)key;

    size_t i = 0;
    for (; i + 4 <= len; i += 4) {
        uint32_t k;
        memcpy(&k, p + i, 4);
        k *= 0xcc9e2d51u;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593u;

        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64u;
    }

    if (i < len) {
        uint32_t k = 0;
        size_t rem = len - i;
        if (rem == 3) k |= (uint32_t)p[i + 2] << 16;
        if (rem >= 2) k |= (uint32_t)p[i + 1] << 8;
        if (rem >= 1) k |= (uint32_t)p[i];

        k *= 0xcc9e2d51u;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593u;
        h ^= k;
    }

    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;

    if (unlikely(h < 2u)) return h + 2u;
    return h;
}

static inline size_t get_shard_idx(uint32_t hash) {
    hash ^= hash >> 16;
    return hash & (CACHE_SHARD_COUNT - 1);
}

static inline uint16_t make_tag(uint32_t hash) {
    return (uint16_t)((hash >> TAG_FRAGMENT_SHIFT) & TAG_FRAGMENT_MASK);
}

/** Attempts to acquire exclusive write lock on a single slot. */
static inline bool slot_try_lock(_Atomic uint32_t* seq_ptr, uint32_t* captured_seq) {
    uint32_t seq = atomic_load_explicit(seq_ptr, memory_order_relaxed);
    if (unlikely(seq & 1u)) return false;

    if (atomic_compare_exchange_weak_explicit(seq_ptr, &seq, seq + 1, memory_order_acquire, memory_order_relaxed)) {
        *captured_seq = seq;
        return true;
    }
    return false;
}

/** Releases exclusive write lock on a single slot. */
static inline void slot_unlock(_Atomic uint32_t* seq_ptr, uint32_t captured_seq) {
    atomic_store_explicit(seq_ptr, captured_seq + 2, memory_order_release);
}

/* ---------------------------------------------------------------- Lock-Free Clock Eviction */

/** Scans local probe sequence to evict an unreferenced slot without global locks. */
static size_t clock_evict_lockfree(aligned_cache_shard_t* shard, uint32_t hash) {
    size_t mask = shard->bucket_count - 1;
    size_t start_idx = hash & mask;

    for (size_t i = 0; i < 32u; i++) {
        size_t idx = (start_idx + i) & mask;
        cache_slot_t* slot = &shard->slots[idx];
        uint16_t tag = atomic_load_explicit(&slot->tag, memory_order_relaxed);

        if (tag & TAG_CONTROL_MASK) continue;

        uint8_t bit = atomic_load_explicit(&slot->clock_bit, memory_order_relaxed);
        if (bit == 1) {
            atomic_store_explicit(&slot->clock_bit, 0, memory_order_relaxed);
            continue;
        }

        uint32_t seq;
        if (slot_try_lock(&slot->seq, &seq)) {
            uint16_t cur_tag = atomic_load_explicit(&slot->tag, memory_order_relaxed);
            if (!(cur_tag & TAG_CONTROL_MASK)) {
                if (slot->overflow && slot->value.overflow_ptr) {
                    slab_free(&shard->slab, slot->value.overflow_ptr);
                    slot->value.overflow_ptr = NULL;
                }
                slot->overflow = false;
                slot_unlock(&slot->seq, seq);
                return idx;
            }
            slot_unlock(&slot->seq, seq);
        }
    }
    return SIZE_MAX;
}

/* ---------------------------------------------------------------- Public API Implementation */

cache_t* cache_create(size_t capacity, uint32_t default_ttl) {
    struct cache_s* c = calloc(1, sizeof(struct cache_s));
    if (!c) return NULL;

    if (capacity == 0) capacity = 1000;
    size_t shard_cap = capacity / CACHE_SHARD_COUNT;
    if (shard_cap < 1) shard_cap = 1;

    c->default_ttl = default_ttl ? default_ttl : CACHE_DEFAULT_TTL;
    cache_tick();

    for (size_t i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* s = &c->shards[i];
        s->capacity = shard_cap;

        size_t desired = (size_t)(shard_cap * INITIAL_BUCKET_MULTIPLIER);
        s->bucket_count = next_power_of_2(desired);

        s->slots = calloc(s->bucket_count, sizeof(cache_slot_t));
        if (!s->slots) goto cleanup_error;

#if defined(__linux__) && defined(MADV_HUGEPAGE)
        madvise(s->slots, s->bucket_count * sizeof(cache_slot_t), MADV_HUGEPAGE);
#endif

        for (size_t k = 0; k < s->bucket_count; k++) {
            atomic_store_explicit(&s->slots[k].tag, TAG_EMPTY, memory_order_relaxed);
        }

        if (!slab_init(&s->slab, shard_cap / 4 + 1, OVERFLOW_SLAB_SIZE)) { goto cleanup_error; }

        atomic_store_explicit(&s->size, 0, memory_order_relaxed);
        atomic_store_explicit(&s->tombstone_count, 0, memory_order_relaxed);
        atomic_store_explicit(&s->clock_hand, 0, memory_order_relaxed);
    }

    return (cache_t*)c;

cleanup_error:
    for (size_t j = 0; j < CACHE_SHARD_COUNT; j++) {
        free(c->shards[j].slots);
        slab_destroy(&c->shards[j].slab);
    }
    free(c);
    return NULL;
}

void cache_destroy(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* s = &cache->shards[i];
        if (s->slots) {
            for (size_t j = 0; j < s->bucket_count; j++) {
                uint16_t tag = atomic_load_explicit(&s->slots[j].tag, memory_order_relaxed);
                if (!(tag & TAG_CONTROL_MASK) && s->slots[j].overflow) {
                    slab_free(&s->slab, s->slots[j].value.overflow_ptr);
                }
            }
        }
        free(s->slots);
        slab_destroy(&s->slab);
    }
    free(cache);
}

const void* cache_get(cache_t* cache_ptr, const char* key, size_t klen, size_t* out_len) {
    if (unlikely(!cache_ptr || !key || !klen || klen > INLINE_KEY_MAX)) return NULL;

    struct cache_s* cache = (struct cache_s*)cache_ptr;
    uint32_t hash = hash_key(key, klen);
    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];
    size_t mask = shard->bucket_count - 1;
    size_t idx = hash & mask;

    /* PREFETCH IMMEDIATELY: Start DRAM line fill into L1 cache for target slot */
    __builtin_prefetch(&shard->slots[idx], 0, 3);

    static _Thread_local unsigned char tls_snapshot[INLINE_VALUE_MAX];
    uint16_t target_tag = make_tag(hash);

    PROBE_INIT(probes);

    for (size_t probe = 0; probe < shard->bucket_count; probe++) {
        cache_slot_t* slot = &shard->slots[idx];

        /* Prefetch next linear probe slot in advance */
        size_t next_idx = (idx + 1) & mask;
        __builtin_prefetch(&shard->slots[next_idx], 0, 1);

        uint16_t tag = atomic_load_explicit(&slot->tag, memory_order_relaxed);
        PROBE_INC(probes);

        if (tag == target_tag) {
            for (;;) {
                uint32_t seq1 = atomic_load_explicit(&slot->seq, memory_order_acquire);
                if (unlikely(seq1 & 1u)) continue;

                if (unlikely(slot->hash != hash || slot->key_len != klen) ||
                    unlikely(memcmp(slot->key, key, klen) != 0)) {
                    break;
                }

                uint32_t value_len = slot->value_len;
                time_t expires_at = slot->expires_at;
                bool overflow = slot->overflow;
                void* overflow_ptr = overflow ? slot->value.overflow_ptr : NULL;

                if (likely(!overflow)) { memcpy(tls_snapshot, slot->value.inline_value, value_len); }

                uint32_t seq2 = atomic_load_explicit(&slot->seq, memory_order_acquire);
                if (likely(seq1 == seq2)) {
                    time_t now = coarse_now();
                    if (unlikely(now >= expires_at)) {
                        PROBE_RECORD(probes);
                        return NULL;
                    }

                    if (atomic_load_explicit(&slot->clock_bit, memory_order_relaxed) == 0) {
                        atomic_store_explicit(&slot->clock_bit, 1, memory_order_relaxed);
                    }

                    if (out_len) *out_len = value_len;
                    PROBE_RECORD(probes);
                    return overflow ? overflow_ptr : tls_snapshot;
                }
            }
        } else if (tag & TAG_EMPTY) {
            PROBE_RECORD(probes);
            return NULL;
        }

        idx = next_idx;
    }

    PROBE_RECORD(probes);
    return NULL;
}

bool cache_set(cache_t* cache_ptr, const char* key, size_t klen, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr || !key || !klen || !value || !value_len || klen > INLINE_KEY_MAX)) return false;

    struct cache_s* cache = (struct cache_s*)cache_ptr;
    uint32_t hash = hash_key(key, klen);
    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];
    size_t mask = shard->bucket_count - 1;
    size_t idx = hash & mask;

    /* PREFETCH IMMEDIATELY: Write-intent prefetch into L1 cache */
    __builtin_prefetch(&shard->slots[idx], 1, 3);

    uint16_t target_tag = make_tag(hash);
    time_t now = coarse_now();

    bool needs_overflow = value_len > INLINE_VALUE_MAX;
    void* overflow_block = NULL;

    if (needs_overflow) {
        if (value_len <= OVERFLOW_SLAB_SIZE) { overflow_block = slab_alloc(&shard->slab); }
        if (!overflow_block) {
            overflow_block = malloc(value_len);
            if (!overflow_block) return false;
        }
        memcpy(overflow_block, value, value_len);
    }

    for (size_t probe = 0; probe < shard->bucket_count; probe++) {
        cache_slot_t* slot = &shard->slots[idx];

        size_t next_idx = (idx + 1) & mask;
        __builtin_prefetch(&shard->slots[next_idx], 1, 1);

        uint16_t tag = atomic_load_explicit(&slot->tag, memory_order_relaxed);

        /* Case 1: Match existing slot for update */
        if (tag == target_tag) {
            uint32_t seq;
            if (slot_try_lock(&slot->seq, &seq)) {
                if (slot->hash == hash && slot->key_len == klen && memcmp(slot->key, key, klen) == 0) {
                    void* old_overflow = slot->overflow ? slot->value.overflow_ptr : NULL;

                    slot->value_len = (uint32_t)value_len;
                    slot->expires_at = now + (time_t)(ttl ? ttl : cache->default_ttl);
                    atomic_store_explicit(&slot->clock_bit, 1, memory_order_relaxed);

                    if (needs_overflow) {
                        slot->overflow = true;
                        slot->value.overflow_ptr = overflow_block;
                    } else {
                        slot->overflow = false;
                        memcpy(slot->value.inline_value, value, value_len);
                    }

                    slot_unlock(&slot->seq, seq);
                    if (old_overflow) slab_free(&shard->slab, old_overflow);
                    return true;
                }
                slot_unlock(&slot->seq, seq);
            }
        }

        /* Case 2: Claim empty or tombstone slot using single CAS on slot->tag */
        if (tag & (TAG_EMPTY | TAG_DELETED)) {
            uint16_t expected = tag;
            if (atomic_compare_exchange_strong_explicit(&slot->tag, &expected, TAG_BUSY, memory_order_acquire,
                                                        memory_order_relaxed)) {
                uint32_t seq;
                while (!slot_try_lock(&slot->seq, &seq)) {}

                slot->hash = hash;
                slot->key_len = (uint32_t)klen;
                slot->value_len = (uint32_t)value_len;
                slot->expires_at = now + (time_t)(ttl ? ttl : cache->default_ttl);
                atomic_store_explicit(&slot->clock_bit, 1, memory_order_relaxed);
                memcpy(slot->key, key, klen);
                slot->key[klen] = '\0';

                if (needs_overflow) {
                    slot->overflow = true;
                    slot->value.overflow_ptr = overflow_block;
                } else {
                    slot->overflow = false;
                    memcpy(slot->value.inline_value, value, value_len);
                }

                slot_unlock(&slot->seq, seq);
                atomic_store_explicit(&slot->tag, target_tag, memory_order_release);

                if (tag & TAG_DELETED) {
                    atomic_fetch_sub_explicit(&shard->tombstone_count, 1, memory_order_relaxed);
                } else {
                    atomic_fetch_add_explicit(&shard->size, 1, memory_order_relaxed);
                }
                return true;
            }
        }

        idx = next_idx;
    }

    if (overflow_block) slab_free(&shard->slab, overflow_block);
    return false;
}

void cache_invalidate(cache_t* cache_ptr, const char* key) {
    if (!cache_ptr || !key) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t klen = strlen(key);
    if (klen > INLINE_KEY_MAX) return;

    uint32_t hash = hash_key(key, klen);
    uint16_t target_tag = make_tag(hash);
    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];

    size_t mask = shard->bucket_count - 1;
    size_t idx = hash & mask;

    for (size_t probe = 0; probe < shard->bucket_count; probe++) {
        cache_slot_t* slot = &shard->slots[idx];
        uint16_t tag = atomic_load_explicit(&slot->tag, memory_order_relaxed);

        if (tag == target_tag) {
            uint32_t seq;
            if (slot_try_lock(&slot->seq, &seq)) {
                if (slot->hash == hash && slot->key_len == klen && memcmp(slot->key, key, klen) == 0) {
                    void* old_overflow = slot->overflow ? slot->value.overflow_ptr : NULL;
                    slot->overflow = false;

                    slot_unlock(&slot->seq, seq);
                    atomic_store_explicit(&slot->tag, TAG_DELETED, memory_order_release);

                    atomic_fetch_sub_explicit(&shard->size, 1, memory_order_relaxed);
                    atomic_fetch_add_explicit(&shard->tombstone_count, 1, memory_order_relaxed);

                    if (old_overflow) slab_free(&shard->slab, old_overflow);
                    return;
                }
                slot_unlock(&slot->seq, seq);
            }
        } else if (tag & TAG_EMPTY) {
            return;
        }

        idx = (idx + 1) & mask;
    }
}

void cache_release(const void* ptr) {
    (void)ptr; /* No-op */
}

void cache_clear(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* s = &cache->shards[i];
        for (size_t j = 0; j < s->bucket_count; j++) {
            uint16_t tag = atomic_load_explicit(&s->slots[j].tag, memory_order_relaxed);
            if (!(tag & TAG_CONTROL_MASK)) {
                cache_slot_t* slot = &s->slots[j];
                uint32_t seq;
                if (slot_try_lock(&slot->seq, &seq)) {
                    if (slot->overflow && slot->value.overflow_ptr) {
                        slab_free(&s->slab, slot->value.overflow_ptr);
                        slot->value.overflow_ptr = NULL;
                    }
                    slot_unlock(&slot->seq, seq);
                }
            }
            atomic_store_explicit(&s->slots[j].tag, TAG_EMPTY, memory_order_relaxed);
        }
        atomic_store_explicit(&s->size, 0, memory_order_relaxed);
        atomic_store_explicit(&s->tombstone_count, 0, memory_order_relaxed);
    }
}

size_t get_total_cache_size(cache_t* cache_ptr) {
    if (!cache_ptr) return 0;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        total += atomic_load_explicit(&cache->shards[i].size, memory_order_relaxed);
    }
    return total;
}

size_t get_total_capacity(cache_t* cache_ptr) {
    if (!cache_ptr) return 0;
    struct cache_s* cache = (struct cache_s*)cache_ptr;
    size_t total = 0;
    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        total += cache->shards[i].capacity;
    }
    return total;
}

/* ---------------------------------------------------------------- Persistence */

static inline bool file_write_chk(const void* ptr, size_t size, size_t count, FILE* stream) {
    return fwrite(ptr, size, count, stream) == count;
}

static inline bool file_read_chk(void* ptr, size_t size, size_t count, FILE* stream) {
    return fread(ptr, size, count, stream) == count;
}

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
    time_t now = coarse_now();

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* shard = &cache->shards[i];

        for (size_t j = 0; j < shard->bucket_count; j++) {
            cache_slot_t* slot = &shard->slots[j];
            uint16_t tag = atomic_load_explicit(&slot->tag, memory_order_relaxed);
            if (tag & TAG_CONTROL_MASK) continue;

            uint32_t seq1 = atomic_load_explicit(&slot->seq, memory_order_acquire);
            if (seq1 & 1u) continue;

            time_t expiry = slot->expires_at;
            if (expiry <= now) continue;

            uint32_t klen = slot->key_len;
            uint64_t vlen = (uint64_t)slot->value_len;
            int64_t exp_out = (int64_t)expiry;
            const void* val_ptr = slot->overflow ? slot->value.overflow_ptr : slot->value.inline_value;

            if (!val_ptr) continue;

            if (!file_write_chk(&klen, sizeof(klen), 1, f) || !file_write_chk(&vlen, sizeof(vlen), 1, f) ||
                !file_write_chk(&exp_out, sizeof(exp_out), 1, f) || !file_write_chk(slot->key, 1, klen, f) ||
                !file_write_chk(val_ptr, 1, (size_t)vlen, f)) {
                fclose(f);
                return false;
            }

            uint32_t seq2 = atomic_load_explicit(&slot->seq, memory_order_acquire);
            if (seq1 == seq2) { actual_count++; }
        }
    }

    fseek(f, (long)(sizeof(magic) + sizeof(version)), SEEK_SET);
    if (!file_write_chk(&actual_count, sizeof(actual_count), 1, f)) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

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

    char key_buf[INLINE_KEY_MAX + 1];
    void* val_buf = NULL;
    size_t val_buf_cap = 0;
    time_t now = coarse_now();
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

        if (klen > INLINE_KEY_MAX) {
            if (fseek(f, (long)klen + (long)vlen, SEEK_CUR) != 0) {
                success = false;
                break;
            }
            continue;
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

        if (!file_read_chk(key_buf, 1, klen, f) || !file_read_chk(val_buf, 1, (size_t)vlen, f)) {
            success = false;
            break;
        }

        key_buf[klen] = '\0';

        if ((time_t)expiry > now) {
            uint32_t remaining_ttl = (uint32_t)((time_t)expiry - now);
            if (!cache_set(cache_ptr, key_buf, klen, val_buf, (size_t)vlen, remaining_ttl)) {
                success = false;
                break;
            }
        }
    }

    free(val_buf);
    fclose(f);
    return success;
}
