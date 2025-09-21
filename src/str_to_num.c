#include "../include/str_to_num.h"
#include "../include/cmp.h"

#include <errno.h>     // for errno, ERANGE
#include <float.h>     // for FLT_EPSILON, DBL_EPSILON, FLT_MAX, DBL_MAX
#include <inttypes.h>  // for strtoimax, strtoumax, intmax_t, uintmax_t
#include <limits.h>    // for INT_MAX, UINT_MAX, etc.
#include <stdbool.h>   // for bool, true, false
#include <stdio.h>     // for fprintf, stderr
#include <stdlib.h>    // for strtod, strtof
#include <string.h>    // for strlen
#include <strings.h>   // for strcasecmp

/** Internal helper for validating string input and performing base conversion. */
static inline StoError validate_and_parse_signed(const char* str, int base, intmax_t* result) {
    if (str == nullptr || result == nullptr) {
        return STO_INVALID;
    }

    char* endptr = nullptr;
    errno        = 0;
    *result      = strtoimax(str, &endptr, base);

    // Check for conversion errors
    if (errno == ERANGE) {
        return STO_OVERFLOW;
    }

    // Check for invalid input (no conversion or partial conversion)
    if (endptr == str || *endptr != '\0') {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

/** Internal helper for validating string input and performing base conversion (unsigned). */
static inline StoError validate_and_parse_unsigned(const char* str, int base, uintmax_t* result) {
    if (str == nullptr || result == nullptr) {
        return STO_INVALID;
    }

    char* endptr = nullptr;
    errno        = 0;
    *result      = strtoumax(str, &endptr, base);

    // Check for conversion errors
    if (errno == ERANGE) {
        return STO_OVERFLOW;
    }

    // Check for invalid input (no conversion or partial conversion)
    if (endptr == str || *endptr != '\0') {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

/** Generic macro for signed integer conversion with range checking. */
#define IMPLEMENT_SIGNED_CONVERSION(func_name, type_name, type_max, type_min)                      \
    StoError func_name(const char* str, type_name* result) {                                       \
        if (result == nullptr) {                                                                   \
            return STO_INVALID;                                                                    \
        }                                                                                          \
                                                                                                   \
        intmax_t temp;                                                                             \
        StoError err = validate_and_parse_signed(str, 10, &temp);                                  \
        if (err != STO_SUCCESS) {                                                                  \
            return err;                                                                            \
        }                                                                                          \
                                                                                                   \
        if (temp > (type_max) || temp < (type_min)) {                                              \
            return STO_OVERFLOW;                                                                   \
        }                                                                                          \
                                                                                                   \
        *result = (type_name)temp;                                                                 \
        return STO_SUCCESS;                                                                        \
    }

/** Generic macro for unsigned integer conversion with range checking. */
#define IMPLEMENT_UNSIGNED_CONVERSION(func_name, type_name, type_max)                              \
    StoError func_name(const char* str, type_name* result) {                                       \
        if (result == nullptr) {                                                                   \
            return STO_INVALID;                                                                    \
        }                                                                                          \
                                                                                                   \
        uintmax_t temp;                                                                            \
        StoError err = validate_and_parse_unsigned(str, 10, &temp);                                \
        if (err != STO_SUCCESS) {                                                                  \
            return err;                                                                            \
        }                                                                                          \
                                                                                                   \
        if (temp > (type_max)) {                                                                   \
            return STO_OVERFLOW;                                                                   \
        }                                                                                          \
                                                                                                   \
        *result = (type_name)temp;                                                                 \
        return STO_SUCCESS;                                                                        \
    }

/** Generic macro for signed integer conversion with custom base. */
#define IMPLEMENT_SIGNED_BASE_CONVERSION(func_name, type_name, type_max, type_min)                 \
    StoError func_name(const char* str, int base, type_name* result) {                             \
        if (result == nullptr) {                                                                   \
            return STO_INVALID;                                                                    \
        }                                                                                          \
                                                                                                   \
        intmax_t temp;                                                                             \
        StoError err = validate_and_parse_signed(str, base, &temp);                                \
        if (err != STO_SUCCESS) {                                                                  \
            return err;                                                                            \
        }                                                                                          \
                                                                                                   \
        if (temp > (type_max) || temp < (type_min)) {                                              \
            return STO_OVERFLOW;                                                                   \
        }                                                                                          \
                                                                                                   \
        *result = (type_name)temp;                                                                 \
        return STO_SUCCESS;                                                                        \
    }

/** Generic macro for unsigned integer conversion with custom base. */
#define IMPLEMENT_UNSIGNED_BASE_CONVERSION(func_name, type_name, type_max)                         \
    StoError func_name(const char* str, int base, type_name* result) {                             \
        if (result == nullptr) {                                                                   \
            return STO_INVALID;                                                                    \
        }                                                                                          \
                                                                                                   \
        uintmax_t temp;                                                                            \
        StoError err = validate_and_parse_unsigned(str, base, &temp);                              \
        if (err != STO_SUCCESS) {                                                                  \
            return err;                                                                            \
        }                                                                                          \
                                                                                                   \
        if (temp > (type_max)) {                                                                   \
            return STO_OVERFLOW;                                                                   \
        }                                                                                          \
                                                                                                   \
        *result = (type_name)temp;                                                                 \
        return STO_SUCCESS;                                                                        \
    }

// Generate all the integer conversion functions using macros
IMPLEMENT_SIGNED_CONVERSION(str_to_i8, int8_t, INT8_MAX, INT8_MIN)
IMPLEMENT_UNSIGNED_CONVERSION(str_to_u8, uint8_t, UINT8_MAX)

IMPLEMENT_SIGNED_CONVERSION(str_to_i16, int16_t, INT16_MAX, INT16_MIN)
IMPLEMENT_UNSIGNED_CONVERSION(str_to_u16, uint16_t, UINT16_MAX)

IMPLEMENT_SIGNED_CONVERSION(str_to_i32, int32_t, INT32_MAX, INT32_MIN)
IMPLEMENT_UNSIGNED_CONVERSION(str_to_u32, uint32_t, UINT32_MAX)

IMPLEMENT_SIGNED_CONVERSION(str_to_i64, int64_t, INT64_MAX, INT64_MIN)
IMPLEMENT_UNSIGNED_CONVERSION(str_to_u64, uint64_t, UINT64_MAX)

IMPLEMENT_SIGNED_CONVERSION(str_to_int, int, INT_MAX, INT_MIN)
IMPLEMENT_UNSIGNED_CONVERSION(str_to_uint, unsigned int, UINT_MAX)

IMPLEMENT_SIGNED_CONVERSION(str_to_long, long, LONG_MAX, LONG_MIN)
IMPLEMENT_UNSIGNED_CONVERSION(str_to_ulong, unsigned long, ULONG_MAX)

// Base conversion variants
IMPLEMENT_SIGNED_BASE_CONVERSION(str_to_int_base, int, INT_MAX, INT_MIN)
IMPLEMENT_SIGNED_BASE_CONVERSION(str_to_long_base, long, LONG_MAX, LONG_MIN)
IMPLEMENT_UNSIGNED_BASE_CONVERSION(str_to_ulong_base, unsigned long, ULONG_MAX)

// Special case for uintptr_t (no upper bound check needed as it can hold any uintmax_t value)
StoError str_to_uintptr(const char* str, uintptr_t* result) {
    if (result == nullptr) {
        return STO_INVALID;
    }

    uintmax_t temp;
    StoError err = validate_and_parse_unsigned(str, 10, &temp);
    if (err != STO_SUCCESS) {
        return err;
    }

    // uintptr_t should be able to hold any valid pointer value
    // On most platforms, uintptr_t == uintmax_t, but we cast safely
    *result = (uintptr_t)temp;
    return STO_SUCCESS;
}

StoError str_to_float(const char* str, float* result) {
    if (str == nullptr || result == nullptr) {
        return STO_INVALID;
    }

    char* endptr = nullptr;
    errno        = 0;
    *result      = strtof(str, &endptr);

    // Check for invalid input
    if (endptr == str || *endptr != '\0') {
        return STO_INVALID;
    }

    // Check for overflow/underflow
    if (errno == ERANGE) {
        // Use comparison function for floating point overflow detection
        const cmp_config_t config = {.epsilon = FLT_EPSILON};
        if (cmp_float(*result, FLT_MAX, config) || cmp_float(*result, -FLT_MAX, config)) {
            return STO_OVERFLOW;
        }
    }

    return STO_SUCCESS;
}

StoError str_to_double(const char* str, double* result) {
    if (str == nullptr || result == nullptr) {
        return STO_INVALID;
    }

    char* endptr = nullptr;
    errno        = 0;
    *result      = strtod(str, &endptr);

    // Check for invalid input
    if (endptr == str || *endptr != '\0') {
        return STO_INVALID;
    }

    // Check for overflow/underflow
    if (errno == ERANGE) {
        // Use comparison function for floating point overflow detection
        const cmp_config_t config = {.epsilon = DBL_EPSILON};
        if (cmp_double(*result, DBL_MAX, config) || cmp_double(*result, -DBL_MAX, config)) {
            return STO_OVERFLOW;
        }
    }

    return STO_SUCCESS;
}

/** Truth value lookup table for efficient boolean parsing. */
typedef struct {
    const char* str;
    bool value;
} bool_mapping_t;

static const bool_mapping_t BOOL_MAPPINGS[] = {
    {"true", true}, {"false", false}, {"yes", true}, {"no", false},
    {"on", true},   {"off", false},   {"1", true},   {"0", false},
};

static const size_t BOOL_MAPPINGS_COUNT = sizeof(BOOL_MAPPINGS) / sizeof(BOOL_MAPPINGS[0]);

StoError str_to_bool(const char* str, bool* result) {
    if (str == nullptr || result == nullptr) {
        return STO_INVALID;
    }

    // Use lookup table for efficient case-insensitive matching
    for (size_t i = 0; i < BOOL_MAPPINGS_COUNT; i++) {
        if (strcasecmp(str, BOOL_MAPPINGS[i].str) == 0) {
            *result = BOOL_MAPPINGS[i].value;
            return STO_SUCCESS;
        }
    }

    return STO_INVALID;
}

const char* sto_error_string(StoError code) {
    switch (code) {
        case STO_SUCCESS:
            return "Conversion successful";
        case STO_INVALID:
            return "Invalid input string or null pointer";
        case STO_OVERFLOW:
            return "Numeric overflow or underflow";
        default:
            return "Unknown error code";
    }
}
