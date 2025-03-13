#include "../include/str.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT(cond, fmt, ...)                                                                                         \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "Assertion failed: %s:%d: %s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);    \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

void test_create_and_basic_ops() {
    str* s = str_new(0);
    ASSERT(s, "str_new failed");
    ASSERT(str_len(s) == 0, "str_len failed");
    ASSERT(str_capacity(s) == STR_MIN_CAPACITY, "str_capacity failed");
    ASSERT(str_empty(s), "str_empty failed");

    ASSERT(str_append(&s, "Hello"), "str_append failed");
    ASSERT(str_len(s) == 5, "str_len failed");
    ASSERT(!str_empty(s), "str_empty failed");

    ASSERT(str_append_char(&s, ' '), "str_append_char failed");
    ASSERT(strcmp(str_cstr(s), "Hello ") == 0, "str_append_char failed");

    ASSERT(str_append(&s, "World!"), "str_append failed");
    ASSERT(strcmp(str_cstr(s), "Hello World!") == 0, "str_append failed");

    str_free(s);
    printf("test_create_and_basic_ops passed\n");
}

void test_manipulations() {
    str* s = str_from("Hello World!");

    ASSERT(str_prepend(&s, "Hey "), "str_prepend failed");
    ASSERT(strcmp(str_cstr(s), "Hey Hello World!") == 0, "str_prepend failed");

    ASSERT(str_insert(&s, 4, "there "), "str_insert failed");
    ASSERT(strcmp(str_cstr(s), "Hey there Hello World!") == 0, "str_insert failed");

    ASSERT(str_remove(&s, 4, 6), "str_remove failed");
    ASSERT(strcmp(str_cstr(s), "Hey Hello World!") == 0, "str_remove failed");

    ASSERT(str_resize(&s, 10), "str_resize failed");
    ASSERT(str_len(s) == 10, "str_len after resize failed");
    ASSERT(strcmp(str_cstr(s), "Hey Hello ") == 0, "str_resize content check failed");

    str_free(s);
    printf("test_manipulations passed\n");
}

void test_comparisons() {
    str* s1 = str_from("Hello");
    str* s2 = str_from("Hello");
    str* s3 = str_from("World");

    ASSERT(str_compare(s1, s2) == 0, "str_compare failed for equal strings");
    ASSERT(str_compare(s1, s3) < 0, "str_compare failed for different strings");
    ASSERT(str_equals(s1, s2), "str_equals failed for equal strings");
    ASSERT(!str_equals(s1, s3), "str_equals failed for different strings");

    ASSERT(str_starts_with(s1, "He"), "str_starts_with failed");
    ASSERT(!str_starts_with(s1, "Wo"), "str_starts_with failed for non-match");
    ASSERT(str_ends_with(s1, "lo"), "str_ends_with failed");
    ASSERT(!str_ends_with(s1, "ld"), "str_ends_with failed for non-match");

    str_free(s1);
    str_free(s2);
    str_free(s3);
    printf("test_comparisons passed\n");
}

void test_search() {
    str* s = str_from("Hello World! Hello Universe!");

    ASSERT(str_find(s, "World") == 6, "str_find failed");
    ASSERT(str_find(s, "Goodbye") == STR_NPOS, "str_find failed for non-existent substring");
    ASSERT(str_rfind(s, "Hello") == 13, "str_rfind failed");
    ASSERT(str_rfind(s, "Goodbye") == STR_NPOS, "str_rfind failed for non-existent substring");

    str_free(s);
    printf("test_search passed\n");
}

void test_trim() {
    str* s = str_from("  Hello World!  ");

    str_trim(s);
    ASSERT(strcmp(str_cstr(s), "Hello World!") == 0, "str_trim failed");

    str* s2 = str_from("  Hello World!  ");
    str_ltrim(s2);
    ASSERT(strcmp(str_cstr(s2), "Hello World!  ") == 0, "str_ltrim failed");

    str* s3 = str_from("  Hello World!  ");
    str_rtrim(s3);
    ASSERT(strcmp(str_cstr(s3), "  Hello World!") == 0, "str_rtrim failed");

    str_free(s);
    str_free(s2);
    str_free(s3);
    printf("test_trim passed\n");
}

void test_case_conversions() {
    str* s = str_from("hello_world");

    str_snake_case(s);
    ASSERT(strcmp(str_cstr(s), "hello_world") == 0, "str_snake_case failed");

    str_camel_case(s);
    ASSERT(strcmp(str_cstr(s), "helloWorld") == 0, "str_camel_case failed");

    str_pascal_case(s);
    ASSERT(strcmp(str_cstr(s), "HelloWorld") == 0, "str_pascal_case failed");

    // Additional tests
    str* s2 = str_from("THE QUICK BROWN FOX");
    str_to_lower(s2);
    ASSERT(strcmp(str_cstr(s2), "the quick brown fox") == 0, "str_lower failed");

    str_to_upper(s2);
    ASSERT(strcmp(str_cstr(s2), "THE QUICK BROWN FOX") == 0, "str_upper failed");

    str_free(s);
    str_free(s2);
    printf("test_case_conversions passed\n");
}

void test_substring_and_replace() {
    str* s = str_from("Hello, World!");

    str* sub = str_substr(s, 7, 5);
    ASSERT(strcmp(str_cstr(sub), "World") == 0, "str_substr failed");
    str_free(sub);

    str* replaced = str_replace(s, "World", "Universe");
    ASSERT(strcmp(str_cstr(replaced), "Hello, Universe!") == 0, "str_replace failed");
    str_free(replaced);

    str* replaced_all = str_replace_all(s, "l", "L");
    ASSERT(strcmp(str_cstr(replaced_all), "HeLLo, WorLd!") == 0, "str_replace_all failed");
    str_free(replaced_all);

    str_free(s);
    printf("test_substring_and_replace passed\n");
}

void test_split_and_join() {
    str* s = str_from("Hello,World,Universe");
    size_t count;
    str** tokens = str_split(s, ",", &count);

    ASSERT(count == 3, "str_split failed");
    ASSERT(strcmp(str_cstr(tokens[0]), "Hello") == 0, "str_split failed");
    ASSERT(strcmp(str_cstr(tokens[1]), "World") == 0, "str_split failed");
    ASSERT(strcmp(str_cstr(tokens[2]), "Universe") == 0, "str_split failed");

    str* joined = str_join((const str**)tokens, count, ",");
    ASSERT(strcmp(str_cstr(joined), "Hello,World,Universe") == 0, "str_join failed");

    for (size_t i = 0; i < count; ++i) {
        str_free(tokens[i]);
    }
    free(tokens);
    str_free(joined);
    str_free(s);

    // Additional test with different delimiter
    s      = str_from("apple|banana|cherry|date");
    tokens = str_split(s, "|", &count);
    ASSERT(count == 4, "str_split failed with | delimiter");
    joined = str_join((const str**)tokens, count, " - ");
    ASSERT(strcmp(str_cstr(joined), "apple - banana - cherry - date") == 0, "str_join failed with different delimiter");

    for (size_t i = 0; i < count; ++i) {
        str_free(tokens[i]);
    }
    free(tokens);
    str_free(joined);
    str_free(s);

    printf("test_split_and_join passed\n");
}

void test_reverse() {
    str* s = str_from("Hello, World!");

    str* reversed = str_reverse(s);
    ASSERT(strcmp(str_cstr(reversed), "!dlroW ,olleH") == 0, "str_reverse failed");
    str_free(reversed);

    str_reverse_in_place(s);
    ASSERT(strcmp(str_cstr(s), "!dlroW ,olleH") == 0, "str_reverse_in_place failed");

    // Additional test with palindrome
    str* palindrome = str_from("A man a plan a canal Panama");
    str_remove_all(&palindrome, " ");
    str_to_lower(palindrome);
    str* palindrome_reversed = str_reverse(palindrome);
    ASSERT(str_equals(palindrome, palindrome_reversed), "Palindrome reverse test failed");

    str_free(s);
    str_free(palindrome);
    str_free(palindrome_reversed);
    printf("test_reverse passed\n");
}

void test_format() {
    str* s = str_format("Hello, %s!", "World");
    ASSERT(strcmp(str_cstr(s), "Hello, World!") == 0, "str_format failed");
    str_free(s);

    s = str_format("The answer is %d.", 42);
    ASSERT(strcmp(str_cstr(s), "The answer is 42.") == 0, "str_format failed");
    str_free(s);

    // more complex format
    s = str_format("The answer is %d, and the value is %f, and the string is %s.", 42, 3.14, "Hello");
    ASSERT(strcmp(str_cstr(s), "The answer is 42, and the value is 3.140000, and the string is Hello.") == 0,
           "str_format failed");

    str_free(s);

    s = str_from("The quick brown fox ");
    ASSERT(s != NULL, "str_from failed");
    bool ok = str_append_fmt(&s, "%s.", "jumps over the lazy dog");
    ASSERT(ok, "str_append_fmt failed");
    ASSERT(strcmp(str_cstr(s), "The quick brown fox jumps over the lazy dog.") == 0, "str_append_fmt failed");
    str_free(s);

    printf("test_format passed\n");
}

int main() {
    test_create_and_basic_ops();
    test_manipulations();
    test_comparisons();
    test_search();
    test_trim();
    test_case_conversions();
    test_substring_and_replace();
    test_split_and_join();
    test_reverse();
    test_format();

    printf("All tests passed!\n");
    return 0;
}
