#include "../include/cstr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Helper function to print cstr content for debugging
void print_cstr(const cstr* s) {
    if (!s) {
        printf("NULL\n");
    } else {
        printf("\"%s\" (len=%zu, cap=%zu, heap=%d)\n", cstr_data_const(s), cstr_len(s), cstr_capacity(s),
               cstr_allocated(s));
    }
}

// Helper function to compare cstr with expected C-string
void assert_cstr_equals(const cstr* s, const char* expected, const char* test_name) {
    if (!s && !expected) {
        return;
    }

    assert(s && expected && "cstr or expected is NULL");
    const char* data = cstr_data_const(s);
    assert(data);

    bool matches = strcmp(data, expected) == 0 && "Content mismatch";
    assert(matches);

    assert(cstr_len(s) == strlen(expected) && "Length mismatch");
    printf("%s: Passed\n", test_name);
}

// Helper function to free an array of cstrs
void free_cstr_array(cstr** arr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        cstr_free(arr[i]);
    }
    free(arr);
}

int main(void) {
    printf("Starting cstr library tests...\n\n");

    // Test str_new
    {
        printf("Testing str_new...\n");
        cstr* s = cstr_init(0);
        assert_cstr_equals(s, "", "str_new with zero capacity");
        assert(cstr_capacity(s) >= 1);
        cstr_free(s);

        s = cstr_init(20);
        assert_cstr_equals(s, "", "str_new with large capacity");
        assert(cstr_capacity(s) >= 20);
        cstr_free(s);
    }

    // Test cstr_new
    {
        printf("\nTesting cstr_new...\n");
        cstr* s = cstr_new("Hello");
        assert_cstr_equals(s, "Hello", "cstr_new with valid string");
        cstr_free(s);

        s = cstr_new("");
        assert_cstr_equals(s, "", "cstr_new with empty string");
        cstr_free(s);

        s = cstr_new(NULL);
        assert(s == NULL && "cstr_new with NULL should return NULL");
        printf("cstr_new with NULL: Passed\n");
    }

    // Test str_format
    {
        printf("\nTesting str_format...\n");
        cstr* s = cstr_format("Hello, %s! %d", "World", 42);
        assert_cstr_equals(s, "Hello, World! 42", "str_format with valid format");
        cstr_free(s);

        s = cstr_format("");
        assert_cstr_equals(s, "", "str_format with empty format");
        cstr_free(s);
    }

    // Test str_free
    {
        printf("\nTesting str_free...\n");
        cstr* s = cstr_new("Test");
        cstr_free(s);
        printf("str_free: Passed (no crash expected)\n");

        cstr_free(NULL);
        printf("str_free with NULL: Passed (no crash expected)\n");
    }

    // Test str_len and str_capacity
    {
        printf("\nTesting str_len and str_capacity...\n");
        cstr* s = cstr_new("Hello");
        assert(cstr_len(s) == 5 && "str_len incorrect");
        assert(cstr_capacity(s) >= 6 && "str_capacity incorrect");
        printf("str_len and str_capacity: Passed\n");
        cstr_free(s);

        assert(cstr_len(NULL) == 0 && "str_len with NULL incorrect");
        assert(cstr_capacity(NULL) == 0 && "str_capacity with NULL incorrect");
        printf("str_len and str_capacity with NULL: Passed\n");
    }

    // Test str_empty
    {
        printf("\nTesting str_empty...\n");
        cstr* s = cstr_new("");
        assert(cstr_empty(s) && "str_empty with empty string");
        cstr_free(s);

        s = cstr_new("NonEmpty");
        assert(!cstr_empty(s) && "str_empty with non-empty string");
        cstr_free(s);

        assert(cstr_empty(NULL) && "str_empty with NULL");
        printf("str_empty: Passed\n");
    }

    // Test str_resize
    {
        printf("\nTesting str_resize...\n");
        cstr* s = cstr_new("Test");
        assert(cstr_resize(s, 10) && "str_resize failed");
        assert(cstr_capacity(s) >= 10 && "str_resize capacity incorrect");
        assert_cstr_equals(s, "Test", "str_resize content preserved");
        cstr_free(s);

        assert(!cstr_resize(NULL, 10) && "str_resize with NULL should fail");
        printf("str_resize with NULL: Passed\n");
    }

    // Test str_append
    {
        printf("\nTesting str_append...\n");
        cstr* s = cstr_new("Hello");
        assert(cstr_append(s, ", World!") && "str_append failed");
        assert_cstr_equals(s, "Hello, World!", "str_append content");
        cstr_free(s);

        s = cstr_new("");
        assert(cstr_append(s, "") && "str_append empty string");
        assert_cstr_equals(s, "", "str_append empty string content");
        cstr_free(s);

        assert(!cstr_append(NULL, "test") && "str_append with NULL cstr should fail");
        s = cstr_new("Test");
        assert(!cstr_append(s, NULL) && "str_append with NULL string should fail");
        cstr_free(s);
        printf("str_append edge cases: Passed\n");
    }

    // Test str_append_fast
    {
        printf("\nTesting str_append_fast...\n");
        cstr* s = cstr_init(20);
        assert(cstr_append_fast(s, "Hello") && "str_append_fast failed");
        assert_cstr_equals(s, "Hello", "str_append_fast content");
        cstr_free(s);

        assert(!cstr_append_fast(NULL, "test") && "str_append_fast with NULL cstr should fail");
        s = cstr_new("Test");
        assert(!cstr_append_fast(s, NULL) && "str_append_fast with NULL string should fail");
        cstr_free(s);
        printf("str_append_fast edge cases: Passed\n");
    }

    // Test str_append_fmt
    {
        printf("\nTesting str_append_fmt...\n");
        cstr* s = cstr_new("Hello");
        assert(cstr_append_fmt(s, ", %s! %d", "World", 42) && "str_append_fmt failed");
        assert_cstr_equals(s, "Hello, World! 42", "str_append_fmt content");
        cstr_free(s);

        s = cstr_new("");
        assert(cstr_append_fmt(s, "") && "str_append_fmt empty format");
        assert_cstr_equals(s, "", "str_append_fmt empty format content");
        cstr_free(s);
    }

    // Test str_append_char
    {
        printf("\nTesting str_append_char...\n");
        cstr* s = cstr_new("Hello");
        assert(cstr_append_char(s, '!') && "str_append_char failed");
        assert_cstr_equals(s, "Hello!", "str_append_char content");
        cstr_free(s);

        assert(!cstr_append_char(NULL, '!') && "str_append_char with NULL should fail");
        printf("str_append_char edge cases: Passed\n");
    }

    // Test str_prepend
    {
        printf("\nTesting str_prepend...\n");
        cstr* s = cstr_new("World");
        assert(cstr_prepend(s, "Hello, ") && "str_prepend failed");
        assert_cstr_equals(s, "Hello, World", "str_prepend content");
        cstr_free(s);

        s = cstr_new("");
        assert(cstr_prepend(s, "") && "str_prepend empty string");
        assert_cstr_equals(s, "", "str_prepend empty string content");
        cstr_free(s);
    }

    // Test str_prepend_fast
    {
        printf("\nTesting str_prepend_fast...\n");
        cstr* s = cstr_init(20);
        assert(cstr_prepend_fast(s, "Hello") && "str_prepend_fast failed");
        assert_cstr_equals(s, "Hello", "str_prepend_fast content");
        cstr_free(s);
    }

    // Test str_insert
    {
        printf("\nTesting str_insert...\n");
        cstr* s = cstr_new("HelloWorld");
        assert(cstr_insert(s, 5, ", ") && "str_insert failed");
        assert_cstr_equals(s, "Hello, World", "str_insert content");
        cstr_free(s);

        s = cstr_new("Test");
        assert(!cstr_insert(s, 5, "x") && "str_insert beyond length should fail");
        cstr_free(s);
    }

    // Test str_remove
    {
        printf("\nTesting str_remove...\n");
        cstr* s = cstr_new("Hello, World");
        assert(cstr_remove(s, 5, 2) && "str_remove failed");
        assert_cstr_equals(s, "HelloWorld", "str_remove content");
        cstr_free(s);

        s = cstr_new("Test");
        assert(cstr_remove(s, 4, 0) && "str_remove at end with zero count");
        assert_cstr_equals(s, "Test", "str_remove at end content");
        cstr_free(s);
    }

    // Test str_clear
    {
        printf("\nTesting str_clear...\n");
        cstr* s = cstr_new("Hello");
        cstr_clear(s);
        assert_cstr_equals(s, "", "str_clear content");
        cstr_free(s);

        cstr_clear(NULL);
        printf("str_clear with NULL: Passed (no crash expected)\n");
    }

    // Test str_remove_all
    {
        printf("\nTesting str_remove_all...\n");
        cstr* s = cstr_new("hello hello world");
        assert(cstr_remove_all(s, "hello ") == 2 && "str_remove_all count incorrect");
        assert_cstr_equals(s, "world", "str_remove_all content");
        cstr_free(s);

        s = cstr_new("test");
        assert(cstr_remove_all(s, "x") == 0 && "str_remove_all no match");
        assert_cstr_equals(s, "test", "str_remove_all no match content");
        cstr_free(s);
    }

    // Test str_at
    {
        printf("\nTesting str_at...\n");
        cstr* s = cstr_new("Hello");
        assert(cstr_at(s, 1) == 'e' && "str_at incorrect");
        assert(cstr_at(s, 5) == '\0' && "str_at out of bounds");
        cstr_free(s);
        assert(cstr_at(NULL, 0) == '\0' && "str_at with NULL");
        printf("str_at: Passed\n");
    }

    // Test str_data
    {
        printf("\nTesting str_data...\n");
        cstr* s = cstr_new("Hello");
        assert(strcmp(cstr_data(s), "Hello") == 0 && "str_data incorrect");
        cstr_free(s);
        assert(cstr_data(NULL) == NULL && "str_data with NULL");
        printf("str_data: Passed\n");
    }

    // Test str_as_view
    {
        printf("\nTesting str_as_view...\n");
        cstr* s    = cstr_new("Hello");
        str_view v = cstr_as_view(s);
        assert(v.length == 5 && strcmp(v.data, "Hello") == 0 && "str_as_view incorrect");
        cstr_free(s);
        v = cstr_as_view(NULL);
        assert(v.data == NULL && v.length == 0 && "str_as_view with NULL");
        printf("str_as_view: Passed\n");
    }

    // Test str_compare
    {
        printf("\nTesting str_compare...\n");
        cstr* s1 = cstr_new("apple");
        cstr* s2 = cstr_new("banana");
        assert(cstr_compare(s1, s2) < 0 && "str_compare apple < banana");
        assert(cstr_compare(s2, s1) > 0 && "str_compare banana > apple");
        cstr* s3 = cstr_new("apple");
        assert(cstr_compare(s1, s3) == 0 && "str_compare equal strings");
        cstr_free(s1);
        cstr_free(s2);
        cstr_free(s3);
        printf("str_compare: Passed\n");
    }

    // Test str_equals
    {
        printf("\nTesting str_equals...\n");
        cstr* s1 = cstr_new("apple");
        cstr* s2 = cstr_new("apple");
        cstr* s3 = cstr_new("banana");
        assert(cstr_equals(s1, s2) && "str_equals equal strings");
        assert(!cstr_equals(s1, s3) && "str_equals different strings");
        cstr_free(s1);
        cstr_free(s2);
        cstr_free(s3);
        printf("str_equals: Passed\n");
    }

    // Test str_starts_with
    {
        printf("\nTesting str_starts_with...\n");
        cstr* s = cstr_new("Hello, World");
        assert(cstr_starts_with(s, "Hello") && "str_starts_with valid prefix");
        assert(!cstr_starts_with(s, "World") && "str_starts_with invalid prefix");
        assert(cstr_starts_with(s, "") && "str_starts_with empty prefix");
        cstr_free(s);
        printf("str_starts_with: Passed\n");
    }

    // Test str_ends_with
    {
        printf("\nTesting str_ends_with...\n");
        cstr* s = cstr_new("Hello, World");
        assert(cstr_ends_with(s, "World") && "str_ends_with valid suffix");
        assert(!cstr_ends_with(s, "Hello") && "str_ends_with invalid suffix");
        assert(cstr_ends_with(s, "") && "str_ends_with empty suffix");
        cstr_free(s);
        printf("str_ends_with: Passed\n");
    }

    // Test str_find
    {
        printf("\nTesting str_find...\n");
        cstr* s = cstr_new("Hello, World");
        assert(cstr_find(s, "World") == 7 && "str_find valid substring");
        assert(cstr_find(s, "NotFound") == STR_NPOS && "str_find not found");
        cstr_free(s);
        printf("str_find: Passed\n");
    }

    // Test str_rfind
    {
        printf("\nTesting str_rfind...\n");
        cstr* s = cstr_new("hello hello world");
        assert(cstr_rfind(s, "hello") == 6 && "str_rfind last occurrence");
        assert(cstr_rfind(s, "notfound") == STR_NPOS && "str_rfind not found");
        cstr_free(s);
        printf("str_rfind: Passed\n");
    }

    // Test str_to_lower
    {
        printf("\nTesting str_to_lower...\n");
        cstr* s = cstr_new("HELLO");
        cstr_lower(s);
        assert_cstr_equals(s, "hello", "str_to_lower content");
        cstr_free(s);
    }

    // Test str_to_upper
    {
        printf("\nTesting str_to_upper...\n");
        cstr* s = cstr_new("hello");
        cstr_upper(s);
        assert_cstr_equals(s, "HELLO", "str_to_upper content");
        cstr_free(s);
    }

    // Test str_snake_case
    {
        printf("\nTesting str_snake_case...\n");
        cstr* s = cstr_new("HelloWorldMyDearFriend");  // should start on stack.
        assert(s);
        cstr_snakecase(s);  // will migrate to the heap.

        // hello_world_myDea_rFriend
        assert_cstr_equals(s, "hello_world_my_dear_friend", "str_snake_case content");
        cstr_free(s);
    }

    // Test str_camel_case
    {
        printf("\nTesting str_camel_case...\n");
        cstr* s = cstr_new("hello_world");
        cstr_camelcase(s);
        assert_cstr_equals(s, "helloWorld", "str_camel_case content");
        cstr_free(s);
    }

    // Test str_pascal_case
    {
        printf("\nTesting str_pascal_case...\n");
        cstr* s = cstr_new("hello_world");
        cstr_pascalcase(s);
        assert_cstr_equals(s, "HelloWorld", "str_pascal_case content");
        cstr_free(s);
    }

    // Test str_title_case
    {
        printf("\nTesting str_title_case...\n");
        cstr* s = cstr_new("hello world");
        cstr_titlecase(s);
        assert_cstr_equals(s, "Hello World", "str_title_case content");
        cstr_free(s);
    }

    // Test str_trim
    {
        printf("\nTesting str_trim...\n");
        cstr* s = cstr_new("  Hello  ");
        cstr_trim(s);
        assert_cstr_equals(s, "Hello", "str_trim content");
        cstr_free(s);
    }

    // Test str_rtrim
    {
        printf("\nTesting str_rtrim...\n");
        cstr* s = cstr_new("Hello  ");
        cstr_rtrim(s);
        assert_cstr_equals(s, "Hello", "str_rtrim content");
        cstr_free(s);
    }

    // Test str_ltrim
    {
        printf("\nTesting str_ltrim...\n");
        cstr* s = cstr_new("  Hello");
        cstr_ltrim(s);
        assert_cstr_equals(s, "Hello", "str_ltrim content");
        cstr_free(s);
    }

    // Test str_trim_chars
    {
        printf("\nTesting str_trim_chars...\n");
        cstr* s = cstr_new("...Hello...");
        cstr_trim_chars(s, ".");
        assert_cstr_equals(s, "Hello", "str_trim_chars content");
        cstr_free(s);
    }

    // Test str_count_substr
    {
        printf("\nTesting str_count_substr...\n");
        cstr* s = cstr_new("hello hello world");
        assert(cstr_count_substr(s, "hello") == 2 && "str_count_substr count");
        assert(cstr_count_substr(s, "notfound") == 0 && "str_count_substr not found");
        cstr_free(s);
        printf("str_count_substr: Passed\n");
    }

    // Test str_remove_char
    {
        printf("\nTesting str_remove_char...\n");
        cstr* s = cstr_new("hello");
        cstr_remove_char(s, 'l');
        assert_cstr_equals(s, "heo", "str_remove_char content");
        cstr_free(s);
    }

    // Test str_substr
    {
        printf("\nTesting str_substr...\n");
        cstr* s   = cstr_new("Hello, World");
        cstr* sub = cstr_substr(s, 7, 5);
        assert_cstr_equals(sub, "World", "str_substr content");
        cstr_free(sub);
        cstr_free(s);
    }

    // Test str_replace
    {
        printf("\nTesting str_replace...\n");
        cstr* s      = cstr_new("hello hello world");
        cstr* result = cstr_replace(s, "hello", "hi");
        assert_cstr_equals(result, "hi hello world", "str_replace first occurrence");
        cstr_free(s);
        cstr_free(result);

        s      = cstr_new("test");
        result = cstr_replace(s, "notfound", "x");
        assert_cstr_equals(result, "test", "str_replace not found");
        cstr_free(s);
        cstr_free(result);
    }

    // Test str_replace_all
    {
        printf("\nTesting str_replace_all...\n");
        cstr* s      = cstr_new("hello hello world");
        cstr* result = cstr_replace_all(s, "hello", "hi");
        assert_cstr_equals(result, "hi hi world", "str_replace_all content");
        cstr_free(s);
        cstr_free(result);
    }

    // Test str_split
    {
        printf("\n**************Testing str_split***************\n");

        // Basic case
        cstr* s = cstr_new("a,b,c");
        size_t count;
        cstr** arr = cstr_split(s, ",", &count);
        assert(count == 3 && "str_split count incorrect");
        assert_cstr_equals(arr[0], "a", "str_split first element");
        assert_cstr_equals(arr[1], "b", "str_split second element");
        assert_cstr_equals(arr[2], "c", "str_split third element");
        free_cstr_array(arr, count);
        cstr_free(s);

        // Empty string case
        s   = cstr_new("");
        arr = cstr_split(s, ",", &count);
        assert(count == 1 && "empty string should return one empty element");
        assert_cstr_equals(arr[0], "", "empty string element");
        free_cstr_array(arr, count);
        cstr_free(s);

        // No delimiter case
        s   = cstr_new("abc");
        arr = cstr_split(s, ",", &count);
        assert(count == 1 && "no delimiter should return original string");
        assert_cstr_equals(arr[0], "abc", "no delimiter element");
        free_cstr_array(arr, count);
        cstr_free(s);

        // Long prose string case
        const char* long_prose =
            "It was the best of times, it was the worst of times, "
            "it was the age of wisdom, it was the age of foolishness, "
            "it was the epoch of belief, it was the epoch of incredulity, "
            "it was the season of Light, it was the season of Darkness, "
            "it was the spring of hope, it was the winter of despair.";
        s   = cstr_new(long_prose);
        arr = cstr_split(s, ", ", &count);  // Split on comma+space

        // Verify we got the expected number of splits
        const size_t expected_splits = 10;  // 10 pairs separated by ", "
        printf("Splits=%lu\n", count);
        assert(count == expected_splits && "long prose split count incorrect");

        // Verify first and last segments
        assert_cstr_equals(arr[0], "It was the best of times", "long prose first element");
        assert_cstr_equals(arr[expected_splits - 1], "it was the winter of despair.",
                           "long prose last element");

        // Verify a middle segment
        assert_cstr_equals(arr[7], "it was the season of Darkness", "long prose middle element");

        // Performance test (just output time, no assert)
        clock_t start = clock();
        for (int i = 0; i < 1000; i++) {
            free_cstr_array(arr, count);
            arr = cstr_split(s, ", ", &count);
        }
        double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
        printf("Long prose performance: %.3f seconds for 1000 splits\n", elapsed);

        free_cstr_array(arr, count);
        cstr_free(s);

        // Edge case: delimiter at start/end
        s   = cstr_new(",a,b,c,");
        arr = cstr_split(s, ",", &count);
        assert(count == 5 && "edge delimiters count incorrect");
        assert_cstr_equals(arr[0], "", "leading delimiter element");
        assert_cstr_equals(arr[4], "", "trailing delimiter element");
        free_cstr_array(arr, count);
        cstr_free(s);

        printf("All str_split tests passed!\n");
    }

    // Test str_join
    {
        printf("\nTesting str_join...\n");
        cstr* s1     = cstr_new("Hello");
        cstr* s2     = cstr_new("World");
        cstr* arr[]  = {s1, s2};
        cstr* result = cstr_join((const cstr**)arr, 2, ", ");
        assert_cstr_equals(result, "Hello, World", "str_join content");
        cstr_free(s1);
        cstr_free(s2);
        cstr_free(result);
    }

    // Test str_reverse
    {
        printf("\nTesting str_reverse...\n");
        cstr* s      = cstr_new("Hello");
        cstr* result = cstr_reverse(s);
        assert_cstr_equals(result, "olleH", "str_reverse content");
        cstr_free(s);
        cstr_free(result);
    }

    // Test str_reverse_in_place
    {
        printf("\nTesting str_reverse_in_place...\n");
        cstr* s = cstr_new("Hello");
        cstr_reverse_inplace(s);
        assert_cstr_equals(s, "olleH", "str_reverse_in_place content");
        cstr_free(s);
    }

    // =========== More comprehensive tests

    {
        printf("Testing str_len / str_capacity...\n");
        cstr* s = cstr_new("Hello");
        assert(cstr_len(s) == 5);
        assert(cstr_capacity(s) >= 6);
        cstr_free(s);
        assert(cstr_len(NULL) == 0);
        assert(cstr_capacity(NULL) == 0);
    }

    {
        printf("Testing str_empty...\n");
        cstr* s = cstr_new("");
        assert(cstr_empty(s));
        cstr_free(s);
        s = cstr_new("x");
        assert(!cstr_empty(s));
        cstr_free(s);
        assert(cstr_empty(NULL));
    }

    {
        printf("Testing str_resize...\n");
        cstr* s = cstr_new("Resize");
        assert(cstr_resize(s, 20));
        assert(cstr_capacity(s) >= 20);
        assert_cstr_equals(s, "Resize", "str_resize preserves content");
        cstr_free(s);
        assert(!cstr_resize(NULL, 10));
    }

    {
        printf("Testing str_append...\n");
        cstr* s = cstr_new("Hi");
        assert(cstr_append(s, " there"));
        assert_cstr_equals(s, "Hi there", "str_append");
        cstr_free(s);
        assert(!cstr_append(NULL, "x"));
        s = cstr_new("Another");
        assert(!cstr_append(s, NULL));
        cstr_free(s);
    }

    {
        printf("Testing str_append_fmt...\n");
        cstr* s = cstr_new("Hi");
        assert(cstr_append_fmt(s, ", %s!", "friend"));
        assert_cstr_equals(s, "Hi, friend!", "str_append_fmt");
        cstr_free(s);
    }

    {
        printf("Testing str_append_char...\n");
        cstr* s = cstr_new("End");
        assert(cstr_append_char(s, '!'));
        assert_cstr_equals(s, "End!", "str_append_char");
        cstr_free(s);
        assert(!cstr_append_char(NULL, '!'));
    }

    {
        printf("Testing str_prepend...\n");
        cstr* s = cstr_new("tail");
        assert(cstr_prepend(s, "head "));
        assert_cstr_equals(s, "head tail", "str_prepend");
        cstr_free(s);
    }

    {
        printf("Testing str_insert...\n");
        cstr* s = cstr_new("Helo");
        assert(cstr_insert(s, 2, "l"));
        assert_cstr_equals(s, "Hello", "str_insert");
        cstr_free(s);
    }

    {
        printf("Testing str_remove...\n");
        cstr* s = cstr_new("Helloo!");
        assert(cstr_remove(s, 5, 1));
        assert_cstr_equals(s, "Hello!", "str_remove");
        cstr_free(s);
    }

    {
        printf("Testing str_clear...\n");
        cstr* s = cstr_new("NotEmpty");
        cstr_clear(s);
        assert_cstr_equals(s, "", "str_clear");
        cstr_free(s);
        cstr_clear(NULL);  // should not crash
    }

    {
        printf("Testing str_remove_all...\n");
        cstr* s        = cstr_new("foo bar foo bar foo");
        size_t removed = cstr_remove_all(s, "foo ");
        assert(removed == 2);
        assert_cstr_equals(s, "bar bar foo", "str_remove_all");
        cstr_free(s);
    }

    {
        printf("Testing str_at...\n");
        cstr* s = cstr_new("Hey");
        assert(cstr_at(s, 0) == 'H');
        assert(cstr_at(s, 3) == '\0');
        assert(cstr_at(NULL, 0) == '\0');
        cstr_free(s);
    }

    {
        printf("Testing str_data...\n");
        cstr* s = cstr_new("Raw");
        assert(strcmp(cstr_data(s), "Raw") == 0);
        cstr_free(s);
        assert(cstr_data(NULL) == NULL);
    }

    {
        printf("Testing str_as_view...\n");
        cstr* s    = cstr_new("Slice");
        str_view v = cstr_as_view(s);
        assert(v.data && strcmp(v.data, "Slice") == 0 && v.length == 5);
        cstr_free(s);
        v = cstr_as_view(NULL);
        assert(v.data == NULL && v.length == 0);
    }

    // Fuzz with large inputs
    {
        // This will reallocate multiple times.
        // but stack string will be promoted to heap.
        for (int i = 0; i < 1000; ++i) {
            cstr* s = cstr_init(0);
            for (int j = 0; j < 100; ++j) {
                assert(cstr_append_char(s, 'a' + (rand() % 26)));
            }
            cstr_free(s);
        }
    }

    // Fuzz after resizing
    {
        // This will reallocate multiple times.
        // but stack string will be promoted to heap.
        for (int i = 0; i < 1000; ++i) {
            cstr* s = cstr_init(0);
            assert(cstr_resize(s, 100));

            for (int j = 0; j < 100; ++j)
                assert(cstr_append_char(s, 'a' + (rand() % 26)));
            cstr_free(s);
        }
    }

    printf("\nAll tests passed successfully!\n");
    return 0;
}
