#ifndef STR_BUILDER_H
#define STR_BUILDER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
Memory efficient string builder.

It is a flexible array member(FAM) based string builder that grows as needed.
It is a simple implementation and does not support shrinking the string.
You should allocate enough memory to avoid frequent reallocations.
*/
typedef struct {
    size_t len;   // length of the string
    size_t cap;   // capacity of the string
    char data[];  // flexible array member containing the string data
} string_builder;

string_builder* sb_alloc(size_t cap);
void sb_free(const string_builder* str);
bool sb_append(string_builder* sb, const char* s);
bool sb_append_char(string_builder* sb, char c);
bool sb_append_int(string_builder* str, int n);
bool sb_append_float(string_builder* str, float f);
bool sb_append_double(string_builder* str, double d);
void sb_clear(string_builder* str);
const char* sb_cstr(const string_builder* str);
size_t sb_len(const string_builder* str);
size_t sb_cap(const string_builder* str);

#if defined(__cplusplus)
}
#endif

#ifdef STR_BUILDER_IMPL
/**
 * Allocates a new string builder with the given capacity.
 * @param cap The initial capacity of the string builder.
 * @return A pointer to the newly allocated string builder or NULL on failure.
 */
string_builder* sb_alloc(size_t cap) {
    string_builder* str = (string_builder*)malloc(sizeof(string_builder) + cap);
    if (str == NULL) {
        perror("malloc");
        return NULL;
    }

    str->len     = 0;
    str->cap     = cap;
    str->data[0] = '\0';
    return str;
}

/**
 * Frees the memory allocated for the string builder.
 * @param str The string builder to free.
 */
void sb_free(const string_builder* str) {
    if (str) {
        free((void*)str);
        str = NULL;
    }
}

/**
 * Grows the string builder to the new capacity.
 * @param str The string builder to grow.
 * @param new_cap The new capacity to grow to.
 * @return true if the string builder was grown successfully, false otherwise.
 */
static bool sb_grow(string_builder** str, size_t new_cap) {
    if (new_cap <= (*str)->cap) {
        return true;  // no need to grow, not an error
    }

    // re-alllocate the string builder with the new capacity
    string_builder* new_str = (string_builder*)realloc(*str, sizeof(string_builder) + new_cap);
    if (new_str == NULL) {
        perror("realloc");
        return false;
    }

    new_str->cap = new_cap;
    *str         = new_str;
    return true;
}

/**
 * Appends the given string to the string builder.
 * @param sb The string builder to append to.
 * @param s The string to append.
 * @return true if the string was appended successfully, false otherwise.
 */
bool sb_append(string_builder* sb, const char* s) {
    size_t len = strlen(s);
    if (sb_grow(&sb, sb->len + len + 1)) {
        memcpy(sb->data + sb->len, s, len);
        sb->len += len;
        sb->data[sb->len] = '\0';
        return true;
    } else {
        fprintf(stderr, "sb_grow failed\n");
        return false;
    }
}

/**
 * Appends the given character to the string builder.
 * @param sb The string builder to append to.
 * @param c The character to append.
 * @return true if the character was appended successfully, false otherwise.
 */
bool sb_append_char(string_builder* sb, char c) {
    if (sb_grow(&sb, sb->len + 1)) {
        sb->data[sb->len++] = c;
        sb->data[sb->len]   = '\0';
        return true;
    } else {
        fprintf(stderr, "sb_grow failed\n");
        return false;
    }
}

/**
 * Appends the given integer to the string builder.
 * @param str The string builder to append to.
 * @param n The integer to append.
 * @return true if the integer was appended successfully, false otherwise.
 */
bool sb_append_int(string_builder* str, int n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", n);
    return sb_append(str, buf);
}

/**
 * Appends the given float to the string builder.
 * @param str The string builder to append to.
 * @param f The float to append.
 * @return true if the float was appended successfully, false otherwise.
 */
bool sb_append_float(string_builder* str, float f) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%f", f);
    return sb_append(str, buf);
}

/**
 * Appends the given double to the string builder.
 * @param str The string builder to append to.
 * @param d The double to append.
 * @return true if the double was appended successfully, false otherwise.
 */
bool sb_append_double(string_builder* str, double d) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lf", d);
    return sb_append(str, buf);
}

void sb_clear(string_builder* str) {
    str->len     = 0;
    str->data[0] = '\0';
}

const char* sb_cstr(const string_builder* str) {
    return str->data;
}

size_t sb_len(const string_builder* str) {
    return str->len;
}

size_t sb_cap(const string_builder* str) {
    return str->cap;
}

#endif  // STR_BUILDER_IMPL

#endif  // STR_BUILDER_H