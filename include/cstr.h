#ifndef CSTR_H
#define CSTR_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L  // for strtok_r
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // for strdup
#endif

#ifndef CSTR_MIN_CAPACITY
#define CSTR_MIN_CAPACITY 16
#else
#if CSTR_MIN_CAPACITY < 1
#error "CSTR_MIN_CAPACITY must be at least 1"
#endif
#endif

#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

#ifndef CSTR_MIN_CAPACITY
#define CSTR_MIN_CAPACITY 16
#endif

// Define a structure for the cstr object that uses Arena allocators
// for fast string operations.
typedef struct cstr {
    char* data;       // Pointer to the string data
    size_t length;    // Length of the string
    size_t capacity;  // Capacity of the string
} cstr;

/**
 * @brief Creates a new empty cstr with a initial capacity.
 * If the initial capacity is 0, it will be set to CSTR_MIN_CAPACITY.
 *
 * @return A pointer to the newly created cstr, or NULL if allocation fails.
 */
cstr* cstr_new(Arena* arena, size_t initial_capacity) __attribute__((warn_unused_result()));

/**
 * @brief Creates a new cstr from a C string using the default allocator.
 *
 * @param s The C string to create the cstr from.
 * @return A pointer to the newly created cstr, or NULL if allocation fails.
 */
cstr* cstr_from(Arena* arena, const char* s) __attribute__((warn_unused_result()));

/**
 * @brief Ensures that the cstr has enough capacity.
 *
 * @param str A pointer to the cstr to check and resize if necessary.
 * @param capacity The desired minimum capacity in bytes.
 * @return true if the cstr now has enough capacity, false if reallocation
 * fails.
 */
bool cstr_ensure_capacity(Arena* arena, cstr* str, size_t capacity);

/**
 * @brief Appends a character to the end of a cstr.
 *
 * @param str A pointer to the cstr to append to.
 * @param c The character to append.
 * @return true if the character was appended successfully, false if
 * allocation fails.
 */
bool cstr_append_char(Arena* arena, cstr* str, char c);

/**
 * @brief Appends a string to the end of a cstr.
 *
 * @param str A pointer to the cstr to append to.
 * @param s The string to append.
 * @return true if the string was appended successfully, false if allocation
 * fails.
 */
bool cstr_append(Arena* arena, cstr* str, const char* s);

/**
 * @brief Gets the length of a cstr.
 *
 * @param str A pointer to the cstr.
 * @return The length of the cstr in bytes.
 */
size_t cstr_len(const cstr* str);

/**
 * @brief Gets the capacity of a cstr.
 *
 * @param str A pointer to the cstr.
 * @return The capacity of the cstr in bytes.
 */
size_t cstr_capacity(const cstr* str);

/**
 * @brief Appends a formatted string to the cstr.
 *
 * @param str A pointer to the cstr to append to.
 * @param fmt The format string for the formatted output.
 * @param ... Additional arguments to the formatted output.
 * @return true if the formatted string was appended successfully, false if
 * allocation fails.
 */
__attribute__((format(printf, 3, 4))) bool cstr_append_fmt(Arena* arena, cstr* str, const char* fmt, ...);

/**
 * @brief Shrinks the cstr to remove excess capacity.
 *
 * @param str A pointer to the cstr to shrink.
 * @return true if the cstr was successfully shrunk, false if reallocation
 * fails.
 */
bool cstr_clip(Arena* arena, cstr* str);

/**
 * @brief Gets the C string representation of a cstr.
 *
 * @param str A pointer to the cstr.
 * @return A pointer to the C string data within the cstr.
 */
char* cstr_data(const cstr* str);

/**
 * @brief Compares two cstrs.
 *
 * @param str1 A pointer to the first cstr.
 * @param str2 A pointer to the second cstr.
 * @return The result of comparing the two cstrs using strcmp.
 */
int cstr_compare(const cstr* str1, const cstr* str2);

// Returns true if the two cstrs are equal, false otherwise.
bool cstr_equals(const cstr* str1, const cstr* str2);

/**
 * @brief Splits a cstr into an array of substrings based on a delimiter.
 *
 * @param str A pointer to the cstr to split.
 * @param delimiter The delimiter character to split on.
 * @param count A pointer to a size_t variable that will store the number of
 * substrings found.
 * @return A pointer to an array of cstr pointers, or NULL if allocation
 * fails.
 */
cstr** cstr_split(Arena* arena, const cstr* str, char delimiter, size_t* count) __attribute__((warn_unused_result()));

/**
 * @brief Splits a char* into an array of substrings based on a delimiter.
 *
 * @param str A pointer to the char* to split.
 * @param delimiter The delimiter character to split on.
 * @param count A pointer to a size_t variable that will store the number of
 * substrings found.
 * @return A pointer to an array of char* pointers, or NULL if allocation
 * fails.
 */
char** cstr_splitchar(const char* str, char delimiter, size_t* count) __attribute__((warn_unused_result()));

// Free an array of char* pointers, like the one returned by cstr_split2.
void cstr_free_array(char** substrings, size_t count);

/**
 * @brief Splits a cstr into an array of substrings based on a delimiter.
 *
 * @param str A pointer to the cstr to split.
 * @param delimiter The delimiter string to split on.
 * @param initial_capacity The initial capacity of the array of cstr
 * pointers.
 * @param count A pointer to a size_t variable that will store the number of
 * substrings found.
 * @return A pointer to an array of cstr pointers, or NULL if allocation
 * fails.
 */
cstr** cstr_split_at(Arena* arena, const cstr* str, const char* delimiter, size_t initial_capacity, size_t* count)
    __attribute__((warn_unused_result()));

/**
 * @brief Joins an array of cstrs into a single cstr with a given separator.
 *
 * @param strs An array of cstr pointers.
 * @param count The number of cstrs in the array.
 * @param separator The separator string to use between the cstrs.
 * @return A pointer to the newly created joined cstr, or NULL if allocation
 * fails.
 */
cstr* cstr_join(Arena* arena, cstr** strs, size_t count, const char* separator) __attribute__((warn_unused_result()));

/**
 * @brief Gets a substring from a cstr.
 *
 * @param str A pointer to the cstr to extract a substring from.
 * @param start The starting index of the substring.
 * @param length The length of the substring.
 * @return A pointer to the newly created substring cstr, or NULL if the
 * input is invalid or allocation fails.
 */
cstr* cstr_substr(Arena* arena, const cstr* str, size_t start, size_t length) __attribute__((warn_unused_result()));

/**
 * @brief Checks if a cstr starts with a given prefix.
 *
 * @param str A pointer to the cstr to check.
 * @param prefix The prefix string to check for.
 * @return true if the cstr starts with the given prefix, false otherwise.
 */
bool cstr_starts_with(const cstr* str, const char* prefix);

/**
 * @brief Checks if a cstr ends with a given suffix.
 *
 * @param str A pointer to the cstr to check.
 * @param suffix The suffix string to check for.
 * @return true if the cstr ends with the given suffix, false otherwise.
 */
bool cstr_ends_with(const cstr* str, const char* suffix);

// /**
//  * @brief Matches a cstr against a regular expression pattern.
//  *
//  * @param str A pointer to the cstr to match.
//  * @param pattern The regular expression pattern to match against.
//  * @return true if the cstr matches the pattern, false otherwise.
//  */
// bool cstr_regex_match(const cstr* str, const char* pattern);

/**
 * @brief Prepends a string to a cstr.
 *
 * @param str A pointer to the cstr to prepend to.
 * @param src The string to prepend.
 * @return true if the string was prepended successfully, false if allocation
 * fails.
 */
bool cstr_prepend(Arena* arena, cstr* str, const char* src);

/**
 * @brief Converts a cstr to snake case.
 *
 * @param str A pointer to the cstr to convert.
 * @return true if the conversion was successful, false if allocation fails.
 */
bool cstr_snakecase(Arena* arena, cstr* str);

/**
 * @brief Converts a cstr to title case.
 *
 * @param str A pointer to the cstr to convert.
 */
void cstr_titlecase(cstr* str);

/**
 * @brief Converts a cstr to camel case.
 *
 * @param str A pointer to the cstr to convert.
 * @return true if the conversion was successful, false if allocation fails.
 */
bool cstr_camelcase(cstr* str);

/**
 * @brief Converts a cstr to pascal case.
 *
 * @param str A pointer to the cstr to convert.
 * @return true if the conversion was successful, false if allocation fails.
 */
bool cstr_pascalcase(cstr* str);

/**
 * @brief Replaces the first occurrence of a substring within a cstr with
 * another string.
 *
 * @param str A pointer to the cstr to perform the replacement on.
 * @param old The substring to replace.
 * @param with The string to replace the substring with.
 * @return A pointer to the newly created cstr with the replacement
 * performed, or NULL if allocation fails or no replacement occurred.
 */
cstr* cstr_replace(Arena* arena, const cstr* str, const char* old, const char* with)
    __attribute__((warn_unused_result()));

/**
 * @brief Replaces all occurrences of a substring within a cstr with another
 * string.
 *
 * @param str A pointer to the cstr to perform the replacement on.
 * @param old The substring to replace.
 * @param with The string to replace the substring with.
 * @return A pointer to the newly created cstr with all replacements
 * performed, or NULL if allocation fails.
 */
cstr* cstr_replace_all(Arena* arena, const cstr* str, const char* old, const char* with)
    __attribute__((warn_unused_result()));

/**
 * @brief Removes leading whitespace characters from a cstr.
 *
 * @param str A pointer to the cstr to trim.
 */
void cstr_ltrim(cstr* str);

/**
 * @brief Removes trailing whitespace characters from a cstr.
 *
 * @param str A pointer to the cstr to trim.
 */
void cstr_rtrim(cstr* str);

/**
 * @brief Removes leading and trailing whitespace characters from a cstr.
 *
 * @param str A pointer to the cstr to trim.
 */
void cstr_trim(cstr* str);

/**
 * @brief Removes leading and trailing characters from a cstr that match any
 * character in the given set.
 *
 * @param str A pointer to the cstr to trim.
 * @param chars The set of characters to trim.
 */
void cstr_trim_chars(cstr* str, const char* chars);

/**
 * @brief Counts the number of occurrences of a substring within a cstr.
 *
 * @param str A pointer to the cstr to search.
 * @param substr The substring to count occurrences of.
 * @return The number of occurrences of the substring within the cstr.
 */
size_t cstr_count_substr(const cstr* str, const char* substr);

/**
 * @brief Removes all occurrences of a character from a cstr.
 *
 * @param str A pointer to the cstr.
 * @param c The character to remove.
 */
void cstr_remove_char(char* str, char c);

/**
 * @brief Removes a substring from a cstr.
 *
 * @param str A pointer to the cstr.
 * @param start The starting index of the substring to remove.
 * @param substr_length The length of the substring to remove.
 */
void cstr_remove_substr(cstr* str, size_t start, size_t substr_length);

/**
 * @brief Reverses a cstr in place.
 *
 * @param str A pointer to the cstr.
 */
void cstr_reverse(cstr* str);

#ifdef __cplusplus
}
#endif

#endif  // CSTR_H
