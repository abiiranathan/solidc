
#include "../include/unicode.h"

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

// Data structure for a utf-8 char string
typedef struct utf8_string {
    char* data;     // utf-8 string
    size_t length;  // number of bytes
    size_t count;   // number of codepoints
} utf8_string;

void ucp_to_utf8(uint32_t codepoint, char* utf8) {
    if (codepoint <= 0x7F) {
        utf8[0] = (char)codepoint;
        utf8[1] = '\0';
    } else if (codepoint <= 0x7FF) {
        utf8[0] = (char)(0xC0 | (codepoint >> 6));
        utf8[1] = (char)(0x80 | (codepoint & 0x3F));
        utf8[2] = '\0';
    } else if (codepoint <= 0xFFFF) {
        utf8[0] = (char)(0xE0 | (codepoint >> 12));
        utf8[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[2] = (char)(0x80 | (codepoint & 0x3F));
        utf8[3] = '\0';
    } else if (codepoint <= 0x10FFFF) {
        utf8[0] = (char)(0xF0 | (codepoint >> 18));
        utf8[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        utf8[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[3] = (char)(0x80 | (codepoint & 0x3F));
        utf8[4] = '\0';
    }
}

uint32_t utf8_to_codepoint(const char* utf8) {
    uint32_t codepoint = 0;
    if ((utf8[0] & 0x80) == 0) {
        codepoint = (unsigned char)utf8[0];
    } else if ((utf8[0] & 0xE0) == 0xC0) {
        codepoint = (utf8[0] & 0x1F) << 6;
        codepoint |= (utf8[1] & 0x3F);
    } else if ((utf8[0] & 0xF0) == 0xE0) {
        codepoint = (utf8[0] & 0x0F) << 12;
        codepoint |= (utf8[1] & 0x3F) << 6;
        codepoint |= (utf8[2] & 0x3F);
    } else if ((utf8[0] & 0xF8) == 0xF0) {
        codepoint = (utf8[0] & 0x07) << 18;
        codepoint |= (utf8[1] & 0x3F) << 12;
        codepoint |= (utf8[2] & 0x3F) << 6;
        codepoint |= (utf8[3] & 0x3F);
    }
    return codepoint;
}

// Returns the number of codepoints in the string.
size_t utf8_count_codepoints(const char* s) {
    size_t count = 0;
    for (size_t i = 0; s[i] != '\0'; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

size_t utf8_byte_length(const char* s) {
    size_t count = 0;
    for (size_t i = 0; s[i] != '\0';) {
        unsigned char byte = s[i];
        if ((byte & 0x80) == 0) {
            count++;
            i++;
        } else if ((byte & 0xE0) == 0xC0) {
            count += 2;
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            count += 3;
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            count += 4;
            i += 4;
        } else {
            // Invalid UTF-8 sequence, skip this byte
            i++;
        }
    }
    return count;
}

// Get the byte length of a UTF-8 character given its first byte
size_t utf8_char_length(const char* str) {
    uint8_t byte = (uint8_t)*str;
    if (byte <= 0x7F) {
        return 1;
    } else if (byte <= 0xDF) {
        return 2;
    } else if (byte <= 0xEF) {
        return 3;
    } else if (byte <= 0xF7) {
        return 4;
    } else {
        return 0;  // Invalid UTF-8 character
    }
}

bool is_valid_codepoint(uint32_t codepoint) {
    return codepoint <= UNICODE_MAX_CODEPOINT;
}

bool is_valid_utf8(const char* utf8) {
    for (size_t i = 0; utf8[i] != '\0'; i++) {
        if ((utf8[i] & 0xC0) == 0x80) {
            return false;
        }
    }
    return true;
}

bool is_codepoint_whitespace(uint32_t codepoint) {
    return iswspace(codepoint);
}

bool is_utf8_whitespace(const char* utf8) {
    return is_codepoint_whitespace(utf8_to_codepoint(utf8));
}

bool is_codepoint_digit(uint32_t codepoint) {
    return iswdigit(codepoint);
}

bool is_utf8_digit(const char* utf8) {
    return is_codepoint_digit(utf8_to_codepoint(utf8));
}

bool is_codepoint_alpha(uint32_t codepoint) {
    return iswalpha(codepoint);
}

bool is_utf8_alpha(const char* utf8) {
    return is_codepoint_alpha(utf8_to_codepoint(utf8));
}

char* utf8_copy(const char* data) {
    size_t length = utf8_byte_length(data);
    char* copy = (char*)malloc(length + 1);
    if (copy) {
        memcpy(copy, data, length);
        copy[length] = '\0';
    }
    return copy;
}

const char* utf8_data(const utf8_string* s) {
    return s->data;
}

utf8_string* utf8_new(const char* data) {
    utf8_string* s = (utf8_string*)malloc(sizeof(utf8_string));
    // we can't use strlen for utf-8 strings
    if (!s) {
        return NULL;
    }

    s->data = utf8_copy(data);
    s->length = utf8_byte_length(data);
    s->count = utf8_count_codepoints(data);
    return s;
}

void utf8_free(utf8_string* s) {
    if (!s) {
        return;
    }

    free(s->data);
    free(s);
    s = NULL;
}

void utf8_print(const utf8_string* s) {
    printf("%s\n", s->data);
}

void utf8_print_info(const utf8_string* s) {
    printf("Byte Length: %zu\n", s->length);
    printf("Code Points: %zu\n", s->count);
}

void utf8_print_codepoints(const utf8_string* s) {
    for (size_t i = 0; s->data[i] != '\0';) {
        uint32_t codepoint = utf8_to_codepoint(&s->data[i]);
        printf("U+%04X ", codepoint);
        i += utf8_byte_length(&s->data[i]);
    }
    printf("\n");
}

int utf8_index_of(const utf8_string* s, const char* utf8) {
    char* index = strstr(s->data, utf8);
    if (index) {
        return (int)(index - s->data);
    }
    return -1;
}

void utf8_append(utf8_string* s, const char* data) {
    size_t length = utf8_byte_length(data);
    size_t count = utf8_count_codepoints(data);

    char* new_data = (char*)realloc(s->data, s->length + length + 1);
    if (new_data) {
        s->data = new_data;
        memcpy(&s->data[s->length], data, length);
        s->length += length;
        s->count += count;
        s->data[s->length] = '\0';
    } else {
        fprintf(stderr, "realloc failed\n");
        exit(1);
    }
}

char* utf8_substr(const utf8_string* s, size_t index, size_t utf8_byte_len) {
    char* substr = (char*)malloc(utf8_byte_len + 1);
    if (substr) {
        memcpy(substr, &s->data[index], utf8_byte_len);
        substr[utf8_byte_len] = '\0';
    }
    return substr;
}

void utf8_insert(utf8_string* s, size_t index, const char* data) {
    size_t length = utf8_byte_length(data);
    size_t count = utf8_count_codepoints(data);
    char* new_data = (char*)realloc(s->data, s->length + length + 1);

    if (new_data) {
        s->data = new_data;
        memmove(&s->data[index + length], &s->data[index], s->length - index + 1);
        memcpy(&s->data[index], data, length);
        s->length += length;
        s->count += count;
    }
}

void utf8_remove(utf8_string* s, size_t index, size_t count) {
    size_t i = index;
    for (size_t j = 0; j < count; j++) {
        i += utf8_byte_length(&s->data[i]);
    }

    memmove(&s->data[index], &s->data[i], s->length - i + 1);
    s->length -= i - index;
    s->count -= count;
}

void utf8_replace(utf8_string* s, const char* old_str, const char* new_str) {
    size_t old_byte_len = utf8_byte_length(old_str);
    size_t new_byte_len = utf8_byte_length(new_str);
    size_t old_count = utf8_count_codepoints(old_str);
    size_t new_count = utf8_count_codepoints(new_str);

    char* index = s->data;
    index = strstr(index, old_str);
    if (index != NULL) {
        size_t offset = index - s->data;
        if (old_byte_len != new_byte_len) {
            char* new_data = (char*)realloc(s->data, s->length - old_byte_len + new_byte_len + 1);
            assert(new_data);
            s->data = new_data;
        }

        if (s->data) {
            memmove(&s->data[offset + new_byte_len], &s->data[offset + old_byte_len],
                    s->length - offset - old_byte_len + 1);
            memcpy(&s->data[offset], new_str, new_byte_len);
            s->length = s->length - old_byte_len + new_byte_len;
            s->count = s->count - old_count + new_count;
            index += new_byte_len;
        }
    }
}

// replace all
void utf8_replace_all(utf8_string* s, const char* old_str, const char* new_str) {
    size_t old_byte_len = utf8_byte_length(old_str);
    size_t new_byte_len = utf8_byte_length(new_str);
    size_t old_count = utf8_count_codepoints(old_str);
    size_t new_count = utf8_count_codepoints(new_str);

    char* index = s->data;
    while ((index = strstr(index, old_str)) != NULL) {
        size_t offset = index - s->data;

        // If new string is an empty string, then remove the old string
        if (new_byte_len > 0 && old_byte_len != new_byte_len) {
            char* new_data = (char*)realloc(s->data, s->length - old_byte_len + new_byte_len + 1);
            assert(new_data);
            s->data = new_data;
        }

        if (s->data) {
            memmove(&s->data[offset + new_byte_len], &s->data[offset + old_byte_len],
                    s->length - offset - old_byte_len + 1);
            memcpy(&s->data[offset], new_str, new_byte_len);
            s->length = s->length - old_byte_len + new_byte_len;
            s->count = s->count - old_count + new_count;
            index += new_byte_len;
        }
    }
}

void utf8_reverse(utf8_string* s) {
    char* reversed = (char*)malloc(s->length + 1);
    if (reversed) {
        size_t j = s->length;
        for (size_t i = 0; i < s->length;) {
            size_t len = utf8_byte_length(&s->data[i]);
            memmove(&reversed[j - len], &s->data[i], len);
            i += len;
            j -= len;
        }
        reversed[s->length] = '\0';
        free(s->data);
        s->data = reversed;
    }
}

unsigned long utf8_writeto(const utf8_string* s, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file) {
        unsigned long bytes = fwrite(s->data, 1, s->length, file);
        fclose(file);
        return bytes;
    }
    return -1;
}

utf8_string* utf8_readfrom(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        size_t length = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* data = (char*)malloc(length + 1);
        if (data) {
            size_t bytes = fread(data, 1, length, file);
            data[bytes] = '\0';
            utf8_string* s = utf8_new(data);
            free(data);
            fclose(file);
            return s;
        }
        fclose(file);
    }
    return NULL;
}

// string_ltrim removes leading whitespace from str.
void utf8_ltrim(char* str) {
    size_t len = utf8_byte_length(str);
    size_t i = 0;
    while (i < len && is_utf8_whitespace(&str[i])) {
        // IMPORTANT: we use utf8_char_length here to skip over the entire character
        // Don't use utf8_byte_length since it will calculate entire length of the string
        i += utf8_char_length(&str[i]);
    }
    memmove(str, &str[i], len - i + 1);
    str[len - i] = '\0';
}

// utf8_rtrim removes trailing whitespace from str.
void utf8_rtrim(char* str) {
    size_t len = utf8_byte_length(str);
    size_t i = len;
    while (i > 0 && is_utf8_whitespace(&str[i - 1])) {
        i -= utf8_char_length(&str[i - 1]);
    }
    str[i] = '\0';
}

// utf8_trim removes leading and trailing whitespace from str.
void utf8_trim(char* str) {
    utf8_ltrim(str);
    utf8_rtrim(str);
}

// utf8_trim_chars removes leading and trailing characters c from str.
void utf8_trim_chars(char* str, const char* c) {
    size_t len = utf8_byte_length(str);
    size_t i = 0;
    while (i < len && strchr(c, str[i]) != NULL) {
        i += utf8_char_length(&str[i]);
    }
    memmove(str, &str[i], len - i + 1);
    len = utf8_byte_length(str);
    i = len;
    while (i > 0 && strchr(c, str[i - 1]) != NULL) {
        i -= utf8_char_length(&str[i - 1]);
    }
    str[i] = '\0';
}

// utf8_trim_char removes leading and trailing character c from str.
void utf8_trim_char(char* str, char c) {
    size_t len = utf8_byte_length(str);
    size_t i = 0;
    while (i < len && str[i] == c) {
        i += utf8_char_length(&str[i]);
    }
    memmove(str, &str[i], len - i + 1);
    len = utf8_byte_length(str);
    i = len;
    while (i > 0 && str[i - 1] == c) {
        i -= utf8_char_length(&str[i - 1]);
    }
    str[i] = '\0';
}

void utf8_tolower(char* str) {
    for (size_t i = 0; str[i] != '\0';) {
        uint32_t codepoint = utf8_to_codepoint(&str[i]);
        if (iswupper(codepoint)) {
            char utf8[5];
            ucp_to_utf8(towlower(codepoint), utf8);
            size_t len = utf8_byte_length(utf8);
            memmove(&str[i], utf8, len);
            i += len;
        } else {
            i += utf8_char_length(&str[i]);
        }
    }
}

void utf8_toupper(char* str) {
    for (size_t i = 0; str[i] != '\0';) {
        uint32_t codepoint = utf8_to_codepoint(&str[i]);
        if (iswlower(codepoint)) {
            char utf8[5];
            ucp_to_utf8(towupper(codepoint), utf8);
            size_t len = utf8_byte_length(utf8);
            memmove(&str[i], utf8, len);
            i += len;
        } else {
            i += utf8_char_length(&str[i]);
        }
    }
}

// Split a string into parts using a delimiter.
// Returns an array of utf8_string pointers that need to be freed.
// The last element of the array is NULL.
utf8_string** utf8_split(const utf8_string* str, const char* delim, size_t* num_parts) {
    size_t count = 0;
    *num_parts = 0;
    size_t len = utf8_byte_length(str->data);
    for (size_t i = 0; i < len;) {
        if (utf8_starts_with(&str->data[i], delim)) {
            count++;
            i += utf8_byte_length(delim);
        } else {
            i += utf8_char_length(&str->data[i]);
        }
    }
    count++;

    utf8_string** parts = (utf8_string**)malloc(count * sizeof(utf8_string*));
    if (!parts) {
        return NULL;
    }

    size_t index = 0;
    size_t start = 0;
    for (size_t i = 0; i < len;) {
        if (utf8_starts_with(&str->data[i], delim)) {
            parts[index] = utf8_new(&str->data[start]);
            parts[index]->data[i - start] = '\0';
            parts[index]->length = i - start;
            parts[index]->count = utf8_count_codepoints(parts[index]->data);
            index++;
            i += utf8_byte_length(delim);
            start = i;
        } else {
            i += utf8_char_length(&str->data[i]);
        }
    }

    parts[index] = utf8_new(&str->data[start]);
    parts[index]->data[len - start] = '\0';
    parts[index]->length = len - start;
    parts[index]->count = utf8_count_codepoints(parts[index]->data);

    *num_parts = count;
    return parts;
}

void utf8_array_remove(utf8_string** array, size_t size, size_t index) {
    if (index >= size) {
        return;
    }

    utf8_free(array[index]);
    for (size_t i = index; i < size; i++) {
        array[i] = array[i + 1];
    }
}

void utf8_split_free(utf8_string** str, size_t size) {
    for (size_t i = 0; i < size; i++) {
        utf8_free(str[i]);
    }
    free(str);
}

bool utf8_starts_with(const char* str, const char* prefix) {
    size_t len = utf8_byte_length(prefix);
    for (size_t i = 0; i < len; i++) {
        if (str[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

bool utf8_ends_with(const char* str, const char* suffix) {
    size_t len = utf8_byte_length(str);
    size_t len2 = utf8_byte_length(suffix);
    if (len2 > len) {
        return false;
    }
    for (size_t i = 0; i < len2; i++) {
        if (str[len - len2 + i] != suffix[i]) {
            return false;
        }
    }
    return true;
}

bool utf8_contains(const char* str, const char* substr) {
    return strstr(str, substr) != NULL;
}
