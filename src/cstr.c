#include "../include/cstr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strdup _strdup
#define strtok_r strtok_s
#endif

// Default allocator functions
static void* default_alloc(Arena* arena, size_t size) {
    (void)arena;
    return malloc(size);
}

static void* default_realloc(Arena* arena, void* ptr, size_t size) {
    (void)arena;
    return realloc(ptr, size);
}

static void default_free(Arena* arena, void* ptr) {
    (void)arena;
    free(ptr);
}

cstr_allocator cstr_arena_allocator(Arena* arena) {
    return (cstr_allocator){arena, arena_alloc, arena_realloc, arena_free};
}

// Create a new empty cstr with a custom allocator.
cstr* cstr_new_with_allocator(size_t capacity, cstr_allocator allocator) {
    // Create a new cstr object
    cstr* str = (cstr*)allocator.alloc(allocator.arena, sizeof(cstr));
    if (!str) {
        return NULL;
    }

    // Set the allocator for this cstr
    str->allocator = allocator;

    // Allocate memory for the string data
    str->data = (char*)allocator.alloc(allocator.arena, capacity);
    if (!str->data) {
        allocator.free(allocator.arena, str);
        return NULL;
    }

    // Initialize the string
    str->length = 0;
    str->capacity = capacity;
    str->data[0] = '\0';
    return str;
}

// Create a new empty cstr with a default capacity.
cstr* cstr_new(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = CSTR_MIN_CAPACITY;
    }
    return cstr_new_with_allocator(
        initial_capacity, (cstr_allocator){NULL, default_alloc, default_realloc, default_free});
}

// Create a new cstr from a C string using a custom allocator.
cstr* cstr_from_with_allocator(const char* s, cstr_allocator allocator) {
    if (!s) {
        return NULL;
    }

    size_t len = strlen(s);
    size_t capacity = len + 1;

    cstr* str = cstr_new_with_allocator(capacity, allocator);
    if (!str) {
        return NULL;
    }

    memcpy(str->data, s, len);
    str->data[len] = '\0';
    str->length = len;
    return str;
}

// Create a new cstr from a C string using the default allocator.
cstr* cstr_from(const char* s) {
    return cstr_from_with_allocator(
        s, (cstr_allocator){NULL, default_alloc, default_realloc, default_free});
}

// Free a cstr.
void cstr_free(cstr* str) {
    if (str) {
        str->allocator.free(str->allocator.arena, str->data);
        str->allocator.free(str->allocator.arena, str);
    }
}

// Ensure that the cstr has enough capacity.
bool cstr_ensure_capacity(cstr* str, size_t capacity) {
    if (str->capacity >= capacity) {
        return true;
    }

    // Double the capacity if the new capacity is larger than twice the current capacity
    size_t new_capacity = (capacity > str->capacity * 2) ? capacity : str->capacity * 2;

    // Reallocate memory for the string data
    char* new_data = (char*)str->allocator.realloc(str->allocator.arena, str->data, new_capacity);
    if (!new_data) {
        return false;
    }

    str->data = new_data;
    str->capacity = new_capacity;
    return true;
}

// Append a character to the end of a cstr.
bool cstr_append_char(cstr* str, char c) {
    if (!cstr_ensure_capacity(str, str->length + 2)) {
        return false;
    }
    str->data[str->length++] = c;
    str->data[str->length] = '\0';
    return true;
}

// Append a string to the end of a cstr.
bool cstr_append(cstr* str, const char* s) {
    if (!s) {
        return false;
    }

    size_t s_len = strlen(s);
    if (!cstr_ensure_capacity(str, str->length + s_len + 1)) {
        return false;
    }

    memcpy(str->data + str->length, s, s_len);
    str->length += s_len;
    str->data[str->length] = '\0';
    return true;
}

// Get the length of a cstr.
size_t cstr_len(const cstr* str) {
    return str->length;
}
// Get the capacity of a cstr.
size_t cstr_capacity(const cstr* str) {
    return str->capacity;
}

// Append a formatted string to the cstr.
__attribute__((format(printf, 2, 3))) bool cstr_append_fmt(cstr* str, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Get the required size for the formatted string
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        return false;
    }

    // Ensure capacity in the cstr
    if (!cstr_ensure_capacity(str, str->length + size + 1)) {
        va_end(args);
        return false;
    }

    // Append the formatted string
    vsnprintf(str->data + str->length, size + 1, fmt, args);
    str->length += size;
    va_end(args);
    return true;
}

// Shrink the cstr to remove excess capacity.
bool cstr_clip(cstr** str) {
    if (!str || !(*str)) {
        return false;
    }

    if ((*str)->length <= 0) {
        return false;
    }

    size_t new_capacity = (*str)->length + 1;

    char* new_data =
        (char*)((*str)->allocator.realloc((*str)->allocator.arena, (*str)->data, new_capacity));
    if (!new_data) {
        return false;
    }

    (*str)->data = new_data;
    (*str)->capacity = new_capacity;
    return true;
}

// Get the C string representation of a cstr.
const char* cstr_data(const cstr* str) {
    return str->data;
}

// Compare two cstrs.
int cstr_compare(const cstr* str1, const cstr* str2) {
    return strcmp(str1->data, str2->data);
}

// Split the cstr into substrings based on a delimiter.
cstr** cstr_split(const cstr* str, char delimiter, size_t* count) {
    *count = 0;
    size_t start = 0;
    cstr** result = NULL;

    for (size_t i = 0; i <= str->length; ++i) {
        if (str->data[i] == delimiter || i == str->length) {
            (*count)++;
            result = (cstr**)realloc(result, (*count) * sizeof(cstr*));
            if (!result) {
                return NULL;
            }

            result[*count - 1] = cstr_new(0);
            if (!result[*count - 1]) {
                goto fail;
            }

            for (size_t j = start; j < i; ++j) {
                if (!cstr_append_char(result[*count - 1], str->data[j])) {
                    goto fail;
                }
            }
            start = i + 1;
        }
    }

    return result;

fail:
    for (size_t i = 0; i < *count; ++i) {
        cstr_free(result[i]);
    }
    free(result);
    return NULL;
}

void cstr_free_array(cstr** strs, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        cstr_free(strs[i]);
    }
    free(strs);
}

/**
 * @brief Splits a char* into an array of substrings based on a delimiter.
 * 
 * @param str A pointer to the char* to split.
 * @param delimiter The delimiter character to split on.
 * @param count A pointer to a size_t variable that will store the number of substrings found.
 * @return A pointer to an array of char* pointers, or NULL if allocation fails. 
 */
char** cstr_split2(const char* str, char delimiter, size_t* count) {
    if (str == NULL) {
        return NULL;
    }

    *count = 0;
    size_t len = strlen(str);
    if (len == 0) {
        return NULL;
    }

    // Count the number of substrings
    for (size_t i = 0; i < len; i++) {
        if (str[i] == delimiter) {
            (*count)++;
        }
    }
    (*count)++;  // Add 1 for the last substring

    // Allocate memory for the array of substrings
    char** substrings = (char**)malloc((*count) * sizeof(char*));
    if (substrings == NULL) {
        return NULL;
    }

    // Special case for a single substring
    if (*count == 1) {
        substrings[0] = strdup(str);
        return substrings;
    }

    // Split the string into substrings
    size_t start = 0;
    size_t end = 0;
    size_t substring_index = 0;

    for (size_t i = 0; i < len; i++) {
        if (str[i] == delimiter || i == len - 1) {
            end = i;

            // Include the delimiter in the substring
            if (i == len - 1) {
                end = len;
            }

            size_t substring_len = end - start + 1;
            substrings[substring_index] = (char*)malloc(substring_len);

            if (substrings[substring_index] == NULL) {
                // Free previously allocated memory
                for (size_t j = 0; j < substring_index; j++) {
                    free(substrings[j]);
                }
                free(substrings);
                return NULL;
            }

            strncpy(substrings[substring_index], &str[start], substring_len);
            substrings[substring_index][substring_len - 1] = '\0';  // Add null terminator
            substring_index++;
            start = i + 1;
        }
    }

    return substrings;
}

// Free an array of char* pointers.
void cstr_free2(char** substrings, size_t count) {
    if (substrings == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(substrings[i]);
    }
    free(substrings);
}

// Split a cstr into an array of cstrs by a given delimiter.
// Returns an array of cstr pointers. It is not terminated by NULL.
cstr** cstr_split_at(const cstr* str, const char* delimiter, size_t initial_capacity,
                     size_t* count) {
    size_t capacity = initial_capacity;
    cstr** tokens = (cstr**)malloc(capacity * sizeof(cstr*));
    if (!tokens) {
        return NULL;
    }

    char* copy = strdup(str->data);
    if (!copy) {
        free(tokens);
        return NULL;
    }

    char* rest = (char*)str;
    *count = 0;

    char* token = strtok_r(copy, delimiter, &rest);
    while (token != NULL) {
        if (*count == capacity) {
            capacity *= 2;
            cstr** new_tokens = (cstr**)realloc(tokens, capacity * sizeof(cstr*));
            if (!new_tokens) {
                goto fail;
            }
            tokens = new_tokens;
        }

        tokens[*count] = cstr_from(token);
        if (tokens[*count] == NULL) {
            goto fail;
        }

        (*count)++;
        token = strtok_r(NULL, delimiter, &rest);
    }

    free(copy);
    return tokens;

fail:
    free(copy);
    for (size_t i = 0; i < *count; i++) {
        cstr_free(tokens[i]);
    }
    // free(tokens);
    return NULL;
}

// Join an array of cstr into a single cstr with a given separator.
cstr* cstr_join(cstr** strs, size_t count, const char* separator) {
    if (count == 0) {
        return cstr_from("");
    }

    // Calculate the total length of the joined string
    size_t total_len = 0;
    for (size_t i = 0; i < count; i++) {
        total_len += strs[i]->length;
    }
    total_len += (count - 1) * strlen(separator);

    cstr* joined = cstr_new(0);
    if (!joined) {
        return NULL;
    }
    cstr_ensure_capacity(joined, total_len + 1);

    for (size_t i = 0; i < count; i++) {
        if (!cstr_append(joined, strs[i]->data)) {
            cstr_free(joined);
            return NULL;
        }

        // Do not append sep after the last string.
        if (i < count - 1) {
            if (!cstr_append(joined, separator)) {
                cstr_free(joined);
                return NULL;
            }
        }
    }
    return joined;
}

// Get a substring from the cstr.
cstr* cstr_substr(const cstr* str, size_t start, size_t length) {
    if (start >= str->length) {
        return NULL;
    }

    if (start + length > str->length) {
        length = str->length - start;
    }

    cstr* result = cstr_new(str->length);
    if (!result) {
        return NULL;
    }

    for (size_t i = 0; i < length; ++i) {
        if (!cstr_append_char(result, str->data[start + i])) {
            cstr_free(result);
            return NULL;
        }
    }
    return result;
}

// Check if the cstr starts with a given prefix.
bool cstr_starts_with(const cstr* str, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    if (prefix_len > str->length) {
        return false;
    }

    return strncmp(str->data, prefix, prefix_len) == 0;
}

// Check if the cstr ends with a given suffix.
bool cstr_ends_with(const cstr* str, const char* suffix) {
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str->length) {
        return false;
    }

    return strncmp(str->data + str->length - suffix_len, suffix, suffix_len) == 0;
}

// // Regex matching using the regex library (requires linking with -lregex).
// bool cstr_regex_match(const cstr* str, const char* pattern) {
//   regex_t regex;
//   int reti;

//   reti = regcomp(&regex, pattern, REG_EXTENDED);
//   if (reti) {
//     // Error compiling the regex
//     return false;
//   }

//   reti = regexec(&regex, str->data, 0, NULL, 0);
//   regfree(&regex);

//   if (!reti) {
//     return true;
//   } else if (reti == REG_NOMATCH) {
//     return false;
//   } else {
//     // Error executing the regex
//     return false;
//   }
// }

cstr* cstr_copy(const cstr* str) {
    return cstr_from(str->data);
}

// Prepend a string to the cstr.
bool cstr_prepend(cstr* str, const char* src) {
    size_t src_len = strlen(src);
    if (!cstr_ensure_capacity(str, str->length + src_len + 1)) {
        return false;
    }

    memmove(str->data + src_len, str->data, str->length + 1);
    memcpy(str->data, src, src_len);
    str->length += src_len;
    return true;
}

// Convert the string to snake case.
bool cstr_snakecase(cstr* str) {
    if (!cstr_ensure_capacity(str, str->length * 2 + 1)) {
        return false;
    }

    size_t length = str->length;
    char* data = str->data;

    // Convert first character to lowercase
    data[0] = tolower(data[0]);
    size_t space_count = 0;
    char prev = data[0];

    for (size_t i = 1; i < length; i++) {
        // Check if current character is a space
        if (isspace(data[i])) {
            space_count++;
            continue;  // Skip spaces
        }

        // Check if current character is uppercase
        if (isupper(data[i])) {
            // Insert underscore before uppercase character
            memmove(&data[i + 1 - space_count], &data[i - space_count], length - i + space_count);
            data[i - space_count] = '_';
            length++;
            i++;  // Skip the inserted underscore
        }

        // Convert the character to lowercase
        data[i - space_count] = tolower(data[i]);

        // Insert underscore before digit if preceded by a non-digit character
        if (isdigit(data[i]) && !isdigit(prev)) {
            memmove(&data[i + 1 - space_count], &data[i - space_count], length - i + space_count);
            data[i - space_count] = '_';
            length++;
            i++;  // Skip the inserted underscore
        }

        prev = data[i];
    }

    // Trim the string to the new length
    data[length - space_count] = '\0';
    str->length = length - space_count;
    return true;
}

// Convert the string to title case.
void cstr_titlecase(cstr* str) {
    int capitalize = 1;
    for (size_t i = 0; i < str->length; i++) {
        if (str->data[i] == ' ') {
            capitalize = 1;
        } else if (capitalize) {
            str->data[i] = toupper(str->data[i]);
            capitalize = 0;
        } else {
            str->data[i] = tolower(str->data[i]);
        }
    }
}

// Convert the string to camel case.
bool cstr_camelcase(cstr* str) {
    char* data = str->data;
    size_t length = str->length;
    if (length == 0) {
        return true;  // Empty string
    }

    int dest_index = 0;
    int capitalize = 0;       // To check if the next character should be capitalized
    int switch_to_upper = 0;  // To check if we should change the character to upper case

    while (data[dest_index] != '\0') {
        if (data[dest_index] == ' ' || data[dest_index] == '_') {
            capitalize = 1;
        } else if (capitalize) {
            data[dest_index] = toupper(data[dest_index]);
            capitalize = 0;
        } else {
            if (isupper(data[dest_index])) {
                switch_to_upper = 1;
            }
            if (switch_to_upper) {
                data[dest_index] = toupper(data[dest_index]);
                switch_to_upper = 0;
            } else {
                data[dest_index] = tolower(data[dest_index]);
            }
        }
        dest_index++;
    }

    // Remove spaces and underscores from the string
    int j = 0;
    for (dest_index = 0; data[dest_index] != '\0'; dest_index++) {
        if (data[dest_index] != ' ' && data[dest_index] != '_') {
            data[j] = data[dest_index];
            j++;
        }
    }

    // Pascal case starts with lower case, unlike camel case.
    data[0] = tolower(data[0]);
    data[j] = '\0';

    str->length = j;
    return true;
}

// You must free the result if result is non-NULL.
cstr* cstr_replace(const cstr* str, const char* old, const char* with) {
    // Sanity checks and initialization
    if (!str || !old || strlen(old) == 0) {
        return NULL;
    }

    if (!with) {
        with = "";
    }

    size_t old_len = strlen(old);
    size_t len_with = strlen(with);

    // Count the number of replacements needed
    int count = 0;
    const char* ins = str->data;
    while ((ins = strstr(ins, old))) {
        ++count;
        ins += old_len;
    }

    // Allocate memory for the new string
    size_t result_len = str->length + (len_with - old_len) * count + 1;
    cstr* result = cstr_new_with_allocator(result_len, str->allocator);
    if (!result) {
        return NULL;
    }

    // Perform replacements
    const char* original = str->data;
    char* tmp = result->data;
    while (count--) {
        const char* ins = strstr(original, old);
        size_t len_front = ins - original;
        tmp = strncpy(tmp, original, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        original += len_front + old_len;
    }
    // Copy the remaining part of the string
    strcpy(tmp, original);

    result->length = strlen(result->data);

    return result;
}

// Convert the string to pascal case.
bool cstr_pascalcase(cstr* str) {
    if (!cstr_camelcase(str)) {
        return false;
    }
    if (str->length > 0) {
        str->data[0] = toupper(str->data[0]);
    }
    return true;
}

// Replace all occurrences of a substring with another string
cstr* cstr_replace_all(const cstr* str, const char* old, const char* with) {
    // Sanity checks and initialization
    if (!str || !old || strlen(old) == 0) {
        return NULL;
    }

    if (!with) {
        with = "";
    }

    size_t old_len = strlen(old);
    size_t len_with = strlen(with);

    // Count the number of replacements needed
    int count = 0;
    const char* ins = str->data;
    while ((ins = strstr(ins, old))) {
        ++count;
        ins += old_len;
    }

    // Allocate memory for the new string
    size_t result_len = str->length + (len_with - old_len) * count + 1;
    cstr* result = cstr_new_with_allocator(result_len, str->allocator);
    if (!result) {
        return NULL;
    }

    // Perform replacements
    const char* original = str->data;
    char* tmp = result->data;
    while (count--) {
        const char* ins = strstr(original, old);
        size_t len_front = ins - original;
        tmp = strncpy(tmp, original, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        original += len_front + old_len;
    }
    // Copy the remaining part of the string
    strcpy(tmp, original);

    result->length = strlen(result->data);
    return result;
}

// Remove leading whitespace characters from the string.
void cstr_ltrim(cstr* str) {
    if (str->length == 0) {
        return;
    }

    size_t i = 0;
    while (i < str->length && isspace(str->data[i])) {
        i++;
    }

    if (i > 0) {
        memmove(str->data, str->data + i, str->length - i + 1);
        str->length -= i;
    }
}

// Remove trailing whitespace characters from the string.
void cstr_rtrim(cstr* str) {
    if (str->length == 0) {
        return;
    }
    size_t i = (str->length - 1);
    while (isspace(str->data[i])) {
        i--;
    }

    str->data[i + 1] = '\0';
    str->length = i + 1;
}

// Remove leading and trailing whitespace characters from the string.
void cstr_trim(cstr* str) {
    cstr_ltrim(str);
    cstr_rtrim(str);
}

// Remove leading and trailing characters from the string
// that match any character in the given set.
void cstr_trim_chars(cstr* str, const char* chars) {
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

    size_t len = end - start + 1;    // length of the remaining string
    memmove(str->data, start, len);  // move the remaining string to the beginning
    str->data[len] = '\0';
}

// Count the number of occurrences of a substring within the string.
size_t cstr_count_substr(const cstr* str, const char* substr) {
    size_t count = 0;
    size_t sub_len = strlen(substr);
    const char* p = str->data;
    while ((p = strstr(p, substr))) {
        count++;
        p += sub_len;
    }
    return count;
}

// Remove all occurrences of character c from str.
void cstr_remove_char(char* str, char c) {
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
void cstr_remove_substr(cstr* str, size_t start, size_t substr_length) {
    if (start >= str->length) {
        return;
    }

    if (start + substr_length > str->length) {
        substr_length = str->length - start;
    }

    memmove(str->data + start, str->data + start + substr_length,
            str->length - start - substr_length + 1);
    str->length -= substr_length;
}

// Reverse the string str in place.
void cstr_reverse(cstr* str) {
    size_t len = str->length;
    for (size_t i = 0; i < len / 2; i++) {
        char temp = str->data[i];
        str->data[i] = str->data[len - i - 1];
        str->data[len - i - 1] = temp;
    }
}
