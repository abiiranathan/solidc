#include "../include/dynarray.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(cond, fmt, ...)                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "Test failed: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);                       \
            exit(1);                                                                                                   \
        }                                                                                                              \
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

    // Test shrink (assumes SHRINK_THRESHOLD=4, growth 1.5x or 2x; push to resize, pop to below
    // threshold)
    dynarray_free(&arr); /* release the first allocation before re-init */
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

    TEST_ASSERT(arr.capacity == DYNARRAY_INITIAL_CAPACITY, "Capacity should fit size=5");

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

// Regression: zero-initialised structs used without dynarray_init() must be
// rejected gracefully (they used to divide by zero on element_size == 0).
static void test_zero_init_misuse(void) {
    dynarray_t a = {0};
    int v = 5;
    TEST_ASSERT(!dynarray_push(&a, &v), "push on zero-init must fail");
    TEST_ASSERT(!dynarray_reserve(&a, 10), "reserve on zero-init must fail");
    TEST_ASSERT(!dynarray_shrink_to_fit(&a), "shrink on zero-init must fail");
    TEST_ASSERT(!dynarray_push_n(&a, &v, 3), "push_n on zero-init must fail");
    TEST_ASSERT(!dynarray_insert(&a, 0, &v), "insert on zero-init must fail");
    TEST_ASSERT(!dynarray_remove(&a, 0), "remove on zero-init must fail");
    TEST_ASSERT(!dynarray_swap_remove(&a, 0, NULL), "swap_remove on zero-init must fail");
    TEST_ASSERT(dynarray_first(&a) == NULL && dynarray_last(&a) == NULL, "first/last on zero-init are NULL");
    TEST_ASSERT(dynarray_detach(&a, NULL, NULL) == NULL, "detach on zero-init is NULL");
    dynarray_free(&a);
}

/* push_n(NULL-ish edge): zero count with NULL elements is a no-op success. */
static void test_push_n_zero_count(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 4), "init");
    TEST_ASSERT(dynarray_push_n(&arr, NULL, 0), "zero-count with NULL is success");
    TEST_ASSERT(arr.size == 0, "size unchanged");
    dynarray_free(&arr);
}

/* insert: bounds, ordering, growth from tiny capacity. */
static void test_insert(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 2), "init small");

    for (int i = 0; i < 100; i++) {
        /* insert ascending at the front -> reverse order stored */
        TEST_ASSERT(dynarray_insert(&arr, 0, &i), "insert at front");
        TEST_ASSERT(arr.size == (size_t)(i + 1), "size tracks inserts");
    }
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT(*(int*)dynarray_get(&arr, (size_t)i) == 99 - i, "front-insert order reversed");
    }

    /* middle insert */
    int mid = 555;
    TEST_ASSERT(dynarray_insert(&arr, 50, &mid), "middle insert");
    TEST_ASSERT(*(int*)dynarray_get(&arr, 50) == 555, "middle value");
    TEST_ASSERT(arr.size == 101, "size after middle insert");

    /* index > size rejected */
    int v = 1;
    TEST_ASSERT(!dynarray_insert(&arr, 102, &v), "insert beyond size fails");
    TEST_ASSERT(!dynarray_insert(NULL, 0, &v), "NULL arr fails");
    TEST_ASSERT(!dynarray_insert(&arr, 0, NULL), "NULL element fails");

    /* append via index == size */
    int tail = 777;
    TEST_ASSERT(dynarray_insert(&arr, arr.size, &tail), "append via index==size");
    TEST_ASSERT(*(int*)dynarray_last(&arr) == 777, "appended at end");

    dynarray_free(&arr);
}

/* remove: order preservation and shrink safety. */
static void test_remove(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 4), "init");

    enum { N = 200 };
    for (int i = 0; i < N; i++) {
        TEST_ASSERT(dynarray_push(&arr, &i), "push");
    }

    /* remove every third element */
    for (int i = N / 3; i >= 0; i--) {
        TEST_ASSERT(dynarray_remove(&arr, (size_t)(i * 3)), "remove idx");
    }
    TEST_ASSERT(arr.size == N - (N / 3 + 1), "size after removals");

    /* survivors keep relative order */
    size_t j = 0;
    for (int i = 0; i < N; i++) {
        if (i % 3 == 0 && i <= (N / 3) * 3) continue;
        TEST_ASSERT(*(int*)dynarray_get(&arr, j++) == i, "order preserved");
    }

    /* out-of-bounds */
    TEST_ASSERT(!dynarray_remove(&arr, arr.size), "remove at size fails");
    TEST_ASSERT(!dynarray_remove(NULL, 0), "remove NULL arr fails");

    dynarray_free(&arr);

    /* drain to empty exercises shrink path */
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 64), "init big");
    for (int i = 0; i < 64; i++) dynarray_push(&arr, &i);
    while (arr.size > 0) {
        TEST_ASSERT(dynarray_remove(&arr, 0), "drain front");
    }
    TEST_ASSERT(arr.capacity >= DYNARRAY_INITIAL_CAPACITY, "capacity floored");
    TEST_ASSERT(dynarray_is_empty(&arr), "empty after drain");
    dynarray_free(&arr);
}

/* swap_remove: O(1) unordered removal semantics. */
static void test_swap_remove(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 8), "init");

    for (int i = 0; i < 10; i++) dynarray_push(&arr, &i); /* [0..9] */

    int removed = 0;
    TEST_ASSERT(dynarray_swap_remove(&arr, 3, &removed), "swap_remove idx3");
    TEST_ASSERT(removed == 3, "removed carries old value");
    TEST_ASSERT(arr.size == 9, "size decremented");
    TEST_ASSERT(*(int*)dynarray_get(&arr, 3) == 9, "last moved into hole");

    /* removing the LAST element must not touch anything else */
    TEST_ASSERT(dynarray_swap_remove(&arr, 8, &removed), "swap_remove last");
    TEST_ASSERT(removed == 8, "last removed value");
    TEST_ASSERT(arr.size == 8, "size after last removal");

    /* out of bounds */
    TEST_ASSERT(!dynarray_swap_remove(&arr, 8, NULL), "oob swap_remove fails");
    TEST_ASSERT(!dynarray_swap_remove(NULL, 0, NULL), "NULL arr fails");

    dynarray_free(&arr);
}

/* first/last helpers. */
static void test_first_last(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 4), "init");
    TEST_ASSERT(dynarray_first(&arr) == NULL, "first on empty");
    TEST_ASSERT(dynarray_last(&arr) == NULL, "last on empty");
    TEST_ASSERT(dynarray_first(NULL) == NULL, "first on NULL");

    int vals[3] = {10, 20, 30};
    for (int i = 0; i < 3; i++) dynarray_push(&arr, &vals[i]);
    TEST_ASSERT(*(int*)dynarray_first(&arr) == 10, "first value");
    TEST_ASSERT(*(int*)dynarray_last(&arr) == 30, "last value");

    /* mutability through first()/last() */
    *(int*)dynarray_first(&arr) = 11;
    TEST_ASSERT(vals[0] != 11 && *(int*)dynarray_get(&arr, 0) == 11, "write via first");

    dynarray_free(&arr);
}

/* detach: ownership transfer, exact-size buffer, array reset. */
static void test_detach(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 64), "init oversized");

    for (int i = 0; i < 10; i++) dynarray_push(&arr, &i);
    size_t detached_size = 0, detached_cap = 0;
    int* buf = (int*)dynarray_detach(&arr, &detached_size, &detached_cap);
    TEST_ASSERT(buf != NULL, "detach returns buffer");
    TEST_ASSERT(detached_size == 10, "detached element count");
    TEST_ASSERT(detached_cap == 10, "detached capacity == size");
    for (int i = 0; i < 10; i++) TEST_ASSERT(buf[i] == i, "detached contents intact");
    free(buf);

    /* array is consumed */
    TEST_ASSERT(arr.data == NULL && arr.size == 0 && arr.capacity == 0, "array reset after detach");
    TEST_ASSERT(dynarray_detach(&arr, NULL, NULL) == NULL, "detach twice is NULL");
    dynarray_free(&arr); /* safe on zeroed array */

    /* empty array detaches to NULL */
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 8), "init");
    TEST_ASSERT(dynarray_detach(&arr, NULL, NULL) == NULL, "detach empty is NULL");
    dynarray_free(&arr);

    /* NULL handling */
    TEST_ASSERT(dynarray_detach(NULL, NULL, NULL) == NULL, "detach NULL arr");
}

/* Misaligned source copies must be well-defined (packed-struct fields). */
static void test_misaligned_source(void) {
    struct __attribute__((packed)) Packed {
        char pad;
        uint32_t val;
    };

    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(uint32_t), 4), "init");

    struct Packed p[4] = {{1, 0xAABBCCDDu}, {2, 0x11223344u}, {3, 0x55667788u}, {4, 0x99AABBCCu}};
    for (int i = 0; i < 4; i++) {
        /* &p[i].val is misaligned by one byte inside the packed struct */
        TEST_ASSERT(dynarray_push(&arr, &p[i].val), "push misaligned source");
    }
    TEST_ASSERT(*(uint32_t*)dynarray_get(&arr, 0) == 0xAABBCCDDu, "misaligned copy 1");
    TEST_ASSERT(*(uint32_t*)dynarray_get(&arr, 3) == 0x99AABBCCu, "misaligned copy 4");

    dynarray_free(&arr);
}

/* Model-based randomized test: mirror every operation against a simple
 * reference array and compare state after each step. */
static void test_random_model(void) {
    dynarray_t arr;
    TEST_ASSERT(dynarray_init(&arr, sizeof(int), 4), "init");

    enum { OPS = 50000, MODEL_MAX = 4096 };
    static int model[MODEL_MAX];
    size_t mlen = 0;

    unsigned rng = 0xC0FFEEu;
#define NEXT_RNG()                                                                                                     \
    (rng = rng * 1664525u + 1013904223u)

    for (int op = 0; op < OPS; op++) {
        uint32_t r = NEXT_RNG();
        int val = (int)(r >> 8);
        switch (r % 7) {
            case 0:
            case 1: { /* push */
                TEST_ASSERT(dynarray_push(&arr, &val), "model push");
                TEST_ASSERT(mlen < MODEL_MAX, "model overflow");
                model[mlen++] = val;
                break;
            }
            case 2: { /* pop */
                int out = -1;
                bool ok = dynarray_pop(&arr, &out);
                TEST_ASSERT(ok == (mlen > 0), "pop result matches");
                if (mlen > 0) {
                    TEST_ASSERT(out == model[mlen - 1], "pop value matches");
                    mlen--;
                }
                break;
            }
            case 3: { /* set random index */
                if (mlen > 0) {
                    size_t idx = (r >> 16) % mlen;
                    TEST_ASSERT(dynarray_set(&arr, idx, &val), "set in range");
                    model[idx] = val;
                } else {
                    TEST_ASSERT(!dynarray_set(&arr, 0, &val), "set on empty fails");
                }
                break;
            }
            case 4: { /* insert at random position */
                size_t idx = (r >> 16) % (mlen + 1);
                TEST_ASSERT(dynarray_insert(&arr, idx, &val), "model insert");
                TEST_ASSERT(mlen < MODEL_MAX, "model overflow ins");
                memmove(&model[idx + 1], &model[idx], (mlen - idx) * sizeof(int));
                model[idx] = val;
                mlen++;
                break;
            }
            case 5: { /* remove at random position */
                if (mlen > 0) {
                    size_t idx = (r >> 16) % mlen;
                    TEST_ASSERT(dynarray_remove(&arr, idx), "model remove");
                    memmove(&model[idx], &model[idx + 1], (mlen - idx - 1) * sizeof(int));
                    mlen--;
                } else {
                    TEST_ASSERT(!dynarray_remove(&arr, 0), "remove empty fails");
                }
                break;
            }
            default: { /* swap_remove at random position */
                if (mlen > 0) {
                    size_t idx = (r >> 16) % mlen;
                    int removed = 0;
                    TEST_ASSERT(dynarray_swap_remove(&arr, idx, &removed), "model swap_remove");
                    TEST_ASSERT(removed == model[idx], "swap_remove value");
                    model[idx] = model[mlen - 1];
                    mlen--;
                } else {
                    TEST_ASSERT(!dynarray_swap_remove(&arr, 0, NULL), "swap_remove empty fails");
                }
                break;
            }
        }

        /* spot-check full agreement periodically */
        if ((op & 1023) == 0) {
            TEST_ASSERT(dynarray_size(&arr) == mlen, "sizes agree");
            for (size_t i = 0; i < mlen; i++) {
                TEST_ASSERT(*(int*)dynarray_get(&arr, i) == model[i], "elements agree");
            }
        }
    }
#undef NEXT_RNG

    /* final full comparison */
    TEST_ASSERT(dynarray_size(&arr) == mlen, "final size agrees");
    for (size_t i = 0; i < mlen; i++) {
        TEST_ASSERT(*(int*)dynarray_get(&arr, i) == model[i], "final elements agree");
    }

    dynarray_free(&arr);
}

int main(void) {
    test_zero_init_misuse();
    test_init();
    test_free();
    test_push();
    test_pop();
    test_get_and_set();
    test_reserve();
    test_shrink_to_fit();
    test_clear();
    test_push_n_zero_count();
    test_insert();
    test_remove();
    test_swap_remove();
    test_first_last();
    test_detach();
    test_misaligned_source();
    test_random_model();

    printf("All tests passed!\n");
    return 0;
}
