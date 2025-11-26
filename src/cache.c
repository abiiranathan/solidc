#include "../include/cache.h"
#include "../include/spinlock.h"

#include <errno.h>
#include <immintrin.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) {
    atomic_int ref_count;       // Reference count for safe concurrent access
    _Atomic uint8_t clock_bit;  // CLOCK algorithm: 1 = recently used, 0 = candidate for eviction
    time_t expires_at;          // Absolute expiration timestamp
    uint32_t hash;              // Full 32-bit hash of the key
    uint32_t key_len;           // Length of the key string (excluding null terminator)
    size_t value_len;           // Length of the value data
    unsigned char data[];       // Flexible array: [key]['\0'][back_ptr][value]
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

typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) {
    cache_slot_t* slots;  // Array of 16-byte slots
    size_t bucket_count;  // Hash table size (always power of 2 for fast modulo)
    size_t size;          // Current number of valid entries
    size_t capacity;      // Maximum entries before eviction triggers
    size_t clock_hand;    // CLOCK algorithm: position for next eviction scan
    fast_rwlock_t lock;   // Reader-writer lock(spin lock) for this shard
} aligned_cache_shard_t;

// / Verify at compile time that the struct is exactly one cache line
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

// Helper to check a single slot
// Returns: 0 = continue, 1 = found key, 2 = found empty (stop), 3 = found tombstone (record)
static inline int check_slot(cache_slot_t* slot, uint64_t target_meta, const char* key, size_t klen, bool* found) {
    uint64_t meta = slot->metadata;
    uint32_t h    = meta_hash(meta);

    if (h == HASH_EMPTY) return 2;
    if (h == HASH_DELETED) return 3;

    if (meta == target_meta) {
        // Pointer is already in L1 due to struct co-location
        cache_entry_t* entry = slot->entry;
        if (memcmp(entry->data, key, klen) == 0) {
            *found = true;
            return 1;
        }
    }
    return 0;
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

    // Pre-compute target metadata: only exact match (hash AND length) is a candidate
    uint64_t target     = pack_meta(hash, (uint32_t)klen);
    cache_slot_t* slots = shard->slots;

    *found = false;

    // Probing loop unrolled to process one full cache line (4 slots) per iteration
    while (probe_count + 4 <= shard->bucket_count) {
        size_t i0 = idx;
        size_t i1 = (idx + 1) & mask;
        size_t i2 = (idx + 2) & mask;
        size_t i3 = (idx + 3) & mask;

        // Slot 0 (Fetches the cache line from RAM)
        int res = check_slot(&slots[i0], target, key, klen, found);
        if (res == 1) return i0;
        if (res == 2) return (first_tombstone != SIZE_MAX) ? first_tombstone : i0;
        if (res == 3 && first_tombstone == SIZE_MAX) first_tombstone = i0;

        // Slot 1 (L1 Hit)
        res = check_slot(&slots[i1], target, key, klen, found);
        if (res == 1) return i1;
        if (res == 2) return (first_tombstone != SIZE_MAX) ? first_tombstone : i1;
        if (res == 3 && first_tombstone == SIZE_MAX) first_tombstone = i1;

        // Slot 2 (L1 Hit)
        res = check_slot(&slots[i2], target, key, klen, found);
        if (res == 1) return i2;
        if (res == 2) return (first_tombstone != SIZE_MAX) ? first_tombstone : i2;
        if (res == 3 && first_tombstone == SIZE_MAX) first_tombstone = i2;

        // Slot 3 (L1 Hit)
        res = check_slot(&slots[i3], target, key, klen, found);
        if (res == 1) return i3;
        if (res == 2) return (first_tombstone != SIZE_MAX) ? first_tombstone : i3;
        if (res == 3 && first_tombstone == SIZE_MAX) first_tombstone = i3;

        idx = (idx + 4) & mask;
        probe_count += 4;
    }

    // Cleanup loop
    while (probe_count < shard->bucket_count) {
        int res = check_slot(&slots[idx], target, key, klen, found);
        if (res == 1) return idx;
        if (res == 2) return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
        if (res == 3 && first_tombstone == SIZE_MAX) first_tombstone = idx;

        idx = (idx + 1) & mask;
        probe_count++;
    }

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
    size_t scanned      = 0;
    size_t mask         = shard->bucket_count - 1;
    cache_slot_t* slots = shard->slots;

    // Scan up to 2 full cycles to find victim
    while (scanned < shard->bucket_count * 2) {
        size_t idx        = shard->clock_hand;
        shard->clock_hand = (shard->clock_hand + 1) & mask;  // Advance hand

        uint64_t meta = slots[idx].metadata;

        // Skip empty or deleted slots
        if (meta_hash(meta) < HASH_MIN_VAL) {
            scanned++;
            continue;
        }

        cache_entry_t* entry = slots[idx].entry;

        // Atomically read and clear clock bit
        uint8_t bit = atomic_load_explicit(&entry->clock_bit, memory_order_relaxed);

        if (bit == 0) {
            // Clock bit was already 0: evict this entry
            slots[idx].metadata = pack_meta(HASH_DELETED, 0);
            slots[idx].entry    = NULL;
            shard->size--;
            entry_ref_dec(entry);
            return true;
        } else {
            atomic_store_explicit(&entry->clock_bit, 0, memory_order_relaxed);
        }

        // Clock bit was 1: gave it a second chance, continue
        scanned++;
    }

    // Fallback: force evict at current clock hand position
    size_t idx = shard->clock_hand;
    if (meta_hash(slots[idx].metadata) >= HASH_MIN_VAL) {
        cache_entry_t* entry = slots[idx].entry;
        slots[idx].metadata  = pack_meta(HASH_DELETED, 0);
        slots[idx].entry     = NULL;
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
        aligned_cache_shard_t* s = &c->shards[i];
        s->capacity              = shard_cap;

        // Allocate 2x capacity to reduce collision rate
        size_t desired  = (size_t)(shard_cap * INITIAL_BUCKET_MULTIPLIER);
        s->bucket_count = next_power_of_2(desired);

        // Allocate slots array aligned to cache line
        // Each slot is 16 bytes, so 4 slots fit exactly in one 64-byte line
        if (posix_memalign((void**)&s->slots, CACHE_LINE_SIZE, s->bucket_count * sizeof(cache_slot_t)) != 0) {
            goto cleanup_error;
        }

        // Hint for huge pages
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
 * Decrements reference counts on all entries.
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
 * The returned pointer is reference-counted. Caller must call cache_release() when done.
 * Automatically checks expiration and invalidates expired entries.
 *
 * @param cache_ptr The cache instance
 * @param key The key string (null-terminated)
 * @param out_len Output: length of the value (can be NULL)
 * @return Pointer to value on success, NULL if not found or expired
 */
const void* cache_get(cache_t* cache_ptr, const char* key, size_t klen, size_t* out_len) {
    if (unlikely(!cache_ptr || !key || !klen)) return NULL;

    struct cache_s* cache = (struct cache_s*)cache_ptr;

    uint32_t hash = hash_key(key, klen);

    // Select shard using low bits of hash
    aligned_cache_shard_t* shard = &cache->shards[hash & (CACHE_SHARD_COUNT - 1)];

    fast_rwlock_rdlock(&shard->lock);

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (!found) {
        fast_rwlock_unlock_rd(&shard->lock);
        return NULL;
    }

    cache_slot_t* slot   = &shard->slots[idx];
    cache_entry_t* entry = slot->entry;

    // Check expiration
    if (unlikely(time(NULL) >= entry->expires_at)) {
        fast_rwlock_unlock_rd(&shard->lock);
        // Invalidate expired entry (requires write lock)
        cache_invalidate(cache_ptr, key);
        return NULL;
    }

    // Silent store optimization
    if (atomic_load_explicit(&entry->clock_bit, memory_order_relaxed) == 0) {
        // Mark as recently used for CLOCK algorithm
        atomic_store_explicit(&entry->clock_bit, 1, memory_order_relaxed);
    }
    entry_ref_inc(entry);  // Increment ref count before returning

    if (out_len) *out_len = entry->value_len;

    fast_rwlock_unlock_rd(&shard->lock);
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
 * @param klen The length of the key.
 * @param value Pointer to value data
 * @param value_len Length of value data in bytes
 * @param ttl Time-to-live in seconds (0 = use default_ttl)
 * @return true on success, false on allocation failure
 */
bool cache_set(cache_t* cache_ptr, const char* key, size_t klen, const void* value, size_t value_len, uint32_t ttl) {
    if (unlikely(!cache_ptr || !key | !klen || !value || !value_len)) return false;

    struct cache_s* cache = (struct cache_s*)cache_ptr;
    uint32_t hash         = hash_key(key, klen);

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
    fast_rwlock_wrlock(&shard->lock);

    // Evict if at capacity
    if (shard->size >= shard->capacity) {
        clock_evict(shard);
    }

    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (found) {
        // Update existing entry
        cache_entry_t* old      = shard->slots[idx].entry;
        shard->slots[idx].entry = new_entry;
        // Metadata (hash+keylen) is identical, no update needed
        entry_ref_dec(old);  // Release old entry
    } else {
        // Insert new entry
        shard->slots[idx].entry    = new_entry;
        shard->slots[idx].metadata = pack_meta(hash, (uint32_t)klen);
        shard->size++;
    }

    fast_rwlock_unlock_wr(&shard->lock);
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

    fast_rwlock_wrlock(&shard->lock);
    bool found;
    size_t idx = find_slot(shard, hash, key, klen, &found);

    if (found) {
        cache_entry_t* entry       = shard->slots[idx].entry;
        shard->slots[idx].metadata = pack_meta(HASH_DELETED, 0);  // Mark as tombstone
        shard->slots[idx].entry    = NULL;
        shard->size--;
        entry_ref_dec(entry);
    }
    fast_rwlock_unlock_wr(&shard->lock);
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
        fast_rwlock_wrlock(&cache->shards[i].lock);
        for (size_t j = 0; j < cache->shards[i].bucket_count; j++) {
            if (meta_hash(cache->shards[i].slots[j].metadata) >= HASH_MIN_VAL) {
                entry_ref_dec(cache->shards[i].slots[j].entry);
            }
        }
        memset(cache->shards[i].slots, 0, cache->shards[i].bucket_count * sizeof(cache_slot_t));
        cache->shards[i].size       = 0;
        cache->shards[i].clock_hand = 0;
        fast_rwlock_unlock_wr(&cache->shards[i].lock);
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
        fast_rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].size;
        fast_rwlock_unlock_rd(&cache->shards[i].lock);
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
        fast_rwlock_rdlock(&cache->shards[i].lock);
        total += cache->shards[i].capacity;
        fast_rwlock_unlock_rd(&cache->shards[i].lock);
    }
    return total;
}

/**
 * Helper to write safely to a file.
 */
static inline bool file_write_chk(const void* ptr, size_t size, size_t count, FILE* stream) {
    return fwrite(ptr, size, count, stream) == count;
}

/**
 * Helper to read safely from a file.
 */
static inline bool file_read_chk(void* ptr, size_t size, size_t count, FILE* stream) {
    return fread(ptr, size, count, stream) == count;
}

bool cache_save(cache_t* cache_ptr, const char* filename) {
    if (!cache_ptr || !filename) return false;
    struct cache_s* cache = (struct cache_s*)cache_ptr;

    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    // 1. Write Header
    uint32_t magic         = CACHE_FILE_MAGIC;
    uint32_t version       = CACHE_FILE_VERSION;
    uint64_t total_entries = 0;

    if (!file_write_chk(&magic, sizeof(magic), 1, f) || !file_write_chk(&version, sizeof(version), 1, f) ||
        !file_write_chk(&total_entries, sizeof(total_entries), 1, f)) {
        fclose(f);
        return false;
    }

    // 2. Iterate shards and write valid entries
    // We lock one shard at a time to avoid freezing the whole system
    uint64_t actual_count = 0;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        aligned_cache_shard_t* shard = &cache->shards[i];

        // Read lock is sufficient
        fast_rwlock_rdlock(&shard->lock);

        for (size_t j = 0; j < shard->bucket_count; j++) {
            cache_slot_t* slot = &shard->slots[j];

            // Skip empty or deleted slots
            if (meta_hash(slot->metadata) < HASH_MIN_VAL) continue;

            cache_entry_t* entry = slot->entry;
            if (!entry) {
                continue;
            }

            // Skip expired entries
            if (entry->expires_at <= time(NULL)) continue;

            // Prepare metadata for disk
            uint32_t klen  = entry->key_len;
            uint64_t vlen  = (uint64_t)entry->value_len;  // Use 64-bit for disk format stability
            int64_t expiry = (int64_t)entry->expires_at;

            // Get pointer to the value data
            const void* val_ptr = value_ptr_from_entry(entry);
            if (!val_ptr) {
                continue;
            }

            // Write: [KeyLen][ValLen][Expiry][KeyBytes][ValBytes]
            if (!file_write_chk(&klen, sizeof(klen), 1, f) || !file_write_chk(&vlen, sizeof(vlen), 1, f) ||
                !file_write_chk(&expiry, sizeof(expiry), 1, f) ||
                !file_write_chk(entry->data, 1, klen, f) ||      // Key stored at entry->data
                !file_write_chk(val_ptr, 1, (size_t)vlen, f)) {  // Value stored after backptr

                fast_rwlock_unlock_rd(&shard->lock);
                fclose(f);
                return false;
            }

            actual_count++;
        }

        fast_rwlock_unlock_rd(&shard->lock);
    }

    // 3. Rewind and write actual count
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

    // 1. Check Header
    uint32_t magic        = 0;
    uint32_t version      = 0;
    uint64_t stored_count = 0;

    if (!file_read_chk(&magic, sizeof(magic), 1, f) || !file_read_chk(&version, sizeof(version), 1, f) ||
        !file_read_chk(&stored_count, sizeof(stored_count), 1, f)) {
        fclose(f);
        return false;
    }

    if (magic != CACHE_FILE_MAGIC || version != CACHE_FILE_VERSION) {
        // Invalid format or version mismatch
        fclose(f);
        return false;
    }

    // 2. Read Entries
    // Use a small stack buffer for keys to avoid mallocs for short keys
    char key_buf[512];
    char* big_key_buf  = NULL;
    void* val_buf      = NULL;
    size_t val_buf_cap = 0;
    time_t now         = time(NULL);

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

        // Handle Key Buffer
        char* kptr = key_buf;
        if (klen >= sizeof(key_buf)) {
            // Key is larger than stack buffer, alloc on heap
            big_key_buf = realloc(big_key_buf, klen + 1);
            if (!big_key_buf) {
                success = false;
                break;
            }
            kptr = big_key_buf;
        }

        // Handle Value Buffer
        if (vlen > val_buf_cap) {
            void* tmp = realloc(val_buf, vlen);
            if (!tmp) {
                success = false;
                break;
            }
            val_buf     = tmp;
            val_buf_cap = vlen;
        }

        // Read Data
        if (!file_read_chk(kptr, 1, klen, f) || !file_read_chk(val_buf, 1, (size_t)vlen, f)) {
            success = false;
            break;
        }

        // Null terminate key for cache_set API
        kptr[klen] = '\0';

        // Only insert if not expired
        if ((time_t)expiry > now) {
            // Calculate remaining TTL
            uint32_t ttl = (uint32_t)((time_t)expiry - now);

            // cache_set handles hashing, sharding, and eviction
            if (!cache_set(cache_ptr, kptr, klen, val_buf, (size_t)vlen, ttl)) {
                // If set fails (e.g. malloc error), we stop
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
