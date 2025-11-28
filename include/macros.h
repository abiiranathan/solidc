#ifndef __SOLIDC_MACROS__
#define __SOLIDC_MACROS__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// ASSERTION MACROS
// =============================================================================

#define ASSERT(cond)                                                                                                   \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("%s:%d [%s]: Assertion '%s' failed.\n", __FILE__, __LINE__, __func__, #cond);                       \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_EQ(a, b)                                                                                                \
    do {                                                                                                               \
        typeof(a) _a = (a);                                                                                            \
        typeof(b) _b = (b);                                                                                            \
        if (_a != _b) {                                                                                                \
            printf("%s:%d [%s]: Assertion '%s == %s' failed (%ld != %ld).\n", __FILE__, __LINE__, __func__, #a, #b,    \
                   (long)_a, (long)_b);                                                                                \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_NE(a, b)                                                                                                \
    do {                                                                                                               \
        typeof(a) _a = (a);                                                                                            \
        typeof(b) _b = (b);                                                                                            \
        if (_a == _b) {                                                                                                \
            printf("%s:%d [%s]: Assertion '%s != %s' failed (both are %ld).\n", __FILE__, __LINE__, __func__, #a, #b,  \
                   (long)_a);                                                                                          \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_TRUE(cond)                                                                                              \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("%s:%d [%s]: Assertion '%s' is not true.\n", __FILE__, __LINE__, __func__, #cond);                  \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_STR_EQ(a, b)                                                                                            \
    do {                                                                                                               \
        const char* _a = (a);                                                                                          \
        const char* _b = (b);                                                                                          \
        if (_a == NULL || _b == NULL) {                                                                                \
            if (_a != _b) {                                                                                            \
                printf("%s:%d [%s]: Assertion '%s == %s' failed (one is NULL).\n", __FILE__, __LINE__, __func__, #a,   \
                       #b);                                                                                            \
                exit(1);                                                                                               \
            }                                                                                                          \
        } else if (strcmp(_a, _b) != 0) {                                                                              \
            printf("%s:%d [%s]: Assertion '%s == %s' failed (\"%s\" != \"%s\").\n", __FILE__, __LINE__, __func__, #a,  \
                   #b, _a, _b);                                                                                        \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_NULL(ptr)                                                                                               \
    do {                                                                                                               \
        if ((ptr) != NULL) {                                                                                           \
            printf("%s:%d [%s]: Expected '%s' to be NULL.\n", __FILE__, __LINE__, __func__, #ptr);                     \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                                                           \
    do {                                                                                                               \
        if ((ptr) == NULL) {                                                                                           \
            printf("%s:%d [%s]: Expected '%s' to not be NULL.\n", __FILE__, __LINE__, __func__, #ptr);                 \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_RANGE(val, min, max)                                                                                    \
    do {                                                                                                               \
        typeof(val) _val = (val);                                                                                      \
        typeof(min) _min = (min);                                                                                      \
        typeof(max) _max = (max);                                                                                      \
        if (_val < _min || _val > _max) {                                                                              \
            printf("%s:%d [%s]: Value %ld is not in range [%ld, %ld].\n", __FILE__, __LINE__, __func__, (long)_val,    \
                   (long)_min, (long)_max);                                                                            \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_FLOAT_EQ(a, b, epsilon)                                                                                 \
    do {                                                                                                               \
        double _a   = (double)(a);                                                                                     \
        double _b   = (double)(b);                                                                                     \
        double _eps = (double)(epsilon);                                                                               \
        if ((_a - _b) > _eps || (_b - _a) > _eps) {                                                                    \
            printf("%s:%d [%s]: Float assertion '%s == %s' failed (%.6f != %.6f, epsilon=%.6f).\n", __FILE__,          \
                   __LINE__, __func__, #a, #b, _a, _b, _eps);                                                          \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

// =============================================================================
// STATIC ASSERTIONS
// =============================================================================

#if defined(__cplusplus)
#define STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

#define IS_POWER_OF_2(n)           ((n) > 0 && ((n) & ((n) - 1)) == 0)
#define STATIC_CHECK_POWER_OF_2(n) STATIC_ASSERT(IS_POWER_OF_2(n), #n " is not a power of 2")

// =============================================================================
// MEMORY AND ARRAY UTILITIES
// =============================================================================

#define ARRAY_SIZE(arr)        (sizeof(arr) / sizeof((arr)[0]))
#define ZERO_MEMORY(ptr, size) memset(ptr, 0, size)
#define ZERO_STRUCT(s)         memset(&(s), 0, sizeof(s))
#define ZERO_ARRAY(arr)        memset(arr, 0, sizeof(arr))

// =============================================================================
// MATHEMATICAL UTILITIES
// =============================================================================

#define MIN(a, b)          ((a) < (b) ? (a) : (b))
#define MAX(a, b)          ((a) > (b) ? (a) : (b))
#define ABS(x)             ((x) < 0 ? -(x) : (x))
#define CLAMP(x, min, max) (MIN(MAX(x, min), max))
#define SWAP(a, b)                                                                                                     \
    do {                                                                                                               \
        typeof(a) _tmp = a;                                                                                            \
        a              = b;                                                                                            \
        b              = _tmp;                                                                                         \
    } while (0)

// Bit manipulation
#define SET_BIT(num, pos)    ((num) |= (1ULL << (pos)))
#define CLEAR_BIT(num, pos)  ((num) &= ~(1ULL << (pos)))
#define TOGGLE_BIT(num, pos) ((num) ^= (1ULL << (pos)))
#define CHECK_BIT(num, pos)  (((num) >> (pos)) & 1)

// Alignment macros
#define ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

// Round to nearest power of 2
#define ROUND_UP_POW2(x)                                                                                               \
    ({                                                                                                                 \
        typeof(x) _x = (x);                                                                                            \
        _x--;                                                                                                          \
        _x |= _x >> 1;                                                                                                 \
        _x |= _x >> 2;                                                                                                 \
        _x |= _x >> 4;                                                                                                 \
        _x |= _x >> 8;                                                                                                 \
        _x |= _x >> 16;                                                                                                \
        if (sizeof(_x) > 4) _x |= _x >> 32;                                                                            \
        _x++;                                                                                                          \
    })

// =============================================================================
// STRING UTILITIES
// =============================================================================

#define STREQ(a, b)      (strcmp(a, b) == 0)
#define STRNEQ(a, b, n)  (strncmp(a, b, n) == 0)
#define STR_EMPTY(s)     ((s) == NULL || (s)[0] == '\0')
#define STR_NOT_EMPTY(s) ((s) != NULL && (s)[0] != '\0')

// =============================================================================
// DEBUGGING AND LOGGING
// =============================================================================

#ifdef NDEBUG
#define DEBUG_PRINT(fmt, ...) (void)fmt;
#define DEBUG_VAR(var)        (void)var;
#define DEBUG_STR(str)        (void)str;
#else
#define DEBUG_PRINT(fmt, ...) printf("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define DEBUG_VAR(var)        printf("[DEBUG] %s:%d: %s = %ld\n", __FILE__, __LINE__, #var, (long)(var))
#define DEBUG_STR(str)        printf("[DEBUG] %s:%d: %s = \"%s\"\n", __FILE__, __LINE__, #str, (str) ? (str) : "NULL")
#endif

#define LOG_ERROR(fmt, ...)                                                                                            \
    fprintf(stderr, "[ERROR] %s:%d [%s]: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) fprintf(stderr, "[WARN] %s:%d [%s]: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) printf("[INFO] %s:%d [%s]: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

// =============================================================================
// CONTAINER ITERATION
// =============================================================================

#define FOR_EACH_ARRAY(item, array)                                                                                    \
    for (typeof((array)[0])*item = (array), *_end = (array) + ARRAY_SIZE(array); item < _end; ++item)

#define FOR_EACH_RANGE(var, start, end) for (typeof(start) var = (start); var < (end); ++var)

// =============================================================================
// TIMING AND PERFORMANCE
// =============================================================================

#ifdef _WIN32
#include <windows.h>

#define TIME_DIFF(start, end, freq) ((double)((end).QuadPart - (start).QuadPart) / (double)(freq).QuadPart)

#define TIME_DIFF_MS(start, end, freq) ((double)((end).QuadPart - (start).QuadPart) * 1000.0 / (double)(freq).QuadPart)

#else
#include <time.h>

#define TIME_DIFF(start, end)                                                                                          \
    ((double)((end).tv_sec - (start).tv_sec) + (double)((end).tv_nsec - (start).tv_nsec) / 1e9)

// returns elapsed time in *milliseconds* as double
#define TIME_DIFF_MS(start, end)                                                                                       \
    ((double)((end).tv_sec - (start).tv_sec) * 1000.0 + (double)((end).tv_nsec - (start).tv_nsec) / 1.0e6)

#endif

#ifdef _WIN32
#define TIME_BLOCK(name, block)                                                                                        \
    do {                                                                                                               \
        LARGE_INTEGER _freq, _start, _end;                                                                             \
        QueryPerformanceFrequency(&_freq);                                                                             \
        QueryPerformanceCounter(&_start);                                                                              \
        block QueryPerformanceCounter(&_end);                                                                          \
        double _time = TIME_DIFF(_start, _end, _freq);                                                                 \
        printf("Time for %s: %.6f seconds\n", name, _time);                                                            \
    } while (0)
#else
#define TIME_BLOCK(name, block)                                                                                        \
    do {                                                                                                               \
        struct timespec _start, _end;                                                                                  \
        clock_gettime(CLOCK_MONOTONIC, &_start);                                                                       \
        block clock_gettime(CLOCK_MONOTONIC, &_end);                                                                   \
        double _time = TIME_DIFF(_start, _end);                                                                        \
        printf("Time for %s: %.6f seconds\n", name, _time);                                                            \
    } while (0)
#endif

#ifdef _WIN32
#define TIME_BLOCK_MS(name, block)                                                                                     \
    do {                                                                                                               \
        LARGE_INTEGER _freq, _start, _end;                                                                             \
        QueryPerformanceFrequency(&_freq);                                                                             \
        QueryPerformanceCounter(&_start);                                                                              \
        block QueryPerformanceCounter(&_end);                                                                          \
        double _ms = TIME_DIFF_MS(_start, _end, _freq);                                                                \
        printf("Time for %s: %.3f ms\n", name, _ms);                                                                   \
    } while (0)
#else
#define TIME_BLOCK_MS(name, block)                                                                                     \
    do {                                                                                                               \
        struct timespec _start, _end;                                                                                  \
        clock_gettime(CLOCK_MONOTONIC, &_start);                                                                       \
        block clock_gettime(CLOCK_MONOTONIC, &_end);                                                                   \
        double _ms = TIME_DIFF_MS(_start, _end);                                                                       \
        printf("Time for %s: %.3f ms\n", name, _ms);                                                                   \
    } while (0)
#endif

#endif  // __SOLIDC_MACROS__
