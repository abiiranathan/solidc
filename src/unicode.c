
#include "../include/unicode.h"

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

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
    const uint8_t* u   = (const uint8_t*)utf8;

    if ((u[0] & 0x80) == 0) {
        // 1-byte sequence (ASCII)
        codepoint = u[0];
    } else if ((u[0] & 0xE0) == 0xC0) {
        // 2-byte sequence
        if ((u[1] & 0xC0) == 0x80) {  // Validate continuation byte
            codepoint = ((u[0] & 0x1FU) << 6) | (u[1] & 0x3F);
            // Overlong encoding check
            if (codepoint < 0x80) {
                return 0xFFFD;  // Replacement character
            }
        } else {
            return 0xFFFD;  // Replacement character
        }
    } else if ((u[0] & 0xF0) == 0xE0) {
        // 3-byte sequence
        if ((u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {  // Validate continuation bytes
            codepoint = ((u[0] & 0x0FU) << 12) | ((u[1] & 0x3FU) << 6) | (u[2] & 0x3F);
            // Overlong encoding check
            if (codepoint < 0x800) {
                return 0xFFFD;  // Replacement character
            }
            // Surrogate check
            if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                return 0xFFFD;  // Replacement character
            }
        } else {
            return 0xFFFD;  // Replacement character
        }
    } else if ((u[0] & 0xF8) == 0xF0) {
        // 4-byte sequence
        if ((u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {  // Validate continuation bytes
            codepoint = ((u[0] & 0x07U) << 18) | ((u[1] & 0x3FU) << 12) | ((u[2] & 0x3FU) << 6) | (u[3] & 0x3F);
            // Overlong encoding check
            if (codepoint < 0x10000) {
                return 0xFFFD;  // Replacement character
            }
            // Too large check
            if (codepoint > 0x10FFFF) {
                return 0xFFFD;  // Replacement character
            }
        } else {
            return 0xFFFD;  // Replacement character
        }
    } else {
        return 0xFFFD;  // Replacement character
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

size_t utf8_valid_byte_count(const char* s) {
    size_t count = 0;
    for (size_t i = 0; s[i] != '\0';) {
        unsigned char byte = (unsigned char)s[i];
        if ((byte & 0x80) == 0) {
            count++;
            i++;
        } else if ((byte & 0xE0) == 0xC0 && s[i + 1] != '\0') {
            // Check that next byte is a continuation byte
            if ((s[i + 1] & 0xC0) == 0x80) {
                count += 2;
                i += 2;
            } else {
                // Invalid UTF-8 sequence, skip this byte
                i++;
            }
        } else if ((byte & 0xF0) == 0xE0 && s[i + 1] != '\0' && s[i + 2] != '\0') {
            // Check that next 2 bytes are continuation bytes
            if ((s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80) {
                count += 3;
                i += 3;
            } else {
                // Invalid UTF-8 sequence, skip this byte
                i++;
            }
        } else if ((byte & 0xF8) == 0xF0 && s[i + 1] != '\0' && s[i + 2] != '\0' && s[i + 3] != '\0') {
            // Check that next 3 bytes are continuation bytes
            if ((s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80 && (s[i + 3] & 0xC0) == 0x80) {
                count += 4;
                i += 4;
            } else {
                // Invalid UTF-8 sequence, skip this byte
                i++;
            }
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
    for (size_t i = 0; utf8[i] != '\0';) {
        unsigned char byte = (unsigned char)utf8[i];
        if ((byte & 0x80) == 0) {
            // Single byte character (ASCII)
            i++;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (utf8[i + 1] == '\0' || (utf8[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding
            uint32_t codepoint = ((byte & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
            if (codepoint < 0x80) {
                return false;
            }
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (utf8[i + 1] == '\0' || utf8[i + 2] == '\0' || (utf8[i + 1] & 0xC0) != 0x80 ||
                (utf8[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding or surrogate
            uint32_t codepoint = ((byte & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
            if (codepoint < 0x800 || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
                return false;
            }
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (utf8[i + 1] == '\0' || utf8[i + 2] == '\0' || utf8[i + 3] == '\0' || (utf8[i + 1] & 0xC0) != 0x80 ||
                (utf8[i + 2] & 0xC0) != 0x80 || (utf8[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding or too large value
            uint32_t codepoint = ((byte & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) |
                                 (utf8[i + 3] & 0x3F);
            if (codepoint < 0x10000 || codepoint > 0x10FFFF) {
                return false;
            }
            i += 4;

        } else {
            // Invalid leading byte
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
    size_t length = utf8_valid_byte_count(data);
    char* copy    = (char*)malloc(length + 1);
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
    if (!data) {
        return NULL;
    }

    utf8_string* s = (utf8_string*)malloc(sizeof(utf8_string));
    if (!s) {
        return NULL;
    }

    s->data = utf8_copy(data);
    if (!s->data) {
        free(s);
        return NULL;
    }

    s->length = utf8_valid_byte_count(data);
    s->count  = utf8_count_codepoints(data);
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
        i += utf8_valid_byte_count(&s->data[i]);
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
    size_t length = utf8_valid_byte_count(data);
    size_t count  = utf8_count_codepoints(data);

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
    size_t length  = utf8_valid_byte_count(data);
    size_t count   = utf8_count_codepoints(data);
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
        i += utf8_valid_byte_count(&s->data[i]);
    }

    memmove(&s->data[index], &s->data[i], s->length - i + 1);
    s->length -= i - index;
    s->count -= count;
}

void utf8_replace(utf8_string* s, const char* old_str, const char* new_str) {
    size_t old_byte_len = utf8_valid_byte_count(old_str);
    size_t new_byte_len = utf8_valid_byte_count(new_str);
    size_t old_count    = utf8_count_codepoints(old_str);
    size_t new_count    = utf8_count_codepoints(new_str);

    char* index = s->data;
    index       = strstr(index, old_str);
    if (index != NULL) {
        size_t offset = (size_t)(index - s->data);
        if (old_byte_len != new_byte_len) {
            char* new_data = (char*)realloc(s->data, s->length - old_byte_len + new_byte_len + 1);
            assert(new_data);
            s->data = new_data;
        }

        if (s->data) {
            memmove(&s->data[offset + new_byte_len],
                    &s->data[offset + old_byte_len],
                    s->length - offset - old_byte_len + 1);
            memcpy(&s->data[offset], new_str, new_byte_len);
            s->length = s->length - old_byte_len + new_byte_len;
            s->count  = s->count - old_count + new_count;
            index += new_byte_len;
        }
    }
}

// replace all
void utf8_replace_all(utf8_string* s, const char* old_str, const char* new_str) {
    size_t old_byte_len = utf8_valid_byte_count(old_str);
    size_t new_byte_len = utf8_valid_byte_count(new_str);
    size_t old_count    = utf8_count_codepoints(old_str);
    size_t new_count    = utf8_count_codepoints(new_str);

    char* index = s->data;
    while ((index = strstr(index, old_str)) != NULL) {
        size_t offset = (size_t)(index - s->data);

        // If new string is an empty string, then remove the old string
        if (new_byte_len > 0 && old_byte_len != new_byte_len) {
            char* new_data = (char*)realloc(s->data, s->length - old_byte_len + new_byte_len + 1);
            assert(new_data);
            s->data = new_data;
        }

        if (s->data) {
            memmove(&s->data[offset + new_byte_len],
                    &s->data[offset + old_byte_len],
                    s->length - offset - old_byte_len + 1);
            memcpy(&s->data[offset], new_str, new_byte_len);
            s->length = s->length - old_byte_len + new_byte_len;
            s->count  = s->count - old_count + new_count;
            index += new_byte_len;
        }
    }
}

void utf8_reverse(utf8_string* s) {
    char* reversed = (char*)malloc(s->length + 1);
    if (reversed) {
        size_t j = s->length;
        for (size_t i = 0; i < s->length;) {
            size_t len = utf8_char_length(&s->data[i]);
            j -= len;
            memcpy(&reversed[j], &s->data[i], len);
            i += len;
        }
        reversed[s->length] = '\0';
        free(s->data);
        s->data = reversed;
    }
}

long utf8_writeto(const utf8_string* s, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file) {
        unsigned long bytes = fwrite(s->data, 1, s->length, file);
        fclose(file);
        if (bytes > 0) {
            return (long)bytes;
        }
    }
    return -1;
}

utf8_string* utf8_readfrom(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* data = (char*)malloc((size_t)length + 1);
        if (data) {
            size_t bytes   = fread(data, 1, (size_t)length, file);
            data[bytes]    = '\0';
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
    size_t len = utf8_valid_byte_count(str);
    size_t i   = 0;
    while (i < len && is_utf8_whitespace(&str[i])) {
        // IMPORTANT: we use utf8_char_length here to skip over the entire
        // character Don't use utf8_byte_length since it will calculate entire
        // length of the string
        i += utf8_char_length(&str[i]);
    }
    memmove(str, &str[i], len - i + 1);
    str[len - i] = '\0';
}

// utf8_rtrim removes trailing whitespace from str.
void utf8_rtrim(char* str) {
    size_t len = strlen(str);  // Start from the actual byte length
    size_t i   = len;          // i represents the potential new null terminator position

    while (i > 0) {
        // Find the starting byte of the character that ends *before* index i
        size_t char_start = i - 1;
        // Move backwards past any continuation bytes (10xxxxxx)
        while (char_start > 0 && (str[char_start] & 0xC0) == 0x80) {
            char_start--;
        }

        // Now char_start points to the beginning of the last character sequence before i
        // (or i-1 if it's a single-byte char)

        // Check if the character sequence starting at char_start is valid UTF-8
        // and represents a whitespace codepoint.
        // We also need to ensure the character actually *ends* where we expect it to (at i-1).
        // A simple way is to check if the length matches the distance.
        size_t char_len = utf8_char_length(&str[char_start]);

        // Basic validation: Does the calculated length match the distance from the start?
        // And is the character within the original string bounds?
        // And is it actually whitespace?
        if (char_len > 0 && char_start + char_len == i && is_utf8_whitespace(&str[char_start])) {
            // It's a valid whitespace character ending at i-1.
            // Move the potential null terminator position back to the start of this character.
            i = char_start;
        } else {
            // Either it's not whitespace, or it's an invalid/incomplete sequence
            // ending at i-1. Stop trimming.
            break;
        }
    }

    // Place the null terminator at the determined position.
    str[i] = '\0';
}

// utf8_trim removes leading and trailing whitespace from str.
void utf8_trim(char* str) {
    utf8_ltrim(str);
    utf8_rtrim(str);
}

// utf8_trim_chars removes leading and trailing characters specified in chars from str.
void utf8_trim_chars(char* str, const char* chars) {
    // --- Build the set of codepoints to trim ---
    // Consider making this dynamic or much larger if 'chars' could contain
    // more than 256 distinct Unicode codepoints.
    uint32_t trim_codepoints[256] = {0};
    size_t num_trim_chars         = 0;
    size_t chars_len              = strlen(chars);  // Use strlen for iterating through chars

    for (size_t k = 0; k < chars_len && num_trim_chars < 255;) {  // Prevent overflow
        // Get length of char *first* to advance correctly, even if invalid
        size_t current_char_len = utf8_char_length(&chars[k]);
        if (current_char_len == 0) {  // Handle invalid byte in chars string
            k++;                      // Skip invalid starting byte
            continue;
        }
        // Check if reading the char goes beyond the bounds
        if (k + current_char_len > chars_len) {
            break;  // Avoid reading past end if last char is incomplete
        }

        uint32_t codepoint = utf8_to_codepoint(&chars[k]);
        // Optional: Add check here if codepoint is 0xFFFD (replacement char)
        // and decide if you want to trim based on that.
        if (codepoint != 0xFFFD) {  // Avoid adding replacement char unless intended
            // Simple check to avoid duplicates (could use a better set structure)
            bool found = false;
            for (size_t idx = 0; idx < num_trim_chars; ++idx) {
                if (trim_codepoints[idx] == codepoint) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                trim_codepoints[num_trim_chars++] = codepoint;
            }
        }
        k += current_char_len;  // Advance by the actual byte length
    }

    // --- Trim leading characters ---
    size_t len = strlen(str);
    size_t i   = 0;
    while (i < len) {
        size_t current_char_len = utf8_char_length(&str[i]);
        if (current_char_len == 0) {  // Invalid start byte
            break;                    // Stop trimming if invalid UTF-8 encountered
        }
        if (i + current_char_len > len) {
            break;  // Incomplete character at the end
        }

        uint32_t codepoint = utf8_to_codepoint(&str[i]);
        bool should_trim   = false;

        if (codepoint != 0xFFFD) {  // Don't trim replacement char unless it's in chars
            for (size_t j = 0; j < num_trim_chars; j++) {
                if (codepoint == trim_codepoints[j]) {
                    should_trim = true;
                    break;
                }
            }
        }

        if (!should_trim) {
            break;  // Found first non-trim character
        }

        // Advance by the actual length of the character we just processed
        i += current_char_len;
    }

    // Perform the move if leading characters were trimmed
    if (i > 0) {
        memmove(str, &str[i], len - i + 1);  // +1 to include null terminator
        len -= i;                            // Update length
    }

    // --- Trim trailing characters (FIXED) ---
    i = len;  // Start at the potential new null terminator position
    while (i > 0) {
        // Find the starting byte of the character sequence ending *before* index i
        size_t char_start = i - 1;
        while (char_start > 0 && (str[char_start] & 0xC0) == 0x80) {
            // Move backwards past continuation bytes (10xxxxxx)
            char_start--;
        }
        // Ensure char_start is still within bounds if i was initially 1
        if ((str[char_start] & 0xC0) == 0x80) {
            // We scanned back past the beginning, indicates invalid leading byte(s)
            break;
        }

        // Now char_start points to the potential beginning of the last character before i.
        size_t char_len = utf8_char_length(&str[char_start]);

        // Validate: Does the calculated length make sense and match the position?
        if (char_len > 0 && char_start + char_len == i) {
            uint32_t codepoint = utf8_to_codepoint(&str[char_start]);
            bool should_trim   = false;

            if (codepoint != 0xFFFD) {  // Don't trim replacement char unless it's in chars
                for (size_t j = 0; j < num_trim_chars; j++) {
                    if (codepoint == trim_codepoints[j]) {
                        should_trim = true;
                        break;
                    }
                }
            }

            if (should_trim) {
                // It's a character to trim ending at i-1.
                // Move the potential null terminator position back.
                i = char_start;
            } else {
                // Found the last character that should *not* be trimmed.
                break;
            }
        } else {
            // Invalid UTF-8 sequence ending at i-1, or length mismatch. Stop trimming.
            break;
        }
    }

    // Place the null terminator at the final correct position.
    str[i] = '\0';
}

// utf8_trim_char removes leading and trailing character c from str.
void utf8_trim_char(char* str, char c) {
    size_t len = utf8_valid_byte_count(str);
    size_t i   = 0;
    while (i < len && str[i] == c) {
        i += utf8_char_length(&str[i]);
    }
    memmove(str, &str[i], len - i + 1);
    len = utf8_valid_byte_count(str);
    i   = len;
    while (i > 0 && str[i - 1] == c) {
        i -= utf8_char_length(&str[i - 1]);
    }
    str[i] = '\0';
}

void utf8_tolower(char* str) {
    for (size_t i = 0; str[i] != '\0';) {
        uint32_t codepoint = utf8_to_codepoint(&str[i]);
        if (iswupper(codepoint)) {
            char utf8[5] = {0};
            ucp_to_utf8(towlower(codepoint), utf8);
            size_t len = utf8_valid_byte_count(utf8);
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
            char utf8[5] = {0};
            ucp_to_utf8(towupper(codepoint), utf8);
            size_t len = utf8_valid_byte_count(utf8);
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
    *num_parts   = 0;
    size_t len   = utf8_valid_byte_count(str->data);
    for (size_t i = 0; i < len;) {
        if (utf8_starts_with(&str->data[i], delim)) {
            count++;
            i += utf8_valid_byte_count(delim);
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
            parts[index]                  = utf8_new(&str->data[start]);
            parts[index]->data[i - start] = '\0';
            parts[index]->length          = i - start;
            parts[index]->count           = utf8_count_codepoints(parts[index]->data);
            index++;
            i += utf8_valid_byte_count(delim);
            start = i;
        } else {
            i += utf8_char_length(&str->data[i]);
        }
    }

    parts[index]                    = utf8_new(&str->data[start]);
    parts[index]->data[len - start] = '\0';
    parts[index]->length            = len - start;
    parts[index]->count             = utf8_count_codepoints(parts[index]->data);

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
    size_t len = utf8_valid_byte_count(prefix);
    for (size_t i = 0; i < len; i++) {
        if (str[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

bool utf8_ends_with(const char* str, const char* suffix) {
    size_t len  = utf8_valid_byte_count(str);
    size_t len2 = utf8_valid_byte_count(suffix);
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
