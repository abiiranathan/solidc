#ifndef __SOLIDC_CMP__
#define __SOLIDC_CMP__

#include <float.h>
#include <math.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define FLOAT_EQUAL(a, b)                                                                                    \
    _Generic((a),                                                                                            \
        float: nearly_equal_float,                                                                           \
        double: nearly_equal_double,                                                                         \
        long double: nearly_equal_long_double)(a, b)

// Helper function to check for signed zero equality
static inline bool is_zero_float(float a) {
    return fabsf(a) <= FLT_EPSILON;
}

static inline bool is_zero_double(double a) {
    return fabs(a) <= DBL_EPSILON;
}

static inline bool is_zero_long_double(long double a) {
    return fabsl(a) <= LDBL_EPSILON;
}

// Improved comparison functions that handle signed zeros
__attribute__((unused)) static inline bool nearly_equal_float(float a, float b) {
    // Handle signed zero cases
    if (is_zero_float(a) && is_zero_float(b)) {
        return true;
    }
    return fabsf(a - b) <= (fmaxf(fabsf(a), fabsf(b)) * FLT_EPSILON);
}

__attribute__((unused)) static inline bool nearly_equal_double(double a, double b) {
    // Handle signed zero cases
    if (is_zero_double(a) && is_zero_double(b)) {
        return true;
    }
    return fabs(a - b) <= (fmax(fabs(a), fabs(b)) * DBL_EPSILON);
}

__attribute__((unused)) static inline bool nearly_equal_long_double(long double a, long double b) {
    // Handle signed zero cases
    if (is_zero_long_double(a) && is_zero_long_double(b)) {
        return true;
    }
    return fabsl(a - b) <= (fmaxl(fabsl(a), fabsl(b)) * LDBL_EPSILON);
}

#if defined(__cplusplus)
}
#endif
#endif
