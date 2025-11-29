#include "../include/hashset.h"
#include <assert.h>  // for assert
#include <stdio.h>   // for printf, fprintf
#include <stdlib.h>  // for exit, EXIT_FAILURE
#include <string.h>  // for strcmp

#define COLOR_RED    "\033[0;31m"
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_CYAN   "\033[0;36m"
#define COLOR_RESET  "\033[0m"

#define LOG_ERROR(fmt, ...)                                                                                            \
    fprintf(stderr, COLOR_RED "[ERROR]: %s:%d:%s(): " fmt COLOR_RESET "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_ASSERT(condition, fmt, ...)                                                                                \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            LOG_ERROR("Assertion failed: " #condition " " fmt, ##__VA_ARGS__);                                         \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

#define LOG_SECTION(name) printf("\n" COLOR_CYAN "=== %s ===" COLOR_RESET "\n", name)

/**
 * Macro to run a test function with automatic logging.
 */
#define RUN_TEST(test_func)                                                                                            \
    do {                                                                                                               \
        printf("  Running %-45s ... ", #test_func);                                                                    \
        fflush(stdout);                                                                                                \
        test_func();                                                                                                   \
        printf(COLOR_GREEN "PASSED" COLOR_RESET "\n");                                                                 \
    } while (0)

/* ============================================================================
 * Basic Creation and Destruction Tests
 * ========================================================================= */

static void test_hashset_create_default(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "Failed to create hash set");
    LOG_ASSERT(hashset_size(set) == 0, "New set should have size 0");
    LOG_ASSERT(hashset_capacity(set) == HASHSET_DEFAULT_CAPACITY, "Expected default capacity");
    LOG_ASSERT(hashset_isempty(set) == true, "New set should be empty");
    hashset_destroy(set);
}

static void test_hashset_create_custom_capacity(void) {
    hashset_t* set = hashset_create(sizeof(int), 64, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "Failed to create hash set");
    LOG_ASSERT(hashset_capacity(set) == 64, "Expected capacity 64");
    hashset_destroy(set);
}

static void test_hashset_create_invalid_keysize(void) {
    hashset_t* set = hashset_create(0, 16, nullptr, nullptr);
    LOG_ASSERT(set == nullptr, "Should fail with key_size = 0");
}

static void test_hashset_destroy_null(void) {
    hashset_destroy(nullptr);  // Should not crash
}

/* ============================================================================
 * Add and Contains Tests
 * ========================================================================= */

static void test_hashset_add_single_element(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int value = 42;
    LOG_ASSERT(hashset_add(set, &value) == true, "Failed to add element");
    LOG_ASSERT(hashset_size(set) == 1, "Size should be 1");
    LOG_ASSERT(hashset_contains(set, &value) == true, "Should contain 42");

    hashset_destroy(set);
}

static void test_hashset_add_multiple_elements(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int values[] = {1, 2, 3, 4, 5, 10, 20, 30, 40, 50};
    size_t count = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < count; i++) {
        LOG_ASSERT(hashset_add(set, &values[i]) == true, "Failed to add %d", values[i]);
    }

    LOG_ASSERT(hashset_size(set) == count, "Size should be %zu", count);

    for (size_t i = 0; i < count; i++) {
        LOG_ASSERT(hashset_contains(set, &values[i]) == true, "Should contain %d", values[i]);
    }

    hashset_destroy(set);
}

static void test_hashset_add_duplicate(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int value = 100;
    LOG_ASSERT(hashset_add(set, &value) == true, "Failed to add element");
    LOG_ASSERT(hashset_size(set) == 1, "");

    // Add duplicate - should succeed but not increase size
    LOG_ASSERT(hashset_add(set, &value) == true, "Add duplicate should succeed");
    LOG_ASSERT(hashset_size(set) == 1, "Size should remain 1");

    hashset_destroy(set);
}

static void test_hashset_add_null_params(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int value = 42;
    LOG_ASSERT(hashset_add(nullptr, &value) == false, "Should fail with null set");
    LOG_ASSERT(hashset_add(set, nullptr) == false, "Should fail with null key");

    hashset_destroy(set);
}

static void test_hashset_contains_not_found(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int values[] = {1, 2, 3};
    for (size_t i = 0; i < 3; i++) {
        hashset_add(set, &values[i]);
    }

    int missing = 99;
    LOG_ASSERT(hashset_contains(set, &missing) == false, "Should not contain 99");

    hashset_destroy(set);
}

/* ============================================================================
 * Remove Tests
 * ========================================================================= */

static void test_hashset_remove_existing(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int values[] = {10, 20, 30, 40, 50};
    for (size_t i = 0; i < 5; i++) {
        hashset_add(set, &values[i]);
    }

    int to_remove = 30;
    LOG_ASSERT(hashset_remove(set, &to_remove) == true, "Failed to remove 30");
    LOG_ASSERT(hashset_size(set) == 4, "Size should be 4");
    LOG_ASSERT(hashset_contains(set, &to_remove) == false, "Should not contain 30");

    // Other elements should still exist
    int check = 10;
    LOG_ASSERT(hashset_contains(set, &check) == true, "Should still contain 10");

    hashset_destroy(set);
}

static void test_hashset_remove_nonexistent(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int value = 42;
    hashset_add(set, &value);

    int missing = 999;
    LOG_ASSERT(hashset_remove(set, &missing) == false, "Should return false");
    LOG_ASSERT(hashset_size(set) == 1, "Size should remain 1");

    hashset_destroy(set);
}

static void test_hashset_remove_all(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int values[] = {1, 2, 3, 4, 5};
    for (size_t i = 0; i < 5; i++) {
        hashset_add(set, &values[i]);
    }

    for (size_t i = 0; i < 5; i++) {
        LOG_ASSERT(hashset_remove(set, &values[i]) == true, "Failed to remove %d", values[i]);
    }

    LOG_ASSERT(hashset_size(set) == 0, "Size should be 0");
    LOG_ASSERT(hashset_isempty(set) == true, "Set should be empty");

    hashset_destroy(set);
}

/* ============================================================================
 * Clear Tests
 * ========================================================================= */

static void test_hashset_clear(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (size_t i = 0; i < 10; i++) {
        hashset_add(set, &values[i]);
    }

    LOG_ASSERT(hashset_size(set) == 10, "");

    hashset_clear(set);

    LOG_ASSERT(hashset_size(set) == 0, "Size should be 0 after clear");
    LOG_ASSERT(hashset_isempty(set) == true, "Set should be empty");

    // Verify elements are gone
    for (size_t i = 0; i < 10; i++) {
        LOG_ASSERT(hashset_contains(set, &values[i]) == false, "Should not contain %d", values[i]);
    }

    // Can add new elements after clear
    int new_value = 999;
    LOG_ASSERT(hashset_add(set, &new_value) == true, "Should be able to add after clear");

    hashset_destroy(set);
}

/* ============================================================================
 * Rehashing Tests
 * ========================================================================= */

static void test_hashset_rehash_on_load(void) {
    // Start with small capacity to force rehash
    hashset_t* set = hashset_create(sizeof(int), 4, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    size_t initial_capacity = hashset_capacity(set);

    // Add enough elements to trigger rehash (load factor > 0.75)
    for (int i = 0; i < 20; i++) {
        LOG_ASSERT(hashset_add(set, &i) == true, "Failed to add %d", i);
    }

    LOG_ASSERT(hashset_size(set) == 20, "Size should be 20");
    LOG_ASSERT(hashset_capacity(set) > initial_capacity, "Capacity should have increased");

    // Verify all elements still accessible after rehash
    for (int i = 0; i < 20; i++) {
        LOG_ASSERT(hashset_contains(set, &i) == true, "Should contain %d after rehash", i);
    }

    hashset_destroy(set);
}

/* ============================================================================
 * Set Operations Tests
 * ========================================================================= */

static void test_hashset_union(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int valuesA[] = {1, 2, 3, 4, 5};
    int valuesB[] = {4, 5, 6, 7, 8};

    for (size_t i = 0; i < 5; i++) {
        hashset_add(setA, &valuesA[i]);
        hashset_add(setB, &valuesB[i]);
    }

    hashset_t* union_set = hashset_union(setA, setB);
    LOG_ASSERT(union_set != nullptr, "Union failed");
    LOG_ASSERT(hashset_size(union_set) == 8, "Union should have 8 elements");

    // Check all elements from both sets
    for (size_t i = 0; i < 5; i++) {
        LOG_ASSERT(hashset_contains(union_set, &valuesA[i]) == true, "");
        LOG_ASSERT(hashset_contains(union_set, &valuesB[i]) == true, "");
    }

    hashset_destroy(setA);
    hashset_destroy(setB);
    hashset_destroy(union_set);
}

static void test_hashset_intersection(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int valuesA[] = {1, 2, 3, 4, 5};
    int valuesB[] = {3, 4, 5, 6, 7};

    for (size_t i = 0; i < 5; i++) {
        hashset_add(setA, &valuesA[i]);
        hashset_add(setB, &valuesB[i]);
    }

    hashset_t* intersection_set = hashset_intersection(setA, setB);
    LOG_ASSERT(intersection_set != nullptr, "Intersection failed");
    LOG_ASSERT(hashset_size(intersection_set) == 3, "Intersection should have 3 elements");

    // Check common elements (3, 4, 5)
    int common[] = {3, 4, 5};
    for (size_t i = 0; i < 3; i++) {
        LOG_ASSERT(hashset_contains(intersection_set, &common[i]) == true, "Should contain %d", common[i]);
    }

    hashset_destroy(setA);
    hashset_destroy(setB);
    hashset_destroy(intersection_set);
}

static void test_hashset_difference(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int valuesA[] = {1, 2, 3, 4, 5};
    int valuesB[] = {3, 4, 5, 6, 7};

    for (size_t i = 0; i < 5; i++) {
        hashset_add(setA, &valuesA[i]);
        hashset_add(setB, &valuesB[i]);
    }

    hashset_t* diff_set = hashset_difference(setA, setB);
    LOG_ASSERT(diff_set != nullptr, "Difference failed");
    LOG_ASSERT(hashset_size(diff_set) == 2, "Difference should have 2 elements (1, 2)");

    // Check elements in A but not in B
    int expected[] = {1, 2};
    for (size_t i = 0; i < 2; i++) {
        LOG_ASSERT(hashset_contains(diff_set, &expected[i]) == true, "Should contain %d", expected[i]);
    }

    // Check elements not in difference
    int not_expected[] = {3, 4, 5};
    for (size_t i = 0; i < 3; i++) {
        LOG_ASSERT(hashset_contains(diff_set, &not_expected[i]) == false, "Should not contain %d", not_expected[i]);
    }

    hashset_destroy(setA);
    hashset_destroy(setB);
    hashset_destroy(diff_set);
}

static void test_hashset_symmetric_difference(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int valuesA[] = {1, 2, 3, 4, 5};
    int valuesB[] = {4, 5, 6, 7, 8};

    for (size_t i = 0; i < 5; i++) {
        hashset_add(setA, &valuesA[i]);
        hashset_add(setB, &valuesB[i]);
    }

    hashset_t* symdiff_set = hashset_symmetric_difference(setA, setB);
    LOG_ASSERT(symdiff_set != nullptr, "Symmetric difference failed");
    LOG_ASSERT(hashset_size(symdiff_set) == 6, "Should have 6 elements");

    // Elements in A or B but not both: {1, 2, 3, 6, 7, 8}
    int expected[] = {1, 2, 3, 6, 7, 8};
    for (size_t i = 0; i < 6; i++) {
        LOG_ASSERT(hashset_contains(symdiff_set, &expected[i]) == true, "Should contain %d", expected[i]);
    }

    // Common elements should not be in symmetric difference
    int not_expected[] = {4, 5};
    for (size_t i = 0; i < 2; i++) {
        LOG_ASSERT(hashset_contains(symdiff_set, &not_expected[i]) == false, "Should not contain %d", not_expected[i]);
    }

    hashset_destroy(setA);
    hashset_destroy(setB);
    hashset_destroy(symdiff_set);
}

/* ============================================================================
 * Subset Tests
 * ========================================================================= */

static void test_hashset_is_subset_true(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int valuesA[] = {2, 3, 4};
    int valuesB[] = {1, 2, 3, 4, 5};

    for (size_t i = 0; i < 3; i++) {
        hashset_add(setA, &valuesA[i]);
    }
    for (size_t i = 0; i < 5; i++) {
        hashset_add(setB, &valuesB[i]);
    }

    LOG_ASSERT(hashset_is_subset(setA, setB) == true, "A should be subset of B");
    LOG_ASSERT(hashset_is_subset(setB, setA) == false, "B should not be subset of A");

    hashset_destroy(setA);
    hashset_destroy(setB);
}

static void test_hashset_is_subset_equal_sets(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int values[] = {1, 2, 3, 4, 5};
    for (size_t i = 0; i < 5; i++) {
        hashset_add(setA, &values[i]);
        hashset_add(setB, &values[i]);
    }

    // Equal sets are subsets of each other
    LOG_ASSERT(hashset_is_subset(setA, setB) == true, "A should be subset of B");
    LOG_ASSERT(hashset_is_subset(setB, setA) == true, "B should be subset of A");

    hashset_destroy(setA);
    hashset_destroy(setB);
}

static void test_hashset_is_proper_subset(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int valuesA[] = {2, 3, 4};
    int valuesB[] = {1, 2, 3, 4, 5};

    for (size_t i = 0; i < 3; i++) {
        hashset_add(setA, &valuesA[i]);
    }
    for (size_t i = 0; i < 5; i++) {
        hashset_add(setB, &valuesB[i]);
    }

    LOG_ASSERT(hashset_is_proper_subset(setA, setB) == true, "A should be proper subset of B");
    LOG_ASSERT(hashset_is_proper_subset(setB, setA) == false, "B should not be proper subset of A");

    hashset_destroy(setA);
    hashset_destroy(setB);
}

static void test_hashset_is_proper_subset_equal_sets(void) {
    hashset_t* setA = hashset_create(sizeof(int), 0, nullptr, nullptr);
    hashset_t* setB = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(setA != nullptr && setB != nullptr, "");

    int values[] = {1, 2, 3};
    for (size_t i = 0; i < 3; i++) {
        hashset_add(setA, &values[i]);
        hashset_add(setB, &values[i]);
    }

    // Equal sets are not proper subsets
    LOG_ASSERT(hashset_is_proper_subset(setA, setB) == false, "Equal sets not proper subsets");

    hashset_destroy(setA);
    hashset_destroy(setB);
}

/* ============================================================================
 * Custom Hash and Equality Function Tests
 * ========================================================================= */

/** Custom string hash function. */
static uint64_t string_hash(const void* key, size_t key_size) {
    (void)key_size;  // String hash doesn't use key_size
    const char* str = *(const char**)key;
    uint64_t hash   = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (uint64_t)c;
    }
    return hash;
}

/** Custom string equality function. */
static bool string_equals(const void* a, const void* b, size_t key_size) {
    (void)key_size;
    const char* str_a = *(const char**)a;
    const char* str_b = *(const char**)b;
    return strcmp(str_a, str_b) == 0;
}

static void test_hashset_custom_string_hash(void) {
    hashset_t* set = hashset_create(sizeof(char*), 0, string_hash, string_equals);
    LOG_ASSERT(set != nullptr, "");

    const char* strings[] = {"hello", "world", "foo", "bar", "baz"};

    for (size_t i = 0; i < 5; i++) {
        LOG_ASSERT(hashset_add(set, &strings[i]) == true, "Failed to add string");
    }

    LOG_ASSERT(hashset_size(set) == 5, "Size should be 5");

    for (size_t i = 0; i < 5; i++) {
        LOG_ASSERT(hashset_contains(set, &strings[i]) == true, "Should contain '%s'", strings[i]);
    }

    const char* missing = "notfound";
    LOG_ASSERT(hashset_contains(set, &missing) == false, "Should not contain 'notfound'");

    hashset_destroy(set);
}

/* ============================================================================
 * Stress Tests
 * ========================================================================= */

static void test_hashset_large_dataset(void) {
    hashset_t* set = hashset_create(sizeof(int), 0, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    const size_t COUNT = 10000;

    // Add many elements
    for (size_t i = 0; i < COUNT; i++) {
        int value = (int)i;
        LOG_ASSERT(hashset_add(set, &value) == true, "Failed to add %zu", i);
    }

    LOG_ASSERT(hashset_size(set) == COUNT, "Size should be %zu", COUNT);

    // Verify all elements
    for (size_t i = 0; i < COUNT; i++) {
        int value = (int)i;
        LOG_ASSERT(hashset_contains(set, &value) == true, "Should contain %zu", i);
    }

    // Remove half
    for (size_t i = 0; i < COUNT / 2; i++) {
        int value = (int)i;
        LOG_ASSERT(hashset_remove(set, &value) == true, "Failed to remove %zu", i);
    }

    LOG_ASSERT(hashset_size(set) == COUNT / 2, "Size should be %zu", COUNT / 2);

    hashset_destroy(set);
}

static void test_hashset_collision_handling(void) {
    // Use very small capacity to force many collisions
    hashset_t* set = hashset_create(sizeof(int), 2, nullptr, nullptr);
    LOG_ASSERT(set != nullptr, "");

    int values[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19};

    for (size_t i = 0; i < 10; i++) {
        LOG_ASSERT(hashset_add(set, &values[i]) == true, "Failed to add %d", values[i]);
    }

    LOG_ASSERT(hashset_size(set) == 10, "Size should be 10");

    // Verify all elements accessible despite collisions
    for (size_t i = 0; i < 10; i++) {
        LOG_ASSERT(hashset_contains(set, &values[i]) == true, "Should contain %d", values[i]);
    }

    hashset_destroy(set);
}

/* ============================================================================
 * Main Test Runner
 * ========================================================================= */

int main(void) {
    printf(COLOR_YELLOW "\n╔════════════════════════════════════════════════════════╗\n");
    printf("║           Hash Set Test Suite                          ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n" COLOR_RESET);

    LOG_SECTION("Basic Creation and Destruction");
    RUN_TEST(test_hashset_create_default);
    RUN_TEST(test_hashset_create_custom_capacity);
    RUN_TEST(test_hashset_create_invalid_keysize);
    RUN_TEST(test_hashset_destroy_null);

    LOG_SECTION("Add and Contains Operations");
    RUN_TEST(test_hashset_add_single_element);
    RUN_TEST(test_hashset_add_multiple_elements);
    RUN_TEST(test_hashset_add_duplicate);
    RUN_TEST(test_hashset_add_null_params);
    RUN_TEST(test_hashset_contains_not_found);

    LOG_SECTION("Remove Operations");
    RUN_TEST(test_hashset_remove_existing);
    RUN_TEST(test_hashset_remove_nonexistent);
    RUN_TEST(test_hashset_remove_all);

    LOG_SECTION("Clear Operations");
    RUN_TEST(test_hashset_clear);

    LOG_SECTION("Rehashing");
    RUN_TEST(test_hashset_rehash_on_load);

    LOG_SECTION("Set Operations");
    RUN_TEST(test_hashset_union);
    RUN_TEST(test_hashset_intersection);
    RUN_TEST(test_hashset_difference);
    RUN_TEST(test_hashset_symmetric_difference);

    LOG_SECTION("Subset Operations");
    RUN_TEST(test_hashset_is_subset_true);
    RUN_TEST(test_hashset_is_subset_equal_sets);
    RUN_TEST(test_hashset_is_proper_subset);
    RUN_TEST(test_hashset_is_proper_subset_equal_sets);

    LOG_SECTION("Custom Hash Functions");
    RUN_TEST(test_hashset_custom_string_hash);

    LOG_SECTION("Stress Tests");
    RUN_TEST(test_hashset_large_dataset);
    RUN_TEST(test_hashset_collision_handling);

    printf(COLOR_YELLOW "\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  " COLOR_GREEN "All tests passed successfully!" COLOR_YELLOW "                        ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n" COLOR_RESET);

    return EXIT_SUCCESS;
}
