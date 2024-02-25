#ifndef DC9DF236_E1D3_4B30_ABD2_0833ADB3BB55
#define DC9DF236_E1D3_4B30_ABD2_0833ADB3BB55

#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

// string_copy returns a new copy of the string str.
// str may be a heap-allocated string or a string literal.
char* string_copy(const char* str) __attribute__((warn_unused_result));

// string_append appends append to str.
// Returns a pointer to the new string. str is not modified.
char* string_append(const char* str, const char* append) __attribute__((warn_unused_result));

// string_append_char appends c to str. str is reallocated to fit the new
// string.
char* string_append_char(const char* str, char c) __attribute__((warn_unused_result));

// string_insert inserts s into str at position pos. str is reallocated to fit
// the new string.
char* string_insert(const char* str, size_t pos, const char* s) __attribute__((warn_unused_result));

// string_split splits str using the delimiter c.
// The number of tokens is stored in num_tokens.
// The function returns an array of strings allocated on the heap.
char** string_split(const char* str, const char* c, size_t* num_tokens)
    __attribute__((warn_unused_result));

// string_substr returns a substring of str from start to end.
// If start or end are out of bounds, or start > end, the function returns NULL.
// The caller is responsible for freeing the returned string.
char* string_substr(const char* str, size_t start, size_t end) __attribute__((warn_unused_result));

// string_join joins the strings in the array strings using the separator sep.
// The function returns a new string allocated on the heap.
char* string_join(const char** strings, size_t len, const char* sep)
    __attribute__((warn_unused_result));

// string_format formats a string using printf-style format and arguments.
char* string_format(const char* format, ...) __attribute__((warn_unused_result));

// string_prepend prepends string b to string a.
char* string_prepend(const char* a, const char* b) __attribute__((warn_unused_result));

// string_lower converts the string str to lowercase.
void string_lower(char* str);

// string_upper converts the string str to uppercase.
void string_upper(char* str);

// string_snakecase converts the string str to snake_case.
// The function returns a new string allocated on the heap.
char* string_snakecase(const char* str) __attribute__((warn_unused_result));

// string_titlecase converts the string str to Title Case.
void string_titlecase(char* str);

// string_camelcase converts the string str to camelCase.
void string_camelcase(char* str);

// string_pascalcase converts the string str to PascalCase.
void string_pascalcase(char* str);

// string_replace replaces the first occurrence of old with new in str.
char* string_replace(const char* str, const char* old, const char* new_str)
    __attribute__((warn_unused_result));

// string_replace_all replaces all occurrences of old with new in str.
// The function returns a new string since it may have been reallocated.
char* string_replace_all(const char* str, const char* old, const char* new_str)
    __attribute__((warn_unused_result));

// string_ltrim removes leading whitespace from str.
void string_ltrim(char* str);

// string_rtrim removes trailing whitespace from str.
void string_rtrim(char* str);

// string_trim removes leading and trailing whitespace from str.
void string_trim(char* str);

// string_trim_chars removes leading and trailing characters c from str.
void string_trim_chars(char* str, const char* c);

// string_trim_char removes leading and trailing character c from str.
void string_trim_char(char* str, char c);

// string_reverse reverses the string str in place.
void string_reverse(char* str);

// string_count_substr returns the number of occurrences of the substring sub in
// str.
size_t string_count_substr(const char* str, const char* sub);

// string_remove_char removes all occurrences of character c from str.
void string_remove_char(char* str, char c);

// string_remove_substr removes characters in str from start index, up to start
// + length.
void string_remove_substr(char* str, size_t start, size_t length);

// string_contains returns 1 if str contains the substring sub, otherwise 0.
int string_contains(const char* str, const char* sub);

// string_contains returns 1 if str contains the substring sub, otherwise 0.
// The comparison is case-insensitive.
int string_contains_nocase(const char* str, const char* sub);

// string_starts_with returns 1 if str starts with the substring sub, otherwise
// 0.
int string_starts_with(const char* str, const char* sub);

// string_starts_with returns 1 if str starts with the substring sub, otherwise
// 0. The comparison is case-insensitive.
int string_starts_with_nocase(const char* str, const char* sub);

// string_ends_with returns 1 if str ends with the substring sub, otherwise 0.
int string_ends_with(const char* str, const char* sub);

// string_ends_with returns 1 if str ends with the substring sub, otherwise 0.
// The comparison is case-insensitive.
int string_ends_with_nocase(const char* str, const char* sub);

// regex_match returns true if str matches the pattern, otherwise false.
bool regex_match(const char* str, const char* pattern);

// regex_replace returns a new string with the first occurrence of pattern
// replaced by replacement.
char* regex_replace(const char* str, const char* pattern, const char* replacement)
    __attribute__((warn_unused_result));

// regex_replace_all returns a new string with all occurrences of pattern
char* regex_replace_all(const char* str, const char* pattern, const char* replacement)
    __attribute__((warn_unused_result));

// regex_split splits str using the delimiter pattern.
const char** regex_split(const char* str, const char* pattern, size_t* len)
    __attribute__((warn_unused_result));

bool strings_equal(const char* a, const char* b);
bool strings_equal_nocase(const char* a, const char* b);

// ======== ROBUST STRING CONVERSION FUNCTIONS =========
// string_to_int converts the string str to an integer.
// valid is set to true if the conversion is successful, otherwise false.
int string_to_int(const char* str, bool* valid);

// string_to_long converts the string str to a long integer.
// valid is set to true if the conversion is successful, otherwise false.
long string_to_long(const char* str, bool* valid);

// string_to_longlong converts the string str to a long long integer.
// valid is set to true if the conversion is successful, otherwise false.
long long string_to_longlong(const char* str, bool* valid);

// string_to_float converts the string str to a float.
// valid is set to true if the conversion is successful, otherwise false.
float string_to_float(const char* str, bool* valid);

// string_to_double converts the string str to a double.
// valid is set to true if the conversion is successful, otherwise false.
double string_to_double(const char* str, bool* valid);

// string_to_bool converts the string str to a boolean.
// valid is set to true if the conversion is successful, otherwise false.
bool string_to_bool(const char* str, bool* valid);

// string_to_int_base converts the string str to an integer using the specified
// base. The base must be in the range 2 to 36.
// valid is set to true if the conversion is successful, otherwise false.
int string_to_int_base(const char* str, int base, bool* valid);

// string_to_long_base converts the string str to a long integer using the
// specified base. The base must be in the range 2 to 36.
// valid is set to true if the conversion is successful, otherwise false.
long string_to_long_base(const char* str, int base, bool* valid);

// Algorithms for advanced string comparison
//=========================================

// string_levenshtein_distance returns the Levenshtein distance between two
// strings. The Levenshtein distance is the minimum number of single-character
// edits (insertions, deletions, or substitutions) required to change one word
// into the other. The time complexity is O(m*n), where m and n are the lengths
// of the strings.
// The function returns -1 if malloc fails.
// Otherwise, it returns the Levenshtein distance.
size_t string_levenshtein_distance(const char* a, const char* b);

// string_hamming_distance returns the Hamming distance between two strings.
// The Hamming distance is the number of positions at which the corresponding
// symbols are different. The time complexity is O(n), where n is the length of
// the strings. The function returns -1 if the strings have different lengths.
size_t string_hamming_distance(const char* a, const char* b);

// string_jaro_distance returns the Jaro distance between two strings.
// The Jaro distance is a measure of similarity between two strings. The time
// complexity is O(n^2), where n is the length of the strings.
// The function returns a value in the range [0, 1].
double string_jaro_distance(const char* a, const char* b);

// Longest common subsequence (LCS) is a classic algorithm.
// It finds the longest subsequence common to all sequences in a set of strings.
// LCS implementation using dynamic programming.
// The time complexity is O(m*n), where m and n are the lengths of the strings.
// The longest common subsequence (LCS) problem is the problem of finding the
// longest subsequence common to all sequences in a set of sequences (often just
// two sequences). It differs from problems of finding common substrings: unlike
// substrings, subsequences are not required to occupy consecutive positions
// within the original sequences.
// The function returns NULL if malloc fails, otherwise it returns the longest
// common subsequence.
//
// @param a The first string.
// @param b The second string.
// @param s A pointer to a pointer to the longest common subsequence.
// s will be allocated on the heap and user is responsible for freeing it.
// @return The length of the longest common subsequence.
int string_lcs(const char* a, const char* b, char** s);

// string_cosine_similarity returns the cosine similarity between two strings.
// The cosine similarity is a measure of similarity between two non-zero vectors
// of an inner product space. It is defined to equal the cosine of the angle
// between them, which is also the same as the inner product of the same vectors
// normalized to both have length 1. The time complexity is O(n), where n is the
// length of the strings.
// The function returns a value in the range [0, 1].
// @param a The first string.
// @param b The second string.
double string_cosine_similarity(const char* a, const char* b);

// string_cosine_similarity_vec returns the cosine similarity between two
// vectors. The time complexity is O(n), where n is the length of the vectors.
// The function returns a value in the range [0, 1].
double cosine_similarity_vec(const int* v1, const int* v2, size_t n);
//  ========== UTILITY MACROS ===========
#define string_foreach(str, c) for (char* c = str; *c != '\0'; c++)

// string_foreach_rev is a reverse string foreach loop macro.
#define string_foreach_rev(str, c) for (char* c = str + strlen(str) - 1; c >= str; c--)

// A simple implementation of the Soundex algorithm(a phonetic algorithm).
// The Soundex algorithm is used to index words by their sound when pronounced
// in English. The goal is for homophones to be encoded to the same
// representation so that they can be matched despite minor differences in
// spelling. The Soundex code for a word consists of a letter followed by three
// numerical digits: the letter is the first letter of the word, and the digits
// encode the remaining consonants. The Soundex algorithm is not suitable for
// all surnames, and is not reliable for surnames of non-English origin. The
// Soundex algorithm is not case sensitive, so it is common to convert the input
// to uppercase before applying the Soundex algorithm.
//
// Implementation based on the description in the Wikipedia article:
// https://en.wikipedia.org/wiki/Soundex
// The time complexity is O(n), where n is the length of the string.
/*
B, F, P, V -> 1
C, G, J, K, Q, S, X, Z -> 2
D, T -> 3
L -> 4
M, N -> 5
R -> 6
*/
char* string_soundex(const char* str);

#ifdef USE_PCRE_REGEX

// regex_sub_match_pcre returns the substring of str that matches the capture
// group in the regex. The function returns a new string allocated on the heap.
char* regex_sub_match_pcre(const char* str, const char* regex, int capture_group)
    __attribute__((warn_unused_result));

// regex_capture returns an array of strings that match the capture groups in
// the regex. The function returns an array of strings allocated on the heap.
char** regex_capture(const char* str, const char* regex, int num_capture_groups, int* num_matches)
    __attribute__((warn_unused_result));
#endif

// ============ FUNCTIONS ============

// libpcre2 is a library for Perl-compatible regular expressions.
// It is a more powerful alternative to the POSIX regex library.
// It is not part of the C standard library, so it may not be available on all
// systems. If you want to use PCRE2, you need to install the library and
// include the header file. #include <pcre2.h> Link with -lpcre2-8
#ifdef USE_PCRE_REGEX
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

#ifdef __cplusplus
}
#endif

// Define this macro in only one file to include the implementation.
#ifdef STR_IMPL
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// copy the string str. Malloc is used to allocate memory for the new string.
// The caller is responsible for freeing the returned string.
// If str is NULL, the function returns NULL.
char* string_copy(const char* str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (copy == NULL) {
        fprintf(stderr, "string_copy(): malloc failed\n");
        exit(EXIT_FAILURE);
    }
    strcpy(copy, str);  // copy the string with null-terminator
    return copy;
}

bool strings_equal(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}
bool strings_equal_nocase(const char* a, const char* b) {
    return strcasecmp(a, b) == 0;
}

// copy the string str with excess memory. Malloc is used to allocate memory for
// the new string. We allocate strlen(str) + excess + 1 bytes to allow for extra
// memory.
static char* copy_with_excess(const char* str, size_t excess) {
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + excess + 1);
    if (copy == NULL) {
        fprintf(stderr, "copy_with_excess(): malloc failed\n");
        exit(EXIT_FAILURE);
    }

    // copy the string without null-terminator
    strncpy(copy, str, len);
    copy[len] = '\0';
    // extra memory is not initialized
    return copy;
}

// string_append appends append to str.
// str is reallocated to fit the new string.
// The src string is null-terminated after the concatenation.
// Returns the a pointer to new string since it may be re-allocated.
char* string_append(const char* str, const char* append) {
    size_t len        = strlen(str);
    size_t append_len = strlen(append);
    size_t new_len    = len + append_len;

    char* res = copy_with_excess(str, append_len);

    // Append the new string
    strncpy(res + len, append, append_len);
    res[new_len] = '\0';
    return res;
}

// string_append_char appends c to str.
// str is reallocated to fit the new string.
char* string_append_char(const char* str, char c) {
    size_t len = strlen(str);
    char* res  = copy_with_excess(str, 1);
    // Append the new character and null-terminator
    res[len]     = c;
    res[len + 1] = '\0';
    return res;
}

// string_insert inserts s into str at position pos.
char* string_insert(const char* str, size_t pos, const char* s) {
    size_t len     = strlen(str);
    size_t s_len   = strlen(s);
    size_t new_len = len + s_len;

    char* res = copy_with_excess(str, s_len);

    // Move the characters after the insertion point
    memmove(res + pos + s_len, res + pos, len - pos);
    // Insert the new string
    strncpy(res + pos, s, s_len);
    // Null-terminate the new string
    res[new_len] = '\0';
    return res;
}

// string_split splits str using the delimiter c.
// The number of tokens is stored in num_tokens.
// The function returns an array of strings.
// The caller is responsible for freeing the array and its elements.
char** string_split(const char* str, const char* c, size_t* num_tokens) {
    char* s = string_copy(str);
    if (s == NULL) {
        fprintf(stderr, "string_split(): string_copy failed\n");
        exit(EXIT_FAILURE);
    }

    // Count the number of tokens.
    char* token = strtok(s, c);
    size_t n    = 0;
    while (token != NULL) {
        n++;
        token = strtok(NULL, c);
    }
    free(s);

    // Allocate the array of tokens.
    char** tokens = (char**)malloc(n * sizeof(char*));
    if (tokens == NULL) {
        fprintf(stderr, "string_split(): malloc failed\n");
        exit(EXIT_FAILURE);
    }

    // Split the string and store the tokens.
    s = string_copy(str);
    if (s == NULL) {
        fprintf(stderr, "string_split(): string_copy failed\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(s, c);
    for (size_t i = 0; i < n; i++) {
        tokens[i] = string_copy(token);
        if (tokens[i] == NULL) {
            fprintf(stderr, "string_split(): string_copy failed\n");
            exit(EXIT_FAILURE);
        }
        token = strtok(NULL, c);
    }
    free(s);

    *num_tokens = n;
    return tokens;
}

// string_substr returns a substring of str from start to end.
// If start or end are out of bounds, or start > end, the function returns NULL.
// The caller is responsible for freeing the returned string.
char* string_substr(const char* str, size_t start, size_t end) {
    size_t len = strlen(str);
    if (start > len || end > len || start > end) {
        return NULL;
    }

    char* substr = (char*)malloc(end - start + 1);
    if (substr == NULL) {
        fprintf(stderr, "string_substr(): malloc failed\n");
        exit(EXIT_FAILURE);
    }

    strncpy(substr, str + start, end - start);
    substr[end - start] = '\0';
    return substr;
}

// string_join joins the strings in the array strings using the separator sep.
// The function returns a new string.
// The caller is responsible for freeing the returned string.
// If len is 0, the function returns NULL
char* string_join(const char** strings, size_t len, const char* sep) {
    if (len == 0) {
        return NULL;
    }

    size_t total_len = 0;
    for (size_t i = 0; i < len; i++) {
        total_len += strlen(strings[i]);
    }
    total_len += (len - 1) * strlen(sep);

    char* joined = (char*)malloc(total_len + 1);
    if (joined == NULL) {
        fprintf(stderr, "string_join(): malloc failed\n");
        exit(EXIT_FAILURE);
    }

    joined[0] = '\0';
    for (size_t i = 0; i < len; i++) {
        strcat(joined, strings[i]);

        // Do not append sep after the last string.
        if (i < len - 1) {
            strcat(joined, sep);
        }
    }

    return joined;
}

char* string_format(const char* format, ...) {
    char* s = NULL;
    va_list args;
    va_start(args, format);
    int ret = vasprintf(&s, format, args);
    if (ret == -1) {
        fprintf(stderr, "string_format(): vasprintf failed\n");
        return NULL;
    }
    va_end(args);
    return s;
}

// string_prepend prepends string b to string a.
char* string_prepend(const char* a, const char* b) {
    char* s = NULL;
    int ret = asprintf(&s, "%s%s", b, a);
    if (ret == -1) {
        fprintf(stderr, "string_prepend(): asprintf failed\n");
        return NULL;
    }
    return s;
}

void string_lower(char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        str[i] = tolower(str[i]);
    }
}

void string_upper(char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        str[i] = toupper(str[i]);
    }
}

char* string_snakecase(const char* s) {
    // account for possible extra _ chars
    char* str = copy_with_excess(s, strlen(s));

    size_t length = strlen(str);
    if (length == 0) {
        return str;
    }

    // Convert first character to lowercase
    str[0]             = tolower(str[0]);
    size_t space_count = 0;
    char prev          = str[0];

    for (size_t i = 1; i < length; i++) {
        // Check if current character is a space
        if (isspace(str[i])) {
            space_count++;
            continue;  // Skip spaces
        }

        // Check if current character is uppercase
        if (isupper(str[i])) {
            // Insert underscore before uppercase character
            memmove(&str[i + 1 - space_count], &str[i - space_count], length - i + space_count);
            str[i - space_count] = '_';
            length++;

            i++;  // Skip the inserted underscore
        }

        // Convert the character to lowercase
        str[i - space_count] = tolower(str[i]);

        // Insert underscore before digit if preceded by a non-digit character
        if (isdigit(str[i]) && !isdigit(prev)) {
            memmove(&str[i + 1 - space_count], &str[i - space_count], length - i + space_count);
            str[i - space_count] = '_';
            length++;

            i++;  // Skip the inserted underscore
        }

        prev = str[i];
    }

    // Trim the string to the new length
    str[length - space_count] = '\0';
    return str;
}

// Title case is a naming convention where the first letter of each word is
// capitalized.
void string_titlecase(char* str) {
    int capitalize = 1;
    size_t length  = strlen(str);

    for (size_t i = 0; i < length; i++) {
        if (str[i] == ' ') {
            capitalize = 1;
        } else if (capitalize) {
            str[i]     = toupper(str[i]);
            capitalize = 0;
        } else {
            str[i] = tolower(str[i]);
        }
    }
}

// Camel case is a naming convention where the first letter of each word is
// capitalized. The first word starts with a lowercase letter.
void string_camelcase(char* str) {
    size_t length = strlen(str);
    if (length == 0) {
        return;
    }

    int dest_index = 0;
    int capitalize = 0;

    while (str[dest_index] != '\0') {
        if (str[dest_index] == ' ' || str[dest_index] == '_') {
            capitalize = 1;
        } else if (capitalize) {
            str[dest_index] = toupper(str[dest_index]);
            capitalize      = 0;
        } else {
            str[dest_index] = tolower(str[dest_index]);
        }
        dest_index++;
    }

    // Remove spaces and underscores from the string
    int j = 0;
    for (dest_index = 0; str[dest_index] != '\0'; dest_index++) {
        if (str[dest_index] != ' ' && str[dest_index] != '_') {
            str[j] = str[dest_index];
            j++;
        }
    }

    // Pascal case starts with lower case, unlike camel case.
    str[0] = tolower(str[0]);
    str[j] = '\0';
}

// Pascal case is similar to camel case, but the first character is uppercase.
void string_pascalcase(char* str) {
    string_camelcase(str);
    if (strlen(str) > 0) {
        str[0] = toupper(str[0]);
    }
}

// Replace the first occurrence of old with new in str.
// If old is not found, the function returns NULL.
char* string_replace(const char* str, const char* old, const char* new_str) {
    char* p = (char*)strstr(str, old);
    if (p == NULL) {
        return NULL;
    }

    size_t old_len = strlen(old);
    size_t new_len = strlen(new_str);
    size_t str_len = strlen(str);

    char* res = NULL;
    if (new_len > old_len) {
        res = copy_with_excess(str, new_len - old_len);
    } else {
        res = string_copy(str);
    }

    memmove(res + (p - str) + new_len, p + old_len, str_len - (p - str) - old_len + 1);
    memcpy(res + (p - str), new_str, new_len);
    return res;
}

char* string_replace_all(const char* str, const char* old, const char* new_str) {
    size_t old_len = strlen(old);
    size_t new_len = strlen(new_str);
    size_t count   = 0;
    const char* p  = str;

    while ((p = strstr(p, old)) != NULL) {
        count++;
        p += old_len;
    }

    size_t str_len     = strlen(str);
    size_t new_str_len = str_len + count * (new_len - old_len);
    char* res          = (char*)malloc(new_str_len + 1);
    if (res == NULL) {
        fprintf(stderr, "string_replace_all(): malloc failed\n");
        exit(EXIT_FAILURE);
    }

    p       = str;
    char* q = res;
    while (1) {
        const char* match = strstr(p, old);
        if (match == NULL) {
            strcpy(q, p);
            break;
        }
        size_t n = match - p;
        memcpy(q, p, n);
        q += n;
        memcpy(q, new_str, new_len);
        q += new_len;
        p = match + old_len;
    }

    res[new_str_len] = '\0';
    return res;
}

// Remove leading whitespace from str.
void string_ltrim(char* str) {
    char* start = str;
    while (isspace(*start)) {
        start++;
    }

    size_t len = strlen(str) - (start - str);  // length of the remaining string
    memmove(str, start, len);                  // move the remaining string to the beginning
    str[len] = '\0';
}

// Remove trailing whitespace from str.
void string_rtrim(char* str) {
    char* end = str + strlen(str) - 1;  // pointer to the last character
    while (isspace(*end)) {
        end--;  // move the pointer to the left
    }

    size_t len = end - str + 1;  // length of the remaining string
    str[len]   = '\0';
}

// Remove leading and trailing whitespace from str.
void string_trim(char* str) {
    string_ltrim(str);
    string_rtrim(str);
}

// Remove leading and trailing characters c from str.
// .e.g. string_trim_chars("  Hello, world!  ", " \n\t\r") -> "Hello, world"
void string_trim_chars(char* str, const char* c) {
    // Remove leading characters
    char* start = str;
    while (strchr(c, *start)) {
        start++;
    }

    // Remove trailing characters
    char* end = str + strlen(str) - 1;
    while (strchr(c, *end)) {
        end--;  // move the pointer to the left
    }

    size_t len = end - start + 1;  // length of the remaining string
    memmove(str, start, len);      // move the remaining string to the beginning
    str[len] = '\0';
}

// Remove leading and trailing character c from str.
void string_trim_char(char* str, char c) {
    string_trim_chars(str, &c);
}

// Reverse the string str in place.
void string_reverse(char* str) {
    size_t len = strlen(str);
    for (size_t i = 0; i < len / 2; i++) {
        char temp        = str[i];
        str[i]           = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

// Return the number of occurrences of the substring sub in str.
size_t string_count_substr(const char* str, const char* sub) {
    size_t count   = 0;
    size_t sub_len = strlen(sub);
    const char* p  = str;

    while ((p = strstr(p, sub)) != NULL) {
        count++;
        p += sub_len;
    }
    return count;
}

// Remove all occurrences of character c from str.
void string_remove_char(char* str, char c) {
    char* dest = str;
    for (char* src = str; *src != '\0'; src++) {
        if (*src != c) {
            *dest = *src;
            dest++;
        }
    }
    *dest = '\0';
}

// Remove characters in str from start index, up to start + length.
void string_remove_substr(char* str, size_t start, size_t length) {
    size_t len = strlen(str);
    if (start >= len) {
        return;
    }

    if (start + length > len) {
        length = len - start;
    }

    memmove(str + start, str + start + length, len - start - length + 1);
}

// string_contains returns 1 if str contains the substring sub, otherwise 0.
int string_contains(const char* str, const char* sub) {
    return strstr(str, sub) != NULL;
}

// string_starts_with returns 1 if str starts with the substring sub, otherwise
// 0.
int string_starts_with(const char* str, const char* sub) {
    return strncmp(str, sub, strlen(sub)) == 0;
}

// string_ends_with returns 1 if str ends with the substring sub, otherwise 0.
int string_ends_with(const char* str, const char* sub) {
    size_t str_len = strlen(str);
    size_t sub_len = strlen(sub);
    if (sub_len > str_len) {
        return 0;
    }
    return strncmp(str + str_len - sub_len, sub, sub_len) == 0;
}

// string_contains returns 1 if str contains the substring sub, otherwise 0.
int string_contains_nocase(const char* str, const char* sub) {
    return strcasestr(str, sub) != NULL;
}

// string_starts_with returns 1 if str starts with the substring sub, otherwise
// 0.
int string_starts_with_nocase(const char* str, const char* sub) {
    return strncasecmp(str, sub, strlen(sub)) == 0;
}

// string_ends_with returns 1 if str ends with the substring sub, otherwise 0.
int string_ends_with_nocase(const char* str, const char* sub) {
    size_t str_len = strlen(str);
    size_t sub_len = strlen(sub);
    if (sub_len > str_len) {
        return 0;
    }
    return strncasecmp(str + str_len - sub_len, sub, sub_len) == 0;
}

// ============ REGEX ============
// regex_match returns true if str matches the pattern, otherwise false.
bool regex_match(const char* str, const char* pattern) {
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        perror("regcomp");
        exit(-1);
    }
    ret = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);
    return ret == 0;
}

// It is used to replace the first occurrence of a pattern in a string
// Returns a new string with the first occurrence of pattern replaced by
// replacement. The caller is responsible for freeing the returned string. The
// pattern is a valid regular expression as per POSIX standard. This is a simple
// implementation and does not handle all edge cases. If you need a more robust
// implementation, consider using a library like PCRE2. Example: const char*
// result = regex_replace("Hello, world!", "world", "there"); printf("%s\n",
// result);  // Output: "Hello, there!" If No match is found or regex can't be
// compiled, the function returns NULL.
char* regex_replace(const char* str, const char* pattern, const char* replacement) {
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        perror("regcomp");
        return NULL;
    }

    regmatch_t match;
    ret = regexec(&regex, str, 1, &match, 0);
    if (ret != 0) {  // No match
        regfree(&regex);
        return NULL;
    }

    size_t start           = match.rm_so;
    size_t end             = match.rm_eo;
    size_t len             = strlen(str);
    size_t replacement_len = strlen(replacement);
    size_t new_len         = len - (end - start) + replacement_len;

    char* new_str = (char*)malloc(new_len + 1);
    if (new_str == NULL) {
        fprintf(stderr, "regex_replace(): malloc failed\n");
        exit(EXIT_FAILURE);
    }

    strncpy(new_str, str, start);
    strncpy(new_str + start, replacement, replacement_len);
    strncpy(new_str + start + replacement_len, str + end, len - end);
    new_str[new_len] = '\0';

    regfree(&regex);
    return new_str;
}

// It is used to replace all occurrences of a pattern in a string
// Returns a new string with all occurrences of pattern replaced by replacement.
char* regex_replace_all(const char* str, const char* pattern, const char* replacement) {
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        perror("regcomp");
        exit(-1);
    }
    // count the number of matches
    size_t count = 0;
    regmatch_t match;
    const char* p = str;
    while (1) {
        ret = regexec(&regex, p, 1, &match, 0);
        if (ret != 0) {
            break;
        }
        count++;
        p += match.rm_eo;
    }

    // replace all matches
    size_t len     = strlen(str);
    size_t new_len = len + count * (strlen(replacement) - strlen(pattern));
    char* result   = (char*)malloc(new_len + 1);
    if (result == NULL) {
        perror("malloc");
        exit(-1);
    }

    char* q = result;
    p       = str;
    while (1) {
        ret = regexec(&regex, p, 1, &match, 0);
        if (ret != 0) {
            break;
        }
        size_t n = match.rm_so;
        memcpy(q, p, n);
        q += n;
        memcpy(q, replacement, strlen(replacement));
        q += strlen(replacement);
        p += match.rm_eo;
    }

    size_t n = len - (p - str);
    memcpy(q, p, n);
    q += n;
    *q = '\0';
    regfree(&regex);

    return result;
}

// It is used to split a string into an array of strings using a regular
// expression pattern. The function returns an array of strings. The caller is
// responsible for freeing the array and its elements.
const char** regex_split(const char* str, const char* pattern, size_t* len) {
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        perror("regcomp");
        exit(-1);
    }

    // count the number of matches
    size_t count = 0;
    regmatch_t match;
    const char* p = str;
    while (1) {
        ret = regexec(&regex, p, 1, &match, 0);
        if (ret != 0) {
            break;
        }
        count++;
        p += match.rm_eo;
    }

    // allocate memory for result
    const char** result = (const char**)malloc(sizeof(char*) * (count + 1));
    if (result == NULL) {
        perror("malloc");
        exit(-1);
    }

    // split the string
    p        = str;
    size_t i = 0;
    while (1) {
        ret = regexec(&regex, p, 1, &match, 0);
        if (ret != 0) {
            break;
        }
        size_t n    = match.rm_so;
        result[i++] = strndup(p, n);
        p += match.rm_eo;
    }

    result[i] = strdup(p);

    regfree(&regex);
    *len = count + 1;
    return result;
}

#ifdef USE_PCRE_REGEX
char* regex_sub_match_pcre(const char* str, const char* regex, int capture_group) {
    pcre2_code* compiled_regex;
    pcre2_match_data* match_data;
    PCRE2_SPTR subject = (PCRE2_SPTR)str;
    PCRE2_SPTR pattern = (PCRE2_SPTR)regex;
    int error_code;
    PCRE2_SIZE error_offset;

    compiled_regex =
        pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0, &error_code, &error_offset, NULL);
    if (compiled_regex == NULL) {
        printf("PCRE2 regex compilation failed\n");
        return NULL;
    }

    match_data = pcre2_match_data_create_from_pattern(compiled_regex, NULL);
    if (match_data == NULL) {
        printf("Failed to create match data\n");
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    int result = pcre2_match(compiled_regex, subject, strlen(str), 0, 0, match_data, NULL);

    if (result < 0) {
        printf("PCRE2 regex matching failed\n");
        pcre2_match_data_free(match_data);
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    if (result < capture_group + 1) {
        printf("Capture group not found\n");
        pcre2_match_data_free(match_data);
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    PCRE2_SIZE* offsets = pcre2_get_ovector_pointer(match_data);

    PCRE2_SIZE start      = offsets[capture_group * 2];
    PCRE2_SIZE end        = offsets[capture_group * 2 + 1];
    PCRE2_SIZE sub_length = end - start;

    char* sub_match = (char*)malloc((sub_length + 1) * sizeof(char));
    if (sub_match == NULL) {
        printf("Memory allocation failed\n");
        pcre2_match_data_free(match_data);
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    strncpy(sub_match, str + start, sub_length);
    sub_match[sub_length] = '\0';

    pcre2_match_data_free(match_data);
    pcre2_code_free(compiled_regex);
    return sub_match;
}

char** regex_capture(const char* str, const char* regex, int num_capture_groups, int* num_matches) {

    pcre2_code* compiled_regex;
    pcre2_match_data* match_data;
    PCRE2_SPTR subject = (PCRE2_SPTR)str;
    PCRE2_SPTR pattern = (PCRE2_SPTR)regex;
    int error_code;
    PCRE2_SIZE error_offset;

    compiled_regex =
        pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0, &error_code, &error_offset, NULL);
    if (compiled_regex == NULL) {
        printf("PCRE2 regex compilation failed\n");
        return NULL;
    }

    match_data = pcre2_match_data_create_from_pattern(compiled_regex, NULL);
    if (match_data == NULL) {
        printf("Failed to create match data\n");
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    int result = pcre2_match(compiled_regex, subject, strlen(str), 0, 0, match_data, NULL);

    if (result < 0) {
        printf("PCRE2 regex matching failed\n");
        pcre2_match_data_free(match_data);
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    int match_count = result / num_capture_groups;
    *num_matches    = result;

    char** sub_matches = (char**)malloc(match_count * num_capture_groups * sizeof(char*));
    if (sub_matches == NULL) {
        printf("Memory allocation failed\n");
        pcre2_match_data_free(match_data);
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    PCRE2_SIZE* offsets = pcre2_get_ovector_pointer(match_data);

    for (int i = 0; i < match_count; i++) {
        for (int j = 0; j < num_capture_groups; j++) {
            PCRE2_SIZE start      = offsets[(i * num_capture_groups + j) * 2];
            PCRE2_SIZE end        = offsets[(i * num_capture_groups + j) * 2 + 1];
            PCRE2_SIZE sub_length = end - start;

            sub_matches[i * num_capture_groups + j] =
                (char*)malloc((sub_length + 1) * sizeof(char));
            if (sub_matches[i * num_capture_groups + j] == NULL) {
                printf("Memory allocation failed\n");
                pcre2_match_data_free(match_data);
                pcre2_code_free(compiled_regex);
                for (int k = 0; k < i * num_capture_groups + j; k++) {
                    free(sub_matches[k]);
                }
                free(sub_matches);
                return NULL;
            }

            strncpy(sub_matches[i * num_capture_groups + j], str + start, sub_length);
            sub_matches[i * num_capture_groups + j][sub_length] = '\0';
        }
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(compiled_regex);
    return sub_matches;
}
#endif

// ======== ROBUST STRING CONVERSION FUNCTIONS =========
// string_to_int converts the string str to an integer.
// valid is set to true if the conversion is successful, otherwise false.
int string_to_int(const char* str, bool* valid) {
    char* endptr;
    long result = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') {
        *valid = false;
        return 0;
    }
    *valid = true;
    return (int)result;
}

// string_to_long converts the string str to a long integer.
// valid is set to true if the conversion is successful, otherwise false.
long string_to_long(const char* str, bool* valid) {
    char* endptr;
    long result = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') {
        *valid = false;
        return 0;
    }
    *valid = true;
    return result;
}

// string_to_longlong converts the string str to a long long integer.
// valid is set to true if the conversion is successful, otherwise false.
long long string_to_longlong(const char* str, bool* valid) {
    char* endptr;
    long long result = strtoll(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') {
        *valid = false;
        return 0;
    }
    *valid = true;
    return result;
}

// string_to_float converts the string str to a float.
// valid is set to true if the conversion is successful, otherwise false.
float string_to_float(const char* str, bool* valid) {
    char* endptr;
    float result = strtof(str, &endptr);
    if (endptr == str || *endptr != '\0') {
        *valid = false;
        return 0;
    }
    *valid = true;
    return result;
}

// string_to_double converts the string str to a double.
// valid is set to true if the conversion is successful, otherwise false.
double string_to_double(const char* str, bool* valid) {
    char* endptr;
    double result = strtod(str, &endptr);
    if (endptr == str || *endptr != '\0') {
        *valid = false;
        return 0;
    }
    *valid = true;
    return result;
}

// string_to_bool converts the string str to a boolean.
// valid is set to true if the conversion is successful, otherwise false.
// The function returns true if str is "true" or "1", otherwise false.
bool string_to_bool(const char* str, bool* valid) {
    if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0) {
        *valid = true;
        return true;
    } else if (strcmp(str, "false") == 0 || strcmp(str, "0") == 0) {
        *valid = true;
        return false;
    }
    *valid = false;
    return false;
}

// string_to_int_base converts the string str to an integer using the specified
// base. The base must be in the range 2 to 36.
// valid is set to true if the conversion is successful, otherwise false.
int string_to_int_base(const char* str, int base, bool* valid) {
    char* endptr;
    long result = strtol(str, &endptr, base);
    if (endptr == str || *endptr != '\0') {
        *valid = false;
        return 0;
    }
    *valid = true;
    return (int)result;
}

// string_to_long_base converts the string str to a long integer using the
// specified base. The base must be in the range 2 to 36.
// valid is set to true if the conversion is successful, otherwise false.
long string_to_long_base(const char* str, int base, bool* valid) {
    char* endptr;
    long result = strtol(str, &endptr, base);
    if (endptr == str || *endptr != '\0') {
        *valid = false;
        return 0;
    }
    *valid = true;
    return result;
}

// implement MIN3
static size_t MIN3(size_t a, size_t b, size_t c) {
    return a < b ? (a < c ? a : c) : (b < c ? b : c);
}

// string_levenshtein_distance returns the Levenshtein distance between two
// strings. The Levenshtein distance is the minimum number of single-character
// edits (insertions, deletions, or substitutions) required to change one word
// into the other. The time complexity is O(m*n), where m and n are the lengths
// of the strings.
// The function returns -1 if malloc fails.
// Otherwise, it returns the Levenshtein distance.
size_t string_levenshtein_distance(const char* a, const char* b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    // Allocate a single array for both current and previous distances
    // to reduce memory usage.
    size_t* distances = (size_t*)malloc((len_b + 1) * sizeof(size_t) * 2);

    if (distances == NULL) {
        fprintf(stderr, "string_levenshtein_distance(): malloc failed\n");
        return -1;
    }

    size_t* prev = distances;
    size_t* curr = distances + len_b + 1;

    for (size_t i = 0; i <= len_b; i++) {
        prev[i] = i;
    }

    for (size_t i = 1; i <= len_a; i++) {
        curr[0] = i;
        for (size_t j = 1; j <= len_b; j++) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j]     = MIN3(prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost);
        }

        // Swap prev and curr
        size_t* temp = prev;
        prev         = curr;
        curr         = temp;
    }

    size_t distance = prev[len_b];
    free(distances);

    return distance;
}

size_t string_hamming_distance(const char* a, const char* b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a != len_b) {
        return -1;
    }

    size_t distance = 0;
    for (size_t i = 0; i < len_a; i++) {
        if (a[i] != b[i]) {
            distance++;
        }
    }
    return distance;
}

// This implementation is based on the Jaro-Winkler distance algorithm.
// https://rosettacode.org/wiki/Jaro_similarity#:~:text=The%20Jaro%20distance%20is%20a,1%20is%20an%20exact%20match.
// The time complexity is O(n^2), where n is the length of the strings.
double string_jaro_distance(const char* str1, const char* str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    if (len1 == 0 || len2 == 0) {
        return 1;
    }
    if (len1 == 0 || len2 == 0) {
        return 0;
    }

    size_t match_distance = (size_t)(fmax(len1, len2) / 2) - 1;

    bool* str1_matches = (bool*)calloc(len1, sizeof(bool));
    assert(str1_matches != NULL);
    bool* str2_matches = (bool*)calloc(len2, sizeof(bool));
    assert(str2_matches != NULL);

    // Count the matches and transpositions
    size_t matches = 0, transpositions = 0;
    for (size_t i = 0; i < len1; i++) {
        size_t start = (size_t)(i > match_distance ? i - match_distance : 0);
        size_t end   = i + match_distance + 1;
        if (end > len2) {
            end = len2;
        }

        for (size_t k = start; k < end; k++) {
            if (str2_matches[k]) {
                continue;
            }
            if (str1[i] != str2[k]) {
                continue;
            }

            str1_matches[i] = true;
            str2_matches[k] = true;
            matches++;
            break;
        }
    }

    if (matches == 0) {
        free(str1_matches);
        free(str2_matches);
        return 0;
    }

    size_t k = 0;
    for (size_t i = 0; i < len1; i++) {
        if (!str1_matches[i]) {
            continue;
        }

        while (!str2_matches[k]) {
            k++;
        }

        if (str1[i] != str2[k]) {
            transpositions++;
        }
        k++;
    }

    free(str1_matches);
    free(str2_matches);

    transpositions /= 2;

    return (matches / (double)len1 + matches / (double)len2 +
            (matches - transpositions) / (double)matches) /
           3.0;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int string_lcs(const char* a, const char* b, char** s) {
    int i, j, k, t;  // Loop variables
    int n = strlen(a), m = strlen(b);

    // lcs_table is a 2D array used to store the length of LCS
    // lcs_table[i][j] is the length of LCS of a[0..i-1] and b[0..j-1]
    int* lcs_table = (int*)calloc((n + 1) * (m + 1), sizeof(int));
    if (!lcs_table) {
        perror("calloc");
        return -1;
    }

    // c is a 2D array of size (n+1)x(m+1) used to store the length of LCS
    int** c = (int**)calloc((n + 1), sizeof(int*));
    if (!c) {
        perror("calloc");
        free(lcs_table);
        return -1;
    }

    for (i = 0; i <= n; i++) {
        c[i] = &lcs_table[i * (m + 1)];
    }

    for (i = 1; i <= n; i++) {
        for (j = 1; j <= m; j++) {
            if (a[i - 1] == b[j - 1]) {
                c[i][j] = c[i - 1][j - 1] + 1;
            } else {
                c[i][j] = MAX(c[i - 1][j], c[i][j - 1]);
            }
        }
    }

    t  = c[n][m];
    *s = (char*)malloc(t);
    if (!*s) {
        perror("malloc");
        free(c);
        free(lcs_table);
        return -1;
    }

    for (i = n, j = m, k = t - 1; k >= 0;) {
        if (a[i - 1] == b[j - 1])
            (*s)[k] = a[i - 1], i--, j--, k--;
        else if (c[i][j - 1] > c[i - 1][j])
            j--;
        else
            i--;
    }
    free(c);
    free(lcs_table);

    // null terminate the string
    (*s)[t] = '\0';
    return t;
}

double cosine_similarity_vec(const int* v1, const int* v2, size_t n) {
    double dot_product = 0.0;
    double norm_v1     = 0.0;
    double norm_v2     = 0.0;

    for (size_t i = 0; i < n; i++) {
        dot_product += v1[i] * v2[i];
        norm_v1 += v1[i] * v1[i];
        norm_v2 += v2[i] * v2[i];
    }
    return dot_product / (sqrt(norm_v1) * sqrt(norm_v2));
}

#define NUM_LETTERS 26
double string_cosine_similarity(const char* s1, const char* s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    // This means we can't handle many non-ASCII characters
    // 26 letters in the alphabet
    int v1[NUM_LETTERS] = {0};
    int v2[NUM_LETTERS] = {0};

    for (size_t i = 0; i < len1; i++) {
        v1[tolower(s1[i]) - 'a']++;
    }

    for (size_t i = 0; i < len2; i++) {
        v2[tolower(s2[i]) - 'a']++;
    }
    return cosine_similarity_vec(v1, v2, NUM_LETTERS);
}

char* string_soundex(const char* str) {
    size_t len = strlen(str);
    if (len == 0) {
        return NULL;
    }

    char* soundex = (char*)malloc(5);  // 5 characters: 1 letter and 3 digits
    if (soundex == NULL) {
        perror("malloc");
        return NULL;
    }

    soundex[0]        = toupper(str[0]);
    int soundex_index = 1;
    int prev_code     = -1;

    for (size_t i = 1; i < len; i++) {
        if (soundex_index == 4) {
            break;
        }

        int code = -1;
        switch (toupper(str[i])) {
            case 'B':
            case 'F':
            case 'P':
            case 'V':
                code = 1;
                break;
            case 'C':
            case 'G':
            case 'J':
            case 'K':
            case 'Q':
            case 'S':
            case 'X':
            case 'Z':
                code = 2;
                break;
            case 'D':
            case 'T':
                code = 3;
                break;
            case 'L':
                code = 4;
                break;
            case 'M':
            case 'N':
                code = 5;
                break;
            case 'R':
                code = 6;
                break;
            default:
                code = -1;
                break;
        }

        if (code != -1 && code != prev_code) {
            soundex[soundex_index] = '0' + code;
            soundex_index++;
        }
        prev_code = code;
    }

    while (soundex_index < 4) {
        soundex[soundex_index] = '0';
        soundex_index++;
    }

    soundex[4] = '\0';
    return soundex;
}

#endif /* STR_IMPL */

#endif /* DC9DF236_E1D3_4B30_ABD2_0833ADB3BB55 */
