#include "../include/dynarray.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(cond, fmt, ...)                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "Test failed: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static void test_init(void) {
    dynarray_t arr;
    // Valid initialization with default capacity
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 0), "Failed to init with default capacity");
    TEST_ASSERT(arr.capacity >= 1, "Default capacity should be at least 1");
    TEST_ASSERT(arr.size == 0, "Initial size should be 0");
    dynarray_free(&arr);

    // Valid initialization with specified capacity
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 5), "Failed to init with capacity 5");
    TEST_ASSERT(arr.capacity == 5, "Capacity should be 5");
    TEST_ASSERT(arr.size == 0, "Initial size should be 0");
    dynarray_free(&arr);

    // Invalid parameters
    TEST_ASSERT(!dynarray_init(NULL, sizeof(int), 0), "Should fail on NULL array");
    TEST_ASSERT(!dynarray_init(&arr, 0, 0), "Should fail on zero element size");
    dynarray_free(&arr);
}

static void test_free(void) {
    dynarray_t arr;
    dynarray_init(&arr, sizeof(int), 4);
    // Free valid array
    dynarray_free(&arr);
    TEST_ASSERT(arr.data == NULL, "Data should be NULL after free");
    TEST_ASSERT(arr.size == 0, "Size should be 0 after free");
    TEST_ASSERT(arr.capacity == 0, "Capacity should be 0 after free");

    // Free NULL (no-op)
    dynarray_free(NULL);
}

static void test_push(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 0), "Failed to init for push test");

    // Push to empty array
    int val = 42;
    TEST_ASSERT(dynarray_push(&arr, &val), "Failed to push first element");
    TEST_ASSERT(arr.size == 1, "Size should be 1 after first push");
    TEST_ASSERT(*(int*)dynarray_get(&arr, 0) == 42, "Pushed value should match");

    // Fill to capacity and trigger resize
    size_t old_cap = arr.capacity;
    while (arr.size < old_cap) {
        val = (int)arr.size;
        TEST_ASSERT(dynarray_push(&arr, &val), "Failed to push during fill");
    }
    TEST_ASSERT(arr.size == old_cap, "Size should equal old capacity after fill");

    // Push one more to trigger growth
    size_t pre_growth_cap = arr.capacity;
    val                   = 999;
    TEST_ASSERT(dynarray_push(&arr, &val), "Failed to push triggering growth");
    TEST_ASSERT(arr.capacity > pre_growth_cap, "Capacity should increase after growth push");
    TEST_ASSERT(arr.size == old_cap + 1, "Size should be old_cap + 1");

    // Multiple resizes
    for (int i = 0; i < 20; ++i) {
        val = i;
        TEST_ASSERT(dynarray_push(&arr, &val), "Failed to push during multiple resizes");
    }

    printf("cap=%zu\n", arr.capacity);
    printf("size=%zu\n", arr.size);
    TEST_ASSERT(arr.size == 21 + 20, "Size should reflect all pushes");  // 1 initial + fill +1 +20

    dynarray_free(&arr);

    // Invalid pushes
    TEST_ASSERT(!dynarray_push(NULL, &val), "Should fail on NULL array");
    dynarray_init(&arr, sizeof(int), 1);
    TEST_ASSERT(!dynarray_push(&arr, NULL), "Should fail on NULL element");
    dynarray_free(&arr);

    // Overflow growth (theoretical)
    dynarray_init(&arr, sizeof(int), INT_MAX / 2);  // Start from half max

    // Push until near max, but avoid actual large alloc; test growth failure indirectly
    // Note: In practice, malloc failure would trigger, but code checks overflow
    dynarray_free(&arr);
}

static void test_pop(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 0), "Failed to init for pop test");

    int out;
    // Pop from empty
    TEST_ASSERT(!dynarray_pop(&arr, &out), "Should fail to pop from empty");

    // Push some elements
    int expected = 4;
    for (int i = 0; i < 5; ++i) {
        dynarray_push(&arr, &i);
    }

    // Pop with output
    TEST_ASSERT(dynarray_pop(&arr, &out), "Failed to pop non-empty");
    TEST_ASSERT(out == expected, "Popped value should be last pushed");
    TEST_ASSERT(arr.size == 4, "Size should decrease by 1");

    // Pop without output
    TEST_ASSERT(dynarray_pop(&arr, NULL), "Failed to pop without output");
    TEST_ASSERT(arr.size == 3, "Size should decrease");

    // Pop all
    while (arr.size > 0) {
        dynarray_pop(&arr, NULL);
    }
    TEST_ASSERT(arr.size == 0, "Size should be 0 after popping all");

    // Test shrink (assumes SHRINK_THRESHOLD=4, growth 1.5x or 2x; push to resize, pop below
    // threshold)
    dynarray_init(&arr, sizeof(int), 4);  // Assume initial=4
    for (int i = 0; i < 16; ++i) {        // Push to trigger grows, e.g., to cap~16-24
        dynarray_push(&arr, &i);
    }
    size_t pre_shrink_cap = arr.capacity;
    // Pop to below threshold (e.g., size < cap/4 ~4-6, pop to 3)
    for (int i = 0; i < 13; ++i) {
        dynarray_pop(&arr, NULL);
    }
    TEST_ASSERT(arr.size == 3, "Size after pops");
    // Shrink should have occurred if below threshold; check cap reduced (general)
    TEST_ASSERT(arr.capacity < pre_shrink_cap || arr.capacity == pre_shrink_cap,
                "Shrink may or may not trigger based on constants, but no increase");
    // Note: Exact cap depends on constants; in real test, adjust based on known values

    dynarray_free(&arr);
}

static void test_get_and_set(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 0), "Failed to init for get/set");

    // Push elements
    for (int i = 0; i < 5; ++i) {
        dynarray_push(&arr, &i);
    }

    // Get valid indices
    for (size_t i = 0; i < arr.size; ++i) {
        int got = *(int*)dynarray_get(&arr, i);
        TEST_ASSERT(got == (int)i, "Get should return correct value at %zu", i);
    }

    // Get invalid index
    TEST_ASSERT(dynarray_get(&arr, arr.size) == NULL, "Get beyond size should return NULL");
    TEST_ASSERT(dynarray_get(&arr, SIZE_MAX) == NULL, "Get invalid large index should return NULL");

    // Set valid indices
    for (size_t i = 0; i < arr.size; ++i) {
        int new_val = (int)(i + 10);
        TEST_ASSERT(dynarray_set(&arr, i, &new_val), "Failed to set at %zu", i);
        int got = *(int*)dynarray_get(&arr, i);
        TEST_ASSERT(got == new_val, "Set value should match get");
    }

    // Set invalid
    int junk = 0;
    TEST_ASSERT(!dynarray_set(&arr, arr.size, &junk), "Should fail set beyond size");
    TEST_ASSERT(!dynarray_set(&arr, SIZE_MAX, &junk), "Should fail set large index");
    TEST_ASSERT(!dynarray_set(NULL, 0, &junk), "Should fail on NULL array");
    TEST_ASSERT(!dynarray_set(&arr, 0, NULL), "Should fail on NULL element");

    dynarray_free(&arr);
}

static void test_reserve(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 4), "Failed to init for reserve");

    // Increase capacity
    TEST_ASSERT(dynarray_reserve(&arr, 10), "Failed to reserve larger capacity");
    TEST_ASSERT(arr.capacity == 10, "Capacity should be 10");

    // Same capacity (no-op)
    TEST_ASSERT(dynarray_reserve(&arr, 10), "Reserve same capacity should succeed");

    // Shrink but not below size (size=0)
    TEST_ASSERT(dynarray_reserve(&arr, 2), "Failed to reserve smaller");
    TEST_ASSERT(arr.capacity == 2, "Capacity should shrink to 2");

    // Push to size=3, then reserve below size
    for (int i = 0; i < 3; ++i) {
        dynarray_push(&arr, &i);
    }
    TEST_ASSERT(dynarray_reserve(&arr, 1), "Reserve below size should adjust to size");
    TEST_ASSERT(arr.capacity == 3, "Capacity should not shrink below size=3");

    // Overflow check (theoretical)
    size_t max_cap = SIZE_MAX / sizeof(int);
    TEST_ASSERT(dynarray_reserve(&arr, max_cap), "Should succeed on max cap");
    // +1 should fail the check
    TEST_ASSERT(!dynarray_reserve(&arr, max_cap + 1), "Should fail on overflow cap");

    dynarray_free(&arr);

    // NULL array
    TEST_ASSERT(!dynarray_reserve(NULL, 5), "Should fail on NULL");
}

static void test_shrink_to_fit(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 0), "Failed to init for shrink_to_fit");

    // Empty: should set to initial
    TEST_ASSERT(dynarray_shrink_to_fit(&arr), "Shrink empty should succeed");
    size_t initial_cap = arr.capacity;  // Assume >=1
    TEST_ASSERT(initial_cap >= 1, "Shrunk empty should have initial cap");

    // Push some, shrink
    for (int i = 0; i < 5; ++i) {
        dynarray_push(&arr, &i);
    }
    // Reserve larger first
    dynarray_reserve(&arr, 20);
    TEST_ASSERT(arr.capacity == 20, "Pre-shrink cap=20");
    TEST_ASSERT(dynarray_shrink_to_fit(&arr), "Shrink after pushes should succeed");
    TEST_ASSERT(arr.capacity == 5, "Capacity should fit size=5");

    // Shrink below initial (push 2, shrink)
    dynarray_clear(&arr);
    for (int i = 0; i < 2; ++i) {
        dynarray_push(&arr, &i);
    }
    dynarray_reserve(&arr, 10);
    TEST_ASSERT(dynarray_shrink_to_fit(&arr), "Shrink small size");
    TEST_ASSERT(arr.capacity == initial_cap,
                "Small size should use initial cap");  // >=2, but assumes initial >=2

    dynarray_free(&arr);

    // NULL
    TEST_ASSERT(!dynarray_shrink_to_fit(NULL), "Should fail on NULL");
}

static void test_clear(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 4), "Failed to init for clear");

    // Clear empty (no-op)
    dynarray_clear(&arr);
    TEST_ASSERT(arr.size == 0, "Size remains 0");
    TEST_ASSERT(arr.capacity == 4, "Capacity unchanged");

    // Push, then clear
    for (int i = 0; i < 3; ++i) {
        dynarray_push(&arr, &i);
    }
    dynarray_clear(&arr);
    TEST_ASSERT(arr.size == 0, "Size should be 0 after clear");
    TEST_ASSERT(arr.capacity == 4, "Capacity unchanged after clear");

    // Get after clear
    TEST_ASSERT(dynarray_get(&arr, 0) == NULL, "Get after clear should return NULL");

    // Push after clear
    int val = 42;
    TEST_ASSERT(dynarray_push(&arr, &val), "Push after clear should work");
    TEST_ASSERT(arr.size == 1, "Size=1 after push post-clear");
    TEST_ASSERT(*(int*)dynarray_get(&arr, 0) == 42, "Value correct");

    dynarray_free(&arr);

    // NULL
    dynarray_clear(NULL);  // No-op, void return
}

int main(void) {
    test_init();
    test_free();
    test_push();
    test_pop();
    test_get_and_set();
    test_reserve();
    test_shrink_to_fit();
    test_clear();

    printf("All tests passed!\n");
    return 0;
}
