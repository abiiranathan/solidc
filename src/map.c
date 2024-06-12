#include "../include/map.h"

// Include lock.h for the cross-platform lock implementation
#include "../include/lock.h"

// Generic map implementation.
typedef struct map {
    entry* entries;                      // Map entries.
    size_t size;                         // number of map entries.
    size_t capacity;                     // map capacity.
    unsigned long (*hash)(const void*);  // Hash.
    key_compare_fn key_compare;          // Comparison function for map keys.
    Lock* lock;                          // Lock for thread safety.
} map;

map* map_create(size_t initial_capacity, key_compare_fn key_compare) {
    if (initial_capacity == 0) {
        initial_capacity = INITIAL_MAP_SIZE;
    }

    if (!key_compare) {
        puts("Key comparison function is required");
        return NULL;
    }

    map* m = (map*)malloc(sizeof(map));
    if (m == NULL) {
        perror("Failed to allocate memory for map");
        return NULL;
    }

    m->entries = (entry*)calloc(initial_capacity, sizeof(entry));
    if (m->entries == NULL) {
        free(m);
        perror("Failed to allocate memory for map entries");
        return NULL;
    }

    m->size = 0;
    m->capacity = initial_capacity;
    m->hash = murmur_hash;  // Default hash function
    m->key_compare = key_compare;

    // Initialize the lock
    m->lock = malloc(sizeof(Lock));
    if (m->lock == NULL) {
        free(m->entries);
        free(m);
        perror("Failed to allocate memory for map lock");
        return NULL;
    }

    lock_init(m->lock);
    return m;
}

void map_destroy(map* m, bool free_entries) {
    if (!m)
        return;

    if (free_entries) {
        // Free the keys and values
        for (size_t i = 0; i < m->capacity; i++) {
            if (m->entries[i].key != NULL) {
                free((void*)m->entries[i].key);
            }

            if (m->entries[i].value != NULL) {
                free((void*)m->entries[i].value);
            }
        }
    }

    // Free the entries
    free(m->entries);

    lock_free(m->lock);
    free(m->lock);
    free(m);
    m = NULL;
}

// Resize the map to a new capacity
// No need to lock this function because it is called from a locked function
static bool map_resize(map* m, size_t new_capacity) {
    entry* new_entries = (entry*)calloc(new_capacity, sizeof(entry));
    if (new_entries == NULL) {
        perror("Failed to allocate memory for new map entries");
        return false;
    }

    // Rehash the old entries into the new entries
    for (size_t i = 0; i < m->size; i++) {
        // Ignore empty slots
        if (m->entries[i].key != NULL) {
            size_t index = m->hash(m->entries[i].key) % new_capacity;

            while (new_entries[index].key != NULL) {
                index = (index + 1) % new_capacity;
            }

            new_entries[index].key = m->entries[i].key;
            new_entries[index].value = m->entries[i].value;
        }
    }

    // Free the old entries and update the map
    free(m->entries);

    m->entries = new_entries;
    m->capacity = new_capacity;

    return true;
}

void map_set(map* m, const void* key, const void* value) {
    if ((double)m->size / m->capacity > LOAD_FACTOR_THRESHOLD) {
        // Check for potential integer overflow
        if (m->capacity > SIZE_MAX / 2 || !map_resize(m, m->capacity * 2)) {
            fprintf(stderr, "Integer overflow or out of memory\n");
            return;
        }
    }

    // Find the index to insert the new entry
    size_t index = m->hash(key) % m->capacity;

    // Linear probing to find the next available index
    while (m->entries[index].key != NULL) {
        if (m->key_compare(m->entries[index].key, key)) {
            m->entries[index].value = value;
            return;  // Key already exists with the same value
        }
        // Key collision, continue probing
        index = (index + 1) % m->capacity;
    }

    m->entries[index].key = key;
    m->entries[index].value = value;
    m->size++;
}

// Thread safe set
void map_set_safe(map* m, const void* key, const void* value) {
    lock_acquire(m->lock);
    map_set(m, key, value);
    lock_release(m->lock);
}

// Set multiple key-value pairs in the map from arrays
void map_set_from_array(map* m, const void** keys, const void** values, size_t num_keys) {
    // lock the map
    lock_acquire(m->lock);
    for (size_t i = 0; i < num_keys; i++) {
        map_set(m, keys[i], values[i]);
    }
    // unlock the map
    lock_release(m->lock);
}

const void* map_get(map* m, const void* key) {
    size_t index = m->hash(key) % m->capacity;

    while (m->entries[index].key != NULL) {
        if (m->key_compare(m->entries[index].key, key)) {
            return m->entries[index].value;
        }
        index = (index + 1) % m->capacity;
    }
    return NULL;
}

// Thread safe get
const void* map_get_safe(map* m, const void* key) {
    lock_acquire(m->lock);
    const void* value = map_get(m, key);
    lock_release(m->lock);
    return value;
}

void map_remove(map* m, const void* key) {
    size_t index = m->hash(key) % m->capacity;
    while (m->entries[index].key != NULL) {
        if (m->key_compare(m->entries[index].key, key)) {
            m->entries[index].key = NULL;
            m->entries[index].value = NULL;
            m->size--;
            return;
        }
        index = (index + 1) % m->capacity;
    }
}

// Thread safe remove
void map_remove_safe(map* m, const void* key) {
    lock_acquire(m->lock);
    map_remove(m, key);
    lock_release(m->lock);
}

size_t map_length(map* m) {
    return m->size;
}

size_t map_capacity(map* m) {
    return m->capacity;
}

entry* map_get_entries(map* m) {
    return m->entries;
}

void map_set_hash(map* m, unsigned long (*hash)(const void*)) {
    m->hash = hash;
}

// Function pointer for comparing 2 integers
bool key_compare_int(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return *(int*)a == *(int*)b;
}

// compares char* for equality.
bool key_compare_char_ptr(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return strcmp((char*)a, (char*)b) == 0;
}

// compares floats for equality
bool key_compare_float(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return *(float*)a == *(float*)b;
}

// compares doubles for equality.
bool key_compare_double(const void* a, const void* b) {
    if (a == NULL || b == NULL)
        return false;
    return *(double*)a == *(double*)b;
}

// hash functions

// DJB2 Has function.
// http://www.cse.yorku.ca/~oz/hash.html
unsigned long djb2_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != '\0') {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

// Simple DataBase Manager hash
// http://www.cse.yorku.ca/~oz/hash.html
unsigned long sdbm_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 0;
    int c;
    while ((c = *str++) != '\0') {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

static inline uint32_t murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

/**
* https://en.wikipedia.org/wiki/MurmurHash
* MurmurHash3 was written by Austin Appleby in 2008, and is placed in the public domain.
*/
uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    uint32_t k;

    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        // Here is a source of differing results across endiannesses.
        // A swap here has no effects on hash properties though.
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }

    // A swap is *not* necessary here because the preceding loop already
    // places the low bytes in the low places according to whatever endianness
    // we use. Swaps only apply when the memory is copied in a chunk.
    h ^= murmur_32_scramble(k);
    /* Finalize. */
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

// For large datasets, this is very good
// compared to the default jdb2 hash(fails with map size of 10000)
unsigned long murmur_hash(const void* key) {
    uint32_t seed = 0xDEADBEEF;
    uint32_t h = murmur3_32((uint8_t*)key, sizeof(int), seed);
    return h;
}
