#ifndef D8710AAF_01A7_49B2_A3EC_1299EE6E9702
#define D8710AAF_01A7_49B2_A3EC_1299EE6E9702

/**
Header-only library for Unicode handling in C.
Supports UTF-8 encoding and Unicode version 1.0.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define UNICODE_VERSION 0x0100          // 1.0
#define UNICODE_MAX_CODEPOINT 0x10FFFF  // 1,114,111
#define UNICODE_MAX_UTF8_BYTES 4        // 4 bytes

typedef struct utf8_string utf8_string;  // Forward declaration for utf8_string

void ucp_to_utf8(uint32_t codepoint, char* utf8);
uint32_t utf8_to_codepoint(const char* utf8);
size_t utf8_count_codepoints(const char* utf8);
size_t utf8_byte_length(const char* s);
size_t utf8_char_length(const char* str);

bool is_valid_codepoint(uint32_t codepoint);
bool is_valid_utf8(const char* utf8);
bool is_codepoint_whitespace(uint32_t codepoint);
bool is_utf8_whitespace(const char* utf8);
bool is_codepoint_digit(uint32_t codepoint);
bool is_utf8_digit(const char* utf8);
bool is_codepoint_alpha(uint32_t codepoint);
bool is_utf8_alpha(const char* utf8);

// String manipulation functions
utf8_string* utf8_new(const char* data);
void utf8_free(utf8_string* s);
char* utf8_copy(const char* data);
void utf8_print(const utf8_string* s);
const char* utf8_data(const utf8_string* s);
void utf8_print_info(const utf8_string* s);
void utf8_print_codepoints(const utf8_string* s);
int utf8_index_of(const utf8_string* s, const char* utf8);
void utf8_append(utf8_string* s, const char* data);
char* utf8_substr(const utf8_string* s, size_t index, size_t utf8_byte_len);
void utf8_insert(utf8_string* s, size_t index, const char* data);
void utf8_remove(utf8_string* s, size_t index, size_t count);
void utf8_replace(utf8_string* s, const char* old_str, const char* new_str);
void utf8_replace_all(utf8_string* s, const char* old_str, const char* new_str);
void utf8_reverse(utf8_string* s);
ssize_t utf8_writeto(const utf8_string* s, const char* filename);
utf8_string* utf8_readfrom(const char* filename);

// Split a string into parts using a delimiter.
// Returns an array of utf8_string pointers that need to be freed with utf8_split_free.
// The last element of the array is NULL.
utf8_string** utf8_split(const utf8_string* str, const char* delim, size_t* num_parts);
void utf8_split_free(utf8_string** str, size_t size);

// string_ltrim removes leading whitespace from str.
void utf8_ltrim(char* str);

// utf8_rtrim removes trailing whitespace from str.
void utf8_rtrim(char* str);

// utf8_trim removes leading and trailing whitespace from str.
void utf8_trim(char* str);

// utf8_trim_chars removes leading and trailing characters c from str.
void utf8_trim_chars(char* str, const char* c);

// utf8_trim_char removes leading and trailing character c from str.
void utf8_trim_char(char* str, char c);

void utf8_tolower(char* str);
void utf8_toupper(char* str);
void utf8_array_remove(utf8_string** array, size_t size, size_t index);

bool utf8_starts_with(const char* str, const char* prefix);
bool utf8_ends_with(const char* str, const char* suffix);
bool utf8_contains(const char* str, const char* substr);
// regex_match returns true if str matches the pattern, otherwise false.
bool regex_match(const char* str, const char* pattern);

#ifdef __cplusplus
}
#endif

#endif /* D8710AAF_01A7_49B2_A3EC_1299EE6E9702 */
