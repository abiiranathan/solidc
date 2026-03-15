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
 * @brief Co-located slot structure for the open-addressing hash table.
 *
 * Size: 16 bytes on 64-bit systems, fitting 4 slots per 64-byte cache line.
 *
 * The metadata field enables a fast-path comparison: a single 64-bit load
 * can simultaneously check both hash and key length before touching the
 * separately allocated cache_entry_t. This avoids a pointer chase and
 * potential cache miss on the common non-matching probe case.
 */
typedef struct {
    /**
     * Packed metadata for fast lookups without pointer chasing.
     * Encoding: High 32 bits = Hash, Low 32 bits = Key Length.
     * Special values (hash portion only):
     *   HASH_EMPTY   (0) - slot has never been used.
     *   HASH_DELETED (1) - slot held an entry that was removed (tombstone).
     * A single 64-bit comparison covers both hash and length simultaneously,
     * reducing false positives that would otherwise force a full memcmp.
     */
    uint64_t metadata;
    cache_entry_t* entry;  // Pointer to heap-allocated entry; NULL when slot is empty/deleted
} cache_slot_t;

/**
 * @brief A single shard of the cache hash table, cache-line aligned to prevent false sharing.
 *
 * The cache is split into CACHE_SHARD_COUNT independent shards, each with its own
 * lock. This reduces contention: threads hashing to different shards never block
 * each other.
 */
typedef struct ALIGN(CACHE_LINE_SIZE) {
    cache_slot_t* slots;     // Array of 16-byte slots; size is always a power of 2
    size_t bucket_count;     // Hash table size (always power of 2 for fast modulo via bitwise AND)
    size_t size;             // Current number of live (non-tombstone) entries
    size_t capacity;         // Maximum live entries before eviction triggers
    size_t tombstone_count;  // Number of HASH_DELETED slots; drives compaction decisions
    size_t clock_hand;       // CLOCK algorithm: index of the next slot to examine for eviction
    fast_rwlock_t lock;      // Reader-writer spinlock; readers share, writers are exclusive
} aligned_cache_shard_t;

// Verify at compile time that the struct is exactly one cache line.
// If this fires, a struct member was added that pushes the size past 64 bytes,
// which would cause two shards to share a cache line (false sharing).
// (Requires C11. If using older C, you can remove this check.)
_Static_assert(sizeof(aligned_cache_shard_t) == CACHE_LINE_SIZE, "Shard size mismatch: False sharing will occur");

struct cache_s {
    aligned_cache_shard_t shards[CACHE_SHARD_COUNT];  // Fixed array of shards; no heap indirection
    uint32_t default_ttl;  // Default time-to-live in seconds, used when ttl=0 is passed to cache_set
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
 * Without this, keys that differ only in low bits would cluster in the same shard.
 */
static inline size_t get_shard_idx(uint32_t hash) {
    hash ^= hash >> 16;
    return hash & (CACHE_SHARD_COUNT - 1);
}

/**
 * Packs hash and key length into single 64-bit value for fast comparison.
 * This is the core optimization: one 64-bit load + compare instead of pointer chase.
 * High 32 bits carry the hash; low 32 bits carry the key length.
 */
static inline uint64_t pack_meta(uint32_t hash, uint32_t len) { return ((uint64_t)hash << 32) | (uint32_t)len; }

/**
 * Extracts the hash portion from packed metadata (upper 32 bits).
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
 * Relaxed is safe here because the caller already holds a lock that provides
 * the necessary acquire/release barriers around the surrounding operations.
 */
static inline void entry_ref_inc(cache_entry_t* entry) {
    if (entry) atomic_fetch_add_explicit(&entry->ref_count, 1, memory_order_relaxed);
}

/**
 * Decrements entry reference count and frees if it reaches zero.
 * Uses acq_rel ordering to ensure all prior modifications are visible before free.
 * acq_rel on the fetch_sub pairs with itself on other threads: the thread that
 * brings the count to zero sees all prior decrements before it frees.
 */
static inline void entry_ref_dec(cache_entry_t* entry) {
    if (!entry) return;
    int old_val = atomic_fetch_sub_explicit(&entry->ref_count, 1, memory_order_acq_rel);
    if (unlikely(old_val == 1)) free(entry);
}

/**
 * Finds a slot in the hash table using standard linear probing.
 * Compares packed metadata first before touching pointers/L1 boundaries.
 *
 * On return, *found indicates whether the key was located. The returned index is:
 *   - The matching slot's index when *found == true.
 *   - The best available insertion slot (empty or first tombstone) when *found == false.
 *
 * NOTE: Compile with -O2 or -O3. Without optimization, pack_meta/meta_hash are
 * not inlined, causing visible call overhead (confirmed by perf annotation showing
 * 86.67% of cycles on the metadata load with unoptimized builds).
 */
static size_t find_slot(aligned_cache_shard_t* shard, uint32_t hash, const char* key, size_t klen, bool* found) {
    size_t mask = shard->bucket_count - 1;  // Power-of-2 allows fast modulo via bitwise AND
    size_t idx = hash & mask;               // Initial probe position
    size_t first_tombstone = SIZE_MAX;      // Track first deleted slot for insertion reuse

    // Pre-pack target once; avoids redundant pack_meta calls inside the loop
    uint64_t target = pack_meta(hash, (uint32_t)klen);
    cache_slot_t* slots = shard->slots;

    *found = false;

    for (size_t probe_count = 0; probe_count < shard->bucket_count; probe_count++) {
        uint64_t meta = slots[idx].metadata;
        uint32_t h = meta_hash(meta);

        if (h == HASH_EMPTY) {
            // Empty slot terminates the probe chain: the key cannot exist past this point.
            // Return the first tombstone if we passed one; it's a better insertion target
            // because reusing tombstones reduces the need for compaction.
            return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
        }

        if (h == HASH_DELETED) {
            // Tombstone: key may still exist further along the chain, keep probing.
            // Record the first tombstone we see for potential insertion reuse.
            if (first_tombstone == SIZE_MAX) first_tombstone = idx;
        } else if (meta == target) {
            // Full metadata match (hash + key length). Now confirm the key itself.
            // This memcmp is a pointer chase into a separately malloc'd block and
            // may cause an L1 miss, but only occurs on genuine hash+length matches,
            // so false positives are rare with a good hash function.
            cache_entry_t* entry = slots[idx].entry;
            if (memcmp(entry->data, key, klen) == 0) {
                *found = true;
                return idx;
            }
        }

        idx = (idx + 1) & mask;  // Linear probe: advance to next slot (wraps via mask)
    }

    // Exhausted all slots (table is completely full of live entries and tombstones).
    // This should not happen under normal operation because eviction prevents the
    // table from reaching 100% occupancy, but return the best available fallback.
    return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
}

/**
 * Re-hashes live entries into a newly allocated slot array to eliminate tombstones.
 * Solves O(n) probing degradation under deletion-heavy workloads.
 *
 * Compaction is triggered when tombstone_count exceeds 75% of bucket_count.
 * Using bucket_count (not capacity) as the threshold is intentional: tombstones
 * occupy slots in the bucket array, so that is the correct denominator.
 *
 * NOTE: ALIGNED_ALLOC'd memory is released with free(). This is correct when
 * ALIGNED_ALLOC resolves to C11 aligned_alloc(), which mandates free() for release.
 * If your aligned_alloc.h uses a custom allocator, verify that free() is the
 * correct release function or replace with the appropriate deallocator here.
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
            // Re-insert live entry using its stored hash; no need to rehash the key.
            uint32_t hash = meta_hash(meta);
            size_t idx = hash & mask;
            // Linear probe to find an empty slot in the new array.
            // No tombstones exist in new_slots, so HASH_EMPTY is the only terminator.
            while (meta_hash(new_slots[idx].metadata) != HASH_EMPTY) {
                idx = (idx + 1) & mask;
            }
            new_slots[idx].metadata = meta;
            new_slots[idx].entry = old_slots[i].entry;
        }
    }

    shard->slots = new_slots;
    shard->tombstone_count = 0;  // All tombstones eliminated by the re-hash
    shard->clock_hand = 0;       // Reset eviction scan position; old offsets are invalid

    // free() is correct here: C11 aligned_alloc() memory is released with free().
    // See note in function header if using a non-standard allocator.
    free(old_slots);
}

/**
 * Evicts one entry using the CLOCK algorithm (approximate LRU).
 *
 * The CLOCK hand sweeps the bucket array. Entries with clock_bit=1 (recently accessed)
 * get a second chance: the bit is cleared and the hand advances. Entries with
 * clock_bit=0 are evicted immediately.
 *
 * Optimization vs. original: the scan is capped at one full pass (bucket_count
 * iterations) rather than two. After one pass all clock bits have been cleared,
 * so a second pass would always evict the first live entry it finds — which is
 * exactly what the unconditional fallback at the end does, making the second
 * pass redundant. Capping at one pass halves worst-case scan time while holding
 * the write lock, directly reducing contention seen in the perf profile.
 */
static bool clock_evict(aligned_cache_shard_t* shard) {
    size_t scanned = 0;
    size_t mask = shard->bucket_count - 1;
    cache_slot_t* slots = shard->slots;

    // Single pass: give recently-used entries one reprieve, evict the first stale one.
    while (scanned < shard->bucket_count) {
        size_t idx = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;

        uint64_t meta = slots[idx].metadata;

        if (meta_hash(meta) < HASH_MIN_VAL) {
            // Empty or tombstone slot; nothing to evict here, advance hand.
            scanned++;
            continue;
        }

        cache_entry_t* entry = slots[idx].entry;
        uint8_t bit = atomic_load_explicit(&entry->clock_bit, memory_order_relaxed);

        if (bit == 0) {
            // Clock bit is clear: this entry has not been accessed since the last
            // sweep. Evict it by marking the slot as a tombstone.
            slots[idx].metadata = pack_meta(HASH_DELETED, 0);
            slots[idx].entry = NULL;
            shard->size--;
            shard->tombstone_count++;
            entry_ref_dec(entry);
            return true;
        } else {
            // Clock bit is set: entry was recently used. Clear the bit to give it
            // one more pass before eviction and continue scanning.
            atomic_store_explicit(&entry->clock_bit, 0, memory_order_relaxed);
        }
        scanned++;
    }

    // Fallback: scan forward from clock_hand until a live slot is found.
    // After one full pass all clock bits are cleared, so a live slot is
    // guaranteed to exist when shard->size > 0.
    for (size_t i = 0; i < shard->bucket_count; i++) {
        size_t idx = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;
        if (meta_hash(slots[idx].metadata) >= HASH_MIN_VAL) {
            cache_entry_t* entry = slots[idx].entry;
            slots[idx].metadata = pack_meta(HASH_DELETED, 0);
            slots[idx].entry = NULL;
            shard->size--;
            shard->tombstone_count++;
            entry_ref_dec(entry);
            return true;
        }
    }
    return false;  // Unreachable when shard->size > 0; satisfies the compiler
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
 *
 * On a hit the caller receives a pointer into the entry's data block. The entry's
 * reference count is incremented before the lock is released, ensuring the memory
 * remains valid even if another thread evicts the entry concurrently. The caller
 * MUST call cache_release() when done with the pointer.
 *
 * On expiry the function upgrades to a write lock to remove the stale entry. A
 * double-check is performed after the upgrade because another thread may have
 * refreshed or removed the entry between the read-unlock and write-lock.
 * Compaction is intentionally NOT triggered here; that is deferred to the write
 * path (cache_set, cache_invalidate) to avoid O(n) work on the read fast path.
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

        // Upgrade to write lock to remove the expired entry.
        // Re-probe after acquiring the write lock (double-check locking): another
        // thread may have removed or replaced this entry in the window between
        // our read-unlock and this write-lock.
        fast_rwlock_wrlock(&shard->lock);
        bool re_found;
        size_t re_idx = find_slot(shard, hash, key, klen, &re_found);
        if (re_found) {
            cache_entry_t* re_entry = shard->slots[re_idx].entry;
            // Confirm the entry is still expired; a concurrent cache_set could have
            // refreshed the TTL between our read-unlock and this write-lock.
            if (now >= re_entry->expires_at) {
                shard->slots[re_idx].metadata = pack_meta(HASH_DELETED, 0);
                shard->slots[re_idx].entry = NULL;
                shard->size--;
                shard->tombstone_count++;
                // Compaction is not triggered here to avoid O(n) rehash on the
                // read path. The next cache_set or cache_invalidate will compact
                // if the tombstone threshold is crossed.
                entry_ref_dec(re_entry);
            }
        }
        fast_rwlock_unlock_wr(&shard->lock);
        return NULL;
    }

    // Silent store optimization: set the clock bit to 1 only if it is currently 0,
    // avoiding an unnecessary atomic write-back on every access.
    if (atomic_load_explicit(&entry->clock_bit, memory_order_relaxed) == 0) {
        atomic_store_explicit(&entry->clock_bit, 1, memory_order_relaxed);
    }
    // Increment ref count before releasing the lock so the entry cannot be freed
    // by a concurrent eviction or invalidation between unlock and caller use.
    entry_ref_inc(entry);

    if (out_len) *out_len = entry->value_len;

    fast_rwlock_unlock_rd(&shard->lock);
    return value_ptr_from_entry(entry);
}

/**
 * Inserts or updates a key-value pair in the cache.
 *
 * The new entry is fully constructed outside the lock to minimize critical section
 * duration. If the key already exists the old entry is replaced in-place (size
 * unchanged). On a new insertion, eviction is triggered if the shard is at capacity.
 *
 * Optimization vs. original: find_slot is called only ONCE per insert. The
 * original code called find_slot a second time after clock_evict() returned,
 * reasoning that the evicted slot might have invalidated idx. This was unnecessary:
 *   1. find_slot returns the best available insertion slot (empty or first tombstone).
 *   2. clock_evict() only converts a *different* live slot into a tombstone.
 *   3. compact_shard() is the only operation that reallocates shard->slots and
 *      invalidates all indices — but compaction is run *before* find_slot, not after.
 * Therefore the original idx remains a valid insertion target after eviction.
 */
bool cache_set(cache_t* cache_ptr, const char* key, size_t klen, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr || !key || !klen || !value || !value_len)) return false;

    struct cache_s* cache = (struct cache_s*)cache_ptr;
    uint32_t hash = hash_key(key, klen);
    time_t now = time(NULL);

    // Allocate and populate the new entry before acquiring the lock.
    // Memory layout: [cache_entry_t header][key]['\0'][back_ptr][value]
    size_t alloc_sz = offsetof(cache_entry_t, data) + klen + 1 + sizeof(void*) + value_len;
    cache_entry_t* new_entry = malloc(alloc_sz);
    if (!new_entry) return false;

    new_entry->hash = hash;
    new_entry->key_len = (uint32_t)klen;
    new_entry->value_len = value_len;
    new_entry->expires_at = now + (ttl ? ttl : cache->default_ttl);
    atomic_init(&new_entry->ref_count, 1);
    atomic_init(&new_entry->clock_bit, 1);  // Mark as recently used on insertion

    // Write key, null terminator, back-pointer, and value into the flexible array.
    unsigned char* ptr = new_entry->data;
    memcpy(ptr, key, klen);
    ptr[klen] = '\0';
    // Back-pointer: allows entry_from_value() to recover the entry from a value ptr.
    *(cache_entry_t**)(ptr + klen + 1) = new_entry;
    memcpy(ptr + klen + 1 + sizeof(cache_entry_t*), value, value_len);

    aligned_cache_shard_t* shard = &cache->shards[get_shard_idx(hash)];
    fast_rwlock_wrlock(&shard->lock);

    // Compact before probing so that find_slot operates on a clean table.
    // Threshold: tombstones exceed 75% of bucket capacity.
    // Using bucket_count (not capacity) is correct: tombstones consume bucket slots,
    // not logical capacity slots. A high tombstone-to-bucket ratio degrades probe
    // chains regardless of how many live entries the shard holds.
    if (unlikely(shard->tombstone_count > (shard->bucket_count * 3) / 4)) {
        compact_shard(shard);
    }

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (found) {
        // Key already exists: swap the entry pointer. Size is unchanged.
        cache_entry_t* old = shard->slots[idx].entry;
        shard->slots[idx].entry = new_entry;
        entry_ref_dec(old);
    } else {
        // New key: evict if at capacity, then insert into the slot find_slot returned.
        if (shard->size >= shard->capacity) {
            // clock_evict() marks some other slot HASH_DELETED. It does NOT touch
            // shard->slots or invalidate idx. The idx returned by find_slot above
            // remains a valid insertion target — no second find_slot call needed.
            if (!clock_evict(shard)) {
                // Respect capacity limit on the cache.
                // This can happen if the shard is full of entries with their clock bits set (recently accessed),
                // causing clock_evict to scan the entire bucket array without finding a candidate for eviction.
                fast_rwlock_unlock_wr(&shard->lock);
                free(new_entry);
                return false;
            }
        }

        // If idx was previously a tombstone, reclaim it (reduce tombstone count).
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

        // Use the updated bucket_count-relative threshold consistent with cache_set.
        if (unlikely(shard->tombstone_count > (shard->bucket_count * 3) / 4)) {
            compact_shard(shard);
        }
        entry_ref_dec(entry);
    }
    fast_rwlock_unlock_wr(&shard->lock);
}

/**
 * Releases a reference to a cached value obtained from cache_get().
 * The caller must not access the pointer after calling this function.
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
 * Returns the total number of live entries across all shards.
 * Each shard is read-locked independently; the returned value is a consistent
 * per-shard snapshot but not an atomic snapshot of the entire cache.
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
 * Capacity is set at creation time and does not change, so locking here is
 * defensive rather than strictly necessary.
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

// --- Persistence Helpers ---

/**
 * Writes exactly @p count elements of @p size bytes from @p ptr to @p stream.
 * Returns true on success, false if the write was short or failed.
 */
static inline bool file_write_chk(const void* ptr, size_t size, size_t count, FILE* stream) {
    return fwrite(ptr, size, count, stream) == count;
}

/**
 * Reads exactly @p count elements of @p size bytes from @p stream into @p ptr.
 * Returns true on success, false if the read was short or failed (including EOF).
 */
static inline bool file_read_chk(void* ptr, size_t size, size_t count, FILE* stream) {
    return fread(ptr, size, count, stream) == count;
}

/**
 * Serialises all non-expired entries to a binary file.
 *
 * File format:
 *   [magic:u32][version:u32][entry_count:u64]
 *   For each entry:
 *     [key_len:u32][value_len:u64][expires_at:i64][key_bytes:key_len][value_bytes:value_len]
 *
 * entry_count is written as a placeholder (zero) in the header and patched to
 * the actual count at the end via fseek, because expired entries are skipped
 * during iteration and the true count is not known upfront.
 */
bool cache_save(cache_t* cache_ptr, const char* filename) {
    if (!cache_ptr || !filename) return false;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    uint32_t magic = CACHE_FILE_MAGIC;
    uint32_t version = CACHE_FILE_VERSION;
    uint64_t total_entries = 0;  // Placeholder; patched after iteration

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
            int64_t expiry = (int64_t)entry->expires_at;  // Cast to fixed-width for portability

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

    // Patch the entry count in the header now that we know the true value.
    fseek(f, sizeof(magic) + sizeof(version), SEEK_SET);
    if (!file_write_chk(&actual_count, sizeof(actual_count), 1, f)) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

/**
 * Loads entries from a file previously written by cache_save().
 *
 * Entries whose expiry timestamp has already passed at load time are silently
 * skipped. The TTL passed to cache_set is computed as the remaining lifetime
 * (expiry - now), preserving the original expiration wall-clock time.
 *
 * Key buffer strategy: a stack buffer (512 bytes) handles the common case.
 * Oversized keys fall back to a heap buffer that is reused across iterations
 * via realloc to avoid per-entry allocation.
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

    char key_buf[512];         // Stack buffer for keys up to 511 bytes (covers the vast majority)
    char* big_key_buf = NULL;  // Heap fallback for oversized keys; reused across loop iterations
    void* val_buf = NULL;      // Heap buffer for values; grown as needed, reused across iterations
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

        // Select key destination: stack buffer if it fits, heap buffer otherwise.
        char* kptr = key_buf;
        if (klen >= sizeof(key_buf)) {
            big_key_buf = realloc(big_key_buf, klen + 1);
            if (!big_key_buf) {
                success = false;
                break;
            }
            kptr = big_key_buf;
        }

        // Grow the value buffer if the current entry's value exceeds its capacity.
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

        // Skip entries that expired before or during this load.
        if ((time_t)expiry > now) {
            // Compute remaining TTL to preserve the original expiry wall-clock time.
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
