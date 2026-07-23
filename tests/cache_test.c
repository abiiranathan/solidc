#include "cache.h"

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// Test statistics counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg)                               \
    do {                                                          \
        if (condition) {                                          \
            tests_passed++;                                       \
            printf("  ✓ %s\n", msg);                              \
        } else {                                                  \
            tests_failed++;                                       \
            fprintf(stderr, "  ✗ %s (line %d)\n", msg, __LINE__); \
        }                                                         \
    } while (0)

/** Helper function to instantiate cache with test default configuration. */
static cache_t* create_test_cache(size_t cap_per_shard, uint32_t ttl_sec) {
    cache_config_t config = {
        .capacity_per_shard = cap_per_shard,
        .slab_blocks_per_shard = 64,
        .default_ttl_sec = ttl_sec,
    };
    return cache_create(&config);
}

/** Test: Basic cache creation and destruction. */
static void test_create_destroy(void) {
    printf("\n[TEST] Cache Creation and Destruction\n");

    cache_t* cache = create_test_cache(100, 60);
    TEST_ASSERT(cache != NULL, "Cache creation succeeded");

    cache_destroy(cache);
    TEST_ASSERT(true, "Cache destroyed without crash");
}

/** Test: Basic put and get operations. */
static void test_set_get(void) {
    printf("\n[TEST] Put and Get Operations\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    const char* key = "test_key";
    const char* value = "test_value";
    size_t keylen = strlen(key);
    size_t value_len = strlen(value);

    // Test put operation
    bool result = cache_put(cache, key, keylen, value, value_len, 0);
    TEST_ASSERT(result == true, "Put operation succeeded");

    // Test get operation
    char val_buf[256] = {0};
    size_t retrieved_len = 0;
    bool found = cache_get(cache, key, keylen, val_buf, sizeof(val_buf), &retrieved_len);

    TEST_ASSERT(found == true, "Get operation returned entry");
    TEST_ASSERT(retrieved_len == value_len, "Retrieved length matches");
    TEST_ASSERT(memcmp(val_buf, value, value_len) == 0, "Retrieved value matches");

    // Test get non-existent key
    found = cache_get(cache, "nonexistent", 11, val_buf, sizeof(val_buf), &retrieved_len);
    TEST_ASSERT(found == false, "Get non-existent key returns false");

    cache_destroy(cache);
}

/** Test: Updating existing keys. */
static void test_update(void) {
    printf("\n[TEST] Update Operations\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    const char* key = "update_key";
    size_t keylen = strlen(key);

    // Initial put
    const char* value1 = "value1";
    cache_put(cache, key, keylen, value1, strlen(value1), 0);

    // Update with new value
    const char* value2 = "value2_longer";
    bool result = cache_put(cache, key, keylen, value2, strlen(value2), 0);
    TEST_ASSERT(result == true, "Update operation succeeded");

    // Verify updated value
    char val_buf[256] = {0};
    size_t len = 0;
    bool found = cache_get(cache, key, keylen, val_buf, sizeof(val_buf), &len);

    TEST_ASSERT(found == true, "Retrieved updated value successfully");
    TEST_ASSERT(len == strlen(value2), "Length matches updated value");
    TEST_ASSERT(memcmp(val_buf, value2, len) == 0, "Content matches updated value");

    cache_destroy(cache);
}

/** Test: Deleting keys. */
static void test_delete(void) {
    printf("\n[TEST] Delete Operations\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    const char* key = "delete_key";
    const char* value = "delete_value";
    size_t key_len = strlen(key);

    cache_put(cache, key, key_len, value, strlen(value), 0);

    // Verify deletion
    bool deleted = cache_delete(cache, key, key_len);
    TEST_ASSERT(deleted == true, "Delete returned true for existing key");

    char val_buf[128];
    size_t len;
    bool found = cache_get(cache, key, key_len, val_buf, sizeof(val_buf), &len);
    TEST_ASSERT(found == false, "Deleted key is no longer retrieved");

    // Double delete check
    deleted = cache_delete(cache, key, key_len);
    TEST_ASSERT(deleted == false, "Deleting already-deleted key returned false");

    cache_destroy(cache);
}

/** Test: Open addressing and capacity bounds. */
static void test_capacity_and_overflow(void) {
    printf("\n[TEST] Open-Addressing Capacity Bounds\n");

    // Small per-shard capacity (1 slot per shard)
    cache_t* cache = create_test_cache(1, 300);
    if (!cache) return;

    int items_inserted = 0;
    for (int i = 0; i < 500; i++) {
        char key[32];
        int len = snprintf(key, sizeof(key), "key%d", i);
        if (cache_put(cache, key, (size_t)len, "data", 4, 0)) { items_inserted++; }
    }

    printf("  Successfully inserted %d items across shards\n", items_inserted);
    TEST_ASSERT(items_inserted > 0, "Inserted items into small capacity cache");

    cache_destroy(cache);
}

/** Test: Input boundary validation. */
static void test_input_validation(void) {
    printf("\n[TEST] Input Validation\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    char val_buf[64];
    size_t len;

    // NULL cache handle
    bool result = cache_put(NULL, "key", 3, "val", 3, 0);
    TEST_ASSERT(result == false, "NULL cache in put rejected");

    // NULL key pointer
    result = cache_put(cache, NULL, 0, "val", 3, 0);
    TEST_ASSERT(result == false, "NULL key in put rejected");

    // NULL value pointer
    result = cache_put(cache, "key", 3, NULL, 3, 0);
    TEST_ASSERT(result == false, "NULL value in put rejected");

    // Key exceeding CACHE_MAX_KEY_LEN
    char oversized_key[CACHE_MAX_KEY_LEN + 16];
    memset(oversized_key, 'k', sizeof(oversized_key));
    result = cache_put(cache, oversized_key, sizeof(oversized_key), "val", 3, 0);
    TEST_ASSERT(result == false, "Oversized key rejected");

    // NULL get requests
    bool found = cache_get(NULL, "key", 3, val_buf, sizeof(val_buf), &len);
    TEST_ASSERT(found == false, "NULL cache in get rejected");

    cache_destroy(cache);
}

/** Multithreaded execution arguments. */
typedef struct {
    cache_t* cache;
    int thread_id;
    int iterations;
} thread_arg_t;

static void* concurrent_reader(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;
    char val_buf[128];

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        int keylen = snprintf(key, sizeof(key), "key%d", i % 50);

        size_t len;
        bool found = cache_get(targ->cache, key, (size_t)keylen, val_buf, sizeof(val_buf), &len);
        if (found && len > 0) {
            volatile char c = val_buf[0];
            (void)c;
        }
    }
    return NULL;
}

static void* concurrent_writer(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        char value[64];
        int keylen = snprintf(key, sizeof(key), "key%d", i % 50);
        int value_len = snprintf(value, sizeof(value), "val_t%d_i%d", targ->thread_id, i);

        cache_put(targ->cache, key, (size_t)keylen, value, (size_t)value_len, 0);
    }
    return NULL;
}

#define NUM_THREADS 8
#define ITERATIONS  2000

/** Test: Concurrent access (per-slot seqlock lock-free safety). */
static void test_concurrent_access(void) {
    printf("\n[TEST] Concurrent Access (Seqlock Safety)\n");

    cache_t* cache = create_test_cache(1000, 300);
    if (!cache) return;

    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].cache = cache;
        args[i].thread_id = i;
        args[i].iterations = ITERATIONS;

        if (i % 2 == 0) {
            pthread_create(&threads[i], NULL, concurrent_reader, &args[i]);
        } else {
            pthread_create(&threads[i], NULL, concurrent_writer, &args[i]);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    TEST_ASSERT(true, "All reader and writer threads completed safely without lockups");
    cache_destroy(cache);
}

/* ============= Performance Benchmarks ============================ */

static double get_current_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void run_benchmarks(void) {
    printf("\n=================================\n");
    printf("  Performance Benchmarks\n");
    printf("=================================\n");

    const size_t NUM_OPS = 1000000;  // 1 Million Operations
    const size_t VAL_SIZE = 256;     // 256B Payload

    cache_t* cache = create_test_cache(1024, 3600);
    if (!cache) {
        printf("Skipping benchmarks due to cache creation failure.\n");
        return;
    }

    char* dummy_val = malloc(VAL_SIZE);
    TEST_ASSERT(dummy_val != NULL, "Benchmark value memory allocated");
    memset(dummy_val, 'A', VAL_SIZE);

    const size_t NUM_KEYS = 50000;
    char (*keys)[24] = malloc(NUM_KEYS * sizeof(*keys));
    size_t* key_lens = malloc(NUM_KEYS * sizeof(*key_lens));

    if (!keys || !key_lens) {
        printf("Skipping benchmarks due to key pre-allocation failure.\n");
        free(dummy_val);
        free(keys);
        free(key_lens);
        cache_destroy(cache);
        return;
    }

    for (size_t i = 0; i < NUM_KEYS; i++) {
        key_lens[i] = (size_t)snprintf(keys[i], sizeof(keys[i]), "key%zu", i);
    }

    // 1. Random Write Benchmark
    uint32_t seed = 42;
    double start = get_current_time();
    for (size_t i = 0; i < NUM_OPS; i++) {
        seed = seed * 1664525u + 1013904223u;
        size_t idx = seed % NUM_KEYS;
        cache_put(cache, keys[idx], key_lens[idx], dummy_val, VAL_SIZE, 0);
    }
    double end = get_current_time();
    double write_duration = end - start;

    printf("Random Writes:\n");
    printf("  Throughput:  %.0f ops/sec\n", (double)NUM_OPS / write_duration);
    printf("  Avg Latency: %.2f ns/op (%.3f us/op)\n", (write_duration / (double)NUM_OPS) * 1e9,
           (write_duration / (double)NUM_OPS) * 1e6);

    // 2. Random Read Benchmark
    char val_out[CACHE_MAX_VALUE_LEN];
    start = get_current_time();
    size_t hit_count = 0;
    for (size_t i = 0; i < NUM_OPS; i++) {
        seed = seed * 1664525u + 1013904223u;
        size_t idx = seed % NUM_KEYS;

        size_t len;
        if (cache_get(cache, keys[idx], key_lens[idx], val_out, sizeof(val_out), &len)) { hit_count++; }
    }
    end = get_current_time();
    double read_duration = end - start;

    printf("Random Reads:\n");
    printf("  Throughput:  %.0f ops/sec (Hits: %zu)\n", (double)NUM_OPS / read_duration, hit_count);
    printf("  Avg Latency: %.2f ns/op (%.3f us/op)\n", (read_duration / (double)NUM_OPS) * 1e9,
           (read_duration / (double)NUM_OPS) * 1e6);

    free(keys);
    free(key_lens);
    free(dummy_val);
    cache_destroy(cache);
}

/** Test: TTL expiration behavior. */
static void test_ttl_expiration(void) {
    printf("\n[TEST] TTL Expiration\n");

    // TTL of 1 second so the test doesn't stall the suite for long.
    cache_t* cache = create_test_cache(100, 1);
    if (!cache) return;

    const char* key = "ttl_key";
    const char* value = "ttl_value";
    size_t keylen = strlen(key);

    bool put_ok = cache_put(cache, key, keylen, value, strlen(value), 1);
    TEST_ASSERT(put_ok == true, "Put with 1s TTL succeeded");

    char val_buf[64];
    size_t len;
    bool found_immediately = cache_get(cache, key, keylen, val_buf, sizeof(val_buf), &len);
    TEST_ASSERT(found_immediately == true, "Entry retrievable before expiration");

    sleep(2);  // Wait past the 1-second TTL.

    bool found_after_expiry = cache_get(cache, key, keylen, val_buf, sizeof(val_buf), &len);
    TEST_ASSERT(found_after_expiry == false, "Entry not retrievable after TTL expiration");

    // An expired slot must be reclaimable by a new put at the same key.
    bool reput_ok = cache_put(cache, key, keylen, "fresh_value", 11, 0);
    TEST_ASSERT(reput_ok == true, "Expired slot reclaimed by subsequent put");

    cache_destroy(cache);
}

/** Test: Default TTL is applied when ttl_sec is 0. */
static void test_default_ttl(void) {
    printf("\n[TEST] Default TTL Fallback\n");

    cache_t* cache = create_test_cache(100, 1);  // default_ttl_sec = 1
    if (!cache) return;

    const char* key = "default_ttl_key";
    bool put_ok = cache_put(cache, key, strlen(key), "v", 1, 0);  // ttl_sec = 0 -> use default
    TEST_ASSERT(put_ok == true, "Put with ttl_sec=0 succeeded");

    sleep(2);

    char val_buf[16];
    size_t len;
    bool found = cache_get(cache, key, strlen(key), val_buf, sizeof(val_buf), &len);
    TEST_ASSERT(found == false, "Entry expired using cache's default TTL");

    cache_destroy(cache);
}

/** Test: Value sizes at and around the inline/slab boundary. */
static void test_inline_slab_boundary(void) {
    printf("\n[TEST] Inline/Slab Storage Boundary\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    char* val = malloc(CACHE_MAX_VALUE_LEN);
    TEST_ASSERT(val != NULL, "Boundary test value buffer allocated");
    if (!val) {
        cache_destroy(cache);
        return;
    }
    memset(val, 'X', CACHE_MAX_VALUE_LEN);

    char val_buf[CACHE_MAX_VALUE_LEN];
    size_t out_len;

    // Exactly at the inline limit: must stay in val_inline, no slab used.
    bool put_inline_edge = cache_put(cache, "inline_edge", 11, val, CACHE_INLINE_VAL_LEN, 0);
    TEST_ASSERT(put_inline_edge == true, "Put at exact inline boundary succeeded");
    bool got_inline_edge = cache_get(cache, "inline_edge", 11, val_buf, sizeof(val_buf), &out_len);
    TEST_ASSERT(got_inline_edge == true && out_len == CACHE_INLINE_VAL_LEN,
                "Get at exact inline boundary returns correct length");
    TEST_ASSERT(memcmp(val_buf, val, CACHE_INLINE_VAL_LEN) == 0, "Inline-boundary value content matches");

    // One byte past inline limit: must spill to slab allocator.
    bool put_slab_edge = cache_put(cache, "slab_edge", 9, val, CACHE_INLINE_VAL_LEN + 1, 0);
    TEST_ASSERT(put_slab_edge == true, "Put one byte past inline boundary succeeded");
    bool got_slab_edge = cache_get(cache, "slab_edge", 9, val_buf, sizeof(val_buf), &out_len);
    TEST_ASSERT(got_slab_edge == true && out_len == CACHE_INLINE_VAL_LEN + 1,
                "Get one byte past inline boundary returns correct length");
    TEST_ASSERT(memcmp(val_buf, val, CACHE_INLINE_VAL_LEN + 1) == 0, "Slab-boundary value content matches");

    // Exactly at CACHE_MAX_VALUE_LEN: largest legal payload.
    bool put_max = cache_put(cache, "max_val", 7, val, CACHE_MAX_VALUE_LEN, 0);
    TEST_ASSERT(put_max == true, "Put at CACHE_MAX_VALUE_LEN succeeded");

    // One byte over CACHE_MAX_VALUE_LEN: must be rejected.
    bool put_over_max = cache_put(cache, "over_max", 8, val, CACHE_MAX_VALUE_LEN + 1, 0);
    TEST_ASSERT(put_over_max == false, "Put exceeding CACHE_MAX_VALUE_LEN rejected");

    free(val);
    cache_destroy(cache);
}

/** Test: Update that moves an entry from inline storage to slab storage and back. */
static void test_inline_to_slab_transition(void) {
    printf("\n[TEST] Inline <-> Slab Transition on Update\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    const char* key = "transition_key";
    size_t keylen = strlen(key);

    char small_val[32];
    memset(small_val, 'S', sizeof(small_val));
    char large_val[CACHE_MAX_VALUE_LEN];
    memset(large_val, 'L', sizeof(large_val));

    // Start inline.
    cache_put(cache, key, keylen, small_val, sizeof(small_val), 0);

    // Update to a slab-backed value; must free no longer needed inline state cleanly.
    bool updated_to_slab = cache_put(cache, key, keylen, large_val, sizeof(large_val), 0);
    TEST_ASSERT(updated_to_slab == true, "Update from inline to slab-backed value succeeded");

    char val_buf[CACHE_MAX_VALUE_LEN];
    size_t out_len;
    bool found = cache_get(cache, key, keylen, val_buf, sizeof(val_buf), &out_len);
    TEST_ASSERT(found == true && out_len == sizeof(large_val), "Slab-backed value retrieved with correct length");
    TEST_ASSERT(memcmp(val_buf, large_val, sizeof(large_val)) == 0, "Slab-backed value content matches");

    // Update back down to inline; the slab block from the previous write must be reclaimed.
    bool updated_to_inline = cache_put(cache, key, keylen, small_val, sizeof(small_val), 0);
    TEST_ASSERT(updated_to_inline == true, "Update from slab back to inline value succeeded");

    found = cache_get(cache, key, keylen, val_buf, sizeof(val_buf), &out_len);
    TEST_ASSERT(found == true && out_len == sizeof(small_val),
                "Inline value retrieved with correct length after transition");
    TEST_ASSERT(memcmp(val_buf, small_val, sizeof(small_val)) == 0, "Inline value content matches after transition");

    cache_destroy(cache);
}

/** Test: Slab pool exhaustion is handled gracefully rather than corrupting state. */
static void test_slab_exhaustion(void) {
    printf("\n[TEST] Slab Pool Exhaustion\n");

    // Only 4 slab blocks available per shard; force every value to spill to slab
    // by exceeding CACHE_INLINE_VAL_LEN, across enough distinct keys to guarantee
    // more than 4 concurrently-live large entries land in at least one shard.
    cache_config_t config = {
        .capacity_per_shard = 64,
        .slab_blocks_per_shard = 4,
        .default_ttl_sec = 300,
    };
    cache_t* cache = cache_create(&config);
    if (!cache) return;

    char large_val[CACHE_INLINE_VAL_LEN + 1];
    memset(large_val, 'Z', sizeof(large_val));

    int successes = 0;
    int failures = 0;
    for (int i = 0; i < 4096; i++) {
        char key[32];
        int len = snprintf(key, sizeof(key), "slabkey%d", i);
        if (cache_put(cache, key, (size_t)len, large_val, sizeof(large_val), 0)) {
            successes++;
        } else {
            failures++;
        }
    }

    TEST_ASSERT(successes > 0, "Some slab-backed puts succeeded before exhaustion");
    TEST_ASSERT(failures > 0, "Slab exhaustion produced graceful put failures, not a crash");

    cache_destroy(cache);
}

/** Test: Zero-length key is rejected by all three operations. */
static void test_zero_length_key(void) {
    printf("\n[TEST] Zero-Length Key Rejection\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    bool put_result = cache_put(cache, "k", 0, "v", 1, 0);
    TEST_ASSERT(put_result == false, "Put with zero-length key rejected");

    char val_buf[16];
    size_t len;
    bool get_result = cache_get(cache, "k", 0, val_buf, sizeof(val_buf), &len);
    TEST_ASSERT(get_result == false, "Get with zero-length key rejected");

    bool del_result = cache_delete(cache, "k", 0);
    TEST_ASSERT(del_result == false, "Delete with zero-length key rejected");

    cache_destroy(cache);
}

/** Test: cache_get with an output buffer too small for the stored value. */
static void test_get_buffer_too_small(void) {
    printf("\n[TEST] Output Buffer Overflow Guard\n");

    cache_t* cache = create_test_cache(100, 300);
    if (!cache) return;

    const char* key = "big_value_key";
    char value[128];
    memset(value, 'B', sizeof(value));
    cache_put(cache, key, strlen(key), value, sizeof(value), 0);

    char small_buf[16];  // Smaller than the 128-byte stored value.
    size_t len;
    bool found = cache_get(cache, key, strlen(key), small_buf, sizeof(small_buf), &len);
    TEST_ASSERT(found == false, "Get with undersized output buffer rejected instead of truncating");

    cache_destroy(cache);
}

/** Test: cache_create rejects invalid configuration. */
static void test_create_invalid_config(void) {
    printf("\n[TEST] Cache Creation Input Validation\n");

    cache_t* cache_null_config = cache_create(NULL);
    TEST_ASSERT(cache_null_config == NULL, "cache_create rejects NULL config");

    cache_config_t zero_capacity_config = {
        .capacity_per_shard = 0,
        .slab_blocks_per_shard = 64,
        .default_ttl_sec = 60,
    };
    cache_t* cache_zero_cap = cache_create(&zero_capacity_config);
    TEST_ASSERT(cache_zero_cap == NULL, "cache_create rejects zero capacity_per_shard");

    // Zero slab blocks is a legal degenerate configuration: all values must be inline-sized.
    cache_config_t zero_slab_config = {
        .capacity_per_shard = 16,
        .slab_blocks_per_shard = 0,
        .default_ttl_sec = 60,
    };
    cache_t* cache_zero_slab = cache_create(&zero_slab_config);
    TEST_ASSERT(cache_zero_slab != NULL, "cache_create accepts zero slab_blocks_per_shard");
    if (cache_zero_slab) {
        char small_val[8] = {0};
        bool inline_put = cache_put(cache_zero_slab, "k", 1, small_val, sizeof(small_val), 0);
        TEST_ASSERT(inline_put == true, "Inline put succeeds with zero slab blocks configured");

        char large_val[CACHE_INLINE_VAL_LEN + 1];
        memset(large_val, 'A', sizeof(large_val));
        bool slab_put = cache_put(cache_zero_slab, "k2", 2, large_val, sizeof(large_val), 0);
        TEST_ASSERT(slab_put == false, "Slab-requiring put fails gracefully with zero slab blocks configured");

        cache_destroy(cache_zero_slab);
    }

    cache_destroy(cache_null_config);  // Must be a no-op; verifies cache_destroy(NULL) safety.
    TEST_ASSERT(true, "cache_destroy(NULL) did not crash");
}

/** Test: Delete under concurrent readers/writers does not corrupt seqlock state. */
static void* concurrent_deleter(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;

    for (int i = 0; i < targ->iterations; i++) {
        char key[32];
        int keylen = snprintf(key, sizeof(key), "key%d", i % 50);
        cache_delete(targ->cache, key, (size_t)keylen);
    }
    return NULL;
}

#define NUM_THREADS_WITH_DELETE 9

/** Test: Concurrent readers, writers, and deleters together. */
static void test_concurrent_access_with_delete(void) {
    printf("\n[TEST] Concurrent Access With Deletes (Seqlock Safety)\n");

    cache_t* cache = create_test_cache(1000, 300);
    if (!cache) return;

    pthread_t threads[NUM_THREADS_WITH_DELETE];
    thread_arg_t args[NUM_THREADS_WITH_DELETE];

    for (int i = 0; i < NUM_THREADS_WITH_DELETE; i++) {
        args[i].cache = cache;
        args[i].thread_id = i;
        args[i].iterations = ITERATIONS;

        if (i % 3 == 0) {
            pthread_create(&threads[i], NULL, concurrent_reader, &args[i]);
        } else if (i % 3 == 1) {
            pthread_create(&threads[i], NULL, concurrent_writer, &args[i]);
        } else {
            pthread_create(&threads[i], NULL, concurrent_deleter, &args[i]);
        }
    }

    for (int i = 0; i < NUM_THREADS_WITH_DELETE; i++) {
        pthread_join(threads[i], NULL);
    }

    TEST_ASSERT(true, "Readers, writers, and deleters completed concurrently without lockups");
    cache_destroy(cache);
}

int main(void) {
    printf("=================================\n");
    printf("  Cache Implementation Tests\n");
    printf("=================================\n");

    test_create_destroy();
    test_set_get();
    test_update();
    test_delete();
    test_capacity_and_overflow();
    test_input_validation();
    test_concurrent_access();

    test_ttl_expiration();
    test_default_ttl();
    test_inline_slab_boundary();
    test_inline_to_slab_transition();
    test_slab_exhaustion();
    test_zero_length_key();
    test_get_buffer_too_small();
    test_create_invalid_config();
    test_concurrent_access_with_delete();

    printf("\n=================================\n");
    printf("  Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    printf("=================================\n");

    run_benchmarks();
    return tests_failed;
}
