#include "cache.h"
#include <stdio.h>

/** Simple hash function (FNV-1a). */
static inline uint32_t hash_key(const char* key) {
    uint32_t hash = 2166136261u;
    for (const char* p = key; *p != '\0'; p++) {
        hash ^= (uint32_t)(unsigned char)(*p);
        hash *= 16777619u;
    }
    return hash;
}

/** Removes an entry from the LRU list. */
static inline void lru_remove(cache_t* cache, cache_entry_t* entry) {
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        cache->head = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cache->tail = entry->prev;
    }

    entry->prev = NULL;
    entry->next = NULL;
}

/** Moves an entry to the front of the LRU list. */
static inline void lru_move_to_front(cache_t* cache, cache_entry_t* entry) {
    if (cache->head == entry) {
        return;  // Already at front
    }

    lru_remove(cache, entry);

    entry->next = cache->head;
    entry->prev = NULL;

    if (cache->head) {
        cache->head->prev = entry;
    }
    cache->head = entry;

    if (!cache->tail) {
        cache->tail = entry;
    }
}

/** Adds an entry to the front of the LRU list. */
static inline void lru_add_to_front(cache_t* cache, cache_entry_t* entry) {
    entry->next = cache->head;
    entry->prev = NULL;

    if (cache->head) {
        cache->head->prev = entry;
    }
    cache->head = entry;

    if (!cache->tail) {
        cache->tail = entry;
    }
}

/** Frees a cache entry and its resources. */
static inline void cache_entry_free(cache_entry_t* entry) {
    if (!entry) {
        return;
    }
    free(entry->value);
    free(entry);
}

/** Evicts the least recently used entry.
Must be called with write lock held. */
static void evict_lru(cache_t* cache) {
    if (!cache->tail) {
        return;
    }

    cache_entry_t* victim = cache->tail;

    // Debug: print which key is being evicted
    // printf("Evicting: %s (LRU)\n", victim->key);

    // Remove from hash table
    uint32_t hash_val = hash_key(victim->key);
    size_t bucket_idx = hash_val % cache->bucket_count;

    cache_entry_t** slot = &cache->buckets[bucket_idx];
    while (*slot && *slot != victim) {
        slot = &(*slot)->hash_next;
    }
    if (*slot) {
        *slot = victim->hash_next;
    }

    // Remove from LRU list
    lru_remove(cache, victim);

    cache_entry_free(victim);
    cache->size--;
}

cache_t* cache_create(size_t capacity, uint32_t default_ttl) {
    cache_t* cache = malloc(sizeof(*cache));
    if (!cache) {
        return NULL;
    }

    *cache = (cache_t){0};

    cache->capacity    = capacity > 0 ? capacity : CACHE_DEFAULT_CAPACITY;
    cache->default_ttl = default_ttl > 0 ? default_ttl : CACHE_DEFAULT_TTL;

    // Use a prime number of buckets for better distribution
    cache->bucket_count = cache->capacity * 2 + 1;
    cache->buckets      = calloc(cache->bucket_count, sizeof(cache_entry_t*));
    if (!cache->buckets) {
        free(cache);
        return NULL;
    }

#ifdef _WIN32
    rwlock_init(&cache->lock);
#else
    if (rwlock_init(&cache->lock) != 0) {
        free(cache->buckets);
        free(cache);
        return NULL;
    }
#endif

    return cache;
}

void cache_destroy(cache_t* cache) {
    if (!cache) {
        return;
    }

    rwlock_wrlock(&cache->lock);

    cache_entry_t* current = cache->head;
    while (current) {
        cache_entry_t* next = current->next;
        cache_entry_free(current);
        current = next;
    }

    free(cache->buckets);
    rwlock_unlock_wr(&cache->lock);
    rwlock_destroy(&cache->lock);
    free(cache);
}

bool cache_get(cache_t* cache, const char* key, char** out_value, size_t* out_len) {
    if (!cache || !key || !out_value || !out_len) {
        return false;
    }

    // Fast path: Read lock for lookup
    rwlock_rdlock(&cache->lock);

    uint32_t hash_val = hash_key(key);
    size_t bucket_idx = hash_val % cache->bucket_count;

    cache_entry_t* entry = cache->buckets[bucket_idx];
    cache_entry_t* found = NULL;

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            found = entry;
            break;
        }
        entry = entry->hash_next;
    }

    if (!found) {
        rwlock_unlock_rd(&cache->lock);
        return false;
    }

    // Check expiration (without holding write lock yet)
    time_t now = time(NULL);
    if (now >= found->expires_at) {
        rwlock_unlock_rd(&cache->lock);
        // Expired - remove it (requires write lock)
        cache_invalidate(cache, key);
        return false;
    }

    // Duplicate value for caller
    *out_value = malloc(found->value_len);
    if (!*out_value) {
        rwlock_unlock_rd(&cache->lock);
        return false;
    }
    memcpy(*out_value, found->value, found->value_len);
    *out_len = found->value_len;

    // Update access count and check if we should promote
    uint32_t access_count = ++found->access_count;

    rwlock_unlock_rd(&cache->lock);

    // Promote if accessed frequently (requires write lock)
    if (access_count >= CACHE_PROMOTION_THRESHOLD) {
        rwlock_wrlock(&cache->lock);

        // Re-verify the entry still exists and hasn't changed
        cache_entry_t* verify = cache->buckets[bucket_idx];
        bool still_exists     = false;
        while (verify) {
            if (verify == found && strcmp(verify->key, key) == 0) {
                still_exists = true;
                break;
            }
            verify = verify->hash_next;
        }

        if (still_exists) {
            found->access_count = 0;  // Reset counter
            lru_move_to_front(cache, found);
        }

        rwlock_unlock_wr(&cache->lock);
    }

    return true;
}

bool cache_set(cache_t* cache, const char* key, const unsigned char* value, size_t value_len,
               uint32_t ttl_override) {
    if (!cache || !key || !value || key[0] == '\0') {
        return false;
    }

    // Check key length
    size_t key_len = strlen(key);
    if (key_len >= CACHE_KEY_MAX_LEN) {
        return false;
    }

    rwlock_wrlock(&cache->lock);

    uint32_t hash_val = hash_key(key);
    size_t bucket_idx = hash_val % cache->bucket_count;

    // Check if key already exists
    cache_entry_t* existing = cache->buckets[bucket_idx];
    while (existing) {
        if (strcmp(existing->key, key) == 0) {
            // Update existing entry
            unsigned char* new_value = malloc(value_len);
            if (!new_value) {
                rwlock_unlock_wr(&cache->lock);
                return false;
            }
            memcpy(new_value, value, value_len);

            free(existing->value);
            existing->value        = new_value;
            existing->value_len    = value_len;
            existing->access_count = 0;

            uint32_t ttl         = ttl_override > 0 ? ttl_override : cache->default_ttl;
            existing->expires_at = time(NULL) + ttl;

            lru_move_to_front(cache, existing);

            rwlock_unlock_wr(&cache->lock);
            return true;
        }
        existing = existing->hash_next;
    }

    // Evict if at capacity
    if (cache->size >= cache->capacity) {
        // Before evicting, reset all access counts to ensure pure LRU eviction
        // This prevents the lazy promotion mechanism from interfering with eviction
        cache_entry_t* entry = cache->head;
        while (entry) {
            entry->access_count = 0;
            entry               = entry->next;
        }
        evict_lru(cache);
    }

    // Create new entry
    cache_entry_t* entry = malloc(sizeof(*entry));
    if (!entry) {
        rwlock_unlock_wr(&cache->lock);
        return false;
    }

    *entry = (cache_entry_t){0};
    strncpy(entry->key, key, CACHE_KEY_MAX_LEN - 1);
    entry->key[CACHE_KEY_MAX_LEN - 1] = '\0';

    entry->value = malloc(value_len);
    if (!entry->value) {
        free(entry);
        rwlock_unlock_wr(&cache->lock);
        return false;
    }
    memcpy(entry->value, value, value_len);
    entry->value_len    = value_len;
    entry->access_count = 0;

    uint32_t ttl      = ttl_override > 0 ? ttl_override : cache->default_ttl;
    entry->expires_at = time(NULL) + ttl;

    // Add to hash table
    entry->hash_next           = cache->buckets[bucket_idx];
    cache->buckets[bucket_idx] = entry;

    // Add to LRU list
    lru_add_to_front(cache, entry);

    cache->size++;

    rwlock_unlock_wr(&cache->lock);
    return true;
}

void cache_invalidate(cache_t* cache, const char* key) {
    if (!cache || !key) {
        return;
    }

    rwlock_wrlock(&cache->lock);

    uint32_t hash_val = hash_key(key);
    size_t bucket_idx = hash_val % cache->bucket_count;

    cache_entry_t** slot = &cache->buckets[bucket_idx];
    while (*slot) {
        if (strcmp((*slot)->key, key) == 0) {
            cache_entry_t* victim = *slot;
            *slot                 = victim->hash_next;
            lru_remove(cache, victim);
            cache_entry_free(victim);
            cache->size--;
            break;
        }
        slot = &(*slot)->hash_next;
    }

    rwlock_unlock_wr(&cache->lock);
}

void cache_clear(cache_t* cache) {
    if (!cache) {
        return;
    }

    rwlock_wrlock(&cache->lock);

    cache_entry_t* current = cache->head;
    while (current) {
        cache_entry_t* next = current->next;
        cache_entry_free(current);
        current = next;
    }

    // Clear hash table buckets
    for (size_t i = 0; i < cache->bucket_count; i++) {
        cache->buckets[i] = NULL;
    }

    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;

    rwlock_unlock_wr(&cache->lock);
}
