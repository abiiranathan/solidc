#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/threadpool.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../include/thread.h"

// Test configuration
#define MAX_THREADS         16
#define MAX_TASKS           10000
#define STRESS_DURATION_SEC 5

// Global test state
static atomic_int test_counter = 0;
static atomic_int completed_tasks = 0;
static atomic_int error_count = 0;
static atomic_bool stress_test_running = false;

// Test result tracking
typedef struct {
    int passed;
    int failed;
    char name[64];
} test_result;

// Thread-safe output
static void safe_printf(const char* format, ...) {
    static atomic_flag print_lock = ATOMIC_FLAG_INIT;
    while (atomic_flag_test_and_set(&print_lock)) {
    }
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    atomic_flag_clear(&print_lock);
}

// Test helper macros
#define TEST_ASSERT(condition, message)                                                  \
    do {                                                                                 \
        if (!(condition)) {                                                              \
            safe_printf("ASSERTION FAILED: %s at %s:%d\n", message, __FILE__, __LINE__); \
            atomic_fetch_add(&error_count, 1);                                           \
            return 0;                                                                    \
        }                                                                                \
    } while (0)

#define RUN_TEST(test_func)                        \
    do {                                           \
        safe_printf("Running %s... ", #test_func); \
        if (test_func()) {                         \
            safe_printf("PASSED\n");               \
            result.passed++;                       \
        } else {                                   \
            safe_printf("FAILED\n");               \
            result.failed++;                       \
        }                                          \
    } while (0)

// =============================================================================
// Test Tasks
// =============================================================================

void simple_task(void* arg) {
    volatile int* value = (int*)arg;
    atomic_fetch_add(&completed_tasks, 1);
    for (volatile int i = 0; i < 1000; i++) {
    }
    (void)value;
}

void counter_task(void* arg) {
    atomic_fetch_add(&test_counter, 1);
    atomic_fetch_add(&completed_tasks, 1);
    (void)arg;
}

void sleep_task(void* arg) {
    int ms = *(int*)arg;
    struct timespec ts = {0, (long)ms * 1000000};
    nanosleep(&ts, NULL);
    atomic_fetch_add(&completed_tasks, 1);
}

void error_task(void* arg) {
    (void)arg;
    static atomic_int shared_resource = 0;
    int old_val = atomic_load(&shared_resource);
    for (volatile int i = 0; i < 100; i++) {
    }
    atomic_store(&shared_resource, old_val + 1);
    atomic_fetch_add(&completed_tasks, 1);
}

void stress_task(void* arg) {
    (void)arg;
    if (!atomic_load(&stress_test_running)) return;
    volatile uint64_t result = 0;
    for (uint64_t i = 0; i < 10000; i++) {
        result += i * i;
    }
    atomic_fetch_add(&completed_tasks, 1);
}

/*
 * batch_counter_task — identical to counter_task but named distinctly so
 * batch tests are easy to identify in a debugger or profiler.
 */
void batch_counter_task(void* arg) {
    atomic_fetch_add(&test_counter, 1);
    atomic_fetch_add(&completed_tasks, 1);
    (void)arg;
}

/*
 * arg_capture_task — writes the int pointed to by arg into a shared results
 * array to verify that each task received the correct argument.
 */
static atomic_int arg_results[MAX_TASKS];

void arg_capture_task(void* arg) {
    int idx = *(int*)arg;
    atomic_store(&arg_results[idx], idx);
    atomic_fetch_add(&completed_tasks, 1);
}

// =============================================================================
// Individual Tests
// =============================================================================

int test_basic_creation_destruction() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    threadpool_destroy(pool, -1);
    return 1;
}

int test_invalid_parameters() {
    Threadpool* pool = threadpool_create(0);
    TEST_ASSERT(pool != NULL, "Pool should handle 0 threads gracefully");
    threadpool_destroy(pool, -1);

    bool result = threadpool_submit(NULL, simple_task, NULL);
    TEST_ASSERT(result == false, "Should reject NULL pool");

    pool = threadpool_create(2);
    result = threadpool_submit(pool, NULL, NULL);
    TEST_ASSERT(result == false, "Should reject NULL function");
    threadpool_destroy(pool, -1);

    return 1;
}

int test_single_thread_execution() {
    Threadpool* pool = threadpool_create(1);
    TEST_ASSERT(pool != NULL, "Single thread pool creation");

    atomic_store(&completed_tasks, 0);
    int values[5] = {1, 2, 3, 4, 5};

    for (int i = 0; i < 5; i++) {
        bool result = threadpool_submit(pool, simple_task, &values[i]);
        TEST_ASSERT(result, "Task submission should succeed");
    }

    threadpool_destroy(pool, -1);
    TEST_ASSERT(atomic_load(&completed_tasks) == 5, "All tasks should complete");
    return 1;
}

int test_multiple_thread_execution() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Multi-thread pool creation");

    atomic_store(&completed_tasks, 0);
    atomic_store(&test_counter, 0);

    const int num_tasks = 100;
    for (int i = 0; i < num_tasks; i++) {
        bool result = threadpool_submit(pool, counter_task, NULL);
        TEST_ASSERT(result, "Task submission should succeed");
    }

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&test_counter) == num_tasks, "Counter should equal number of tasks");
    TEST_ASSERT(atomic_load(&completed_tasks) == num_tasks, "All tasks should complete");
    return 1;
}

int test_task_ordering_independence() {
    Threadpool* pool = threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Pool creation for ordering test");

    atomic_store(&completed_tasks, 0);

    int delays[] = {50, 10, 30, 5, 100, 1, 75, 25};
    const int n_tasks = (int)(sizeof(delays) / sizeof(delays[0]));

    for (int i = 0; i < n_tasks; i++) {
        bool result = threadpool_submit(pool, sleep_task, &delays[i]);
        TEST_ASSERT(result, "Sleep task submission should succeed");
    }

    threadpool_destroy(pool, -1);
    TEST_ASSERT(atomic_load(&completed_tasks) == n_tasks, "All sleep tasks should complete");
    return 1;
}

int test_high_contention() {
    Threadpool* pool = threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Pool creation for contention test");

    atomic_store(&completed_tasks, 0);

    const int num_tasks = 1000;
    for (int i = 0; i < num_tasks; i++) {
        bool result = threadpool_submit(pool, error_task, NULL);
        TEST_ASSERT(result, "High contention task submission should succeed");
    }

    threadpool_destroy(pool, -1);
    TEST_ASSERT(atomic_load(&completed_tasks) == num_tasks, "All contention tasks should complete");
    return 1;
}

int test_queue_overflow_behavior() {
    Threadpool* pool = threadpool_create(2);
    TEST_ASSERT(pool != NULL, "Pool creation for overflow test");

    atomic_store(&completed_tasks, 0);

    int long_delay = 100;
    int successful_submissions = 0;

    for (int i = 0; i < 20; i++) {
        bool result = threadpool_submit(pool, sleep_task, &long_delay);
        if (result) {
            successful_submissions++;
        }
    }

    threadpool_destroy(pool, -1);

    TEST_ASSERT(successful_submissions > 0, "Should submit some tasks even under pressure");
    TEST_ASSERT(atomic_load(&completed_tasks) == successful_submissions, "All submitted tasks should complete");
    return 1;
}

int test_rapid_create_destroy() {
    for (int i = 0; i < 50; i++) {
        Threadpool* pool = threadpool_create(4);
        TEST_ASSERT(pool != NULL, "Rapid pool creation should succeed");
        for (int j = 0; j < 10; j++) {
            threadpool_submit(pool, simple_task, NULL);
        }
        threadpool_destroy(pool, -1);
    }
    return 1;
}

int test_stress_test() {
    Threadpool* pool = threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Stress test pool creation");

    atomic_store(&completed_tasks, 0);
    atomic_store(&stress_test_running, true);

    time_t start_time = time(NULL);
    int submitted = 0;

    while (time(NULL) - start_time < STRESS_DURATION_SEC) {
        if (threadpool_submit(pool, stress_task, NULL) == 0) {
            submitted++;
        }
        if (submitted % 1000 == 0) {
            struct timespec ts = {0, 1000000};
            nanosleep(&ts, NULL);
        }
    }

    atomic_store(&stress_test_running, false);
    threadpool_destroy(pool, -1);

    int completed = atomic_load(&completed_tasks);
    safe_printf("Stress test: submitted %d, completed %d tasks in %d seconds\n", submitted, completed,
                STRESS_DURATION_SEC);

    TEST_ASSERT(completed > 0, "Should complete some tasks during stress test");
    return 1;
}

int test_thread_safety_validation() {
    Threadpool* pool = threadpool_create(16);
    TEST_ASSERT(pool != NULL, "Thread safety test pool creation");

    atomic_store(&test_counter, 0);
    atomic_store(&completed_tasks, 0);

    const int num_tasks = 10000;
    for (int i = 0; i < num_tasks; i++) {
        int result = threadpool_submit(pool, counter_task, NULL);
        TEST_ASSERT(result, "Thread safety task submission should succeed");
    }

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&test_counter) == num_tasks, "Atomic counter should be consistent");
    TEST_ASSERT(atomic_load(&completed_tasks) == num_tasks, "All thread safety tasks should complete");
    return 1;
}

// =============================================================================
// Memory and Resource Tests
// =============================================================================

int test_memory_leaks() {
    for (int i = 0; i < 100; i++) {
        Threadpool* pool = threadpool_create(4);
        TEST_ASSERT(pool != NULL, "Memory test pool creation");
        for (int j = 0; j < 50; j++) {
            threadpool_submit(pool, simple_task, NULL);
        }
        threadpool_destroy(pool, -1);
    }
    safe_printf("Memory leak test completed (check with valgrind for verification)\n");
    return 1;
}

// =============================================================================
// threadpool_submit_batch Tests
// =============================================================================

/*
 * test_batch_basic
 *
 * Submit a small flat batch and verify every task ran exactly once.
 * This is the simplest correctness check: the right number of completions,
 * the right return value, no crash.
 */
int test_batch_basic() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Batch basic: pool creation");

    atomic_store(&test_counter, 0);
    atomic_store(&completed_tasks, 0);

    const int N = 64;
    void (*fns[64])(void*);
    for (int i = 0; i < N; i++) fns[i] = batch_counter_task;

    size_t submitted = threadpool_submit_batch(pool, fns, NULL, (size_t)N);
    TEST_ASSERT((int)submitted == N, "Batch basic: all tasks submitted");

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&test_counter) == N, "Batch basic: counter correct");
    TEST_ASSERT(atomic_load(&completed_tasks) == N, "Batch basic: all completed");
    return 1;
}

/*
 * test_batch_null_args
 *
 * Passing NULL for the args array is documented as equivalent to passing
 * NULL to every individual task.  batch_counter_task ignores its argument,
 * so all tasks must still complete without a crash or assertion failure.
 */
int test_batch_null_args() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Batch null args: pool creation");

    atomic_store(&completed_tasks, 0);

    const int N = 32;
    void (*fns[32])(void*);
    for (int i = 0; i < N; i++) fns[i] = batch_counter_task;

    size_t submitted = threadpool_submit_batch(pool, fns, NULL, (size_t)N);
    TEST_ASSERT((int)submitted == N, "Batch null args: all tasks submitted");

    threadpool_destroy(pool, -1);
    TEST_ASSERT(atomic_load(&completed_tasks) == N, "Batch null args: all tasks completed");
    return 1;
}

/*
 * test_batch_null_entries
 *
 * NULL entries in the functions array must be silently skipped.  The return
 * value should equal the number of non-NULL entries, not the raw count.
 */
int test_batch_null_entries() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Batch null entries: pool creation");

    atomic_store(&completed_tasks, 0);

    /*
     * 10 slots: alternating valid / NULL → 5 real tasks, 5 skipped.
     */
    const int TOTAL = 10;
    const int VALID = 5;
    void (*fns[10])(void*);
    for (int i = 0; i < TOTAL; i++) {
        fns[i] = (i % 2 == 0) ? batch_counter_task : NULL;
    }

    size_t submitted = threadpool_submit_batch(pool, fns, NULL, (size_t)TOTAL);
    TEST_ASSERT((int)submitted == VALID, "Batch null entries: submitted count equals non-NULL entries");

    threadpool_destroy(pool, -1);
    TEST_ASSERT(atomic_load(&completed_tasks) == VALID, "Batch null entries: only non-NULL tasks ran");
    return 1;
}

/*
 * test_batch_argument_delivery
 *
 * Verify that each task receives the correct argument from the args array.
 * arg_capture_task writes its index into arg_results[idx]; after the pool
 * drains we check every slot equals its own index.
 */
int test_batch_argument_delivery() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Batch arg delivery: pool creation");

    atomic_store(&completed_tasks, 0);

    const int N = 256;
    TEST_ASSERT(N <= MAX_TASKS, "Batch arg delivery: N within bounds");

    void (**fns)(void*) = malloc((size_t)N * sizeof(*fns));
    void** args_arr = malloc((size_t)N * sizeof(*args_arr));
    int* indices = malloc((size_t)N * sizeof(*indices));
    TEST_ASSERT(fns && args_arr && indices, "Batch arg delivery: allocation");

    for (int i = 0; i < N; i++) {
        atomic_store(&arg_results[i], -1);
        indices[i] = i;
        fns[i] = arg_capture_task;
        args_arr[i] = &indices[i];
    }

    size_t submitted = threadpool_submit_batch(pool, fns, args_arr, (size_t)N);
    TEST_ASSERT((int)submitted == N, "Batch arg delivery: all submitted");

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&completed_tasks) == N, "Batch arg delivery: all tasks ran");
    for (int i = 0; i < N; i++) {
        TEST_ASSERT(atomic_load(&arg_results[i]) == i, "Batch arg delivery: each task got the correct argument");
    }

    free(fns);
    free(args_arr);
    free(indices);
    return 1;
}

/*
 * test_batch_large
 *
 * Submit a batch larger than GLOBAL_Q_SIZE (16384) to exercise the
 * gq_push_batch chunking path — where the batch must be flushed across
 * multiple mutex acquisitions because the queue fills up mid-write.
 *
 * With 8 workers actively draining the queue during submission, the global
 * queue will cycle multiple times, so every task should still complete.
 */
int test_batch_large() {
    Threadpool* pool = threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Batch large: pool creation");

    atomic_store(&test_counter, 0);
    atomic_store(&completed_tasks, 0);

    /*
     * 20000 tasks > GLOBAL_Q_SIZE (16384) — forces at least two chunks
     * through gq_push_batch.
     */
    const int N = 20000;

    void (**fns)(void*) = malloc((size_t)N * sizeof(*fns));
    TEST_ASSERT(fns != NULL, "Batch large: allocation");
    for (int i = 0; i < N; i++) fns[i] = batch_counter_task;

    size_t submitted = threadpool_submit_batch(pool, fns, NULL, (size_t)N);
    TEST_ASSERT((int)submitted == N, "Batch large: all tasks submitted");

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&test_counter) == N, "Batch large: counter correct");
    TEST_ASSERT(atomic_load(&completed_tasks) == N, "Batch large: all completed");

    free(fns);
    return 1;
}

/*
 * test_batch_single_task
 *
 * A batch of size 1 must behave identically to threadpool_submit.
 * Edge case: no off-by-one in the chunking arithmetic.
 */
int test_batch_single_task() {
    Threadpool* pool = threadpool_create(2);
    TEST_ASSERT(pool != NULL, "Batch single: pool creation");

    atomic_store(&test_counter, 0);
    atomic_store(&completed_tasks, 0);

    void (*fn)(void*) = batch_counter_task;
    size_t submitted = threadpool_submit_batch(pool, &fn, NULL, 1);
    TEST_ASSERT(submitted == 1, "Batch single: task submitted");

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&test_counter) == 1, "Batch single: counter incremented");
    TEST_ASSERT(atomic_load(&completed_tasks) == 1, "Batch single: task completed");
    return 1;
}

/*
 * test_batch_zero_count
 *
 * Submitting a batch of size 0 must return 0 immediately without touching
 * any queue or waking any worker.
 */
int test_batch_zero_count() {
    Threadpool* pool = threadpool_create(2);
    TEST_ASSERT(pool != NULL, "Batch zero: pool creation");

    void (*fn)(void*) = batch_counter_task;
    size_t submitted = threadpool_submit_batch(pool, &fn, NULL, 0);
    TEST_ASSERT(submitted == 0, "Batch zero: returns 0 for empty batch");

    threadpool_destroy(pool, -1);
    return 1;
}

/*
 * test_batch_null_pool
 *
 * Passing NULL as the pool must return 0 safely — no crash, no UB.
 */
int test_batch_null_pool() {
    void (*fn)(void*) = batch_counter_task;
    size_t submitted = threadpool_submit_batch(NULL, &fn, NULL, 1);
    TEST_ASSERT(submitted == 0, "Batch null pool: returns 0");
    return 1;
}

/*
 * test_batch_multiple_batches
 *
 * Submit several independent batches sequentially, waiting for the pool to
 * drain between each one via threadpool_wait.  Verifies that:
 * 1. threadpool_wait correctly identifies completion after a batch.
 * 2. The pool is reusable across multiple batch submissions.
 * 3. Totals accumulate correctly across phases.
 */
int test_batch_multiple_batches() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Batch multi: pool creation");

    const int BATCHES = 5;
    const int BATCH_N = 200;
    const int TOTAL = BATCHES * BATCH_N;

    atomic_store(&test_counter, 0);
    atomic_store(&completed_tasks, 0);

    void (**fns)(void*) = malloc((size_t)BATCH_N * sizeof(*fns));
    TEST_ASSERT(fns != NULL, "Batch multi: allocation");
    for (int i = 0; i < BATCH_N; i++) fns[i] = batch_counter_task;

    for (int b = 0; b < BATCHES; b++) {
        size_t submitted = threadpool_submit_batch(pool, fns, NULL, (size_t)BATCH_N);
        TEST_ASSERT((int)submitted == BATCH_N, "Batch multi: all tasks in batch submitted");
        threadpool_wait(pool);

        int done = atomic_load(&completed_tasks);
        TEST_ASSERT(done == (b + 1) * BATCH_N, "Batch multi: correct count after each threadpool_wait");
    }

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&test_counter) == TOTAL, "Batch multi: total counter correct");
    TEST_ASSERT(atomic_load(&completed_tasks) == TOTAL, "Batch multi: all tasks completed");

    free(fns);
    return 1;
}

/*
 * test_batch_concurrent_submitters
 *
 * Two threads simultaneously call threadpool_submit_batch on the same pool.
 * Both batches must complete fully with no lost tasks and no corruption —
 * exercising the mutex serialisation inside gq_push_batch.
 */
typedef struct {
    Threadpool* pool;
    void (**fns)(void*);
    int count;
    atomic_int submitted;
} concurrent_batch_arg;

static void* concurrent_batch_thread(void* varg) {
    concurrent_batch_arg* a = (concurrent_batch_arg*)varg;
    size_t n = threadpool_submit_batch(a->pool, a->fns, NULL, (size_t)a->count);
    atomic_store(&a->submitted, (int)n);
    return NULL;
}

int test_batch_concurrent_submitters() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Batch concurrent: pool creation");

    atomic_store(&test_counter, 0);
    atomic_store(&completed_tasks, 0);

    const int N = 500;

    void (**fns_a)(void*) = malloc((size_t)N * sizeof(*fns_a));
    void (**fns_b)(void*) = malloc((size_t)N * sizeof(*fns_b));
    TEST_ASSERT(fns_a && fns_b, "Batch concurrent: allocation");
    for (int i = 0; i < N; i++) {
        fns_a[i] = batch_counter_task;
        fns_b[i] = batch_counter_task;
    }

    concurrent_batch_arg arg_a = {pool, fns_a, N, 0};
    concurrent_batch_arg arg_b = {pool, fns_b, N, 0};

    pthread_t ta, tb;
    thread_create(&ta, concurrent_batch_thread, &arg_a);
    thread_create(&tb, concurrent_batch_thread, &arg_b);
    thread_join(ta, NULL);
    thread_join(tb, NULL);

    TEST_ASSERT(atomic_load(&arg_a.submitted) == N, "Batch concurrent: thread A submitted all tasks");
    TEST_ASSERT(atomic_load(&arg_b.submitted) == N, "Batch concurrent: thread B submitted all tasks");

    threadpool_destroy(pool, -1);

    TEST_ASSERT(atomic_load(&test_counter) == 2 * N, "Batch concurrent: total counter correct");
    TEST_ASSERT(atomic_load(&completed_tasks) == 2 * N, "Batch concurrent: all tasks completed");

    free(fns_a);
    free(fns_b);
    return 1;
}

// =============================================================================
// Main Test Runner
// =============================================================================

void print_test_summary(test_result result) {
    safe_printf("\n================================================================\n");
    safe_printf("TEST SUMMARY\n");
    safe_printf("============\n");
    safe_printf("Total Tests: %d\n", result.passed + result.failed);
    safe_printf("Passed:      %d\n", result.passed);
    safe_printf("Failed:      %d\n", result.failed);
    safe_printf("Errors:      %d\n", atomic_load(&error_count));
    if (result.failed == 0 && atomic_load(&error_count) == 0) {
        safe_printf("ALL TESTS PASSED\n");
    } else {
        safe_printf("SOME TESTS FAILED\n");
    }
    safe_printf("================================================================\n");
}

int main(void) {
    test_result result = {0, 0, "Threadpool Tests"};

    safe_printf("Comprehensive Threadpool Test Suite\n");
    safe_printf("===================================\n\n");

    // Basic functionality
    RUN_TEST(test_basic_creation_destruction);
    RUN_TEST(test_invalid_parameters);
    RUN_TEST(test_single_thread_execution);
    RUN_TEST(test_multiple_thread_execution);

    // Concurrency and ordering
    RUN_TEST(test_task_ordering_independence);
    RUN_TEST(test_high_contention);
    RUN_TEST(test_thread_safety_validation);

    // Stress and edge cases
    RUN_TEST(test_queue_overflow_behavior);
    RUN_TEST(test_rapid_create_destroy);
    RUN_TEST(test_stress_test);

    // Resource management
    RUN_TEST(test_memory_leaks);

    safe_printf("\n--- threadpool_submit_batch ---\n\n");

    // Parameter and edge cases
    RUN_TEST(test_batch_null_pool);
    RUN_TEST(test_batch_zero_count);
    RUN_TEST(test_batch_null_args);
    RUN_TEST(test_batch_null_entries);

    // Correctness
    RUN_TEST(test_batch_single_task);
    RUN_TEST(test_batch_basic);
    RUN_TEST(test_batch_argument_delivery);

    // Capacity and chunking
    RUN_TEST(test_batch_large);

    // Multi-phase and concurrency
    RUN_TEST(test_batch_multiple_batches);
    RUN_TEST(test_batch_concurrent_submitters);

    print_test_summary(result);
    return (result.failed > 0 || atomic_load(&error_count) > 0) ? 1 : 0;
}
