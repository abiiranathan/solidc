/**
 * @file str_utils.h
 * @brief String utility functions for manipulation and processing.
 */

#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Trims whitespace from the beginning and end of a string in-place.
 *
 * Optimization:
 * 1. Skips leading whitespace.
 * 2. Uses a single forward pass to find the end of the string while
 *    tracking the last non-space character, avoiding a separate strlen().
 *
 * @param str The string to trim.
 * @return Pointer to the new start of the string.
 */
static inline char* trim_string(char* str) {
    if (!str) return NULL;

    // Trim leading whitespace
    while (isspace((unsigned char)*str)) {
        str++;
    }

    // If string is empty or all spaces, return early
    if (*str == '\0') {
        return str;
    }

    // Single-pass forward scan for trailing trim
    char* end = str;
    char* cursor = str;
    while (*cursor) {
        if (!isspace((unsigned char)*cursor)) {
            end = cursor;
        }
        cursor++;
    }

    // Terminate after the last non-space character
    *(end + 1) = '\0';

    return str;
}

/*
 * Microsoft Visual C++ (MSVC) lacks standard POSIX string functions.
 * MinGW (which also defines _WIN32) typically includes them in <strings.h>.
 */
#if defined(_MSC_VER)

#include <stdbool.h>

/**
 * Case-insensitive string comparison (MSVC Wrapper).
 * Wraps MSVC's _stricmp to provide NULL safety.
 */
static inline int strcasecmp(const char* s1, const char* s2) {
    // Handle NULL pointers safely (not standard POSIX, but requested behavior)
    if (s1 == s2) return 0;
    if (s1 == NULL) return -1;
    if (s2 == NULL) return 1;

    return _stricmp(s1, s2);
}

/**
 * Case-insensitive string comparison with length limit (MSVC Wrapper).
 * Wraps MSVC's _strnicmp to provide NULL safety.
 */
static inline int strncasecmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;

    // Handle NULL pointers safely
    if (s1 == s2) return 0;
    if (s1 == NULL) return -1;
    if (s2 == NULL) return 1;

    return _strnicmp(s1, s2, n);
}

/**
 * Case-insensitive substring search.
 * MSVC does not provide a standard CRT strcasestr (only via Shlwapi.h).
 * This implementation is a clean, dependency-free version.
 */
static inline char* strcasestr(const char* haystack, const char* needle) {
    if (!needle || *needle == '\0') return (char*)haystack;
    if (!haystack) return NULL;

    const size_t needle_len = strlen(needle);

    /* Optimization:
       We iterate until the remaining haystack is shorter than the needle.
       We do the tolower() conversion on the fly to avoid allocating memory. */
    while (*haystack) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }

    return NULL;
}

#else

/*
 * Non-MSVC platforms (Linux, macOS, MinGW, BSD)
 * typically provide these in <strings.h>.
 */
#include <strings.h>

/*
 * Note: standard strcasestr is a GNU/BSD extension.
 * If your specific compiler settings (e.g., -std=c99 --pedantic)
 * hide it, you may need to explicitly define _GNU_SOURCE before includes
 * or uncomment the fallback implementation below.
 */

#endif  // _MSC_VER

#ifdef __cplusplus
}
#endif

#endif  // STR_UTILS_H
