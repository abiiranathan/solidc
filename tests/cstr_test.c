#include "../include/cstr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT(cond, msg)                                                                          \
    if (!(cond)) {                                                                                 \
        printf("%s\n", msg);                                                                       \
        exit(1);                                                                                   \
    }

#define ASSERT_EQ(a, b, fmt, ...)                                                                  \
    if ((a) != (b)) {                                                                              \
        printf("Assertion failed: %s == %s\n", #a, #b);                                            \
        printf(fmt, ##__VA_ARGS__);                                                                \
        exit(1);                                                                                   \
    }

#define ASSERT_NE(a, b, fmt, ...)                                                                  \
    if ((a) == (b)) {                                                                              \
        printf("Assertion failed: %s != %s\n", #a, #b);                                            \
        printf(fmt, ##__VA_ARGS__);                                                                \
        exit(1);                                                                                   \
    }

#define ASSERT_TRUE(cond, fmt, ...)                                                                \
    if (!(cond)) {                                                                                 \
        printf("Assertion failed: %s\n", #cond);                                                   \
        printf(fmt, ##__VA_ARGS__);                                                                \
        exit(1);                                                                                   \
    }

// Test functions
int main(void) {
    // Test cstr_new and cstr_free
    cstr* str1 = cstr_new(0);
    ASSERT(str1 != NULL, "cstr_new failed on cstr1");
    cstr_free(str1);

    // Test cstr_from and cstr_free
    cstr* str2 = cstr_from("Hello, world!");
    ASSERT(str2 != NULL, "Assertion failed on cstr2");

    ASSERT_EQ(cstr_len(str2), 13, "Expected length of 13 , got: %zu\n", cstr_len(str2));
    ASSERT_EQ(strcmp(cstr_data(str2), "Hello, world!"), 0, "Expected 'Hello, world!', got: %s\n",
              cstr_data(str2));

    cstr_free(str2);

    // Test cstr_append_char
    cstr* str3 = cstr_new(0);
    ASSERT(str3 != NULL, "Assertion failed");
    ASSERT_TRUE(cstr_append_char(str3, 'H'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(str3, 'e'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(str3, 'l'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(str3, 'l'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(str3, 'o'), "Assertion failed");
    ASSERT_EQ(strcmp(cstr_data(str3), "Hello"), 0, "Expected 'Hello', got: %s\n", cstr_data(str3));
    cstr_free(str3);

    // Test cstr_append
    cstr* str4 = cstr_new(0);
    ASSERT(str4 != NULL, "Assertion failed");
    ASSERT_TRUE(cstr_append(str4, "Hello, "), "Assertion failed");
    ASSERT_TRUE(cstr_append(str4, "world!"), "Assertion failed");
    ASSERT_EQ(strcmp(cstr_data(str4), "Hello, world!"), 0, "Expected 'Hello, world!', got: %s\n",
              cstr_data(str4));
    cstr_free(str4);

    // Test cstr_append_fmt
    cstr* str5 = cstr_new(0);
    ASSERT(str5 != NULL, "Assertion failed");
    ASSERT_TRUE(cstr_append_fmt(str5, "The answer is %d", 42), "Assertion failed");
    ASSERT_EQ(strcmp(cstr_data(str5), "The answer is 42"), 0,
              "Expected 'The answer is 42', got: %s\n", cstr_data(str5));
    cstr_free(str5);

    // Test cstr_split
    cstr* str6 = cstr_from("Hello, world!");
    ASSERT(str6 != NULL, "Assertion failed");

    size_t count;
    cstr** parts = cstr_split(str6, ',', &count);
    ASSERT(count == 2, "Assertion failed");
    ASSERT(strcmp(cstr_data(parts[0]), "Hello") == 0, "Assertion failed");
    ASSERT(strcmp(cstr_data(parts[1]), " world!") == 0, "Assertion failed");
    for (size_t i = 0; i < count; ++i) {
        cstr_free(parts[i]);
    }
    free(parts);
    cstr_free(str6);

    // Test cstr_split_at
    cstr* str7 = cstr_from("one,two,three");
    size_t count2;
    cstr** parts2 = cstr_split_at(str7, ",", 4, &count2);
    ASSERT(count2 == 3, "Assertion failed");
    ASSERT(strcmp(cstr_data(parts2[0]), "one") == 0, "Assertion failed");
    ASSERT(strcmp(cstr_data(parts2[1]), "two") == 0, "Assertion failed");
    ASSERT(strcmp(cstr_data(parts2[2]), "three") == 0, "Assertion failed");
    for (size_t i = 0; i < count2; ++i) {
        cstr_free(parts2[i]);
    }
    free(parts2);
    cstr_free(str7);

    // Test cstr_join
    cstr* str8 = cstr_from("one");
    cstr* str9 = cstr_from("two");
    cstr* str10 = cstr_from("three");
    cstr** strs = malloc(3 * sizeof(cstr*));
    strs[0] = str8;
    strs[1] = str9;
    strs[2] = str10;
    cstr* joined = cstr_join(strs, 3, ",");
    ASSERT(strcmp(cstr_data(joined), "one,two,three") == 0, "Assertion failed");
    cstr_free(joined);
    free(strs);
    cstr_free(str8);
    cstr_free(str9);
    cstr_free(str10);

    // Test cstr_substr
    cstr* str11 = cstr_from("Hello, world!");
    cstr* substr = cstr_substr(str11, 7, 5);
    ASSERT(strcmp(cstr_data(substr), "world") == 0, "Assertion failed");
    cstr_free(substr);
    cstr_free(str11);

    // Test cstr_starts_with
    cstr* str12 = cstr_from("Hello, world!");
    ASSERT(cstr_starts_with(str12, "Hello"), "Assertion failed");
    ASSERT(!cstr_starts_with(str12, "world"), "Assertion failed");
    cstr_free(str12);

    // Test cstr_ends_with
    cstr* str13 = cstr_from("Hello, world!");
    ASSERT(cstr_ends_with(str13, "world!"), "Assertion failed");
    ASSERT(!cstr_ends_with(str13, "Hello"), "Assertion failed");
    cstr_free(str13);

    // Test cstr_snakecase
    cstr* str14 = cstr_from("HelloWorld");
    cstr_snakecase(str14);
    ASSERT(strcmp(cstr_data(str14), "hello_world") == 0, "Assertion failed");
    cstr_free(str14);

    // Test cstr_titlecase
    cstr* str15 = cstr_from("hello world");
    cstr_titlecase(str15);
    ASSERT(strcmp(cstr_data(str15), "Hello World") == 0, "Assertion failed");
    cstr_free(str15);

    // Test cstr_camelcase
    cstr* str16 = cstr_from("hello_world");
    cstr_camelcase(str16);
    ASSERT(strcmp(cstr_data(str16), "helloWorld") == 0, "Assertion failed");
    cstr_free(str16);

    // Test cstr_pascalcase
    cstr* str17 = cstr_from("hello_world");
    cstr_pascalcase(str17);
    ASSERT(strcmp(cstr_data(str17), "HelloWorld") == 0, "Assertion failed");
    cstr_free(str17);

    // Test cstr_replace
    cstr* str18 = cstr_from("Hello, world!");
    cstr* replaced = cstr_replace(str18, "world", "universe!!!!!!!!!!!!!!!!!!!!!");
    printf("%s\n", cstr_data(replaced));
    ASSERT(strcmp(cstr_data(replaced), "Hello, universe!!!!!!!!!!!!!!!!!!!!!!") == 0,
           "Assertion failed");
    cstr_free(str18);
    cstr_free(replaced);

    // Test cstr_replace_all
    cstr* str19 = cstr_from("Hello, world! Hello, world!");
    cstr* replaced_all = cstr_replace_all(str19, "world", "universe");
    printf("%s\n", cstr_data(replaced_all));
    ASSERT(strcmp(cstr_data(replaced_all), "Hello, universe! Hello, universe!") == 0,
           "Assertion failed");
    cstr_free(str19);
    cstr_free(replaced_all);

    // Test cstr_ltrim
    cstr* str20 = cstr_from("   Hello, world!   ");
    cstr_ltrim(str20);
    ASSERT(strcmp(cstr_data(str20), "Hello, world!   ") == 0, "Assertion failed");
    cstr_free(str20);

    // Test cstr_rtrim
    cstr* str21 = cstr_from("   Hello, world!   ");
    cstr_rtrim(str21);
    ASSERT(strcmp(cstr_data(str21), "   Hello, world!") == 0, "Assertion failed");
    cstr_free(str21);

    // Test cstr_trim
    cstr* str22 = cstr_from("   Hello, world!   ");
    cstr_trim(str22);
    ASSERT(strcmp(cstr_data(str22), "Hello, world!") == 0, "Assertion failed");
    cstr_free(str22);

    // Test cstr_trim_chars
    cstr* str23 = cstr_from("***Hello, world!***");
    cstr_trim_chars(str23, " *");
    ASSERT(strcmp(cstr_data(str23), "Hello, world!") == 0, "Assertion failed");
    cstr_free(str23);

    // Test cstr_count_substr
    cstr* str25 = cstr_from("Hello, world! Hello, world!");
    size_t count3 = cstr_count_substr(str25, "world");
    ASSERT(count3 == 2, "Assertion failed");
    cstr_free(str25);

    // Test cstr_remove_char
    cstr* str26 = cstr_from("Hello, world!");
    cstr_remove_char(str26->data, ',');
    ASSERT(strcmp(cstr_data(str26), "Hello world!") == 0, "Assertion failed");
    cstr_free(str26);

    // Test cstr_remove_substr
    cstr* str27 = cstr_from("Hello, world!");
    cstr_remove_substr(str27, 7, 5);
    ASSERT(strcmp(cstr_data(str27), "Hello, !") == 0, "Assertion failed");
    cstr_free(str27);

    // Test cstr_reverse
    cstr* str28 = cstr_from("Hello, world!");
    cstr_reverse(str28);
    ASSERT(strcmp(cstr_data(str28), "!dlrow ,olleH") == 0, "Assertion failed");
    cstr_free(str28);

    // Test cstr_clip
    cstr* str30 = cstr_from("Hello, world!");
    cstr_ensure_capacity(str30, 100);
    ASSERT(str30->capacity == 100, "Assertion failed");
    cstr_clip(&str30);
    // check the capacity is reduced to the minimum required
    ASSERT(str30->capacity == 14, "Assertion failed");
    cstr_free(str30);

    // Test cstr_with_allocator
    Arena* arena = arena_create(1024, 0);
    cstr_allocator allocator = cstr_arena_allocator(arena);

    cstr* str29 = cstr_from_with_allocator("Hello, world!", allocator);
    ASSERT(str29 != NULL, "Assertion failed");
    ASSERT(strcmp(cstr_data(str29), "Hello, world!") == 0, "Assertion failed");

    cstr_free(str29);
    arena_destroy(arena);

    printf("All tests passed!\n");
    return 0;
}