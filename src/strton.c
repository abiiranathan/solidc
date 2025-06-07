#include "../include/strton.h"
#include "../include/cmp.h"

#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

StoError sto_uint8(const char* str, uint8_t* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, 10);

    if ((num > UINT8_MAX) || (errno == ERANGE && (num == 0 || num == UINTMAX_MAX))) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (uint8_t)num;
    return STO_SUCCESS;
}

StoError sto_int8(const char* str, int8_t* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, 10);

    if ((num > INT8_MAX || num < INT8_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int8_t)num;
    return STO_SUCCESS;
}

StoError sto_uint16(const char* str, uint16_t* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, 10);

    if ((num > UINT16_MAX) || (errno == ERANGE && (num == 0 || num == UINTMAX_MAX))) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (uint16_t)num;
    return STO_SUCCESS;
}

StoError sto_int16(const char* str, int16_t* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, 10);

    if ((num > INT16_MAX || num < INT16_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int16_t)num;
    return STO_SUCCESS;
}

StoError sto_uint32(const char* str, uint32_t* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, 10);

    if ((num > UINT32_MAX) || (errno == ERANGE && (num == 0 || num == UINTMAX_MAX))) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (uint32_t)num;
    return STO_SUCCESS;
}

StoError sto_int32(const char* str, int32_t* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, 10);

    if ((num > INT32_MAX || num < INT32_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int32_t)num;
    return STO_SUCCESS;
}

StoError sto_uint64(const char* str, uint64_t* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, 10);

    if ((num > UINT64_MAX) || (errno == ERANGE && (num == 0 || num == UINTMAX_MAX))) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (uint64_t)num;
    return STO_SUCCESS;
}

StoError sto_int64(const char* str, int64_t* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, 10);

    if ((num > INT64_MAX || num < INT64_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int64_t)num;
    return STO_SUCCESS;
}

StoError sto_ulong(const char* str, unsigned long* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, 10);

    if ((num > ULONG_MAX) || (errno == ERANGE && (num == 0 || num == UINTMAX_MAX))) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (unsigned long)num;
    return STO_SUCCESS;
}

StoError sto_long(const char* str, long* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, 10);

    if ((num > LONG_MAX || num < LONG_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (long)num;
    return STO_SUCCESS;
}

StoError sto_double(const char* str, double* result) {
    char* endptr = NULL;
    errno        = 0;
    *result      = strtod(str, &endptr);

    if ((FLOAT_EQUAL(*result, (double)DBL_MAX) || FLOAT_EQUAL(*result, (double)-DBL_MAX)) &&
        errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

StoError sto_int(const char* str, int* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, 10);

    if ((num > INT_MAX || num < INT_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int)num;
    return STO_SUCCESS;
}

StoError sto_ulong_b(const char* str, int base, unsigned long* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, base);

    if ((num > ULONG_MAX) || (errno == ERANGE && (num == 0 || num == UINTMAX_MAX))) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (unsigned long)num;
    return STO_SUCCESS;
}

StoError sto_long_b(const char* str, int base, long* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, base);

    if ((num > LONG_MAX || num < LONG_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (long)num;
    return STO_SUCCESS;
}

StoError sto_int_b(const char* str, int base, int* result) {
    char* endptr = NULL;
    errno        = 0;
    intmax_t num = strtoimax(str, &endptr, base);

    if ((num > INT_MAX || num < INT_MIN) || errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int)num;
    return STO_SUCCESS;
}

StoError sto_float(const char* str, float* result) {
    char* endptr = NULL;
    errno        = 0;
    *result      = strtof(str, &endptr);

    if ((FLOAT_EQUAL(*result, (float)FLT_MAX) || FLOAT_EQUAL(*result, (float)-FLT_MAX)) && errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

StoError sto_uint(const char* str, unsigned int* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, 10);

    if ((num > UINT_MAX) || (errno == ERANGE && (num == 0 || num == UINTMAX_MAX))) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (unsigned int)num;
    return STO_SUCCESS;
}

StoError sto_uintptr(const char* str, uintptr_t* result) {
    char* endptr  = NULL;
    errno         = 0;
    uintmax_t num = strtoumax(str, &endptr, 10);

    if (errno == ERANGE && (num == 0 || num == UINTMAX_MAX)) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (uintptr_t)num;
    return STO_SUCCESS;
}

StoError sto_bool(const char* str, bool* result) {
    if (str == NULL || result == NULL) {
        return STO_INVALID;
    }

    switch (str[0]) {
        case 't':
        case 'T':
            if (strcasecmp(str, "true") == 0) {
                *result = true;
                return STO_SUCCESS;
            }
            break;
        case 'f':
        case 'F':
            if (strcasecmp(str, "false") == 0) {
                *result = false;
                return STO_SUCCESS;
            }
            break;
        case '1':
            if (str[1] == '\0') {
                *result = true;
                return STO_SUCCESS;
            }
            break;
        case '0':
            if (str[1] == '\0') {
                *result = false;
                return STO_SUCCESS;
            }
            break;

        case 'y':
        case 'Y':
            if (strcasecmp(str, "yes") == 0) {
                *result = true;
                return STO_SUCCESS;
            }
            break;

        case 'n':
        case 'N':
            if (strcasecmp(str, "no") == 0) {
                *result = false;
                return STO_SUCCESS;
            }
            break;

        case 'o':
        case 'O':
            if (strcasecmp(str, "on") == 0) {
                *result = true;
                return STO_SUCCESS;
            } else if (strcasecmp(str, "off") == 0) {
                *result = false;
                return STO_SUCCESS;
            }
            break;
    }

    return STO_INVALID;
}

const char* sto_error(StoError code) {
    switch (code) {
        case STO_SUCCESS:
            return "Conversion successful";
        case STO_INVALID:
            return "Invalid input";
        case STO_OVERFLOW:
            return "Overflow or undeflow error";
    }

    return "Unknown error";
}
