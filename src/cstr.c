#include "../include/cstr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strdup _strdup
#define strtok_r strtok_s
#endif

// Create a new empty cstr with a default capacity.
cstr* cstr_new(Arena* arena, size_t cap) {
    if (cap == 0) {
        cap = CSTR_MIN_CAPACITY;
    }

    // Create a new cstr object
    cstr* str = (cstr*)arena_alloc(arena, sizeof(cstr));
    if (!str) {
        return NULL;
    }

    // Allocate memory for the string data
    str->data = (char*)arena_alloc(arena, cap);
    if (!str->data) {
        return NULL;
    }

    // Initialize the string
    str->length = 0;
    str->capacity = cap;
    str->data[0] = '\0';
    return str;
}

// Create a new cstr from a C string using the default allocator.
cstr* cstr_from(Arena* arena, const char* s) {
    size_t len = strlen(s);
    size_t capacity = len + 1;

    cstr* str = cstr_new(arena, capacity);
    if (!str) {
        return NULL;
    }

    memcpy(str->data, s, len);
    str->data[len] = '\0';
    str->length = len;
    return str;
}

// Ensure that the cstr has enough capacity.
bool cstr_ensure_capacity(Arena* arena, cstr* str, size_t capacity) {
    if (str->capacity >= capacity) {
        return true;
    }

    // Reallocate memory for the string data
    char* new_data = (char*)arena_realloc(arena, str->data, capacity);
    if (!new_data) {
        return false;
    }

    str->data = new_data;
    str->capacity = capacity;
    return true;
}

// Append a character to the end of a cstr.
bool cstr_append_char(Arena* arena, cstr* str, char c) {
    if (!cstr_ensure_capacity(arena, str, str->length + 2)) {
        return false;
    }
    str->data[str->length++] = c;
    str->data[str->length] = '\0';
    return true;
}

// Append a string to the end of a cstr.
bool cstr_append(Arena* arena, cstr* str, const char* s) {
    if (!s) {
        return false;
    }

    size_t s_len = strlen(s);
    if (!cstr_ensure_capacity(arena, str, str->length + s_len + 1)) {
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
bool cstr_append_fmt(Arena* arena, cstr* str, const char* fmt, ...) {
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
    if (!cstr_ensure_capacity(arena, str, str->length + size + 1)) {
        va_end(args);
        return false;
    }

    // Append the formatted string
    int n = vsnprintf(str->data + str->length, size + 1, fmt, args);
    if (n < 0) {
        // Error occurred while formatting the string
        va_end(args);
        return false;
    }

    str->length += size;
    va_end(args);
    return true;
}

// Shrink the cstr to remove excess capacity.
bool cstr_clip(Arena* arena, cstr* str) {
    size_t new_capacity = (str)->length + 1;
    if (new_capacity == str->capacity) {
        return true;
    }

    if (str->length == 0) {
        str->data[0] = '\0';
        return true;
    }

    char* new_data = (char*)arena_realloc(arena, str->data, new_capacity);
    if (!new_data) {
        return false;
    }

    str->data = new_data;
    str->capacity = new_capacity;
    return true;
}

// Get the C string representation of a cstr.
char* cstr_data(const cstr* str) {
    return str->data;
}

// Compare two cstrs.
int cstr_compare(const cstr* str1, const cstr* str2) {
    return strcmp(str1->data, str2->data);
}

bool cstr_equals(const cstr* str1, const cstr* str2) {
    return strcmp(str1->data, str2->data) == 0;
}

// Split the cstr into substrings based on a delimiter.
cstr** cstr_split(Arena* arena, const cstr* str, char delimiter, size_t* num_splits) {
    if (str == NULL || str->length == 0) {
        return NULL;
    }

    // Count the number of substrings
    size_t count = 0;
    *num_splits = 0;
    for (size_t i = 0; i < str->length; i++) {
        if (str->data[i] == delimiter) {
            count++;
        }
    }
    count++;  // Add 1 for the last substring
    *num_splits = count;

    // Allocate memory for the array of substrings
    cstr** substrings = (cstr**)arena_alloc(arena, count * sizeof(cstr*));
    if (substrings == NULL) {
        return NULL;
    }

    // Special case for a single substring
    if (count == 1) {
        substrings[0] = cstr_from(arena, str->data);
        return substrings;
    }

    // Split the string into substrings
    size_t start = 0;
    size_t end = 0;
    size_t substring_index = 0;

    for (size_t i = 0; i < str->length; i++) {
        if (str->data[i] == delimiter || i == str->length - 1) {
            end = i;

            // Include the delimiter in the substring
            if (i == str->length - 1) {
                end = str->length;
            }

            size_t substring_len = end - start + 1;
            substrings[substring_index] = (cstr*)cstr_new(arena, substring_len);
            if (substrings[substring_index] == NULL) {
                return NULL;
            }

            strncpy(substrings[substring_index]->data, &str->data[start], substring_len);
            (substrings[substring_index]->data)[substring_len - 1] = '\0';  // Add null terminator

            // update the length of the substring
            substrings[substring_index]->length = substring_len - 1;

            substring_index++;
            start = i + 1;
        }
    }
    return substrings;
}

/**
 * @brief Splits a char* into an array of substrings based on a delimiter.
 * 
 * @param str A pointer to the char* to split.
 * @param delimiter The delimiter character to split on.
 * @param count A pointer to a size_t variable that will store the number of substrings found.
 * @return A pointer to an array of char* pointers, or NULL if allocation fails. 
 */
char** cstr_splitchar(const char* str, char delimiter, size_t* num_splits) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    if (len == 0) {
        return NULL;
    }

    size_t count = 0;
    *num_splits = 0;

    // Count the number of substrings
    for (size_t i = 0; i < len; i++) {
        if (str[i] == delimiter) {
            count++;
        }
    }
    count++;  // Add 1 for the last substring
    *num_splits = count;

    // Allocate memory for the array of substrings
    char** substrings = (char**)malloc(count * sizeof(char*));
    if (substrings == NULL) {
        return NULL;
    }

    // Special case for a single substring
    if (count == 1) {
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
void cstr_free_array(char** substrings, size_t count) {
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
cstr** cstr_split_at(Arena* arena, const cstr* str, const char* delimiter, size_t initial_capacity,
                     size_t* count) {
    size_t capacity = initial_capacity;
    cstr** tokens = (cstr**)arena_alloc(arena, capacity * sizeof(cstr*));
    if (!tokens) {
        return NULL;
    }

    cstr* copy = cstr_from(arena, str->data);
    if (!copy) {
        return NULL;
    }

    char* start = copy->data;
    size_t local_count = 0;

    while (1) {
        char* end = strstr(start, delimiter);
        if (!end) {
            // Add the last token to the array.
            if (local_count == capacity) {
                capacity *= 2;
                cstr** new_tokens = (cstr**)arena_alloc(arena, capacity * sizeof(cstr*));
                if (!new_tokens) {
                    return NULL;
                }
                memcpy(new_tokens, tokens, local_count * sizeof(cstr*));
                tokens = new_tokens;
            }

            tokens[local_count] = cstr_from(arena, start);
            if (tokens[local_count] == NULL) {
                return NULL;
            }

            tokens[local_count]->length = strlen(start);
            local_count++;
            break;
        }

        size_t token_length = end - start;
        if (local_count == capacity) {
            capacity *= 2;
            cstr** new_tokens = (cstr**)arena_alloc(arena, capacity * sizeof(cstr*));
            if (!new_tokens) {
                return NULL;
            }
            memcpy(new_tokens, tokens, local_count * sizeof(cstr*));
            tokens = new_tokens;
        }

        tokens[local_count] = (cstr*)arena_alloc(arena, sizeof(cstr));
        if (!tokens[local_count]) {
            return NULL;
        }
        tokens[local_count]->data = (char*)arena_alloc(arena, token_length + 1);
        if (!tokens[local_count]->data) {
            return NULL;
        }
        strncpy(tokens[local_count]->data, start, token_length);
        tokens[local_count]->data[token_length] = '\0';
        tokens[local_count]->length = token_length;

        local_count++;
        start = end + strlen(delimiter);
    }

    *count = local_count;  // Set the count pointer to the local count value

    return tokens;
}

// Join an array of cstr into a single cstr with a given separator.
cstr* cstr_join(Arena* arena, cstr** strs, size_t count, const char* separator) {
    if (count == 0) {
        return cstr_from(arena, "");
    }

    // Calculate the total length of the joined string
    size_t total_len = 0;
    for (size_t i = 0; i < count; i++) {
        total_len += strs[i]->length;
    }
    total_len += (count - 1) * strlen(separator);  // Add separator length

    cstr* joined = cstr_new(arena, total_len + 1);
    if (!joined) {
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        if (!cstr_append(arena, joined, strs[i]->data)) {
            return NULL;
        }

        // Do not append sep after the last string.
        if (i < count - 1) {
            if (!cstr_append(arena, joined, separator)) {
                return NULL;
            }
        }
    }
    return joined;
}

// Get a substring from the cstr.
cstr* cstr_substr(Arena* arena, const cstr* str, size_t start, size_t length) {
    if (start >= str->length) {
        return NULL;
    }

    if (start + length > str->length) {
        length = str->length - start;
    }

    cstr* result = cstr_new(arena, str->length);
    if (!result) {
        return NULL;
    }

    for (size_t i = 0; i < length; ++i) {
        if (!cstr_append_char(arena, result, str->data[start + i])) {
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

// Prepend a string to the cstr.
bool cstr_prepend(Arena* arena, cstr* str, const char* src) {
    size_t src_len = strlen(src);
    if (!cstr_ensure_capacity(arena, str, str->length + src_len + 1)) {
        return false;
    }

    memmove(str->data + src_len, str->data, str->length + 1);
    memcpy(str->data, src, src_len);
    str->length += src_len;
    return true;
}

// Convert the string to snake case.
bool cstr_snakecase(Arena* arena, cstr* str) {
    if (!cstr_ensure_capacity(arena, str, (str->length * 2) + 1)) {
        return false;
    }

    size_t length = str->length;
    char* data = str->data;

    // Convert first character to lowercase
    data[0] = (char)tolower(data[0]);
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
        data[i - space_count] = (char)tolower(data[i]);

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
            str->data[i] = (char)toupper(str->data[i]);
            capitalize = 0;
        } else {
            str->data[i] = (char)tolower(str->data[i]);
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
            data[dest_index] = (char)toupper(data[dest_index]);
            capitalize = 0;
        } else {
            if (isupper(data[dest_index])) {
                switch_to_upper = 1;
            }
            if (switch_to_upper) {
                data[dest_index] = (char)toupper(data[dest_index]);
                switch_to_upper = 0;
            } else {
                data[dest_index] = (char)tolower(data[dest_index]);
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

    data[0] = (char)tolower(data[0]);
    data[j] = '\0';

    str->length = j;
    return true;
}

// Convert the string to pascal case.
bool cstr_pascalcase(cstr* str) {
    if (!cstr_camelcase(str)) {
        return false;
    }
    if (str->length > 0) {
        str->data[0] = (char)toupper(str->data[0]);
    }
    return true;
}

cstr* cstr_replace(Arena* arena, const cstr* str, const char* old, const char* with) {
    // Sanity checks and initialization
    if (!str || !old || !with || strlen(old) == 0) {
        return NULL;
    }

    size_t old_len = strlen(old);
    size_t len_with = strlen(with);

    // Count the number of replacements needed
    int numOccurrences = 0;
    const char* originalStrPtr = str->data;
    while ((originalStrPtr = strstr(originalStrPtr, old))) {
        ++numOccurrences;
        originalStrPtr += old_len;
    }

    // Allocate memory for the new string
    size_t result_len = str->length + ((len_with - old_len) * numOccurrences) + 1;
    cstr* result = cstr_new(arena, result_len);
    if (!result) {
        return NULL;
    }

    // Perform replacements
    const char* original = str->data;
    char* tmp = result->data;
    while (numOccurrences--) {
        const char* substringLocation = strstr(original, old);

        // Copy the part of the string before the old substring
        size_t len_front = substringLocation - original;

        // Copy the part of the string after the old substring
        tmp = strncpy(tmp, original, len_front) + len_front;

        // Copy the new substring
        tmp = strcpy(tmp, with) + len_with;

        // Move the original pointer to the end of the old substring
        original += len_front + old_len;
    }

    // Copy the remaining part of the string
    strcpy(tmp, original);

    result->length = strlen(result->data);
    return result;
}

// Replace all occurrences of a substring with another string
cstr* cstr_replace_all(Arena* arena, const cstr* str, const char* old, const char* with) {
    // Sanity checks and initialization
    if (!str || !old || !with || strlen(old) == 0) {
        return NULL;
    }

    size_t old_len = strlen(old);
    size_t len_with = strlen(with);

    int numOccurrences = 0;
    const char* inputStringPtr = str->data;
    while ((inputStringPtr = strstr(inputStringPtr, old))) {
        ++numOccurrences;
        inputStringPtr += old_len;
    }

    // Allocate memory for the new string
    size_t result_len = str->length + (len_with - old_len) * numOccurrences + 1;
    cstr* result = cstr_new(arena, result_len);
    if (!result) {
        return NULL;
    }

    // Perform replacements
    const char* originalString = str->data;
    char* tmp = result->data;
    while (numOccurrences--) {
        const char* ins = strstr(originalString, old);
        size_t len_front = ins - originalString;
        tmp = strncpy(tmp, originalString, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        originalString += len_front + old_len;
    }

    // Copy the remaining part of the string
    strcpy(tmp, originalString);

    result->length = strlen(result->data);
    return result;
}

// Remove leading whitespace characters from the string.
void cstr_ltrim(cstr* str) {
    if (str->length == 0) {
        return;
    }

    // Find the first non-whitespace character index in the string
    size_t index = 0;
    while (index < str->length && isspace(str->data[index])) {
        index++;
    }

    // Shift the string to the left
    if (index > 0) {
        memmove(str->data, str->data + index, str->length - index + 1);
        str->length -= index;
    }
}

// Remove trailing whitespace characters from the string.
void cstr_rtrim(cstr* str) {
    if (str->length == 0) {
        return;
    }

    // Check if the string is all whitespace
    if (str->length == 1 && isspace(str->data[0])) {
        str->data[0] = '\0';
        str->length = 0;
        return;
    }

    // Find the last non-whitespace character index in the string
    size_t lastIndex = (str->length - 1);
    while (isspace(str->data[lastIndex])) {
        lastIndex--;
    }

    str->data[lastIndex + 1] = '\0';
    str->length = lastIndex + 1;
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
