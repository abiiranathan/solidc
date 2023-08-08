#include "str.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct Str {
    char* data;       // string data on the heap.
    size_t length;    // Actual string length(minus null terminater)
    size_t capacity;  // Allocated capacity
} Str;

Str* str_new(const char* str) {
    if (str == NULL) {
        return NULL;
    }

    Str* string = malloc(sizeof(Str));
    if (string == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    string->data = malloc((len + 1) * sizeof(char));
    if (string->data == NULL) {
        free(string);
        return NULL;
    }

    memcpy(string->data, str, len + 1);
    string->length = len;

    // Set the initial capacity to the string length
    string->capacity = len;
    return string;
}

bool str_ensure_capacity(Str* str, size_t capacity) {
    if (str == NULL) {
        return false;
    }

    // We are already good.
    if (capacity <= str->capacity) {
        return true;
    }

    char* new_data = realloc(str->data, (capacity + 1) * sizeof(char));
    if (new_data == NULL) {
        return false;
    }

    str->data = new_data;
    str->capacity = capacity;
    return true;
}

void str_free(Str* string) {
    if (string == NULL) {
        return;
    }
    if (string->data) {
        free(string->data);
    }
    free(string);
}

int str_compare(const Str* str1, const char* str2) {
    if (str1 == NULL || str2 == NULL) {
        return -1;
    }
    return strcmp(str1->data, str2);
}

char* str_data(const Str* str) {
    return str->data;
}

Str* str_copy(const Str* str) {
    if (str == NULL) {
        return NULL;
    }

    Str* copy = malloc(sizeof(Str));
    if (copy == NULL) {
        return NULL;
    }

    copy->data = malloc((str->length + 1) * sizeof(char));
    if (copy->data == NULL) {
        free(copy);
        return NULL;
    }

    memcpy(copy->data, str->data, str->length + 1);
    copy->length = str->length;
    // Set the capacity to match the length
    copy->capacity = str->length;
    return copy;
}

void str_concat(Str* str1, const char* str2) {
    if (str1 == NULL || str2 == NULL) {
        return;
    }

    size_t len1 = str1->length;
    size_t len2 = strlen(str2);
    size_t new_length = len1 + len2;

    // Check for size_t overflow
    if (new_length < len1) {
        printf("size_t overflow");
        return;
    }

    if (str_ensure_capacity(str1, new_length)) {
        memcpy(str1->data + len1, str2, len2);
        str1->length = new_length;
        str1->data[new_length] = '\0';
    }
}

size_t str_length(const Str* str) {
    if (str == NULL) {
        return -1;
    }

    return str->length;
}

char* str_at(const Str* str, size_t index) {
    if (str == NULL || index >= str->length) {
        return NULL;
    }

    return str->data + index;
}

bool str_contains(const Str* str, const char* substring) {
    if (str == NULL || substring == NULL) {
        return false;
    }
    return strstr(str->data, substring) != NULL;
}

bool str_is_empty(const Str* str) {
    if (str == NULL) {
        return true;
    }

    return str->length == 0;
}

int str_find(const Str* str, const char* substring) {
    if (str == NULL || substring == NULL) {
        return -1;
    }
    return strstr(str->data, substring) - str->data;
}

void str_replace(Str* str, const char* old, const char* newstr) {
    if (str == NULL || old == NULL || newstr == NULL) {
        return;
    }

    int index = str_find(str, old);
    if (index == -1) {
        return;
    }

    size_t old_len = strlen(old);
    size_t new_len = strlen(newstr);

    int length_diff = new_len - old_len;
    if (length_diff == 0) {
        // same substring size.
        memcpy(str->data + index, newstr, new_len);
    } else if (length_diff < 0) {
        // substring is shorter, so there is memory overlap.
        memmove(str->data + index + new_len, str->data + index + old_len,
                str->length - index - old_len + 1);
        memcpy(str->data + index, newstr, new_len);
        str->length += length_diff;
    } else {
        // Substring is longer.
        size_t new_length = str->length + length_diff;

        // Check is string already has enough capacity.
        if (new_length > str->capacity) {
            char* new_data = realloc(str->data, (new_length + 1) * sizeof(char));
            if (new_data == NULL) {
                return;
            }
            str->data = new_data;
            str->capacity = new_length;
        }
        memmove(str->data + index + new_len, str->data + index + old_len,
                str->length - index - old_len + 1);
        memcpy(str->data + index, newstr, new_len);
        str->length += length_diff;
    }
}

void str_replace_all(Str* str, const char* old, const char* newstr) {
    if (str == NULL || old == NULL || newstr == NULL) {
        return;
    }

    regex_t regex;
    int ret = regcomp(&regex, old, REG_EXTENDED);
    if (ret != 0) {
        // Failed to compile the regular expression pattern
        printf("failed to compile regex");
        return;
    }

    regmatch_t match;
    while (regexec(&regex, str->data, 1, &match, 0) == 0) {
        // Calculate the position of the matched substring in the new string
        size_t index = match.rm_so;

        // Replace the matched substring with the new substring
        str_remove(str, index, match.rm_eo - index);
        str_insert(str, newstr, index);
    }
    regfree(&regex);
}

void str_to_upper(Str* str) {
    if (str == NULL) {
        return;
    }

    for (size_t i = 0; i < str->length; i++) {
        str->data[i] = toupper(str->data[i]);
    }
}

void str_to_lower(Str* str) {
    if (str == NULL) {
        return;
    }

    for (size_t i = 0; i < str->length; i++) {
        str->data[i] = tolower(str->data[i]);
    }
}

void str_split(const Str* str, const char* delimiter, char** substrings, int* num_substrings) {
    if (str == NULL || delimiter == NULL || substrings == NULL || num_substrings == NULL) {
        return;
    }

    *num_substrings = 0;
    char* data = str->data;
    size_t data_len = str->length;
    size_t delimiter_len = strlen(delimiter);

    // If Empty delimiter, split into individual characters
    if (delimiter_len == 0) {
        for (size_t i = 0; i < data_len; i++) {
            substrings[*num_substrings] = &data[i];
            (*num_substrings)++;
        }
        return;
    }

    char* start = data;
    char* end;

    while ((end = strstr(start, delimiter)) != NULL) {
        substrings[*num_substrings] = start;
        (*num_substrings)++;

        // Null-terminate the current substring
        *end = '\0';

        // Move the start pointer beyond the delimiter
        start = end + delimiter_len;
    }

    // Handle the last substring after the last delimiter
    size_t last_substring_len = data_len - (start - data);
    if (last_substring_len > 0) {
        substrings[*num_substrings] = start;
        (*num_substrings)++;
    }
}

bool str_match(const Str* str, const char* regex) {
    if (str == NULL || regex == NULL) {
        return false;
    }

    regex_t re;
    int ret = regcomp(&re, regex, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
        char error_msg[100];
        regerror(ret, &re, error_msg, sizeof(error_msg));
        printf("Error compiling regex: %s\n", error_msg);
        return false;
    }

    int matches = regexec(&re, str->data, 0, NULL, 0);
    if (matches != 0) {
        char error_msg[100];
        regerror(matches, &re, error_msg, sizeof(error_msg));
        printf("Error executing regex: %s\n", error_msg);
    }

    regfree(&re);

    return matches == 0;
}

void str_to_camel_case(Str* str) {
    if (str == NULL) {
        return;
    }

    char* data = str->data;
    int dest_index = 0;
    int capitalize = 1;  // Flag to indicate whether the next character should be capitalized

    // Process the string and convert to camel case
    while (data[dest_index] != '\0') {
        if (data[dest_index] == ' ' || data[dest_index] == '_') {
            capitalize = 1;  // Set the capitalize flag for the next character
        } else if (capitalize) {
            data[dest_index] = toupper(data[dest_index]);
            capitalize = 0;  // After capitalizing a character, reset the flag
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
    data[j] = '\0';

    // Update the length of the string
    str->length = j;
}

void str_to_title_case(Str* str) {
    if (str == NULL) {
        return;
    }

    char* data = str->data;
    size_t length = str->length;

    if (length > 0) {
        data[0] = toupper(data[0]);
    }

    for (size_t i = 1; i < length; i++) {
        if (data[i - 1] == ' ') {
            data[i] = toupper(data[i]);
        } else {
            data[i] = tolower(data[i]);
        }
    }
}

void str_to_snake_case(Str* str) {
    size_t length = str->length;
    char* data = str->data;

    // Convert first character to lowercase
    data[0] = tolower(data[0]);

    size_t space_count = 0;

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
    }

    // Trim the string to the new length
    data[length - space_count] = '\0';
}

void str_insert(Str* s, const char* str, size_t index) {
    if (s == NULL || str == NULL) {
        return;
    }

    if (index > s->length) {
        index = s->length;
    }

    size_t len = strlen(str);
    size_t new_length = s->length + len;

    // Check for size_t overflow
    if (new_length < s->length) {
        return;
    }

    if (new_length > s->capacity) {
        char* new_data = realloc(s->data, (new_length + 1) * sizeof(char));
        if (new_data == NULL) {
            return;
        }
        s->data = new_data;
        s->capacity = new_length;
    }

    memmove(s->data + index + len, s->data + index, s->length - index + 1);
    memcpy(s->data + index, str, len);
    s->length = new_length;
}

void str_remove(Str* s, size_t index, size_t count) {
    if (s == NULL || index > s->length)
        return;

    if (count > s->length - index) {
        count = s->length - index;
    }

    if (str_ensure_capacity(s, s->length - count)) {
        memmove(s->data + index, s->data + index + count, s->length - index - count);

        s->data[s->length - count] = '\0';
        s->length -= count;
    };
}

void str_join(const char** substrings, int count, char delimiter, char* buffer, size_t bufsize) {

    size_t substr_len = 0;
    for (int i = 0; i < count; i++) {
        substr_len += strlen(substrings[i]);
    }

    // Account for delimiter characters, except last
    // If we are joining with '\0', count is 0
    size_t joined_length = substr_len + (delimiter ? (count - 1) : 0);

    // Check if the joined string fits within the buffer
    if (joined_length >= bufsize) {
        printf("buffer size(%ld) is too small\n", bufsize);
        return;
    }

    char* current = buffer;
    for (int i = 0; i < count; i++) {
        size_t sub_len = strlen(substrings[i]);
        memcpy(current, substrings[i], sub_len);
        current += sub_len;

        if (delimiter != '\0' && i < count - 1) {
            *current++ = delimiter;
        }
    }
    *current = '\0';  // Terminate the joined string
}

void str_substring(const Str* s, size_t start, size_t end, char* substring, size_t bufsize) {

    // Bounds check on s.
    if (start > s->length || end > s->length) {
        return;
    }

    // Check buffer is big enough
    size_t len = end - start;
    if (len >= bufsize) {
        printf("buffer size(%ld) is too small\n", bufsize);
        // Adjust length to fit within buffer size
        len = bufsize - 1;
    }

    memcpy(substring, s->data + start, len);
    substring[len] = '\0';
}

void str_reverse(Str* s) {
    if (s == NULL) {
        return;
    }

    char* data = s->data;
    size_t length = s->length;

    for (int i = 0; i < length / 2; i++) {
        char temp = data[i];
        data[i] = data[length - i - 1];
        data[length - i - 1] = temp;
    }
}

int str_startswith(const Str* s, const char* prefix) {
    if (s == NULL || prefix == NULL) {
        return 0;
    }

    size_t prefix_length = strlen(prefix);
    if (prefix_length > s->length) {
        return 0;
    }

    return strncmp(s->data, prefix, prefix_length) == 0;
}

int str_endswith(const Str* s, const char* suffix) {
    if (s == NULL || suffix == NULL) {
        return 0;
    }

    size_t suffix_length = strlen(suffix);
    if (suffix_length > s->length) {
        return 0;
    }

    return strncmp(s->data + s->length - suffix_length, suffix, suffix_length) == 0;
}

char* regex_sub_match(const char* str, const char* regex, int capture_group) {
    regex_t compiled_regex;
    regmatch_t matches[2];
    int result;

    if (regcomp(&compiled_regex, regex, REG_EXTENDED) != 0) {
        printf("Regex compilation failed\n");
        return NULL;
    }

    result = regexec(&compiled_regex, str, 2, matches, 0);
    if (result != 0) {
        printf("Regex matching failed\n");
        regfree(&compiled_regex);
        return NULL;
    }

    if (matches[capture_group].rm_so == -1) {
        printf("Capture group not found\n");
        regfree(&compiled_regex);
        return NULL;
    }

    int start = matches[capture_group].rm_so;
    int end = matches[capture_group].rm_eo;
    int sub_length = end - start;

    char* sub_match = malloc((sub_length + 1) * sizeof(char));
    if (sub_match == NULL) {
        printf("Memory allocation failed\n");
        regfree(&compiled_regex);
        return NULL;
    }

    strncpy(sub_match, str + start, sub_length);
    sub_match[sub_length] = '\0';

    regfree(&compiled_regex);
    return sub_match;
}

#ifdef USE_PCRE_REGEX

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

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

    PCRE2_SIZE start = offsets[capture_group * 2];
    PCRE2_SIZE end = offsets[capture_group * 2 + 1];
    PCRE2_SIZE sub_length = end - start;

    char* sub_match = malloc((sub_length + 1) * sizeof(char));
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

char** regex_sub_matches_pcre(const char* str, const char* regex, int num_capture_groups,
                              int* num_matches) {

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
    *num_matches = match_count;

    char** sub_matches = malloc(match_count * num_capture_groups * sizeof(char*));
    if (sub_matches == NULL) {
        printf("Memory allocation failed\n");
        pcre2_match_data_free(match_data);
        pcre2_code_free(compiled_regex);
        return NULL;
    }

    PCRE2_SIZE* offsets = pcre2_get_ovector_pointer(match_data);

    for (int i = 0; i < match_count; i++) {
        for (int j = 0; j < num_capture_groups; j++) {
            PCRE2_SIZE start = offsets[(i * num_capture_groups + j) * 2];
            PCRE2_SIZE end = offsets[(i * num_capture_groups + j) * 2 + 1];
            PCRE2_SIZE sub_length = end - start;

            sub_matches[i * num_capture_groups + j] = malloc((sub_length + 1) * sizeof(char));
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