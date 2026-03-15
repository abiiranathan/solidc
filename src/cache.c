#include "../include/cache.h"

#include "../include/align.h"
#include "../include/aligned_alloc.h"
#include "../include/spinlock.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__linux__) && defined(MADV_HUGEPAGE)
#include <sys/mman.h>
#endif

#define CACHE_LINE_SIZE           64          // Size of CPU cache line for alignment
#define INITIAL_BUCKET_MULTIPLIER 2           // Load factor: buckets = capacity * 2
#define CACHE_FILE_MAGIC          0x45484346  // ASCII "FCHE" (Fast Cache) in Little Endian
#define CACHE_FILE_VERSION        1

// --- Packed Metadata Constants ---
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
typedef struct ALIGN(CACHE_LINE_SIZE) {
    atomic_int ref_count;       // Reference count for safe concurrent access
    _Atomic uint8_t clock_bit;  // CLOCK algorithm: 1 = recently used, 0 = candidate for eviction
    time_t expires_at;          // Absolute expiration timestamp
    uint32_t hash;              // Full 32-bit hash of the key
    uint32_t key_len;           // Length of the key string (excluding null terminator)
    size_t value_len;           // Length of the value data
    // Flexible array: [key]['\0'][back_ptr][value]
#if defined(_MSC_VER) && !defined(__cplusplus)
    unsigned char data[1];  // MSVC C mode workaround
#else
    unsigned char data[];  // C99 flexible array member
#endif
} cache_entry_t;

/**
 * @brief Co-located slot structure.
 * Size: 16 bytes (on 64-bit systems).
 * Layout: [Metadata (8B)] [Entry Pointer (8B)]
 */
typedef struct {
    /**
     * Packed metadata array for fast lookups without pointer chasing.
     * Each uint64_t stores: High 32 bits = Hash, Low 32 bits = Key Length
     * This allows single 64-bit comparison instead of loading pointer + comparing fields.
     * Fits 8 metadata slots per 64-byte cache line.
     */
    uint64_t metadata;
    cache_entry_t* entry;  // Pointer to the entry
} cache_slot_t;

typedef struct ALIGN(CACHE_LINE_SIZE) {
    cache_slot_t* slots;     // Array of 16-byte slots
    size_t bucket_count;     // Hash table size (always power of 2 for fast modulo)
    size_t size;             // Current number of valid entries
    size_t capacity;         // Maximum entries before eviction triggers
    size_t tombstone_count;  // Number of HASH_DELETED slots
    size_t clock_hand;       // CLOCK algorithm: position for next eviction scan
    fast_rwlock_t lock;      // Reader-writer lock(spin lock) for this shard
} aligned_cache_shard_t;

// Verify at compile time that the struct is exactly one cache line
// (Requires C11. If using older C, you can remove this check)
_Static_assert(sizeof(aligned_cache_shard_t) == CACHE_LINE_SIZE, "Shard size mismatch: False sharing will occur");

struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT];  // Fixed array of shards
    uint32_t default_ttl;                             // Default time-to-live in seconds
};

/**
 * Rounds up to the next power of 2.
 * Required because hash table size must be power of 2 for fast modulo via bitwise AND.
 */
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
 * Mixes high bits into low bits to prevent shard-selection hotspots.
 */
static inline size_t get_shard_idx(uint32_t hash) {
    hash ^= hash >> 16;
    return hash & (CACHE_SHARD_COUNT - 1);
}

/**
 * Packs hash and key length into single 64-bit value for fast comparison.
 * This is the core optimization: one 64-bit load + compare instead of pointer chase.
 */
static inline uint64_t pack_meta(uint32_t hash, uint32_t len) { return ((uint64_t)hash << 32) | (uint32_t)len; }

/**
 * Extracts hash from packed metadata.
 */
static inline uint32_t meta_hash(uint64_t meta) { return (uint32_t)(meta >> 32); }

/**
 * Calculates the value pointer from a cache entry.
 * Value is stored after: [key]['\0'][back_pointer]
 */
static inline const void* value_ptr_from_entry(const cache_entry_t* entry) {
    return (const void*)(entry->data + entry->key_len + 1 + sizeof(cache_entry_t*));
}

/**
 * Recovers cache_entry_t pointer from a returned value pointer.
 * Uses the back-pointer stored between key and value in the entry's data array.
 */
static inline cache_entry_t* entry_from_value(const void* value_ptr) {
    if (unlikely(!value_ptr)) return NULL;
    cache_entry_t* const* back_ptr = (cache_entry_t* const*)((const char*)value_ptr - sizeof(cache_entry_t*));
    return *back_ptr;
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
 * Finds a slot in the hash table using standard linear probing.
 * Compares packed metadata first before touching pointers/L1 boundaries.
 */
static size_t find_slot(aligned_cache_shard_t* shard, uint32_t hash, const char* key, size_t klen, bool* found) {
    size_t mask = shard->bucket_count - 1;  // Power-of-2 allows fast modulo
    size_t idx = hash & mask;
    size_t first_tombstone = SIZE_MAX;  // Track first deleted slot for insertion

    uint64_t target = pack_meta(hash, (uint32_t)klen);
    cache_slot_t* slots = shard->slots;

    *found = false;

    for (size_t probe_count = 0; probe_count < shard->bucket_count; probe_count++) {
        uint64_t meta = slots[idx].metadata;
        uint32_t h = meta_hash(meta);

        if (h == HASH_EMPTY) {
            return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
        }

        if (h == HASH_DELETED) {
            if (first_tombstone == SIZE_MAX) first_tombstone = idx;
        } else if (meta == target) {
            cache_entry_t* entry = slots[idx].entry;
            if (memcmp(entry->data, key, klen) == 0) {
                *found = true;
                return idx;
            }
        }

        idx = (idx + 1) & mask;
    }

    return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
}

/**
 * Re-hashes live entries into a newly allocated slot array to eliminate tombstones.
 * Solves O(n) probing degradation under deletion-heavy workloads.
 */
static void compact_shard(aligned_cache_shard_t* shard) {
    size_t old_bucket_count = shard->bucket_count;
    cache_slot_t* old_slots = shard->slots;

    cache_slot_t* new_slots = ALIGNED_ALLOC(CACHE_LINE_SIZE, old_bucket_count * sizeof(cache_slot_t));
    if (!new_slots) return;  // Graceful fallback: we keep running with existing setup

#ifdef MADV_HUGEPAGE
    madvise(new_slots, old_bucket_count * sizeof(cache_slot_t), MADV_HUGEPAGE);
#endif
    memset(new_slots, 0, old_bucket_count * sizeof(cache_slot_t));

    size_t mask = old_bucket_count - 1;
    for (size_t i = 0; i < old_bucket_count; i++) {
        uint64_t meta = old_slots[i].metadata;
        if (meta_hash(meta) >= HASH_MIN_VAL) {
            uint32_t hash = meta_hash(meta);
            size_t idx = hash & mask;
            while (meta_hash(new_slots[idx].metadata) != HASH_EMPTY) {
                idx = (idx + 1) & mask;
            }
            new_slots[idx].metadata = meta;
            new_slots[idx].entry = old_slots[i].entry;
        }
    }

    shard->slots = new_slots;
    shard->tombstone_count = 0;
    shard->clock_hand = 0;

    free(old_slots);
}

/**
 * Evicts an entry using the CLOCK algorithm (approximation of LRU).
 */
static bool clock_evict(aligned_cache_shard_t* shard) {
    size_t scanned = 0;
    size_t mask = shard->bucket_count - 1;
    cache_slot_t* slots = shard->slots;

    while (scanned < shard->bucket_count * 2) {
        size_t idx = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;

        uint64_t meta = slots[idx].metadata;

        if (meta_hash(meta) < HASH_MIN_VAL) {
            scanned++;
            continue;
        }

        cache_entry_t* entry = slots[idx].entry;
        uint8_t bit = atomic_load_explicit(&entry->clock_bit, memory_order_relaxed);

        if (bit == 0) {
            slots[idx].metadata = pack_meta(HASH_DELETED, 0);
            slots[idx].entry = NULL;
            shard->size--;
            shard->tombstone_count++;
            entry_ref_dec(entry);
            return true;
        } else {
            atomic_store_explicit(&entry->clock_bit, 0, memory_order_relaxed);
        }
        scanned++;
    }

    size_t idx = shard->clock_hand;
    if (meta_hash(slots[idx].metadata) >= HASH_MIN_VAL) {
        cache_entry_t* entry = slots[idx].entry;
        slots[idx].metadata = pack_meta(HASH_DELETED, 0);
        slots[idx].entry = NULL;
        shard->size--;
        shard->tombstone_count++;
        entry_ref_dec(entry);
        return true;
    }
    return false;
}

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

        s->slots = ALIGNED_ALLOC(CACHE_LINE_SIZE, s->bucket_count * sizeof(cache_slot_t));
        if (!s->slots) {
            goto cleanup_error;
        }

#ifdef MADV_HUGEPAGE
        madvise(s->slots, s->bucket_count * sizeof(cache_slot_t), MADV_HUGEPAGE);
#endif
        memset(s->slots, 0, s->bucket_count * sizeof(cache_slot_t));

        fast_rwlock_init(&s->lock);
    }

    return (cache_t*)c;

cleanup_error:
    for (size_t j = 0; j < CACHE_SHARD_COUNT; j++) {
        aligned_cache_shard_t* s = &c->shards[j];
        free(s->slots);
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
        fast_rwlock_wrlock(&cache->shards[i].lock);
        if (cache->shards[i].slots) {
            for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
                if (meta_hash(cache->shards[i].slots[j].metadata) >= HASH_MIN_VAL) {
                    entry_ref_dec(cache->shards[i].slots[j].entry);
                }
            }
            free(cache->shards[i].slots);
        }
        fast_rwlock_unlock_wr(&cache->shards[i].lock);
    }
    free(cache);
}

/**
 * Retrieves a value from the cache by key.
 */
const void* cache_get(cache_t* cache_ptr, const char* key, size_t klen, size_t* out_len) {
    if (unlikely(!cache_ptr || !key || !klen)) return NULL;

    struct cache_s* cache = (struct cache_s*)cache_ptr;
    uint32_t hash = hash_key(key, klen);
    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];

    fast_rwlock_rdlock(&shard->lock);

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (!found) {
        fast_rwlock_unlock_rd(&shard->lock);
        return NULL;
    }

    cache_slot_t* slot = &shard->slots[idx];
    cache_entry_t* entry = slot->entry;

    time_t now = time(NULL);

    // Check expiration
    if (unlikely(now >= entry->expires_at)) {
        fast_rwlock_unlock_rd(&shard->lock);

        // Invalidate expired entry directly to avoid external rehashing API
        fast_rwlock_wrlock(&shard->lock);
        bool re_found;
        size_t re_idx = find_slot(shard, hash, key, klen, &re_found);
        if (re_found) {
            cache_entry_t* re_entry = shard->slots[re_idx].entry;
            // Double check expiry in case another thread simultaneously updated the TTL
            if (now >= re_entry->expires_at) {
                shard->slots[re_idx].metadata = pack_meta(HASH_DELETED, 0);
                shard->slots[re_idx].entry = NULL;
                shard->size--;
                shard->tombstone_count++;

                if (unlikely(shard->tombstone_count > shard->capacity / 2)) {
                    compact_shard(shard);
                }
                entry_ref_dec(re_entry);
            }
        }
        fast_rwlock_unlock_wr(&shard->lock);
        return NULL;
    }

    // Silent store optimization
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
    time_t now = time(NULL);

    size_t alloc_sz = offsetof(cache_entry_t, data) + klen + 1 + sizeof(void*) + value_len;
    cache_entry_t* new_entry = malloc(alloc_sz);
    if (!new_entry) return false;

    new_entry->hash = hash;
    new_entry->key_len = (uint32_t)klen;
    new_entry->value_len = value_len;
    new_entry->expires_at = now + (ttl ? ttl : cache->default_ttl);
    atomic_init(&new_entry->ref_count, 1);
    atomic_init(&new_entry->clock_bit, 1);

    unsigned char* ptr = new_entry->data;
    memcpy(ptr, key, klen);
    ptr[klen] = '\0';
    *(cache_entry_t**)(ptr + klen + 1) = new_entry;
    memcpy(ptr + klen + 1 + sizeof(cache_entry_t*), value, value_len);

    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];
    fast_rwlock_wrlock(&shard->lock);

    // Run proactive compaction if accumulating too many tombstones
    if (unlikely(shard->tombstone_count > shard->capacity / 2)) {
        compact_shard(shard);
    }

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (found) {
        // Update existing entry (no eviction needed, size remains identical)
        cache_entry_t* old = shard->slots[idx].entry;
        shard->slots[idx].entry = new_entry;
        entry_ref_dec(old);
    } else {
        // Inserting a new entry
        if (shard->size >= shard->capacity) {
            if (clock_evict(shard)) {
                // Must ensure slot index is accurately updated post-eviction
                idx = find_slot(shard, hash, key, klen, &found);
            }
        }

        if (meta_hash(shard->slots[idx].metadata) == HASH_DELETED) {
            shard->tombstone_count--;
        }
        shard->slots[idx].entry = new_entry;
        shard->slots[idx].metadata = pack_meta(hash, (uint32_t)klen);
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
    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];

    fast_rwlock_wrlock(&shard->lock);
    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (found) {
        cache_entry_t* entry = shard->slots[idx].entry;
        shard->slots[idx].metadata = pack_meta(HASH_DELETED, 0);
        shard->slots[idx].entry = NULL;
        shard->size--;
        shard->tombstone_count++;

        if (unlikely(shard->tombstone_count > shard->capacity / 2)) {
            compact_shard(shard);
        }
        entry_ref_dec(entry);
    }
    fast_rwlock_unlock_wr(&shard->lock);
}

/**
 * Releases a reference to a cached value.
 */
void cache_release(const void* ptr) {
    if (!ptr) return;
    entry_ref_dec(entry_from_value(ptr));
}

/**
 * Removes all entries from the cache.
 */
void cache_clear(cache_t* cache_ptr) {
    if (!cache_ptr) return;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        fast_rwlock_wrlock(&cache->shards[i].lock);
        for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
            if (meta_hash(cache->shards[i].slots[j].metadata) >= HASH_MIN_VAL) {
                entry_ref_dec(cache->shards[i].slots[j].entry);
            }
        }
        memset(cache->shards[i].slots, 0, cache->shards[i].bucket_count * sizeof(cache_slot_t));
        cache->shards[i].size = 0;
        cache->shards[i].tombstone_count = 0;
        cache->shards[i].clock_hand = 0;
        fast_rwlock_unlock_wr(&cache->shards[i].lock);
    }
}

/**
 * Returns the total number of entries across all shards.
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
 * Returns the total capacity across all shards.
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
    time_t now = time(NULL);

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* shard = &cache->shards[i];

        fast_rwlock_rdlock(&shard->lock);

        for (size_t j = 0; j < shard->bucket_count; j++) {
            cache_slot_t* slot = &shard->slots[j];

            if (meta_hash(slot->metadata) < HASH_MIN_VAL) continue;

            cache_entry_t* entry = slot->entry;
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

    fseek(f, sizeof(magic) + sizeof(version), SEEK_SET);
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
            uint32_t ttl = (uint32_t)((time_t)expiry - now);

            if (!cache_set(cache_ptr, kptr, klen, val_buf, (size_t)vlen, ttl)) {
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
