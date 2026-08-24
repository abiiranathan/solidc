/**
 * @file map_swiss.c
 * @brief Swiss table implementation of the swiss_map.h API.
 *
 * See include/swiss_map.h for the design overview.  Hashing semantics
 * intentionally match src/map.c (identity for keys <= 8 bytes, XXH3
 * otherwise) so benchmarks isolate structural differences — SIMD group
 * probing and 1-byte control metadata — from hashing differences.
 */
#include "../include/swiss_map.h"

#include <immintrin.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "../include/lock.h"

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define SW_GROUP 16          /* SIMD group width                     */
#define SW_EMPTY   0x80      /* ctrl byte: never used                */
#define SW_DELETED 0xFE      /* ctrl byte: tombstone                 */
#define SW_MASK_H2 0x7F      /* H2 = hash & 0x7F stored in ctrl      */

/* Max load factor is fixed at 7/8 (the abseil sweet spot); the config's
 * max_load_factor is clamped into (0.1, 0.875] but only as a ceiling:
 * growth always happens at >= 7/8 full. */
#define SW_MAX_LOAD_NUM 7
#define SW_MAX_LOAD_DEN 8

typedef struct swiss_map {
    unsigned char* ctrl;   /* capacity + SW_GROUP bytes; tail mirrors head */
    void** kv;             /* interleaved key/value pointers, capacity*2   */
    size_t* lens;          /* per-entry key_len: resizes rehash FAITHFULLY
                              with the original length instead of whatever
                              key_len the triggering call happened to use
                              (a footgun documented on map_set).          */
    size_t size;           /* live entries                                 */
    size_t tombs;          /* tombstones                                   */
    size_t capacity;       /* multiple of 16                               */
    size_t growth_left;    /* capacity*7/8 - size - tombs_reclaimed...     */
    float max_load_factor;
    HashFunction hash;
    KeyCmpFunction key_compare;
    KeyFreeFunction key_free;
    ValueFreeFunction value_free;
    Lock lock;
} SwissMap;

/* ------------------------------------------------------------------ */
/* Group scan primitives                                               */
/* ------------------------------------------------------------------ */

#if defined(__SSE2__)
#define SW_HAVE_SSE2 1
static inline __m128i sw_group_load(const unsigned char* p) { return _mm_loadu_si128((const __m128i*)p); }
static inline uint32_t sw_group_match(__m128i g, unsigned char needle) {
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(g, _mm_set1_epi8((char)needle)));
}
static inline uint32_t sw_group_empty(__m128i g) {
    /* Exact equality with EMPTY: tombstones (DELETED) do not match. */
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(g, _mm_set1_epi8((char)SW_EMPTY)));
}
#else
/* Portable fallback: emulate a 16-byte group scan. */
typedef struct {
    unsigned char b[SW_GROUP];
} sw_group;
static inline sw_group sw_group_load(const unsigned char* p) {
    sw_group g;
    memcpy(g.b, p, SW_GROUP);
    return g;
}
static inline uint32_t sw_group_match(sw_group g, unsigned char needle) {
    uint32_t m = 0;
    for (int i = 0; i < SW_GROUP; i++)
        if (g.b[i] == needle) m |= 1u << i;
    return m;
}
static inline uint32_t sw_group_empty(sw_group g) { return sw_group_match(g, SW_EMPTY); }
#endif

static inline size_t sw_h1(size_t hash, size_t mask) { return (size_t)(hash >> 7) & mask; }
static inline unsigned char sw_h2(size_t hash) { return (unsigned char)(hash & SW_MASK_H2); }

static inline bool sw_is_full(unsigned char c) { return (c & 0x80) == 0; }

static inline size_t sw_cap_round(size_t n) {
    size_t c = 16;
    while (c < n) c <<= 1;
    return c;
}

static inline size_t swiss_default_hash(const void* key, size_t len) {
    if (len <= sizeof(uint64_t)) {
        union {
            uint64_t u64;
            uint8_t u8[8];
        } v = {0};
        const uint8_t* s = (const uint8_t*)key;
        for (size_t i = 0; i < len; i++) v.u8[i] = s[i];
        return (size_t)v.u64;
    }
    return (size_t)XXH3_64bits(key, len);
}

/*
 * Swiss tables REQUIRE well-distributed hashes: group probing degrades
 * badly when nearby keys cluster into nearby groups (sequential identity
 * hashes do exactly that — measured 100x slowdowns).  A Murmur3-style
 * finalizer restores avalanche at negligible cost.  This divergence from
 * src/map.c hashing is deliberate: Robin Hood tolerates ordered hashes,
 * group probing does not.
 */
static inline size_t swiss_finalize(size_t h) {
    uint64_t x = (uint64_t)h;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (size_t)x;
}

static inline size_t swiss_hash_of(SwissMap* m, const void* key, size_t key_len) {
    if (m->hash) return swiss_finalize(m->hash(key, key_len));
    return swiss_finalize(swiss_default_hash(key, key_len));
}

static void swiss_ctrl_init(SwissMap* m) {
    memset(m->ctrl, SW_EMPTY, m->capacity + SW_GROUP);
}

static void swiss_set_ctrl(SwissMap* m, size_t i, unsigned char h2) {
    m->ctrl[i] = h2;
    if (i < SW_GROUP) m->ctrl[m->capacity + i] = h2; /* mirror group */
}

static inline void** sw_slot_kv(SwissMap* m, size_t i) { return &m->kv[i * 2]; }

/* Allocate fresh arrays for @p cap and install them.  The PREVIOUS
 * arrays are NOT freed here -- swiss_resize() must keep them readable
 * while re-inserting -- callers free them explicitly when done. */
static bool swiss_alloc_arrays(SwissMap* m, size_t cap, unsigned char** old_ctrl_out, void*** old_kv_out,
                               size_t** old_lens_out, size_t* old_cap_out) {
    unsigned char* ctrl = malloc(cap + SW_GROUP + SW_GROUP);
    void** kv = calloc(cap * 2, sizeof(void*));
    size_t* lens = calloc(cap, sizeof(size_t));
    if (!ctrl || !kv || !lens) {
        free(ctrl);
        free(kv);
        free(lens);
        return false;
    }
    if (old_ctrl_out) *old_ctrl_out = m->ctrl;
    if (old_kv_out) *old_kv_out = m->kv;
    if (old_lens_out) *old_lens_out = m->lens;
    if (old_cap_out) *old_cap_out = m->capacity;

    m->ctrl = ctrl;
    m->kv = kv;
    m->lens = lens;
    m->capacity = cap;
    swiss_ctrl_init(m);
    m->size = 0;
    m->tombs = 0;
    m->growth_left = cap / SW_MAX_LOAD_DEN * SW_MAX_LOAD_NUM;
    return true;
}

SwissMap* swiss_create(const SwissConfig* config) {
    if (!config || !config->key_compare) return NULL;

    SwissMap* m = calloc(1, sizeof(SwissMap));
    if (!m) return NULL;

    size_t cap = config->initial_capacity > 0 ? sw_cap_round(config->initial_capacity) : 16;
    if (cap < 16) cap = 16;
    if (cap > SIZE_MAX / (2 * sizeof(void*))) {
        free(m);
        return NULL;
    }

    if (!swiss_alloc_arrays(m, cap, NULL, NULL, NULL, NULL)) {
        free(m);
        return NULL;
    }

    m->max_load_factor =
        (config->max_load_factor > 0.1f && config->max_load_factor <= 0.875f) ? config->max_load_factor : 0.75f;
    m->key_compare = config->key_compare;
    m->key_free = config->key_free;
    m->value_free = config->value_free;
    lock_init(&m->lock);
    return m;
}

void swiss_destroy(SwissMap* m) {
    if (!m) return;

    if (m->key_free || m->value_free) {
        for (size_t i = 0; i < m->capacity; i++) {
            if (sw_is_full(m->ctrl[i])) {
                void** kv = sw_slot_kv(m, i);
                if (m->key_free) m->key_free(kv[0]);
                if (m->value_free) m->value_free(kv[1]);
            }
        }
    }

    free(m->ctrl);
    free(m->kv);
    free(m->lens);
    lock_free(&m->lock);
    free(m);
}

size_t swiss_length(SwissMap* m) { return m ? m->size : 0; }
size_t swiss_capacity(SwissMap* m) { return m ? m->capacity : 0; }

/* Rehash every live entry into a fresh table of new_cap slots.
 * Returns false on allocation failure (original table untouched). */
static bool swiss_resize(SwissMap* m, size_t new_cap, size_t key_len) {
    (void)key_len; /* kept for API symmetry; stored lens are authoritative */
    unsigned char* old_ctrl;
    void** old_kv;
    size_t* old_lens;
    size_t old_cap;

    if (!swiss_alloc_arrays(m, new_cap, &old_ctrl, &old_kv, &old_lens, &old_cap)) {
        return false; /* original table untouched on failure */
    }

    const size_t mask = m->capacity - 1;
    for (size_t i = 0; i < old_cap; i++) {
        if (!sw_is_full(old_ctrl[i])) continue;

        void** e = &old_kv[i * 2];
        size_t h = swiss_hash_of(m, e[0], old_lens[i]);
        size_t pos = sw_h1(h, mask);
        unsigned char h2 = sw_h2(h);

        /* Probe for the first group with an empty slot; re-inserted
         * entries are unique so no match check is needed. */
        size_t stride = 0;
        for (;;) {
            __m128i g = sw_group_load(m->ctrl + pos);
            uint32_t emp = sw_group_empty(g);
            if (emp) {
                int j = __builtin_ctz(emp);
                size_t slot = (pos + (size_t)j) & mask;
                swiss_set_ctrl(m, slot, h2);
                void** kv = sw_slot_kv(m, slot);
                kv[0] = e[0];
                kv[1] = e[1];
                m->lens[slot] = old_lens[i]; /* carry original key_len */
                m->size++;
                break;
            }
            stride += SW_GROUP;
            pos = (pos + stride) & mask;
        }
    }

    free(old_ctrl);
    free(old_kv);
    free(old_lens);

    /* FIX: alloc_arrays granted a fresh budget of new_cap*7/8; debit it
     * for every element we just re-inserted, otherwise growth_left
     * overstates free space and the table only grows again when truly
     * full.  Non-negative: old load was already <= 7/8. */
    m->growth_left -= m->size;
    return true;
}

/* Grow or rehash-in-place when out of growth budget. */
static bool swiss_maybe_grow(SwissMap* m, size_t key_len) {
    if (m->growth_left > 0) return true;

    /* Tombstones are eating the budget: reclaim by rehashing at the same
     * capacity when they are significant, otherwise grow. */
    if (m->tombs > m->capacity / 32 + m->size / 32) {
        size_t keep = m->capacity;
        return swiss_resize(m, keep, key_len);
    }
    return swiss_resize(m, m->capacity * 2, key_len);
}

bool swiss_set(SwissMap* m, void* key, size_t key_len, void* value) {
    if (!m || !key) return false;

    if (m->growth_left == 0 && !swiss_maybe_grow(m, key_len)) return false;

    size_t h = swiss_hash_of(m, key, key_len);
    size_t mask = m->capacity - 1;
    size_t pos = sw_h1(h, mask);
    unsigned char h2 = sw_h2(h);

    ssize_t first_deleted = -1; /* first tombstone seen: reuse candidate */
    size_t stride = 0;

    for (;;) {
        __m128i g = sw_group_load(m->ctrl + pos);

        /* Check full-matching candidates in this group. */
        uint32_t matches = sw_group_match(g, h2);
        while (matches) {
            int j = __builtin_ctz(matches);
            matches &= matches - 1;
            size_t slot = (pos + (size_t)j) & mask;
            void** kv = sw_slot_kv(m, slot);
            if (m->key_compare(kv[0], key)) {
                /* Update in place. */
                if (m->value_free) m->value_free(kv[1]);
                kv[1] = value;
                return true;
            }
        }

        /* Empty terminates the probe: key cannot be beyond it. */
        uint32_t empties = sw_group_empty(g);
        if (empties) {
            break;
        }

        /* Remember the first tombstone for insertion. */
        if (first_deleted < 0) {
            uint32_t dels = sw_group_match(g, SW_DELETED);
            if (dels) first_deleted = (ssize_t)((pos + (size_t)__builtin_ctz(dels)) & mask);
        }

        stride += SW_GROUP;
        pos = (pos + stride) & mask;
    }

    /* Insert. */
    size_t slot;
    if (first_deleted >= 0) {
        slot = (size_t)first_deleted;
        m->tombs--;
    } else {
        /* Re-find an empty slot from the start of the probe. */
        size_t p = sw_h1(h, mask);
        size_t st = 0;
        for (;;) {
            __m128i g = sw_group_load(m->ctrl + p);
            uint32_t empties = sw_group_empty(g);
            if (empties) {
                slot = (p + (size_t)__builtin_ctz(empties)) & mask;
                break;
            }
            st += SW_GROUP;
            p = (p + st) & mask;
        }
        m->growth_left--;
    }

    swiss_set_ctrl(m, slot, h2);
    void** kv = sw_slot_kv(m, slot);
    kv[0] = key;
    kv[1] = value;
    m->lens[slot] = key_len;
    m->size++;
    return true;
}

void* swiss_get(SwissMap* m, void* key, size_t key_len) {
    if (!m || !key) return NULL;

    size_t h = swiss_hash_of(m, key, key_len);
    size_t mask = m->capacity - 1;
    size_t pos = sw_h1(h, mask);
    unsigned char h2 = sw_h2(h);
    size_t stride = 0;

    for (;;) {
        __m128i g = sw_group_load(m->ctrl + pos);

        uint32_t matches = sw_group_match(g, h2);
        while (matches) {
            int j = __builtin_ctz(matches);
            matches &= matches - 1;
            size_t slot = (pos + (size_t)j) & mask;
            void** kv = sw_slot_kv(m, slot);
            if (m->key_compare(kv[0], key)) return kv[1];
        }

        /* Stop at the first group containing an empty slot. */
        if (sw_group_empty(g)) return NULL;

        stride += SW_GROUP;
        pos = (pos + stride) & mask;
    }
}

bool swiss_remove(SwissMap* m, void* key, size_t key_len) {
    if (!m || !key) return false;

    size_t h = swiss_hash_of(m, key, key_len);
    size_t mask = m->capacity - 1;
    size_t pos = sw_h1(h, mask);
    unsigned char h2 = sw_h2(h);
    size_t stride = 0;

    for (;;) {
        __m128i g = sw_group_load(m->ctrl + pos);

        uint32_t matches = sw_group_match(g, h2);
        while (matches) {
            int j = __builtin_ctz(matches);
            matches &= matches - 1;
            size_t slot = (pos + (size_t)j) & mask;
            void** kv = sw_slot_kv(m, slot);
            if (m->key_compare(kv[0], key)) {
                if (m->key_free) m->key_free(kv[0]);
                if (m->value_free) m->value_free(kv[1]);
                kv[0] = NULL;
                kv[1] = NULL;
                m->lens[slot] = 0;
                swiss_set_ctrl(m, slot, SW_DELETED);
                m->size--;
                m->tombs++;
                return true;
            }
        }

        if (sw_group_empty(g)) return false;

        stride += SW_GROUP;
        pos = (pos + stride) & mask;
    }
}

swiss_iterator swiss_iter(SwissMap* m) {
    swiss_iterator it = {.map = m, .index = 0};
    return it;
}

bool swiss_next(swiss_iterator* it, void** key, void** value) {
    SwissMap* m = it->map;
    while (it->index < m->capacity) {
        size_t i = it->index++;
        if (sw_is_full(m->ctrl[i])) {
            void** kv = sw_slot_kv(m, i);
            if (key) *key = kv[0];
            if (value) *value = kv[1];
            return true;
        }
    }
    return false;
}

bool swiss_set_safe(SwissMap* m, void* key, size_t key_len, void* value) {
    if (!m) return false;
    lock_acquire(&m->lock);
    bool r = swiss_set(m, key, key_len, value);
    lock_release(&m->lock);
    return r;
}

void* swiss_get_safe(SwissMap* m, void* key, size_t key_len) {
    if (!m) return NULL;
    lock_acquire(&m->lock);
    void* r = swiss_get(m, key, key_len);
    lock_release(&m->lock);
    return r;
}

bool swiss_remove_safe(SwissMap* m, void* key, size_t key_len) {
    if (!m) return false;
    lock_acquire(&m->lock);
    bool r = swiss_remove(m, key, key_len);
    lock_release(&m->lock);
    return r;
}
