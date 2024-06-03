#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define RESULT(T, E)                                                                               \
    struct Return__Result__##T {                                                                   \
        bool is_ok;                                                                                \
        union {                                                                                    \
            T value;                                                                               \
            E error;                                                                               \
        };                                                                                         \
    }

#define Option(T) RESULT(T, const char*)

#define OK(T, V)                                                                                   \
    (struct Return__Result__##T) {                                                                 \
        .is_ok = true, .value = V                                                                  \
    }
#define ERR(T, E)                                                                                  \
    (struct Return__Result__##T) {                                                                 \
        .is_ok = false, .error = E                                                                 \
    }

#define panic(msg, err)                                                                            \
    (fprintf(stderr, "%s:%d %s: %s\n", __FILE__, __LINE__, msg, err), fflush(stderr),              \
     exit(EXIT_FAILURE))

#define UNWRAP(R) ((R).is_ok ? (R).value : (panic("Unwrap error", (R).error), (R).is_ok))
#define UNWRAP_OR(R, DEFAULT) ((R).is_ok ? (R).value : (DEFAULT))
#define EXPECT(R, MSG) ((R).is_ok ? (R).value : (panic("Unwrap error", MSG), (R).is_ok))
#define UNWRAP_ERR(R) ((R).is_ok ? NULL : (R).error)
#define UNWRAP_OR_ELSE(R, DEFAULT, FUNC) ((R).is_ok ? (R).value : (FUNC((R).error), (DEFAULT)))
#define UNWRAP_OK(R, FUNC)                                                                         \
    do {                                                                                           \
        if ((R).is_ok) {                                                                           \
            FUNC((R).value);                                                                       \
        } else {                                                                                   \
            fprintf(stderr, "Error: %s\n", (R).error);                                             \
        }                                                                                          \
    } while (0)

// Define common types
typedef char* string;
typedef Option(int) ResultInt;
typedef Option(float) ResultFloat;
typedef Option(double) ResultDouble;
typedef Option(char) ResultChar;
typedef Option(string) ResultString;

// OK and ERR wrappers
#define OK_INT(V) OK(int, V)
#define OK_FLOAT(V) OK(float, V)
#define OK_DOUBLE(V) OK(double, V)
#define OK_CHAR(V) OK(char, V)
#define OK_STRING(V) OK(string, V)

#define ERR_INT(E) ERR(int, E)
#define ERR_FLOAT(E) ERR(float, E)
#define ERR_DOUBLE(E) ERR(double, E)
#define ERR_CHAR(E) ERR(char, E)
#define ERR_STRING(E) ERR(string, E)

#endif
