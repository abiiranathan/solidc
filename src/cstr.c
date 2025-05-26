#include "../include/cstr.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A dynamically resizable C-string.
typedef struct cstr {
    size_t length;    // The length of the string
    size_t capacity;  // The capacity of the string
    char data[];      // The string data as a flexible array member
} cstr;

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

size_t str_round_capacity(size_t capacity) {
    size_t c = STR_MIN_CAPACITY;
    while (c < capacity) {
        c = (size_t)((double)c * STR_RESIZE_FACTOR);
    }
    return c;
}

cstr* str_new(size_t capacity) {
    capacity = str_round_capacity(MAX(capacity, 1));
    cstr* s  = malloc(sizeof(cstr) + capacity);
    if (s) {
        s->length   = 0;
        s->capacity = capacity;
        s->data[0]  = '\0';
    }
    return s;
}

cstr* str_from(const char* s) {
    if (!s)
        return NULL;

    size_t len = strlen(s);
    cstr* str  = str_new(len + 1);
    if (str) {
        memcpy(str->data, s, len + 1);
        str->length = len;
    }
    return str;
}

cstr* str_format(const char* format, ...) {
    va_list args;
    va_start(args, format);

    // Get the required size for the formatted string
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return NULL;
    }

    cstr* str = str_new((size_t)size + 1);
    if (!str) {
        va_end(args);
        return NULL;
    }

    // Append the formatted string
    int written = vsnprintf(str->data, str->capacity, format, args);
    va_end(args);

    if (written < 0 || (size_t)written >= str->capacity) {
        // Handle error or truncation
        str->length            = str->capacity - 1;
        str->data[str->length] = '\0';
    } else {
        str->length = (size_t)written;
    }

    return str;
}

inline void str_free(cstr* s) {
    if (s) {
        free(s);
    }
}

inline size_t str_len(const cstr* s) {
    return s ? s->length : 0;
}

inline size_t str_capacity(const cstr* s) {
    return s ? s->capacity : 0;
}

bool str_empty(const cstr* s) {
    return !s || s->length == 0;
}

bool str_resize(cstr** s, size_t capacity) {
    if (!s || !*s) {
        return false;
    }

    if ((*s)->capacity >= capacity) {
        return true;
    }

    capacity    = str_round_capacity(capacity);
    cstr* new_s = realloc(*s, sizeof(cstr) + capacity);
    if (!new_s) {
        return false;
    }
    new_s->capacity = capacity;
    *s              = new_s;
    return true;
}

bool str_append(cstr** s, const char* append) {
    if (!s || !*s || !append)
        return false;

    size_t append_len = strlen(append);
    if (!str_resize(s, (*s)->length + append_len + 1))
        return false;

    // Append the string and null-terminator
    memcpy((*s)->data + (*s)->length, append, append_len + 1);
    (*s)->length += append_len;
    return true;
}

// Append a C string to end end of string without capacity checks.
// This is faster if you know for sure that you already have enough capacity.
// Forexample after calling str_resize.
bool str_append_fast(cstr** s, const char* append) {
    if (!s || !*s || !append)
        return false;

    size_t append_len = strlen(append);
    // Append the string and null-terminator
    memcpy((*s)->data + (*s)->length, append, append_len + 1);
    (*s)->length += append_len;
    return true;
}

bool str_append_fmt(cstr** s, const char* format, ...) {
    va_list args;
    va_start(args, format);

    // Get the required size for the formatted string
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (size < 0)
        return false;

    if (!str_resize(s, (*s)->length + size + 1))
        return false;

    // Append the formatted string
    int written = vsnprintf((*s)->data + (*s)->length, size + 1, format, args);
    va_end(args);

    if (written < 0 || written >= size) {
        // Handle error or truncation
        (*s)->length += size;
        (*s)->data[(*s)->length] = '\0';
    } else {
        (*s)->length += written;
    }

    return true;
}

bool str_append_char(cstr** s, char c) {
    if (!s || !*s)
        return false;

    if (!str_resize(s, (*s)->length + 2))
        return false;

    (*s)->data[(*s)->length++] = c;
    (*s)->data[(*s)->length]   = '\0';
    return true;
}

bool str_prepend(cstr** s, const char* prepend) {
    if (!s || !*s || !prepend)
        return false;

    size_t prepend_len = strlen(prepend);
    if (!str_resize(s, (*s)->length + prepend_len + 1))
        return false;

    memmove((*s)->data + prepend_len, (*s)->data, (*s)->length + 1);
    memcpy((*s)->data, prepend, prepend_len);
    (*s)->length += prepend_len;
    return true;
}

bool str_prepend_fast(cstr** s, const char* prepend) {
    if (!s || !*s || !prepend)
        return false;

    size_t prepend_len = strlen(prepend);
    memmove((*s)->data + prepend_len, (*s)->data, (*s)->length + 1);
    memcpy((*s)->data, prepend, prepend_len);
    (*s)->length += prepend_len;
    return true;
}

bool str_insert(cstr** s, size_t index, const char* insert) {
    if (!s || !*s || !insert || index > (*s)->length)
        return false;

    size_t insert_len = strlen(insert);
    if (!str_resize(s, (*s)->length + insert_len + 1))
        return false;

    memmove((*s)->data + index + insert_len, (*s)->data + index, (*s)->length - index + 1);
    memcpy((*s)->data + index, insert, insert_len);
    (*s)->length += insert_len;
    return true;
}

bool str_remove(cstr** s, size_t index, size_t count) {
    if (!s || !*s || index >= (*s)->length)
        return false;

    count = MIN(count, (*s)->length - index);
    memmove((*s)->data + index, (*s)->data + index + count, (*s)->length - index - count + 1);
    (*s)->length -= count;
    return true;
}

inline void str_clear(cstr* s) {
    if (s) {
        s->length  = 0;
        s->data[0] = '\0';
    }
}

size_t str_remove_all(cstr** s, const char* substr) {
    size_t substr_len = strlen(substr);
    char* read        = (*s)->data;
    char* write       = (*s)->data;
    size_t count      = 0;

    while (*read) {
        if (strncmp(read, substr, substr_len) == 0) {
            read += substr_len;
            count++;
        } else {
            *write++ = *read++;
        }
    }
    *write       = '\0';
    (*s)->length = write - (*s)->data;
    return count;
}

inline char str_at(const cstr* s, size_t index) {
    return (s && index < s->length) ? s->data[index] : '\0';
}

inline char* str_data(cstr* s) {
    return s ? s->data : NULL;
}

// Get a string view of the data.
str_view str_as_view(const cstr* s) {
    str_view view = {NULL, 0};
    if (s) {
        view.data   = s->data;
        view.length = s->length;
    }
    return view;
}

int str_compare(const cstr* s1, const cstr* s2) {
    if (!s1 || !s2)
        return s1 == s2 ? 0 : (s1 ? 1 : -1);
    return strcmp(s1->data, s2->data);
}

bool str_equals(const cstr* s1, const cstr* s2) {
    return str_compare(s1, s2) == 0;
}

bool str_starts_with(const cstr* s, const char* prefix) {
    if (!s || !prefix)
        return false;
    size_t prefix_len = strlen(prefix);
    return s->length >= prefix_len && memcmp(s->data, prefix, prefix_len) == 0;
}

bool str_ends_with(const cstr* s, const char* suffix) {
    if (!s || !suffix)
        return false;
    size_t suffix_len = strlen(suffix);
    return s->length >= suffix_len && memcmp(s->data + s->length - suffix_len, suffix, suffix_len) == 0;
}

int str_find(const cstr* s, const char* substr) {
    if (!s || !substr)
        return STR_NPOS;
    char* found = strstr(s->data, substr);
    return found ? (int)(found - s->data) : STR_NPOS;
}

int str_rfind(const cstr* s, const char* substr) {
    if (!s || !substr || !*substr)
        return STR_NPOS;

    size_t substr_len = strlen(substr);
    if (substr_len > s->length)
        return STR_NPOS;

    for (size_t i = s->length - substr_len + 1; i > 0; --i) {
        if (memcmp(s->data + i - 1, substr, substr_len) == 0) {
            return (int)(i - 1);
        }
    }
    return STR_NPOS;
}

void str_to_lower(cstr* s) {
    if (!s)
        return;
    for (size_t i = 0; i < s->length; ++i) {
        s->data[i] = (char)tolower(s->data[i]);
    }
}

void str_to_upper(cstr* s) {
    if (!s)
        return;
    for (size_t i = 0; i < s->length; ++i) {
        s->data[i] = (char)toupper(s->data[i]);
    }
}

void str_snake_case(cstr* s) {
    if (!s)
        return;
    for (size_t i = 0; i < s->length; ++i) {
        if (isupper(s->data[i])) {
            s->data[i] = (char)tolower(s->data[i]);
            if (i > 0) {
                str_insert(&s, i, "_");
                ++i;
            }
        }
    }
}

void str_camel_case(cstr* s) {
    if (!s || s->length == 0) {
        return;
    }

    size_t read = 0, write = 0;
    bool capitalize_next = false;

    // First character should be lowercase
    s->data[write++] = (char)tolower(s->data[read++]);

    while (read < s->length) {
        char c = s->data[read++];

        if (c == ' ' || c == '_') {
            capitalize_next = true;
        } else if (capitalize_next) {
            s->data[write++] = (char)toupper(c);
            capitalize_next  = false;
        } else {
            s->data[write++] = (char)tolower(c);
        }
    }

    s->data[write] = '\0';
    s->length      = write;
}

void str_pascal_case(cstr* s) {
    if (!s || s->length == 0) {
        return;
    }

    size_t read = 0, write = 0;

    // Start with true to capitalize the first letter
    bool new_word = true;

    while (read < s->length) {
        char c = s->data[read++];

        if (c == ' ' || c == '_') {
            new_word = true;
        } else if (new_word) {
            s->data[write++] = (char)toupper(c);
            new_word         = false;
        } else if (isupper(c) && read < s->length && islower(s->data[read])) {
            // If current char is uppercase and next char is lowercase,
            // it's the start of a new word in camelCase
            s->data[write++] = c;
            new_word         = false;
        } else {
            s->data[write++] = (char)tolower(c);
        }
    }

    s->data[write] = '\0';
    s->length      = write;
}

void str_title_case(cstr* str) {
    int capitalize = 1;
    for (size_t i = 0; i < str->length; i++) {
        if (str->data[i] == ' ') {
            capitalize = 1;
        } else if (capitalize) {
            str->data[i] = (char)toupper(str->data[i]);
            capitalize   = 0;
        } else {
            str->data[i] = (char)tolower(str->data[i]);
        }
    }
}

void str_trim(cstr* s) {
    if (!s || s->length == 0)
        return;

    size_t start = 0, end = s->length - 1;
    while (start < s->length && isspace(s->data[start]))
        ++start;
    while (end > start && isspace(s->data[end]))
        --end;

    s->length = end - start + 1;
    memmove(s->data, s->data + start, s->length);
    s->data[s->length] = '\0';
}

void str_rtrim(cstr* s) {
    if (!s || s->length == 0)
        return;

    size_t end = s->length - 1;
    while (end > 0 && isspace(s->data[end]))
        --end;

    s->length          = end + 1;
    s->data[s->length] = '\0';
}

void str_ltrim(cstr* s) {
    if (!s || s->length == 0)
        return;

    size_t start = 0;
    while (start < s->length && isspace(s->data[start]))
        ++start;

    s->length -= start;
    memmove(s->data, s->data + start, s->length);
    s->data[s->length] = '\0';
}

void str_trim_chars(cstr* str, const char* chars) {
    // Remove leading characters
    char* start = str->data;
    while (strchr(chars, *start)) {
        start++;
    }

    // Remove trailing characters
    char* end = str->data + str->length - 1;
    while (strchr(chars, *end)) {
        end--;  // move the pointer to the left
    }

    size_t len = (size_t)(end - start + 1);  // length of the remaining string
    memmove(str->data, start,
            len);  // move the remaining string to the beginning
    str->data[len] = '\0';
}

// Count the number of occurrences of a substring within the string.
size_t str_count_substr(const cstr* str, const char* substr) {
    size_t count   = 0;
    size_t sub_len = strlen(substr);
    const char* p  = str->data;
    while ((p = strstr(p, substr))) {
        count++;
        p += sub_len;
    }
    return count;
}

// Remove all occurrences of character c from str.
void str_remove_char(cstr* str, char c) {
    char* dest = str->data;
    for (char* src = str->data; *src != '\0'; src++) {
        if (*src != c) {
            *dest = *src;
            dest++;
        }
    }
    *dest = '\0';
}

// Remove characters in str from start index, up to start + length.
void str_remove_substr(cstr* str, size_t start, size_t substr_length) {
    if (start >= str->length) {
        return;
    }

    if (start + substr_length > str->length) {
        substr_length = str->length - start;
    }

    memmove(str->data + start, str->data + start + substr_length, str->length - start - substr_length + 1);
    str->length -= substr_length;
}

cstr* str_substr(const cstr* s, size_t start, size_t length) {
    if (!s || start >= s->length)
        return NULL;

    length       = MIN(length, s->length - start);
    cstr* result = str_new(length + 1);
    if (result) {
        memcpy(result->data, s->data + start, length);
        result->data[length] = '\0';
        result->length       = length;
    }
    return result;
}

cstr* str_replace(const cstr* s, const char* old, const char* new_str) {
    if (!s || !old || !new_str)
        return NULL;

    size_t old_len = strlen(old);
    size_t new_len = strlen(new_str);
    size_t count   = 0;
    const char* p  = s->data;

    while ((p = strstr(p, old))) {
        ++count;
        p += old_len;
    }

    size_t result_len = s->length + count * (new_len - old_len);
    cstr* result      = str_new(result_len + 1);
    if (!result)
        return NULL;

    char* dest = result->data;
    p          = s->data;

    while (*p) {
        if (strncmp(p, old, old_len) == 0) {
            memcpy(dest, new_str, new_len);
            dest += new_len;
            p += old_len;
        } else {
            *dest++ = *p++;
        }
    }

    *dest          = '\0';
    result->length = result_len;
    return result;
}

cstr* str_replace_all(const cstr* s, const char* old, const char* new_str) {
    if (!s || !old || !new_str)
        return NULL;

    size_t old_len = strlen(old);
    size_t new_len = strlen(new_str);
    size_t count   = 0;
    const char* p  = s->data;

    while ((p = strstr(p, old))) {
        ++count;
        p += old_len;
    }

    size_t result_len = s->length + count * (new_len - old_len);
    cstr* result      = str_new(result_len + 1);
    if (!result)
        return NULL;

    char* dest = result->data;
    p          = s->data;

    while (*p) {
        if (strncmp(p, old, old_len) == 0) {
            memcpy(dest, new_str, new_len);
            dest += new_len;
            p += old_len;
        } else {
            *dest++ = *p++;
        }
    }

    *dest          = '\0';
    result->length = result_len;
    return result;
}

cstr** str_split(const cstr* s, const char* delim, size_t* count) {
    if (!s || !delim || !count)
        return NULL;

    size_t delim_len  = strlen(delim);
    size_t max_splits = s->length / delim_len + 1;
    cstr** result     = malloc(max_splits * sizeof(cstr*));
    if (!result)
        return NULL;

    *count            = 0;
    const char* start = s->data;
    const char* end   = s->data;

    while ((end = strstr(start, delim))) {
        size_t len     = end - start;
        result[*count] = str_new(len + 1);
        if (!result[*count]) {
            for (size_t i = 0; i < *count; ++i)
                str_free(result[i]);
            free(result);
            return NULL;
        }
        memcpy(result[*count]->data, start, len);
        result[*count]->data[len] = '\0';
        result[*count]->length    = len;
        ++(*count);
        start = end + delim_len;
    }

    size_t len     = s->data + s->length - start;
    result[*count] = str_new(len + 1);
    if (!result[*count]) {
        for (size_t i = 0; i < *count; ++i)
            str_free(result[i]);
        free(result);
        return NULL;
    }
    memcpy(result[*count]->data, start, len);
    result[*count]->data[len] = '\0';
    result[*count]->length    = len;
    ++(*count);

    return result;
}

cstr* str_join(const cstr** strings, size_t count, const char* delim) {
    if (!strings || count == 0 || !delim)
        return NULL;

    size_t total_len = 0;
    size_t delim_len = strlen(delim);

    for (size_t i = 0; i < count; ++i) {
        if (!strings[i])
            return NULL;
        total_len += strings[i]->length;
    }
    total_len += (count - 1) * delim_len;

    cstr* result = str_new(total_len + 1);
    if (!result)
        return NULL;

    char* dest = result->data;
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            memcpy(dest, delim, delim_len);
            dest += delim_len;
        }
        memcpy(dest, strings[i]->data, strings[i]->length);
        dest += strings[i]->length;
    }

    *dest          = '\0';
    result->length = total_len;
    return result;
}

cstr* str_reverse(const cstr* s) {
    if (!s || s->length == 0)
        return NULL;

    cstr* result = malloc(sizeof(cstr) + s->length + 1);
    if (!result)
        return NULL;

    result->length   = s->length;
    result->capacity = s->length + 1;

    char* src  = (char*)s->data + s->length - 1;
    char* dest = result->data;
    size_t len = s->length;

    while (len--) {
        *dest++ = *src--;
    }

    *dest = '\0';
    return result;
}

void str_reverse_in_place(cstr* s) {
    if (!s || s->length < 2)
        return;

    char* start = s->data;
    char* end   = s->data + s->length - 1;

    while (start < end) {
        char temp = *start;
        *start++  = *end;
        *end--    = temp;
    }
}
