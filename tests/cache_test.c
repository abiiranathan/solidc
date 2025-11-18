#include "../include/cache.h"
#include <assert.h>  // for assert
#include <stdio.h>   // for printf, fprintf
#include <string.h>  // for strcmp, memcmp
#include <unistd.h>  // for sleep (POSIX) or windows.h for Sleep

#ifdef _WIN32
#include <windows.h>
#define sleep_seconds(n) Sleep((n) * 1000)
#define thread_t         HANDLE
#define thread_create(thread, func, arg)                                                           \
    (*(thread) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(func), (arg), 0, NULL))
#define thread_join(thread) WaitForSingleObject((thread), INFINITE)
#define thread_return_t     DWORD WINAPI
#else
#include <pthread.h>
#define sleep_seconds(n)                 sleep(n)
#define thread_t                         pthread_t
#define thread_create(thread, func, arg) pthread_create((thread), NULL, (func), (arg))
#define thread_join(thread)              pthread_join((thread), NULL)
#define thread_return_t                  void*
#endif

// Test statistics
static int tests_passed = 0;
static int tests_failed = 0;

/** Macro for test assertions with detailed output. */
#define TEST_ASSERT(condition, msg)                                                                \
    do {                                                                                           \
        if (condition) {                                                                           \
            tests_passed++;                                                                        \
            printf("  ✓ %s\n", msg);                                                               \
        } else {                                                                                   \
            tests_failed++;                                                                        \
            fprintf(stderr, "  ✗ %s (line %d)\n", msg, __LINE__);                                  \
        }                                                                                          \
    } while (0)

/** Test: Basic cache creation and destruction. */
static void test_create_destroy(void) {
    printf("\n[TEST] Cache Creation and Destruction\n");

    cache_t* cache = cache_create(100, 60);
    TEST_ASSERT(cache != NULL, "Cache created successfully");
    TEST_ASSERT(cache->capacity == 100, "Cache capacity set correctly");
    TEST_ASSERT(cache->default_ttl == 60, "Cache default TTL set correctly");
    TEST_ASSERT(cache->size == 0, "Cache initially empty");

    cache_destroy(cache);
    TEST_ASSERT(true, "Cache destroyed without crash");

    // Test with default values
    cache = cache_create(0, 0);
    TEST_ASSERT(cache != NULL, "Cache created with defaults");
    TEST_ASSERT(cache->capacity == CACHE_DEFAULT_CAPACITY, "Default capacity used");
    TEST_ASSERT(cache->default_ttl == CACHE_DEFAULT_TTL, "Default TTL used");
    cache_destroy(cache);

    // Test NULL safety
    cache_destroy(NULL);
    TEST_ASSERT(true, "NULL cache_destroy handled safely");
}

/** Test: Basic set and get operations. */
static void test_set_get(void) {
    printf("\n[TEST] Set and Get Operations\n");

    cache_t* cache = cache_create(10, 300);
    TEST_ASSERT(cache != NULL, "Cache created");

    const char* key            = "test_key";
    const unsigned char* value = (const unsigned char*)"test_value";
    size_t value_len           = strlen((const char*)value);

    // Test set
    bool result = cache_set(cache, key, value, value_len, 0);
    TEST_ASSERT(result == true, "Set operation succeeded");
    TEST_ASSERT(cache->size == 1, "Cache size incremented");

    // Test get
    char* retrieved      = NULL;
    size_t retrieved_len = 0;
    result               = cache_get(cache, key, &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "Get operation succeeded");
    TEST_ASSERT(retrieved != NULL, "Retrieved value is not NULL");
    TEST_ASSERT(retrieved_len == value_len, "Retrieved length matches");
    TEST_ASSERT(memcmp(retrieved, value, value_len) == 0, "Retrieved value matches");
    free(retrieved);

    // Test get non-existent key
    result = cache_get(cache, "nonexistent", &retrieved, &retrieved_len);
    TEST_ASSERT(result == false, "Get non-existent key returns false");

    // Test binary data (with null bytes)
    const unsigned char binary_data[] = {0x00, 0x01, 0x02, 0x00, 0xFF, 0xAB};
    size_t binary_len                 = sizeof(binary_data);
    result                            = cache_set(cache, "binary_key", binary_data, binary_len, 0);
    TEST_ASSERT(result == true, "Binary data set succeeded");

    result = cache_get(cache, "binary_key", &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "Binary data get succeeded");
    TEST_ASSERT(retrieved_len == binary_len, "Binary data length matches");
    TEST_ASSERT(memcmp(retrieved, binary_data, binary_len) == 0, "Binary data matches");
    free(retrieved);

    cache_destroy(cache);
}

/** Test: Update existing keys. */
static void test_update(void) {
    printf("\n[TEST] Update Operations\n");

    cache_t* cache  = cache_create(10, 300);
    const char* key = "update_key";

    // Initial set
    const unsigned char* value1 = (const unsigned char*)"value1";
    cache_set(cache, key, value1, strlen((const char*)value1), 0);

    // Update with new value
    const unsigned char* value2 = (const unsigned char*)"value2_longer";
    bool result                 = cache_set(cache, key, value2, strlen((const char*)value2), 0);
    TEST_ASSERT(result == true, "Update operation succeeded");
    TEST_ASSERT(cache->size == 1, "Cache size unchanged after update");

    // Verify updated value
    char* retrieved      = NULL;
    size_t retrieved_len = 0;
    cache_get(cache, key, &retrieved, &retrieved_len);
    TEST_ASSERT(memcmp(retrieved, value2, strlen((const char*)value2)) == 0,
                "Updated value retrieved");
    free(retrieved);

    cache_destroy(cache);
}

/** Test: TTL expiration. */
static void test_expiration(void) {
    printf("\n[TEST] TTL Expiration\n");

    cache_t* cache = cache_create(10, 2);  // 2 second default TTL

    const char* key            = "expiring_key";
    const unsigned char* value = (const unsigned char*)"expiring_value";
    cache_set(cache, key, value, strlen((const char*)value), 0);

    // Should exist immediately
    char* retrieved      = NULL;
    size_t retrieved_len = 0;
    bool result          = cache_get(cache, key, &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "Key exists before expiration");
    free(retrieved);

    // Wait for expiration
    printf("  (waiting 3 seconds for expiration...)\n");
    sleep_seconds(3);

    // Should be expired now
    result = cache_get(cache, key, &retrieved, &retrieved_len);
    TEST_ASSERT(result == false, "Key expired after TTL");
    TEST_ASSERT(cache->size == 0, "Expired entry removed from cache");

    // Test custom TTL override
    cache_set(cache, "custom_ttl", value, strlen((const char*)value), 10);
    result = cache_get(cache, "custom_ttl", &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "Custom TTL key exists");
    free(retrieved);

    cache_destroy(cache);
}

/** Test: LRU eviction. */
static void test_lru_eviction(void) {
    printf("\n[TEST] LRU Eviction\n");

    cache_t* cache = cache_create(3, 300);  // Capacity of 3

    // Fill cache to capacity in specific order
    cache_set(cache, "key1", (const unsigned char*)"value1", 6, 0);
    cache_set(cache, "key2", (const unsigned char*)"value2", 6, 0);
    cache_set(cache, "key3", (const unsigned char*)"value3", 6, 0);
    TEST_ASSERT(cache->size == 3, "Cache filled to capacity");

    // Access pattern: key1, key2, key3 (key1 is LRU if we don't access it again)
    // But since we just added them, key3 is MRU, key1 is LRU

    // Let's explicitly access key2 and key3 to make key1 the true LRU
    char* retrieved      = NULL;
    size_t retrieved_len = 0;

    // Access key2
    cache_get(cache, "key2", &retrieved, &retrieved_len);
    free(retrieved);

    // Access key3
    cache_get(cache, "key3", &retrieved, &retrieved_len);
    free(retrieved);

    // Now key1 should be LRU (least recently accessed)

    // Add new key - should evict key1 (least recently used)
    cache_set(cache, "key4", (const unsigned char*)"value4", 6, 0);
    TEST_ASSERT(cache->size == 3, "Cache size maintained after eviction");

    // key1 should be evicted (was LRU)
    bool result = cache_get(cache, "key1", &retrieved, &retrieved_len);
    TEST_ASSERT(result == false, "LRU entry (key1) was evicted");

    // Other keys should still exist
    result = cache_get(cache, "key2", &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "key2 still exists");
    free(retrieved);

    result = cache_get(cache, "key3", &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "key3 still exists");
    free(retrieved);

    result = cache_get(cache, "key4", &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "Newly added key4 exists");
    free(retrieved);

    cache_destroy(cache);
}

/** Test: Cache invalidation. */
static void test_invalidation(void) {
    printf("\n[TEST] Cache Invalidation\n");

    cache_t* cache = cache_create(10, 300);

    // Add entries
    cache_set(cache, "key1", (const unsigned char*)"value1", 6, 0);
    cache_set(cache, "key2", (const unsigned char*)"value2", 6, 0);
    TEST_ASSERT(cache->size == 2, "Two entries added");

    // Invalidate one
    cache_invalidate(cache, "key1");
    TEST_ASSERT(cache->size == 1, "Size decremented after invalidation");

    char* retrieved      = NULL;
    size_t retrieved_len = 0;
    bool result          = cache_get(cache, "key1", &retrieved, &retrieved_len);
    TEST_ASSERT(result == false, "Invalidated key not found");

    result = cache_get(cache, "key2", &retrieved, &retrieved_len);
    TEST_ASSERT(result == true, "Other key still exists");
    free(retrieved);

    // Test invalidating non-existent key (should not crash)
    cache_invalidate(cache, "nonexistent");
    TEST_ASSERT(cache->size == 1, "Invalidating non-existent key handled safely");

    // Test NULL safety
    cache_invalidate(NULL, "key");
    cache_invalidate(cache, NULL);
    TEST_ASSERT(true, "NULL invalidation handled safely");

    cache_destroy(cache);
}

/** Test: Cache clear. */
static void test_clear(void) {
    printf("\n[TEST] Cache Clear\n");

    cache_t* cache = cache_create(10, 300);

    // Add multiple entries
    for (int i = 0; i < 5; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        cache_set(cache, key, (const unsigned char*)"value", 5, 0);
    }
    TEST_ASSERT(cache->size == 5, "Five entries added");

    // Clear cache
    cache_clear(cache);
    TEST_ASSERT(cache->size == 0, "Cache cleared");
    TEST_ASSERT(cache->head == NULL, "LRU head is NULL after clear");
    TEST_ASSERT(cache->tail == NULL, "LRU tail is NULL after clear");

    // Verify entries are gone
    char* retrieved      = NULL;
    size_t retrieved_len = 0;
    bool result          = cache_get(cache, "key0", &retrieved, &retrieved_len);
    TEST_ASSERT(result == false, "Cleared entries not found");

    // Can still add after clear
    result = cache_set(cache, "new_key", (const unsigned char*)"new_value", 9, 0);
    TEST_ASSERT(result == true, "Can add entries after clear");
    TEST_ASSERT(cache->size == 1, "Size correct after adding to cleared cache");

    cache_destroy(cache);
}

/** Test: Input validation. */
static void test_input_validation(void) {
    printf("\n[TEST] Input Validation\n");

    cache_t* cache       = cache_create(10, 300);
    char* retrieved      = NULL;
    size_t retrieved_len = 0;

    // NULL cache
    bool result = cache_set(NULL, "key", (const unsigned char*)"value", 5, 0);
    TEST_ASSERT(result == false, "NULL cache in set rejected");

    result = cache_get(NULL, "key", &retrieved, &retrieved_len);
    TEST_ASSERT(result == false, "NULL cache in get rejected");

    // NULL key
    result = cache_set(cache, NULL, (const unsigned char*)"value", 5, 0);
    TEST_ASSERT(result == false, "NULL key in set rejected");

    result = cache_get(cache, NULL, &retrieved, &retrieved_len);
    TEST_ASSERT(result == false, "NULL key in get rejected");

    // Empty key
    result = cache_set(cache, "", (const unsigned char*)"value", 5, 0);
    TEST_ASSERT(result == false, "Empty key in set rejected");

    // NULL value
    result = cache_set(cache, "key", NULL, 5, 0);
    TEST_ASSERT(result == false, "NULL value in set rejected");

    // NULL output pointers in get
    result = cache_get(cache, "key", NULL, &retrieved_len);
    TEST_ASSERT(result == false, "NULL out_value in get rejected");

    result = cache_get(cache, "key", &retrieved, NULL);
    TEST_ASSERT(result == false, "NULL out_len in get rejected");

    // Key too long
    char long_key[CACHE_KEY_MAX_LEN + 10];
    memset(long_key, 'a', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = '\0';
    result = cache_set(cache, long_key, (const unsigned char*)"value", 5, 0);
    TEST_ASSERT(result == false, "Oversized key rejected");

    cache_destroy(cache);
}

/** Thread argument for concurrent tests. */
typedef struct {
    cache_t* cache;
    int thread_id;
    int iterations;
    size_t value_size;
} thread_arg_t;

/** Thread function for concurrent reads. */
static thread_return_t concurrent_reader(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i % 10);

        char* value      = NULL;
        size_t value_len = 0;
        cache_get(targ->cache, key, &value, &value_len);
        free(value);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/** Thread function for concurrent writes. */
static thread_return_t concurrent_writer(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        char value[64];
        snprintf(key, sizeof(key), "key%d", i % 10);
        snprintf(value, sizeof(value), "value_thread%d_iter%d", targ->thread_id, i);

        cache_set(targ->cache, key, (const unsigned char*)value, strlen(value), 0);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

#define NUM_THREADS 8
#define ITERATIONS  1000

/** Test: Concurrent access (thread safety). */
static void test_concurrent_access(void) {
    printf("\n[TEST] Concurrent Access (Thread Safety)\n");

    cache_t* cache = cache_create(20, 300);

    // Pre-populate cache
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        cache_set(cache, key, (const unsigned char*)"initial_value", 13, 0);
    }

    thread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];

    // Create mixed reader/writer threads
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

    // Join all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i]);
    }

    TEST_ASSERT(true, "All threads completed without deadlock");
    TEST_ASSERT(cache->size <= cache->capacity,
                "Cache size within capacity after concurrent access");

    // Verify cache is still functional
    bool result = cache_set(cache, "final_key", (const unsigned char*)"final_value", 11, 0);
    TEST_ASSERT(result == true, "Cache functional after concurrent access");

    cache_destroy(cache);
}

/** Test: Lazy LRU promotion. */
static void test_lazy_lru(void) {
    printf("\n[TEST] Lazy LRU Promotion\n");

    cache_t* cache = cache_create(5, 300);

    // Add entries
    cache_set(cache, "key1", (const unsigned char*)"value1", 6, 0);
    cache_set(cache, "key2", (const unsigned char*)"value2", 6, 0);

    // Access key1 multiple times (below threshold)
    char* retrieved      = NULL;
    size_t retrieved_len = 0;
    for (int i = 0; i < CACHE_PROMOTION_THRESHOLD - 1; i++) {
        cache_get(cache, "key1", &retrieved, &retrieved_len);
        free(retrieved);
    }

    TEST_ASSERT(cache->head->key[3] == '2', "key2 still at head (lazy promotion)");

    // One more access should trigger promotion
    cache_get(cache, "key1", &retrieved, &retrieved_len);
    free(retrieved);

    TEST_ASSERT(cache->head->key[3] == '1', "key1 promoted to head after threshold");

    cache_destroy(cache);
}

// ============= Benchmarks ============================
#include <stdio.h>
#include <sys/time.h>  // For gettimeofday on POSIX
#include "../include/cache.h"

#ifdef _WIN32
#include <windows.h>  // For QueryPerformanceCounter on Windows
#endif

/** Benchmark results structure */
typedef struct {
    double operations_per_sec;
    double throughput_mb_per_sec;
    size_t total_operations;
    size_t total_bytes;
    double elapsed_seconds;
} benchmark_result_t;

// Add ANSI color codes for pretty output (optional)
#define BOLD  "\033[1m"
#define RESET "\033[0m"

/** Worker thread for concurrent benchmark */
static thread_return_t concurrent_benchmark_worker(void* arg);

/** High-resolution timer functions */
static double get_current_time(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}

/** Benchmark: Sequential writes */
static benchmark_result_t benchmark_sequential_writes(cache_t* cache, size_t num_operations,
                                                      size_t value_size) {
    double start_time = get_current_time();

    for (size_t i = 0; i < num_operations; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%08zu", i);

        // Generate some data
        unsigned char* value = malloc(value_size);
        if (!value) continue;

        // Fill with pattern
        for (size_t j = 0; j < value_size; j++) {
            value[j] = (unsigned char)((i + j) & 0xFF);
        }

        cache_set(cache, key, value, value_size, 3600);
        free(value);
    }

    double end_time = get_current_time();
    double elapsed  = end_time - start_time;

    return (benchmark_result_t){
        .operations_per_sec    = num_operations / elapsed,
        .throughput_mb_per_sec = (num_operations * value_size) / (elapsed * 1024 * 1024),
        .total_operations      = num_operations,
        .total_bytes           = num_operations * value_size,
        .elapsed_seconds       = elapsed};
}

/** Benchmark: Sequential reads */
static benchmark_result_t benchmark_sequential_reads(cache_t* cache, size_t num_operations) {
    double start_time       = get_current_time();
    size_t successful_reads = 0;
    size_t total_bytes_read = 0;

    for (size_t i = 0; i < num_operations; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%08zu", i % cache->capacity);  // Wrap around

        char* value      = NULL;
        size_t value_len = 0;
        if (cache_get(cache, key, &value, &value_len)) {
            successful_reads++;
            total_bytes_read += value_len;
            free(value);
        }
    }

    double end_time = get_current_time();
    double elapsed  = end_time - start_time;

    return (benchmark_result_t){.operations_per_sec    = successful_reads / elapsed,
                                .throughput_mb_per_sec = total_bytes_read / (elapsed * 1024 * 1024),
                                .total_operations      = successful_reads,
                                .total_bytes           = total_bytes_read,
                                .elapsed_seconds       = elapsed};
}

/** Benchmark: Mixed read/write workload */
static benchmark_result_t benchmark_mixed_workload(cache_t* cache, size_t num_operations,
                                                   size_t value_size, float write_ratio) {
    double start_time      = get_current_time();
    size_t operations_done = 0;
    size_t total_bytes     = 0;

    for (size_t i = 0; i < num_operations; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%08zu", i % (cache->capacity * 2));

        if ((float)rand() / (float)RAND_MAX < write_ratio) {
            // Write operation
            unsigned char* value = malloc(value_size);
            if (value) {
                for (size_t j = 0; j < value_size; j++) {
                    value[j] = (unsigned char)((i + j) & 0xFF);
                }
                cache_set(cache, key, value, value_size, 3600);
                free(value);
                total_bytes += value_size;
            }
        } else {
            // Read operation
            char* value      = NULL;
            size_t value_len = 0;
            if (cache_get(cache, key, &value, &value_len)) {
                total_bytes += value_len;
                free(value);
            }
        }
        operations_done++;
    }

    double end_time = get_current_time();
    double elapsed  = end_time - start_time;

    return (benchmark_result_t){.operations_per_sec    = operations_done / elapsed,
                                .throughput_mb_per_sec = total_bytes / (elapsed * 1024 * 1024),
                                .total_operations      = operations_done,
                                .total_bytes           = total_bytes,
                                .elapsed_seconds       = elapsed};
}

/** Benchmark: Concurrent access performance */
static benchmark_result_t benchmark_concurrent_access(cache_t* cache, size_t num_threads,
                                                      size_t operations_per_thread,
                                                      size_t value_size) {
    thread_t* threads           = malloc(num_threads * sizeof(thread_t));
    benchmark_result_t* results = malloc(num_threads * sizeof(benchmark_result_t));

    double start_time = get_current_time();

    // Create worker threads
    for (size_t i = 0; i < num_threads; i++) {
        thread_arg_t* arg = malloc(sizeof(thread_arg_t));
        arg->cache        = cache;
        arg->thread_id    = i;
        arg->iterations   = operations_per_thread;
        arg->value_size   = value_size;
        thread_create(&threads[i], concurrent_benchmark_worker, arg);
    }

    // Wait for all threads
    for (size_t i = 0; i < num_threads; i++) {
        thread_join(threads[i]);
    }

    double end_time = get_current_time();
    double elapsed  = end_time - start_time;

    // Aggregate results (simplified - in real implementation, collect from threads)
    size_t total_operations = num_threads * operations_per_thread;

    free(threads);
    free(results);

    return (benchmark_result_t){
        .operations_per_sec    = total_operations / elapsed,
        .throughput_mb_per_sec = (total_operations * value_size) / (elapsed * 1024 * 1024),
        .total_operations      = total_operations,
        .total_bytes           = total_operations * value_size,
        .elapsed_seconds       = elapsed};
}

/** Worker thread for concurrent benchmark */
static thread_return_t concurrent_benchmark_worker(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;
    cache_t* cache     = targ->cache;

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_t%d_i%d", targ->thread_id, i);

        if (i % 2 == 0) {  // 50% reads, 50% writes
            // Write
            unsigned char* value = malloc(targ->value_size);
            if (value) {
                for (size_t j = 0; j < targ->value_size; j++) {
                    value[j] = (unsigned char)(((size_t)i + j) & 0xFF);
                }
                cache_set(cache, key, value, targ->value_size, 3600);
                free(value);
            }
        } else {
            // Read
            char* value      = NULL;
            size_t value_len = 0;
            if (cache_get(cache, key, &value, &value_len)) {
                free(value);
            }
        }
    }

    free(targ);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/** Print benchmark results */
static void print_benchmark_result(const char* name, benchmark_result_t result) {
    printf("  %s:\n", name);
    printf("    Operations: %zu in %.3f seconds\n", result.total_operations,
           result.elapsed_seconds);
    printf("    Performance: %.2f ops/sec\n", result.operations_per_sec);
    printf("    Throughput:  %.2f MB/sec\n", result.throughput_mb_per_sec);
    printf("    Total data:  %.2f MB\n", result.total_bytes / (1024.0 * 1024.0));
}

/** Comprehensive benchmark suite */
static void run_benchmarks(void) {
    printf("\n" BOLD "=================================" RESET);
    printf("\n" BOLD "  Performance Benchmarks" RESET);
    printf("\n" BOLD "=================================" RESET);

    const size_t CACHE_SIZE   = 10000;
    const size_t VALUE_SMALL  = 100;    // 100 bytes
    const size_t VALUE_MEDIUM = 1024;   // 1 KB
    const size_t VALUE_LARGE  = 16384;  // 16 KB

    cache_t* cache = cache_create(CACHE_SIZE, 3600);
    if (!cache) {
        fprintf(stderr, "Failed to create cache for benchmarks\n");
        return;
    }

    printf("\n[Benchmark] Sequential Writes (Different Sizes)\n");
    benchmark_result_t result;

    result = benchmark_sequential_writes(cache, 10000, VALUE_SMALL);
    print_benchmark_result("Small values (100B)", result);

    result = benchmark_sequential_writes(cache, 5000, VALUE_MEDIUM);
    print_benchmark_result("Medium values (1KB)", result);

    result = benchmark_sequential_writes(cache, 1000, VALUE_LARGE);
    print_benchmark_result("Large values (16KB)", result);

    printf("\n[Benchmark] Sequential Reads\n");
    // Pre-populate cache
    benchmark_sequential_writes(cache, CACHE_SIZE, VALUE_MEDIUM);

    result = benchmark_sequential_reads(cache, 50000);
    print_benchmark_result("Read-heavy workload", result);

    printf("\n[Benchmark] Mixed Read/Write Workload\n");
    cache_clear(cache);

    result = benchmark_mixed_workload(cache, 20000, VALUE_MEDIUM, 0.1f);  // 10% writes
    print_benchmark_result("Read-heavy (90/10)", result);

    cache_clear(cache);
    result = benchmark_mixed_workload(cache, 20000, VALUE_MEDIUM, 0.5f);  // 50% writes
    print_benchmark_result("Balanced (50/50)", result);

    printf("\n[Benchmark] Concurrent Performance\n");
    cache_clear(cache);

    // Pre-warm cache
    benchmark_sequential_writes(cache, CACHE_SIZE / 2, VALUE_MEDIUM);

    result = benchmark_concurrent_access(cache, 4, 5000, VALUE_MEDIUM);
    print_benchmark_result("4 threads", result);

    cache_clear(cache);
    benchmark_sequential_writes(cache, CACHE_SIZE / 2, VALUE_MEDIUM);

    result = benchmark_concurrent_access(cache, 8, 2500, VALUE_MEDIUM);
    print_benchmark_result("8 threads", result);

    printf("\n[Benchmark] Memory Usage Profile\n");
    cache_clear(cache);

    size_t initial_operations = 1000;
    double start_time, end_time;

    printf("  Measuring operations with different value sizes:\n");

    for (size_t value_size = 64; value_size <= 65536; value_size *= 4) {
        start_time = get_current_time();
        benchmark_sequential_writes(cache, initial_operations, value_size);
        end_time = get_current_time();

        double ops_sec = initial_operations / (end_time - start_time);
        double mb_sec = (initial_operations * value_size) / ((end_time - start_time) * 1024 * 1024);

        printf("    %5zu bytes: %6.0f ops/sec, %5.1f MB/sec\n", value_size, ops_sec, mb_sec);

        cache_clear(cache);
        initial_operations = initial_operations * 3 / 4;  // Reduce for larger values
        if (initial_operations < 100) initial_operations = 100;
    }

    cache_destroy(cache);

    printf("\n" BOLD "=================================" RESET);
    printf("\n" BOLD "  Benchmark Summary" RESET);
    printf("\n" BOLD "=================================" RESET);
    printf("\nKey Metrics to Monitor:\n");
    printf("• Operations/sec: Higher is better\n");
    printf("• Throughput (MB/sec): Data transfer rate\n");
    printf("• Thread scaling: How performance changes with concurrency\n");
    printf("• Value size impact: How performance scales with data size\n");
}

/** Main test runner. */
int main(void) {
    printf("=================================\n");
    printf("  Cache Implementation Tests\n");
    printf("=================================\n");

    test_create_destroy();
    test_set_get();
    test_update();
    test_expiration();
    test_lru_eviction();
    test_invalidation();
    test_clear();
    test_input_validation();
    test_concurrent_access();
    test_lazy_lru();

    printf("\n=================================\n");
    printf("  Test Results\n");
    printf("=================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("=================================\n");

    // Only run benchmarks if all tests passed
    if (tests_failed == 0) {
        printf("✓ All tests passed!\n");
        // Run performance benchmarks
        run_benchmarks();
        return 0;
    } else {
        fprintf(stderr, "✗ Some tests failed! Skipping benchmarks.\n");
        return 1;
    }
}
