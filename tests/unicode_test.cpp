#define UNICODE_IMPL

#include "../unicode.h"

#include <gtest/gtest.h>

TEST(UnicodeTest, UcpToUtf8) {
    char utf8[5];
    ucp_to_utf8(0x00000041, utf8);
    EXPECT_STREQ(utf8, "A");
    ucp_to_utf8(0x000000C0, utf8);
    EXPECT_STREQ(utf8, "\xC3\x80");
    ucp_to_utf8(0x00000801, utf8);
    EXPECT_STREQ(utf8, "\xE0\xA0\x81");
    ucp_to_utf8(0x00010001, utf8);
    EXPECT_STREQ(utf8, "\xF0\x90\x80\x81");

    // Emojis
    ucp_to_utf8(0x0001F600, utf8);
    EXPECT_STREQ(utf8, "\xF0\x9F\x98\x80");  // 😀

    ucp_to_utf8(0x0001F1FA, utf8);
    EXPECT_STREQ(utf8, "\xF0\x9F\x87\xBA");  // 🇺
}

TEST(UnicodeTest, Utf8ToCodepoint) {
    EXPECT_EQ(utf8_to_codepoint("A"), 0x00000041);                 // 65
    EXPECT_EQ(utf8_to_codepoint("\xC3\x80"), 0x000000C0);          // 192
    EXPECT_EQ(utf8_to_codepoint("\xE0\xA0\x81"), 0x00000801);      // 2049
    EXPECT_EQ(utf8_to_codepoint("\xF0\x90\x80\x81"), 0x00010001);  // 65537

    // Emojis
    EXPECT_EQ(utf8_to_codepoint("\xF0\x9F\x98\x80"), 0x0001F600);  // 😀
    EXPECT_EQ(utf8_to_codepoint("\xF0\x9F\x87\xBA"), 0x0001F1FA);  // 🇺
}

TEST(UnicodeTest, Utf8CountCodepoints) {
    EXPECT_EQ(utf8_count_codepoints("A"), 1);
    EXPECT_EQ(utf8_count_codepoints("\xC3\x80"), 1);
    EXPECT_EQ(utf8_count_codepoints("\xE0\xA0\x81"), 1);
    EXPECT_EQ(utf8_count_codepoints("\xF0\x90\x80\x81"), 1);
    EXPECT_EQ(utf8_count_codepoints("A\xC3\x80\xE0\xA0\x81\xF0\x90\x80\x81"), 4);
    EXPECT_EQ(utf8_count_codepoints(
                  "A\xC3\x80\xE0\xA0\x81\xF0\x90\x80\x81\xF0\x9F\x98\x80\xF0\x9F\x87\xBA"),
              6);
}

TEST(UnicodeTest, Utf8ByteLength) {
    EXPECT_EQ(utf8_byte_length("A"), 1);
    EXPECT_EQ(utf8_byte_length("\xC3\x80"), 2);
    EXPECT_EQ(utf8_byte_length("\xE0\xA0\x81"), 3);
    EXPECT_EQ(utf8_byte_length("\xF0\x90\x80\x81"), 4);
    EXPECT_EQ(utf8_byte_length("A\xC3\x80\xE0\xA0\x81\xF0\x90\x80\x81"), 10);
    EXPECT_EQ(
        utf8_byte_length("A\xC3\x80\xE0\xA0\x81\xF0\x90\x80\x81\xF0\x9F\x98\x80\xF0\x9F\x87\xBA"),
        18);
}

TEST(UnicodeTest, IsWhitespaceCodepoint) {
    EXPECT_TRUE(is_codepoint_whitespace(0x00000009));  // \t
    EXPECT_TRUE(is_codepoint_whitespace(0x0000000A));  // \n
    EXPECT_TRUE(is_codepoint_whitespace(0x0000000B));  // \v
    EXPECT_TRUE(is_codepoint_whitespace(0x0000000C));  // \f
    EXPECT_TRUE(is_codepoint_whitespace(0x0000000D));  // \r
    EXPECT_TRUE(is_codepoint_whitespace(0x00000020));  // space
}

TEST(UnicodeTest, IsWhitespaceUtf8) {
    EXPECT_TRUE(is_utf8_whitespace("\t"));  // \t
    EXPECT_TRUE(is_utf8_whitespace("\n"));  // \n
    EXPECT_TRUE(is_utf8_whitespace("\v"));  // \v
    EXPECT_TRUE(is_utf8_whitespace("\f"));  // \f
    EXPECT_TRUE(is_utf8_whitespace("\r"));  // \r
    EXPECT_TRUE(is_utf8_whitespace(" "));   // space
}

TEST(UnicodeTest, IsDigitCodepoint) {
    EXPECT_TRUE(is_codepoint_digit(0x00000030));  // 0
    EXPECT_TRUE(is_codepoint_digit(0x00000031));  // 1
    EXPECT_TRUE(is_codepoint_digit(0x00000032));  // 2
    EXPECT_TRUE(is_codepoint_digit(0x00000033));  // 3
    EXPECT_TRUE(is_codepoint_digit(0x00000034));  // 4
    EXPECT_TRUE(is_codepoint_digit(0x00000035));  // 5
    EXPECT_TRUE(is_codepoint_digit(0x00000036));  // 6
    EXPECT_TRUE(is_codepoint_digit(0x00000037));  // 7
    EXPECT_TRUE(is_codepoint_digit(0x00000038));  // 8
    EXPECT_TRUE(is_codepoint_digit(0x00000039));  // 9
}

TEST(UnicodeTest, IsDigitUtf8) {
    EXPECT_TRUE(is_utf8_digit("0"));  // 0
    EXPECT_TRUE(is_utf8_digit("1"));  // 1
    EXPECT_TRUE(is_utf8_digit("2"));  // 2
    EXPECT_TRUE(is_utf8_digit("3"));  // 3
    EXPECT_TRUE(is_utf8_digit("4"));  // 4
    EXPECT_TRUE(is_utf8_digit("5"));  // 5
    EXPECT_TRUE(is_utf8_digit("6"));  // 6
    EXPECT_TRUE(is_utf8_digit("7"));  // 7
    EXPECT_TRUE(is_utf8_digit("8"));  // 8
    EXPECT_TRUE(is_utf8_digit("9"));  // 9
}

TEST(UnicodeTest, IsAlphaCodepoint) {
    EXPECT_TRUE(is_codepoint_alpha(0x00000041));  // A
    EXPECT_TRUE(is_codepoint_alpha(0x00000042));  // B
    EXPECT_TRUE(is_codepoint_alpha(0x00000043));  // C
    EXPECT_TRUE(is_codepoint_alpha(0x00000044));  // D
    EXPECT_TRUE(is_codepoint_alpha(0x00000045));  // E
    EXPECT_TRUE(is_codepoint_alpha(0x00000046));  // F
    EXPECT_TRUE(is_codepoint_alpha(0x00000047));  // G
    EXPECT_TRUE(is_codepoint_alpha(0x00000048));  // H
    EXPECT_TRUE(is_codepoint_alpha(0x00000049));  // I
    EXPECT_TRUE(is_codepoint_alpha(0x0000004A));  // J
    EXPECT_TRUE(is_codepoint_alpha(0x0000004B));  // K
    EXPECT_TRUE(is_codepoint_alpha(0x0000004C));  // L
    EXPECT_TRUE(is_codepoint_alpha(0x0000004D));  // M
    EXPECT_TRUE(is_codepoint_alpha(0x0000004E));  // N
    EXPECT_TRUE(is_codepoint_alpha(0x0000004F));  // O
    EXPECT_TRUE(is_codepoint_alpha(0x00000050));  // P
    EXPECT_TRUE(is_codepoint_alpha(0x00000051));  // Q
    EXPECT_TRUE(is_codepoint_alpha(0x00000052));  // R
    EXPECT_TRUE(is_codepoint_alpha(0x00000053));  // S
    EXPECT_TRUE(is_codepoint_alpha(0x00000054));  // T
    EXPECT_TRUE(is_codepoint_alpha(0x00000055));  // U
    EXPECT_TRUE(is_codepoint_alpha(0x00000056));  // V
    EXPECT_TRUE(is_codepoint_alpha(0x00000057));  // W
    EXPECT_TRUE(is_codepoint_alpha(0x00000058));  // X
    EXPECT_TRUE(is_codepoint_alpha(0x00000059));  // Y
    EXPECT_TRUE(is_codepoint_alpha(0x0000005A));  // Z

    // Small letters
    EXPECT_TRUE(is_codepoint_alpha(0x00000061));  // a
    EXPECT_TRUE(is_codepoint_alpha(0x00000062));  // b
    EXPECT_TRUE(is_codepoint_alpha(0x00000063));  // c
    EXPECT_TRUE(is_codepoint_alpha(0x00000064));  // d
    EXPECT_TRUE(is_codepoint_alpha(0x00000065));  // e
    EXPECT_TRUE(is_codepoint_alpha(0x00000066));  // f
    EXPECT_TRUE(is_codepoint_alpha(0x00000067));  // g
    EXPECT_TRUE(is_codepoint_alpha(0x00000068));  // h
    EXPECT_TRUE(is_codepoint_alpha(0x00000069));  // i
    EXPECT_TRUE(is_codepoint_alpha(0x0000006A));  // j
    EXPECT_TRUE(is_codepoint_alpha(0x0000006B));  // k
    EXPECT_TRUE(is_codepoint_alpha(0x0000006C));  // l
    EXPECT_TRUE(is_codepoint_alpha(0x0000006D));  // m
    EXPECT_TRUE(is_codepoint_alpha(0x0000006E));  // n
    EXPECT_TRUE(is_codepoint_alpha(0x0000006F));  // o
    EXPECT_TRUE(is_codepoint_alpha(0x00000070));  // p
    EXPECT_TRUE(is_codepoint_alpha(0x00000071));  // q
    EXPECT_TRUE(is_codepoint_alpha(0x00000072));  // r
    EXPECT_TRUE(is_codepoint_alpha(0x00000073));  // s
    EXPECT_TRUE(is_codepoint_alpha(0x00000074));  // t
    EXPECT_TRUE(is_codepoint_alpha(0x00000075));  // u
    EXPECT_TRUE(is_codepoint_alpha(0x00000076));  // v
    EXPECT_TRUE(is_codepoint_alpha(0x00000077));  // w
    EXPECT_TRUE(is_codepoint_alpha(0x00000078));  // x
    EXPECT_TRUE(is_codepoint_alpha(0x00000079));  // y
    EXPECT_TRUE(is_codepoint_alpha(0x0000007A));  // z
}

// utf8_string
TEST(UnicodeTest, Utf8StringInit) {
    const char* utf8_data = "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳";
    utf8_string* str      = utf8_new(utf8_data);
    EXPECT_STREQ(str->data, utf8_data);

    // test the count and length
    EXPECT_EQ(str->count, 13);
    EXPECT_EQ(str->length, 46);

    utf8_free(str);
}

TEST(UnicodeTest, Utf8StringIndex) {
    const char* utf8_data = "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳";
    utf8_string* str      = utf8_new(utf8_data);

    // test all 46 bytes
    for (size_t i = 0; i < str->length; i++) {
        EXPECT_EQ(utf8_index_of(str, &str->data[i]), i);
    }

    utf8_free(str);
}

// utf8_replace - replace a first occurrence of a substring
TEST(UnicodeTest, Utf8StringReplace) {
    const char* utf8_data = "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳";
    utf8_string* str      = utf8_new(utf8_data);

    utf8_replace(str, "🇺🇸", "🇺🇸🇺🇸");
    EXPECT_STREQ(str->data, "A¢€😀🇯🇵🇺🇸🇺🇸🇸🇦🇱🇷🇳");

    utf8_free(str);
}

// utf8_replace_all - replace all occurrences of a substring
TEST(UnicodeTest, Utf8StringReplaceAll) {
    const char* utf8_data = "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳";
    utf8_string* str      = utf8_new(utf8_data);

    utf8_replace_all(str, "🇺🇸", "🇺🇸🇺🇸");
    EXPECT_STREQ(str->data, "A¢€😀🇯🇵🇺🇸🇺🇸🇸🇦🇱🇷🇳");
    utf8_free(str);
}

TEST(UnicodeTest, Utf8StringCopy) {
    const char* utf8_data = "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳";
    char* copy            = utf8_copy(utf8_data);
    EXPECT_STREQ(copy, utf8_data);
    free(copy);
}

// utf8_writeto
TEST(UnicodeTest, Utf8StringWriteto) {
    const char* utf8_data = "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳";
    utf8_string* str      = utf8_new(utf8_data);
    ssize_t bytes         = utf8_writeto(str, "utf8_tests.txt");
    EXPECT_EQ(bytes, 46);
    utf8_free(str);

    str = utf8_readfrom("utf8_tests.txt");
    ASSERT_TRUE(str != nullptr);

    EXPECT_STREQ(str->data, utf8_data);

    // delete the file
    remove("utf8_tests.txt");
}

// string_ltrim removes leading whitespace from str.
// void utf8_ltrim(char* str);
TEST(UnicodeTest, Utf8StringLtrim) {
    char* str = utf8_copy("  A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳");
    utf8_ltrim(str);
    EXPECT_STREQ(str, "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳");
    free(str);
}

// utf8_rtrim removes trailing whitespace from str.
// void utf8_rtrim(char* str);
TEST(UnicodeTest, Utf8StringRtrim) {
    char* str = utf8_copy("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳  ");
    utf8_rtrim(str);
    EXPECT_STREQ(str, "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳");
    free(str);
}

// utf8_trim removes leading and trailing whitespace from str.
// void utf8_trim(char* str);
TEST(UnicodeTest, Utf8StringTrim) {
    char* str = utf8_copy("  A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳  ");
    utf8_trim(str);
    EXPECT_STREQ(str, "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳");
    free(str);
}

// utf8_trim_chars removes leading and trailing characters c from str.
// void utf8_trim_chars(char* str, const char* c);
TEST(UnicodeTest, Utf8StringTrimChars) {
    char* str = utf8_copy("  A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳  ");
    utf8_trim_chars(str, " ");
    EXPECT_STREQ(str, "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳");
    free(str);
}

// utf8_trim_char removes leading and trailing character c from str.
// void utf8_trim_char(char* str, char c);
TEST(UnicodeTest, Utf8StringTrimChar) {
    char* str = utf8_copy("  A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳  ");
    utf8_trim_char(str, ' ');
    EXPECT_STREQ(str, "A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳");
    free(str);
}

// bool utf8_starts_with(const char* str, const char* prefix);
TEST(UnicodeTest, Utf8StringStartsWith) {
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀🇯"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀🇯🇵"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀🇯🇵🇺"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀🇯🇵🇺🇸"));
    EXPECT_TRUE(utf8_starts_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀🇯🇵🇺🇸🇸"));
}

// bool utf8_ends_with(const char* str, const char* suffix);
TEST(UnicodeTest, Utf8StringEndsWith) {
    EXPECT_TRUE(utf8_ends_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "🇳"));
    EXPECT_TRUE(utf8_ends_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "🇷🇳"));
    EXPECT_TRUE(utf8_ends_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "🇱🇷🇳"));
    EXPECT_TRUE(utf8_ends_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "🇦🇱🇷🇳"));
    EXPECT_TRUE(utf8_ends_with("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "🇸🇦🇱🇷🇳"));
}

// bool utf8_contains(const char* str, const char* substr);
TEST(UnicodeTest, Utf8StringContains) {
    EXPECT_TRUE(utf8_contains("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A"));
    EXPECT_TRUE(utf8_contains("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "¢"));
    EXPECT_TRUE(utf8_contains("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "€"));
    EXPECT_TRUE(utf8_contains("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "😀"));
    EXPECT_TRUE(utf8_contains("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "🇯"));
}

// regex_match returns true if str matches the pattern, otherwise false.
// bool regex_match(const char* str, const char* pattern)
TEST(UnicodeTest, Utf8StringRegexMatch) {
    EXPECT_TRUE(regex_match("A", "A"));
    EXPECT_TRUE(regex_match("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A"));
    EXPECT_TRUE(regex_match("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢"));
    EXPECT_TRUE(regex_match("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€"));
    EXPECT_TRUE(regex_match("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀"));
    EXPECT_TRUE(regex_match("A¢€😀🇯🇵🇺🇸🇸🇦🇱🇷🇳", "A¢€😀🇯"));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}