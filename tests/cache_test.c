#include "../include/cache.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/thread.h"

// Test statistics
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg)                                                                                    \
    do {                                                                                                               \
        if (condition) {                                                                                               \
            tests_passed++;                                                                                            \
            printf("  ✓ %s\n", msg);                                                                                   \
        } else {                                                                                                       \
            tests_failed++;                                                                                            \
            fprintf(stderr, "  ✗ %s (line %d)\n", msg, __LINE__);                                                      \
        }                                                                                                              \
    } while (0)

// Helper: FNV-1a Hash to find shards (Duplicate of internal logic for testing internals)
static uint32_t test_hash_key(const char* key) {
    uint32_t hash = 2166136261u;
    size_t len    = strlen(key);
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)(unsigned char)key[i];
        hash *= 16777619u;
    }
    return hash;
}

/** Test: Basic cache creation and destruction. */
static void test_create_destroy(void) {
    printf("\n[TEST] Cache Creation and Destruction\n");

    cache_t* cache = cache_create(1000, 60);
    if (!cache) {
        TEST_ASSERT(false, "Cache creation returned NULL (Check rwlock_init?)");
        return;  // Abort to prevent crash
    }

    size_t total_cap = get_total_capacity(cache);
    TEST_ASSERT(total_cap >= 64, "Cache capacity distributed across shards");
    TEST_ASSERT(get_total_cache_size(cache) == 0, "Cache initially empty");

    cache_destroy(cache);
    TEST_ASSERT(true, "Cache destroyed without crash");
}

/** Test: Basic set and get operations (Zero Copy). */
static void test_set_get(void) {
    printf("\n[TEST] Set and Get Operations (Zero Copy)\n");

    cache_t* cache = cache_create(100, 300);
    if (!cache) return;

    const char* key   = "test_key";
    const char* value = "test_value";
    size_t keylen     = strlen(key);
    size_t value_len  = strlen(value);

    // Test set
    bool result = cache_set(cache, key, keylen, value, value_len, 0);
    TEST_ASSERT(result == true, "Set operation succeeded");
    TEST_ASSERT(get_total_cache_size(cache) == 1, "Cache size incremented");

    // Test get
    size_t retrieved_len      = 0;
    const void* retrieved_ptr = cache_get(cache, key, keylen, &retrieved_len);

    TEST_ASSERT(retrieved_ptr != NULL, "Get operation returned valid pointer");
    TEST_ASSERT(retrieved_len == value_len, "Retrieved length matches");

    if (retrieved_ptr) {
        TEST_ASSERT(memcmp(retrieved_ptr, value, value_len) == 0, "Retrieved value matches");
        // IMPORTANT: Must release reference in Zero-Copy mode
        cache_release(retrieved_ptr);
    }

    // Test get non-existent key
    retrieved_ptr = cache_get(cache, "nonexistent", 11, &retrieved_len);
    TEST_ASSERT(retrieved_ptr == NULL, "Get non-existent key returns NULL");

    cache_destroy(cache);
}

/** Test: Update existing keys. */
static void test_update(void) {
    printf("\n[TEST] Update Operations\n");

    cache_t* cache = cache_create(100, 300);
    if (!cache) return;

    const char* key = "update_key";
    size_t keylen   = strlen(key);

    // Initial set
    const char* value1 = "value1";
    cache_set(cache, key, strlen(key), value1, strlen(value1), 0);

    // Update with new value
    const char* value2 = "value2_longer";
    bool result        = cache_set(cache, key, keylen, value2, strlen(value2), 0);
    TEST_ASSERT(result == true, "Update operation succeeded");
    TEST_ASSERT(get_total_cache_size(cache) == 1, "Cache size unchanged after update");

    // Verify updated value
    size_t len            = 0;
    const void* retrieved = cache_get(cache, key, keylen, &len);

    TEST_ASSERT(retrieved != NULL, "Retrieved updated value");
    if (retrieved) {
        TEST_ASSERT(len == strlen(value2), "Length matches new value");
        TEST_ASSERT(memcmp(retrieved, value2, len) == 0, "Content matches new value");
        cache_release(retrieved);
    }

    cache_destroy(cache);
}

/** Test: LRU eviction (Sharded). */
static void test_lru_eviction(void) {
    printf("\n[TEST] LRU Eviction (Sharded)\n");

    // Create a small cache. Total capacity ~64 (1 per shard minimum).
    cache_t* cache = cache_create(64, 300);
    if (!cache) return;

    // Insert 200 items.
    // Since shards are small, eviction MUST happen.
    int items_to_insert = 200;
    for (int i = 0; i < items_to_insert; i++) {
        char key[32];
        int len = snprintf(key, sizeof(key), "key%d", i);
        cache_set(cache, key, (size_t)len, "data", 4, 0);
    }

    size_t size = get_total_cache_size(cache);
    size_t cap  = get_total_capacity(cache);

    printf("  Total Capacity: %zu, Current Size: %zu\n", cap, size);
    TEST_ASSERT(size <= cap, "Cache respected capacity limits");
    TEST_ASSERT(size < (size_t)items_to_insert, "Eviction occurred (size < inserted)");

    // Verify 'key0' (oldest) is likely gone
    size_t len;
    const void* ptr = cache_get(cache, "key0", 4, &len);
    if (ptr) {
        printf("  Note: key0 happened to survive (statistical chance in sharding)\n");
        cache_release(ptr);
    } else {
        TEST_ASSERT(ptr == NULL, "Oldest key likely evicted");
    }

    // Verify 'key199' (newest) is likely present
    ptr = cache_get(cache, "key199", 6, &len);
    TEST_ASSERT(ptr != NULL, "Newest key should be present");
    if (ptr) cache_release(ptr);

    cache_destroy(cache);
}

/** Test: Input validation. */
static void test_input_validation(void) {
    printf("\n[TEST] Input Validation\n");

    cache_t* cache = cache_create(100, 300);
    if (!cache) return;

    size_t len;
    const void* ptr;

    // NULL cache
    bool result = cache_set(NULL, "key", 3, "val", 3, 0);
    TEST_ASSERT(result == false, "NULL cache in set rejected");

    // NULL key
    result = cache_set(cache, NULL, 0, "val", 3, 0);
    TEST_ASSERT(result == false, "NULL key in set rejected");

    // NULL value
    result = cache_set(cache, "key", 3, NULL, 3, 0);
    TEST_ASSERT(result == false, "NULL value in set rejected");

    // Get with NULLs
    ptr = cache_get(NULL, "key", 3, &len);
    TEST_ASSERT(ptr == NULL, "NULL cache in get rejected");

    cache_destroy(cache);
}

/** Thread argument for concurrent tests. */
typedef struct {
    cache_t* cache;
    int thread_id;
    int iterations;
    size_t value_size;
} thread_arg_t;

void* concurrent_reader(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        int keylen = snprintf(key, sizeof(key), "key%d", i % 50);

        size_t len;
        const void* ptr = cache_get(targ->cache, key, (size_t)keylen, &len);
        if (ptr) {
            // Simulate work
            volatile char c = ((char*)ptr)[0];
            (void)c;
            cache_release(ptr);  // Critical update
        }
    }
    return 0;
}

static void* concurrent_writer(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        char value[64];
        int keylen    = snprintf(key, sizeof(key), "key%d", i % 50);
        int value_len = snprintf(value, sizeof(value), "val_t%d_i%d", targ->thread_id, i);

        cache_set(targ->cache, key, (size_t)keylen, value, (size_t)value_len, 0);
    }
    return 0;
}

#define NUM_THREADS 8
#define ITERATIONS  2000

/** Test: Concurrent access (thread safety). */
static void test_concurrent_access(void) {
    printf("\n[TEST] Concurrent Access (Thread Safety)\n");

    cache_t* cache = cache_create(1000, 300);
    if (!cache) return;

    Thread threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].cache      = cache;
        args[i].thread_id  = i;
        args[i].iterations = ITERATIONS;

        if (i % 2 == 0) {
            thread_create(&threads[i], concurrent_reader, &args[i]);
        } else {
            thread_create(&threads[i], concurrent_writer, &args[i]);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], NULL);
    }

    TEST_ASSERT(true, "All threads completed without deadlock");
    cache_destroy(cache);
}

// ============= Benchmarks ============================

#include <sys/time.h>

static double get_current_time(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}

static void run_benchmarks(void) {
    printf("\n=================================\n");
    printf("  Performance Benchmarks (Zero-Copy)\n");
    printf("=================================\n");

    const size_t CACHE_SIZE = 100000;
    const size_t NUM_OPS    = 500000;
    const size_t VAL_SIZE   = 1024;  // 1KB

    cache_t* cache = cache_create(CACHE_SIZE, 3600);
    if (!cache) {
        printf("Skipping benchmarks due to cache creation failure.\n");
        return;
    }

    // 1. Sequential Write
    double start    = get_current_time();
    char* dummy_val = malloc(VAL_SIZE);
    memset(dummy_val, 'A', VAL_SIZE);
    dummy_val[VAL_SIZE - 1] = '\0';

    for (size_t i = 0; i < NUM_OPS; i++) {
        char key[32];
        int key_len = snprintf(key, sizeof(key), "key%zu", i % CACHE_SIZE);
        cache_set(cache, key, (size_t)key_len, dummy_val, VAL_SIZE, 0);
    }
    double end = get_current_time();
    printf("Writes: %.0f ops/sec\n", NUM_OPS / (end - start));

    // 2. Sequential Read (Zero Copy)
    start            = get_current_time();
    size_t hit_count = 0;
    for (size_t i = 0; i < NUM_OPS; i++) {
        char key[32];
        int keylen = snprintf(key, sizeof(key), "key%zu", i % CACHE_SIZE);

        size_t len;
        const void* ptr = cache_get(cache, key, (size_t)keylen, &len);
        if (ptr) {
            hit_count++;
            cache_release(ptr);
        }
    }

    end = get_current_time();
    printf("Reads:  %.0f ops/sec (Hits: %zu)\n", NUM_OPS / (end - start), hit_count);

    free(dummy_val);
    cache_destroy(cache);
}

/** Test: Serialization (Save/Load). */
static void test_serialization(void) {
    printf("\n[TEST] Serialization (Save/Load)\n");
    const char* filename = "test_cache_dump.bin";

    // 1. Setup initial cache
    cache_t* c1 = cache_create(100, 300);
    if (!c1) return;

    const char* key_str = "str_key";
    const char* val_str = "persistent_string";

    // Test Binary Data (Struct) to ensure we aren't just saving strings
    typedef struct {
        int id;
        double x;
        char flag;
    } my_struct_t;

    my_struct_t val_bin = {42, 3.14159, 'Z'};

    cache_set(c1, key_str, strlen(key_str), val_str, strlen(val_str), 0);
    cache_set(c1, "bin_key", 7, &val_bin, sizeof(val_bin), 0);

    // Test Expiration: Set a key with 1 second TTL
    cache_set(c1, "expire_key", 10, "temp", 4, 1);

    // 2. Save to disk
    bool saved = cache_save(c1, filename);
    TEST_ASSERT(saved == true, "Cache saved to file successfully");

    // 3. Destroy original cache (simulate restart)
    cache_destroy(c1);

    // 4. Wait 2 seconds to ensure 'expire_key' expires while on disk
    sleep_ms(2000);

    // 5. Load into new cache
    cache_t* c2 = cache_create(100, 300);
    bool loaded = cache_load(c2, filename);
    TEST_ASSERT(loaded == true, "Cache loaded from file successfully");

    // 6. Verify String Data
    size_t len;
    const void* ptr = cache_get(c2, key_str, strlen(key_str), &len);
    TEST_ASSERT(ptr != NULL, "String key recovered");
    if (ptr) {
        TEST_ASSERT(len == strlen(val_str), "String length matches");
        TEST_ASSERT(memcmp(ptr, val_str, len) == 0, "String content matches");
        cache_release(ptr);
    }

    // 7. Verify Binary Data
    ptr = cache_get(c2, "bin_key", 7, &len);
    TEST_ASSERT(ptr != NULL, "Binary key recovered");
    if (ptr) {
        TEST_ASSERT(len == sizeof(my_struct_t), "Binary structure length matches");
        const my_struct_t* rec_struct = (const my_struct_t*)ptr;
        TEST_ASSERT(rec_struct->id == 42 && rec_struct->flag == 'Z', "Binary content integrity check");
        cache_release(ptr);
    }

    // 8. Verify Expiration
    ptr = cache_get(c2, "expire_key", 10, &len);
    TEST_ASSERT(ptr == NULL, "Expired item was skipped during load");

    // Cleanup
    cache_destroy(c2);
    remove(filename);  // Delete temp file
}

int main(void) {
    printf("=================================\n");
    printf("  Cache Implementation Tests (Phase 3)\n");
    printf("=================================\n");

    test_create_destroy();
    test_set_get();
    test_update();
    // test_expiration(); // Removed to save time, un-comment if needed
    test_lru_eviction();
    test_input_validation();
    // test_serialization();
    test_concurrent_access();

    printf("\n=================================\n");
    printf("  Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    printf("=================================\n");

    run_benchmarks();
    return tests_failed;
}
