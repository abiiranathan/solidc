#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../include/cstr.h"

void more_string_tests();

// Helper function to print cstr content for debugging
void print_cstr(const cstr* s) {
    if (!s) {
        printf("NULL\n");
    } else {
        printf(
            "\"%s\" (len=%zu, cap=%zu, heap=%d)\n", str_data_const(s), str_len(s), str_capacity(s), str_allocated(s));
    }
}

// Helper function to compare cstr with expected C-string
void assert_cstr_equals(const cstr* s, const char* expected, const char* test_name) {
    if (!s && !expected) {
        return;
    }
    assert(s && expected && "cstr or expected is NULL");
    assert(strcmp(str_data_const(s), expected) == 0 && "Content mismatch");
    assert(str_len(s) == strlen(expected) && "Length mismatch");
    printf("%s: Passed\n", test_name);
}

// Helper function to free an array of cstrs
void free_cstr_array(cstr** arr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        str_free(arr[i]);
    }
    free(arr);
}

int main(void) {
    printf("Starting cstr library tests...\n\n");

    // Test str_new
    {
        printf("Testing str_new...\n");
        cstr* s = str_new(0);
        assert_cstr_equals(s, "", "str_new with zero capacity");
        assert(str_capacity(s) >= 1);
        str_free(s);

        s = str_new(20);
        assert_cstr_equals(s, "", "str_new with large capacity");
        assert(str_capacity(s) >= 20);
        str_free(s);
    }

    // Test str_from
    {
        printf("\nTesting str_from...\n");
        cstr* s = str_from("Hello");
        assert_cstr_equals(s, "Hello", "str_from with valid string");
        str_free(s);

        s = str_from("");
        assert_cstr_equals(s, "", "str_from with empty string");
        str_free(s);

        s = str_from(NULL);
        assert(s == NULL && "str_from with NULL should return NULL");
        printf("str_from with NULL: Passed\n");
    }

    // Test str_format
    {
        printf("\nTesting str_format...\n");
        cstr* s = str_format("Hello, %s! %d", "World", 42);
        assert_cstr_equals(s, "Hello, World! 42", "str_format with valid format");
        str_free(s);

        s = str_format("");
        assert_cstr_equals(s, "", "str_format with empty format");
        str_free(s);
    }

    // Test str_free
    {
        printf("\nTesting str_free...\n");
        cstr* s = str_from("Test");
        str_free(s);
        printf("str_free: Passed (no crash expected)\n");

        str_free(NULL);
        printf("str_free with NULL: Passed (no crash expected)\n");
    }

    // Test str_len and str_capacity
    {
        printf("\nTesting str_len and str_capacity...\n");
        cstr* s = str_from("Hello");
        assert(str_len(s) == 5 && "str_len incorrect");
        assert(str_capacity(s) >= 6 && "str_capacity incorrect");
        printf("str_len and str_capacity: Passed\n");
        str_free(s);

        assert(str_len(NULL) == 0 && "str_len with NULL incorrect");
        assert(str_capacity(NULL) == 0 && "str_capacity with NULL incorrect");
        printf("str_len and str_capacity with NULL: Passed\n");
    }

    // Test str_empty
    {
        printf("\nTesting str_empty...\n");
        cstr* s = str_from("");
        assert(str_empty(s) && "str_empty with empty string");
        str_free(s);

        s = str_from("NonEmpty");
        assert(!str_empty(s) && "str_empty with non-empty string");
        str_free(s);

        assert(str_empty(NULL) && "str_empty with NULL");
        printf("str_empty: Passed\n");
    }

    // Test str_resize
    {
        printf("\nTesting str_resize...\n");
        cstr* s = str_from("Test");
        assert(str_resize(&s, 10) && "str_resize failed");
        assert(str_capacity(s) >= 10 && "str_resize capacity incorrect");
        assert_cstr_equals(s, "Test", "str_resize content preserved");
        str_free(s);

        assert(!str_resize(NULL, 10) && "str_resize with NULL should fail");
        printf("str_resize with NULL: Passed\n");
    }

    // Test str_append
    {
        printf("\nTesting str_append...\n");
        cstr* s = str_from("Hello");
        assert(str_append(&s, ", World!") && "str_append failed");
        assert_cstr_equals(s, "Hello, World!", "str_append content");
        str_free(s);

        s = str_from("");
        assert(str_append(&s, "") && "str_append empty string");
        assert_cstr_equals(s, "", "str_append empty string content");
        str_free(s);

        assert(!str_append(NULL, "test") && "str_append with NULL cstr should fail");
        s = str_from("Test");
        assert(!str_append(&s, NULL) && "str_append with NULL string should fail");
        str_free(s);
        printf("str_append edge cases: Passed\n");
    }

    // Test str_append_fast
    {
        printf("\nTesting str_append_fast...\n");
        cstr* s = str_new(20);
        assert(str_append_fast(&s, "Hello") && "str_append_fast failed");
        assert_cstr_equals(s, "Hello", "str_append_fast content");
        str_free(s);

        assert(!str_append_fast(NULL, "test") && "str_append_fast with NULL cstr should fail");
        s = str_from("Test");
        assert(!str_append_fast(&s, NULL) && "str_append_fast with NULL string should fail");
        str_free(s);
        printf("str_append_fast edge cases: Passed\n");
    }

    // Test str_append_fmt
    {
        printf("\nTesting str_append_fmt...\n");
        cstr* s = str_from("Hello");
        assert(str_append_fmt(&s, ", %s! %d", "World", 42) && "str_append_fmt failed");
        assert_cstr_equals(s, "Hello, World! 42", "str_append_fmt content");
        str_free(s);

        s = str_from("");
        assert(str_append_fmt(&s, "") && "str_append_fmt empty format");
        assert_cstr_equals(s, "", "str_append_fmt empty format content");
        str_free(s);
    }

    // Test str_append_char
    {
        printf("\nTesting str_append_char...\n");
        cstr* s = str_from("Hello");
        assert(str_append_char(&s, '!') && "str_append_char failed");
        assert_cstr_equals(s, "Hello!", "str_append_char content");
        str_free(s);

        assert(!str_append_char(NULL, '!') && "str_append_char with NULL should fail");
        printf("str_append_char edge cases: Passed\n");
    }

    // Test str_prepend
    {
        printf("\nTesting str_prepend...\n");
        cstr* s = str_from("World");
        assert(str_prepend(&s, "Hello, ") && "str_prepend failed");
        assert_cstr_equals(s, "Hello, World", "str_prepend content");
        str_free(s);

        s = str_from("");
        assert(str_prepend(&s, "") && "str_prepend empty string");
        assert_cstr_equals(s, "", "str_prepend empty string content");
        str_free(s);
    }

    // Test str_prepend_fast
    {
        printf("\nTesting str_prepend_fast...\n");
        cstr* s = str_new(20);
        assert(str_prepend_fast(&s, "Hello") && "str_prepend_fast failed");
        assert_cstr_equals(s, "Hello", "str_prepend_fast content");
        str_free(s);
    }

    // Test str_insert
    {
        printf("\nTesting str_insert...\n");
        cstr* s = str_from("HelloWorld");
        assert(str_insert(&s, 5, ", ") && "str_insert failed");
        assert_cstr_equals(s, "Hello, World", "str_insert content");
        str_free(s);

        s = str_from("Test");
        assert(!str_insert(&s, 5, "x") && "str_insert beyond length should fail");
        str_free(s);
    }

    // Test str_remove
    {
        printf("\nTesting str_remove...\n");
        cstr* s = str_from("Hello, World");
        assert(str_remove(&s, 5, 2) && "str_remove failed");
        assert_cstr_equals(s, "HelloWorld", "str_remove content");
        str_free(s);

        s = str_from("Test");
        assert(str_remove(&s, 4, 0) && "str_remove at end with zero count");
        assert_cstr_equals(s, "Test", "str_remove at end content");
        str_free(s);
    }

    // Test str_clear
    {
        printf("\nTesting str_clear...\n");
        cstr* s = str_from("Hello");
        str_clear(s);
        assert_cstr_equals(s, "", "str_clear content");
        str_free(s);

        str_clear(NULL);
        printf("str_clear with NULL: Passed (no crash expected)\n");
    }

    // Test str_remove_all
    {
        printf("\nTesting str_remove_all...\n");
        cstr* s = str_from("hello hello world");
        assert(str_remove_all(&s, "hello ") == 2 && "str_remove_all count incorrect");
        assert_cstr_equals(s, "world", "str_remove_all content");
        str_free(s);

        s = str_from("test");
        assert(str_remove_all(&s, "x") == 0 && "str_remove_all no match");
        assert_cstr_equals(s, "test", "str_remove_all no match content");
        str_free(s);
    }

    // Test str_at
    {
        printf("\nTesting str_at...\n");
        cstr* s = str_from("Hello");
        assert(str_at(s, 1) == 'e' && "str_at incorrect");
        assert(str_at(s, 5) == '\0' && "str_at out of bounds");
        str_free(s);
        assert(str_at(NULL, 0) == '\0' && "str_at with NULL");
        printf("str_at: Passed\n");
    }

    // Test str_data
    {
        printf("\nTesting str_data...\n");
        cstr* s = str_from("Hello");
        assert(strcmp(str_data(s), "Hello") == 0 && "str_data incorrect");
        str_free(s);
        assert(str_data(NULL) == NULL && "str_data with NULL");
        printf("str_data: Passed\n");
    }

    // Test str_as_view
    {
        printf("\nTesting str_as_view...\n");
        cstr* s    = str_from("Hello");
        str_view v = str_as_view(s);
        assert(v.length == 5 && strcmp(v.data, "Hello") == 0 && "str_as_view incorrect");
        str_free(s);
        v = str_as_view(NULL);
        assert(v.data == NULL && v.length == 0 && "str_as_view with NULL");
        printf("str_as_view: Passed\n");
    }

    // Test str_compare
    {
        printf("\nTesting str_compare...\n");
        cstr* s1 = str_from("apple");
        cstr* s2 = str_from("banana");
        assert(str_compare(s1, s2) < 0 && "str_compare apple < banana");
        assert(str_compare(s2, s1) > 0 && "str_compare banana > apple");
        cstr* s3 = str_from("apple");
        assert(str_compare(s1, s3) == 0 && "str_compare equal strings");
        str_free(s1);
        str_free(s2);
        str_free(s3);
        printf("str_compare: Passed\n");
    }

    // Test str_equals
    {
        printf("\nTesting str_equals...\n");
        cstr* s1 = str_from("apple");
        cstr* s2 = str_from("apple");
        cstr* s3 = str_from("banana");
        assert(str_equals(s1, s2) && "str_equals equal strings");
        assert(!str_equals(s1, s3) && "str_equals different strings");
        str_free(s1);
        str_free(s2);
        str_free(s3);
        printf("str_equals: Passed\n");
    }

    // Test str_starts_with
    {
        printf("\nTesting str_starts_with...\n");
        cstr* s = str_from("Hello, World");
        assert(str_starts_with(s, "Hello") && "str_starts_with valid prefix");
        assert(!str_starts_with(s, "World") && "str_starts_with invalid prefix");
        assert(str_starts_with(s, "") && "str_starts_with empty prefix");
        str_free(s);
        printf("str_starts_with: Passed\n");
    }

    // Test str_ends_with
    {
        printf("\nTesting str_ends_with...\n");
        cstr* s = str_from("Hello, World");
        assert(str_ends_with(s, "World") && "str_ends_with valid suffix");
        assert(!str_ends_with(s, "Hello") && "str_ends_with invalid suffix");
        assert(str_ends_with(s, "") && "str_ends_with empty suffix");
        str_free(s);
        printf("str_ends_with: Passed\n");
    }

    // Test str_find
    {
        printf("\nTesting str_find...\n");
        cstr* s = str_from("Hello, World");
        assert(str_find(s, "World") == 7 && "str_find valid substring");
        assert(str_find(s, "NotFound") == STR_NPOS && "str_find not found");
        str_free(s);
        printf("str_find: Passed\n");
    }

    // Test str_rfind
    {
        printf("\nTesting str_rfind...\n");
        cstr* s = str_from("hello hello world");
        assert(str_rfind(s, "hello") == 6 && "str_rfind last occurrence");
        assert(str_rfind(s, "notfound") == STR_NPOS && "str_rfind not found");
        str_free(s);
        printf("str_rfind: Passed\n");
    }

    // Test str_to_lower
    {
        printf("\nTesting str_to_lower...\n");
        cstr* s = str_from("HELLO");
        str_to_lower(s);
        assert_cstr_equals(s, "hello", "str_to_lower content");
        str_free(s);
    }

    // Test str_to_upper
    {
        printf("\nTesting str_to_upper...\n");
        cstr* s = str_from("hello");
        str_to_upper(s);
        assert_cstr_equals(s, "HELLO", "str_to_upper content");
        str_free(s);
    }

    // Test str_snake_case
    {
        printf("\nTesting str_snake_case...\n");
        cstr* s = str_from("HelloWorld");
        str_snake_case(s);
        assert_cstr_equals(s, "hello_world", "str_snake_case content");
        str_free(s);
    }

    // Test str_camel_case
    {
        printf("\nTesting str_camel_case...\n");
        cstr* s = str_from("hello_world");
        str_camel_case(s);
        assert_cstr_equals(s, "helloWorld", "str_camel_case content");
        str_free(s);
    }

    // Test str_pascal_case
    {
        printf("\nTesting str_pascal_case...\n");
        cstr* s = str_from("hello_world");
        str_pascal_case(s);
        assert_cstr_equals(s, "HelloWorld", "str_pascal_case content");
        str_free(s);
    }

    // Test str_title_case
    {
        printf("\nTesting str_title_case...\n");
        cstr* s = str_from("hello world");
        str_title_case(s);
        assert_cstr_equals(s, "Hello World", "str_title_case content");
        str_free(s);
    }

    // Test str_trim
    {
        printf("\nTesting str_trim...\n");
        cstr* s = str_from("  Hello  ");
        str_trim(s);
        assert_cstr_equals(s, "Hello", "str_trim content");
        str_free(s);
    }

    // Test str_rtrim
    {
        printf("\nTesting str_rtrim...\n");
        cstr* s = str_from("Hello  ");
        str_rtrim(s);
        assert_cstr_equals(s, "Hello", "str_rtrim content");
        str_free(s);
    }

    // Test str_ltrim
    {
        printf("\nTesting str_ltrim...\n");
        cstr* s = str_from("  Hello");
        str_ltrim(s);
        assert_cstr_equals(s, "Hello", "str_ltrim content");
        str_free(s);
    }

    // Test str_trim_chars
    {
        printf("\nTesting str_trim_chars...\n");
        cstr* s = str_from("...Hello...");
        str_trim_chars(s, ".");
        assert_cstr_equals(s, "Hello", "str_trim_chars content");
        str_free(s);
    }

    // Test str_count_substr
    {
        printf("\nTesting str_count_substr...\n");
        cstr* s = str_from("hello hello world");
        assert(str_count_substr(s, "hello") == 2 && "str_count_substr count");
        assert(str_count_substr(s, "notfound") == 0 && "str_count_substr not found");
        str_free(s);
        printf("str_count_substr: Passed\n");
    }

    // Test str_remove_char
    {
        printf("\nTesting str_remove_char...\n");
        cstr* s = str_from("hello");
        str_remove_char(s, 'l');
        assert_cstr_equals(s, "heo", "str_remove_char content");
        str_free(s);
    }

    // Test str_remove_range
    {
        printf("\nTesting str_remove_range...\n");
        cstr* s = str_from("Hello, World");
        str_remove_range(s, 5, 2);
        assert_cstr_equals(s, "HelloWorld", "str_remove_range content");
        str_free(s);
    }

    // Test str_substr
    {
        printf("\nTesting str_substr...\n");
        cstr* s   = str_from("Hello, World");
        cstr* sub = str_substr(s, 7, 5);
        assert_cstr_equals(sub, "World", "str_substr content");
        str_free(sub);
        str_free(s);
    }

    // Test str_replace
    {
        printf("\nTesting str_replace...\n");
        cstr* s      = str_from("hello hello world");
        cstr* result = str_replace(s, "hello", "hi");
        assert_cstr_equals(result, "hi hello world", "str_replace first occurrence");
        str_free(s);
        str_free(result);

        s      = str_from("test");
        result = str_replace(s, "notfound", "x");
        assert_cstr_equals(result, "test", "str_replace not found");
        str_free(s);
        str_free(result);
    }

    // Test str_replace_all
    {
        printf("\nTesting str_replace_all...\n");
        cstr* s      = str_from("hello hello world");
        cstr* result = str_replace_all(s, "hello", "hi");
        assert_cstr_equals(result, "hi hi world", "str_replace_all content");
        str_free(s);
        str_free(result);
    }

    // Test str_split
    {
        printf("\nTesting str_split...\n");
        cstr* s = str_from("a,b,c");
        size_t count;
        cstr** arr = str_split(s, ",", &count);
        assert(count == 3 && "str_split count incorrect");
        assert_cstr_equals(arr[0], "a", "str_split first element");
        assert_cstr_equals(arr[1], "b", "str_split second element");
        assert_cstr_equals(arr[2], "c", "str_split third element");
        free_cstr_array(arr, count);
        str_free(s);
    }

    // Test str_join
    {
        printf("\nTesting str_join...\n");
        cstr* s1     = str_from("Hello");
        cstr* s2     = str_from("World");
        cstr* arr[]  = {s1, s2};
        cstr* result = str_join((const cstr**)arr, 2, ", ");
        assert_cstr_equals(result, "Hello, World", "str_join content");
        str_free(s1);
        str_free(s2);
        str_free(result);
    }

    // Test str_reverse
    {
        printf("\nTesting str_reverse...\n");
        cstr* s      = str_from("Hello");
        cstr* result = str_reverse(s);
        assert_cstr_equals(result, "olleH", "str_reverse content");
        str_free(s);
        str_free(result);
    }

    // Test str_reverse_in_place
    {
        printf("\nTesting str_reverse_in_place...\n");
        cstr* s = str_from("Hello");
        str_reverse_in_place(s);
        assert_cstr_equals(s, "olleH", "str_reverse_in_place content");
        str_free(s);
    }

    // =========== More comprehensive tests

    more_string_tests();

    printf("\nAll tests passed successfully!\n");
    return 0;
}

void more_string_tests(void) {
    {
        printf("Testing str_len / str_capacity...\n");
        cstr* s = str_from("Hello");
        assert(str_len(s) == 5);
        assert(str_capacity(s) >= 6);
        str_free(s);
        assert(str_len(NULL) == 0);
        assert(str_capacity(NULL) == 0);
    }

    {
        printf("Testing str_empty...\n");
        cstr* s = str_from("");
        assert(str_empty(s));
        str_free(s);
        s = str_from("x");
        assert(!str_empty(s));
        str_free(s);
        assert(str_empty(NULL));
    }

    {
        printf("Testing str_resize...\n");
        cstr* s = str_from("Resize");
        assert(str_resize(&s, 20));
        assert(str_capacity(s) >= 20);
        assert_cstr_equals(s, "Resize", "str_resize preserves content");
        str_free(s);
        assert(!str_resize(NULL, 10));
    }

    {
        printf("Testing str_append...\n");
        cstr* s = str_from("Hi");
        assert(str_append(&s, " there"));
        assert_cstr_equals(s, "Hi there", "str_append");
        str_free(s);
        assert(!str_append(NULL, "x"));
        s = str_from("Another");
        assert(!str_append(&s, NULL));
        str_free(s);
    }

    {
        printf("Testing str_append_fmt...\n");
        cstr* s = str_from("Hi");
        assert(str_append_fmt(&s, ", %s!", "friend"));
        assert_cstr_equals(s, "Hi, friend!", "str_append_fmt");
        str_free(s);
    }

    {
        printf("Testing str_append_char...\n");
        cstr* s = str_from("End");
        assert(str_append_char(&s, '!'));
        assert_cstr_equals(s, "End!", "str_append_char");
        str_free(s);
        assert(!str_append_char(NULL, '!'));
    }

    {
        printf("Testing str_prepend...\n");
        cstr* s = str_from("tail");
        assert(str_prepend(&s, "head "));
        assert_cstr_equals(s, "head tail", "str_prepend");
        str_free(s);
    }

    {
        printf("Testing str_insert...\n");
        cstr* s = str_from("Helo");
        assert(str_insert(&s, 2, "l"));
        assert_cstr_equals(s, "Hello", "str_insert");
        str_free(s);
    }

    {
        printf("Testing str_remove...\n");
        cstr* s = str_from("Helloo!");
        assert(str_remove(&s, 5, 1));
        assert_cstr_equals(s, "Hello!", "str_remove");
        str_free(s);
    }

    {
        printf("Testing str_clear...\n");
        cstr* s = str_from("NotEmpty");
        str_clear(s);
        assert_cstr_equals(s, "", "str_clear");
        str_free(s);
        str_clear(NULL);  // should not crash
    }

    {
        printf("Testing str_remove_all...\n");
        cstr* s        = str_from("foo bar foo bar foo");
        size_t removed = str_remove_all(&s, "foo ");
        assert(removed == 2);
        assert_cstr_equals(s, "bar bar foo", "str_remove_all");
        str_free(s);
    }

    {
        printf("Testing str_at...\n");
        cstr* s = str_from("Hey");
        assert(str_at(s, 0) == 'H');
        assert(str_at(s, 3) == '\0');
        assert(str_at(NULL, 0) == '\0');
        str_free(s);
    }

    {
        printf("Testing str_data...\n");
        cstr* s = str_from("Raw");
        assert(strcmp(str_data(s), "Raw") == 0);
        str_free(s);
        assert(str_data(NULL) == NULL);
    }

    {
        printf("Testing str_as_view...\n");
        cstr* s    = str_from("Slice");
        str_view v = str_as_view(s);
        assert(v.data && strcmp(v.data, "Slice") == 0 && v.length == 5);
        str_free(s);
        v = str_as_view(NULL);
        assert(v.data == NULL && v.length == 0);
    }

    // Fuzz with large inputs
    {
        // This will reallocate multiple times.
        // but stack string will be promoted to heap.
        for (int i = 0; i < 1000; ++i) {
            cstr* s = str_new(0);
            for (int j = 0; j < 100; ++j)
                str_append_char(&s, 'a' + (rand() % 26));
            str_free(s);
        }
    }

    // Fuzz after resizing
    {
        // This will reallocate multiple times.
        // but stack string will be promoted to heap.
        for (int i = 0; i < 1000; ++i) {
            cstr* s = str_new(0);
            str_resize(&s, 100);

            for (int j = 0; j < 100; ++j)
                str_append_char(&s, 'a' + (rand() % 26));
            str_free(s);
        }
    }

    printf("--- more_string_tests completed ---\n");
}
