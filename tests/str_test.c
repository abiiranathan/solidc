#include "../include/str.h"
#include "../include/arena.h"
#include "../include/macros.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Helper function to print str content for debugging
void print_str(const char* s) { printf("str: '%s' (len=%zu)\n", s ? s : "NULL", s ? strlen(s) : 0); }

int main(void) {
    printf("Starting cstr library tests...\n\n");

    // Test str_empty
    ASSERT(str_is_empty("") && "str_empty should return true for empty string");

    // Test str_concat
    const char* sc = str_concat("Hello", ", ", "World!", NULL);
    ASSERT(strcmp(sc, "Hello, World!") == 0 && "str_concat content");
    free((void*)sc);  // Free the memory allocated by str_concat

    // Test str_starts_with
    ASSERT_TRUE(str_starts_with("Hello, World", "Hello") && "str_starts_with valid prefix");

    // Test str_ends_with
    ASSERT_TRUE(str_ends_with("Hello, World", "World") && "str_ends_with valid suffix");

    // Test str_find
    int found = str_find("Hello, World", "World");
    ASSERT(found == 7 && "str_find valid substring");

    // Test str_rfind
    found = str_rfind("Hello, World, Hello", "Hello");
    ASSERT(found == 14 && "str_rfind last occurrence");

    // ---------------------------------------------------------
    // IN-PLACE MODIFICATION TESTS
    // Use a mutable character array instead of string literals
    // ---------------------------------------------------------
    char text[256];

    // Test str_to_lower
    strcpy(text, "HeLLo WoRLd");
    str_lower(text);
    ASSERT(strcmp(text, "hello world") == 0 && "str_to_lower content");

    // Test str_to_upper
    strcpy(text, "HeLLo WoRLd");
    str_upper(text);
    ASSERT(strcmp(text, "HELLO WORLD") == 0 && "str_to_upper content");

    // Test str_snake_case (Allocates memory because it expands the string)
    char* snake = str_to_snakecase("HelloWorldMyDearFriend");
    ASSERT(strcmp(snake, "hello_world_my_dear_friend") == 0 && "str_snake_case content");
    free(snake);

    // HTTPServer -> http_server
    snake = str_to_snakecase("HTTPServer");
    ASSERT(strcmp(snake, "http_server") == 0 && "str_snake_case all caps content");
    free(snake);

    // Test str_camel_case
    strcpy(text, "hello_world");
    str_camelcase(text);
    ASSERT(strcmp(text, "helloWorld") == 0 && "str_camel_case content");

    // Test str_pascal_case
    strcpy(text, "hello_world");
    str_pascalcase(text);
    ASSERT(strcmp(text, "HelloWorld") == 0 && "str_pascal_case content");

    // Test str_title_case
    strcpy(text, "hello world");
    str_titlecase(text);
    ASSERT(strcmp(text, "Hello World") == 0 && "str_title_case content");

    // Test str_trim
    strcpy(text, "  Hello  ");
    str_trim(text);
    ASSERT(strcmp(text, "Hello") == 0 && "str_trim content");

    // Test str_rtrim
    strcpy(text, "Hello  ");
    str_rtrim(text);
    ASSERT(strcmp(text, "Hello") == 0 && "str_rtrim content");

    // Test str_ltrim
    strcpy(text, "  Hello");
    str_ltrim(text);
    ASSERT(strcmp(text, "Hello") == 0 && "str_ltrim content");

    // Test str_trim_chars
    strcpy(text, "***Hello***");
    str_trim_chars(text, "*");
    ASSERT(strcmp(text, "Hello") == 0 && "str_trim_chars content");

    // Test str_count_substr
    size_t count = str_count_substr("hello hello world", "hello");
    ASSERT(count == 2 && "str_count_substr count");

    count = str_count_substr("aaaaa", "aa");
    ASSERT(count == 2 && "str_count_substr overlapping count");

    // Test str_remove_char
    str_remove_char(NULL, 'x');  // Should not crash

    strcpy(text, "Hello ***World***");
    str_remove_char(text, '*');
    ASSERT(strcmp(text, "Hello World") == 0 && "str_remove_char content");

    // Test str_substr
    ASSERT_NULL(str_substr(NULL, 0, 1));      // Should not crash
    ASSERT_NULL(str_substr("Hello", 10, 1));  // Should not crash
    char* substr = str_substr("Hello, World", 7, 5);
    ASSERT_STR_EQ(substr, "World");
    free(substr);

    // Test str_replace
    str_replace(NULL, "old", "new");  // Should not crash
    char* replaced = str_replace("hello hello world", "hello", "hi");
    ASSERT_STR_EQ(replaced, "hi hello world");
    free(replaced);

    // Test str_replace_all
    replaced = str_replace_all("hello hello world", "hello", "hi");
    ASSERT_STR_EQ(replaced, "hi hi world");
    free(replaced);

    // Test str_split
    const char* long_prose =
        "It was the best of times, it was the worst of times, "
        "it was the age of wisdom, it was the age of foolishness, "
        "it was the epoch of belief, it was the epoch of incredulity, "
        "it was the season of Light, it was the season of Darkness, "
        "it was the spring of hope, it was the winter of despair.";

    size_t count_out;
    char** parts = str_split(long_prose, ", ", &count_out);  // null-terminated array of parts, count in count_out
    ASSERT(count_out == 10 && "str_split count");
    ASSERT_STR_EQ(parts[0], "It was the best of times");
    ASSERT_STR_EQ(parts[1], "it was the worst of times");

    ASSERT_STR_EQ(parts[7], "it was the season of Darkness");
    ASSERT_NULL(parts[10]);  // Should not crash
    str_free_split(parts);

    // Test str_join
    const char* join_parts[] = {"Hello", "World", "Test"};
    char* joined = str_join(join_parts, 3, ", ");
    ASSERT_STR_EQ(joined, "Hello, World, Test");
    free(joined);

    // Test str_reverse
    {
        printf("\nTesting str_reverse...\n");
        strcpy(text, "Hello");
        str_reverse(text);
        ASSERT_STR_EQ(text, "olleH");
    }

    printf("\nAll tests passed successfully!\n");
    return 0;
}
