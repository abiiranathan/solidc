#include "../include/cstr.h"
#include "../include/macros.h"

int main(void) {
    cstr* str1 = str_new(0);
    ASSERT(str1 != NULL);

    cstr* str2 = str_from("Hello, world!");
    ASSERT(str2 != NULL);

    ASSERT_EQ(str_len(str2), 13);
    ASSERT_STR_EQ(str_data(str2), "Hello, world!");

    cstr* str3 = str_new(0);
    ASSERT(str3 != NULL);
    ASSERT_TRUE(str_append_char(&str3, 'H'));
    ASSERT_TRUE(str_append_char(&str3, 'e'));
    ASSERT_TRUE(str_append_char(&str3, 'l'));
    ASSERT_TRUE(str_append_char(&str3, 'l'));
    ASSERT_TRUE(str_append_char(&str3, 'o'));
    ASSERT_STR_EQ(str_data(str3), "Hello");

    // Test str_append
    cstr* str4 = str_new(0);
    ASSERT(str4 != NULL);
    ASSERT_TRUE(str_append(&str4, "Hello, "));
    ASSERT_TRUE(str_append(&str4, "world!"));
    ASSERT_STR_EQ(str_data(str4), "Hello, world!");

    // Test str_append_fmt
    cstr* str5 = str_new(0);
    ASSERT(str5 != NULL);
    ASSERT_TRUE(str_append_fmt(&str5, "The answer is %d", 42));
    ASSERT_STR_EQ(str_data(str5), "The answer is 42");

    // Test str_split
    cstr* str6 = str_from("Hello, world!");
    ASSERT(str6 != NULL);

    size_t count;
    cstr** parts = str_split(str6, ",", &count);
    ASSERT(count == 2);
    printf("part1: %s\n", str_data(parts[0]));
    printf("part2: %s\n", str_data(parts[1]));

    ASSERT_STR_EQ(str_data(parts[0]), "Hello");
    ASSERT_STR_EQ(str_data(parts[1]), " world!");

    // Test str_split2 that work with plain char*
    const cstr* str = str_from("Hello, world, my, people!,I really love you!");
    ASSERT(str != NULL);
    size_t count1;
    cstr** parts1 = str_split(str, ",", &count1);

    // print the parts
    for (size_t i = 0; i < count1; ++i) {
        printf("part%ld: %s\n", i + 1, str_data(parts1[i]));
    }

    ASSERT(count1 == 5);
    ASSERT_STR_EQ(str_data(parts1[0]), "Hello");
    ASSERT_STR_EQ(str_data(parts1[1]), " world");
    ASSERT_STR_EQ(str_data(parts1[2]), " my");
    ASSERT_STR_EQ(str_data(parts1[3]), " people!");
    ASSERT_STR_EQ(str_data(parts1[4]), "I really love you!");

    for (size_t i = 0; i < count; ++i) {
        str_free(parts[i]);
    }

    // Test str_split_at
    cstr* str7 = str_from("one,two,three");
    size_t count2;
    cstr** parts2 = str_split(str7, ",", &count2);

    ASSERT(count2 == 3);
    ASSERT_STR_EQ(str_data(parts2[0]), "one");
    ASSERT_STR_EQ(str_data(parts2[1]), "two");
    ASSERT_STR_EQ(str_data(parts2[2]), "three");

    size_t k = 0;
    parts    = str_split(str_from("Host: localhost:8080"), ": ", &k);
    ASSERT(k == 2);
    ASSERT_STR_EQ(str_data(parts[0]), "Host");
    ASSERT_STR_EQ(str_data(parts[1]), "localhost:8080");

    // Test str_substr
    cstr* str11  = str_from("Hello, world!");
    cstr* substr = str_substr(str11, 7, 5);
    ASSERT_STR_EQ(str_data(substr), "world");

    // Test str_starts_with
    cstr* str12 = str_from("Hello, world!");
    ASSERT(str_starts_with(str12, "Hello"));
    ASSERT(!str_starts_with(str12, "world"));

    // Test str_ends_with
    cstr* str13 = str_from("Hello, world!");
    ASSERT(str_ends_with(str13, "world!"));
    ASSERT(!str_ends_with(str13, "Hello"));

    // Test str_snakecase
    cstr* str14 = str_from("HelloWorld");
    str_snake_case(str14);
    ASSERT_STR_EQ(str_data(str14), "hello_world");

    // Test str_titlecase
    cstr* str15 = str_from("hello world");
    str_title_case(str15);
    ASSERT_STR_EQ(str_data(str15), "Hello World");

    // Test str_camelcase
    cstr* str16 = str_from("hello_world");
    str_camel_case(str16);
    ASSERT_STR_EQ(str_data(str16), "helloWorld");

    // Test str_pascalcase
    cstr* str17 = str_from("hello_world");
    str_pascal_case(str17);
    ASSERT_STR_EQ(str_data(str17), "HelloWorld");

    // Test str_replace
    cstr* str18    = str_from("Hello, world!");
    cstr* replaced = str_replace(str18, "world", "universe!!!!!!!!!!!!!!!!!!!!!");
    printf("%s\n", str_data(replaced));
    ASSERT_STR_EQ(str_data(replaced), "Hello, universe!!!!!!!!!!!!!!!!!!!!!!");

    // Test str_replace_all
    cstr* str19        = str_from("Hello, world! Hello, world!");
    cstr* replaced_all = str_replace_all(str19, "world", "universe");
    printf("%s\n", str_data(replaced_all));
    ASSERT_STR_EQ(str_data(replaced_all), "Hello, universe! Hello, universe!");

    // Test str_ltrim
    cstr* str20 = str_from("   Hello, world!   ");
    str_ltrim(str20);
    ASSERT_STR_EQ(str_data(str20), "Hello, world!   ");

    // Test str_rtrim
    cstr* str21 = str_from("   Hello, world!   ");
    str_rtrim(str21);
    ASSERT_STR_EQ(str_data(str21), "   Hello, world!");

    // Test str_trim
    cstr* str22 = str_from("   Hello, world!   ");
    str_trim(str22);
    ASSERT_STR_EQ(str_data(str22), "Hello, world!");

    // Test str_trim_chars
    cstr* str23 = str_from("***Hello, world!***");
    str_trim_chars(str23, " *");
    ASSERT_STR_EQ(str_data(str23), "Hello, world!");

    // Test str_count_substr
    cstr* str25   = str_from("Hello, world! Hello, world!");
    size_t count3 = str_count_substr(str25, "world");
    ASSERT(count3 == 2);

    // Test str_remove_char
    cstr* str26 = str_from("Hello, world!");
    str_remove_char(str26, ',');
    ASSERT_STR_EQ(str_data(str26), "Hello world!");

    // Test str_remove_substr
    cstr* str27 = str_from("Hello, world!");
    str_remove_substr(str27, 7, 5);
    ASSERT_STR_EQ(str_data(str27), "Hello, !");

    // Test str_reverse
    cstr* str28 = str_from("Hello, world!");
    str_reverse_in_place(str28);
    ASSERT_STR_EQ(str_data(str28), "!dlrow ,olleH");

    cstr* str29 = str_from("Hello, world!");
    cstr* str30 = str_reverse(str29);
    ASSERT_STR_EQ(str_data(str30), "!dlrow ,olleH");

    cstr* text = str_from("A man a plan a canal Panama");
    ASSERT(text);

    str_remove_all(&text, " ");
    printf("Text: %s\n", str_data(text));

    return 0;
}
