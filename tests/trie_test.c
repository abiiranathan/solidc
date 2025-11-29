#include "../include/trie.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <stdint.h>
#include <stdio.h>   // for printf, fprintf
#include <stdlib.h>  // for EXIT_SUCCESS, EXIT_FAILURE
#include <string.h>  // for strcmp

// Test result tracking
static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/** ANSI color codes for test output. */
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_RED    "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_RESET  "\033[0m"

/**
 * Prints a test result message.
 * @param test_name Name of the test.
 * @param passed Whether the test passed.
 * @param message Optional failure message.
 */
static void print_test_result(const char* test_name, bool passed, const char* message) {
    tests_run++;
    if (passed) {
        tests_passed++;
        printf("%s[PASS]%s %s\n", COLOR_GREEN, COLOR_RESET, test_name);
    } else {
        tests_failed++;
        printf("%s[FAIL]%s %s", COLOR_RED, COLOR_RESET, test_name);
        if (message != nullptr) {
            printf(": %s", message);
        }
        printf("\n");
    }
}

/** Helper macro for running test assertions. */
#define TEST_ASSERT(condition, test_name, message) print_test_result(test_name, (condition), message)

/** Test: Create and destroy empty Trie. */
static void test_create_destroy(void) {
    trie_t* trie = trie_create();
    TEST_ASSERT(trie != nullptr, "test_create_destroy", "Failed to create Trie");

    if (trie != nullptr) {
        TEST_ASSERT(trie_is_empty(trie), "test_create_destroy_empty", "New Trie should be empty");
        TEST_ASSERT(trie_get_word_count(trie) == 0, "test_create_destroy_count", "Word count should be 0");
        trie_destroy(trie);
    }
}

/** Test: Insert single word. */
static void test_insert_single(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_insert_single", "Failed to create Trie");
        return;
    }

    bool result = trie_insert(trie, "hello");
    TEST_ASSERT(result, "test_insert_single_result", "Insert should succeed");
    TEST_ASSERT(trie_search(trie, "hello"), "test_insert_single_search", "Word should be found");
    TEST_ASSERT(trie_get_word_count(trie) == 1, "test_insert_single_count", "Word count should be 1");
    TEST_ASSERT(!trie_is_empty(trie), "test_insert_single_not_empty", "Trie should not be empty");

    trie_destroy(trie);
}

/** Test: Insert multiple words. */
static void test_insert_multiple(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_insert_multiple", "Failed to create Trie");
        return;
    }

    const char* words[]    = {"apple", "app", "application", "banana", "band"};
    const size_t num_words = sizeof(words) / sizeof(words[0]);

    for (size_t i = 0; i < num_words; i++) {
        bool result = trie_insert(trie, words[i]);
        TEST_ASSERT(result, "test_insert_multiple_insert", words[i]);
    }

    TEST_ASSERT(trie_get_word_count(trie) == num_words, "test_insert_multiple_count", "Word count mismatch");

    for (size_t i = 0; i < num_words; i++) {
        TEST_ASSERT(trie_search(trie, words[i]), "test_insert_multiple_search", words[i]);
    }

    trie_destroy(trie);
}

/** Test: Search for non-existent words. */
static void test_search_nonexistent(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_search_nonexistent", "Failed to create Trie");
        return;
    }

    trie_insert(trie, "hello");

    TEST_ASSERT(!trie_search(trie, "world"), "test_search_nonexistent_world", "Should not find 'world'");
    TEST_ASSERT(!trie_search(trie, "hel"), "test_search_nonexistent_prefix", "Should not find prefix 'hel'");
    TEST_ASSERT(!trie_search(trie, "helloo"), "test_search_nonexistent_longer", "Should not find 'helloo'");

    trie_destroy(trie);
}

/** Test: Prefix matching. */
static void test_starts_with(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_starts_with", "Failed to create Trie");
        return;
    }

    trie_insert(trie, "application");
    trie_insert(trie, "apple");

    TEST_ASSERT(trie_starts_with(trie, "app"), "test_starts_with_app", "Should find prefix 'app'");
    TEST_ASSERT(trie_starts_with(trie, "appl"), "test_starts_with_appl", "Should find prefix 'appl'");
    TEST_ASSERT(trie_starts_with(trie, "application"), "test_starts_with_full", "Should find full word as prefix");
    TEST_ASSERT(!trie_starts_with(trie, "ban"), "test_starts_with_nonexistent", "Should not find 'ban'");

    trie_destroy(trie);
}

/** Test: Delete operations. */
static void test_delete(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_delete", "Failed to create Trie");
        return;
    }

    trie_insert(trie, "hello");
    trie_insert(trie, "world");

    TEST_ASSERT(trie_get_word_count(trie) == 2, "test_delete_initial_count", "Should have 2 words");

    bool deleted = trie_delete(trie, "hello");
    TEST_ASSERT(deleted, "test_delete_result", "Delete should succeed");
    TEST_ASSERT(!trie_search(trie, "hello"), "test_delete_verify", "Word should not be found after deletion");
    TEST_ASSERT(trie_get_word_count(trie) == 1, "test_delete_count", "Word count should be 1");

    // Try to delete non-existent word
    deleted = trie_delete(trie, "nonexistent");
    TEST_ASSERT(!deleted, "test_delete_nonexistent", "Delete should fail for non-existent word");

    trie_destroy(trie);
}

/** Test: Frequency tracking. */
static void test_frequency(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_frequency", "Failed to create Trie");
        return;
    }

    // Insert same word multiple times
    trie_insert(trie, "test");
    trie_insert(trie, "test");
    trie_insert(trie, "test");

    uint32_t freq = trie_get_frequency(trie, "test");
    TEST_ASSERT(freq == 3, "test_frequency_count", "Frequency should be 3");
    TEST_ASSERT(trie_get_word_count(trie) == 1, "test_frequency_unique", "Word count should still be 1");

    // Check frequency of non-existent word
    freq = trie_get_frequency(trie, "nonexistent");
    TEST_ASSERT(freq == 0, "test_frequency_nonexistent", "Frequency should be 0");

    trie_destroy(trie);
}

/** Test: Edge cases with empty strings and nullptr. */
static void test_edge_cases(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_edge_cases", "Failed to create Trie");
        return;
    }

    // Test nullptr inputs
    TEST_ASSERT(!trie_insert(trie, nullptr), "test_edge_nullptr_insert", "Insert nullptr should fail");
    TEST_ASSERT(!trie_search(trie, nullptr), "test_edge_nullptr_search", "Search nullptr should fail");
    TEST_ASSERT(!trie_starts_with(trie, nullptr), "test_edge_nullptr_prefix", "Prefix nullptr should fail");

    // Test empty string
    TEST_ASSERT(!trie_insert(trie, ""), "test_edge_empty_insert", "Insert empty string should fail");
    TEST_ASSERT(!trie_search(trie, ""), "test_edge_empty_search", "Search empty string should fail");

    // Test nullptr trie
    TEST_ASSERT(!trie_insert(nullptr, "test"), "test_edge_nullptr_trie_insert", "Insert to nullptr trie should fail");
    TEST_ASSERT(!trie_search(nullptr, "test"), "test_edge_nullptr_trie_search", "Search in nullptr trie should fail");
    TEST_ASSERT(trie_is_empty(nullptr), "test_edge_nullptr_trie_empty", "nullptr trie should be considered empty");

    trie_destroy(trie);
    trie_destroy(nullptr);  // Should handle nullptr gracefully
}

/** Test: Special characters and extended ASCII. */
static void test_special_characters(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_special_characters", "Failed to create Trie");
        return;
    }

    // Test with special characters
    const char* special_words[] = {"hello-world", "test@example.com", "path/to/file", "100%", "C++"};

    const size_t num_special = sizeof(special_words) / sizeof(special_words[0]);

    for (size_t i = 0; i < num_special; i++) {
        bool result = trie_insert(trie, special_words[i]);
        TEST_ASSERT(result, "test_special_insert", special_words[i]);
    }

    for (size_t i = 0; i < num_special; i++) {
        TEST_ASSERT(trie_search(trie, special_words[i]), "test_special_search", special_words[i]);
    }

    trie_destroy(trie);
}

/** Test: Case sensitivity. */
static void test_case_sensitivity(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_case_sensitivity", "Failed to create Trie");
        return;
    }

    trie_insert(trie, "Hello");

    TEST_ASSERT(trie_search(trie, "Hello"), "test_case_exact", "Should find exact case match");
    TEST_ASSERT(!trie_search(trie, "hello"), "test_case_lower", "Should not find lowercase version");
    TEST_ASSERT(!trie_search(trie, "HELLO"), "test_case_upper", "Should not find uppercase version");

    trie_destroy(trie);
}

/** Test: Long words. */
static void test_long_words(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_long_words", "Failed to create Trie");
        return;
    }

    // Create a very long word (1000 characters)
    char long_word[1001];
    for (size_t i = 0; i < 1000; i++) {
        long_word[i] = 'a' + (i % 26);
    }
    long_word[1000] = '\0';

    bool result = trie_insert(trie, long_word);
    TEST_ASSERT(result, "test_long_insert", "Should insert long word");
    TEST_ASSERT(trie_search(trie, long_word), "test_long_search", "Should find long word");

    trie_destroy(trie);
}

/** Test: Autocomplete basic functionality. */
static void test_autocomplete_basic(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_autocomplete_basic", "Failed to create Trie");
        return;
    }

    // Create an arena for suggestions
    Arena* arena = arena_create(4096);
    if (arena == nullptr) {
        TEST_ASSERT(false, "test_autocomplete_basic", "Failed to create Arena");
        trie_destroy(trie);
        return;
    }

    // Insert test words
    const char* words[]    = {"app", "apple", "application", "apply", "apricot"};
    const size_t num_words = sizeof(words) / sizeof(words[0]);

    for (size_t i = 0; i < num_words; i++) {
        trie_insert(trie, words[i]);
    }

    // Test autocomplete with "app" prefix
    size_t count             = 0;
    const char** suggestions = trie_autocomplete(trie, "app", 10, &count, arena);

    TEST_ASSERT(suggestions != nullptr, "test_autocomplete_basic_not_null", "Should return suggestions");
    TEST_ASSERT(count == 4, "test_autocomplete_basic_count", "Should return 4 suggestions");

    if (suggestions != nullptr && count == 4) {
        // Verify all suggestions start with "app"
        bool all_valid = true;
        for (size_t i = 0; i < count; i++) {
            if (strncmp(suggestions[i], "app", 3) != 0) {
                all_valid = false;
                break;
            }
        }
        TEST_ASSERT(all_valid, "test_autocomplete_basic_prefix", "All suggestions should start with 'app'");
    }

    // Cleanup: Destroy arena frees all suggestions and the array itself
    arena_destroy(arena);
    trie_destroy(trie);
}

/** Test: Autocomplete with limit. */
static void test_autocomplete_limit(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_autocomplete_limit", "Failed to create Trie");
        return;
    }

    Arena* arena = arena_create(4096);
    if (arena == nullptr) {
        trie_destroy(trie);
        return;
    }

    // Insert many words with same prefix
    const char* words[]    = {"test1", "test2", "test3", "test4", "test5", "test6"};
    const size_t num_words = sizeof(words) / sizeof(words[0]);

    for (size_t i = 0; i < num_words; i++) {
        trie_insert(trie, words[i]);
    }

    // Request only 3 suggestions
    size_t count             = 0;
    const char** suggestions = trie_autocomplete(trie, "test", 3, &count, arena);

    TEST_ASSERT(suggestions != nullptr, "test_autocomplete_limit_not_null", "Should return suggestions");
    TEST_ASSERT(count == 3, "test_autocomplete_limit_count", "Should return exactly 3");

    arena_destroy(arena);
    trie_destroy(trie);
}

/** Test: Autocomplete with non-existent prefix. */
static void test_autocomplete_nonexistent(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_autocomplete_nonexistent", "Failed to create Trie");
        return;
    }

    Arena* arena = arena_create(4096);
    if (arena == nullptr) {
        trie_destroy(trie);
        return;
    }

    trie_insert(trie, "hello");
    trie_insert(trie, "world");

    size_t count             = 0;
    const char** suggestions = trie_autocomplete(trie, "xyz", 10, &count, arena);

    TEST_ASSERT(suggestions == nullptr, "test_autocomplete_nonexistent_null",
                "Should return nullptr for non-existent prefix");
    TEST_ASSERT(count == 0, "test_autocomplete_nonexistent_count", "Count should be 0");

    arena_destroy(arena);
    trie_destroy(trie);
}

/** Test: Autocomplete with exact word as prefix. */
static void test_autocomplete_exact_prefix(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_autocomplete_exact_prefix", "Failed to create Trie");
        return;
    }

    Arena* arena = arena_create(4096);
    if (arena == nullptr) {
        trie_destroy(trie);
        return;
    }

    trie_insert(trie, "test");
    trie_insert(trie, "testing");
    trie_insert(trie, "tester");

    // Use exact word "test" as prefix
    size_t count             = 0;
    const char** suggestions = trie_autocomplete(trie, "test", 10, &count, arena);

    TEST_ASSERT(suggestions != nullptr, "test_autocomplete_exact_not_null", "Should return suggestions");
    TEST_ASSERT(count == 3, "test_autocomplete_exact_count", "Should return 3 suggestions");

    if (suggestions != nullptr) {
        // Verify "test" itself is included
        bool found_exact = false;
        for (size_t i = 0; i < count; i++) {
            if (strcmp(suggestions[i], "test") == 0) {
                found_exact = true;
                break;
            }
        }
        TEST_ASSERT(found_exact, "test_autocomplete_exact_included", "Should include exact match 'test'");
    }

    arena_destroy(arena);
    trie_destroy(trie);
}

/** Test: Autocomplete with empty trie. */
static void test_autocomplete_empty(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_autocomplete_empty", "Failed to create Trie");
        return;
    }

    Arena* arena = arena_create(4096);
    if (arena == nullptr) {
        trie_destroy(trie);
        return;
    }

    size_t count             = 0;
    const char** suggestions = trie_autocomplete(trie, "test", 10, &count, arena);

    TEST_ASSERT(suggestions == nullptr, "test_autocomplete_empty_null", "Should return nullptr for empty trie");
    TEST_ASSERT(count == 0, "test_autocomplete_empty_count", "Count should be 0");

    arena_destroy(arena);
    trie_destroy(trie);
}

/** Test: Autocomplete edge cases. */
static void test_autocomplete_edge_cases(void) {
    trie_t* trie = trie_create();
    if (trie == nullptr) {
        TEST_ASSERT(false, "test_autocomplete_edge_cases", "Failed to create Trie");
        return;
    }

    Arena* arena = arena_create(4096);
    if (arena == nullptr) {
        trie_destroy(trie);
        return;
    }

    trie_insert(trie, "test");

    size_t count = 0;

    // Test nullptr trie
    const char** suggestions = trie_autocomplete(nullptr, "test", 10, &count, arena);
    TEST_ASSERT(suggestions == nullptr, "test_autocomplete_edge_null_trie", "nullptr trie should return nullptr");

    // Test nullptr prefix
    suggestions = trie_autocomplete(trie, nullptr, 10, &count, arena);
    TEST_ASSERT(suggestions == nullptr, "test_autocomplete_edge_null_prefix", "nullptr prefix should return nullptr");

    // Test nullptr out_count
    suggestions = trie_autocomplete(trie, "test", 10, nullptr, arena);
    TEST_ASSERT(suggestions == nullptr, "test_autocomplete_edge_null_count", "nullptr out_count should return nullptr");

    // Test zero max_suggestions
    suggestions = trie_autocomplete(trie, "test", 0, &count, arena);
    TEST_ASSERT(suggestions == nullptr, "test_autocomplete_edge_zero_max",
                "Zero max_suggestions should return nullptr");

    // Test nullptr arena
    suggestions = trie_autocomplete(trie, "test", 10, &count, nullptr);
    TEST_ASSERT(suggestions == nullptr, "test_autocomplete_edge_null_arena", "nullptr arena should return nullptr");

    arena_destroy(arena);
    trie_destroy(trie);
}

/** Prints test summary. */
static void print_summary(void) {
    printf("\n");
    printf("======================================\n");
    printf("Test Summary\n");
    printf("======================================\n");
    printf("Total tests run:    %s%d%s\n", COLOR_YELLOW, tests_run, COLOR_RESET);
    printf("Tests passed:       %s%d%s\n", COLOR_GREEN, tests_passed, COLOR_RESET);
    printf("Tests failed:       %s%d%s\n", COLOR_RED, tests_failed, COLOR_RESET);
    printf("======================================\n");

    if (tests_failed == 0) {
        printf("%s✓ All tests passed!%s\n", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%s✗ Some tests failed%s\n", COLOR_RED, COLOR_RESET);
    }
}

/**
 * Main test runner.
 * @return EXIT_SUCCESS if all tests pass, EXIT_FAILURE otherwise.
 */
int main(void) {
    printf("Running Trie Data Structure Tests...\n\n");

    // Run all test suites
    test_create_destroy();
    test_insert_single();
    test_insert_multiple();
    test_search_nonexistent();
    test_starts_with();
    test_delete();
    test_frequency();
    test_edge_cases();
    test_special_characters();
    test_case_sensitivity();
    test_long_words();
    test_autocomplete_basic();
    test_autocomplete_limit();
    test_autocomplete_nonexistent();
    test_autocomplete_exact_prefix();
    test_autocomplete_empty();
    test_autocomplete_edge_cases();

    // Print results
    print_summary();

    return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
