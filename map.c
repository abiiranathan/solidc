#include <limits.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include xxHash for faster hashing
#define XXH_INLINE_ALL
#include <xxhash.h>

// Define initial map size and load factor threshold
#define INITIAL_MAP_SIZE 16
#define LOAD_FACTOR_THRESHOLD 0.75

// Define alignment for cache line optimization
#define CACHE_LINE_SIZE 64

// Entry structure with cache line alignment
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) entry {
    const void* key;
    void* value;
} entry;

// Map structure
typedef struct map {
    entry* entries;                                 // Map entries
    uint8_t* deleted_bitmap;                        // Bitmap for deleted entries
    size_t size;                                    // Number of map entries
    size_t capacity;                                // Map capacity
    unsigned long (*hash)(const void*);             // Hash function
    bool (*key_compare)(const void*, const void*);  // Key comparison function
    pthread_mutex_t lock;                           // Lock for thread safety
    bool free_entries;                              // Free keys and values (if heap-allocated)
} map;

// Function prototypes
map* map_create(size_t initial_capacity, bool (*key_compare)(const void*, const void*),
                bool free_entries);

void map_destroy(map* m);
bool map_resize(map* m, size_t new_capacity);
void map_set(map* m, void* key, void* value);
void* map_get(map* m, void* key);
void map_remove(map* m, void* key);
size_t map_length(map* m);
size_t map_capacity(map* m);

// Thread-safe versions
void map_set_safe(map* m, void* key, void* value);
void* map_get_safe(map* m, void* key);
void map_remove_safe(map* m, void* key);

// Key comparison functions
bool key_compare_int(const void* a, const void* b);
bool key_compare_char_ptr(const void* a, const void* b);

// Hash functions
unsigned long xxhash(const void* key);

// Map creation
map* map_create(size_t initial_capacity, bool (*key_compare)(const void*, const void*),
                bool free_entries) {
    if (initial_capacity == 0) {
        initial_capacity = INITIAL_MAP_SIZE;
    }

    if (!key_compare) {
        fprintf(stderr, "Key comparison function is required\n");
        return NULL;
    }

    map* m = (map*)malloc(sizeof(map));
    if (!m) {
        perror("Failed to allocate memory for map");
        return NULL;
    }

    m->entries = (entry*)calloc(initial_capacity, sizeof(entry));
    if (!m->entries) {
        free(m);
        perror("Failed to allocate memory for map entries");
        return NULL;
    }

    m->deleted_bitmap = (uint8_t*)calloc((initial_capacity + 7) / 8, sizeof(uint8_t));
    if (!m->deleted_bitmap) {
        free(m->entries);
        free(m);
        perror("Failed to allocate memory for deleted bitmap");
        return NULL;
    }

    m->size         = 0;
    m->capacity     = initial_capacity;
    m->hash         = xxhash;  // Use xxHash for faster hashing
    m->key_compare  = key_compare;
    m->free_entries = free_entries;

    pthread_mutex_init(&m->lock, NULL);
    return m;
}

// Map destruction
void map_destroy(map* m) {
    if (!m)
        return;

    if (m->free_entries) {
        for (size_t i = 0; i < m->capacity; i++) {
            if (m->entries[i].key)
                free((void*)m->entries[i].key);
            if (m->entries[i].value)
                free(m->entries[i].value);
        }
    }

    free(m->entries);
    free(m->deleted_bitmap);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

// Resize the map
bool map_resize(map* m, size_t new_capacity) {
    entry* new_entries = (entry*)calloc(new_capacity, sizeof(entry));
    if (!new_entries) {
        perror("Failed to allocate memory for new map entries");
        return false;
    }

    uint8_t* new_deleted_bitmap = (uint8_t*)calloc((new_capacity + 7) / 8, sizeof(uint8_t));
    if (!new_deleted_bitmap) {
        free(new_entries);
        perror("Failed to allocate memory for new deleted bitmap");
        return false;
    }

    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].key && !(m->deleted_bitmap[i / 8] & (1 << (i % 8)))) {
            size_t index = m->hash(m->entries[i].key) % new_capacity;
            size_t j     = 0;
            while (new_entries[index].key) {
                j++;
                index = (index + j * j) % new_capacity;  // Quadratic probing
            }
            new_entries[index] = m->entries[i];
        }
    }

    free(m->entries);
    free(m->deleted_bitmap);

    m->entries        = new_entries;
    m->deleted_bitmap = new_deleted_bitmap;
    m->capacity       = new_capacity;

    return true;
}

// Set a key-value pair
void map_set(map* m, void* key, void* value) {
    if ((double)m->size / (double)m->capacity > LOAD_FACTOR_THRESHOLD) {
        if (m->capacity > SIZE_MAX / 2 || !map_resize(m, m->capacity * 2)) {
            fprintf(stderr, "Integer overflow or out of memory\n");
            return;
        }
    }

    size_t index = m->hash(key) % m->capacity;
    size_t i     = 0;
    while (m->entries[index].key && !m->key_compare(m->entries[index].key, key)) {
        i++;
        index = (index + i * i) % m->capacity;  // Quadratic probing
    }

    if (!m->entries[index].key)
        m->size++;
    m->entries[index].key   = key;
    m->entries[index].value = value;
    m->deleted_bitmap[index / 8] &= ~(1 << (index % 8));  // Clear deleted flag
}

// Get a value by key
void* map_get(map* m, void* key) {
    size_t index = m->hash(key) % m->capacity;
    size_t i     = 0;
    while (m->entries[index].key || (m->deleted_bitmap[index / 8] & (1 << (index % 8)))) {
        if (m->entries[index].key && m->key_compare(m->entries[index].key, key)) {
            return m->entries[index].value;
        }
        i++;
        index = (index + i * i) % m->capacity;  // Quadratic probing
    }
    return NULL;
}

size_t map_length(map* m) {
    return m->size;
}

size_t map_capacity(map* m) {
    return m->capacity;
}

// Remove a key-value pair
void map_remove(map* m, void* key) {
    size_t index = m->hash(key) % m->capacity;
    size_t i     = 0;

    while (m->entries[index].key || (m->deleted_bitmap[index / 8] & (1 << (index % 8)))) {
        if (m->entries[index].key && m->key_compare(m->entries[index].key, key)) {
            if (m->free_entries) {
                free((void*)m->entries[index].key);
                free((void*)m->entries[index].value);
            }

            m->entries[index].key   = NULL;
            m->entries[index].value = NULL;
            m->deleted_bitmap[index / 8] |= (1 << (index % 8));  // Mark as deleted
            m->size--;
            return;
        }
        i++;
        index = (index + i * i) % m->capacity;  // Quadratic probing
    }
}

// Thread-safe operations
void map_set_safe(map* m, void* key, void* value) {
    pthread_mutex_lock(&m->lock);
    map_set(m, key, value);
    pthread_mutex_unlock(&m->lock);
}

void* map_get_safe(map* m, void* key) {
    pthread_mutex_lock(&m->lock);
    void* value = map_get(m, key);
    pthread_mutex_unlock(&m->lock);
    return value;
}

void map_remove_safe(map* m, void* key) {
    pthread_mutex_lock(&m->lock);
    map_remove(m, key);
    pthread_mutex_unlock(&m->lock);
}

// Key comparison functions
bool key_compare_int(const void* a, const void* b) {
    return a && b && *(int*)a == *(int*)b;
}

bool key_compare_char_ptr(const void* a, const void* b) {
    return a && b && strcmp((char*)a, (char*)b) == 0;
}

// xxHash implementation
unsigned long xxhash(const void* key) {
    return XXH64(key, sizeof(int), 0);
}

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 4
#define NUM_ENTRIES 64

// Shared map
map* shared_map;

// Worker function for inserting elements
void* insert_worker(void* arg) {
    int thread_id = *(int*)arg;
    for (int i = thread_id * NUM_ENTRIES; i < (thread_id + 1) * NUM_ENTRIES; i++) {
        int* key   = malloc(sizeof(int));
        int* value = malloc(sizeof(int));
        *key       = i;
        *value     = i * 10;
        map_set_safe(shared_map, key, value);
    }
    return NULL;
}

// Worker function for retrieving elements
void* retrieve_worker(void* arg) {
    int thread_id = *(int*)arg;
    for (int i = thread_id * NUM_ENTRIES; i < (thread_id + 1) * NUM_ENTRIES; i++) {
        int key    = i;
        int* value = (int*)map_get_safe(shared_map, &key);
        if (value) {
            printf("Thread %d: Key %d => Value %d\n", thread_id, key, *value);
        }
    }
    return NULL;
}

// Worker function for removing elements
void* remove_worker(void* arg) {
    int thread_id = *(int*)arg;
    for (int i = thread_id * NUM_ENTRIES; i < (thread_id + 1) * NUM_ENTRIES; i++) {
        int key = i;
        map_remove_safe(shared_map, &key);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Create the shared map
    shared_map = map_create(128, key_compare_int, true);
    if (!shared_map) {
        fprintf(stderr, "Failed to create map\n");
        return EXIT_FAILURE;
    }

    // Insert elements concurrently
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, insert_worker, &thread_ids[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Insertion complete. Map size: %zu\n", map_length(shared_map));

    // Retrieve elements concurrently
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, retrieve_worker, &thread_ids[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Remove elements concurrently
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, remove_worker, &thread_ids[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Removal complete. Map size: %zu\n", map_length(shared_map));

    // Destroy the map
    map_destroy(shared_map);
    return EXIT_SUCCESS;
}
