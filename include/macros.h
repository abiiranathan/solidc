#ifndef __SOLIDC_MACROS__
#define __SOLIDC_MACROS__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            printf("%s:%d [%s]: Assertion '%s == %s' failed (%ld != %ld).\n",                                          \
                   __FILE__,                                                                                           \
                   __LINE__,                                                                                           \
                   __func__,                                                                                           \
                   #a,                                                                                                 \
                   #b,                                                                                                 \
                   (long)_a,                                                                                           \
                   (long)_b);                                                                                          \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#define ASSERT_NE(a, b)                                                                                                \
    do {                                                                                                               \
        typeof(a) _a = (a);                                                                                            \
        typeof(b) _b = (b);                                                                                            \
        if (_a == _b) {                                                                                                \
            printf("%s:%d [%s]: Assertion '%s != %s' failed (both are %ld).\n",                                        \
                   __FILE__,                                                                                           \
                   __LINE__,                                                                                           \
                   __func__,                                                                                           \
                   #a,                                                                                                 \
                   #b,                                                                                                 \
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
                printf(                                                                                                \
                    "%s:%d [%s]: Assertion '%s == %s' failed (one is NULL).\n", __FILE__, __LINE__, __func__, #a, #b); \
                exit(1);                                                                                               \
            }                                                                                                          \
        } else if (strcmp(_a, _b) != 0) {                                                                              \
            printf("%s:%d [%s]: Assertion '%s == %s' failed (\"%s\" != \"%s\").\n",                                    \
                   __FILE__,                                                                                           \
                   __LINE__,                                                                                           \
                   __func__,                                                                                           \
                   #a,                                                                                                 \
                   #b,                                                                                                 \
                   _a,                                                                                                 \
                   _b);                                                                                                \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

#endif
