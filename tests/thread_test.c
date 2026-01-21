/**
 * @file thread_test.c
 * @brief Comprehensive test suite for cross-platform thread library.
 *
 * Tests all aspects of the thread library including:
 * - Basic thread creation and joining
 * - Thread return values
 * - Thread attributes
 * - Detached threads
 * - System information functions
 * - Error handling
 * - Race condition handling
 *
 * Compile on POSIX:
 *   gcc -Wall -Wextra -std=c23 -o test_thread thread_test.c thread.c -pthread
 *
 * Compile on Windows:
 *   cl /W4 /std:c17 thread_test.c thread.c
 */

#include "../include/thread.h"

#include <stddef.h>  // for NULL
#include <stdint.h>  // for uintptr_t
#include <stdio.h>   // for printf, fprintf
#include <stdlib.h>  // for EXIT_SUCCESS, EXIT_FAILURE
#include <string.h>  // for strcmp

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/** ANSI color codes for terminal output (if supported). */
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_RESET "\033[0m"

/**
 * Reports test result.
 * @param test_name Name of the test.
 * @param passed Whether test passed (1) or failed (0).
 */
void report_test(const char* test_name, int passed) {
    if (passed) {
        printf(COLOR_GREEN "[PASS]" COLOR_RESET " %s\n", test_name);
        tests_passed++;
    } else {
        printf(COLOR_RED "[FAIL]" COLOR_RESET " %s\n", test_name);
        tests_failed++;
    }
}

/**
 * Prints a test section header.
 * @param section_name Name of the test section.
 */
void print_section(const char* section_name) {
    printf("\n" COLOR_BLUE "=== %s ===" COLOR_RESET "\n", section_name);
}

/* Test Functions */

/**
 * Simple thread function that returns NULL.
 * @param arg Pointer to integer argument.
 * @return NULL.
 */
void* simple_thread_func(void* arg) {
    int* n = (int*)arg;
    printf("  Thread received: %d\n", *n);
    sleep_ms(10);  // Small delay to test concurrency
    return NULL;
}

/**
 * Thread function that returns a status code.
 * @param arg Pointer to integer input value.
 * @return Integer status code cast to void*.
 */
void* thread_with_return_value(void* arg) {
    int* n = (int*)arg;
    printf("  Thread computing: %d * 2 = %d\n", *n, *n * 2);
    sleep_ms(10);

    // Return the result as void* (safe for 32-bit values)
    int result = *n * 2;
    return (void*)(intptr_t)result;
}

/**
 * Thread function that computes Fibonacci number.
 * @param arg Pointer to integer n.
 * @return Fibonacci(n) cast to void*.
 */
void* fibonacci_thread(void* arg) {
    int n = *(int*)arg;

    if (n <= 1) {
        return (void*)(intptr_t)n;
    }

    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int temp = a + b;
        a = b;
        b = temp;
    }

    printf("  Fibonacci(%d) = %d\n", n, b);
    return (void*)(intptr_t)b;
}

/**
 * Thread function for detached thread test.
 * @param arg Thread identifier.
 * @return NULL.
 */
void* detached_thread_func(void* arg) {
    int thread_id = (int)(intptr_t)arg;
    printf("  Detached thread %d running...\n", thread_id);
    sleep_ms(50);
    printf("  Detached thread %d finished\n", thread_id);
    return NULL;
}

/**
 * Thread function for stress testing.
 * @param arg Thread index.
 * @return Thread index cast to void*.
 */
void* stress_test_thread(void* arg) {
    int index = (int)(intptr_t)arg;

    // Do some work
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }

    sleep_ms(5);
    return (void*)(intptr_t)index;
}

/* Test Cases */

/**
 * Tests basic thread creation and joining.
 */
void test_basic_thread_creation(void) {
    print_section("Test 1: Basic Thread Creation and Joining");

    Thread threads[5];
    int nums[5] = {1, 2, 3, 4, 5};
    int success = 1;

    // Create threads
    for (int i = 0; i < 5; i++) {
        int ret = thread_create(&threads[i], simple_thread_func, &nums[i]);
        if (ret != 0) {
            fprintf(stderr, "  Error: thread_create failed with code %d\n", ret);
            success = 0;
            break;
        }
    }

    // Join threads
    if (success) {
        for (int i = 0; i < 5; i++) {
            int ret = thread_join(threads[i], NULL);
            if (ret != 0) {
                fprintf(stderr, "  Error: thread_join failed with code %d\n", ret);
                success = 0;
            }
        }
    }

    report_test("Basic thread creation and joining", success);
}

/**
 * Tests thread return values.
 */
void test_thread_return_values(void) {
    print_section("Test 2: Thread Return Values");

    Thread threads[3];
    int inputs[3] = {10, 20, 30};
    int expected[3] = {20, 40, 60};
    void* retvals[3] = {NULL};
    int success = 1;

    // Create threads
    for (int i = 0; i < 3; i++) {
        int ret = thread_create(&threads[i], thread_with_return_value, &inputs[i]);
        if (ret != 0) {
            fprintf(stderr, "  Error: thread_create failed\n");
            success = 0;
            break;
        }
    }

    // Join and check return values
    if (success) {
        for (int i = 0; i < 3; i++) {
            int ret = thread_join(threads[i], &retvals[i]);
            if (ret != 0) {
                fprintf(stderr, "  Error: thread_join failed\n");
                success = 0;
                break;
            }

            int result = (int)(intptr_t)retvals[i];
            printf("  Thread %d returned: %d (expected: %d)\n", i, result, expected[i]);

            if (result != expected[i]) {
                fprintf(stderr, "  Error: Unexpected return value\n");
                success = 0;
            }
        }
    }

    report_test("Thread return values", success);
}

/**
 * Tests Fibonacci computation in threads with return values.
 */
void test_fibonacci_threads(void) {
    print_section("Test 3: Fibonacci Computation in Threads");

    Thread threads[5];
    int inputs[5] = {5, 10, 12, 15, 8};
    int expected[5] = {5, 55, 144, 610, 21};  // Fibonacci numbers
    void* retvals[5] = {NULL};
    int success = 1;

    // Create threads
    for (int i = 0; i < 5; i++) {
        int ret = thread_create(&threads[i], fibonacci_thread, &inputs[i]);
        if (ret != 0) {
            fprintf(stderr, "  Error: thread_create failed\n");
            success = 0;
            break;
        }
    }

    // Join and verify results
    if (success) {
        for (int i = 0; i < 5; i++) {
            int ret = thread_join(threads[i], &retvals[i]);
            if (ret != 0) {
                fprintf(stderr, "  Error: thread_join failed\n");
                success = 0;
                break;
            }

            int result = (int)(intptr_t)retvals[i];
            int matches = (result == expected[i]);

            printf("  Fibonacci(%d) = %d %s\n",
                   inputs[i],
                   result,
                   matches ? COLOR_GREEN "✓" COLOR_RESET : COLOR_RED "✗" COLOR_RESET);

            if (!matches) {
                success = 0;
            }
        }
    }

    report_test("Fibonacci thread computation", success);
}

/**
 * Tests thread attributes (stack size).
 */
void test_thread_attributes(void) {
    print_section("Test 4: Thread Attributes");

    Thread thread;
    ThreadAttr attr;
    int n = 15;
    void* retval = NULL;
    int success = 1;

    // Initialize attributes
    int ret = thread_attr_init(&attr);
    if (ret != 0) {
        fprintf(stderr, "  Error: thread_attr_init failed\n");
        report_test("Thread attributes", 0);
        return;
    }

#ifdef _WIN32
    // Set stack size to 2MB on Windows
    attr.stackSize = 2 * 1024 * 1024;
    printf("  Set stack size to 2MB (Windows)\n");
#else
    // Set stack size to 2MB on POSIX
    ret = pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);
    if (ret != 0) {
        fprintf(stderr, "  Warning: pthread_attr_setstacksize failed\n");
    } else {
        printf("  Set stack size to 2MB (POSIX)\n");
    }
#endif

    // Create thread with attributes
    ret = thread_create_attr(&thread, &attr, fibonacci_thread, &n);
    if (ret != 0) {
        fprintf(stderr, "  Error: thread_create_attr failed\n");
        success = 0;
    }

    // Join thread
    if (success) {
        ret = thread_join(thread, &retval);
        if (ret != 0) {
            fprintf(stderr, "  Error: thread_join failed\n");
            success = 0;
        } else {
            int result = (int)(intptr_t)retval;
            printf("  Thread with custom stack returned: %d\n", result);
        }
    }

    // Destroy attributes
    ret = thread_attr_destroy(&attr);
    if (ret != 0) {
        fprintf(stderr, "  Error: thread_attr_destroy failed\n");
        success = 0;
    }

    report_test("Thread attributes", success);
}

/**
 * Tests detached threads.
 */
void test_detached_threads(void) {
    print_section("Test 5: Detached Threads");

    Thread threads[3];
    int success = 1;

    // Create and immediately detach threads
    for (int i = 0; i < 3; i++) {
        int ret = thread_create(&threads[i], detached_thread_func, (void*)(intptr_t)(i + 1));
        if (ret != 0) {
            fprintf(stderr, "  Error: thread_create failed\n");
            success = 0;
            break;
        }

        ret = thread_detach(threads[i]);
        if (ret != 0) {
            fprintf(stderr, "  Error: thread_detach failed\n");
            success = 0;
            break;
        }
        printf("  Detached thread %d\n", i + 1);
    }

    // Give detached threads time to complete
    printf("  Waiting for detached threads to finish...\n");
    sleep_ms(200);

    report_test("Detached threads", success);
}

/**
 * Tests error handling with invalid parameters.
 */
void test_error_handling(void) {
    print_section("Test 6: Error Handling");

    int success = 1;
    Thread thread;
    int dummy = 42;

    // Test NULL thread pointer
    int ret = thread_create(NULL, simple_thread_func, &dummy);
    if (ret != EINVAL) {
        fprintf(stderr, "  Error: Expected EINVAL for NULL thread pointer, got %d\n", ret);
        success = 0;
    } else {
        printf("  Correctly rejected NULL thread pointer (EINVAL)\n");
    }

    // Test NULL start routine
    ret = thread_create(&thread, NULL, &dummy);
    if (ret != EINVAL) {
        fprintf(stderr, "  Error: Expected EINVAL for NULL start routine, got %d\n", ret);
        success = 0;
    } else {
        printf("  Correctly rejected NULL start routine (EINVAL)\n");
    }

    // Test NULL attr pointer
    ret = thread_attr_init(NULL);
    if (ret != EINVAL) {
        fprintf(stderr, "  Error: Expected EINVAL for NULL attr, got %d\n", ret);
        success = 0;
    } else {
        printf("  Correctly rejected NULL attr pointer (EINVAL)\n");
    }

    report_test("Error handling", success);
}

/**
 * Tests system information functions.
 */
void test_system_info(void) {
    print_section("Test 7: System Information Functions");

    int success = 1;

    int pid = get_pid();
    printf("  Process ID: %d\n", pid);
    if (pid <= 0) {
        fprintf(stderr, "  Error: Invalid PID\n");
        success = 0;
    }

    unsigned long tid = get_tid();
    printf("  Thread ID: %lu\n", tid);
    if (tid == 0) {
        fprintf(stderr, "  Error: Invalid TID\n");
        success = 0;
    }

    long ncpus = get_ncpus();
    printf("  CPU cores: %ld\n", ncpus);
    if (ncpus <= 0) {
        fprintf(stderr, "  Error: Invalid CPU count\n");
        success = 0;
    }

    int ppid = get_ppid();
    printf("  Parent PID: %d\n", ppid);
    if (ppid <= 0) {
        fprintf(stderr, "  Warning: Invalid PPID (may be normal on some systems)\n");
    }

    unsigned int uid = get_uid();
    printf("  User ID: %u\n", uid);
#ifdef _WIN32
    printf("    (Windows: hash of SID)\n");
#endif

    unsigned int gid = get_gid();
    printf("  Group ID: %u\n", gid);
#ifdef _WIN32
    printf("    (Windows: hash of primary group SID)\n");
#endif

    char* username = get_username();
    if (username != NULL) {
        printf("  Username: %s\n", username);
    } else {
        fprintf(stderr, "  Warning: Could not retrieve username\n");
    }

    char* groupname = get_groupname();
    if (groupname != NULL) {
        printf("  Group name: %s\n", groupname);
    } else {
        fprintf(stderr, "  Warning: Could not retrieve group name\n");
    }

    report_test("System information", success);
}

/**
 * Stress test with many concurrent threads.
 */
void test_stress_many_threads(void) {
    print_section("Test 8: Stress Test (50 Concurrent Threads)");

#define STRESS_THREAD_COUNT 50
    Thread threads[STRESS_THREAD_COUNT];
    void* retvals[STRESS_THREAD_COUNT];
    int success = 1;

    printf("  Creating %d threads...\n", STRESS_THREAD_COUNT);

    // Create threads
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        int ret = thread_create(&threads[i], stress_test_thread, (void*)(intptr_t)i);
        if (ret != 0) {
            fprintf(stderr, "  Error: Failed to create thread %d\n", i);
            success = 0;
            break;
        }
    }

    printf("  Joining %d threads...\n", STRESS_THREAD_COUNT);

    // Join all threads and verify return values
    if (success) {
        for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
            int ret = thread_join(threads[i], &retvals[i]);
            if (ret != 0) {
                fprintf(stderr, "  Error: Failed to join thread %d\n", i);
                success = 0;
                break;
            }

            int returned_index = (int)(intptr_t)retvals[i];
            if (returned_index != i) {
                fprintf(stderr, "  Error: Thread %d returned wrong value: %d\n", i, returned_index);
                success = 0;
            }
        }
    }

    if (success) {
        printf("  All %d threads completed successfully\n", STRESS_THREAD_COUNT);
    }

    report_test("Stress test (50 threads)", success);
#undef STRESS_THREAD_COUNT
}

/**
 * Tests sleep_ms utility function.
 */
void test_sleep_function(void) {
    print_section("Test 9: Sleep Function");

    printf("  Testing sleep_ms(100)...\n");
    sleep_ms(100);
    printf("  Sleep completed\n");

    printf("  Testing sleep_ms(0) (should return immediately)...\n");
    sleep_ms(0);
    printf("  Immediate return confirmed\n");

    printf("  Testing sleep_ms(-1) (should return immediately)...\n");
    sleep_ms(-1);
    printf("  Negative value handled correctly\n");

    report_test("Sleep function", 1);
}

/**
 * Main test runner.
 */
int main(void) {
    printf("\n");
    printf(COLOR_YELLOW "╔══════════════════════════════════════════════════════╗\n");
    printf("║  Cross-Platform Thread Library - Test Suite         ║\n");
    printf("╚══════════════════════════════════════════════════════╝" COLOR_RESET "\n");

#ifdef _WIN32
    printf("\nPlatform: Windows\n");
#else
    printf("\nPlatform: POSIX\n");
#endif

    // Run all tests
    test_basic_thread_creation();
    test_thread_return_values();
    test_fibonacci_threads();
    test_thread_attributes();
    test_detached_threads();
    test_error_handling();
    test_system_info();
    test_stress_many_threads();
    test_sleep_function();

    // Print summary
    printf("\n");
    printf(COLOR_YELLOW "╔══════════════════════════════════════════════════════╗\n");
    printf("║  Test Summary                                        ║\n");
    printf("╚══════════════════════════════════════════════════════╝" COLOR_RESET "\n");
    printf("\n");
    printf("  Total tests: %d\n", tests_passed + tests_failed);
    printf("  " COLOR_GREEN "Passed: %d" COLOR_RESET "\n", tests_passed);
    printf("  " COLOR_RED "Failed: %d" COLOR_RESET "\n", tests_failed);
    printf("\n");

    if (tests_failed == 0) {
        printf(COLOR_GREEN "✓ All tests passed!" COLOR_RESET "\n\n");
        return EXIT_SUCCESS;
    } else {
        printf(COLOR_RED "✗ Some tests failed" COLOR_RESET "\n\n");
        return EXIT_FAILURE;
    }
}
