#ifndef __SOLIDC_CMP__
#define __SOLIDC_CMP__

#include <float.h>
#include <math.h>
#include <stdbool.h>

#if defined(__cplusplus__)
#extern "C" {
#endif

#define FLOAT_EQUAL(a, b)                                                                                              \
    _Generic((a), float: nearly_equal_float, double: nearly_equal_double, long double: nearly_equal_long_double)(a, b)

// Function for float comparison
__attribute__((unused)) static inline bool nearly_equal_float(float a, float b) {
    return fabsf(a - b) <= (fmaxf(fabsf(a), fabsf(b)) * FLT_EPSILON);
}

// Function for double comparison
__attribute__((unused)) static inline bool nearly_equal_double(double a, double b) {
    return fabs(a - b) <= (fmax(fabs(a), fabs(b)) * DBL_EPSILON);
}

// Function for long double comparison
__attribute__((unused)) static inline bool nearly_equal_long_double(long double a, long double b) {
    return fabsl(a - b) <= (fmaxl(fabsl(a), fabsl(b)) * LDBL_EPSILON);
}

#if defined(__cplusplus__)
}
#endif
#endif
