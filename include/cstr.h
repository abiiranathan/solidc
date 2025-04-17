#ifndef CSTR_H
#define CSTR_H

#include <stdbool.h>
#include <stddef.h>
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(STR_MIN_CAPACITY)
#define STR_MIN_CAPACITY 16
#endif

#if !defined(STR_RESIZE_FACTOR)
#define STR_RESIZE_FACTOR 1.5
#endif

STATIC_CHECK_POWER_OF_2(STR_MIN_CAPACITY);

#define STR_NPOS -1

// A dynamically resizable c-string.
// All strings MUST be NULL-terminated.
typedef struct cstr cstr;

// Lightweight string view that points to a const char * data
// with pre-computed length. Avoid overhead of strlen.
typedef struct {
    const char* data;  // Pointer to string data
    size_t length;     // Length of the view
} str_view;

// ========== Creation and destruction ==========

// Create a new empty string with the given capacity.
// If capacity is < STR_MIN_CAPACITY, the capacity is ignored.
// To create a str directly from a char *, use str_from.
__attribute__((warn_unused_result)) cstr* str_new(size_t capacity);

// Create a new string from a C string.
__attribute__((warn_unused_result)) cstr* str_from(const char* cstr);

// Allocate and create a formatted string.
__attribute__((format(printf, 1, 2))) cstr* str_format(const char* format, ...);

// Free the memory used by a string.
void str_free(cstr* s);

// ========== Information ==========

// Get the length of the string.
size_t str_len(const cstr* s);

// Get the capacity of the string.
size_t str_capacity(const cstr* s);

// Check if the string is empty.
bool str_empty(const cstr* s);

// Ensure that the string has at least the given capacity.
bool str_resize(cstr** s, size_t capacity);

// ============ Modification ============

// Append a C string to the end of the string.
bool str_append(cstr** s, const char* append);

// Append a formatted string to the end of the string.
bool str_append_fmt(cstr** s, const char* format, ...);

// Append a character to the end of the string.
bool str_append_char(cstr** s, char c);

// Prepend a C string to the beginning of the string.
bool str_prepend(cstr** s, const char* prepend);

// Insert a C string at the given index in the string.
bool str_insert(cstr** s, size_t index, const char* insert);

// Remove a substring from the string at the given index.
// The count parameter specifies the number of characters to remove.
bool str_remove(cstr** s, size_t index, size_t count);

// Remove all occurrences of a substring from the string.
// Returns the number of occurrences removed.
size_t str_remove_all(cstr** s, const char* substr);

// Clear the contents of the string.
void str_clear(cstr* s);

// ============ Access ============

// Get the character at the given index in the string.
// Retuns NULL terminator ('\0') if not in string.
char str_at(const cstr* s, size_t index);

// Get a pointer to the internal data of the string.
char* str_data(cstr* s);

// Get a string view of the str with data and length populated.
// You must not modify this directly.
str_view str_as_view(const cstr* s);

// =========== Comparison and search ==================

// Compare two strings lexicographically.
int str_compare(const cstr* s1, const cstr* s2);

// Check if two strings are equal.
bool str_equals(const cstr* s1, const cstr* s2);

// Check if the string starts with the given prefix.
bool str_starts_with(const cstr* s, const char* prefix);

// Check if the string ends with the given suffix.
bool str_ends_with(const cstr* s, const char* suffix);

// Find the first occurrence of a substring in the string.
// Returns the index of the first character of the substring or STR_NPOS (-1) if
// not found.
int str_find(const cstr* s, const char* substr);

// Find the last occurrence of a substring in the string.
int str_rfind(const cstr* s, const char* substr);

// ============== Transformation ====================

// Convert the string to lowercase.
void str_to_lower(cstr* s);

// Convert the string to UPPERCASE.
void str_to_upper(cstr* s);

// Convert the string to snake_case.
void str_snake_case(cstr* s);

// convert the string to title case.
void str_title_case(cstr* str);

// Convert the string to camelCase.
void str_camel_case(cstr* s);

// Convert the string to PascalCase.
void str_pascal_case(cstr* s);

// Trim leading and trailing whitespace characters from the string.
void str_trim(cstr* s);

// Remove trailing whitespace characters from the string.
void str_rtrim(cstr* s);

// Remove leading whitespace characters from the string.
void str_ltrim(cstr* s);

// Trim chars from str.
void str_trim_chars(cstr* str, const char* chars);

// Count the number of occurrences of a substring within the string.
size_t str_count_substr(const cstr* str, const char* substr);

// Remove characters in str from start index, up to start + length.
void str_remove_substr(cstr* str, size_t start, size_t substr_length);

// Remove all occurrences of character c from str.
void str_remove_char(cstr* str, char c);

// ============== Substrings and replacements ==============

// Get a substring of the string starting at the given index.
__attribute__((warn_unused_result)) cstr* str_substr(const cstr* s, size_t start, size_t length);

// Replace the first occurrence of a substring in the string.
__attribute__((warn_unused_result)) cstr* str_replace(const cstr* s, const char* old, const char* new_str);

// Replace all occurrences of a substring in the string.
__attribute__((warn_unused_result)) cstr* str_replace_all(const cstr* s, const char* old, const char* new_str);

// ========== Splitting and joining ===========

// Split the string into substrings based on a delimiter.
__attribute__((warn_unused_result)) cstr** str_split(const cstr* s, const char* delim, size_t* count);

// Join an array of strings into a single string using a delimiter.
__attribute__((warn_unused_result)) cstr* str_join(const cstr** strings, size_t count, const char* delim);

// Reverse the string, returning a new string.
__attribute__((warn_unused_result)) cstr* str_reverse(const cstr* s);

// Reverse the string in place.
void str_reverse_in_place(cstr* s);

#ifdef __cplusplus
}
#endif

#endif  // CSTR_H
