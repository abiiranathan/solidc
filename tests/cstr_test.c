#include "../include/cstr.h"
#include "../include/arena.h"
#include "../include/macros.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // for rand, malloc, free
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
void ASSERT_cstr_equals(const cstr* s, const char* expected, const char* test_name) {
    if (!s && !expected) {
        return;
    }

    ASSERT(s && expected && "cstr or expected is NULL");
    const char* data = cstr_data_const(s);
    ASSERT(data);

    bool matches = strcmp(data, expected) == 0 && "Content mismatch";
    ASSERT(matches);

    ASSERT(cstr_len(s) == strlen(expected) && "Length mismatch");
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
        printf("Testing str_init...\n");
        cstr* s = cstr_init(0);
        ASSERT_cstr_equals(s, "", "str_new with zero capacity");
        ASSERT(cstr_capacity(s) >= 1);
        cstr_free(s);

        s = cstr_init(2048);
        ASSERT_cstr_equals(s, "", "str_init with large capacity");
        ASSERT(cstr_capacity(s) >= 2048);
        cstr_free(s);

        // Make sure large strings fail to overflow
        s = cstr_init(SIZE_MAX);
        cstr_debug(s);
        ASSERT_EQ(s, NULL);  // overflow should be handled
        puts("Overflow check passed");
    }

    // Test cstr_new
    {
        printf("\nTesting cstr_new...\n");
        cstr* s = cstr_new("Hello");
        ASSERT_cstr_equals(s, "Hello", "cstr_new with valid string");
        cstr_free(s);

        s = cstr_new("");
        ASSERT_cstr_equals(s, "", "cstr_new with empty string");
        cstr_free(s);

        s = cstr_new(NULL);
        ASSERT(s == NULL && "cstr_new with NULL should return NULL");
        printf("cstr_new with NULL: Passed\n");
    }

    // Test str_format
    {
        printf("\nTesting str_format...\n");
        cstr* s = cstr_format("Hello, %s! %d", "World", 42);
        ASSERT_cstr_equals(s, "Hello, World! 42", "str_format with valid format");
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
        ASSERT(cstr_len(s) == 5 && "str_len incorrect");
        ASSERT(cstr_capacity(s) >= 6 && "str_capacity incorrect");
        printf("str_len and str_capacity: Passed\n");
        cstr_free(s);

        ASSERT(cstr_len(NULL) == 0 && "str_len with NULL incorrect");
        ASSERT(cstr_capacity(NULL) == 0 && "str_capacity with NULL incorrect");
        printf("str_len and str_capacity with NULL: Passed\n");
    }

    // Test str_empty
    {
        printf("\nTesting str_empty...\n");
        cstr* s = cstr_new("");
        ASSERT(cstr_empty(s) && "str_empty with empty string");
        cstr_free(s);

        s = cstr_new("NonEmpty");
        ASSERT(!cstr_empty(s) && "str_empty with non-empty string");
        cstr_free(s);

        ASSERT(cstr_empty(NULL) && "str_empty with NULL");
        printf("str_empty: Passed\n");
    }

    // Test str_resize
    {
        printf("\nTesting str_resize...\n");
        cstr* s = cstr_new("Test");
        ASSERT(cstr_resize(s, 10) && "str_resize failed");
        ASSERT(cstr_capacity(s) >= 10 && "str_resize capacity incorrect");
        ASSERT_cstr_equals(s, "Test", "str_resize content preserved");

        // Resize with large capacity should fail.
        ASSERT(cstr_resize(s, SIZE_MAX - 1) == false);
        cstr_debug(s);

        cstr_free(s);
    }

    // Test str_append
    {
        printf("\nTesting str_append...\n");
        cstr* s = cstr_new("Hello");
        ASSERT(cstr_append(s, ", World!") && "str_append failed");
        ASSERT_cstr_equals(s, "Hello, World!", "str_append content");
        cstr_free(s);

        s = cstr_new("");
        ASSERT(cstr_append(s, "") && "str_append empty string");
        ASSERT_cstr_equals(s, "", "str_append empty string content");
        cstr_free(s);
    }

    // Test str_append_fast
    {
        printf("\nTesting str_append_fast...\n");
        cstr* s = cstr_init(20);
        ASSERT(cstr_append_fast(s, "Hello") && "str_append_fast failed");
        ASSERT_cstr_equals(s, "Hello", "str_append_fast content");
        cstr_free(s);
    }

    // Test str_append_fmt
    {
        printf("\nTesting str_append_fmt...\n");
        cstr* s = cstr_new("Hello");
        ASSERT(cstr_append_fmt(s, ", %s! %d", "World", 42) && "str_append_fmt failed");
        ASSERT_cstr_equals(s, "Hello, World! 42", "str_append_fmt content");
        cstr_free(s);
    }

    // Test str_append_char
    {
        printf("\nTesting str_append_char...\n");
        cstr* s = cstr_new("Hello");
        ASSERT(cstr_append_char(s, '!') && "str_append_char failed");
        ASSERT_cstr_equals(s, "Hello!", "str_append_char content");
        cstr_free(s);
    }

    // Test str_prepend
    {
        printf("\nTesting str_prepend...\n");
        cstr* s = cstr_new("World");
        ASSERT(cstr_prepend(s, "Hello, ") && "str_prepend failed");
        ASSERT_cstr_equals(s, "Hello, World", "str_prepend content");
        cstr_free(s);

        s = cstr_new("");
        ASSERT(cstr_prepend(s, "") && "str_prepend empty string");
        ASSERT_cstr_equals(s, "", "str_prepend empty string content");
        cstr_free(s);
    }

    // Test str_prepend_fast
    {
        printf("\nTesting str_prepend_fast...\n");
        cstr* s = cstr_init(20);
        ASSERT(cstr_prepend_fast(s, "Hello") && "str_prepend_fast failed");
        ASSERT_cstr_equals(s, "Hello", "str_prepend_fast content");
        cstr_free(s);
    }

    // Test str_insert
    {
        printf("\nTesting str_insert...\n");
        cstr* s = cstr_new("HelloWorld");
        ASSERT(cstr_insert(s, 5, ", ") && "str_insert failed");
        ASSERT_cstr_equals(s, "Hello, World", "str_insert content");
        cstr_free(s);

        s = cstr_new("Test");
        ASSERT(!cstr_insert(s, 5, "x") && "str_insert beyond length should fail");
        cstr_free(s);
    }

    // Test str_remove
    {
        printf("\nTesting str_remove...\n");
        cstr* s = cstr_new("Hello, World");
        ASSERT(cstr_remove(s, 5, 2) && "str_remove failed");
        ASSERT_cstr_equals(s, "HelloWorld", "str_remove content");
        cstr_free(s);

        s = cstr_new("Test");
        ASSERT(cstr_remove(s, 4, 0) && "str_remove at end with zero count");
        ASSERT_cstr_equals(s, "Test", "str_remove at end content");
        cstr_free(s);
    }

    // Test str_clear
    {
        printf("\nTesting str_clear...\n");
        cstr* s = cstr_new("Hello");
        cstr_clear(s);
        ASSERT_cstr_equals(s, "", "str_clear content");
        cstr_free(s);
    }

    // Test str_remove_all
    {
        printf("\nTesting str_remove_all...\n");
        cstr* s = cstr_new("hello hello world");
        ASSERT(cstr_remove_all(s, "hello ") == 2 && "str_remove_all count incorrect");
        ASSERT_cstr_equals(s, "world", "str_remove_all content");
        cstr_free(s);

        s = cstr_new("test");
        ASSERT(cstr_remove_all(s, "x") == 0 && "str_remove_all no match");
        ASSERT_cstr_equals(s, "test", "str_remove_all no match content");
        cstr_free(s);
    }

    // Test str_at
    {
        printf("\nTesting str_at...\n");
        cstr* s = cstr_new("Hello");
        ASSERT(cstr_at(s, 1) == 'e' && "str_at incorrect");
        ASSERT(cstr_at(s, 5) == '\0' && "str_at out of bounds");
        cstr_free(s);
        ASSERT(cstr_at(NULL, 0) == '\0' && "str_at with NULL");
        printf("str_at: Passed\n");
    }

    // Test str_data
    {
        printf("\nTesting str_data...\n");
        cstr* s = cstr_new("Hello");
        ASSERT(strcmp(cstr_data(s), "Hello") == 0 && "str_data incorrect");
        cstr_free(s);
        ASSERT(cstr_data(NULL) == NULL && "str_data with NULL");
        printf("str_data: Passed\n");
    }

    // Test str_as_view
    {
        printf("\nTesting str_as_view...\n");
        cstr* s = cstr_new("Hello");
        cstr_view v = cstr_as_view(s);
        ASSERT(v.length == 5 && strcmp(v.data, "Hello") == 0 && "str_as_view incorrect");
        cstr_free(s);
        v = cstr_as_view(NULL);
        ASSERT(v.data == NULL && v.length == 0 && "str_as_view with NULL");
        printf("str_as_view: Passed\n");
    }

    // Test str_compare
    {
        printf("\nTesting str_compare...\n");
        cstr* s1 = cstr_new("apple");
        cstr* s2 = cstr_new("banana");
        ASSERT(cstr_cmp(s1, s2) < 0 && "str_compare apple < banana");
        ASSERT(cstr_cmp(s2, s1) > 0 && "str_compare banana > apple");
        cstr* s3 = cstr_new("apple");
        ASSERT(cstr_cmp(s1, s3) == 0 && "str_compare equal strings");
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
        ASSERT(cstr_equals(s1, s2) && "str_equals equal strings");
        ASSERT(!cstr_equals(s1, s3) && "str_equals different strings");
        cstr_free(s1);
        cstr_free(s2);
        cstr_free(s3);
        printf("str_equals: Passed\n");
    }

    // Test str_starts_with
    {
        printf("\nTesting str_starts_with...\n");
        cstr* s = cstr_new("Hello, World");
        ASSERT(cstr_starts_with(s, "Hello") && "str_starts_with valid prefix");
        ASSERT(!cstr_starts_with(s, "World") && "str_starts_with invalid prefix");
        ASSERT(cstr_starts_with(s, "") && "str_starts_with empty prefix");
        cstr_free(s);
        printf("str_starts_with: Passed\n");
    }

    // Test str_ends_with
    {
        printf("\nTesting str_ends_with...\n");
        cstr* s = cstr_new("Hello, World");
        ASSERT(cstr_ends_with(s, "World") && "str_ends_with valid suffix");
        ASSERT(!cstr_ends_with(s, "Hello") && "str_ends_with invalid suffix");
        ASSERT(cstr_ends_with(s, "") && "str_ends_with empty suffix");
        cstr_free(s);
        printf("str_ends_with: Passed\n");
    }

    // Test str_find
    {
        printf("\nTesting str_find...\n");
        cstr* s = cstr_new("Hello, World");
        ASSERT(cstr_find(s, "World") == 7 && "str_find valid substring");
        ASSERT(cstr_find(s, "NotFound") == CSTR_NPOS && "str_find not found");
        cstr_free(s);
        printf("str_find: Passed\n");
    }

    // Test str_rfind
    {
        printf("\nTesting str_rfind...\n");
        cstr* s = cstr_new("hello hello world");
        ASSERT(cstr_rfind(s, "hello") == 6 && "str_rfind last occurrence");
        ASSERT(cstr_rfind(s, "notfound") == CSTR_NPOS && "str_rfind not found");
        cstr_free(s);
        printf("str_rfind: Passed\n");
    }

    // Test cstr_lower (scalar, SWAR, and scalar cleanup paths)
    {
        printf("\nTesting cstr_lower...\n");

        // 1. Short string (< 8 bytes: scalar path)
        cstr* s_short = cstr_new("HELLO");
        cstr_lower(s_short);
        ASSERT_cstr_equals(s_short, "hello", "cstr_lower short content");
        cstr_free(s_short);

        // 2. Exactly 8 bytes (1 full SWAR chunk, 0 scalar remainder)
        cstr* s_exact = cstr_new("ABCDEFGH");
        cstr_lower(s_exact);
        ASSERT_cstr_equals(s_exact, "abcdefgh", "cstr_lower exact 8-byte SWAR");
        cstr_free(s_exact);

        // 3. Long string (> 8 bytes with symbols/numbers: tests multiple SWAR chunks + scalar cleanup)
        const char* input_long = "HELLO WORLD! 123 TESTING_SWAR_PATH_WITH_LONG_STRING_64BIT";
        const char* expected_long = "hello world! 123 testing_swar_path_with_long_string_64bit";

        cstr* s_long = cstr_new(input_long);
        cstr_lower(s_long);
        ASSERT_cstr_equals(s_long, expected_long, "cstr_lower long SWAR content");
        cstr_free(s_long);
    }

    // Test cstr_upper (scalar, SWAR, and scalar cleanup paths)
    {
        printf("\nTesting cstr_upper...\n");

        // 1. Short string (< 8 bytes: scalar path)
        cstr* s_short = cstr_new("hello");
        cstr_upper(s_short);
        ASSERT_cstr_equals(s_short, "HELLO", "cstr_upper short content");
        cstr_free(s_short);

        // 2. Exactly 8 bytes (1 full SWAR chunk, 0 scalar remainder)
        cstr* s_exact = cstr_new("abcdefgh");
        cstr_upper(s_exact);
        ASSERT_cstr_equals(s_exact, "ABCDEFGH", "cstr_upper exact 8-byte SWAR");
        cstr_free(s_exact);

        // 3. Long string (> 8 bytes with symbols/numbers: tests multiple SWAR chunks + scalar cleanup)
        const char* input_long = "hello world! 123 testing_swar_path_with_long_string_64bit";
        const char* expected_long = "HELLO WORLD! 123 TESTING_SWAR_PATH_WITH_LONG_STRING_64BIT";

        cstr* s_long = cstr_new(input_long);
        cstr_upper(s_long);
        ASSERT_cstr_equals(s_long, expected_long, "cstr_upper long SWAR content");
        cstr_free(s_long);
    }

    // Regression: GPR-SWAR carry contamination (see simd.h design notes).
    // The old implementation missed 'a'/'A' and corrupted '[', '{', UTF-8.
    {
        printf("\nTesting cstr case-conversion edge cases...\n");

        struct {
            const char* input;
            const char* lower;
            const char* upper;
        } cases[] = {
            {"aaaaaaaa", "aaaaaaaa", "AAAAAAAA"}, /* all-'a': old SWAR no-op'd upper */
            {"AAAAAAAA", "aaaaaaaa", "AAAAAAAA"}, /* all-'A': old SWAR no-op'd lower */
            {"[a]{b}", "[a]{b}", "[A]{B}"},       /* bracket/brace neighbours */
            {"z[\\]^_", "z[\\]^_", "Z[\\]^_"},    /* 0x5B-0x5F range */
            {"`abcdefghijklmno", "`abcdefghijklmno", "`ABCDEFGHIJKLMNO"},
            {"pqrstuvwxyz{", "pqrstuvwxyz{", "PQRSTUVWXYZ{"},
            {"Mixed CASE with 1234!", "mixed case with 1234!", "MIXED CASE WITH 1234!"},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            cstr* sl = cstr_new(cases[i].input);
            cstr_lower(sl);
            ASSERT_cstr_equals(sl, cases[i].lower, "regression lower");
            cstr_free(sl);

            cstr* su = cstr_new(cases[i].input);
            cstr_upper(su);
            ASSERT_cstr_equals(su, cases[i].upper, "regression upper");
            cstr_free(su);
        }

        /* UTF-8 bytes (>= 0x80) must never be modified. */
        const char* utf8 = "caf\xc3\xa9 na\xc3\xafve r\xc3\xa9sum\xc3\xa9 xx"; /* > 16 bytes */
        cstr* s8 = cstr_new(utf8);
        cstr_lower(s8);
        ASSERT_cstr_equals(s8, utf8, "cstr_lower preserves UTF-8 bytes");
        cstr_free(s8); /* heap-allocated cstr: free(), not drop() */
        s8 = cstr_new(utf8);
        cstr_upper(s8);
        {
            /* Only ASCII letters change; every high byte must survive. */
            const char* got = cstr_data_const(s8);
            int corrupt = 0;
            for (const char* p = utf8; *p; p++) {
                if ((unsigned char)*p >= 0x80 && !memchr(got, *p, strlen(utf8))) corrupt++;
            }
            ASSERT(corrupt == 0 && "cstr_upper preserves all UTF-8 bytes");
        }
        cstr_free(s8);

        /* Deterministic pseudo-fuzz: mixed-case ASCII round-trips exactly. */
        unsigned rng = 12345;
        char in[64], lo[64], up[64];
        for (int t = 0; t < 2000; t++) {
            uint32_t len = 1 + (rng >> 8) % 60;
            for (uint32_t i = 0; i < len; i++) {
                rng = rng * 1103515245u + 12345u;
                unsigned r = (rng >> 16) % 10;
                in[i] = (char)((r < 6) ? ('A' + (rng >> 3) % 26)
                                       : ((r < 9) ? ('a' + (rng >> 3) % 26) : ('!' + (rng >> 5) % 6)));
            }
            in[len] = '\0';
            for (uint32_t i = 0; i < len; i++) {
                unsigned char c = (unsigned char)in[i];
                lo[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
                up[i] = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
            }
            lo[len] = up[len] = '\0';

            cstr* sl = cstr_new(in);
            cstr_lower(sl);
            ASSERT_cstr_equals(sl, lo, "fuzz lower");
            cstr_free(sl);

            cstr* su = cstr_new(in);
            cstr_upper(su);
            ASSERT_cstr_equals(su, up, "fuzz upper");
            cstr_free(su);
        }
    }

    // Test str_snake_case
    {
        printf("\nTesting str_snake_case...\n");

        // 1. CamelCase -> snake_case with SSO promotion
        cstr* s1 = cstr_new("HelloWorldMyDearFriend");
        ASSERT(s1);
        /* len 22 exceeds the SSO capacity, so this string must already be
         * heap-allocated (SSO promotion happened at construction). */
        ASSERT(cstr_allocated(s1));
        ASSERT(cstr_snakecase(s1));
        ASSERT_cstr_equals(s1, "hello_world_my_dear_friend", "str_snake_case CamelCase content");
        cstr_free(s1);

        // 2. Kebab-case and spaces
        cstr* s2 = cstr_new("hello-world my dear friend");
        ASSERT(cstr_snakecase(s2));
        ASSERT_cstr_equals(s2, "hello_world_my_dear_friend", "str_snake_case kebab/spaces content");
        cstr_free(s2);

        // 3. Acronyms & consecutive uppercase
        cstr* s3 = cstr_new("XMLParserIOStream");
        ASSERT(cstr_snakecase(s3));
        ASSERT_cstr_equals(s3, "xml_parser_io_stream", "str_snake_case acronyms content");
        cstr_free(s3);
    }

    // Test str_camel_case
    {
        printf("\nTesting str_camel_case...\n");

        // 1. Standard snake_case
        cstr* s1 = cstr_new("hello_world");
        cstr_camelcase(s1);
        ASSERT_cstr_equals(s1, "helloWorld", "str_camel_case snake_case content");
        cstr_free(s1);

        // 2. Kebab-case, spaces, multiple delimiters, and leading/trailing delimiters
        cstr* s2 = cstr_new("__hello-world--my   dear_friend--");
        cstr_camelcase(s2);
        ASSERT_cstr_equals(s2, "helloWorldMyDearFriend", "str_camel_case mixed delimiters");
        cstr_free(s2);

        // 3. PascalCase input
        cstr* s3 = cstr_new("HelloWorld");
        cstr_camelcase(s3);
        ASSERT_cstr_equals(s3, "helloWorld", "str_camel_case PascalCase input");
        cstr_free(s3);
    }

    // Test str_pascal_case
    {
        printf("\nTesting str_pascal_case...\n");

        // 1. Standard snake_case
        cstr* s1 = cstr_new("hello_world");
        cstr_pascalcase(s1);
        ASSERT_cstr_equals(s1, "HelloWorld", "str_pascal_case snake_case content");
        cstr_free(s1);

        // 2. Kebab-case and leading hyphens/spaces
        cstr* s2 = cstr_new("--hello-world my_dear_friend--");
        cstr_pascalcase(s2);
        ASSERT_cstr_equals(s2, "HelloWorldMyDearFriend", "str_pascal_case mixed delimiters");
        cstr_free(s2);

        // 3. camelCase input
        cstr* s3 = cstr_new("helloWorld");
        cstr_pascalcase(s3);
        ASSERT_cstr_equals(s3, "HelloWorld", "str_pascal_case camelCase input");
        cstr_free(s3);
    }

    // Test str_title_case
    {
        printf("\nTesting str_title_case...\n");

        // 1. Standard space-separated
        cstr* s1 = cstr_new("hello world");
        cstr_titlecase(s1);
        ASSERT_cstr_equals(s1, "Hello World", "str_title_case standard content");
        cstr_free(s1);

        // 2. Mixed case, multiple spaces, and non-alpha symbols
        cstr* s2 = cstr_new("  hElLo   wORLD!  foo-bar_baz  ");
        cstr_titlecase(s2);
        ASSERT_cstr_equals(s2, "  Hello   World!  Foo-Bar_Baz  ", "str_title_case mixed formatting");
        cstr_free(s2);
    }

    // Test str_trim
    {
        printf("\nTesting str_trim...\n");
        cstr* s = cstr_new("  Hello  ");
        cstr_trim(s);
        ASSERT_cstr_equals(s, "Hello", "str_trim content");
        cstr_free(s);
    }

    // Test str_rtrim
    {
        printf("\nTesting str_rtrim...\n");
        cstr* s = cstr_new("Hello  ");
        cstr_rtrim(s);
        ASSERT_cstr_equals(s, "Hello", "str_rtrim content");
        cstr_free(s);
    }

    // Test str_ltrim
    {
        printf("\nTesting str_ltrim...\n");
        cstr* s = cstr_new("  Hello");
        cstr_ltrim(s);
        ASSERT_cstr_equals(s, "Hello", "str_ltrim content");
        cstr_free(s);
    }

    // Test str_trim_chars
    {
        printf("\nTesting str_trim_chars...\n");
        cstr* s = cstr_new("...Hello...");
        cstr_trim_chars(s, ".");
        ASSERT_cstr_equals(s, "Hello", "str_trim_chars content");
        cstr_free(s);
    }

    // Test str_count_substr
    {
        printf("\nTesting str_count_substr...\n");
        cstr* s = cstr_new("hello hello world");
        ASSERT(cstr_count_substr(s, "hello") == 2 && "str_count_substr count");
        ASSERT(cstr_count_substr(s, "notfound") == 0 && "str_count_substr not found");
        cstr_free(s);
        printf("str_count_substr: Passed\n");
    }

    // Test str_remove_char
    {
        printf("\nTesting str_remove_char...\n");
        cstr* s = cstr_new("hello");
        cstr_remove_char(s, 'l');
        ASSERT_cstr_equals(s, "heo", "str_remove_char content");
        cstr_free(s);
    }

    // Test str_substr
    {
        printf("\nTesting str_substr...\n");
        cstr* s = cstr_new("Hello, World");
        cstr* sub = cstr_substr(s, 7, 5);
        ASSERT_cstr_equals(sub, "World", "str_substr content");
        cstr_free(sub);
        cstr_free(s);
    }

    // Test str_replace
    {
        printf("\nTesting str_replace...\n");
        cstr* s = cstr_new("hello hello world");
        cstr* result = cstr_replace(s, "hello", "hi");
        ASSERT_cstr_equals(result, "hi hello world", "str_replace first occurrence");
        cstr_free(s);
        cstr_free(result);

        s = cstr_new("test");
        result = cstr_replace(s, "notfound", "x");
        ASSERT_cstr_equals(result, "test", "str_replace not found");
        cstr_free(s);
        cstr_free(result);
    }

    // Test str_replace_all
    {
        printf("\nTesting str_replace_all...\n");
        cstr* s = cstr_new("hello hello world");
        cstr* result = cstr_replace_all(s, "hello", "hi");
        ASSERT_cstr_equals(result, "hi hi world", "str_replace_all content");
        cstr_free(s);
        cstr_free(result);
    }

    // Test str_split
    {
        printf("\n**************Testing str_split***************\n");

        // Basic case
        cstr* s = cstr_new("a,b,c");
        size_t count = 0;
        cstr** arr = cstr_split(s, ",", &count);
        ASSERT(count == 3 && "str_split count incorrect");
        ASSERT_cstr_equals(arr[0], "a", "str_split first element");
        ASSERT_cstr_equals(arr[1], "b", "str_split second element");
        ASSERT_cstr_equals(arr[2], "c", "str_split third element");
        free_cstr_array(arr, count);
        cstr_free(s);

        // Empty string case
        s = cstr_new("");
        arr = cstr_split(s, ",", &count);
        ASSERT(count == 1 && "empty string should return one empty element");
        ASSERT_cstr_equals(arr[0], "", "empty string element");
        free_cstr_array(arr, count);
        cstr_free(s);

        // No delimiter case
        s = cstr_new("abc");
        arr = cstr_split(s, ",", &count);
        ASSERT(count == 1 && "no delimiter should return original string");
        ASSERT_cstr_equals(arr[0], "abc", "no delimiter element");
        free_cstr_array(arr, count);
        cstr_free(s);

        // Long prose string case
        const char* long_prose =
            "It was the best of times, it was the worst of times, "
            "it was the age of wisdom, it was the age of foolishness, "
            "it was the epoch of belief, it was the epoch of incredulity, "
            "it was the season of Light, it was the season of Darkness, "
            "it was the spring of hope, it was the winter of despair.";
        s = cstr_new(long_prose);
        arr = cstr_split(s, ", ", &count);  // Split on comma+space

        // Verify we got the expected number of splits
        const size_t expected_splits = 10;  // 10 pairs separated by ", "
        printf("Splits=%zu\n", count);
        ASSERT(count == expected_splits && "long prose split count incorrect");

        // Verify first and last segments
        ASSERT_cstr_equals(arr[0], "It was the best of times", "long prose first element");
        ASSERT_cstr_equals(arr[expected_splits - 1], "it was the winter of despair.", "long prose last element");

        // Verify a middle segment
        ASSERT_cstr_equals(arr[7], "it was the season of Darkness", "long prose middle element");

        // Performance test (just output time, no ASSERT)
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
        s = cstr_new(",a,b,c,");
        arr = cstr_split(s, ",", &count);
        ASSERT(count == 5 && "edge delimiters count incorrect");
        ASSERT_cstr_equals(arr[0], "", "leading delimiter element");
        ASSERT_cstr_equals(arr[4], "", "trailing delimiter element");
        free_cstr_array(arr, count);
        cstr_free(s);

        printf("All str_split tests passed!\n");
    }

    // Test str_join
    {
        printf("\nTesting str_join...\n");
        cstr* s1 = cstr_new("Hello");
        cstr* s2 = cstr_new("World");
        cstr* arr[] = {s1, s2};
        cstr* result = cstr_join((const cstr**)arr, 2, ", ");
        ASSERT_cstr_equals(result, "Hello, World", "str_join content");
        cstr_free(s1);
        cstr_free(s2);
        cstr_free(result);
    }

    // Test str_reverse
    {
        printf("\nTesting str_reverse...\n");
        cstr* s = cstr_new("Hello");
        cstr* result = cstr_reverse(s);
        ASSERT_cstr_equals(result, "olleH", "str_reverse content");
        cstr_free(s);
        cstr_free(result);
    }

    // Test str_reverse_in_place
    {
        printf("\nTesting str_reverse_in_place...\n");
        cstr* s = cstr_new("Hello");
        cstr_reverse_inplace(s);
        ASSERT_cstr_equals(s, "olleH", "str_reverse_in_place content");
        cstr_free(s);
    }

    // =========== More comprehensive tests

    {
        printf("Testing str_len / str_capacity...\n");
        cstr* s = cstr_new("Hello");
        ASSERT(cstr_len(s) == 5);
        ASSERT(cstr_capacity(s) >= 6);
        cstr_free(s);
        ASSERT(cstr_len(NULL) == 0);
        ASSERT(cstr_capacity(NULL) == 0);
    }

    {
        printf("Testing str_empty...\n");
        cstr* s = cstr_new("");
        ASSERT(cstr_empty(s));
        cstr_free(s);
        s = cstr_new("x");
        ASSERT(!cstr_empty(s));
        cstr_free(s);
        ASSERT(cstr_empty(NULL));
    }

    {
        printf("Testing str_resize...\n");
        cstr* s = cstr_new("Resize");
        ASSERT(cstr_resize(s, 20));
        ASSERT(cstr_capacity(s) >= 20);
        ASSERT_cstr_equals(s, "Resize", "str_resize preserves content");
        cstr_free(s);
    }

    {
        printf("Testing str_append...\n");
        cstr* s = cstr_new("Hi");
        ASSERT(cstr_append(s, " there"));
        ASSERT_cstr_equals(s, "Hi there", "str_append");
        cstr_free(s);
    }

    {
        printf("Testing str_append_fmt...\n");
        cstr* s = cstr_new("Hi");
        ASSERT(cstr_append_fmt(s, ", %s!", "friend"));
        ASSERT_cstr_equals(s, "Hi, friend!", "str_append_fmt");
        cstr_free(s);
    }

    {
        printf("Testing str_append_char...\n");
        cstr* s = cstr_new("End");
        ASSERT(cstr_append_char(s, '!'));
        ASSERT_cstr_equals(s, "End!", "str_append_char");
        cstr_free(s);
    }

    {
        printf("Testing str_prepend...\n");
        cstr* s = cstr_new("tail");
        ASSERT(cstr_prepend(s, "head "));
        ASSERT_cstr_equals(s, "head tail", "str_prepend");
        cstr_free(s);
    }

    {
        printf("Testing str_insert...\n");
        cstr* s = cstr_new("Helo");
        ASSERT(cstr_insert(s, 2, "l"));
        ASSERT_cstr_equals(s, "Hello", "str_insert");
        cstr_free(s);
    }

    {
        printf("Testing str_remove...\n");
        cstr* s = cstr_new("Helloo!");
        ASSERT(cstr_remove(s, 5, 1));
        ASSERT_cstr_equals(s, "Hello!", "str_remove");
        cstr_free(s);
    }

    {
        printf("Testing str_clear...\n");
        cstr* s = cstr_new("NotEmpty");
        cstr_clear(s);
        ASSERT_cstr_equals(s, "", "str_clear");
        cstr_free(s);
    }

    {
        printf("Testing str_remove_all...\n");
        cstr* s = cstr_new("foo bar foo bar foo");
        size_t removed = cstr_remove_all(s, "foo ");
        ASSERT(removed == 2);
        ASSERT_cstr_equals(s, "bar bar foo", "str_remove_all");
        cstr_free(s);
    }

    {
        printf("Testing str_at...\n");
        cstr* s = cstr_new("Hey");
        ASSERT(cstr_at(s, 0) == 'H');
        ASSERT(cstr_at(s, 3) == '\0');
        cstr_free(s);
    }

    {
        printf("Testing str_data...\n");
        cstr* s = cstr_new("Raw");
        ASSERT(strcmp(cstr_data(s), "Raw") == 0);
        cstr_free(s);
    }

    {
        printf("Testing str_as_view...\n");
        cstr* s = cstr_new("Slice");
        cstr_view v = cstr_as_view(s);
        ASSERT(v.data && strcmp(v.data, "Slice") == 0 && v.length == 5);
        cstr_free(s);
    }

    // ================== extened tests ==========================
    // ============================================
    // Test Self-Aliasing & Reallocation Safety (UAF Prevention)
    // ============================================
    {
        printf("\n**************Testing Self-Aliasing Safety***************\n");

        // 1. Self-append when SSO -> Heap promotion occurs
        cstr* s1 = cstr_new("Hello");      // Length 5 (SSO)
        ASSERT(cstr_append_cstr(s1, s1));  // "HelloHello" (Len 10)
        ASSERT_cstr_equals(s1, "HelloHello", "self_append_sso_1");
        ASSERT(cstr_append_cstr(s1, s1));  // "HelloHelloHelloHello" (Len 20 -> Promotes to Heap)
        ASSERT_cstr_equals(s1, "HelloHelloHelloHello", "self_append_sso_promotion");
        cstr_free(s1);

        // 2. Self-append on Heap with realloc growth
        cstr* s2 = cstr_new("1234567890123456");  // Heap allocated (Len 16)
        ASSERT(cstr_append_cstr(s2, s2));         // Triggers heap realloc
        ASSERT_cstr_equals(s2, "12345678901234561234567890123456", "self_append_heap_realloc");
        cstr_free(s2);

        // 3. Self-prepend with realloc growth
        cstr* s3 = cstr_new("PrependMe!");
        ASSERT(cstr_prepend_cstr(s3, s3));
        ASSERT_cstr_equals(s3, "PrependMe!PrependMe!", "self_prepend_growth");
        cstr_free(s3);

        // 4. Self-insert with realloc growth
        cstr* s4 = cstr_new("AB");
        ASSERT(cstr_insert_cstr(s4, 1, s4));  // "AAB B" -> "AABB"
        ASSERT_cstr_equals(s4, "AABB", "self_insert_growth");
        cstr_free(s4);

        printf("Self-Aliasing tests passed!\n");
    }

    // ============================================
    // Test Security Memory Zeroization (cstr_wipe)
    // ============================================
    {
        printf("\n**************Testing cstr_wipe***************\n");
        cstr* s = cstr_new("SensitivePassword123!");
        size_t orig_cap = cstr_capacity(s);

        cstr_wipe(s);
        ASSERT(cstr_len(s) == 0 && "cstr_wipe should reset length to 0");
        ASSERT_cstr_equals(s, "", "cstr_wipe string content empty");
        ASSERT(cstr_capacity(s) == orig_cap && "cstr_wipe preserves buffer capacity");

        // Verify underlying memory was zeroed out
        const char* raw = cstr_data_const(s);
        for (size_t i = 0; i < orig_cap; i++) {
            ASSERT(raw[i] == '\0' && "cstr_wipe memory byte zeroed");
        }

        cstr_free(s);
        printf("cstr_wipe tests passed!\n");
    }

    // ============================================
    // Test Binary Safety & Embedded NULs in Trim
    // ============================================
    {
        printf("\n**************Testing Binary Safety in Trim***************\n");

        // Create a string with embedded NUL byte: "abc\0def"
        cstr* s = cstr_new_len("abc\0def", 7);
        ASSERT(cstr_len(s) == 7);

        // Trimming 'a' should not treat the embedded '\0' as a character to trim
        cstr_trim_chars(s, "a");
        ASSERT(cstr_len(s) == 6);
        ASSERT(memcmp(cstr_data(s), "bc\0def", 6) == 0);

        cstr_free(s);
        printf("Binary safety trim tests passed!\n");
    }

    // ============================================
    // Test Vectorized cstr_remove_char
    // ============================================
    {
        printf("\n**************Testing Vectorized cstr_remove_char***************\n");

        // 1. Target char not present
        cstr* s1 = cstr_new("hello world");
        cstr_remove_char(s1, 'z');
        ASSERT_cstr_equals(s1, "hello world", "remove_char missing");
        cstr_free(s1);

        // 2. Remove leading, middle, and trailing target occurrences
        cstr* s2 = cstr_new("xhelloxworldx");
        cstr_remove_char(s2, 'x');
        ASSERT_cstr_equals(s2, "helloworld", "remove_char lead/mid/trail");
        cstr_free(s2);

        // 3. Remove all characters (string becomes empty)
        cstr* s3 = cstr_new("aaaaaa");
        cstr_remove_char(s3, 'a');
        ASSERT_cstr_equals(s3, "", "remove_char all match");
        cstr_free(s3);

        // 4. Long string (>32 bytes) sparse removal (SWAR/memchr fast path)
        cstr* s4 = cstr_new("a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z");
        cstr_remove_char(s4, ',');
        ASSERT_cstr_equals(s4, "abcdefghijklmnopqrstuvwxyz", "remove_char long sparse");
        cstr_free(s4);

        printf("Vectorized cstr_remove_char tests passed!\n");
    }

    // ============================================
    // Test 1-Byte vs Multi-Byte cstr_split Fast Path
    // ============================================
    {
        printf("\n**************Testing cstr_split Delimiter Edge Cases***************\n");

        size_t count = 0;

        // 1. Single-character delimiter with consecutive delimiters
        cstr* s1 = cstr_new("a,,b,c,");
        cstr** arr1 = cstr_split(s1, ",", &count);
        ASSERT(count == 5);
        ASSERT_cstr_equals(arr1[0], "a", "split 1-char [0]");
        ASSERT_cstr_equals(arr1[1], "", "split 1-char consecutive [1]");
        ASSERT_cstr_equals(arr1[2], "b", "split 1-char [2]");
        ASSERT_cstr_equals(arr1[3], "c", "split 1-char [3]");
        ASSERT_cstr_equals(arr1[4], "", "split 1-char trailing [4]");
        free_cstr_array(arr1, count);
        cstr_free(s1);

        // 2. Multi-character delimiter with consecutive matches
        cstr* s2 = cstr_new("a<br><br>b<br>");
        cstr** arr2 = cstr_split(s2, "<br>", &count);
        ASSERT(count == 4);
        ASSERT_cstr_equals(arr2[0], "a", "split multi-char [0]");
        ASSERT_cstr_equals(arr2[1], "", "split multi-char consecutive [1]");
        ASSERT_cstr_equals(arr2[2], "b", "split multi-char [2]");
        ASSERT_cstr_equals(arr2[3], "", "split multi-char trailing [3]");
        free_cstr_array(arr2, count);
        cstr_free(s2);

        printf("cstr_split edge cases passed!\n");
    }

    // ============================================
    // Test Optimized cstr_join Edge Cases
    // ============================================
    {
        printf("\n**************Testing cstr_join Edge Cases***************\n");

        // 1. Single string in array (no delimiter added)
        cstr* s1 = cstr_new("Solo");
        const cstr* arr1[] = {s1};
        cstr* res1 = cstr_join(arr1, 1, ", ");
        ASSERT_cstr_equals(res1, "Solo", "join single element");
        cstr_free(s1);
        cstr_free(res1);

        // 2. 1-Byte delimiter fast-path
        cstr* a = cstr_new("1");
        cstr* b = cstr_new("2");
        cstr* c = cstr_new("3");
        const cstr* arr2[] = {a, b, c};
        cstr* res2 = cstr_join(arr2, 3, "-");
        ASSERT_cstr_equals(res2, "1-2-3", "join 1-byte delimiter");

        // 3. Empty string delimiter
        cstr* res3 = cstr_join(arr2, 3, "");
        ASSERT_cstr_equals(res3, "123", "join empty delimiter");

        // 4. Joining elements containing empty strings
        cstr* empty_str = cstr_new("");
        const cstr* arr3[] = {a, empty_str, b};
        cstr* res4 = cstr_join(arr3, 3, ":");
        ASSERT_cstr_equals(res4, "1::2", "join containing empty elements");

        cstr_free(a);
        cstr_free(b);
        cstr_free(c);
        cstr_free(empty_str);
        cstr_free(res2);
        cstr_free(res3);
        cstr_free(res4);

        printf("cstr_join edge cases passed!\n");
    }

    // Fuzz with large inputs
    {
        // This will reallocate multiple times.
        // but stack string will be promoted to heap.
        for (int i = 0; i < 1000; ++i) {
            cstr* s = cstr_init(0);
            for (int j = 0; j < 100; ++j) {
                ASSERT(cstr_append_char(s, 'a' + (rand() % 26)));
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
            ASSERT(cstr_resize(s, 100));

            for (int j = 0; j < 100; ++j) ASSERT(cstr_append_char(s, 'a' + (rand() % 26)));
            cstr_free(s);
        }
    }

    // ============================================
    // Test New Optimized Input Functions
    // ============================================
    {
        printf("\n**************Testing Optimized Input Functions***************\n");
        cstr* s = cstr_new("Hello World");
        cstr* sub = cstr_new("World");
        cstr* pre = cstr_new("Hello");

        // Ends With
        ASSERT(cstr_ends_with_cstr(s, sub));
        ASSERT(!cstr_ends_with_cstr(s, pre));

        // Starts With
        ASSERT(cstr_starts_with_cstr(s, pre));
        ASSERT(!cstr_starts_with_cstr(s, sub));

        // Find / Contains
        ASSERT(cstr_contains_cstr(s, sub));
        ASSERT(cstr_find_cstr(s, sub) == 6);

        // RFind
        cstr* s2 = cstr_new("Hello World World");
        ASSERT(cstr_rfind_cstr(s2, sub) == 12);
        cstr_free(s2);

        // Comparison
        ASSERT(cstr_cmp(s, s) == 0);
        ASSERT(cstr_cmp(s, sub) < 0);
        ASSERT(cstr_ncmp(s, pre, 5) == 0);

        // Append cstr
        cstr* s3 = cstr_new("Foo");
        cstr* s4 = cstr_new("Bar");
        ASSERT(cstr_append_cstr(s3, s4));
        ASSERT_cstr_equals(s3, "FooBar", "cstr_append_cstr");

        // Cat / NCat
        ASSERT(cstr_cat(s3, s4));
        ASSERT_cstr_equals(s3, "FooBarBar", "cstr_cat");

        ASSERT(cstr_ncat(s3, s4, 2));
        ASSERT_cstr_equals(s3, "FooBarBarBa", "cstr_ncat");

        // Copy / Assign
        ASSERT(cstr_assign(s3, s4));
        ASSERT_cstr_equals(s3, "Bar", "cstr_assign");

        // Prepend
        ASSERT(cstr_prepend_cstr(s3, s4));
        ASSERT_cstr_equals(s3, "BarBar", "cstr_prepend_cstr");

        // Insert
        ASSERT(cstr_insert_cstr(s3, 3, s4));
        ASSERT_cstr_equals(s3, "BarBarBar", "cstr_insert_cstr");

        // Remove All
        cstr_remove_all_cstr(s3, s4);
        ASSERT_cstr_equals(s3, "", "cstr_remove_all_cstr");

        cstr_free(s);
        cstr_free(sub);
        cstr_free(pre);
        cstr_free(s3);
        cstr_free(s4);
        printf("Optimized Input tests passed!\n");
    }

    printf("\nAll tests passed successfully!\n");
    return 0;
}
