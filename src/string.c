#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/cstr.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Small String Optimization (SSO) constants
#// Small String Optimization (SSO) constants
#define CSTR_SSO_BUFFER_SIZE (sizeof(size_t) + sizeof(size_t) + sizeof(char*) - 1)
#define CSTR_SSO_CAPACITY_ON_STACK CSTR_SSO_BUFFER_SIZE
#define CSTR_MAX_STACK_LEN (CSTR_SSO_CAPACITY_ON_STACK - 1)

/**
 * A dynamically resizable C-string with Small String Optimization (SSO).
 * Stores small strings (up to CSTR_MAX_STACK_LEN) on the stack, larger ones on the heap.
 */
typedef struct cstr {
    bool is_heap;  // Flag indicating if the string is heap-allocated
    union {
        struct {
            size_t length;    // Length of the string (excluding null terminator)
            size_t capacity;  // Total allocated space (including null terminator)
            char* data;       // Pointer to heap-allocated string
        } heap;
        struct {
            char data[CSTR_SSO_BUFFER_SIZE];  // Buffer for stack-allocated string
            unsigned char len;                // Length of the stack string
        } stack;
    };
} cstr;

// --- Internal Helper Functions ---

/**
 * Checks if the string is stored on the heap.
 * @param s Pointer to the cstr.
 * @return True if the string is heap-allocated, false if stack-allocated.
 */
static inline bool cstr_is_heap(const cstr* s) {
    return s->is_heap;
}

/**
 * Retrieves the length of the string.
 * @param s Pointer to the cstr.
 * @return The length of the string (excluding null terminator).
 */
static inline size_t cstr_get_length(const cstr* s) {
    return s->is_heap ? s->heap.length : s->stack.len;
}

/**
 * Retrieves a pointer to the string data.
 * @param s Pointer to the cstr.
 * @return Pointer to the string data (null-terminated).
 */
static inline char* cstr_get_data(cstr* s) {
    return s->is_heap ? s->heap.data : s->stack.data;
}

/**
 * Retrieves a const pointer to the string data.
 * @param s Pointer to the cstr.
 * @return Const pointer to the string data (null-terminated).
 */
static inline const char* cstr_get_data_const(const cstr* s) {
    return s->is_heap ? s->heap.data : s->stack.data;
}

/**
 * Retrieves the current capacity of the string.
 * @param s Pointer to the cstr.
 * @return The capacity (including space for null terminator).
 */
static inline size_t cstr_get_capacity(const cstr* s) {
    return s->is_heap ? s->heap.capacity : CSTR_SSO_CAPACITY_ON_STACK;
}

/**
 * Sets the length of the string (excluding null terminator).
 * Assumes sufficient capacity; caller must ensure null termination.
 * @param s Pointer to the cstr.
 * @param len New length of the string.
 */
static inline void cstr_set_length(cstr* s, size_t len) {
    if (s->is_heap) {
        s->heap.length = len;
    } else {
        assert(len <= CSTR_MAX_STACK_LEN);  // Length must fit in buffer
        s->stack.len = (unsigned char)len;
    }
}

/**
 * Rounds up the requested capacity to the next power of STR_RESIZE_FACTOR.
 * @param capacity Requested capacity (including null terminator).
 * @return Rounded capacity, or SIZE_MAX on overflow.
 */
static size_t str_round_capacity(size_t capacity) {
    size_t rounded = STR_MIN_CAPACITY;
    while (rounded < capacity) {
        rounded = (size_t)((double)rounded * STR_RESIZE_FACTOR);
        if (rounded == 0) {  // Overflow protection
            return SIZE_MAX;
        }
    }
    return rounded;
}

/**
 * Promotes a stack-allocated string to the heap.
 * @param s Pointer to the cstr (currently stack-allocated).
 * @param capacity_needed Minimum capacity required (including null terminator).
 * @return True on success, false on allocation failure or overflow.
 */
static bool cstr_promote_to_heap(cstr* s, size_t capacity_needed) {
    // Precondition: string is stack-allocated
    assert(!s->is_heap && "cstr_promote_to_heap called on heap-allocated string");

    // Extract stack data before modifying the struct
    size_t current_len     = s->stack.len;
    const char* stack_data = s->stack.data;  // Pointer to stack data, valid now

    // Allocate new heap memory
    size_t new_capacity = str_round_capacity(capacity_needed);
    if (new_capacity == SIZE_MAX) {
        fprintf(stderr, "cstr_promote_to_heap() failed: size_t overflow\n");
        return false;
    }

    char* new_data = malloc(new_capacity);
    if (!new_data) {
        perror("cstr_promote_to_heap() failed");
        return false;
    }

    // Copy stack data directly to heap allocation
    memcpy(new_data, stack_data, current_len + 1);  // Copy including null terminator

    // Transition to heap mode
    s->is_heap       = true;
    s->heap.data     = new_data;
    s->heap.length   = current_len;
    s->heap.capacity = new_capacity;
    return true;
}

/**
 * Ensures the string has at least the specified capacity.
 * @param s Pointer to the cstr.
 * @param capacity_needed Minimum capacity required (including null terminator).
 * @return True on success, false on allocation failure or overflow.
 */
static bool cstr_ensure_capacity(cstr* s, size_t capacity_needed) {
    if (capacity_needed <= cstr_get_capacity(s)) {
        return true;
    }

    if (s->is_heap) {
        size_t new_capacity = str_round_capacity(capacity_needed);
        if (new_capacity == SIZE_MAX) {
            return false;
        }
        char* new_data = realloc(s->heap.data, new_capacity);
        if (!new_data) {
            return false;
        }
        s->heap.data     = new_data;
        s->heap.capacity = new_capacity;
    } else {
        return cstr_promote_to_heap(s, capacity_needed);
    }
    return true;
}

// --- Public API Functions ---

/**
 * Creates a new empty cstr with at least the specified capacity.
 * @param initial_capacity Requested capacity (including null terminator).
 * @return Pointer to the new cstr, or NULL on allocation failure.
 */
cstr* str_new(size_t initial_capacity) {
    cstr* s = malloc(sizeof(cstr));
    if (!s) {
        return NULL;
    }

    initial_capacity = MAX(initial_capacity, 1);  // Ensure space for null terminator

    if (initial_capacity <= CSTR_SSO_CAPACITY_ON_STACK) {
        // Initialize as stack-allocated string
        s->is_heap       = false;
        s->stack.len     = 0;
        s->stack.data[0] = '\0';
    } else {
        // Initialize as heap-allocated string
        size_t capacity = str_round_capacity(initial_capacity);
        if (capacity == SIZE_MAX) {
            free(s);
            return NULL;
        }

        char* data = calloc(1, capacity);
        if (!data) {
            free(s);
            return NULL;
        }

        data[0] = '\0';

        s->is_heap       = true;
        s->heap.data     = data;
        s->heap.length   = 0;
        s->heap.capacity = capacity;
    }
    return s;
}

// Other functions remain largely unchanged, using the new helper functions with 'is_heap' checks...
// Example: str_from function
cstr* str_from(const char* c_str) {
    if (!c_str) {
        return NULL;
    }
    size_t len    = strlen(c_str);
    size_t needed = len + 1;
    cstr* s       = str_new(needed);
    if (!s) {
        return NULL;
    }

    char* data = cstr_get_data(s);
    memcpy(data, c_str, needed);
    cstr_set_length(s, len);
    return s;
}

/**
 * Checks if the string is stored on the heap.
 * @param s Pointer to the cstr.
 * @return True if the string is heap-allocated, false if stack-allocated.
 */
bool str_allocated(const cstr* s) {
    return cstr_is_heap(s);
}

/**
 * Creates a new cstr from a formatted string.
 * @param format Format string (like printf).
 * @param ... Arguments for the format string.
 * @return Pointer to the new cstr, or NULL on allocation failure or formatting error.
 */
cstr* str_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (len < 0) {
        va_end(args);
        return NULL;
    }

    size_t capacity = (size_t)len + 1;
    cstr* s         = str_new(capacity);
    if (!s) {
        va_end(args);
        return NULL;
    }

    char* data          = cstr_get_data(s);
    size_t str_capacity = cstr_get_capacity(s);

    int written = vsnprintf(data, str_capacity, format, args);
    va_end(args);

    if (written < 0) {
        str_free(s);
        return NULL;
    }
    cstr_set_length(s, MIN((size_t)written, str_capacity - 1));
    data[cstr_get_length(s)] = '\0';
    return s;
}

/**
 * Frees the cstr and its associated memory.
 * @param s Pointer to the cstr to free.
 */
void str_free(cstr* s) {
    if (!s) {
        return;
    }

    if (cstr_is_heap(s)) {
        // printf("Freed heap string %p\n", s->heap.data);
        free(s->heap.data);
    } else {
        // printf("Stack string %p not freed\n", s->heap.data);
    }
    free(s);
}

/**
 * Returns the length of the cstr.
 * @param s Pointer to the cstr.
 * @return Length of the string (excluding null terminator), or 0 if s is NULL.
 */
size_t str_len(const cstr* s) {
    return s ? cstr_get_length(s) : 0;
}

/**
 * Returns the capacity of the cstr.
 * @param s Pointer to the cstr.
 * @return Capacity (including null terminator), or 0 if s is NULL.
 */
size_t str_capacity(const cstr* s) {
    return s ? cstr_get_capacity(s) : 0;
}

/**
 * Checks if the cstr is empty.
 * @param s Pointer to the cstr.
 * @return True if the cstr is NULL or empty, false otherwise.
 */
bool str_empty(const cstr* s) {
    return !s || cstr_get_length(s) == 0;
}

/**
 * Resizes the cstr to have at least the specified capacity.
 * @param s_ptr Pointer to the cstr pointer.
 * @param new_capacity Minimum capacity required (including null terminator).
 * @return True on success, false on allocation failure or if s_ptr/s is NULL.
 */
bool str_resize(cstr** s_ptr, size_t new_capacity) {
    if (!s_ptr || !*s_ptr) {
        return false;
    }
    return cstr_ensure_capacity(*s_ptr, new_capacity);
}

/**
 * Appends a C-string to the cstr.
 * @param s_ptr Pointer to the cstr pointer.
 * @param append_str C-string to append.
 * @return True on success, false on allocation failure or invalid input.
 */
bool str_append(cstr** s_ptr, const char* append_str) {
    if (!s_ptr || !*s_ptr || !append_str) {
        return false;
    }

    cstr* s           = *s_ptr;
    size_t append_len = strlen(append_str);
    if (append_len == 0) {
        return true;
    }
    size_t new_capacity = cstr_get_length(s) + append_len + 1;
    if (!cstr_ensure_capacity(s, new_capacity)) {
        return false;
    }

    char* data = cstr_get_data(s);
    memcpy(data + cstr_get_length(s), append_str, append_len + 1);
    cstr_set_length(s, cstr_get_length(s) + append_len);
    return true;
}

/**
 * Appends a C-string to the cstr, assuming sufficient capacity.
 * @param s_ptr Pointer to the cstr pointer.
 * @param append_str C-string to append.
 * @return True on success, false on invalid input.
 */
bool str_append_fast(cstr** s_ptr, const char* append_str) {
    if (!s_ptr || !*s_ptr || !append_str) {
        return false;
    }

    cstr* s           = *s_ptr;
    size_t append_len = strlen(append_str);
    if (append_len == 0) {
        return true;
    }

    assert(cstr_get_length(s) + append_len + 1 <= cstr_get_capacity(s));  // Caller ensures capacity

    char* data = cstr_get_data(s);
    memcpy(data + cstr_get_length(s), append_str, append_len + 1);
    cstr_set_length(s, cstr_get_length(s) + append_len);
    return true;
}

/**
 * Appends a formatted string to the cstr.
 * @param s_ptr Pointer to the cstr pointer.
 * @param format Format string (like printf).
 * @param ... Arguments for the format string.
 * @return True on success, false on allocation failure or formatting error.
 */
bool str_append_fmt(cstr** s_ptr, const char* format, ...) {
    if (!s_ptr || !*s_ptr || !format) {
        return false;
    }
    cstr* s = *s_ptr;
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int append_len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (append_len <= 0) {
        va_end(args);
        return append_len == 0;
    }
    size_t new_capacity = cstr_get_length(s) + (size_t)append_len + 1;
    if (!cstr_ensure_capacity(s, new_capacity)) {
        va_end(args);
        return false;
    }
    char* data      = cstr_get_data(s);
    size_t capacity = cstr_get_capacity(s) - cstr_get_length(s);
    int written     = vsnprintf(data + cstr_get_length(s), capacity, format, args);
    va_end(args);
    if (written < 0) {
        return false;
    }
    cstr_set_length(s, cstr_get_length(s) + MIN((size_t)written, capacity - 1));
    data[cstr_get_length(s)] = '\0';
    return true;
}

/**
 * Appends a single character to the cstr.
 * @param s_ptr Pointer to the cstr pointer.
 * @param c Character to append.
 * @return True on success, false on allocation failure or invalid input.
 */
bool str_append_char(cstr** s_ptr, char c) {
    if (!s_ptr || !*s_ptr) {
        return false;
    }
    cstr* s             = *s_ptr;
    size_t new_capacity = cstr_get_length(s) + 2;  // Char + null terminator
    if (!cstr_ensure_capacity(s, new_capacity)) {
        return false;
    }
    char* data    = cstr_get_data(s);
    size_t len    = cstr_get_length(s);
    data[len]     = c;
    data[len + 1] = '\0';
    cstr_set_length(s, len + 1);
    return true;
}

/**
 * Prepends a C-string to the cstr.
 * @param s_ptr Pointer to the cstr pointer.
 * @param prepend_str C-string to prepend.
 * @return True on success, false on allocation failure or invalid input.
 */
bool str_prepend(cstr** s_ptr, const char* prepend_str) {
    if (!s_ptr || !*s_ptr || !prepend_str) {
        return false;
    }
    cstr* s            = *s_ptr;
    size_t prepend_len = strlen(prepend_str);
    if (prepend_len == 0) {
        return true;
    }
    size_t new_capacity = cstr_get_length(s) + prepend_len + 1;
    if (!cstr_ensure_capacity(s, new_capacity)) {
        return false;
    }
    char* data = cstr_get_data(s);
    memmove(data + prepend_len, data, cstr_get_length(s) + 1);
    memcpy(data, prepend_str, prepend_len);
    cstr_set_length(s, cstr_get_length(s) + prepend_len);
    return true;
}

/**
 * Prepends a C-string to the cstr, assuming sufficient capacity.
 * @param s_ptr Pointer to the cstr pointer.
 * @param prepend_str C-string to prepend.
 * @return True on success, false on invalid input.
 */
bool str_prepend_fast(cstr** s_ptr, const char* prepend_str) {
    if (!s_ptr || !*s_ptr || !prepend_str) {
        return false;
    }
    cstr* s            = *s_ptr;
    size_t prepend_len = strlen(prepend_str);
    if (prepend_len == 0) {
        return true;
    }

    assert(cstr_get_length(s) + prepend_len + 1 <= cstr_get_capacity(s));  // Caller ensures capacity

    char* data = cstr_get_data(s);
    memmove(data + prepend_len, data, cstr_get_length(s) + 1);
    memcpy(data, prepend_str, prepend_len);
    cstr_set_length(s, cstr_get_length(s) + prepend_len);
    return true;
}

/**
 * Inserts a C-string at the specified index in the cstr.
 * @param s_ptr Pointer to the cstr pointer.
 * @param index Insertion position.
 * @param insert_str C-string to insert.
 * @return True on success, false on allocation failure or invalid input/index.
 */
bool str_insert(cstr** s_ptr, size_t index, const char* insert_str) {
    if (!s_ptr || !*s_ptr || !insert_str || index > cstr_get_length(*s_ptr)) {
        return false;
    }
    cstr* s           = *s_ptr;
    size_t insert_len = strlen(insert_str);
    if (insert_len == 0) {
        return true;
    }
    size_t new_capacity = cstr_get_length(s) + insert_len + 1;
    if (!cstr_ensure_capacity(s, new_capacity)) {
        return false;
    }
    char* data = cstr_get_data(s);
    memmove(data + index + insert_len, data + index, cstr_get_length(s) - index + 1);
    memcpy(data + index, insert_str, insert_len);
    cstr_set_length(s, cstr_get_length(s) + insert_len);
    return true;
}

/**
 * Removes a range of characters from the cstr.
 * @param s_ptr Pointer to the cstr pointer.
 * @param index Starting index of the range to remove.
 * @param count Number of characters to remove.
 * @return True on success, false on invalid input or index.
 */
bool str_remove(cstr** s_ptr, size_t index, size_t count) {
    if (!s_ptr || !*s_ptr || index >= cstr_get_length(*s_ptr)) {
        return index == cstr_get_length(*s_ptr) && count == 0;
    }
    cstr* s = *s_ptr;
    count   = MIN(count, cstr_get_length(s) - index);
    if (count == 0) {
        return true;
    }
    char* data = cstr_get_data(s);
    memmove(data + index, data + index + count, cstr_get_length(s) - index - count + 1);
    cstr_set_length(s, cstr_get_length(s) - count);
    return true;
}

/**
 * Clears the cstr, setting its length to 0.
 * @param s Pointer to the cstr.
 */
void str_clear(cstr* s) {
    if (s) {
        cstr_set_length(s, 0);
        cstr_get_data(s)[0] = '\0';
    }
}

/**
 * Removes all occurrences of a substring from the cstr.
 * @param s_ptr Pointer to the cstr pointer.
 * @param substr Substring to remove.
 * @return Number of occurrences removed.
 */
size_t str_remove_all(cstr** s_ptr, const char* substr) {
    if (!s_ptr || !*s_ptr || !substr || !*substr) {
        return 0;
    }
    cstr* s           = *s_ptr;
    size_t substr_len = strlen(substr);
    char* data        = cstr_get_data(s);
    char* write_ptr   = data;
    char* read_ptr    = data;
    size_t count      = 0;
    const char* end   = data + cstr_get_length(s);

    while (read_ptr < end) {
        if ((size_t)(end - read_ptr) >= substr_len && strncmp(read_ptr, substr, substr_len) == 0) {
            read_ptr += substr_len;
            count++;
        } else {
            *write_ptr++ = *read_ptr++;
        }
    }
    *write_ptr = '\0';
    cstr_set_length(s, (size_t)(write_ptr - data));
    return count;
}

/**
 * Returns the character at the specified index.
 * @param s Pointer to the cstr.
 * @param index Index of the character.
 * @return The character at the index, or '\0' if s is NULL or index is out of bounds.
 */
char str_at(const cstr* s, size_t index) {
    return (s && index < cstr_get_length(s)) ? cstr_get_data_const(s)[index] : '\0';
}

/**
 * Returns a pointer to the cstr's data.
 * @param s Pointer to the cstr.
 * @return Pointer to the null-terminated string, or NULL if s is NULL.
 */
char* str_data(cstr* s) {
    return s ? cstr_get_data(s) : NULL;
}

/**
 * Returns a pointer to the cstr's data.
 * @param s Pointer to the cstr.
 * @return Pointer to the null-terminated string, or NULL if s is NULL.
 */
const char* str_data_const(const cstr* s) {
    return s ? cstr_get_data_const(s) : NULL;
}

/**
 * Converts the cstr to a string view.
 * @param s Pointer to the cstr.
 * @return A str_view representing the cstr's data and length.
 */
str_view str_as_view(const cstr* s) {
    str_view view = {NULL, 0};
    if (s) {
        view.data   = cstr_get_data_const(s);
        view.length = cstr_get_length(s);
    }
    return view;
}

/**
 * Compares two cstrs lexicographically.
 * @param s1 First cstr.
 * @param s2 Second cstr.
 * @return Negative if s1 < s2, zero if s1 == s2, positive if s1 > s2.
 */
int str_compare(const cstr* s1, const cstr* s2) {
    if (!s1 && !s2) {
        return 0;
    }
    if (!s1) {
        return -1;
    }
    if (!s2) {
        return 1;
    }
    return strcmp(cstr_get_data_const(s1), cstr_get_data_const(s2));
}

/**
 * Checks if two cstrs are equal.
 * @param s1 First cstr.
 * @param s2 Second cstr.
 * @return True if the cstrs are equal, false otherwise.
 */
bool str_equals(const cstr* s1, const cstr* s2) {
    if (s1 == s2) {
        return true;  // Same pointer or both NULL
    }
    if (!s1 || !s2) {
        return false;  // One is NULL, other isn't
    }
    size_t len1 = cstr_get_length(s1);
    size_t len2 = cstr_get_length(s2);
    if (len1 != len2) {
        return false;  // Different lengths
    }
    return memcmp(cstr_get_data_const(s1), cstr_get_data_const(s2), len1) == 0;
}

/**
 * Checks if the cstr starts with the given prefix.
 * @param s Pointer to the cstr.
 * @param prefix Prefix to check.
 * @return True if the cstr starts with the prefix, false otherwise.
 */
bool str_starts_with(const cstr* s, const char* prefix) {
    if (!s || !prefix) {
        return false;
    }
    size_t prefix_len = strlen(prefix);
    return prefix_len == 0 ||
           (prefix_len <= cstr_get_length(s) && memcmp(cstr_get_data_const(s), prefix, prefix_len) == 0);
}

/**
 * Checks if the cstr ends with the given suffix.
 * @param s Pointer to the cstr.
 * @param suffix Suffix to check.
 * @return True if the cstr ends with the suffix, false otherwise.
 */
bool str_ends_with(const cstr* s, const char* suffix) {
    if (!s || !suffix) {
        return false;
    }
    size_t suffix_len = strlen(suffix);
    size_t s_len      = cstr_get_length(s);
    return suffix_len == 0 ||
           (suffix_len <= s_len && memcmp(cstr_get_data_const(s) + s_len - suffix_len, suffix, suffix_len) == 0);
}

/**
 * Finds the first occurrence of a substring in the cstr.
 * @param s Pointer to the cstr.
 * @param substr Substring to find.
 * @return Index of the first occurrence, or STR_NPOS if not found.
 */
int str_find(const cstr* s, const char* substr) {
    if (!s || !substr) {
        return STR_NPOS;
    }
    const char* found = strstr(cstr_get_data_const(s), substr);
    return found ? (int)(found - cstr_get_data_const(s)) : STR_NPOS;
}

/**
 * Finds the last occurrence of a substring in the cstr.
 * @param s Pointer to behaves like the original `str_remove_substr`, renamed for clarity.
 * @param s Pointer to the cstr.
 * @param start_index Starting index of the range to remove.
 * @param length Number of characters to remove.
 */
void str_remove_range(cstr* s, size_t start_index, size_t length) {
    if (!s || start_index >= cstr_get_length(s) || length == 0) {
        return;
    }
    length     = MIN(length, cstr_get_length(s) - start_index);
    char* data = cstr_get_data(s);
    memmove(data + start_index, data + start_index + length, cstr_get_length(s) - start_index - length + 1);
    cstr_set_length(s, cstr_get_length(s) - length);
}

/**
 * Removes all occurrences of a character from the cstr.
 * @param s Pointer to the cstr.
 * @param c Character to remove.
 */
void str_remove_char(cstr* s, char c) {
    if (!s || cstr_get_length(s) == 0) {
        return;
    }

    char* data      = cstr_get_data(s);
    size_t len      = cstr_get_length(s);
    char* write_ptr = data;
    char* read_ptr  = data;

    for (size_t i = 0; i < len; i++, read_ptr++) {
        if (*read_ptr != c) {
            *write_ptr++ = *read_ptr;
        }
    }
    *write_ptr = '\0';
    cstr_set_length(s, (size_t)(write_ptr - data));
}

// Remove characters in str from start index, up to start + length.
void str_remove_substr(cstr* str, size_t start, size_t substr_length) {
    if (!str)
        return;

    size_t length = cstr_get_length(str);
    if (start >= length) {
        fprintf(stderr, "[ERROR]: Start: %lu is >= length of the string %lu\n", start, length);
        return;
    }

    if (start + substr_length > length) {
        substr_length = length - start;
    }

    char* data = cstr_get_data(str);
    memmove(data + start, data + start + substr_length, length - start - substr_length + 1);
    length -= substr_length;
    cstr_set_length(str, length);
}

/**
 * Extracts a substring from the cstr.
 * @param s Pointer to the cstr.
 * @param start Starting index of the substring.
 * @param length Length of the substring.
 * @return Pointer to the new cstr containing the substring, or NULL on invalid input.
 */
cstr* str_substr(const cstr* s, size_t start, size_t length) {
    if (!s || start >= cstr_get_length(s)) {
        return NULL;
    }
    length       = MIN(length, cstr_get_length(s) - start);
    cstr* result = str_new(length + 1);
    if (!result) {
        return NULL;
    }
    char* data = cstr_get_data(result);
    memcpy(data, cstr_get_data_const(s) + start, length);
    data[length] = '\0';
    cstr_set_length(result, length);
    return result;
}

/**
 * Replaces all occurrences of a substring with another.
 * @param s Pointer to the cstr.
 * @param old_sub Substring to replace.
 * @param new_sub Replacement substring.
 * @return Pointer to the new cstr with replacements, or NULL on invalid input.
 */
cstr* str_replace_all(const cstr* s, const char* old_sub, const char* new_sub) {
    if (!s || !old_sub || !new_sub) {
        return NULL;
    }
    size_t old_len = strlen(old_sub);
    if (old_len == 0) {
        return str_from(cstr_get_data_const(s));
    }
    size_t count = str_count_substr(s, old_sub);
    if (count == 0) {
        return str_from(cstr_get_data_const(s));
    }
    size_t new_len    = strlen(new_sub);
    size_t result_len = cstr_get_length(s) + count * (new_len > old_len ? new_len - old_len : 0);
    if (new_len < old_len) {
        result_len = cstr_get_length(s) - count * (old_len - new_len);
    }
    cstr* result = str_new(result_len + 1);
    if (!result) {
        return NULL;
    }
    char* dest      = cstr_get_data(result);
    const char* src = cstr_get_data_const(s);
    size_t pos      = 0;
    const char* p   = src;
    const char* end = src + cstr_get_length(s);

    while (p < end) {
        if ((size_t)(end - p) >= old_len && strncmp(p, old_sub, old_len) == 0) {
            memcpy(dest + pos, new_sub, new_len);
            pos += new_len;
            p += old_len;
        } else {
            dest[pos++] = *p++;
        }
    }
    dest[pos] = '\0';
    cstr_set_length(result, pos);
    return result;
}

// Count the number of occurrences of a substring within the string.
size_t str_count_substr(const cstr* str, const char* substr) {
    size_t count   = 0;
    size_t sub_len = strlen(substr);
    const char* p  = cstr_get_data_const(str);
    while ((p = strstr(p, substr))) {
        count++;
        p += sub_len;
    }
    return count;
}

int str_rfind(const cstr* s, const char* substr) {
    if (!s || !substr || !*substr)
        return STR_NPOS;

    size_t substr_len = strlen(substr);
    if (substr_len > cstr_get_length(s))
        return STR_NPOS;

    for (size_t i = cstr_get_length(s) - substr_len + 1; i > 0; --i) {
        if (memcmp(cstr_get_data((cstr*)s) + i - 1, substr, substr_len) == 0) {
            return (int)(i - 1);
        }
    }
    return STR_NPOS;
}

/**
 * Replaces the first occurrence of a substring with another in the cstr.
 * @param s Pointer to the cstr.
 * @param old_str Substring to replace.
 * @param new_str Replacement substring.
 * @return Pointer to the new cstr with the first occurrence replaced, or NULL on invalid input or allocation failure.
 */
cstr* str_replace(const cstr* s, const char* old_str, const char* new_str) {
    if (!s || !old_str || !new_str) {
        return NULL;
    }

    size_t old_len = strlen(old_str);
    if (old_len == 0) {
        return str_from(cstr_get_data_const(s));  // Return a copy if old_str is empty
    }

    const char* s_data = cstr_get_data_const(s);
    const char* found  = strstr(s_data, old_str);
    if (!found) {
        return str_from(s_data);  // Return a copy if no occurrence is found
    }

    size_t s_len      = cstr_get_length(s);
    size_t new_len    = strlen(new_str);
    size_t pos        = (size_t)(found - s_data);
    size_t result_len = s_len - MIN(old_len, s_len - pos) + new_len;

    cstr* result = str_new(result_len + 1);
    if (!result) {
        return NULL;
    }

    char* dest = cstr_get_data(result);
    // Copy up to the found position
    if (pos > 0) {
        memcpy(dest, s_data, pos);
    }
    // Copy the new string
    memcpy(dest + pos, new_str, new_len);
    // Copy the remaining part of the original string
    size_t remaining_len = s_len - pos - old_len;
    if (remaining_len > 0) {
        memcpy(dest + pos + new_len, s_data + pos + old_len, remaining_len);
    }
    dest[result_len] = '\0';
    cstr_set_length(result, result_len);

    return result;
}

/**
 * Splits the cstr into substrings based on a delimiter.
 * @param s Pointer to the cstr.
 * @param delim Delimiter string.
 * @param count_out Pointer to store the number of resulting substrings.
 * @return Array of cstr pointers, or NULL on invalid input or allocation failure.
 */
cstr** str_split(const cstr* s, const char* delim, size_t* count_out) {
    if (!s || !delim || !count_out) {
        if (count_out)
            *count_out = 0;
        return NULL;
    }
    size_t delim_len = strlen(delim);
    if (delim_len == 0) {
        *count_out    = 1;
        cstr** result = malloc(sizeof(cstr*));
        if (!result) {
            *count_out = 0;
            return NULL;
        }
        result[0] = str_from(cstr_get_data_const(s));
        if (!result[0]) {
            free(result);
            *count_out = 0;
            return NULL;
        }
        return result;
    }

    size_t max_splits = (cstr_get_length(s) / delim_len) + 2;
    cstr** result     = malloc(max_splits * sizeof(cstr*));
    if (!result) {
        *count_out = 0;
        return NULL;
    }

    *count_out      = 0;
    const char* p   = cstr_get_data_const(s);
    const char* end = p + cstr_get_length(s);

    while (p <= end) {
        const char* next_delim = strstr(p, delim);
        size_t token_len       = next_delim ? (size_t)(next_delim - p) : (size_t)(end - p);

        if (*count_out >= max_splits) {
            max_splits *= 2;
            cstr** temp = realloc(result, max_splits * sizeof(cstr*));
            if (!temp) {
                for (size_t i = 0; i < *count_out; i++) {
                    str_free(result[i]);
                }
                free(result);
                *count_out = 0;
                return NULL;
            }
            result = temp;
        }

        result[*count_out] = str_new(token_len + 1);
        if (!result[*count_out]) {
            for (size_t i = 0; i < *count_out; i++) {
                str_free(result[i]);
            }
            free(result);
            *count_out = 0;
            return NULL;
        }
        char* data = cstr_get_data(result[*count_out]);
        memcpy(data, p, token_len);
        data[token_len] = '\0';
        cstr_set_length(result[*count_out], token_len);
        (*count_out)++;

        if (!next_delim || next_delim >= end) {
            break;
        }
        p = next_delim + delim_len;
    }
    return result;
}

/**
 * Joins multiple cstrs with a delimiter.
 * @param strings Array of cstr pointers.
 * @param count Number of cstrs in the array.
 * @param delim Delimiter string (can be NULL for no delimiter).
 * @return Pointer to the new cstr with joined strings, or NULL on invalid input.
 */
cstr* str_join(const cstr** strings, size_t count, const char* delim) {
    if (!strings || count == 0) {
        return str_new(1);
    }
    size_t delim_len = delim ? strlen(delim) : 0;
    size_t total_len = 0;
    for (size_t i = 0; i < count; i++) {
        if (!strings[i]) {
            return NULL;
        }
        total_len += cstr_get_length(strings[i]);
    }
    total_len += (count > 1) ? (count - 1) * delim_len : 0;

    cstr* result = str_new(total_len + 1);
    if (!result) {
        return NULL;
    }
    char* dest = cstr_get_data(result);
    size_t pos = 0;

    for (size_t i = 0; i < count; i++) {
        size_t len = cstr_get_length(strings[i]);
        if (len > 0) {
            memcpy(dest + pos, cstr_get_data_const(strings[i]), len);
            pos += len;
        }
        if (delim_len > 0 && i < count - 1) {
            memcpy(dest + pos, delim, delim_len);
            pos += delim_len;
        }
    }
    dest[pos] = '\0';
    cstr_set_length(result, pos);
    return result;
}

/**
 * Creates a new cstr with the contents of the input cstr reversed.
 * @param s Pointer to the cstr.
 * @return Pointer to the new reversed cstr, or NULL on invalid input or allocation failure.
 */
cstr* str_reverse(const cstr* s) {
    if (!s || cstr_get_length(s) == 0) {
        return NULL;
    }
    cstr* result = str_new(cstr_get_length(s) + 1);
    if (!result) {
        return NULL;
    }
    char* dest      = cstr_get_data(result);
    const char* src = cstr_get_data_const(s);
    size_t len      = cstr_get_length(s);
    for (size_t i = 0; i < len; i++) {
        dest[i] = src[len - 1 - i];
    }
    dest[len] = '\0';
    cstr_set_length(result, len);
    return result;
}

/**
 * Reverses the contents of the cstr in place.
 * @param s Pointer to the cstr.
 */
void str_reverse_in_place(cstr* s) {
    if (!s || cstr_get_length(s) < 2) {
        return;
    }
    char* data = cstr_get_data(s);
    size_t len = cstr_get_length(s);
    for (size_t i = 0; i < len / 2; i++) {
        char temp         = data[i];
        data[i]           = data[len - 1 - i];
        data[len - 1 - i] = temp;
    }
}

void str_to_lower(cstr* s) {
    if (!s)
        return;

    char* data    = cstr_get_data(s);
    size_t length = cstr_get_length(s);
    for (size_t i = 0; i < length; ++i) {
        data[i] = (char)tolower(data[i]);
    }
}

void str_to_upper(cstr* s) {
    if (!s)
        return;
    size_t length = cstr_get_length(s);
    char* data    = cstr_get_data(s);
    for (size_t i = 0; i < length; ++i) {
        data[i] = (char)toupper(data[i]);
    }
}

void str_snake_case(cstr* s) {
    if (!s)
        return;
    size_t length = cstr_get_length(s);
    char* data    = cstr_get_data(s);

    for (size_t i = 0; i < length; ++i) {
        if (isupper(data[i])) {
            data[i] = (char)tolower(data[i]);
            if (i > 0) {
                str_insert(&s, i, "_");
                ++i;
            }
        }
    }
}

void str_camel_case(cstr* s) {
    if (!s) {
        return;
    }

    size_t length = cstr_get_length(s);
    if (length == 0) {
        return;
    }

    size_t read = 0, write = 0;
    bool capitalize_next = false;
    char* data           = cstr_get_data(s);

    // First character should be lowercase
    data[write++] = (char)tolower(data[read++]);

    while (read < length) {
        char c = data[read++];

        if (c == ' ' || c == '_') {
            capitalize_next = true;
        } else if (capitalize_next) {
            data[write++]   = (char)toupper(c);
            capitalize_next = false;
        } else {
            data[write++] = (char)tolower(c);
        }
    }

    data[write] = '\0';
    cstr_set_length(s, write);
}

void str_pascal_case(cstr* s) {
    if (!s) {
        return;
    }

    size_t length = cstr_get_length(s);
    if (length == 0) {
        return;
    }

    size_t read = 0, write = 0;
    char* data = cstr_get_data(s);

    // Start with true to capitalize the first letter
    bool new_word = true;

    while (read < length) {
        char c = data[read++];

        if (c == ' ' || c == '_') {
            new_word = true;
        } else if (new_word) {
            data[write++] = (char)toupper(c);
            new_word      = false;
        } else if (isupper(c) && read < length && islower(data[read])) {
            // If current char is uppercase and next char is lowercase,
            // it's the start of a new word in camelCase
            data[write++] = c;
            new_word      = false;
        } else {
            data[write++] = (char)tolower(c);
        }
    }

    data[write] = '\0';
    cstr_set_length(s, write);
}

void str_title_case(cstr* s) {
    size_t length = cstr_get_length(s);
    char* data    = cstr_get_data(s);

    int capitalize = 1;
    for (size_t i = 0; i < length; i++) {
        if (data[i] == ' ') {
            capitalize = 1;
        } else if (capitalize) {
            data[i]    = (char)toupper(data[i]);
            capitalize = 0;
        } else {
            data[i] = (char)tolower(data[i]);
        }
    }
}

void str_trim(cstr* s) {
    if (!s) {
        return;
    }

    size_t length = cstr_get_length(s);
    if (length == 0) {
        return;
    }

    size_t start = 0, end = length - 1;
    char* data = cstr_get_data(s);

    while (start < length && isspace(data[start]))
        ++start;
    while (end > start && isspace(data[end]))
        --end;

    length = end - start + 1;
    memmove(data, data + start, length);
    data[length] = '\0';
    cstr_set_length(s, length);
}

void str_rtrim(cstr* s) {
    if (!s) {
        return;
    }

    size_t length = cstr_get_length(s);
    if (length == 0) {
        return;
    }

    size_t end = length - 1;
    char* data = cstr_get_data(s);
    while (end > 0 && isspace(data[end]))
        --end;

    length       = end + 1;
    data[length] = '\0';
    cstr_set_length(s, length);
}

void str_ltrim(cstr* s) {
    if (!s) {
        return;
    }

    size_t length = cstr_get_length(s);
    if (length == 0) {
        return;
    }

    size_t start = 0;
    char* data   = cstr_get_data(s);

    while (start < length && isspace(data[start]))
        ++start;

    length -= start;
    memmove(data, data + start, length);
    data[length] = '\0';
    cstr_set_length(s, length);
}

void str_trim_chars(cstr* s, const char* chars) {
    if (!s) {
        return;
    }

    size_t length = cstr_get_length(s);
    if (length == 0) {
        return;
    }
    char* data = cstr_get_data(s);

    // Remove leading characters
    char* start = data;
    while (strchr(chars, *start)) {
        start++;
    }

    // Remove trailing characters
    char* end = data + length - 1;
    while (strchr(chars, *end)) {
        end--;  // move the pointer to the left
    }

    size_t len = (size_t)(end - start + 1);  // length of the remaining string
    memmove(data, start, len);               // move the remaining string to the beginning
    data[len] = '\0';
    cstr_set_length(s, len);
}
