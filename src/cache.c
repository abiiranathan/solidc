#include "cache.h"
#include <stdio.h>

// FNV-1a Hash Function
static inline uint32_t hash_key(const char* key, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)(unsigned char)key[i];
        hash *= 16777619u;
    }
    return hash;
}

// Helper: Get pointer to key inside the entry
static inline const char* entry_key(const cache_entry_t* entry) {
    return (const char*)entry->data;
}

// Helper: Get pointer to value inside the entry
static inline const unsigned char* entry_value(const cache_entry_t* entry) {
    return entry->data + entry->key_len + 1;  // +1 for null terminator
}

// --- LRU Logic (Shard Local) ---

static inline void lru_remove(cache_shard_t* shard, cache_entry_t* entry) {
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

static inline void lru_add_to_front(cache_shard_t* shard, cache_entry_t* entry) {
    entry->next = shard->head;
    entry->prev = NULL;
    if (shard->head) shard->head->prev = entry;
    shard->head = entry;
    if (!shard->tail) shard->tail = entry;
}

static void lru_evict(cache_shard_t* shard) {
    if (!shard->tail) return;

    cache_entry_t* victim = shard->tail;

    // Remove from hash bucket
    size_t bucket_idx    = victim->hash % shard->bucket_count;
    cache_entry_t** slot = &shard->buckets[bucket_idx];

    while (*slot && *slot != victim) {
        slot = &(*slot)->hash_next;
    }
    if (*slot) *slot = victim->hash_next;

    // Remove from list
    lru_remove(shard, victim);

    free(victim);
    shard->size--;
}

// --- API Implementation ---

cache_t* cache_create(size_t total_capacity, uint32_t default_ttl) {
    cache_t* cache = malloc(sizeof(*cache));
    if (!cache) return NULL;

    if (total_capacity == 0) total_capacity = 10000;
    size_t cap_per_shard = total_capacity / CACHE_SHARD_COUNT;
    if (cap_per_shard < 10) cap_per_shard = 10;

    cache->default_ttl = default_ttl > 0 ? default_ttl : CACHE_DEFAULT_TTL;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        cache_shard_t* shard = &cache->shards[i];
        shard->capacity      = cap_per_shard;
        shard->size          = 0;
        shard->head          = NULL;
        shard->tail          = NULL;

        // Prime number buckets roughly 1.5x capacity to minimize collisions
        shard->bucket_count = cap_per_shard + (cap_per_shard / 2) + 1;
        shard->buckets      = calloc(shard->bucket_count, sizeof(cache_entry_t*));

        if (!shard->buckets) {
            // Cleanup previous shards if failure
            for (int j = 0; j < i; j++) {
                free(cache->shards[j].buckets);
                rwlock_destroy(&cache->shards[j].lock);
            }
            free(cache);
            return NULL;
        }

#ifdef _WIN32
        rwlock_init(&shard->lock);
#else
        if (rwlock_init(&shard->lock) != 0) {
            // Handle error...
            free(shard->buckets);
            free(cache);
            return NULL;
        }
#endif
    }

    return cache;
}

void cache_destroy(cache_t* cache) {
    if (!cache) return;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        cache_shard_t* shard = &cache->shards[i];
        rwlock_wrlock(&shard->lock);

        cache_entry_t* curr = shard->head;
        while (curr) {
            cache_entry_t* next = curr->next;
            free(curr);
            curr = next;
        }
        free(shard->buckets);
        rwlock_unlock_wr(&shard->lock);
        rwlock_destroy(&shard->lock);
    }
    free(cache);
}

bool cache_get(cache_t* cache, const char* key, char** out_value, size_t* out_len) {
    if (!cache || !key) return false;

    size_t key_len = strlen(key);
    uint32_t hash  = hash_key(key, key_len);
    // Determine which shard owns this key
    uint32_t shard_idx   = hash % CACHE_SHARD_COUNT;
    cache_shard_t* shard = &cache->shards[shard_idx];

    rwlock_rdlock(&shard->lock);

    // Look in specific shard's bucket
    size_t bucket_idx    = hash % shard->bucket_count;
    cache_entry_t* entry = shard->buckets[bucket_idx];

    while (entry) {
        if (entry->hash == hash && entry->key_len == key_len && memcmp(entry_key(entry), key, key_len) == 0) {
            break;
        }
        entry = entry->hash_next;
    }

    if (!entry) {
        rwlock_unlock_rd(&shard->lock);
        return false;
    }

    // Check expiry
    if (time(NULL) >= entry->expires_at) {
        rwlock_unlock_rd(&shard->lock);
        // Note: We could upgrade lock to delete here, but for simplicity
        // we let the next set() or background cleaner handle it,
        // or call invalidate() explicitly.
        cache_invalidate(cache, key);
        return false;
    }

    // Copy value
    *out_value = malloc(entry->value_len);
    if (*out_value) {
        memcpy(*out_value, entry_value(entry), entry->value_len);
        *out_len = entry->value_len;
    }

    // Lazy Promotion logic
    uint32_t accesses = ++entry->access_count;
    rwlock_unlock_rd(&shard->lock);

    // Only promote if threshold hit
    if (accesses >= CACHE_PROMOTION_THRESHOLD) {
        rwlock_wrlock(&shard->lock);
        // Re-verify entry still valid
        cache_entry_t* verify = shard->buckets[bucket_idx];
        bool still_valid      = false;
        while (verify) {
            if (verify == entry) {
                still_valid = true;
                break;
            }
            verify = verify->hash_next;
        }

        if (still_valid) {
            entry->access_count = 0;
            lru_remove(shard, entry);
            lru_add_to_front(shard, entry);
        }
        rwlock_unlock_wr(&shard->lock);
    }

    return *out_value != NULL;
}

bool cache_set(cache_t* cache, const char* key, const unsigned char* value, size_t value_len, uint32_t ttl_override) {
    if (!cache || !key || !value) return false;

    size_t key_len       = strlen(key);
    uint32_t hash        = hash_key(key, key_len);
    uint32_t shard_idx   = hash % CACHE_SHARD_COUNT;
    cache_shard_t* shard = &cache->shards[shard_idx];

    rwlock_wrlock(&shard->lock);

    // 1. Check if update existing
    size_t bucket_idx       = hash % shard->bucket_count;
    cache_entry_t* existing = shard->buckets[bucket_idx];

    while (existing) {
        if (existing->hash == hash && existing->key_len == key_len && memcmp(entry_key(existing), key, key_len) == 0) {

            // If size fits, overwrite. If not, free and realloc.
            // Simplified: We just remove old and add new to handle size changes easily.
            // Remove logic:
            cache_entry_t** slot = &shard->buckets[bucket_idx];
            while (*slot != existing)
                slot = &(*slot)->hash_next;
            *slot = existing->hash_next;
            lru_remove(shard, existing);
            free(existing);
            shard->size--;
            break;
        }
        existing = existing->hash_next;
    }

    // 2. Evict if full
    if (shard->size >= shard->capacity) {
        lru_evict(shard);  // Evicts tail (O(1))
    }

    // 3. Allocate Single Block
    // Size = Struct + Key + Null + Value
    size_t total_size    = sizeof(cache_entry_t) + key_len + 1 + value_len;
    cache_entry_t* entry = malloc(total_size);

    if (!entry) {
        rwlock_unlock_wr(&shard->lock);
        return false;
    }

    // Setup Entry
    entry->hash         = hash;
    entry->key_len      = key_len;
    entry->value_len    = value_len;
    entry->access_count = 0;
    entry->expires_at   = time(NULL) + (ttl_override ? ttl_override : cache->default_ttl);

    // Copy Key
    memcpy(entry->data, key, key_len);
    entry->data[key_len] = '\0';

    // Copy Value
    memcpy(entry->data + key_len + 1, value, value_len);

    // Link
    lru_add_to_front(shard, entry);

    entry->hash_next           = shard->buckets[bucket_idx];
    shard->buckets[bucket_idx] = entry;
    shard->size++;

    rwlock_unlock_wr(&shard->lock);
    return true;
}

void cache_invalidate(cache_t* cache, const char* key) {
    if (!cache || !key) return;

    size_t key_len       = strlen(key);
    uint32_t hash        = hash_key(key, key_len);
    uint32_t shard_idx   = hash % CACHE_SHARD_COUNT;
    cache_shard_t* shard = &cache->shards[shard_idx];

    rwlock_wrlock(&shard->lock);

    size_t bucket_idx    = hash % shard->bucket_count;
    cache_entry_t** slot = &shard->buckets[bucket_idx];

    while (*slot) {
        cache_entry_t* entry = *slot;
        if (entry->hash == hash && entry->key_len == key_len && memcmp(entry_key(entry), key, key_len) == 0) {

            *slot = entry->hash_next;
            lru_remove(shard, entry);
            free(entry);
            shard->size--;
            break;
        }
        slot = &entry->hash_next;
    }

    rwlock_unlock_wr(&shard->lock);
}

void cache_clear(cache_t* cache) {
    if (!cache) return;

    for (int i = 0; i < CACHE_SHARD_COUNT; i++) {
        cache_shard_t* shard = &cache->shards[i];
        rwlock_wrlock(&shard->lock);

        cache_entry_t* curr = shard->head;
        while (curr) {
            cache_entry_t* next = curr->next;
            free(curr);
            curr = next;
        }

        // Reset buckets
        memset(shard->buckets, 0, shard->bucket_count * sizeof(cache_entry_t*));
        shard->head = NULL;
        shard->tail = NULL;
        shard->size = 0;

        rwlock_unlock_wr(&shard->lock);
    }
}
