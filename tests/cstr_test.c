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
    // Create an arena with default chunk size and alignment
    Arena* arena = arena_create(0, 0);

    // Test cstr_new and cstr_free
    cstr* str1 = cstr_new(arena, 0);
    ASSERT(str1 != NULL, "cstr_new failed on cstr1");

    // Test cstr_from and cstr_free
    cstr* str2 = cstr_from(arena, "Hello, world!");
    ASSERT(str2 != NULL, "Assertion failed on cstr2");

    ASSERT_EQ(cstr_len(str2), 13, "Expected length of 13 , got: %zu\n", cstr_len(str2));
    ASSERT_EQ(strcmp(cstr_data(str2), "Hello, world!"), 0, "Expected 'Hello, world!', got: %s\n",
              cstr_data(str2));

    // Test cstr_append_char
    cstr* str3 = cstr_new(arena, 0);

    ASSERT(str3 != NULL, "Assertion failed");
    ASSERT_TRUE(cstr_append_char(arena, str3, 'H'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(arena, str3, 'e'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(arena, str3, 'l'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(arena, str3, 'l'), "Assertion failed");
    ASSERT_TRUE(cstr_append_char(arena, str3, 'o'), "Assertion failed");
    ASSERT_EQ(strcmp(cstr_data(str3), "Hello"), 0, "Expected 'Hello', got: %s\n", cstr_data(str3));

    // Test cstr_append
    cstr* str4 = cstr_new(arena, 0);
    ASSERT(str4 != NULL, "Assertion failed");
    ASSERT_TRUE(cstr_append(arena, str4, "Hello, "), "Assertion failed");
    ASSERT_TRUE(cstr_append(arena, str4, "world!"), "Assertion failed");
    ASSERT_EQ(strcmp(cstr_data(str4), "Hello, world!"), 0, "Expected 'Hello, world!', got: %s\n",
              cstr_data(str4));

    // Test cstr_append_fmt
    cstr* str5 = cstr_new(arena, 0);
    ASSERT(str5 != NULL, "Assertion failed");
    ASSERT_TRUE(cstr_append_fmt(arena, str5, "The answer is %d", 42), "Assertion failed");
    ASSERT_EQ(strcmp(cstr_data(str5), "The answer is 42"), 0,
              "Expected 'The answer is 42', got: %s\n", cstr_data(str5));

    // Test cstr_split
    cstr* str6 = cstr_from(arena, "Hello, world!");
    ASSERT(str6 != NULL, "Assertion failed");

    size_t count;
    cstr** parts = cstr_split(arena, str6, ',', &count);
    ASSERT(count == 2, "Assertion failed");
    printf("part1: %s\n", cstr_data(parts[0]));
    printf("part2: %s\n", cstr_data(parts[1]));

    ASSERT(strcmp(cstr_data(parts[0]), "Hello") == 0, "Assertion failed");
    ASSERT(strcmp(cstr_data(parts[1]), " world!") == 0, "Assertion failed");

    // Test cstr_split2 that work with plain char*
    const char* str = "Hello, world, my, people!,I really love you!";
    size_t count1;
    char** parts1 = cstr_splitchar(str, ',', &count1);

    // print the parts
    for (size_t i = 0; i < count1; ++i) {
        printf("part%ld: %s\n", i + 1, parts1[i]);
    }

    ASSERT(count1 == 5, "expected 4 parts");
    ASSERT(strcmp(parts1[0], "Hello") == 0, "Assertion failed");
    ASSERT(strcmp(parts1[1], " world") == 0, "Assertion failed");
    ASSERT(strcmp(parts1[2], " my") == 0, "Assertion failed");
    ASSERT(strcmp(parts1[3], " people!") == 0, "Assertion failed");
    ASSERT(strcmp(parts1[4], "I really love you!") == 0, "Assertion failed");
    cstr_free_array(parts1, count1);

    // Test with a string that has no delimiter
    const char* str_no_delim = "Hello";
    size_t count_no_delim;
    char** parts_no_delim = cstr_splitchar(str_no_delim, ',', &count_no_delim);
    printf("count_no_delimite: %ld: %s\n", count_no_delim, parts_no_delim[0]);
    ASSERT(count_no_delim == 1, "expected 1 part");
    ASSERT(strcmp(parts_no_delim[0], "Hello") == 0, "cstr_splitchar(): Assertion failed");
    cstr_free_array(parts_no_delim, count_no_delim);

    // Test cstr_split_at
    cstr* str7 = cstr_from(arena, "one,two,three");
    size_t count2;
    cstr** parts2 = cstr_split_at(arena, str7, ",", 4, &count2);
    printf("num_splits: %ld\n", count);

    ASSERT(count2 == 3, "cstr_split_at(): Assertion failed");
    ASSERT(strcmp(cstr_data(parts2[0]), "one") == 0, "cstr_split_at(): Assertion failed");
    ASSERT(strcmp(cstr_data(parts2[1]), "two") == 0, "cstr_split_at(): Assertion failed");
    ASSERT(strcmp(cstr_data(parts2[2]), "three") == 0, "cstr_split_at(): Assertion failed");

    // Test cstr_join
    cstr* str8 = cstr_from(arena, "one");
    cstr* str9 = cstr_from(arena, "two");
    cstr* str10 = cstr_from(arena, "three");

    cstr** strs = arena_alloc(arena, 3 * sizeof(cstr*));
    strs[0] = str8;
    strs[1] = str9;
    strs[2] = str10;
    cstr* joined = cstr_join(arena, strs, 3, ",");
    ASSERT(strcmp(cstr_data(joined), "one,two,three") == 0, "cstr_join(): Assertion failed");

    // Test cstr_substr
    cstr* str11 = cstr_from(arena, "Hello, world!");
    cstr* substr = cstr_substr(arena, str11, 7, 5);
    ASSERT(strcmp(cstr_data(substr), "world") == 0, "cstr_substr(): Assertion failed");

    // Test cstr_starts_with
    cstr* str12 = cstr_from(arena, "Hello, world!");
    ASSERT(cstr_starts_with(str12, "Hello"), "Assertion failed");
    ASSERT(!cstr_starts_with(str12, "world"), "Assertion failed");

    // Test cstr_ends_with
    cstr* str13 = cstr_from(arena, "Hello, world!");
    ASSERT(cstr_ends_with(str13, "world!"), "Assertion failed");
    ASSERT(!cstr_ends_with(str13, "Hello"), "Assertion failed");

    // Test cstr_snakecase
    cstr* str14 = cstr_from(arena, "HelloWorld");
    cstr_snakecase(arena, str14);
    ASSERT(strcmp(cstr_data(str14), "hello_world") == 0, "Assertion failed");

    // Test cstr_titlecase
    cstr* str15 = cstr_from(arena, "hello world");
    cstr_titlecase(str15);
    ASSERT(strcmp(cstr_data(str15), "Hello World") == 0, "Assertion failed");

    // Test cstr_camelcase
    cstr* str16 = cstr_from(arena, "hello_world");
    cstr_camelcase(str16);
    ASSERT(strcmp(cstr_data(str16), "helloWorld") == 0, "Assertion failed");

    // Test cstr_pascalcase
    cstr* str17 = cstr_from(arena, "hello_world");
    cstr_pascalcase(str17);
    ASSERT(strcmp(cstr_data(str17), "HelloWorld") == 0, "Assertion failed");

    // Test cstr_replace
    cstr* str18 = cstr_from(arena, "Hello, world!");
    cstr* replaced = cstr_replace(arena, str18, "world", "universe!!!!!!!!!!!!!!!!!!!!!");
    printf("%s\n", cstr_data(replaced));
    ASSERT(strcmp(cstr_data(replaced), "Hello, universe!!!!!!!!!!!!!!!!!!!!!!") == 0,
           "Assertion failed");

    // Test cstr_replace_all
    cstr* str19 = cstr_from(arena, "Hello, world! Hello, world!");
    cstr* replaced_all = cstr_replace_all(arena, str19, "world", "universe");
    printf("%s\n", cstr_data(replaced_all));
    ASSERT(strcmp(cstr_data(replaced_all), "Hello, universe! Hello, universe!") == 0,
           "Assertion failed");

    // Test cstr_ltrim
    cstr* str20 = cstr_from(arena, "   Hello, world!   ");
    cstr_ltrim(str20);
    ASSERT(strcmp(cstr_data(str20), "Hello, world!   ") == 0, "Assertion failed");

    // Test cstr_rtrim
    cstr* str21 = cstr_from(arena, "   Hello, world!   ");
    cstr_rtrim(str21);
    ASSERT(strcmp(cstr_data(str21), "   Hello, world!") == 0, "Assertion failed");

    // Test cstr_trim
    cstr* str22 = cstr_from(arena, "   Hello, world!   ");
    cstr_trim(str22);
    ASSERT(strcmp(cstr_data(str22), "Hello, world!") == 0, "Assertion failed");

    // Test cstr_trim_chars
    cstr* str23 = cstr_from(arena, "***Hello, world!***");
    cstr_trim_chars(str23, " *");
    ASSERT(strcmp(cstr_data(str23), "Hello, world!") == 0, "Assertion failed");

    // Test cstr_count_substr
    cstr* str25 = cstr_from(arena, "Hello, world! Hello, world!");
    size_t count3 = cstr_count_substr(str25, "world");
    ASSERT(count3 == 2, "Assertion failed");

    // Test cstr_remove_char
    cstr* str26 = cstr_from(arena, "Hello, world!");
    cstr_remove_char(str26->data, ',');
    ASSERT(strcmp(cstr_data(str26), "Hello world!") == 0, "Assertion failed");

    // Test cstr_remove_substr
    cstr* str27 = cstr_from(arena, "Hello, world!");
    cstr_remove_substr(str27, 7, 5);
    ASSERT(strcmp(cstr_data(str27), "Hello, !") == 0, "Assertion failed");

    // Test cstr_reverse
    cstr* str28 = cstr_from(arena, "Hello, world!");
    cstr_reverse(str28);
    ASSERT(strcmp(cstr_data(str28), "!dlrow ,olleH") == 0, "Assertion failed");

    // Test cstr_clip
    cstr* str30 = cstr_from(arena, "Hello, world!");
    cstr_ensure_capacity(arena, str30, 100);
    ASSERT(str30->capacity == 100, "Assertion failed");
    cstr_clip(arena, str30);

    // check the capacity is reduced to the minimum required
    ASSERT(str30->capacity == 14, "Assertion failed");

    arena_destroy(arena);

    printf("All tests passed!\n");
    return 0;
}