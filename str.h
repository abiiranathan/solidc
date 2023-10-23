#ifndef STR_H
#define STR_H

#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Opaque object that performs common string operations.
typedef struct Str Str;

/**
 * Allocate a new null-terminated string on the heap.
 *
 * @param str The input C string.
 * @return A new Str object, or NULL if memory allocation fails or str is NULL.
 */
Str* str_new(const char* str);

/**
 * Frees the memory allocated for a Str object.
 *
 * @param string The Str object to free.
 */
void str_free(Str* string);

/**
 * Compares two Str objects lexicographically.
 *
 * @param str1 The first Str object.
 * @param str2 The second char* ptr.
 * @return An integer less than, equal to, or greater than zero if str1 is less than, equal to, or greater than str2.
 *         Returns -1 if either str1 or str2 is NULL.
 */
int str_compare(const Str* str1, const char* str2);

// Resize Str to fit capacity. Returns 1 if successful.
bool str_ensure_capacity(Str* str, size_t capacity);

/* Returns the underling char * */
char* str_data(const Str* str);

/**
 * Creates a copy of a Str object.
 *
 * @param str The Str object to copy.
 * @return A new copy of the Str object, or NULL if memory allocation fails or str is NULL.
 */
Str* str_copy(const Str* str);

/**
 * Concatenates two Str objects. str1 is resized if neccessary to fit new String object.
 *
 * @param str1 The first Str object.
 * @param str2 The second char* object.
 * @return void
 */
void str_concat(Str* str1, const char* str2);

/**
 * Retrieves the length of a Str object.
 *
 * @param str The Str object.
 * @return The length of the Str object, or -1 if str is NULL.
 */
size_t str_length(const Str* str);

/**
 * Retrieves a character at the specified index from a Str object.
 *
 * @param str The Str object.
 * @param index The index of the character to retrieve.
 * @return A pointer to the character at the specified index, or NULL if str is NULL or index is out of bounds.
 */
char* str_at(const Str* str, size_t index);

/**
 * Checks if a Str object contains a specified substring.
 *
 * @param str The Str object to search in.
 * @param substring The substring to search for.
 * @return true if the substring is found, false otherwise. Returns false if either str or substring is NULL.
 */
bool str_contains(const Str* str, const char* substring);

/**
 * Checks if a Str object is empty.
 *
 * @param str The Str object to check.
 * @return true if the Str is empty, false otherwise. Returns true if str is NULL.
 */
bool str_is_empty(const Str* str);

/**
 * Finds the first occurrence of a substring in a Str object.
 *
 * @param str The Str object to search in.
 * @param substring The substring to search for.
 * @return The index of the first occurrence of the substring, or -1 if the substring is not found
 *         or either str or substring is NULL.
 */
int str_find(const Str* str, const char* substring);

/**
 * Replaces the first occurrence of a substring in a Str object with a new substring.
 *
 * @param str The Str object.
 * @param old_substring The substring to replace.
 * @param new_substring The new substring to insert.
 */
void str_replace(Str* str, const char* old_substring, const char* new_substring);

/**
 * Replaces all occurrences of a substring in a Str object with a new substring.
 *
 * @param str The Str object.
 * @param old_substring The substring to replace.
 * @param new_substring The new substring to insert.
 * @return void
 */
void str_replace_all(Str* str, const char* old_substring, const char* new_substring);

/**
 * Converts a Str object to uppercase.
 *
 * @param str The Str object to convert.
 */
void str_to_upper(Str* str);

/**
 * Converts a Str object to lowercase.
 *
 * @param str The Str object to convert.
 */
void str_to_lower(Str* str);

/**
 * Splits a Str object into substrings based on a delimiter.
 *
 * @param str The Str object to split.
 * @param delimiter The delimiter used to separate substrings.
 * @param substrings An array to store the resulting substrings.
 * @param num_substrings A pointer to an integer to store the number of substrings.
 */
char** str_split(const Str* str, const char* delimiter, int* num_substrings);

// Free memory allocated by str_split.
void str_free_substrings(char** substrings, int num_substrings);

/**
 * Checks if a Str object matches a regular expression.
 *
 * @param str The Str object to match.
 * @param regex The regular expression to use for matching.
 * @return true if the Str matches the regular expression, false otherwise. Returns false if either str or regex is NULL.
 */
bool str_match(const Str* str, const char* regex);

/**
 * Converts a Str object to camel case.
 *
 * @param str The Str object to convert.
 */
void str_to_camel_case(Str* str);

/**
 * Converts a Str object to title case.
 *
 * @param str The Str object to convert.
 */
void str_to_title_case(Str* str);

/**
 * Converts a Str object to snake case.
 *
 * @param str The Str object to convert.
 */
void str_to_snake_case(Str* str);

/**
 * Inserts a substring into a Str object at the specified index.
 *
 * @param s The Str object.
 * @param str The substring to insert.
 * @param index The index at which to insert the substring.
 */
void str_insert(Str* s, const char* str, size_t index);

/**
 * Removes a substring from a Str object starting at the specified index.
 *
 * @param s The Str object.
 * @param index The starting index of the substring to remove.
 * @param count The number of characters to remove.
 */
void str_remove(Str* s, size_t index, size_t count);

/**
 * Joins an array of substrings into a single Str object using a delimiter.
 *
 * @param substrings An array of substrings.
 * @param count The number of substrings in the array.
 * @param delimiter The delimiter used to join the substrings.
 * @param buffer Buffer to hold joined string.
 * @param bufsize Buffer size.
 */
void str_join(const char** substrings, int count, char delimiter, char* buffer, size_t bufsize);

/**
 * Extracts a substring from a Str object. If the buffer is not large, enough
 * it will be truncated.
 *
 * @param s The Str object.
 * @param start The starting index of the substring.
 * @param end The ending index of the substring.
 * @param substring The substring buffer.
 */
void str_substring(const Str* s, size_t start, size_t end, char* substring, size_t bufsize);

/**
 * Reverses the characters in a Str object.
 *
 * @param s The Str object to reverse.
 */
void str_reverse(Str* s);

/**
 * Checks if a Str object starts with a specified prefix.
 *
 * @param s The Str object.
 * @param prefix The prefix to check.
 * @return true if the Str starts with the prefix, false otherwise. Returns false if either s or prefix is NULL.
 */
int str_startswith(const Str* s, const char* prefix);

/**
 * Checks if a Str object ends with a specified suffix.
 *
 * @param s The Str object.
 * @param suffix The suffix to check.
 * @return true if the Str ends with the suffix, false otherwise. Returns false if either s or suffix is NULL.
 */
int str_endswith(const Str* s, const char* suffix);

char* regex_sub_match(const char* str, const char* regex, int capture_group);

#ifdef USE_PCRE_REGEX
char* regex_sub_match_pcre(const char* str, const char* regex, int capture_group);
char** regex_sub_matches_pcre(const char* str, const char* regex, int num_capture_groups,
                              int* num_matches);
#endif

#endif /* STR_H */
