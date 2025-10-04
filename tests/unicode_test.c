#include <assert.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/unicode.h"

// Define test result structure
typedef struct {
    const char* test_name;
    bool passed;
    char* message;
} TestResult;

// Array to store test results
#define MAX_TESTS 100
TestResult test_results[MAX_TESTS];
int test_count = 0;

// Function to record test results
void record_test(const char* test_name, bool passed, const char* message) {
    if (test_count < MAX_TESTS) {
        test_results[test_count].test_name = test_name;
        test_results[test_count].passed    = passed;
        test_results[test_count].message   = message ? strdup(message) : NULL;
        test_count++;
    }
}

// Helper function to compare bytes in strings
void print_bytes(const char* s, size_t len) {
    printf("Bytes: ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", (unsigned char)s[i]);
    }
    printf("\n");
}

// Test for utf8_to_codepoint and ucp_to_utf8
void test_codepoint_conversion() {
    printf("Testing codepoint conversion...\n");

    // Test ASCII
    char utf8_buffer[5];
    uint32_t codepoint = 'A';
    ucp_to_utf8(codepoint, utf8_buffer);
    uint32_t result = utf8_to_codepoint(utf8_buffer);
    record_test("ASCII codepoint conversion", result == codepoint, NULL);

    // Test 2-byte character
    codepoint = 0x00A9;  // Copyright symbol ©
    ucp_to_utf8(codepoint, utf8_buffer);
    result = utf8_to_codepoint(utf8_buffer);
    record_test("2-byte codepoint conversion", result == codepoint, NULL);

    // Test 3-byte character
    codepoint = 0x20AC;  // Euro symbol €
    ucp_to_utf8(codepoint, utf8_buffer);
    result = utf8_to_codepoint(utf8_buffer);
    record_test("3-byte codepoint conversion", result == codepoint, NULL);

    // Test 4-byte character (emoji)
    codepoint = 0x1F600;  // Grinning face emoji 😀
    ucp_to_utf8(codepoint, utf8_buffer);
    result = utf8_to_codepoint(utf8_buffer);
    record_test("4-byte codepoint conversion", result == codepoint, NULL);

    // Test invalid UTF-8 sequence
    const char invalid_utf8[] = {(char)0xC0, (char)0xAF, 0x00};  // Invalid 2-byte sequence
    result                    = utf8_to_codepoint((const char*)invalid_utf8);
    record_test("Invalid UTF-8 sequence", result == 0xFFFD,
                NULL);  // Should return replacement character
}

// Test for utf8_byte_length and utf8_count_codepoints
void test_length_functions() {
    printf("Testing length functions...\n");

    // Test ASCII string
    const char* ascii = "Hello, world!";
    size_t byte_len   = utf8_valid_byte_count(ascii);
    size_t cp_count   = utf8_count_codepoints(ascii);
    record_test("ASCII byte length", byte_len == strlen(ascii), NULL);
    record_test("ASCII codepoint count", cp_count == strlen(ascii), NULL);

    // Test mixed string
    const char* mixed = "Hello, 世界!";  // "Hello, world!" with Chinese characters
    byte_len          = utf8_valid_byte_count(mixed);
    cp_count          = utf8_count_codepoints(mixed);
    record_test("Mixed string byte length", byte_len == strlen(mixed), NULL);
    record_test("Mixed string codepoint count", cp_count == 10,
                NULL);  // 8 ASCII + 2 Chinese chars

    // Test emoji string
    const char* emoji = "😀👍🌍";  // Three emoji
    byte_len          = utf8_valid_byte_count(emoji);
    cp_count          = utf8_count_codepoints(emoji);
    record_test("Emoji byte length", byte_len == strlen(emoji), NULL);
    record_test("Emoji codepoint count", cp_count == 3, NULL);

    // Test empty string
    const char* empty = "";
    byte_len          = utf8_valid_byte_count(empty);
    cp_count          = utf8_count_codepoints(empty);
    record_test("Empty string byte length", byte_len == 0, NULL);
    record_test("Empty string codepoint count", cp_count == 0, NULL);

    // Test with invalid UTF-8
    const char invalid[] = {(char)0xC0, (char)0xAF, 'A', 0x00};  // Invalid 2-byte sequence + ASCII
    byte_len             = utf8_valid_byte_count(invalid);
    cp_count             = utf8_count_codepoints(invalid);
    record_test("Invalid UTF-8 byte length handling", byte_len == 3, NULL);
    record_test("Invalid UTF-8 codepoint count handling", cp_count > 0, NULL);
}

// Test for utf8_char_length
void test_char_length() {
    printf("Testing character length function...\n");

    // ASCII character (1 byte)
    const char* ascii = "A";
    size_t len        = utf8_char_length(ascii);
    record_test("ASCII char length", len == 1, NULL);

    // 2-byte character
    const char two_byte[] = {(char)0xC2, (char)0xA9, 0x00};  // Copyright symbol ©
    len                   = utf8_char_length(two_byte);
    record_test("2-byte char length", len == 2, NULL);

    // 3-byte character
    const char three_byte[] = {(char)0xE2, (char)0x82, (char)0xAC, 0x00};  // Euro symbol €
    len                     = utf8_char_length(three_byte);
    record_test("3-byte char length", len == 3, NULL);

    // 4-byte character (emoji)
    const char four_byte[] = {(char)0xF0, (char)0x9F, (char)0x98, (char)0x80,
                              0x00};  // Grinning face emoji 😀
    len                    = utf8_char_length(four_byte);
    record_test("4-byte char length", len == 4, NULL);

    // Invalid byte
    const char invalid[] = {(char)0xFF, 0x00};  // Invalid UTF-8 lead byte
    len                  = utf8_char_length(invalid);
    record_test("Invalid UTF-8 lead byte", len == 0, NULL);
}

// Test validation functions
void test_validation() {
    printf("Testing validation functions...\n");

    // Valid codepoints
    record_test("ASCII codepoint validity", is_valid_codepoint('A'), NULL);
    record_test("BMP codepoint validity", is_valid_codepoint(0x20AC),
                NULL);  // Euro symbol
    record_test("SMP codepoint validity", is_valid_codepoint(0x1F600),
                NULL);  // Emoji
    record_test("Max valid codepoint", is_valid_codepoint(0x10FFFF), NULL);

    // Invalid codepoints
    record_test("Beyond max codepoint", !is_valid_codepoint(0x110000), NULL);

    // Valid UTF-8 strings
    record_test("ASCII string validity", is_valid_utf8("Hello"), NULL);
    record_test("Mixed string validity", is_valid_utf8("Hello, 世界!"), NULL);
    record_test("Emoji string validity", is_valid_utf8("😀👍🌍"), NULL);

    // Invalid UTF-8 strings
    const char invalid1[] = {(char)0xC0, (char)0xAF, 0x00};              // Invalid 2-byte sequence
    const char invalid2[] = {(char)0xE0, (char)0x80, (char)0xAF, 0x00};  // Overlong encoding
    const char invalid3[] = {(char)0xED, (char)0xA0, (char)0x80, 0x00};  // UTF-16 surrogate
    const char invalid4[] = {(char)0xF4, (char)0x90, (char)0x80, (char)0x80,
                             0x00};  // Beyond Unicode range

    record_test("Invalid 2-byte sequence", !is_valid_utf8(invalid1), NULL);
    record_test("Overlong encoding", !is_valid_utf8(invalid2), NULL);
    record_test("UTF-16 surrogate", !is_valid_utf8(invalid3), NULL);
    record_test("Beyond Unicode range", !is_valid_utf8(invalid4), NULL);
}

// Test character classification functions
void test_char_classification() {
    printf("Testing character classification...\n");

    // Set locale to support wide characters
    setlocale(LC_ALL, "en_US.UTF-8");

    // Test whitespace
    record_test("Space is whitespace", is_codepoint_whitespace(' '), NULL);
    record_test("Tab is whitespace", is_codepoint_whitespace('\t'), NULL);
    record_test("A is not whitespace", !is_codepoint_whitespace('A'), NULL);

    // Test UTF-8 whitespace
    record_test("UTF-8 space is whitespace", is_utf8_whitespace(" "), NULL);
    record_test("UTF-8 tab is whitespace", is_utf8_whitespace("\t"), NULL);

    // Test digits
    record_test("0 is digit", is_codepoint_digit('0'), NULL);
    record_test("9 is digit", is_codepoint_digit('9'), NULL);
    record_test("A is not digit", !is_codepoint_digit('A'), NULL);

    // Test UTF-8 digits
    record_test("UTF-8 0 is digit", is_utf8_digit("0"), NULL);
    record_test("UTF-8 9 is digit", is_utf8_digit("9"), NULL);

    // Test alpha
    record_test("A is alpha", is_codepoint_alpha('A'), NULL);
    record_test("z is alpha", is_codepoint_alpha('z'), NULL);
    record_test("0 is not alpha", !is_codepoint_alpha('0'), NULL);

    // Test UTF-8 alpha
    record_test("UTF-8 A is alpha", is_utf8_alpha("A"), NULL);
    record_test("UTF-8 z is alpha", is_utf8_alpha("z"), NULL);

    // Test non-ASCII characters
    const char euro[] = {(char)0xE2, (char)0x82, (char)0xAC, 0x00};  // Euro symbol €
    record_test("Euro symbol is not digit", !is_utf8_digit(euro), NULL);
    record_test("Euro symbol is not alpha", !is_utf8_alpha(euro), NULL);

    // Test some international alphabetic characters
    const char alpha_char[] = {(char)0xC3, (char)0xA9, 0x00};  // é
    record_test("é is alpha", is_utf8_alpha(alpha_char), NULL);
}

// Test basic string operations
void test_basic_operations() {
    printf("Testing basic string operations...\n");

    // Test string creation
    utf8_string* s = utf8_new("Hello, 世界!");
    record_test("String creation", s != NULL, NULL);
    record_test("String data", strcmp(s->data, "Hello, 世界!") == 0, NULL);
    record_test("String length", s->length == strlen("Hello, 世界!"), NULL);
    record_test("String codepoint count", s->count == 10, NULL);

    // Test copy function
    char* copy = utf8_copy("Test");
    record_test("String copy", strcmp(copy, "Test") == 0, NULL);
    free(copy);

    // Test append
    utf8_append(s, " 🌍");
    record_test("String append", strstr(s->data, "🌍") != NULL, NULL);
    record_test("Appended length", s->length == strlen("Hello, 世界! 🌍"), NULL);

    // Test substring
    char* substr = utf8_substr(s, 0, 5);
    record_test("Substring", strcmp(substr, "Hello") == 0, NULL);
    free(substr);

    // Test replace
    utf8_replace(s, "Hello", "Hi");
    record_test("Replace", strstr(s->data, "Hi") == s->data, NULL);
    record_test("Replace adjusts length", s->length == strlen("Hi, 世界! 🌍"), NULL);

    // Test reverse
    utf8_string* rev = utf8_new("ABC");
    utf8_reverse(rev);
    record_test("Reverse ASCII", strcmp(rev->data, "CBA") == 0, NULL);
    utf8_free(rev);

    // Test UTF-8 reverse (with multibyte chars)
    rev = utf8_new("A世B");
    utf8_reverse(rev);
    size_t len = utf8_valid_byte_count(rev->data);
    // Check if reversed string has the same length
    record_test("Reverse UTF-8 length", len == utf8_valid_byte_count("B世A"), NULL);

    // Test case conversion
    utf8_string* case_test = utf8_new("Hello");
    utf8_tolower(case_test->data);
    record_test("To lowercase", strcmp(case_test->data, "hello") == 0, NULL);
    utf8_toupper(case_test->data);
    record_test("To uppercase", strcmp(case_test->data, "HELLO") == 0, NULL);
    utf8_free(case_test);

    // Cleanup
    utf8_free(s);
    utf8_free(rev);
}

// Test trimming functions
void test_trim_functions() {
    printf("Testing trim functions...\n");

    // Test ltrim
    char str1[] = "  Hello";
    utf8_ltrim(str1);
    record_test("Left trim spaces", strcmp(str1, "Hello") == 0, NULL);

    // Test rtrim
    char str2[] = "Hello  ";
    utf8_rtrim(str2);
    record_test("Right trim spaces", strcmp(str2, "Hello") == 0, NULL);

    // Test trim
    char str3[] = "  Hello  ";
    utf8_trim(str3);
    record_test("Trim spaces both sides", strcmp(str3, "Hello") == 0, NULL);

    // Test trim specific char
    char str4[] = "---Hello---";
    utf8_trim_char(str4, '-');
    record_test("Trim specific char", strcmp(str4, "Hello") == 0, NULL);

    // Test trim multiple chars
    char str5[] = "abc123abc";
    utf8_trim_chars(str5, "abc");
    record_test("Trim multiple chars", strcmp(str5, "123") == 0, NULL);

    // Test UTF-8 trim
    char str6[] = "　Hello　";  // Full-width spaces
    utf8_trim(str6);
    printf("Got 6: \"%s\"\n", str6);
    record_test("Trim UTF-8 whitespace", strcmp(str6, "Hello") == 0, NULL);

    // Test UTF-8 specific char trim
    char str7[] = "世Hello世";
    utf8_trim_chars(str7, "世");
    printf("Got 7: \"%s\"\n", str7);
    record_test("Trim UTF-8 specific chars", strcmp(str7, "Hello") == 0, NULL);
}

// Test split function
void test_split() {
    printf("Testing split function...\n");

    utf8_string* s      = utf8_new("apple,banana,cherry");
    size_t num_parts    = 0;
    utf8_string** parts = utf8_split(s, ",", &num_parts);

    record_test("Split count", num_parts == 3, NULL);
    record_test("Split part 1", strcmp(parts[0]->data, "apple") == 0, NULL);
    record_test("Split part 2", strcmp(parts[1]->data, "banana") == 0, NULL);
    record_test("Split part 3", strcmp(parts[2]->data, "cherry") == 0, NULL);

    // Test empty parts
    utf8_free(s);
    s = utf8_new("a,,b");
    utf8_split_free(parts, num_parts);
    parts = utf8_split(s, ",", &num_parts);

    record_test("Split with empty parts count", num_parts == 3, NULL);
    record_test("Split empty part", strcmp(parts[1]->data, "") == 0, NULL);

    // Test UTF-8 delimiter
    utf8_free(s);
    s = utf8_new("a世b世c");
    utf8_split_free(parts, num_parts);
    parts = utf8_split(s, "世", &num_parts);

    record_test("Split with UTF-8 delimiter count", num_parts == 3, NULL);
    record_test("Split with UTF-8 delimiter part 1", strcmp(parts[0]->data, "a") == 0, NULL);
    record_test("Split with UTF-8 delimiter part 2", strcmp(parts[1]->data, "b") == 0, NULL);
    record_test("Split with UTF-8 delimiter part 3", strcmp(parts[2]->data, "c") == 0, NULL);

    // Cleanup
    utf8_split_free(parts, num_parts);
    utf8_free(s);
}

// Test string check functions
void test_string_checks() {
    printf("Testing string check functions...\n");

    // Test starts_with
    record_test("Starts with (true)", utf8_starts_with("Hello world", "Hello"), NULL);
    record_test("Starts with (false)", !utf8_starts_with("Hello world", "world"), NULL);

    // Test ends_with
    record_test("Ends with (true)", utf8_ends_with("Hello world", "world"), NULL);
    record_test("Ends with (false)", !utf8_ends_with("Hello world", "Hello"), NULL);

    // Test contains
    record_test("Contains (true)", utf8_contains("Hello world", "lo wo"), NULL);
    record_test("Contains (false)", !utf8_contains("Hello world", "universe"), NULL);

    // Test with UTF-8
    record_test("UTF-8 starts with", utf8_starts_with("世界Hello", "世界"), NULL);
    record_test("UTF-8 ends with", utf8_ends_with("Hello世界", "世界"), NULL);
    record_test("UTF-8 contains", utf8_contains("Hello世界Goodbye", "世界"), NULL);
}

// Test file I/O operations
void test_file_operations() {
    printf("Testing file operations...\n");

    // Test write to file
    utf8_string* s = utf8_new("Hello, 世界! This is a 😀 test.");
    long bytes     = utf8_writeto(s, "test_utf8.txt");
    record_test("Write to file", bytes > 0, NULL);

    // Test read from file
    utf8_string* s2 = utf8_readfrom("test_utf8.txt");
    record_test("Read from file", s2 != NULL, NULL);
    record_test("File content correct", strcmp(s->data, s2->data) == 0, NULL);

    // Cleanup
    utf8_free(s);
    utf8_free(s2);
    remove("test_utf8.txt");
}

int main() {
    // Set locale to support wide characters
    setlocale(LC_ALL, "en_US.UTF-8");

    printf("=== UTF-8 String Library Test Suite ===\n\n");

    // Run all tests
    test_codepoint_conversion();
    test_length_functions();
    test_char_length();
    test_validation();
    test_char_classification();
    test_basic_operations();
    test_trim_functions();
    test_split();
    test_string_checks();
    test_file_operations();

    // Print test results
    printf("\n=== Test Results ===\n");
    int passed = 0;
    int failed = 0;

    for (int i = 0; i < test_count; i++) {
        if (test_results[i].passed) {
            printf("[✓] %s\n", test_results[i].test_name);
            passed++;
        } else {
            printf("[✗] %s", test_results[i].test_name);
            if (test_results[i].message) {
                printf(" - %s", test_results[i].message);
            }
            printf("\n");
            failed++;
        }

        if (test_results[i].message) {
            free(test_results[i].message);
        }
    }

    printf("\nSummary: %d passed, %d failed, %d total\n", passed, failed, test_count);

    return failed > 0 ? 1 : 0;
}
