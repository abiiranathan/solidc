#include "str.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT(cond)                                                                       \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            exit(1);                                                                       \
        }                                                                                  \
    } while (0)

#define ASSERT_STR_EQ(actual, expected)                                                                             \
    do {                                                                                                            \
        const char* a = (actual);                                                                                   \
        const char* e = (expected);                                                                                 \
        if (!a || !e || strcmp(a, e) != 0) {                                                                        \
            fprintf(stderr, "STR MISMATCH: got \"%s\", expected \"%s\" at %s:%d\n", a ? a : "NULL", e ? e : "NULL", \
                    __FILE__, __LINE__);                                                                            \
            exit(1);                                                                                                \
        }                                                                                                           \
    } while (0)

int main(void) {
    printf("Starting str.h comprehensive test runner...\n\n");

    /* 1. Predicates */
    {
        printf("Testing Predicates...\n");
        ASSERT(str_is_empty(NULL));
        ASSERT(str_is_empty(""));
        ASSERT(!str_is_empty("a"));

        ASSERT(str_is_blank(NULL));
        ASSERT(str_is_blank("  \t\n "));
        ASSERT(!str_is_blank("  a "));

        ASSERT(str_is_alpha("Hello"));
        ASSERT(!str_is_alpha("Hello1"));

        ASSERT(str_is_digit("12345"));
        ASSERT(!str_is_digit("123a5"));

        ASSERT(str_is_numeric("-12345"));
        ASSERT(str_is_numeric("+42"));
        ASSERT(!str_is_numeric("--42"));

        ASSERT(str_is_float("3.14159"));
        ASSERT(str_is_float("-1e10"));
        ASSERT(!str_is_float("3.14a"));

        ASSERT(str_equals("test", "test"));
        ASSERT(!str_equals("test", "Test"));

        ASSERT(str_iequals("test", "Test"));
        ASSERT(str_starts_with("HelloWorld", "Hello"));
        ASSERT(str_ends_with("HelloWorld", "World"));
        ASSERT(str_contains("HelloWorld", "loWo"));
    }

    /* 2. SWAR Case Conversion */
    {
        printf("Testing SWAR Case Conversion...\n");
        char buf1[64] = "HELLO WORLD! 123 TESTING_SWAR_PATH_WITH_LONG_STRING_64BIT";
        str_lower(buf1);
        ASSERT_STR_EQ(buf1, "hello world! 123 testing_swar_path_with_long_string_64bit");

        char buf2[64] = "hello world! 123 testing_swar_path_with_long_string_64bit";
        str_upper(buf2);
        ASSERT_STR_EQ(buf2, "HELLO WORLD! 123 TESTING_SWAR_PATH_WITH_LONG_STRING_64BIT");

        char buf3[32] = "hello_world_test";
        str_capitalize(buf3);
        ASSERT_STR_EQ(buf3, "Hello_world_test");
    }

    /* 3. Casing Helpers (Camel, Pascal, Title, Snake) */
    {
        printf("Testing Casing Helpers...\n");
        char camel[64] = "__hello-world--my   dear_friend--";
        str_camelcase(camel);
        ASSERT_STR_EQ(camel, "helloWorldMyDearFriend");

        char pascal[64] = "--hello-world my_dear_friend--";
        str_pascalcase(pascal);
        ASSERT_STR_EQ(pascal, "HelloWorldMyDearFriend");

        char title[64] = "  hElLo   wORLD!  foo-bar_baz  ";
        str_titlecase(title);
        ASSERT_STR_EQ(title, "  Hello   World!  Foo-Bar_Baz  ");

        char* snake = str_to_snakecase("XMLParserIOStream");
        ASSERT_STR_EQ(snake, "xml_parser_io_stream");
        free(snake);
    }

    /* 4. Vectorized Removal & Trimming */
    {
        printf("Testing Vectorized Removal & Trimming...\n");
        char rem_buf[64] = "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z";
        str_remove_char(rem_buf, ',');
        ASSERT_STR_EQ(rem_buf, "abcdefghijklmnopqrstuvwxyz");

        char trim_buf[32] = "...Hello...";
        str_trim_chars(trim_buf, ".");
        ASSERT_STR_EQ(trim_buf, "Hello");

        char rem_all[64] = "foo bar foo bar foo";
        ASSERT(str_remove_all(rem_all, "foo ") == 2);
        ASSERT_STR_EQ(rem_all, "bar bar foo");
    }

    /* 5. Memory Security Zeroization */
    {
        printf("Testing Security Memory Zeroization (str_wipe)...\n");
        char secret[] = "SuperSecretPassword123!";
        size_t len = strlen(secret);
        str_wipe(secret);
        ASSERT(strlen(secret) == 0);
        for (size_t i = 0; i < len; i++) {
            ASSERT(secret[i] == '\0');
        }
    }

    /* 6. Allocating Helpers & Formatting */
    {
        printf("Testing Allocating Helpers...\n");
        char* dup = str_dup("hello");
        ASSERT_STR_EQ(dup, "hello");
        free(dup);

        char* rep = str_repeat("ab", 3);
        ASSERT_STR_EQ(rep, "ababab");
        free(rep);

        char* pad_l = str_pad_left("42", 5, '0');
        ASSERT_STR_EQ(pad_l, "00042");
        free(pad_l);

        char* pad_r = str_pad_right("42", 5, ' ');
        ASSERT_STR_EQ(pad_r, "42   ");
        free(pad_r);

        char* ctr = str_center("hi", 6, '-');
        ASSERT_STR_EQ(ctr, "--hi--");
        free(ctr);

        char* replaced = str_replace_all("hello hello world", "hello", "hi");
        ASSERT_STR_EQ(replaced, "hi hi world");
        free(replaced);
    }

    /* 7. Optimized Split & Join */
    {
        printf("Testing Optimized Split & Join...\n");
        size_t count = 0;

        /* Single-character split path */
        char** parts1 = str_split("a,,b,c,", ",", &count);
        ASSERT(count == 5);
        ASSERT_STR_EQ(parts1[0], "a");
        ASSERT_STR_EQ(parts1[1], "");
        ASSERT_STR_EQ(parts1[2], "b");
        ASSERT_STR_EQ(parts1[3], "c");
        ASSERT_STR_EQ(parts1[4], "");
        str_free_split(parts1);

        /* Multi-character split path */
        char** parts2 = str_split("a<br><br>b", "<br>", &count);
        ASSERT(count == 3);
        ASSERT_STR_EQ(parts2[0], "a");
        ASSERT_STR_EQ(parts2[1], "");
        ASSERT_STR_EQ(parts2[2], "b");
        str_free_split(parts2);

        /* Join fast-path */
        const char* join_arr[] = {"1", "2", "3"};
        char* joined = str_join(join_arr, 3, "-");
        ASSERT_STR_EQ(joined, "1-2-3");
        free(joined);

        /* Concat variadic */
        char* concat = str_concat("Hello", ", ", "world", "!", NULL);
        ASSERT_STR_EQ(concat, "Hello, world!");
        free(concat);
    }

    /* 8. Number Conversions */
    {
        printf("Testing Number Conversions...\n");
        char num_buf[32];
        ASSERT_STR_EQ(str_from_int(-42, num_buf, sizeof(num_buf)), "-42");
        ASSERT_STR_EQ(str_from_long(1000000L, num_buf, sizeof(num_buf)), "1000000");
        ASSERT_STR_EQ(str_from_double(3.14159, 2, num_buf, sizeof(num_buf)), "3.14");
    }

    printf("\nAll str.h tests passed successfully!\n");
    return 0;
}
