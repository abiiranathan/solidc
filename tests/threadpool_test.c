#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/threadpool.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Test configuration
#define MAX_THREADS         16
#define MAX_TASKS           10000
#define STRESS_DURATION_SEC 5

// Global test state
static atomic_int test_counter         = 0;
static atomic_int completed_tasks      = 0;
static atomic_int error_count          = 0;
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
        // Spin wait
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);

    atomic_flag_clear(&print_lock);
}

// Test helper macros
#define TEST_ASSERT(condition, message)                                                            \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            safe_printf("ASSERTION FAILED: %s at %s:%d\n", message, __FILE__, __LINE__);           \
            atomic_fetch_add(&error_count, 1);                                                     \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)

#define RUN_TEST(test_func)                                                                        \
    do {                                                                                           \
        safe_printf("Running %s... ", #test_func);                                                 \
        if (test_func()) {                                                                         \
            safe_printf("PASSED\n");                                                               \
            result.passed++;                                                                       \
        } else {                                                                                   \
            safe_printf("FAILED\n");                                                               \
            result.failed++;                                                                       \
        }                                                                                          \
    } while (0)

// =============================================================================
// Test Tasks
// =============================================================================

void simple_task(void* arg) {
    volatile int* value = (int*)arg;
    atomic_fetch_add(&completed_tasks, 1);
    // Small delay to simulate work
    for (volatile int i = 0; i < 1000; i++)
        ;

    (void)value;
}

void counter_task(void* arg) {
    atomic_fetch_add(&test_counter, 1);
    atomic_fetch_add(&completed_tasks, 1);
    (void)arg;
}

void sleep_task(void* arg) {
    int ms             = *(int*)arg;
    struct timespec ts = {0, ms * 1000000};
    nanosleep(&ts, NULL);
    atomic_fetch_add(&completed_tasks, 1);
}

void error_task(void* arg) {
    (void)arg;

    // Simulate potential race condition
    static atomic_int shared_resource = 0;
    int old_val                       = atomic_load(&shared_resource);

    // Intentional delay to increase chance of race
    for (volatile int i = 0; i < 100; i++)
        ;

    // This should be atomic but we're testing it's handled correctly
    atomic_store(&shared_resource, old_val + 1);
    atomic_fetch_add(&completed_tasks, 1);
}

void stress_task(void* arg) {
    (void)arg;

    if (!atomic_load(&stress_test_running)) return;

    // CPU-intensive work
    volatile uint64_t result = 0;
    for (uint64_t i = 0; i < 10000; i++) {
        result += i * i;
    }

    atomic_fetch_add(&completed_tasks, 1);
}

// =============================================================================
// Individual Tests
// =============================================================================

int test_basic_creation_destruction() {
    Threadpool* pool = threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");

    threadpool_destroy(pool);
    return 1;
}

int test_invalid_parameters() {
    // Test creation with 0 threads (should default to 1)
    Threadpool* pool = threadpool_create(0);
    TEST_ASSERT(pool != NULL, "Pool should handle 0 threads gracefully");
    threadpool_destroy(pool);

    // Test NULL pool submission
    int result = threadpool_submit(NULL, simple_task, NULL);
    TEST_ASSERT(result != 0, "Should reject NULL pool");

    // Test NULL function submission
    pool   = threadpool_create(2);
    result = threadpool_submit(pool, NULL, NULL);
    TEST_ASSERT(result != 0, "Should reject NULL function");
    threadpool_destroy(pool);

    return 1;
}

int test_single_thread_execution() {
    Threadpool* pool = threadpool_create(1);
    TEST_ASSERT(pool != NULL, "Single thread pool creation");

    atomic_store(&completed_tasks, 0);
    int values[5] = {1, 2, 3, 4, 5};

    for (int i = 0; i < 5; i++) {
        int result = threadpool_submit(pool, simple_task, &values[i]);
        TEST_ASSERT(result == 0, "Task submission should succeed");
    }

    threadpool_destroy(pool);

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
        int result = threadpool_submit(pool, counter_task, NULL);
        TEST_ASSERT(result == 0, "Task submission should succeed");
    }

    threadpool_destroy(pool);

    int final_counter   = atomic_load(&test_counter);
    int final_completed = atomic_load(&completed_tasks);

    TEST_ASSERT(final_counter == num_tasks, "Counter should equal number of tasks");
    TEST_ASSERT(final_completed == num_tasks, "All tasks should complete");

    return 1;
}

int test_task_ordering_independence() {
    Threadpool* pool = threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Pool creation for ordering test");

    atomic_store(&completed_tasks, 0);

    // Submit tasks with different execution times
    int delays[]        = {50, 10, 30, 5, 100, 1, 75, 25};  // milliseconds
    const int num_tasks = sizeof(delays) / sizeof(delays[0]);

    for (int i = 0; i < num_tasks; i++) {
        int result = threadpool_submit(pool, sleep_task, &delays[i]);
        TEST_ASSERT(result == 0, "Sleep task submission should succeed");
    }

    threadpool_destroy(pool);

    TEST_ASSERT(atomic_load(&completed_tasks) == num_tasks, "All sleep tasks should complete");
    return 1;
}

int test_high_contention() {
    Threadpool* pool = threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Pool creation for contention test");

    atomic_store(&completed_tasks, 0);

    const int num_tasks = 1000;
    for (int i = 0; i < num_tasks; i++) {
        int result = threadpool_submit(pool, error_task, NULL);
        TEST_ASSERT(result == 0, "High contention task submission should succeed");
    }

    threadpool_destroy(pool);

    TEST_ASSERT(atomic_load(&completed_tasks) == num_tasks, "All contention tasks should complete");
    return 1;
}

int test_queue_overflow_behavior() {
    Threadpool* pool = threadpool_create(2);
    TEST_ASSERT(pool != NULL, "Pool creation for overflow test");

    atomic_store(&completed_tasks, 0);

    // Submit many long-running tasks to potentially overflow queues
    int long_delay             = 100;  // 100ms
    int successful_submissions = 0;

    for (int i = 0; i < 20; i++) {
        int result = threadpool_submit(pool, sleep_task, &long_delay);
        if (result == 0) {
            successful_submissions++;
        }
    }

    threadpool_destroy(pool);

    // Should have submitted at least some tasks successfully
    TEST_ASSERT(successful_submissions > 0, "Should submit some tasks even under pressure");
    TEST_ASSERT(atomic_load(&completed_tasks) == successful_submissions,
                "All submitted tasks should complete");

    return 1;
}

int test_rapid_create_destroy() {
    for (int i = 0; i < 50; i++) {
        Threadpool* pool = threadpool_create(4);
        TEST_ASSERT(pool != NULL, "Rapid pool creation should succeed");

        // Submit a few quick tasks
        for (int j = 0; j < 10; j++) {
            threadpool_submit(pool, simple_task, NULL);
        }

        threadpool_destroy(pool);
    }
    return 1;
}

int test_stress_test() {
    Threadpool* pool = threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Stress test pool creation");

    atomic_store(&completed_tasks, 0);
    atomic_store(&stress_test_running, true);

    time_t start_time   = time(NULL);
    int submitted_tasks = 0;

    // Submit tasks continuously for the stress duration
    while (time(NULL) - start_time < STRESS_DURATION_SEC) {
        if (threadpool_submit(pool, stress_task, NULL) == 0) {
            submitted_tasks++;
        }

        // Occasional brief pause to avoid overwhelming
        if (submitted_tasks % 1000 == 0) {
            struct timespec ts = {0, 1000000};  // 1ms
            nanosleep(&ts, NULL);
        }
    }

    atomic_store(&stress_test_running, false);
    threadpool_destroy(pool);

    int completed = atomic_load(&completed_tasks);
    safe_printf("Stress test: submitted %d, completed %d tasks in %d seconds\n", submitted_tasks,
                completed, STRESS_DURATION_SEC);

    TEST_ASSERT(completed > 0, "Should complete some tasks during stress test");
    return 1;
}

int test_thread_safety_validation() {
    Threadpool* pool = threadpool_create(16);
    TEST_ASSERT(pool != NULL, "Thread safety test pool creation");

    atomic_store(&test_counter, 0);
    atomic_store(&completed_tasks, 0);

    const int num_tasks = 10000;

    // Submit tasks that increment a shared counter
    for (int i = 0; i < num_tasks; i++) {
        int result = threadpool_submit(pool, counter_task, NULL);
        TEST_ASSERT(result == 0, "Thread safety task submission should succeed");
    }

    threadpool_destroy(pool);

    int final_counter   = atomic_load(&test_counter);
    int final_completed = atomic_load(&completed_tasks);

    TEST_ASSERT(final_counter == num_tasks, "Atomic counter should be consistent");
    TEST_ASSERT(final_completed == num_tasks, "All thread safety tasks should complete");

    return 1;
}

// =============================================================================
// Memory and Resource Tests
// =============================================================================

int test_memory_leaks() {
    // Create and destroy many pools to check for leaks
    for (int i = 0; i < 100; i++) {
        Threadpool* pool = threadpool_create(4);
        TEST_ASSERT(pool != NULL, "Memory test pool creation");

        // Submit some tasks
        for (int j = 0; j < 50; j++) {
            threadpool_submit(pool, simple_task, NULL);
        }

        threadpool_destroy(pool);
    }

    safe_printf("Memory leak test completed (check with valgrind for verification)\n");
    return 1;
}

// =============================================================================
// Main Test Runner
// =============================================================================

void print_test_summary(test_result result) {
    safe_printf(
        "\n"
        "="
        "================================================================\n");
    safe_printf("TEST SUMMARY\n");
    safe_printf("============\n");
    safe_printf("Total Tests: %d\n", result.passed + result.failed);
    safe_printf("Passed: %d\n", result.passed);
    safe_printf("Failed: %d\n", result.failed);
    safe_printf("Errors: %d\n", atomic_load(&error_count));

    if (result.failed == 0 && atomic_load(&error_count) == 0) {
        safe_printf("🎉 ALL TESTS PASSED! 🎉\n");
    } else {
        safe_printf("❌ SOME TESTS FAILED ❌\n");
    }
    safe_printf("================================================================\n");
}

int main(void) {
    test_result result = {0, 0, "Threadpool Tests"};

    safe_printf("Comprehensive Threadpool Test Suite\n");
    safe_printf("===================================\n\n");

    // Basic functionality tests
    RUN_TEST(test_basic_creation_destruction);
    RUN_TEST(test_invalid_parameters);
    RUN_TEST(test_single_thread_execution);
    RUN_TEST(test_multiple_thread_execution);

    // Concurrency and ordering tests
    RUN_TEST(test_task_ordering_independence);
    RUN_TEST(test_high_contention);
    RUN_TEST(test_thread_safety_validation);

    // Stress and edge case tests
    RUN_TEST(test_queue_overflow_behavior);
    RUN_TEST(test_rapid_create_destroy);
    RUN_TEST(test_stress_test);

    // Resource management tests
    RUN_TEST(test_memory_leaks);

    print_test_summary(result);

    return (result.failed > 0 || atomic_load(&error_count) > 0) ? 1 : 0;
}
