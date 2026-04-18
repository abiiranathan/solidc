/**
 * @file str.h
 * @brief Header-only string utility library for C11.
 *
 * All functions operate on ordinary NUL-terminated C strings.
 * Functions whose names begin with @c str_ fall into two categories:
 *
 *  **In-place** – modify the buffer passed in; no allocation, O(n) at most.
 *  **Allocating** – return a newly @c malloc'd string (or array).
 *                   The caller is responsible for calling @c free() on every
 *                   pointer that is returned (including each element of a
 *                   split array; see @ref str_split and @ref str_free_split).
 *
 * Every function that accepts a pointer guards against NULL unless the
 * parameter is explicitly documented as "must not be NULL".
 *
 * @note This header requires C11 or later.  MSVC users get thin shims for
 *       @c strcasecmp, @c strncasecmp and @c strcasestr at the bottom of the
 *       file.
 *
 * @section sections Sections
 *  - Internal fast search
 *  - Predicates (query / test)
 *  - Search & position
 *  - Case conversion (in-place)
 *  - Trimming, reversal & removal (in-place)
 *  - Allocating helpers
 *  - Number ↔ string conversions
 *  - Legacy / platform helpers
 */

#ifndef __STR_H__
#define __STR_H__

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Internal fast search
 * ======================================================================= */

/**
 * @internal
 * @brief Finds the first occurrence of needle @p nd (length @p nlen) inside
 *        haystack @p hs (length @p hlen).
 *
 * Avoids the overhead of platform @c memmem and is tuned for short needles:
 *  - zero-length needle  → returns @p hs immediately
 *  - one-byte needle     → delegates to @c memchr
 *  - short needles ≤ 9   → byte loop (avoids function call)
 *  - longer needles      → @c memcmp on candidate suffix
 *
 * @param hs   Haystack (need not be NUL-terminated; length given by @p hlen).
 * @param hlen Length of the haystack in bytes.
 * @param nd   Needle   (need not be NUL-terminated; length given by @p nlen).
 * @param nlen Length of the needle in bytes.
 * @return     Pointer to first match inside @p hs, or @c NULL if not found.
 */
static inline const char* str_search_impl(const char* hs, size_t hlen, const char* nd, size_t nlen) {
    if (nlen == 0) return hs;
    if (hlen < nlen) return NULL;
    if (nlen == 1) return (const char*)memchr(hs, (unsigned char)nd[0], hlen);

    const char* cur = hs;
    const char* end = hs + hlen - nlen;
    unsigned char first = (unsigned char)nd[0];
    unsigned char last = (unsigned char)nd[nlen - 1];

    while (cur <= end) {
        cur = (const char*)memchr(cur, first, (size_t)(end - cur + 1));
        if (!cur) return NULL;

        if ((unsigned char)cur[nlen - 1] == last) {
            if (nlen <= 9) {
                /* Unrolled inner comparison avoids memcmp call overhead. */
                size_t i = 0;
                for (; i < nlen - 2; i++) {
                    if (cur[1 + i] != nd[1 + i]) goto next_iter;
                }
                return cur;
            } else {
                if (memcmp(cur + 1, nd + 1, nlen - 2) == 0) return cur;
            }
        }
    next_iter:
        cur++;
    }
    return NULL;
}

/* =========================================================================
 * Predicates — O(n), no allocation
 * ======================================================================= */

/**
 * @brief Returns @c true when @p str is NULL or has zero length.
 *
 * Distinguishes from @ref str_is_blank, which also considers whitespace-only
 * strings as "empty."
 *
 * @param str  String to test (may be NULL).
 * @return     @c true  if @p str == NULL or @p str[0] == '\\0'.
 */
static inline bool str_is_empty(const char* str) { return !str || str[0] == '\0'; }

/**
 * @brief Returns @c true when @p str is NULL, empty, or contains only
 *        ASCII whitespace characters.
 *
 * @param str  String to test (may be NULL).
 * @return     @c true  if every byte satisfies @c isspace().
 */
static inline bool str_is_blank(const char* str) {
    if (!str) return true;
    for (; *str; str++) {
        if (!isspace((unsigned char)*str)) return false;
    }
    return true;
}

/**
 * @brief Returns @c true when every character in @p str satisfies
 *        @c isalpha().
 *
 * An empty or NULL string returns @c false.
 *
 * @param str  NUL-terminated string (may be NULL).
 */
static inline bool str_is_alpha(const char* str) {
    if (!str || !*str) return false;
    for (; *str; str++) {
        if (!isalpha((unsigned char)*str)) return false;
    }
    return true;
}

/**
 * @brief Returns @c true when every character in @p str satisfies
 *        @c isdigit() (ASCII decimal digits 0–9).
 *
 * An empty or NULL string returns @c false.
 *
 * @param str  NUL-terminated string (may be NULL).
 */
static inline bool str_is_digit(const char* str) {
    if (!str || !*str) return false;
    for (; *str; str++) {
        if (!isdigit((unsigned char)*str)) return false;
    }
    return true;
}

/**
 * @brief Returns @c true when every character in @p str satisfies
 *        @c isalnum().
 *
 * An empty or NULL string returns @c false.
 *
 * @param str  NUL-terminated string (may be NULL).
 */
static inline bool str_is_alnum(const char* str) {
    if (!str || !*str) return false;
    for (; *str; str++) {
        if (!isalnum((unsigned char)*str)) return false;
    }
    return true;
}

/**
 * @brief Returns @c true when @p str represents a valid decimal integer,
 *        optionally preceded by a single @c + or @c - sign.
 *
 * Whitespace is not permitted.  Strings like "  42" or "3.14" return false.
 *
 * @param str  NUL-terminated string (may be NULL).
 */
static inline bool str_is_numeric(const char* str) {
    if (!str || !*str) return false;
    if (*str == '+' || *str == '-') str++;
    if (!*str) return false; /* bare sign */
    for (; *str; str++) {
        if (!isdigit((unsigned char)*str)) return false;
    }
    return true;
}

/**
 * @brief Returns @c true when @p str represents a valid floating-point
 *        number acceptable by @c strtod (no leading/trailing whitespace).
 *
 * Examples that return true:  "3.14", "-1e10", "+.5", "NaN", "inf".
 *
 * @param str  NUL-terminated string (may be NULL).
 */
static inline bool str_is_float(const char* str) {
    if (!str || !*str) return false;
    char* end;
    errno = 0;
    (void)strtod(str, &end);
    return *end == '\0' && end != str;
}

/**
 * @brief Case-sensitive equality test.
 *
 * @param a   First string  (may be NULL).
 * @param b   Second string (may be NULL).
 * @return    @c true iff both are non-NULL and @c strcmp returns 0.
 */
static inline bool str_equals(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

/**
 * @brief Case-insensitive equality test (ASCII only).
 *
 * @param a   First string  (may be NULL).
 * @param b   Second string (may be NULL).
 * @return    @c true iff both are non-NULL and differ only in ASCII case.
 */
static inline bool str_iequals(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    }
    return *a == '\0' && *b == '\0';
}

/**
 * @brief Returns @c true when @p substr appears anywhere in @p str.
 *
 * Equivalent to @c str_find(str, substr) >= 0 but without computing the
 * offset.
 *
 * @param str     Haystack (may be NULL).
 * @param substr  Needle   (may be NULL).
 */
static inline bool str_contains(const char* str, const char* substr) {
    if (!str || !substr) return false;
    size_t hlen = strlen(str);
    size_t nlen = strlen(substr);
    return str_search_impl(str, hlen, substr, nlen) != NULL;
}

/**
 * @brief Tests whether @p str starts with @p prefix.
 *
 * An empty prefix always matches (returns @c true).
 *
 * @param str     String to test (may be NULL).
 * @param prefix  Prefix to look for (may be NULL).
 */
static inline bool str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    size_t plen = strlen(prefix);
    if (plen == 0) return true;
    return strncmp(str, prefix, plen) == 0;
}

/**
 * @brief Tests whether @p str ends with @p suffix.
 *
 * An empty suffix always matches (returns @c true).
 *
 * @param str     String to test (may be NULL).
 * @param suffix  Suffix to look for (may be NULL).
 */
static inline bool str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t slen = strlen(suffix);
    if (slen == 0) return true;
    size_t len = strlen(str);
    if (slen > len) return false;
    return memcmp(str + len - slen, suffix, slen) == 0;
}

/* =========================================================================
 * Search & position — O(n), no allocation
 * ======================================================================= */

/**
 * @brief Returns the byte offset of the first occurrence of @p substr in
 *        @p str, or @c -1 if not found.
 *
 * @param str     Haystack (may be NULL).
 * @param substr  Needle   (may be NULL).
 * @return        Zero-based byte index, or @c -1.
 */
static inline int str_find(const char* str, const char* substr) {
    if (!str || !substr) return -1;
    size_t hlen = strlen(str);
    size_t nlen = strlen(substr);
    const char* found = str_search_impl(str, hlen, substr, nlen);
    return found ? (int)(found - str) : -1;
}

/**
 * @brief Returns the byte offset of the *last* occurrence of @p substr in
 *        @p str, or @c -1 if not found.
 *
 * Scans left-to-right keeping track of the most recent match so that the
 * overall complexity stays O(n · m) worst-case (same as @ref str_find).
 *
 * @param str     Haystack (may be NULL).
 * @param substr  Needle   (may be NULL).
 * @return        Zero-based byte index, or @c -1.
 */
static inline int str_rfind(const char* str, const char* substr) {
    if (!str || !substr) return -1;
    size_t hlen = strlen(str);
    size_t nlen = strlen(substr);
    if (nlen == 0 || nlen > hlen) return -1;

    const char* last = NULL;
    const char* p = str;

    while ((p = str_search_impl(p, hlen - (size_t)(p - str), substr, nlen)) != NULL) {
        last = p++;
        /* Once there is no room left for another match, stop early. */
        if ((size_t)(p - str) + nlen > hlen) break;
    }
    return last ? (int)(last - str) : -1;
}

/**
 * @brief Counts the number of non-overlapping occurrences of @p substr in
 *        @p str.
 *
 * An empty needle returns 0.
 *
 * @param str     Haystack (may be NULL).
 * @param substr  Needle   (may be NULL).
 * @return        Occurrence count (0 if either pointer is NULL or needle is
 *                empty).
 */
static inline size_t str_count_substr(const char* str, const char* substr) {
    if (!str || !substr) return 0;
    size_t nlen = strlen(substr);
    size_t hlen = strlen(str);
    if (nlen == 0 || nlen > hlen) return 0;

    size_t count = 0;
    const char* p = str;
    size_t rem = hlen;

    while ((p = str_search_impl(p, rem, substr, nlen)) != NULL) {
        count++;
        p += nlen;
        rem = hlen - (size_t)(p - str);
        if (rem < nlen) break;
    }
    return count;
}

/**
 * @brief Counts the number of words in @p str.
 *
 * Words are defined as maximal runs of non-whitespace characters,
 * matching the same boundary used by @c str_split(str, " ", ...) after
 * collapsing runs.
 *
 * @param str  NUL-terminated string (may be NULL).
 * @return     Word count.
 */
static inline size_t str_word_count(const char* str) {
    if (!str) return 0;
    size_t count = 0;
    bool in_word = false;
    for (; *str; str++) {
        if (isspace((unsigned char)*str)) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            count++;
        }
    }
    return count;
}

/* =========================================================================
 * In-place case conversion — O(n), no allocation
 * ======================================================================= */

/**
 * @brief Converts all ASCII uppercase letters in @p str to lowercase in
 *        place.
 *
 * Only the 26 ASCII letters A–Z are affected; other bytes are unchanged.
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_lower(char* str) {
    if (!str) return;
    for (; *str; str++) {
        unsigned char c = (unsigned char)*str;
        if ((unsigned)(c - 'A') <= 25u) *str = (char)(c | 0x20u);
    }
}

/**
 * @brief Converts all ASCII lowercase letters in @p str to uppercase in
 *        place.
 *
 * Only the 26 ASCII letters a–z are affected; other bytes are unchanged.
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_upper(char* str) {
    if (!str) return;
    for (; *str; str++) {
        unsigned char c = (unsigned char)*str;
        if ((unsigned)(c - 'a') <= 25u) *str = (char)(c & ~0x20u);
    }
}

/**
 * @brief Capitalises the first character of @p str and lowercases the rest,
 *        in place.
 *
 * Example: @c "hELLO wORLD" → @c "Hello world".
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_capitalize(char* str) {
    if (!str || !*str) return;
    *str = (char)toupper((unsigned char)*str);
    str++;
    for (; *str; str++) *str = (char)tolower((unsigned char)*str);
}

/**
 * @brief Converts @p str to camelCase in place.
 *
 * Leading underscores and spaces are stripped.  Each word boundary
 * (underscore or whitespace) causes the following letter to be
 * uppercased.  The very first character is forced to lowercase so the
 * result begins with a lowercase letter (@c "my_var" → @c "myVar").
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_camelcase(char* str) {
    if (!str || !*str) return;

    size_t r = 0;
    size_t w = 0;
    size_t len = strlen(str);

    /* Skip leading underscores / spaces */
    while (r < len && (str[r] == '_' || isspace((unsigned char)str[r]))) r++;

    /* First character is lowercased */
    if (r < len) {
        unsigned char c = (unsigned char)str[r++];
        str[w++] = (char)((unsigned)(c - 'A') <= 25u ? (c | 0x20u) : c);
    }

    bool cap = false;
    while (r < len) {
        unsigned char c = (unsigned char)str[r++];
        if (c == '_' || isspace(c)) {
            cap = true;
        } else if (cap) {
            str[w++] = (char)toupper(c);
            cap = false;
        } else {
            str[w++] = (char)tolower(c);
        }
    }
    str[w] = '\0';
}

/**
 * @brief Converts @p str to PascalCase in place.
 *
 * Like @ref str_camelcase but the very first letter is also uppercased
 * (@c "my_var" → @c "MyVar").
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_pascalcase(char* str) {
    if (!str || !*str) return;

    size_t r = 0;
    size_t w = 0;
    size_t len = strlen(str);

    /* Skip leading underscores / spaces */
    while (r < len && (str[r] == '_' || isspace((unsigned char)str[r]))) r++;

    bool new_word = true;
    while (r < len) {
        unsigned char c = (unsigned char)str[r++];
        if (c == '_' || isspace(c)) {
            new_word = true;
        } else {
            str[w++] = new_word ? (char)toupper(c) : (char)tolower(c);
            new_word = false;
        }
    }
    str[w] = '\0';
}

/**
 * @brief Converts @p str to Title Case in place.
 *
 * The first letter after any whitespace run is uppercased; all other
 * letters are lowercased.  Non-letter bytes are passed through unchanged.
 *
 * Example: @c "the quick BROWN fox" → @c "The Quick Brown Fox".
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_titlecase(char* str) {
    if (!str) return;
    bool cap = true;
    for (; *str; str++) {
        unsigned char c = (unsigned char)*str;
        if (isspace(c)) {
            cap = true;
        } else if (cap) {
            *str = (char)toupper(c);
            cap = false;
        } else {
            *str = (char)tolower(c);
        }
    }
}

/* =========================================================================
 * In-place trimming, reversal & removal — O(n), no allocation
 * ======================================================================= */

/**
 * @brief Removes leading ASCII whitespace from @p str in place.
 *
 * The surviving content is shifted to the beginning of the buffer via
 * @c memmove; the pointer itself is unchanged.
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_ltrim(char* str) {
    if (!str || !*str) return;
    size_t len = strlen(str);
    size_t start = 0;
    while (start < len && isspace((unsigned char)str[start])) start++;
    if (start == 0) return;
    memmove(str, str + start, len - start + 1);
}

/**
 * @brief Removes trailing ASCII whitespace from @p str in place by
 *        writing a NUL byte over the first trailing space.
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_rtrim(char* str) {
    if (!str || !*str) return;
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) len--;
    str[len] = '\0';
}

/**
 * @brief Removes both leading and trailing ASCII whitespace from @p str
 *        in place.
 *
 * Combines @ref str_rtrim and @ref str_ltrim.
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_trim(char* str) {
    if (!str || !*str) return;
    str_rtrim(str);
    str_ltrim(str);
}

/**
 * @brief Removes any leading or trailing characters that appear in
 *        @p chars from @p str in place.
 *
 * Example: @c str_trim_chars("***hello***", "*") → @c "hello".
 *
 * @param str    NUL-terminated string to modify (may be NULL → no-op).
 * @param chars  Set of characters to strip (may be NULL → no-op).
 */
static inline void str_trim_chars(char* str, const char* chars) {
    if (!str || !chars || !*chars) return;
    size_t len = strlen(str);
    size_t start = 0;

    while (start < len && strchr(chars, str[start])) start++;
    if (start == len) {
        str[0] = '\0';
        return;
    }

    size_t end = len - 1;
    while (end > start && strchr(chars, str[end])) end--;

    size_t new_len = end - start + 1;
    if (start) memmove(str, str + start, new_len);
    str[new_len] = '\0';
}

/**
 * @brief Truncates @p str to at most @p max_len bytes in place by writing
 *        a NUL byte at position @p max_len.
 *
 * If @p str is already shorter than @p max_len, it is unchanged.
 *
 * @param str      NUL-terminated string to modify (may be NULL → no-op).
 * @param max_len  Maximum number of bytes to retain.
 */
static inline void str_truncate(char* str, size_t max_len) {
    if (!str) return;
    size_t len = strlen(str);
    if (len > max_len) str[max_len] = '\0';
}

/**
 * @brief Reverses @p str in place.
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 */
static inline void str_reverse(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len < 2) return;
    for (size_t i = 0, j = len - 1; i < j; i++, j--) {
        char t = str[i];
        str[i] = str[j];
        str[j] = t;
    }
}

/**
 * @brief Removes every occurrence of the single byte @p c from @p str in
 *        place, compacting the remaining bytes.
 *
 * @param str  NUL-terminated string to modify (may be NULL → no-op).
 * @param c    The character to remove.
 */
static inline void str_remove_char(char* str, char c) {
    if (!str) return;
    char *w = str, *r = str;
    while (*r) {
        if (*r != c) *w++ = *r;
        r++;
    }
    *w = '\0';
}

/**
 * @brief Removes every non-overlapping occurrence of the substring
 *        @p substr from @p str in place.
 *
 * @param str     NUL-terminated string to modify (may be NULL → 0).
 * @param substr  Substring to remove (may be NULL or empty → 0).
 * @return        Number of occurrences removed.
 */
static inline size_t str_remove_all(char* str, const char* substr) {
    if (!str || !substr || !*substr) return 0;
    size_t sub_len = strlen(substr);
    char* w = str;
    char* r = str;
    size_t count = 0;

    while (*r) {
        if (strncmp(r, substr, sub_len) == 0) {
            r += sub_len;
            count++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    return count;
}

/**
 * @brief Removes @p slen bytes from @p str starting at byte offset
 *        @p start, in place.
 *
 * If @p start ≥ length of @p str or @p slen == 0 the string is
 * unchanged.  If @p start + @p slen exceeds the length, everything from
 * @p start to the end is removed.
 *
 * @param str    NUL-terminated string to modify (may be NULL → no-op).
 * @param start  Zero-based byte offset of the first byte to remove.
 * @param slen   Number of bytes to remove.
 */
static inline void str_remove_substr(char* str, size_t start, size_t slen) {
    if (!str || slen == 0) return;
    size_t len = strlen(str);
    if (start >= len) return;
    if (slen > len - start) slen = len - start;

    size_t tail = len - start - slen;
    if (tail > 0)
        memmove(str + start, str + start + slen, tail + 1);
    else
        str[start] = '\0';
}

/* =========================================================================
 * Allocating helpers — caller must free() every returned pointer
 * ======================================================================= */

/**
 * @brief Returns a newly allocated copy of @p str.
 *
 * Equivalent to POSIX @c strdup.  Provided here for completeness and for
 * platforms that lack @c strdup in their C standard library headers.
 *
 * @param str  NUL-terminated string to duplicate (may be NULL → @c NULL).
 * @return     Heap-allocated copy, or @c NULL on allocation failure / NULL
 *             input.  The caller must @c free() the result.
 */
static inline char* str_dup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* r = (char*)malloc(len);
    if (r) memcpy(r, str, len);
    return r;
}

/**
 * @brief Returns a newly allocated copy of at most @p n bytes of @p str,
 *        always NUL-terminated.
 *
 * Equivalent to POSIX @c strndup.
 *
 * @param str  Source string (may be NULL → @c NULL).
 * @param n    Maximum number of bytes to copy (not counting the NUL).
 * @return     Heap-allocated string of length ≤ @p n, or @c NULL.  The
 *             caller must @c free() the result.
 */
static inline char* str_ndup(const char* str, size_t n) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (n < len) len = n;
    char* r = (char*)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, str, len);
    r[len] = '\0';
    return r;
}

/**
 * @brief Extracts a substring from @p str beginning at byte @p start and
 *        spanning at most @p length bytes.
 *
 * If @p start is beyond the end of @p str, @c NULL is returned.  If
 * @p start + @p length extends past the end, the result is clamped to the
 * actual remaining length.
 *
 * @param str     Source string (may be NULL → @c NULL).
 * @param start   Zero-based byte offset of the first byte to extract.
 * @param length  Maximum number of bytes to extract.
 * @return        Heap-allocated substring, or @c NULL.  Caller must @c free().
 */
static inline char* str_substr(const char* str, size_t start, size_t length) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (start > len) return NULL;

    size_t avail = len - start;
    size_t copy = (length > avail) ? avail : length;

    char* r = (char*)malloc(copy + 1);
    if (!r) return NULL;
    memcpy(r, str + start, copy);
    r[copy] = '\0';
    return r;
}

/**
 * @brief Returns a newly allocated string consisting of @p str repeated
 *        @p n times.
 *
 * @c str_repeat("ab", 3) → @c "ababab".
 * @c str_repeat("x",  0) → @c "".
 *
 * @param str  Source string (may be NULL → @c NULL).
 * @param n    Number of repetitions.
 * @return     Heap-allocated result, or @c NULL.  Caller must @c free().
 */
static inline char* str_repeat(const char* str, size_t n) {
    if (!str) return NULL;
    size_t slen = strlen(str);
    size_t result = slen * n; /* 0 when n == 0 */

    char* r = (char*)malloc(result + 1);
    if (!r) return NULL;

    for (size_t i = 0; i < n; i++) memcpy(r + i * slen, str, slen);
    r[result] = '\0';
    return r;
}

/**
 * @brief Left-pads @p str with @p pad_char so the total width is at least
 *        @p width characters.
 *
 * If @c strlen(str) >= @p width the string is returned unchanged (duplicated).
 *
 * @param str       Source string (may be NULL → @c NULL).
 * @param width     Desired minimum total width.
 * @param pad_char  Character used for padding (typically @c ' ' or @c '0').
 * @return          Heap-allocated padded string, or @c NULL.  Caller must
 *                  @c free().
 */
static inline char* str_pad_left(const char* str, size_t width, char pad_char) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (len >= width) return str_dup(str);

    size_t pad = width - len;
    char* r = (char*)malloc(width + 1);
    if (!r) return NULL;

    memset(r, (unsigned char)pad_char, pad);
    memcpy(r + pad, str, len);
    r[width] = '\0';
    return r;
}

/**
 * @brief Right-pads @p str with @p pad_char so the total width is at least
 *        @p width characters.
 *
 * If @c strlen(str) >= @p width the string is returned unchanged (duplicated).
 *
 * @param str       Source string (may be NULL → @c NULL).
 * @param width     Desired minimum total width.
 * @param pad_char  Character used for padding (typically @c ' ').
 * @return          Heap-allocated padded string, or @c NULL.  Caller must
 *                  @c free().
 */
static inline char* str_pad_right(const char* str, size_t width, char pad_char) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (len >= width) return str_dup(str);

    size_t pad = width - len;
    char* r = (char*)malloc(width + 1);
    if (!r) return NULL;

    memcpy(r, str, len);
    memset(r + len, (unsigned char)pad_char, pad);
    r[width] = '\0';
    return r;
}

/**
 * @brief Centers @p str within a field of @p width characters, padding both
 *        sides with @p pad_char.
 *
 * When the padding cannot be split evenly, the extra character goes on the
 * right (matching Python's @c str.center behaviour).
 *
 * @param str       Source string (may be NULL → @c NULL).
 * @param width     Desired total width.
 * @param pad_char  Character used for padding.
 * @return          Heap-allocated centered string, or @c NULL.  Caller must
 *                  @c free().
 */
static inline char* str_center(const char* str, size_t width, char pad_char) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (len >= width) return str_dup(str);

    size_t total_pad = width - len;
    size_t left_pad = total_pad / 2;
    size_t right_pad = total_pad - left_pad;

    char* r = (char*)malloc(width + 1);
    if (!r) return NULL;

    memset(r, (unsigned char)pad_char, left_pad);
    memcpy(r + left_pad, str, len);
    memset(r + left_pad + len, (unsigned char)pad_char, right_pad);
    r[width] = '\0';
    return r;
}

/**
 * @brief Converts a string from camelCase or PascalCase to snake_case.
 *
 * An underscore is inserted before each uppercase letter (which is then
 * lowercased).  The leading character is never preceded by an underscore.
 *
 * @c str_to_snakecase("myVarName")  → @c "my_var_name"
 * @c str_to_snakecase("HTTPServer") → @c "http_server"
 *
 * @param str  NUL-terminated source string (may be NULL → @c NULL).
 * @return     Heap-allocated snake_case string, or @c NULL.  Caller must
 *             @c free().
 */
static inline char* str_to_snakecase(const char* str) {
    if (!str) return NULL;
    size_t orig = strlen(str);
    if (orig == 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    /* Pass 1: Count how many underscores we need to insert */
    size_t extra = 0;
    for (size_t i = 1; i < orig; i++) {
        unsigned char c = (unsigned char)str[i];
        if ((unsigned)(c - 'A') <= 25u) {  // If current is uppercase
            unsigned char prev = (unsigned char)str[i - 1];
            unsigned char next = (i + 1 < orig) ? (unsigned char)str[i + 1] : '\0';

            bool prev_is_lower = (unsigned)(prev - 'a') <= 25u;
            bool next_is_lower = (unsigned)(next - 'a') <= 25u;

            // Insert '_' if transitioning from lower->UPPER or UPPER->UPPER->lower
            if (prev != '_' && (prev_is_lower || next_is_lower)) {
                extra++;
            }
        }
    }

    char* r = (char*)malloc(orig + extra + 1);
    if (!r) return NULL;

    /* Pass 2: Build the string */
    size_t w = 0;
    for (size_t i = 0; i < orig; i++) {
        unsigned char c = (unsigned char)str[i];
        if ((unsigned)(c - 'A') <= 25u) {  // If current is uppercase
            if (i > 0) {
                unsigned char prev = (unsigned char)str[i - 1];
                unsigned char next = (i + 1 < orig) ? (unsigned char)str[i + 1] : '\0';

                bool prev_is_lower = (unsigned)(prev - 'a') <= 25u;
                bool next_is_lower = (unsigned)(next - 'a') <= 25u;

                if (prev != '_' && (prev_is_lower || next_is_lower)) {
                    r[w++] = '_';
                }
            }
            r[w++] = (char)(c | 0x20u);  // Convert to lowercase
        } else {
            r[w++] = (char)c;  // Keep as is
        }
    }
    r[w] = '\0';

    return r;
}

/**
 * @brief Replaces the first occurrence of @p old_str with @p new_str and
 *        returns the result as a new heap-allocated string.
 *
 * If @p old_str is not found, a duplicate of @p str is returned.
 *
 * @param str      Source string (may be NULL → @c NULL).
 * @param old_str  Substring to search for (may be NULL → duplicate of @p str).
 * @param new_str  Replacement string   (may be NULL → duplicate of @p str).
 * @return         Heap-allocated result, or @c NULL.  Caller must @c free().
 */
static inline char* str_replace(const char* str, const char* old_str, const char* new_str) {
    if (!str) return NULL;
    if (!old_str || !new_str) return str_dup(str);

    size_t hlen = strlen(str);
    size_t old_len = strlen(old_str);
    if (old_len == 0) return str_dup(str);

    const char* found = str_search_impl(str, hlen, old_str, old_len);
    if (!found) return str_dup(str);

    size_t new_len = strlen(new_str);
    size_t prefix_len = (size_t)(found - str);
    size_t suffix_len = hlen - prefix_len - old_len;
    size_t result_len = prefix_len + new_len + suffix_len;

    char* r = (char*)malloc(result_len + 1);
    if (!r) return NULL;

    memcpy(r, str, prefix_len);
    memcpy(r + prefix_len, new_str, new_len);
    memcpy(r + prefix_len + new_len, found + old_len, suffix_len);
    r[result_len] = '\0';
    return r;
}

/**
 * @brief Replaces every non-overlapping occurrence of @p old_sub with
 *        @p new_sub and returns the result as a new heap-allocated string.
 *
 * Uses a two-pass algorithm (collect offsets, then build result) that keeps
 * allocations to a minimum.  Offset storage starts on the stack (64 slots)
 * and spills to the heap only when needed.
 *
 * @param str      Source string (may be NULL → @c NULL).
 * @param old_sub  Substring to replace (may be NULL → duplicate of @p str).
 * @param new_sub  Replacement string   (may be NULL → duplicate of @p str).
 * @return         Heap-allocated result, or @c NULL.  Caller must @c free().
 */
static inline char* str_replace_all(const char* str, const char* old_sub, const char* new_sub) {
    if (!str) return NULL;
    size_t hlen = strlen(str);
    if (!old_sub || !new_sub) return str_dup(str);

    size_t old_len = strlen(old_sub);
    if (old_len == 0) return str_dup(str);

    size_t new_len = strlen(new_sub);

#define STR_RA_STACK_CAP 64
    size_t stack_offs[STR_RA_STACK_CAP];
    size_t* offs = stack_offs;
    size_t offs_cap = STR_RA_STACK_CAP;
    size_t count = 0;

    const char* p = str;
    size_t rem = hlen;

    /* Pass 1 – collect match offsets */
    while ((p = str_search_impl(p, rem, old_sub, old_len)) != NULL) {
        if (count >= offs_cap) {
            if (offs_cap > SIZE_MAX / 2 / sizeof(size_t)) goto oom;
            size_t new_cap = offs_cap * 2;
            size_t* no;
            if (offs == stack_offs) {
                no = (size_t*)malloc(new_cap * sizeof(size_t));
                if (!no) goto oom;
                memcpy(no, stack_offs, count * sizeof(size_t));
            } else {
                no = (size_t*)realloc(offs, new_cap * sizeof(size_t));
                if (!no) goto oom;
            }
            offs = no;
            offs_cap = new_cap;
        }
        offs[count++] = (size_t)(p - str);
        p += old_len;
        rem = hlen - (size_t)(p - str);
    }

    if (count == 0) {
        if (offs != stack_offs) free(offs);
        return str_dup(str);
    }

    /* Pass 2 – build result */
    size_t result_len;
    if (new_len >= old_len)
        result_len = hlen + count * (new_len - old_len);
    else
        result_len = hlen - count * (old_len - new_len);

    char* r = (char*)malloc(result_len + 1);
    if (!r) goto oom;

    size_t wp = 0, sp = 0;
    for (size_t i = 0; i < count; i++) {
        size_t gap = offs[i] - sp;
        if (gap) {
            memcpy(r + wp, str + sp, gap);
            wp += gap;
        }
        if (new_len) {
            memcpy(r + wp, new_sub, new_len);
            wp += new_len;
        }
        sp = offs[i] + old_len;
    }
    size_t tail = hlen - sp;
    if (tail) {
        memcpy(r + wp, str + sp, tail);
        wp += tail;
    }
    r[wp] = '\0';

    if (offs != stack_offs) free(offs);
    return r;

oom:
    if (offs != stack_offs) free(offs);
    return NULL;
#undef STR_RA_STACK_CAP
}

/**
 * @brief Splits @p str on every occurrence of @p delim and returns a
 *        NULL-terminated array of heap-allocated token strings.
 *
 * The number of tokens is stored in @c *count_out.  The returned array
 * must be freed by calling @ref str_free_split.
 *
 * Splitting @c "a::b" on @c "::" yields @c {"a", "b", NULL}.
 * Splitting @c ""     on @c ","  yields @c {"",  NULL}.
 * A NULL or empty delimiter yields an array containing a single copy of
 * the entire string.
 *
 * @param str        Haystack string (may be NULL → @c NULL).
 * @param delim      Delimiter string (may be NULL or empty → single token).
 * @param count_out  Receives the number of tokens; must not be NULL.
 * @return           NULL-terminated @c char** array, or @c NULL on failure.
 *                   Free with @ref str_free_split.
 */
static inline char** str_split(const char* str, const char* delim, size_t* count_out) {
    if (!count_out) return NULL;
    *count_out = 0;
    if (!str) return NULL;

    /* No delimiter → single token */
    if (!delim || !*delim) {
        char** r = (char**)malloc(2 * sizeof(char*));
        if (!r) return NULL;
        r[0] = str_dup(str);
        if (!r[0]) {
            free(r);
            return NULL;
        }
        r[1] = NULL;
        *count_out = 1;
        return r;
    }

    size_t dlen = strlen(delim);
    size_t cap = 8;
    char** result = (char**)malloc(cap * sizeof(char*));
    if (!result) return NULL;

    const char* start = str;
    const char* end = str + strlen(str);
    size_t count = 0;

    for (;;) {
        const char* found = str_search_impl(start, (size_t)(end - start), delim, dlen);
        const char* tok_end = found ? found : end;

        /* Keep one extra slot for the NULL terminator */
        if (count + 1 >= cap) {
            cap *= 2;
            char** tmp = (char**)realloc(result, cap * sizeof(char*));
            if (!tmp) goto split_err;
            result = tmp;
        }

        size_t tok_len = (size_t)(tok_end - start);
        result[count] = (char*)malloc(tok_len + 1);
        if (!result[count]) goto split_err;
        memcpy(result[count], start, tok_len);
        result[count][tok_len] = '\0';
        count++;

        if (!found) break;
        start = found + dlen;
    }

    result[count] = NULL;
    *count_out = count;
    return result;

split_err:
    for (size_t i = 0; i < count; i++) free(result[i]);
    free(result);
    return NULL;
}

/**
 * @brief Frees a split array returned by @ref str_split.
 *
 * Iterates the NULL-terminated array, frees each token, then frees the
 * array itself.  Passing @c NULL is a no-op.
 *
 * @param parts  NULL-terminated array of strings returned by @ref str_split.
 */
static inline void str_free_split(char** parts) {
    if (!parts) return;
    for (size_t i = 0; parts[i]; i++) free(parts[i]);
    free(parts);
}

/**
 * @brief Joins @p count strings from @p strings with @p delim between each
 *        adjacent pair and returns the result as a new heap-allocated string.
 *
 * @param strings  Array of NUL-terminated strings.  Must have at least
 *                 @p count elements; no element may be @c NULL.
 * @param count    Number of strings in @p strings.
 * @param delim    Delimiter placed between elements (may be @c NULL → no
 *                 separator).
 * @return         Heap-allocated joined string, or @c NULL.  Caller must
 *                 @c free().
 */
static inline char* str_join(const char** strings, size_t count, const char* delim) {
    if (!strings || count == 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t dlen = delim ? strlen(delim) : 0;
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (!strings[i]) return NULL;
        total += strlen(strings[i]);
        if (i + 1 < count) total += dlen;
    }

    char* r = (char*)malloc(total + 1);
    if (!r) return NULL;

    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]);
        if (len) {
            memcpy(r + pos, strings[i], len);
            pos += len;
        }
        if (dlen && i + 1 < count) {
            memcpy(r + pos, delim, dlen);
            pos += dlen;
        }
    }
    r[pos] = '\0';
    return r;
}

/**
 * @brief Concatenates a NULL-terminated list of strings into a new
 *        heap-allocated string.
 *
 * Usage:
 * @code
 *   char *s = str_concat("Hello", ", ", "world", "!", NULL);
 *   // s == "Hello, world!"
 *   free(s);
 * @endcode
 *
 * @param first  First string to concatenate (may be @c NULL → empty string).
 * @param ...    Additional strings, terminated by a final @c NULL sentinel.
 * @return       Heap-allocated result, or @c NULL on allocation failure.
 *               Caller must @c free().
 */
static inline char* str_concat(const char* first, ...) {
    /* Pass 1 – measure total length */
    size_t total = 0;
    {
        va_list ap;
        va_start(ap, first);
        const char* s = first;
        while (s) {
            total += strlen(s);
            s = va_arg(ap, const char*);
        }
        va_end(ap);
    }

    char* r = (char*)malloc(total + 1);
    if (!r) return NULL;

    /* Pass 2 – copy */
    size_t pos = 0;
    {
        va_list ap;
        va_start(ap, first);
        const char* s = first;
        while (s) {
            size_t len = strlen(s);
            memcpy(r + pos, s, len);
            pos += len;
            s = va_arg(ap, const char*);
        }
        va_end(ap);
    }
    r[pos] = '\0';
    return r;
}

/**
 * @brief Computes the FNV-1a 32-bit hash of @p str.
 *
 * Fast, well-distributed, non-cryptographic hash suitable for use in hash
 * tables.  Returns 0 for a NULL input.
 *
 * @param str  NUL-terminated string to hash (may be NULL → 0).
 * @return     32-bit hash value.
 */
static inline uint32_t str_hash(const char* str) {
    if (!str) return 0u;
    uint32_t h = 2166136261u; /* FNV offset basis */
    for (; *str; str++) {
        h ^= (unsigned char)*str;
        h *= 16777619u; /* FNV prime */
    }
    return h;
}

/* =========================================================================
 * Number -> string conversions
 * ======================================================================= */

/**
 * @brief Converts the integer @p value to its decimal string representation,
 *        writing into the caller-supplied buffer @p buf.
 *
 * @param value   Value to convert.
 * @param buf     Destination buffer; must not be NULL.
 * @param buflen  Size of @p buf in bytes.  12 bytes is sufficient for any
 *                32-bit @c int ("-2147483648\0"); 24 bytes covers 64-bit.
 * @return        @p buf on success, @c NULL if @p buf is NULL, @p buflen is
 *                zero, or the formatted value would not fit.
 */
static inline char* str_from_int(int value, char* buf, size_t buflen) {
    if (!buf || buflen == 0) return NULL;
    int need = snprintf(buf, buflen, "%d", value);
    if (need < 0 || (size_t)need >= buflen) return NULL;
    return buf;
}

/**
 * @brief Converts the @c long @p value to its decimal string representation,
 *        writing into the caller-supplied buffer @p buf.
 *
 * @param value   Value to convert.
 * @param buf     Destination buffer; must not be NULL.
 * @param buflen  Size of @p buf in bytes.  24 bytes is sufficient for any
 *                64-bit @c long ("-9223372036854775808\0").
 * @return        @p buf on success, @c NULL if @p buf is NULL, @p buflen is
 *                zero, or the formatted value would not fit.
 */
static inline char* str_from_long(long value, char* buf, size_t buflen) {
    if (!buf || buflen == 0) return NULL;
    int need = snprintf(buf, buflen, "%ld", value);
    if (need < 0 || (size_t)need >= buflen) return NULL;
    return buf;
}

/**
 * @brief Converts the @c double @p value to a string with @p precision
 *        decimal places, writing into the caller-supplied buffer @p buf.
 *
 * @param value      Value to convert.
 * @param precision  Digits after the decimal point; negative values default
 *                   to 6, values above 64 are clamped to 64.
 * @param buf        Destination buffer; must not be NULL.
 * @param buflen     Size of @p buf in bytes.  128 bytes is sufficient for
 *                   any finite double at up to 64 decimal places.
 * @return           @p buf on success, @c NULL if @p buf is NULL, @p buflen
 *                   is zero, or the formatted value would not fit.
 */
static inline char* str_from_double(double value, int precision, char* buf, size_t buflen) {
    if (!buf || buflen == 0) return NULL;
    if (precision < 0) precision = 6;
    if (precision > 64) precision = 64;
    int need = snprintf(buf, buflen, "%.*f", precision, value);
    if (need < 0 || (size_t)need >= buflen) return NULL;
    return buf;
}

/* =========================================================================
 * Legacy / platform helpers
 * ======================================================================= */

#if defined(_MSC_VER)

/**
 * @brief Case-insensitive string comparison (MSVC shim for POSIX
 *        @c strcasecmp).
 */
static inline int strcasecmp(const char* s1, const char* s2) {
    if (s1 == s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return _stricmp(s1, s2);
}

/**
 * @brief Case-insensitive bounded string comparison (MSVC shim for POSIX
 *        @c strncasecmp).
 */
static inline int strncasecmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;
    if (s1 == s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return _strnicmp(s1, s2, n);
}

/**
 * @brief Case-insensitive substring search (MSVC shim for GNU @c strcasestr).
 *
 * @param haystack  String to search in.
 * @param needle    Substring to search for.
 * @return          Pointer to first case-insensitive match, or @c NULL.
 */
static inline char* strcasestr(const char* haystack, const char* needle) {
    if (!needle || *needle == '\0') return (char*)haystack;
    if (!haystack) return NULL;
    const size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncasecmp(haystack, needle, nlen) == 0) return (char*)haystack;
        haystack++;
    }
    return NULL;
}
#endif /* _MSC_VER */

#ifdef __cplusplus
}
#endif

#endif /* __STR_H__ */
