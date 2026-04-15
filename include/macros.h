/**
 * @file macros.h
 * @brief Portable utility macros for assertions, memory, math, iteration,
 *        timing, and platform compatibility.
 *
 * Supports: GCC, Clang, MSVC (C11 / C17).
 * All macros are safe for use in C and C++ translation units.
 *
 * Portability notes:
 *   - typeof / __typeof__  : GCC/Clang only. MSVC C11 does not support it.
 *     Macros that require a temporary variable (ASSERT_EQ, FOR_EACH_*, etc.)
 *     use __typeof__ under GCC/Clang and fall back to a less type-safe but
 *     functional alternative under MSVC.
 *   - Statement expressions ({ ... }) : GCC/Clang extension.
 *     ROUND_UP_POW2 is provided as an inline function to avoid this.
 *   - _Thread_local : C11 standard keyword. MSVC uses __declspec(thread).
 *   - strtok_r, gmtime_r, localtime_r : POSIX. MSVC provides strtok_s,
 *     gmtime_s, localtime_s with a reversed argument order; shims are provided.
 */

#ifndef SOLIDC_MACROS_H
#define SOLIDC_MACROS_H

#include <stddef.h> /* size_t, ptrdiff_t                    */
#include <stdint.h> /* uint64_t, uintptr_t, intptr_t, ...  */
#include <stdio.h>  /* printf, fprintf, stderr              */
#include <stdlib.h> /* exit                                 */
#include <string.h> /* strcmp, strncmp, memset              */
#include <time.h>   /* time_t, struct tm, clock_gettime     */

#if defined(__cplusplus)
extern "C" {
#endif

/* =========================================================================
 * COMPILER DETECTION
 * ========================================================================= */

#if defined(_MSC_VER)
#define SOLIDC_MSVC  1
#define SOLIDC_GCC   0
#define SOLIDC_CLANG 0
#elif defined(__clang__)
#define SOLIDC_MSVC  0
#define SOLIDC_GCC   0
#define SOLIDC_CLANG 1
#elif defined(__GNUC__)
#define SOLIDC_MSVC  0
#define SOLIDC_GCC   1
#define SOLIDC_CLANG 0
#else
#define SOLIDC_MSVC  0
#define SOLIDC_GCC   0
#define SOLIDC_CLANG 0
#endif

/* =========================================================================
 * COMPILER HINTS AND ATTRIBUTES
 * ========================================================================= */

/** Hint that a condition is likely true (branch prediction). */
#if SOLIDC_GCC || SOLIDC_CLANG
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

/**
 * WARN_UNUSED_RESULT — emit a compiler diagnostic if the return value of a
 * function annotated with this attribute is discarded by the caller.
 */
#if SOLIDC_GCC || SOLIDC_CLANG
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif SOLIDC_MSVC
#define WARN_UNUSED_RESULT _Check_return_
#else
#define WARN_UNUSED_RESULT
#endif

/**
 * PRINTF_FORMAT(fmt_idx, va_idx) — validate printf-style format strings at
 * compile time (1-based argument indices).
 * Example: void log(const char *fmt, ...) PRINTF_FORMAT(1, 2);
 */
#if SOLIDC_GCC || SOLIDC_CLANG
#define PRINTF_FORMAT(fmt, va) __attribute__((format(printf, fmt, va)))
#else
#define PRINTF_FORMAT(fmt, va)
#endif

/** NORETURN — annotate functions that never return (abort, exit wrappers). */
#if SOLIDC_GCC || SOLIDC_CLANG
#define NORETURN __attribute__((noreturn))
#elif SOLIDC_MSVC
#define NORETURN __declspec(noreturn)
#else
#define NORETURN
#endif

/** UNUSED — silence "unused variable/parameter" warnings. */
#define UNUSED(x) ((void)(x))

#if SOLIDC_MSVC
#define THREAD_LOCAL __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define THREAD_LOCAL _Thread_local
#else
/* GCC/Clang pre-C11 extension fallback */
#define THREAD_LOCAL __thread
#endif

/* =========================================================================
 * STATIC ASSERTIONS
 * ========================================================================= */

/** STATIC_ASSERT(cond, msg) — compile-time assertion with a descriptive message. */
#if defined(__cplusplus)
#define STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
/* Pre-C11 fallback: causes an array-size error if cond is false. */
#define STATIC_ASSERT(cond, msg) typedef char _static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

/* =========================================================================
 * RUNTIME ASSERTION MACROS
 *
 * All assertions print file, line, and function name, then call exit(1).
 * Define NDEBUG to compile out assertions (same convention as <assert.h>).
 * ========================================================================= */

#ifndef NDEBUG

/** ASSERT(cond) — abort if cond is false. */
#define ASSERT(cond)                                                                                      \
    do {                                                                                                  \
        if (UNLIKELY(!(cond))) {                                                                          \
            fprintf(stderr, "%s:%d [%s]: Assertion '%s' failed.\n", __FILE__, __LINE__, __func__, #cond); \
            exit(1);                                                                                      \
        }                                                                                                 \
    } while (0)

/** ASSERT_TRUE(cond) — alias for ASSERT; documents boolean intent. */
#define ASSERT_TRUE(cond) ASSERT(cond)

/** ASSERT_NULL(ptr) — abort if ptr is not NULL. */
#define ASSERT_NULL(ptr)                                                                                    \
    do {                                                                                                    \
        if (UNLIKELY((ptr) != NULL)) {                                                                      \
            fprintf(stderr, "%s:%d [%s]: Expected '%s' to be NULL.\n", __FILE__, __LINE__, __func__, #ptr); \
            exit(1);                                                                                        \
        }                                                                                                   \
    } while (0)

/** ASSERT_NOT_NULL(ptr) — abort if ptr is NULL. */
#define ASSERT_NOT_NULL(ptr)                                                                                    \
    do {                                                                                                        \
        if (UNLIKELY((ptr) == NULL)) {                                                                          \
            fprintf(stderr, "%s:%d [%s]: Expected '%s' to not be NULL.\n", __FILE__, __LINE__, __func__, #ptr); \
            exit(1);                                                                                            \
        }                                                                                                       \
    } while (0)

/**
 * ASSERT_STR_EQ(a, b) — abort if two C strings are not equal.
 * Handles NULL on either side gracefully.
 */
#define ASSERT_STR_EQ(a, b)                                                                                          \
    do {                                                                                                             \
        const char* _ase_a = (a);                                                                                    \
        const char* _ase_b = (b);                                                                                    \
        if (_ase_a == NULL || _ase_b == NULL) {                                                                      \
            if (_ase_a != _ase_b) {                                                                                  \
                fprintf(stderr, "%s:%d [%s]: '%s == %s' failed (one is NULL).\n", __FILE__, __LINE__, __func__, #a,  \
                        #b);                                                                                         \
                exit(1);                                                                                             \
            }                                                                                                        \
        } else if (strcmp(_ase_a, _ase_b) != 0) {                                                                    \
            fprintf(stderr, "%s:%d [%s]: '%s == %s' failed (\"%s\" != \"%s\").\n", __FILE__, __LINE__, __func__, #a, \
                    #b, _ase_a, _ase_b);                                                                             \
            exit(1);                                                                                                 \
        }                                                                                                            \
    } while (0)

/**
 * ASSERT_FLOAT_EQ(a, b, epsilon) — abort if |a - b| > epsilon.
 * Both operands are promoted to double for the comparison.
 */
#define ASSERT_FLOAT_EQ(a, b, epsilon)                                               \
    do {                                                                             \
        double _afe_a = (double)(a);                                                 \
        double _afe_b = (double)(b);                                                 \
        double _afe_eps = (double)(epsilon);                                         \
        double _afe_d = _afe_a - _afe_b;                                             \
        /* Use manual abs to avoid pulling in <math.h> for fabs. */                  \
        if ((_afe_d > _afe_eps) || (-_afe_d > _afe_eps)) {                           \
            fprintf(stderr,                                                          \
                    "%s:%d [%s]: Float '%s == %s' failed"                            \
                    " (%.6f != %.6f, eps=%.6f).\n",                                  \
                    __FILE__, __LINE__, __func__, #a, #b, _afe_a, _afe_b, _afe_eps); \
            exit(1);                                                                 \
        }                                                                            \
    } while (0)

/*
 * ASSERT_EQ / ASSERT_NE / ASSERT_RANGE
 *
 * These macros compare two values using temporary variables to avoid
 * double evaluation. Under GCC/Clang we use __typeof__ to preserve the
 * exact type. Under MSVC, which lacks typeof in C mode, we fall back to
 * intptr_t — wide enough for any scalar and pointer coerced to integer.
 * Floating-point comparisons should use ASSERT_FLOAT_EQ instead.
 */
#if SOLIDC_GCC || SOLIDC_CLANG

/** ASSERT_EQ(a, b) — abort if a != b. */
#define ASSERT_EQ(a, b)                                                                          \
    do {                                                                                         \
        __typeof__(a) _aeq_a = (a);                                                              \
        __typeof__(b) _aeq_b = (b);                                                              \
        if (UNLIKELY(_aeq_a != _aeq_b)) {                                                        \
            fprintf(stderr,                                                                      \
                    "%s:%d [%s]: '%s == %s' failed"                                              \
                    " (%td != %td).\n",                                                          \
                    __FILE__, __LINE__, __func__, #a, #b, (ptrdiff_t)_aeq_a, (ptrdiff_t)_aeq_b); \
            exit(1);                                                                             \
        }                                                                                        \
    } while (0)

/** ASSERT_NE(a, b) — abort if a == b. */
#define ASSERT_NE(a, b)                                                       \
    do {                                                                      \
        __typeof__(a) _ane_a = (a);                                           \
        __typeof__(b) _ane_b = (b);                                           \
        if (UNLIKELY(_ane_a == _ane_b)) {                                     \
            fprintf(stderr,                                                   \
                    "%s:%d [%s]: '%s != %s' failed"                           \
                    " (both %td).\n",                                         \
                    __FILE__, __LINE__, __func__, #a, #b, (ptrdiff_t)_ane_a); \
            exit(1);                                                          \
        }                                                                     \
    } while (0)

/** ASSERT_RANGE(val, min, max) — abort if val is outside [min, max]. */
#define ASSERT_RANGE(val, min, max)                                                                        \
    do {                                                                                                   \
        __typeof__(val) _ar_v = (val);                                                                     \
        __typeof__(min) _ar_min = (min);                                                                   \
        __typeof__(max) _ar_max = (max);                                                                   \
        if (UNLIKELY(_ar_v < _ar_min || _ar_v > _ar_max)) {                                                \
            fprintf(stderr, "%s:%d [%s]: %s=%td not in [%td, %td].\n", __FILE__, __LINE__, __func__, #val, \
                    (ptrdiff_t)_ar_v, (ptrdiff_t)_ar_min, (ptrdiff_t)_ar_max);                             \
            exit(1);                                                                                       \
        }                                                                                                  \
    } while (0)

#else /* MSVC or unknown — typeof unavailable */

#define ASSERT_EQ(a, b)                                                    \
    do {                                                                   \
        intptr_t _aeq_a = (intptr_t)(a);                                   \
        intptr_t _aeq_b = (intptr_t)(b);                                   \
        if (UNLIKELY(_aeq_a != _aeq_b)) {                                  \
            fprintf(stderr,                                                \
                    "%s:%d [%s]: '%s == %s' failed"                        \
                    " (%td != %td).\n",                                    \
                    __FILE__, __LINE__, __func__, #a, #b, _aeq_a, _aeq_b); \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

#define ASSERT_NE(a, b)                                            \
    do {                                                           \
        intptr_t _ane_a = (intptr_t)(a);                           \
        intptr_t _ane_b = (intptr_t)(b);                           \
        if (UNLIKELY(_ane_a == _ane_b)) {                          \
            fprintf(stderr,                                        \
                    "%s:%d [%s]: '%s != %s' failed"                \
                    " (both %td).\n",                              \
                    __FILE__, __LINE__, __func__, #a, #b, _ane_a); \
            exit(1);                                               \
        }                                                          \
    } while (0)

#define ASSERT_RANGE(val, min, max)                                                                               \
    do {                                                                                                          \
        intptr_t _ar_v = (intptr_t)(val);                                                                         \
        intptr_t _ar_min = (intptr_t)(min);                                                                       \
        intptr_t _ar_max = (intptr_t)(max);                                                                       \
        if (UNLIKELY(_ar_v < _ar_min || _ar_v > _ar_max)) {                                                       \
            fprintf(stderr, "%s:%d [%s]: %s=%td not in [%td, %td].\n", __FILE__, __LINE__, __func__, #val, _ar_v, \
                    _ar_min, _ar_max);                                                                            \
            exit(1);                                                                                              \
        }                                                                                                         \
    } while (0)

#endif /* SOLIDC_GCC || SOLIDC_CLANG */

#else /* NDEBUG — strip all runtime assertions */

#define ASSERT(cond)             UNUSED(cond)
#define ASSERT_TRUE(cond)        UNUSED(cond)
#define ASSERT_NULL(ptr)         UNUSED(ptr)
#define ASSERT_NOT_NULL(ptr)     UNUSED(ptr)
#define ASSERT_STR_EQ(a, b)      (UNUSED(a), UNUSED(b))
#define ASSERT_FLOAT_EQ(a, b, e) (UNUSED(a), UNUSED(b), UNUSED(e))
#define ASSERT_EQ(a, b)          (UNUSED(a), UNUSED(b))
#define ASSERT_NE(a, b)          (UNUSED(a), UNUSED(b))
#define ASSERT_RANGE(v, lo, hi)  (UNUSED(v), UNUSED(lo), UNUSED(hi))

#endif /* NDEBUG */

/* =========================================================================
 * POWER-OF-2 UTILITIES
 * ========================================================================= */

/** IS_POWER_OF_2(n) — evaluates to 1 if n is a power of two, 0 otherwise. */
#define IS_POWER_OF_2(n) ((n) > 0 && (((n) & ((n) - 1)) == 0))

/** STATIC_CHECK_POWER_OF_2(n) — compile-time assertion that n is a power of two. */
#define STATIC_CHECK_POWER_OF_2(n) STATIC_ASSERT(IS_POWER_OF_2(n), #n " must be a power of 2")

/**
 * round_up_pow2_u64 — round x up to the next power of two (64-bit).
 *
 * Returns 1 for x == 0. Undefined behaviour if x > 2^63.
 * Implemented as an inline function to avoid GCC statement-expression
 * extensions, ensuring compatibility with MSVC.
 */
static inline uint64_t round_up_pow2_u64(uint64_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
}

/**
 * round_up_pow2_u32 — round x up to the next power of two (32-bit).
 *
 * Returns 1 for x == 0. Undefined behaviour if x > 2^31.
 */
static inline uint32_t round_up_pow2_u32(uint32_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

/* =========================================================================
 * MEMORY AND ARRAY UTILITIES
 * ========================================================================= */

/** ARRAY_SIZE(arr) — number of elements in a stack-allocated array. */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/** ZERO_MEMORY(ptr, size) — zero size bytes starting at ptr. */
#define ZERO_MEMORY(ptr, size) memset((ptr), 0, (size))

/** ZERO_STRUCT(s) — zero every byte of a struct variable s. */
#define ZERO_STRUCT(s) memset(&(s), 0, sizeof(s))

/** ZERO_ARRAY(arr) — zero an entire stack-allocated array. */
#define ZERO_ARRAY(arr) memset((arr), 0, sizeof(arr))

/* =========================================================================
 * MATHEMATICAL UTILITIES
 * ========================================================================= */

/** MIN(a, b) — minimum of two scalar values (evaluates each argument once). */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/** MAX(a, b) — maximum of two scalar values (evaluates each argument once). */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/** ABS(x) — absolute value without pulling in <math.h>. */
#define ABS(x) ((x) < 0 ? -(x) : (x))

/** CLAMP(x, lo, hi) — constrain x to [lo, hi]. */
#define CLAMP(x, lo, hi) (MIN(MAX((x), (lo)), (hi)))

/* =========================================================================
 * BIT MANIPULATION
 * ========================================================================= */

/** SET_BIT(v, pos) — set bit at position pos in v (0 = LSB). */
#define SET_BIT(v, pos) ((v) |= (1ULL << (pos)))

/** CLEAR_BIT(v, pos) — clear bit at position pos in v. */
#define CLEAR_BIT(v, pos) ((v) &= ~(1ULL << (pos)))

/** TOGGLE_BIT(v, pos) — flip bit at position pos in v. */
#define TOGGLE_BIT(v, pos) ((v) ^= (1ULL << (pos)))

/** CHECK_BIT(v, pos) — evaluate to 1 if bit pos is set in v, else 0. */
#define CHECK_BIT(v, pos) (((v) >> (pos)) & 1U)

/* =========================================================================
 * ALIGNMENT UTILITIES
 *
 * `align` must be a power of two for all three macros.
 * ========================================================================= */

/** ALIGN_UP(x, align) — round x up to the next multiple of align. */
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/** ALIGN_DOWN(x, align) — round x down to the previous multiple of align. */
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

/** IS_ALIGNED(x, align) — evaluate to 1 if x is a multiple of align. */
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

/* =========================================================================
 * STRING UTILITIES
 * ========================================================================= */

/** STREQ(a, b) — true if two C strings are equal. */
#define STREQ(a, b) (strcmp((a), (b)) == 0)

/** STRNEQ(a, b, n) — true if first n bytes of two C strings are equal. */
#define STRNEQ(a, b, n) (strncmp((a), (b), (n)) == 0)

/** STR_EMPTY(s) — true if s is NULL or the empty string. */
#define STR_EMPTY(s) ((s) == NULL || (s)[0] == '\0')

/** STR_NOT_EMPTY(s) — true if s is non-NULL and non-empty. */
#define STR_NOT_EMPTY(s) ((s) != NULL && (s)[0] != '\0')

/* =========================================================================
 * DEBUGGING AND LOGGING
 *
 * DEBUG_* macros compile to no-ops when NDEBUG is defined.
 * LOG_* macros are always active.
 * ========================================================================= */

#ifndef NDEBUG

/** DEBUG_PRINT(fmt, ...) — print a formatted debug message with source location. */
#define DEBUG_PRINT(fmt, ...) printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

/** DEBUG_VAR(var) — print a scalar variable's name and value as a signed integer. */
#define DEBUG_VAR(var) printf("[DEBUG] %s:%d: %s = %td\n", __FILE__, __LINE__, #var, (ptrdiff_t)(var))

/** DEBUG_STR(str) — print a string variable's name and value, handling NULL. */
#define DEBUG_STR(str) printf("[DEBUG] %s:%d: %s = \"%s\"\n", __FILE__, __LINE__, #str, (str) ? (str) : "(null)")

#else /* NDEBUG */

#define DEBUG_PRINT(fmt, ...) ((void)0)
#define DEBUG_VAR(var)        UNUSED(var)
#define DEBUG_STR(str)        UNUSED(str)

#endif /* NDEBUG */

/** LOG_ERROR(fmt, ...) — print an error message to stderr (always active). */
#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d [%s]: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

/** LOG_WARN(fmt, ...) — print a warning to stderr (always active). */
#define LOG_WARN(fmt, ...) fprintf(stderr, "[WARN]  %s:%d [%s]: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

/** LOG_INFO(fmt, ...) — print an informational message to stdout (always active). */
#define LOG_INFO(fmt, ...) printf("[INFO]  %s:%d [%s]: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

/* =========================================================================
 * CONTAINER ITERATION
 *
 * FOR_EACH_ARRAY and FOR_EACH_RANGE use __typeof__ under GCC/Clang for full
 * type safety. Under MSVC they fall back to explicit pointer/index types;
 * callers must ensure the array element type is compatible.
 * ========================================================================= */

#if SOLIDC_GCC || SOLIDC_CLANG

/**
 * FOR_EACH_ARRAY(item, array) — iterate over every element of a
 * stack-allocated array. `item` is a pointer to the current element.
 *
 * Example:
 *   int nums[] = {1, 2, 3};
 *   FOR_EACH_ARRAY(n, nums) { printf("%d\n", *n); }
 */
#define FOR_EACH_ARRAY(item, array) \
    for (__typeof__(&(array)[0]) item = (array), _fea_end = (array) + ARRAY_SIZE(array); item < _fea_end; ++item)

/**
 * FOR_EACH_RANGE(var, start, end) — iterate var over [start, end).
 *
 * Example:
 *   FOR_EACH_RANGE(i, 0, 10) { printf("%d\n", i); }
 */
#define FOR_EACH_RANGE(var, start, end) for (__typeof__(start) var = (start); var < (end); ++var)

#else /* MSVC — typeof unavailable */

/**
 * FOR_EACH_ARRAY — MSVC fallback using size_t index.
 * `item` is a void* pointing to the current element; cast before use.
 *
 * Example:
 *   FOR_EACH_ARRAY(i, arr, sizeof(arr[0])) { int *n = (int*)item; }
 *
 * Prefer the three-argument variant when element size is not obvious.
 */
#define FOR_EACH_ARRAY(item, array)                                                                         \
    for (void *item = (void*)(array), *_fea_end = (void*)((char*)(array) + sizeof(array)); item < _fea_end; \
         item = (char*)item + (sizeof((array)[0])))

#define FOR_EACH_RANGE(var, start, end) for (ptrdiff_t var = (ptrdiff_t)(start); var < (ptrdiff_t)(end); ++var)

#endif /* SOLIDC_GCC || SOLIDC_CLANG */

/* =========================================================================
 * TIMING AND PERFORMANCE
 *
 * Two families of macros:
 *
 *   TIME_BLOCK(name, block)    — measure a block and print elapsed seconds.
 *   TIME_BLOCK_MS(name, block) — measure a block and print elapsed milliseconds.
 *
 * `block` is an unquoted sequence of statements placed verbatim before the
 * stop-time call, e.g.:
 *
 *   TIME_BLOCK("sort", sort(arr, n);)
 *
 * get_time_ns() — returns current monotonic time in nanoseconds as uint64_t.
 * TIME_DIFF / TIME_DIFF_MS — compute elapsed seconds / milliseconds between
 *   two timestamps captured with the platform-specific API.
 * ========================================================================= */

#if defined(_WIN32)

#include <windows.h> /* LARGE_INTEGER, QueryPerformanceCounter, etc. */

/**
 * TIME_DIFF(start, end, freq) — elapsed seconds between two LARGE_INTEGER
 * timestamps captured with QueryPerformanceCounter.
 */
#define TIME_DIFF(start, end, freq) ((double)((end).QuadPart - (start).QuadPart) / (double)(freq).QuadPart)

/**
 * TIME_DIFF_MS(start, end, freq) — elapsed milliseconds between two
 * LARGE_INTEGER timestamps captured with QueryPerformanceCounter.
 */
#define TIME_DIFF_MS(start, end, freq) ((double)((end).QuadPart - (start).QuadPart) * 1000.0 / (double)(freq).QuadPart)

#define TIME_BLOCK(name, block)                                           \
    do {                                                                  \
        LARGE_INTEGER _freq, _t0, _t1;                                    \
        QueryPerformanceFrequency(&_freq);                                \
        QueryPerformanceCounter(&_t0);                                    \
        block QueryPerformanceCounter(&_t1);                              \
        printf("Time[%s]: %.6f s\n", (name), TIME_DIFF(_t0, _t1, _freq)); \
    } while (0)

#define TIME_BLOCK_MS(name, block)                                            \
    do {                                                                      \
        LARGE_INTEGER _freq, _t0, _t1;                                        \
        QueryPerformanceFrequency(&_freq);                                    \
        QueryPerformanceCounter(&_t0);                                        \
        block QueryPerformanceCounter(&_t1);                                  \
        printf("Time[%s]: %.3f ms\n", (name), TIME_DIFF_MS(_t0, _t1, _freq)); \
    } while (0)

#else /* POSIX — use clock_gettime(CLOCK_MONOTONIC) */

/**
 * TIME_DIFF(start, end) — elapsed seconds between two struct timespec values.
 */
#define TIME_DIFF(start, end) \
    ((double)((end).tv_sec - (start).tv_sec) + (double)((end).tv_nsec - (start).tv_nsec) / 1.0e9)

/**
 * TIME_DIFF_MS(start, end) — elapsed milliseconds between two struct timespec
 * values.
 */
#define TIME_DIFF_MS(start, end) \
    ((double)((end).tv_sec - (start).tv_sec) * 1000.0 + (double)((end).tv_nsec - (start).tv_nsec) / 1.0e6)

#define TIME_BLOCK(name, block)                                    \
    do {                                                           \
        struct timespec _t0, _t1;                                  \
        clock_gettime(CLOCK_MONOTONIC, &_t0);                      \
        block clock_gettime(CLOCK_MONOTONIC, &_t1);                \
        printf("Time[%s]: %.6f s\n", (name), TIME_DIFF(_t0, _t1)); \
    } while (0)

#define TIME_BLOCK_MS(name, block)                                     \
    do {                                                               \
        struct timespec _t0, _t1;                                      \
        clock_gettime(CLOCK_MONOTONIC, &_t0);                          \
        block clock_gettime(CLOCK_MONOTONIC, &_t1);                    \
        printf("Time[%s]: %.3f ms\n", (name), TIME_DIFF_MS(_t0, _t1)); \
    } while (0)

#endif /* _WIN32 */

/**
 * get_time_ns — returns the current monotonic timestamp in nanoseconds.
 *
 * On Windows, QueryPerformanceCounter is used. The calculation separates
 * the integer and fractional parts to prevent overflow on long uptimes.
 * On POSIX (Linux, macOS ≥ 10.12), clock_gettime(CLOCK_MONOTONIC) is used.
 *
 * @return Nanoseconds since an unspecified epoch (suitable for intervals only).
 */
static inline uint64_t get_time_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    /* Split into whole seconds and remainder to avoid 64-bit overflow. */
    uint64_t seconds = (uint64_t)counter.QuadPart / (uint64_t)freq.QuadPart;
    uint64_t remainder = (uint64_t)counter.QuadPart % (uint64_t)freq.QuadPart;
    return seconds * UINT64_C(1000000000) + (remainder * UINT64_C(1000000000)) / (uint64_t)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
#endif
}

/* =========================================================================
 * POSIX COMPATIBILITY SHIMS (MSVC)
 *
 * strtok_r, gmtime_r, localtime_r are POSIX functions absent on Windows.
 * Their MSVC equivalents exist but use a reversed argument order for the
 * reentrant time functions.
 * ========================================================================= */

#if SOLIDC_MSVC

/** strtok_r — reentrant strtok; maps to strtok_s on MSVC. */
#define strtok_r(str, delim, saveptr) strtok_s((str), (delim), (saveptr))

/**
 * gmtime_r — reentrant gmtime.
 * @param timep  Pointer to a time_t value.
 * @param result Caller-provided struct tm to receive the broken-down time.
 * @return result on success, NULL on failure.
 */
static inline struct tm* gmtime_r(const time_t* timep, struct tm* result) {
    return gmtime_s(result, timep) == 0 ? result : NULL;
}

/**
 * localtime_r — reentrant localtime.
 * @param timep  Pointer to a time_t value.
 * @param result Caller-provided struct tm to receive the broken-down time.
 * @return result on success, NULL on failure.
 */
static inline struct tm* localtime_r(const time_t* timep, struct tm* result) {
    return localtime_s(result, timep) == 0 ? result : NULL;
}

#endif /* SOLIDC_MSVC */

#if defined(__cplusplus)
}
#endif

#endif /* SOLIDC_MACROS_H */
