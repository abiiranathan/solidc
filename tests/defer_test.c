/**
 * @file defer_test.c
 * @brief Comprehensive test suite for defer.h implementation
 * 
 * This test verifies that defer works correctly in various scenarios:
 * - Basic cleanup
 * - LIFO execution order (Last In, First Out)
 * - Nested blocks
 * - Early returns
 * - Variable capture
 * - Multiple defers in same scope
 */

#include "../include/defer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global test state
static int cleanup_order[10];
static int cleanup_index = 0;

void reset_test_state(void) {
    memset(cleanup_order, 0, sizeof(cleanup_order));
    cleanup_index = 0;
}

void record_cleanup(int value) {
    if (cleanup_index < 10) {
        cleanup_order[cleanup_index++] = value;
    }
}

int verify_cleanup_order(const int *expected, int count) {
    if (cleanup_index != count) {
        printf("FAIL: Expected %d cleanups, got %d\n", count, cleanup_index);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (cleanup_order[i] != expected[i]) {
            printf("FAIL: cleanup_order[%d] = %d, expected %d\n", i, cleanup_order[i], expected[i]);
            return 0;
        }
    }

    return 1;
}

// Test 1: Basic defer functionality
int test_basic_defer(void) {
    printf("\n=== Test 1: Basic Defer ===\n");
    reset_test_state();

    {
        defer {
            printf("  Cleanup executed\n");
            record_cleanup(1);
        };
        printf("  Main code executing\n");
    }

    int expected[] = {1};
    if (verify_cleanup_order(expected, 1)) {
        printf("PASS: Basic defer\n");
        return 1;
    }
    return 0;
}

// Test 2: LIFO execution order
int test_lifo_order(void) {
    printf("\n=== Test 2: LIFO Order (Last In, First Out) ===\n");
    reset_test_state();

    {
        defer {
            printf("  First defer (executes last)\n");
            record_cleanup(1);
        };

        defer {
            printf("  Second defer (executes second)\n");
            record_cleanup(2);
        };

        defer {
            printf("  Third defer (executes first)\n");
            record_cleanup(3);
        };

        printf("  Main code executing\n");
    }

    int expected[] = {3, 2, 1};  // Reverse order
    if (verify_cleanup_order(expected, 3)) {
        printf("PASS: LIFO order\n");
        return 1;
    }
    return 0;
}

// Test 3: Nested blocks
int test_nested_blocks(void) {
    printf("\n=== Test 3: Nested Blocks ===\n");
    reset_test_state();

    {
        printf("  Outer block start\n");
        defer {
            printf("  Outer defer (executes last)\n");
            record_cleanup(1);
        };

        {
            printf("  Inner block start\n");
            defer {
                printf("  Inner defer 1 (executes second)\n");
                record_cleanup(2);
            };

            defer {
                printf("  Inner defer 2 (executes first)\n");
                record_cleanup(3);
            };

            printf("  Inner block end\n");
        }

        printf("  Back in outer block\n");
    }
    printf("  After outer block\n");

    int expected[] = {3, 2, 1};  // Inner defers run first, then outer
    if (verify_cleanup_order(expected, 3)) {
        printf("PASS: Nested blocks\n");
        return 1;
    }
    return 0;
}

// Test 4: Variable capture
int test_variable_capture(void) {
    printf("\n=== Test 4: Variable Capture ===\n");
    reset_test_state();

    {
        int captured_value = 42;
        void *ptr = malloc(100);

        defer {
            printf("  Captured value: %d\n", captured_value);
            printf("  Freeing pointer: %p\n", ptr);
            free(ptr);
            record_cleanup(captured_value);
        };

        printf("  Using captured_value: %d\n", captured_value);
        captured_value = 99;  // Modify after defer registration
        printf("  Modified captured_value: %d\n", captured_value);
    }

    int expected[] = {99};  // Should capture the modified value
    if (verify_cleanup_order(expected, 1)) {
        printf("PASS: Variable capture\n");
        return 1;
    }
    return 0;
}

// Test 5: Early return
int test_early_return_helper(int should_return_early) {
    printf("  Helper function start\n");

    defer {
        printf("  Cleanup 1 (always executes)\n");
        record_cleanup(1);
    };

    defer {
        printf("  Cleanup 2 (always executes)\n");
        record_cleanup(2);
    };

    if (should_return_early) {
        printf("  Returning early\n");
        return 42;
    }

    printf("  Normal execution path\n");
    return 0;
}

int test_early_return(void) {
    printf("\n=== Test 5: Early Return ===\n");

    // Test early return
    reset_test_state();
    int result = test_early_return_helper(1);
    int expected_early[] = {2, 1};

    if (result != 42 || !verify_cleanup_order(expected_early, 2)) {
        printf("FAIL: Early return\n");
        return 0;
    }

    // Test normal path
    reset_test_state();
    result = test_early_return_helper(0);
    int expected_normal[] = {2, 1};

    if (result != 0 || !verify_cleanup_order(expected_normal, 2)) {
        printf("FAIL: Normal path\n");
        return 0;
    }

    printf("PASS: Early return\n");
    return 1;
}

// Test 6: Resource cleanup pattern
int test_resource_cleanup(void) {
    printf("\n=== Test 6: Resource Cleanup Pattern ===\n");
    reset_test_state();

    {
        // Allocate multiple resources
        void *mem1 = malloc(64);
        if (!mem1) return 0;
        defer {
            printf("  Freeing mem1\n");
            free(mem1);
            record_cleanup(1);
        };

        void *mem2 = malloc(128);
        if (!mem2) return 0;
        defer {
            printf("  Freeing mem2\n");
            free(mem2);
            record_cleanup(2);
        };

        void *mem3 = malloc(256);
        if (!mem3) return 0;
        defer {
            printf("  Freeing mem3\n");
            free(mem3);
            record_cleanup(3);
        };

        printf("  All resources allocated\n");
    }

    int expected[] = {3, 2, 1};  // Cleanup in reverse allocation order
    if (verify_cleanup_order(expected, 3)) {
        printf("PASS: Resource cleanup\n");
        return 1;
    }
    return 0;
}

// Test 7: Deeply nested blocks
int test_deep_nesting(void) {
    printf("\n=== Test 7: Deeply Nested Blocks ===\n");
    reset_test_state();

    {
        printf("  Level 0\n");
        defer {
            record_cleanup(0);
            printf("  Cleanup level 0\n");
        };

        {
            printf("  Level 1\n");
            defer {
                record_cleanup(1);
                printf("  Cleanup level 1\n");
            };

            {
                printf("  Level 2\n");
                defer {
                    record_cleanup(2);
                    printf("  Cleanup level 2\n");
                };

                {
                    printf("  Level 3\n");
                    defer {
                        record_cleanup(3);
                        printf("  Cleanup level 3\n");
                    };
                }
            }
        }
    }

    int expected[] = {3, 2, 1, 0};
    if (verify_cleanup_order(expected, 4)) {
        printf("PASS: Deep nesting\n");
        return 1;
    }
    return 0;
}

// Test 8: Mixed code and defers
int test_mixed_code(void) {
    printf("\n=== Test 8: Mixed Code and Defers ===\n");
    reset_test_state();

    {
        int value = 10;
        printf("  Initial value: %d\n", value);

        defer {
            printf("  Defer 1 sees value: %d\n", value);
            record_cleanup(value);
        };

        value = 20;
        printf("  Modified value: %d\n", value);

        defer {
            printf("  Defer 2 sees value: %d\n", value);
            record_cleanup(value);
        };

        value = 30;
        printf("  Final value: %d\n", value);
    }

    int expected[] = {30, 30};  // Both see the final value
    if (verify_cleanup_order(expected, 2)) {
        printf("PASS: Mixed code\n");
        return 1;
    }
    return 0;
}

int main(void) {
    printf("========================================\n");
    printf("  DEFER TEST SUITE\n");
    printf("========================================\n");

    int passed = 0;
    int total = 8;

    passed += test_basic_defer();
    passed += test_lifo_order();
    passed += test_nested_blocks();
    passed += test_variable_capture();
    passed += test_early_return();
    passed += test_resource_cleanup();
    passed += test_deep_nesting();
    passed += test_mixed_code();

    printf("\n========================================\n");
    printf("  RESULTS: %d/%d tests passed\n", passed, total);
    printf("========================================\n");

    return (passed == total) ? 0 : 1;
}
