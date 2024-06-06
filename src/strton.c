#include "../include/strton.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

StoError sto_ulong(const char* str, unsigned long* result) {
    char* endptr;
    errno = 0;
    *result = strtoul(str, &endptr, 10);

    if ((*result == ULONG_MAX && errno == ERANGE)) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }
    return STO_SUCCESS;
}

StoError sto_long(const char* str, long* result) {
    char* endptr;
    errno = 0;
    *result = strtol(str, &endptr, 10);

    if ((*result == LONG_MAX || *result == LONG_MIN) && errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

StoError sto_double(const char* str, double* result) {
    char* endptr;
    errno = 0;
    *result = strtod(str, &endptr);

    if ((*result == DBL_MAX || *result == -DBL_MAX) && errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

StoError sto_int(const char* str, int* result) {
    char* endptr;
    errno = 0;
    long temp_result = strtol(str, &endptr, 10);

    if ((temp_result > INT_MAX || temp_result < INT_MIN) && errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int)temp_result;
    return STO_SUCCESS;
}

StoError sto_ulong_b(const char* str, int base, unsigned long* result) {
    char* endptr;
    errno = 0;
    *result = strtoul(str, &endptr, base);

    if ((*result == ULONG_MAX && errno == ERANGE)) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

StoError sto_long_b(const char* str, int base, long* result) {
    char* endptr;
    errno = 0;
    *result = strtol(str, &endptr, base);

    if ((*result == LONG_MAX || *result == LONG_MIN) && errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    return STO_SUCCESS;
}

StoError sto_int_b(const char* str, int base, int* result) {
    char* endptr;
    errno = 0;
    long temp_result = strtol(str, &endptr, base);

    if ((temp_result > INT_MAX || temp_result < INT_MIN) && errno == ERANGE) {
        return STO_OVERFLOW;
    }

    if (*endptr != '\0' || endptr == str) {
        return STO_INVALID;
    }

    *result = (int)temp_result;
    return STO_SUCCESS;
}

// Fast boolean converter for true/false, yes/no, on/off, 1/0
StoError sto_bool(const char* str, bool* result) {
    if (str == NULL || result == NULL)
        return STO_INVALID;

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
            }
            if (strcasecmp(str, "off") == 0) {
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
    }
    return STO_INVALID;
}
