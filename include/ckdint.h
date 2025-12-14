#ifndef SOLIDC_CKDINT_H
#define SOLIDC_CKDINT_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>  // for fprintf in your example

/* ======================================================================== */
/* Compiler detection */
/* ======================================================================== */

#if defined(_MSC_VER)
#define CKd_COMPILER_MSVC  1
#define CKd_COMPILER_GCC   0
#define CKd_COMPILER_CLANG 0
#elif defined(__clang__)
#define CKd_COMPILER_MSVC  0
#define CKd_COMPILER_GCC   0
#define CKd_COMPILER_CLANG 1
#elif defined(__GNUC__)
#define CKd_COMPILER_MSVC  0
#define CKd_COMPILER_GCC   1
#define CKd_COMPILER_CLANG 0
#else
#define CKd_COMPILER_MSVC  0
#define CKd_COMPILER_GCC   0
#define CKd_COMPILER_CLANG 0
#endif

#if (CKd_COMPILER_GCC || CKd_COMPILER_CLANG) && !CKd_COMPILER_MSVC
#if __has_builtin(__builtin_add_overflow) || (__GNUC__ >= 5)
#define HAS_OVERFLOW_BUILTINS 1
#endif
#endif
#ifndef HAS_OVERFLOW_BUILTINS
#define HAS_OVERFLOW_BUILTINS 0
#endif

/* ======================================================================== */
/* Built-in versions (GCC/Clang) */
/* ======================================================================== */

#if HAS_OVERFLOW_BUILTINS
#define ckd_add(result, a, b) __builtin_add_overflow((a), (b), (result))
#define ckd_sub(result, a, b) __builtin_sub_overflow((a), (b), (result))
#define ckd_mul(result, a, b) __builtin_mul_overflow((a), (b), (result))
#else

/* ======================================================================== */
/* Manual implementations (works on MSVC and everywhere else) */
/* ======================================================================== */

/* ----------------------- Signed addition ----------------------- */
static __inline bool ckd_add_i8(int8_t* r, int8_t a, int8_t b) {
    if ((b > 0 && a > INT8_MAX - b) || (b < 0 && a < INT8_MIN - b)) {
        *r = a + b;
        return true;
    }
    *r = a + b;
    return false;
}
static __inline bool ckd_add_i16(int16_t* r, int16_t a, int16_t b) {
    if ((b > 0 && a > INT16_MAX - b) || (b < 0 && a < INT16_MIN - b)) {
        *r = a + b;
        return true;
    }
    *r = a + b;
    return false;
}
static __inline bool ckd_add_i32(int32_t* r, int32_t a, int32_t b) {
    if ((b > 0 && a > INT32_MAX - b) || (b < 0 && a < INT32_MIN - b)) {
        *r = a + b;
        return true;
    }
    *r = a + b;
    return false;
}
static __inline bool ckd_add_i64(int64_t* r, int64_t a, int64_t b) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
        *r = a + b;
        return true;
    }
    *r = a + b;
    return false;
}

/* ----------------------- Unsigned addition ----------------------- */
static __inline bool ckd_add_u8(uint8_t* r, uint8_t a, uint8_t b) {
    *r = a + b;
    return *r < a;
}
static __inline bool ckd_add_u16(uint16_t* r, uint16_t a, uint16_t b) {
    *r = a + b;
    return *r < a;
}
static __inline bool ckd_add_u32(uint32_t* r, uint32_t a, uint32_t b) {
    *r = a + b;
    return *r < a;
}
static __inline bool ckd_add_u64(uint64_t* r, uint64_t a, uint64_t b) {
    *r = a + b;
    return *r < a;
}

/* Windows-friendly aliases: unsigned long (32/64) and unsigned long long (64) */
static __inline bool ckd_add_ul(unsigned long* r, unsigned long a, unsigned long b) {
    *r = a + b;
    return *r < a;
}
static __inline bool ckd_add_ull(unsigned long long* r, unsigned long long a, unsigned long long b) {
    *r = a + b;
    return *r < a;
}

/* ----------------------- Signed multiplication ----------------------- */
static __inline bool ckd_mul_i8(int8_t* r, int8_t a, int8_t b) {
    int16_t tmp = (int16_t)a * b;
    *r          = (int8_t)tmp;
    return tmp > INT8_MAX || tmp < INT8_MIN;
}
static __inline bool ckd_mul_i16(int16_t* r, int16_t a, int16_t b) {
    int32_t tmp = (int32_t)a * b;
    *r          = (int16_t)tmp;
    return tmp > INT16_MAX || tmp < INT16_MIN;
}
static __inline bool ckd_mul_i32(int32_t* r, int32_t a, int32_t b) {
    int64_t tmp = (int64_t)a * b;
    *r          = (int32_t)tmp;
    return tmp > INT32_MAX || tmp < INT32_MIN;
}
static __inline bool ckd_mul_i64(int64_t* r, int64_t a, int64_t b) {
    /* Simple but safe method for signed 64-bit mul */
    if (a == 0 || b == 0) {
        *r = 0;
        return false;
    }
    int64_t res = a * b;
    if (b != 0 && res / b != a) {
        *r = res;
        return true;
    }
    *r = res;
    return false;
}

/* ----------------------- Unsigned multiplication ----------------------- */
static __inline bool ckd_mul_u32(uint32_t* r, uint32_t a, uint32_t b) {
    uint64_t tmp = (uint64_t)a * b;
    *r           = (uint32_t)tmp;
    return tmp > UINT32_MAX;
}
static __inline bool ckd_mul_u64(uint64_t* r, uint64_t a, uint64_t b) {
    if (a == 0 || b == 0) {
        *r = 0;
        return false;
    }
    if (a > UINT64_MAX / b) {
        *r = a * b;
        return true;
    }
    *r = a * b;
    return false;
}

static __inline bool ckd_mul_ul(unsigned long* r, unsigned long a, unsigned long b) {
    unsigned long long tmp = (unsigned long long)a * b;
    *r                     = (unsigned long)tmp;
    return tmp > ULONG_MAX;
}

static __inline bool ckd_mul_ull(unsigned long long* r, unsigned long long a, unsigned long long b) {
    if (a == 0 || b == 0) {
        *r = 0;
        return false;
    }
    if (a > ULLONG_MAX / b) {
        *r = a * b;
        return true;
    }
    *r = a * b;
    return false;
}

#endif /* !HAS_OVERFLOW_BUILTINS */

#endif /* SOLIDC_CKDINT_H */
