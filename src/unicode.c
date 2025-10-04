#include "../include/unicode.h"

#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

/**
 * Converts a Unicode codepoint to its UTF-8 byte sequence representation.
 *
 * UTF-8 encoding uses 1-4 bytes per codepoint:
 * - 1 byte:  U+0000 to U+007F    (0xxxxxxx)
 * - 2 bytes: U+0080 to U+07FF    (110xxxxx 10xxxxxx)
 * - 3 bytes: U+0800 to U+FFFF    (1110xxxx 10xxxxxx 10xxxxxx)
 * - 4 bytes: U+10000 to U+10FFFF (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
 *
 * @param codepoint The Unicode codepoint to convert (must be <= 0x10FFFF).
 * @param utf8 Output buffer that receives the UTF-8 bytes. Must be at least 5 bytes
 *             (4 bytes for max UTF-8 sequence + 1 for null terminator).
 *             Buffer is always null-terminated on success.
 * @note Invalid codepoints (> 0x10FFFF) result in no output and utf8[0] = '\0'.
 * @note This function does not validate surrogate pairs (0xD800-0xDFFF).
 */
void ucp_to_utf8(uint32_t codepoint, char utf8[UTF8_MAX_LEN]) {
    if (!utf8) {
        return;
    }

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
    } else {
        utf8[0] = '\0';
    }
}

/**
 * Decodes a UTF-8 byte sequence to its Unicode codepoint.
 *
 * This function validates the UTF-8 sequence for:
 * - Correct continuation byte patterns (10xxxxxx)
 * - Overlong encodings (smallest possible byte sequence)
 * - Surrogate pairs (0xD800-0xDFFF, invalid in UTF-8)
 * - Out-of-range values (> 0x10FFFF)
 *
 * @param utf8 Pointer to UTF-8 encoded byte sequence. Must not be NULL.
 * @return The decoded Unicode codepoint, or 0xFFFD (replacement character)
 *         if the sequence is invalid or malformed.
 * @note The function only examines the first complete UTF-8 sequence.
 */
uint32_t utf8_to_codepoint(const char* utf8) {
    if (!utf8) {
        return 0xFFFD;  // replacement character
    }

    uint32_t codepoint = 0;
    const uint8_t* u   = (const uint8_t*)utf8;

    if ((u[0] & 0x80) == 0) {
        codepoint = u[0];
    } else if ((u[0] & 0xE0) == 0xC0) {
        if ((u[1] & 0xC0) == 0x80) {
            codepoint = ((u[0] & 0x1FU) << 6) | (u[1] & 0x3F);
            if (codepoint < 0x80) {
                return 0xFFFD;
            }
        } else {
            return 0xFFFD;
        }
    } else if ((u[0] & 0xF0) == 0xE0) {
        if ((u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
            codepoint = ((u[0] & 0x0FU) << 12) | ((u[1] & 0x3FU) << 6) | (u[2] & 0x3F);
            if (codepoint < 0x800) {
                return 0xFFFD;
            }
            if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                return 0xFFFD;
            }
        } else {
            return 0xFFFD;
        }
    } else if ((u[0] & 0xF8) == 0xF0) {
        if ((u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
            codepoint = ((u[0] & 0x07U) << 18) | ((u[1] & 0x3FU) << 12) | ((u[2] & 0x3FU) << 6) |
                        (u[3] & 0x3F);
            if (codepoint < 0x10000) {
                return 0xFFFD;
            }
            if (codepoint > 0x10FFFF) {
                return 0xFFFD;
            }
        } else {
            return 0xFFFD;
        }
    } else {
        return 0xFFFD;
    }

    return codepoint;
}

/**
 * Counts the number of Unicode codepoints in a UTF-8 string.
 *
 * A codepoint is counted by identifying UTF-8 leading bytes, which are any byte
 * that does NOT match the continuation byte pattern (10xxxxxx). This includes:
 * - ASCII bytes (0xxxxxxx)
 * - 2-byte sequence leaders (110xxxxx)
 * - 3-byte sequence leaders (1110xxxx)
 * - 4-byte sequence leaders (11110xxx)
 *
 * @param s The null-terminated UTF-8 string. NULL returns 0.
 * @return The number of Unicode codepoints (characters) in the string.
 * @note Invalid UTF-8 sequences may result in incorrect counts.
 */
size_t utf8_count_codepoints(const char* s) {
    if (!s) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; s[i] != '\0'; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

/**
 * Counts the number of valid UTF-8 bytes in a string.
 *
 * This function validates each UTF-8 sequence by checking:
 * - Correct leading byte patterns
 * - Presence of all expected continuation bytes
 * - Proper continuation byte format (10xxxxxx)
 *
 * Invalid sequences are skipped (counted as 0 bytes), allowing partial
 * processing of corrupted UTF-8 data.
 *
 * @param s The null-terminated string to validate. NULL returns 0.
 * @return The total number of bytes in all valid UTF-8 sequences.
 * @note This does NOT validate codepoint ranges or overlong encodings.
 */
size_t utf8_valid_byte_count(const char* s) {
    if (!s) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; s[i] != '\0';) {
        unsigned char byte = (unsigned char)s[i];
        if ((byte & 0x80) == 0) {
            count++;
            i++;
        } else if ((byte & 0xE0) == 0xC0 && s[i + 1] != '\0') {
            if ((s[i + 1] & 0xC0) == 0x80) {
                count += 2;
                i += 2;
            } else {
                i++;
            }
        } else if ((byte & 0xF0) == 0xE0 && s[i + 1] != '\0' && s[i + 2] != '\0') {
            if ((s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80) {
                count += 3;
                i += 3;
            } else {
                i++;
            }
        } else if ((byte & 0xF8) == 0xF0 && s[i + 1] != '\0' && s[i + 2] != '\0' &&
                   s[i + 3] != '\0') {
            if ((s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80 &&
                (s[i + 3] & 0xC0) == 0x80) {
                count += 4;
                i += 4;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
    return count;
}

/**
 * Determines the byte length of a UTF-8 character from its first byte.
 *
 * UTF-8 character lengths by leading byte:
 * - 0x00-0x7F: 1 byte  (ASCII)
 * - 0xC0-0xDF: 2 bytes
 * - 0xE0-0xEF: 3 bytes
 * - 0xF0-0xF7: 4 bytes
 *
 * @param str Pointer to the first byte of a UTF-8 character. Must not be NULL.
 * @return The byte length (1-4), or 0 if the byte is invalid UTF-8.
 * @note This only examines the leading byte and does not validate the full sequence.
 */
size_t utf8_char_length(const char* str) {
    if (!str) {
        return 0;
    }

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
        return 0;
    }
}

/**
 * Validates whether a codepoint is within the valid Unicode range.
 *
 * @param codepoint The Unicode codepoint to validate.
 * @return true if codepoint is in range [0, 0x10FFFF], false otherwise.
 * @note This does NOT check for surrogate pairs or other invalid ranges.
 */
bool is_valid_codepoint(uint32_t codepoint) {
    return codepoint <= UNICODE_MAX_CODEPOINT;
}

/**
 * Comprehensively validates a UTF-8 encoded string.
 *
 * Validation checks include:
 * - Correct UTF-8 byte sequence structure
 * - Valid continuation bytes (10xxxxxx)
 * - No overlong encodings (shortest form requirement)
 * - No surrogate pairs (0xD800-0xDFFF)
 * - Codepoints within valid range (0x0-0x10FFFF)
 *
 * @param utf8 The null-terminated UTF-8 string to validate. NULL returns false.
 * @return true if the entire string is valid UTF-8, false otherwise.
 * @note An empty string is considered valid.
 */
bool is_valid_utf8(const char* utf8) {
    if (!utf8) {
        return false;
    }

    for (size_t i = 0; utf8[i] != '\0';) {
        unsigned char byte = (unsigned char)utf8[i];
        if ((byte & 0x80) == 0) {
            i++;
        } else if ((byte & 0xE0) == 0xC0) {
            if (utf8[i + 1] == '\0' || (utf8[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            uint32_t codepoint = (uint32_t)((byte & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
            if (codepoint < 0x80) {
                return false;
            }
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            if (utf8[i + 1] == '\0' || utf8[i + 2] == '\0' || (utf8[i + 1] & 0xC0) != 0x80 ||
                (utf8[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            uint32_t codepoint = (uint32_t)((byte & 0x0F) << 12) |
                                 (uint32_t)((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
            if (codepoint < 0x800 || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
                return false;
            }
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            if (utf8[i + 1] == '\0' || utf8[i + 2] == '\0' || utf8[i + 3] == '\0' ||
                (utf8[i + 1] & 0xC0) != 0x80 || (utf8[i + 2] & 0xC0) != 0x80 ||
                (utf8[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            uint32_t codepoint = (uint32_t)((byte & 0x07) << 18) |
                                 ((uint32_t)(utf8[i + 1] & 0x3F) << 12) |
                                 ((uint32_t)(utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
            if (codepoint < 0x10000 || codepoint > 0x10FFFF) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

/**
 * Checks if a Unicode codepoint represents whitespace.
 *
 * @param codepoint The Unicode codepoint to test.
 * @return true if the codepoint is a whitespace character per the C locale.
 * @note Uses iswspace() which respects the current locale settings.
 */
bool is_codepoint_whitespace(uint32_t codepoint) {
    return iswspace(codepoint);
}

/**
 * Checks if a UTF-8 character represents whitespace.
 *
 * @param utf8 Pointer to a UTF-8 encoded character. Must not be NULL.
 * @return true if the character is whitespace, false otherwise or on error.
 */
bool is_utf8_whitespace(const char* utf8) {
    if (!utf8) {
        return false;
    }
    return is_codepoint_whitespace(utf8_to_codepoint(utf8));
}

/**
 * Checks if a Unicode codepoint represents a digit.
 *
 * @param codepoint The Unicode codepoint to test.
 * @return true if the codepoint is a digit character per the C locale.
 */
bool is_codepoint_digit(uint32_t codepoint) {
    return iswdigit(codepoint);
}

/**
 * Checks if a UTF-8 character represents a digit.
 *
 * @param utf8 Pointer to a UTF-8 encoded character. Must not be NULL.
 * @return true if the character is a digit, false otherwise or on error.
 */
bool is_utf8_digit(const char* utf8) {
    if (!utf8) {
        return false;
    }
    return is_codepoint_digit(utf8_to_codepoint(utf8));
}

/**
 * Checks if a Unicode codepoint represents an alphabetic character.
 *
 * @param codepoint The Unicode codepoint to test.
 * @return true if the codepoint is alphabetic per the C locale.
 */
bool is_codepoint_alpha(uint32_t codepoint) {
    return iswalpha(codepoint);
}

/**
 * Checks if a UTF-8 character represents an alphabetic character.
 *
 * @param utf8 Pointer to a UTF-8 encoded character. Must not be NULL.
 * @return true if the character is alphabetic, false otherwise or on error.
 */
bool is_utf8_alpha(const char* utf8) {
    if (!utf8) {
        return false;
    }
    return is_codepoint_alpha(utf8_to_codepoint(utf8));
}

/**
 * Checks if a Unicode codepoint represents an alphanumeric character.
 *
 * @param codepoint The Unicode codepoint to test.
 * @return true if the codepoint is alphanumeric.
 */
bool is_codepoint_alnum(uint32_t codepoint) {
    return iswalnum(codepoint);
}

/**
 * Checks if a UTF-8 character represents an alphanumeric character.
 *
 * @param utf8 Pointer to a UTF-8 encoded character. Must not be NULL.
 * @return true if the character is alphanumeric, false otherwise or on error.
 */
bool is_utf8_alnum(const char* utf8) {
    if (!utf8) {
        return false;
    }
    return is_codepoint_alnum(utf8_to_codepoint(utf8));
}

/**
 * Checks if a Unicode codepoint represents a punctuation character.
 *
 * @param codepoint The Unicode codepoint to test.
 * @return true if the codepoint is punctuation per the C locale.
 */
bool is_codepoint_punct(uint32_t codepoint) {
    return iswpunct(codepoint);
}

/**
 * Checks if a UTF-8 character represents a punctuation character.
 *
 * @param utf8 Pointer to a UTF-8 encoded character. Must not be NULL.
 * @return true if the character is punctuation, false otherwise or on error.
 */
bool is_utf8_punct(const char* utf8) {
    if (!utf8) {
        return false;
    }
    return is_codepoint_punct(utf8_to_codepoint(utf8));
}

/**
 * Creates a copy of a UTF-8 string containing only valid UTF-8 sequences.
 *
 * @param data The null-terminated UTF-8 string to copy. NULL returns NULL.
 * @return A newly allocated copy of the valid UTF-8 portion, or NULL on allocation failure.
 * @note Caller must free the returned string using free().
 */
char* utf8_copy(const char* data) {
    if (!data) {
        return nullptr;
    }

    size_t length = utf8_valid_byte_count(data);
    char* copy    = (char*)malloc(length + 1);
    if (copy) {
        memcpy(copy, data, length);
        copy[length] = '\0';
    }
    return copy;
}

/**
 * Returns a pointer to the internal UTF-8 data buffer.
 *
 * @param s The utf8_string object. Must not be NULL.
 * @return Pointer to the internal null-terminated UTF-8 string, or NULL if s is NULL.
 * @note The returned pointer is owned by the utf8_string and should not be freed.
 */
const char* utf8_data(const utf8_string* s) {
    if (!s) {
        return nullptr;
    }
    return s->data;
}

/**
 * Creates a new utf8_string object from a C string.
 *
 * The string is validated and only valid UTF-8 bytes are stored.
 *
 * @param data The null-terminated UTF-8 string. NULL returns NULL.
 * @return A newly allocated utf8_string object, or NULL on allocation failure or NULL input.
 * @note Caller must free the returned object using utf8_free().
 */
utf8_string* utf8_new(const char* data) {
    if (!data) {
        return nullptr;
    }

    utf8_string* s = (utf8_string*)malloc(sizeof(utf8_string));
    if (!s) {
        return nullptr;
    }

    s->data = utf8_copy(data);
    if (!s->data) {
        free(s);
        return nullptr;
    }

    s->length = utf8_valid_byte_count(data);
    s->count  = utf8_count_codepoints(data);
    return s;
}

/**
 * Creates an empty utf8_string with preallocated capacity.
 *
 * @param capacity The initial byte capacity to allocate.
 * @return A newly allocated empty utf8_string, or NULL on allocation failure.
 * @note Caller must free the returned object using utf8_free().
 */
utf8_string* utf8_new_with_capacity(size_t capacity) {
    utf8_string* s = (utf8_string*)malloc(sizeof(utf8_string));
    if (!s) {
        return nullptr;
    }

    s->data = (char*)malloc(capacity + 1);
    if (!s->data) {
        free(s);
        return nullptr;
    }

    s->data[0] = '\0';
    s->length  = 0;
    s->count   = 0;
    return s;
}

/**
 * Frees all resources associated with a utf8_string.
 *
 * @param s The utf8_string to free. NULL is safely ignored.
 * @note After calling this function, the pointer s is invalid and should not be used.
 */
void utf8_free(utf8_string* s) {
    if (!s) {
        return;
    }

    free(s->data);
    free(s);
}

/**
 * Prints the UTF-8 string content to stdout followed by a newline.
 *
 * @param s The utf8_string to print. Must not be NULL.
 */
void utf8_print(const utf8_string* s) {
    if (!s || !s->data) {
        return;
    }
    printf("%s\n", s->data);
}

/**
 * Prints metadata about the UTF-8 string to stdout.
 *
 * @param s The utf8_string to inspect. Must not be NULL.
 */
void utf8_print_info(const utf8_string* s) {
    if (!s) {
        return;
    }
    printf("Byte Length: %zu\n", s->length);
    printf("Code Points: %zu\n", s->count);
}

/**
 * Prints the Unicode codepoints of the string in U+XXXX format.
 *
 * @param s The utf8_string to print. Must not be NULL.
 */
void utf8_print_codepoints(const utf8_string* s) {
    if (!s || !s->data) {
        return;
    }

    for (size_t i = 0; s->data[i] != '\0';) {
        uint32_t codepoint = utf8_to_codepoint(&s->data[i]);
        printf("U+%04X ", codepoint);
        size_t len = utf8_char_length(&s->data[i]);
        if (len == 0) {
            break;
        }
        i += len;
    }
    printf("\n");
}

/**
 * Finds the byte index of the first occurrence of a substring.
 *
 * @param s The utf8_string to search in. Must not be NULL.
 * @param utf8 The substring to search for. Must not be NULL.
 * @return The byte index of the first occurrence, or -1 if not found or on error.
 */
int utf8_index_of(const utf8_string* s, const char* utf8) {
    if (!s || !s->data || !utf8) {
        return -1;
    }

    char* index = strstr(s->data, utf8);
    if (index) {
        return (int)(index - s->data);
    }
    return -1;
}

/**
 * Finds the byte index of the last occurrence of a substring.
 *
 * @param s The utf8_string to search in. Must not be NULL.
 * @param utf8 The substring to search for. Must not be NULL.
 * @return The byte index of the last occurrence, or -1 if not found or on error.
 */
int utf8_last_index_of(const utf8_string* s, const char* utf8) {
    if (!s || !s->data || !utf8) {
        return -1;
    }

    size_t needle_len = strlen(utf8);
    if (needle_len == 0 || needle_len > s->length) {
        return -1;
    }

    for (size_t i = s->length - needle_len; i > 0; i--) {
        if (memcmp(&s->data[i], utf8, needle_len) == 0) {
            return (int)i;
        }
    }

    if (memcmp(s->data, utf8, needle_len) == 0) {
        return 0;
    }

    return -1;
}

/**
 * Appends UTF-8 data to the end of a utf8_string.
 *
 * @param s The utf8_string to append to. Must not be NULL.
 * @param data The null-terminated UTF-8 string to append. NULL is safely ignored.
 * @return true on success, false on allocation failure.
 */
bool utf8_append(utf8_string* s, const char* data) {
    if (!s || !data) {
        return false;
    }

    size_t length = utf8_valid_byte_count(data);
    size_t count  = utf8_count_codepoints(data);

    char* new_data = (char*)realloc(s->data, s->length + length + 1);
    if (!new_data) {
        return false;
    }

    s->data = new_data;
    memcpy(&s->data[s->length], data, length);
    s->length += length;
    s->count += count;
    s->data[s->length] = '\0';
    return true;
}

/**
 * Extracts a substring by byte range.
 *
 * @param s The source utf8_string. Must not be NULL.
 * @param index The starting byte index.
 * @param utf8_byte_len The number of bytes to extract.
 * @return A newly allocated substring, or NULL on allocation failure or invalid params.
 * @note Caller must free the returned string using free().
 * @note Does not validate that the byte range aligns with UTF-8 character boundaries.
 */
char* utf8_substr(const utf8_string* s, size_t index, size_t utf8_byte_len) {
    if (!s || !s->data || index >= s->length || utf8_byte_len == 0) {
        return nullptr;
    }

    if (index + utf8_byte_len > s->length) {
        utf8_byte_len = s->length - index;
    }

    char* substr = (char*)malloc(utf8_byte_len + 1);
    if (substr) {
        memcpy(substr, &s->data[index], utf8_byte_len);
        substr[utf8_byte_len] = '\0';
    }
    return substr;
}

/**
 * Inserts UTF-8 data at a specific byte index.
 *
 * @param s The utf8_string to modify. Must not be NULL.
 * @param index The byte index at which to insert.
 * @param data The null-terminated UTF-8 string to insert. NULL is safely ignored.
 * @return true on success, false on allocation failure or invalid index.
 */
bool utf8_insert(utf8_string* s, size_t index, const char* data) {
    if (!s || !s->data || !data || index > s->length) {
        return false;
    }

    size_t length  = utf8_valid_byte_count(data);
    size_t count   = utf8_count_codepoints(data);
    char* new_data = (char*)realloc(s->data, s->length + length + 1);

    if (!new_data) {
        return false;
    }

    s->data = new_data;
    memmove(&s->data[index + length], &s->data[index], s->length - index + 1);
    memcpy(&s->data[index], data, length);
    s->length += length;
    s->count += count;
    return true;
}

/**
 * Removes a specified number of codepoints starting at a byte index.
 *
 * @param s The utf8_string to modify. Must not be NULL.
 * @param index The starting byte index.
 * @param count The number of codepoints (not bytes) to remove.
 * @return true on success, false if parameters are invalid.
 */
bool utf8_remove(utf8_string* s, size_t index, size_t count) {
    if (!s || !s->data || index >= s->length || count == 0) {
        return false;
    }

    size_t i = index;
    for (size_t j = 0; j < count && i < s->length; j++) {
        size_t len = utf8_char_length(&s->data[i]);
        if (len == 0) {
            break;
        }
        i += len;
    }

    if (i > s->length) {
        i = s->length;
    }

    memmove(&s->data[index], &s->data[i], s->length - i + 1);
    s->length -= i - index;
    s->count -= count;
    return true;
}

/**
 * Replaces the first occurrence of a substring with another string.
 *
 * @param s The utf8_string to modify. Must not be NULL.
 * @param old_str The substring to find and replace. Must not be NULL.
 * @param new_str The replacement string. Must not be NULL.
 * @return true if replacement occurred, false if old_str not found or on error.
 */
bool utf8_replace(utf8_string* s, const char* old_str, const char* new_str) {
    if (!s || !s->data || !old_str || !new_str) {
        return false;
    }

    size_t old_byte_len = utf8_valid_byte_count(old_str);
    size_t new_byte_len = utf8_valid_byte_count(new_str);
    size_t old_count    = utf8_count_codepoints(old_str);
    size_t new_count    = utf8_count_codepoints(new_str);

    char* index = strstr(s->data, old_str);
    if (index == nullptr) {
        return false;
    }

    size_t offset = (size_t)(index - s->data);
    if (old_byte_len != new_byte_len) {
        char* new_data = (char*)realloc(s->data, s->length - old_byte_len + new_byte_len + 1);
        if (!new_data) {
            return false;
        }
        s->data = new_data;
    }

    memmove(&s->data[offset + new_byte_len], &s->data[offset + old_byte_len],
            s->length - offset - old_byte_len + 1);
    memcpy(&s->data[offset], new_str, new_byte_len);
    s->length = s->length - old_byte_len + new_byte_len;
    s->count  = s->count - old_count + new_count;
    return true;
}

/**
 * Replaces all occurrences of a substring with another string.
 *
 * @param s The utf8_string to modify. Must not be NULL.
 * @param old_str The substring to find and replace. Must not be NULL or empty.
 * @param new_str The replacement string. Must not be NULL (can be empty for deletion).
 * @return The number of replacements made, or 0 if none found or on error.
 */
size_t utf8_replace_all(utf8_string* s, const char* old_str, const char* new_str) {
    if (!s || !s->data || !old_str || !new_str) {
        return 0;
    }

    size_t old_byte_len = utf8_valid_byte_count(old_str);
    if (old_byte_len == 0) {
        return 0;
    }

    size_t new_byte_len = utf8_valid_byte_count(new_str);
    size_t old_count    = utf8_count_codepoints(old_str);
    size_t new_count    = utf8_count_codepoints(new_str);
    size_t replacements = 0;

    char* index = s->data;
    while ((index = strstr(index, old_str)) != nullptr) {
        size_t offset = (size_t)(index - s->data);

        if (old_byte_len != new_byte_len) {
            char* new_data = (char*)realloc(s->data, s->length - old_byte_len + new_byte_len + 1);
            if (!new_data) {
                return replacements;
            }
            s->data = new_data;
            index   = s->data + offset;
        }

        memmove(&s->data[offset + new_byte_len], &s->data[offset + old_byte_len],
                s->length - offset - old_byte_len + 1);
        memcpy(&s->data[offset], new_str, new_byte_len);
        s->length = s->length - old_byte_len + new_byte_len;
        s->count  = s->count - old_count + new_count;
        index += new_byte_len;
        replacements++;
    }

    return replacements;
}

/**
 * Reverses a UTF-8 string by codepoints (not bytes).
 *
 * Each complete UTF-8 character is treated as an atomic unit.
 *
 * @param s The utf8_string to reverse in-place. Must not be NULL.
 * @return true on success, false on allocation failure.
 */
bool utf8_reverse(utf8_string* s) {
    if (!s || !s->data || s->length == 0) {
        return false;
    }

    char* reversed = (char*)malloc(s->length + 1);
    if (!reversed) {
        return false;
    }

    size_t j = s->length;
    for (size_t i = 0; i < s->length;) {
        size_t len = utf8_char_length(&s->data[i]);
        if (len == 0 || j < len) {
            free(reversed);
            return false;
        }
        j -= len;
        memcpy(&reversed[j], &s->data[i], len);
        i += len;
    }
    reversed[s->length] = '\0';
    free(s->data);
    s->data = reversed;
    return true;
}

/**
 * Writes a utf8_string to a file.
 *
 * @param s The utf8_string to write. Must not be NULL.
 * @param filename The file path. Existing files are overwritten. Must not be NULL.
 * @return The number of bytes written, or -1 on error.
 */
long utf8_writeto(const utf8_string* s, const char* filename) {
    if (!s || !s->data || !filename) {
        return -1;
    }

    FILE* file = fopen(filename, "w");
    if (!file) {
        return -1;
    }

    size_t bytes = fwrite(s->data, 1, s->length, file);
    fclose(file);

    if (bytes != s->length) {
        return -1;
    }

    return (long)bytes;
}

/**
 * Reads a UTF-8 string from a file.
 *
 * @param filename The file path to read from. Must not be NULL.
 * @return A newly allocated utf8_string containing the file contents, or NULL on error.
 * @note Caller must free the returned object using utf8_free().
 */
utf8_string* utf8_readfrom(const char* filename) {
    if (!filename) {
        return nullptr;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        return nullptr;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return nullptr;
    }

    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return nullptr;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return nullptr;
    }

    char* data = (char*)malloc((size_t)length + 1);
    if (!data) {
        fclose(file);
        return nullptr;
    }

    size_t bytes = fread(data, 1, (size_t)length, file);
    data[bytes]  = '\0';
    fclose(file);

    utf8_string* s = utf8_new(data);
    free(data);
    return s;
}

/**
 * Removes leading whitespace from a UTF-8 string in-place.
 *
 * Whitespace is determined by is_utf8_whitespace() which uses locale settings.
 *
 * @param str The null-terminated UTF-8 string to modify. NULL is safely ignored.
 */
void utf8_ltrim(char* str) {
    if (!str) {
        return;
    }

    size_t len = strlen(str);
    size_t i   = 0;
    while (i < len && is_utf8_whitespace(&str[i])) {
        size_t char_len = utf8_char_length(&str[i]);
        if (char_len == 0) {
            break;
        }
        i += char_len;
    }

    if (i > 0) {
        memmove(str, &str[i], len - i + 1);
    }
}

/**
 * Removes trailing whitespace from a UTF-8 string in-place.
 *
 * Whitespace is determined by is_utf8_whitespace() which uses locale settings.
 *
 * @param str The null-terminated UTF-8 string to modify. NULL is safely ignored.
 */
void utf8_rtrim(char* str) {
    if (!str) {
        return;
    }

    size_t len = strlen(str);
    size_t i   = len;

    while (i > 0) {
        size_t char_start = i - 1;
        while (char_start > 0 && (str[char_start] & 0xC0) == 0x80) {
            char_start--;
        }

        size_t char_len = utf8_char_length(&str[char_start]);

        if (char_len > 0 && char_start + char_len == i && is_utf8_whitespace(&str[char_start])) {
            i = char_start;
        } else {
            break;
        }
    }

    str[i] = '\0';
}

/**
 * Removes leading and trailing whitespace from a UTF-8 string in-place.
 *
 * @param str The null-terminated UTF-8 string to modify. NULL is safely ignored.
 */
void utf8_trim(char* str) {
    if (!str) {
        return;
    }
    utf8_ltrim(str);
    utf8_rtrim(str);
}

/**
 * Removes leading and trailing characters from a UTF-8 string in-place.
 *
 * Any codepoint appearing in the 'chars' string will be trimmed from both ends.
 *
 * @param str The null-terminated UTF-8 string to modify. NULL is safely ignored.
 * @param chars String containing codepoints to trim. NULL is safely ignored.
 */
void utf8_trim_chars(char* str, const char* chars) {
    if (!str || !chars) {
        return;
    }

    uint32_t trim_codepoints[256] = {0};
    size_t num_trim_chars         = 0;
    size_t chars_len              = strlen(chars);

    for (size_t k = 0; k < chars_len && num_trim_chars < 256;) {
        size_t current_char_len = utf8_char_length(&chars[k]);
        if (current_char_len == 0 || k + current_char_len > chars_len) {
            k++;
            continue;
        }

        uint32_t codepoint = utf8_to_codepoint(&chars[k]);
        if (codepoint != 0xFFFD) {
            bool found = false;
            for (size_t idx = 0; idx < num_trim_chars; ++idx) {
                if (trim_codepoints[idx] == codepoint) {
                    found = true;
                    break;
                }
            }
            if (!found && num_trim_chars < 256) {
                trim_codepoints[num_trim_chars++] = codepoint;
            }
        }
        k += current_char_len;
    }

    size_t len = strlen(str);
    size_t i   = 0;
    while (i < len) {
        size_t current_char_len = utf8_char_length(&str[i]);
        if (current_char_len == 0 || i + current_char_len > len) {
            break;
        }

        uint32_t codepoint = utf8_to_codepoint(&str[i]);
        bool should_trim   = false;

        if (codepoint != 0xFFFD) {
            for (size_t j = 0; j < num_trim_chars; j++) {
                if (codepoint == trim_codepoints[j]) {
                    should_trim = true;
                    break;
                }
            }
        }

        if (!should_trim) {
            break;
        }

        i += current_char_len;
    }

    if (i > 0) {
        memmove(str, &str[i], len - i + 1);
        len -= i;
    }

    i = len;
    while (i > 0) {
        size_t char_start = i - 1;
        while (char_start > 0 && (str[char_start] & 0xC0) == 0x80) {
            char_start--;
        }

        if ((str[char_start] & 0xC0) == 0x80) {
            break;
        }

        size_t char_len = utf8_char_length(&str[char_start]);

        if (char_len > 0 && char_start + char_len == i) {
            uint32_t codepoint = utf8_to_codepoint(&str[char_start]);
            bool should_trim   = false;

            if (codepoint != 0xFFFD) {
                for (size_t j = 0; j < num_trim_chars; j++) {
                    if (codepoint == trim_codepoints[j]) {
                        should_trim = true;
                        break;
                    }
                }
            }

            if (should_trim) {
                i = char_start;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    str[i] = '\0';
}

/**
 * Removes leading and trailing occurrences of a single character.
 *
 * @param str The null-terminated UTF-8 string to modify. NULL is safely ignored.
 * @param c The ASCII character to trim (only works for single-byte chars).
 * @note This function is optimized for ASCII characters only.
 */
void utf8_trim_char(char* str, char c) {
    if (!str) {
        return;
    }

    size_t len = strlen(str);
    size_t i   = 0;
    while (i < len && str[i] == c) {
        i++;
    }

    if (i > 0) {
        memmove(str, &str[i], len - i + 1);
        len -= i;
    }

    while (len > 0 && str[len - 1] == c) {
        len--;
    }
    str[len] = '\0';
}

/**
 * Converts all uppercase characters in a UTF-8 string to lowercase in-place.
 *
 * Uses locale-aware conversion via towlower().
 *
 * @param str The null-terminated UTF-8 string to modify. NULL is safely ignored.
 * @note May change string length if case conversion alters byte count.
 */
void utf8_tolower(char* str) {
    if (!str) {
        return;
    }

    for (size_t i = 0; str[i] != '\0';) {
        uint32_t codepoint = utf8_to_codepoint(&str[i]);
        size_t old_len     = utf8_char_length(&str[i]);

        if (old_len == 0) {
            break;
        }

        if (iswupper(codepoint)) {
            char utf8[5] = {0};
            ucp_to_utf8(towlower(codepoint), utf8);
            size_t new_len = utf8_valid_byte_count(utf8);

            if (new_len != old_len) {
                size_t remaining = strlen(&str[i + old_len]);
                memmove(&str[i + new_len], &str[i + old_len], remaining + 1);
            }
            memcpy(&str[i], utf8, new_len);
            i += new_len;
        } else {
            i += old_len;
        }
    }
}

/**
 * Converts all lowercase characters in a UTF-8 string to uppercase in-place.
 *
 * Uses locale-aware conversion via towupper().
 *
 * @param str The null-terminated UTF-8 string to modify. NULL is safely ignored.
 * @note May change string length if case conversion alters byte count.
 */
void utf8_toupper(char* str) {
    if (!str) {
        return;
    }

    for (size_t i = 0; str[i] != '\0';) {
        uint32_t codepoint = utf8_to_codepoint(&str[i]);
        size_t old_len     = utf8_char_length(&str[i]);

        if (old_len == 0) {
            break;
        }

        if (iswlower(codepoint)) {
            char utf8[5] = {0};
            ucp_to_utf8(towupper(codepoint), utf8);
            size_t new_len = utf8_valid_byte_count(utf8);

            if (new_len != old_len) {
                size_t remaining = strlen(&str[i + old_len]);
                memmove(&str[i + new_len], &str[i + old_len], remaining + 1);
            }
            memcpy(&str[i], utf8, new_len);
            i += new_len;
        } else {
            i += old_len;
        }
    }
}

/**
 * Splits a UTF-8 string into parts using a delimiter.
 *
 * The string is divided at each occurrence of the delimiter. Empty parts
 * (when delimiter appears consecutively) are included in the result.
 *
 * @param str The utf8_string to split. Must not be NULL.
 * @param delim The delimiter string. Must not be NULL or empty.
 * @param num_parts Output parameter receiving the number of parts. Must not be NULL.
 * @return Array of utf8_string pointers, or NULL on error. Caller must free using
 * utf8_split_free().
 */
utf8_string** utf8_split(const utf8_string* str, const char* delim, size_t* num_parts) {
    if (!str || !str->data || !delim || !num_parts) {
        if (num_parts) {
            *num_parts = 0;
        }
        return nullptr;
    }

    size_t delim_len = utf8_valid_byte_count(delim);
    if (delim_len == 0) {
        *num_parts = 0;
        return nullptr;
    }

    size_t count = 1;
    size_t len   = str->length;
    for (size_t i = 0; i < len;) {
        if (i + delim_len <= len && utf8_starts_with(&str->data[i], delim)) {
            count++;
            i += delim_len;
        } else {
            size_t char_len = utf8_char_length(&str->data[i]);
            if (char_len == 0) {
                break;
            }
            i += char_len;
        }
    }

    utf8_string** parts = (utf8_string**)malloc(count * sizeof(utf8_string*));
    if (!parts) {
        *num_parts = 0;
        return nullptr;
    }

    size_t index = 0;
    size_t start = 0;
    for (size_t i = 0; i < len;) {
        if (i + delim_len <= len && utf8_starts_with(&str->data[i], delim)) {
            parts[index] = utf8_new(&str->data[start]);
            if (parts[index]) {
                parts[index]->data[i - start] = '\0';
                parts[index]->length          = i - start;
                parts[index]->count           = utf8_count_codepoints(parts[index]->data);
            }
            index++;
            i += delim_len;
            start = i;
        } else {
            size_t char_len = utf8_char_length(&str->data[i]);
            if (char_len == 0) {
                break;
            }
            i += char_len;
        }
    }

    parts[index] = utf8_new(&str->data[start]);
    if (parts[index]) {
        parts[index]->data[len - start] = '\0';
        parts[index]->length            = len - start;
        parts[index]->count             = utf8_count_codepoints(parts[index]->data);
    }

    *num_parts = count;
    return parts;
}

/**
 * Removes an element from a utf8_string array and frees it.
 *
 * Elements after the removed index are shifted down.
 *
 * @param array The array of utf8_string pointers. Must not be NULL.
 * @param size The current size of the array.
 * @param index The index to remove. Must be < size.
 */
void utf8_array_remove(utf8_string** array, size_t size, size_t index) {
    if (!array || index >= size) {
        return;
    }

    utf8_free(array[index]);
    for (size_t i = index; i < size - 1; i++) {
        array[i] = array[i + 1];
    }
    array[size - 1] = nullptr;
}

/**
 * Frees an array of utf8_string objects returned by utf8_split().
 *
 * @param str The array of utf8_string pointers. NULL is safely ignored.
 * @param size The number of elements in the array.
 */
void utf8_split_free(utf8_string** str, size_t size) {
    if (!str) {
        return;
    }

    for (size_t i = 0; i < size; i++) {
        utf8_free(str[i]);
    }
    free(str);
}

/**
 * Checks if a string starts with a given prefix.
 *
 * @param str The null-terminated UTF-8 string to check. Must not be NULL.
 * @param prefix The prefix to test for. Must not be NULL.
 * @return true if str starts with prefix, false otherwise.
 */
bool utf8_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) {
        return false;
    }

    size_t len = utf8_valid_byte_count(prefix);
    for (size_t i = 0; i < len; i++) {
        if (str[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

/**
 * Checks if a string ends with a given suffix.
 *
 * @param str The null-terminated UTF-8 string to check. Must not be NULL.
 * @param suffix The suffix to test for. Must not be NULL.
 * @return true if str ends with suffix, false otherwise.
 */
bool utf8_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) {
        return false;
    }

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

/**
 * Checks if a string contains a substring.
 *
 * @param str The null-terminated UTF-8 string to search in. Must not be NULL.
 * @param substr The substring to search for. Must not be NULL.
 * @return true if substr is found in str, false otherwise.
 */
bool utf8_contains(const char* str, const char* substr) {
    if (!str || !substr) {
        return false;
    }
    return strstr(str, substr) != nullptr;
}

/**
 * Compares two UTF-8 strings lexicographically.
 *
 * @param s1 First UTF-8 string. NULL is treated as empty.
 * @param s2 Second UTF-8 string. NULL is treated as empty.
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2.
 */
int utf8_compare(const char* s1, const char* s2) {
    if (!s1 && !s2) {
        return 0;
    }
    if (!s1) {
        return -1;
    }
    if (!s2) {
        return 1;
    }
    return strcmp(s1, s2);
}

/**
 * Compares two UTF-8 strings for equality.
 *
 * @param s1 First UTF-8 string. Must not be NULL.
 * @param s2 Second UTF-8 string. Must not be NULL.
 * @return true if strings are byte-for-byte identical, false otherwise.
 */
bool utf8_equals(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return s1 == s2;
    }
    return strcmp(s1, s2) == 0;
}

/**
 * Duplicates a utf8_string object.
 *
 * @param s The utf8_string to duplicate. Must not be NULL.
 * @return A newly allocated copy, or NULL on allocation failure.
 * @note Caller must free the returned object using utf8_free().
 */
utf8_string* utf8_clone(const utf8_string* s) {
    if (!s || !s->data) {
        return nullptr;
    }
    return utf8_new(s->data);
}

/**
 * Concatenates two utf8_string objects into a new string.
 *
 * @param s1 First utf8_string. Must not be NULL.
 * @param s2 Second utf8_string. Must not be NULL.
 * @return A newly allocated utf8_string containing s1 + s2, or NULL on error.
 * @note Caller must free the returned object using utf8_free().
 */
utf8_string* utf8_concat(const utf8_string* s1, const utf8_string* s2) {
    if (!s1 || !s1->data || !s2 || !s2->data) {
        return nullptr;
    }

    utf8_string* result = utf8_new_with_capacity(s1->length + s2->length);
    if (!result) {
        return nullptr;
    }

    memcpy(result->data, s1->data, s1->length);
    memcpy(result->data + s1->length, s2->data, s2->length);
    result->data[s1->length + s2->length] = '\0';
    result->length                        = s1->length + s2->length;
    result->count                         = s1->count + s2->count;

    return result;
}
