/**
 * @file simd.h
 * @brief Portable Single Instruction Multiple Data (SIMD) Intrinsics Wrapper.
 *
 * This header provides a unified abstraction layer over hardware-accelerated
 * vector operations. It automatically detects the target architecture and
 * maps abstract vector types to the underlying hardware registers.
 *
 * @details
 * Supported Backends:
 * - **x86/x64**: Uses SSE (Streaming SIMD Extensions). Baseline is SSE2.
 *   - Automatic optimizations for SSE4.1(via compiler flags).
 * - **ARM**: Uses NEON.
 *   - Specific paths for ARMv7 (32-bit) and AArch64 (64-bit).
 * - **Scalar**: Fallback for architectures without supported SIMD units.
 *
 * Design Philosophy:
 * - **Vertical Operations**: Primary focus. Operations like add, sub, mul happen
 *   component-wise ($x+x, y+y, \dots$). These are the fastest instructions.
 * - **Horizontal Operations**: Operations like dot products or reductions ($x+y+z+w$)
 *   are generally slower than vertical ones and are implemented using shuffles/adds.
 * - **SOA (Structure of Arrays)**: This library is best used with Data-Oriented Design
 *   where data is laid out as `xxxx... yyyy... zzzz...` rather than `xyz xyz...`.
 *
 * Usage Example:
 * @code
 *   simd_vec_t pos_a = simd_set(10.0f, 20.0f, 30.0f, 1.0f);
 *   simd_vec_t pos_b = simd_set(5.0f, 5.0f, 5.0f, 0.0f);
 *
 *   // Vertical addition (happens in one CPU cycle equivalent)
 *   simd_vec_t result = simd_add(pos_a, pos_b);
 *
 *   // Dot product
 *   float dist_sq = simd_dot3(result, result);
 * @endcode
 */

#ifndef SIMD_H
#define SIMD_H

#include "align.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* =========================================================
   Architecture Detection
   ========================================================= */

/*
 * Detect x86_64 or x86.
 * Note: We assume SSE2 is available on all modern x86 targets.
 */
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define SIMD_ARCH_X86
#include <immintrin.h>

/* Check for SSE4.1 support (Standard on Core 2 Duo Penryn and later) */
#if defined(__SSE4_1__)
#define SIMD_HAS_SSE41
#endif

/*
 * Detect ARM NEON.
 * Distinguishes between AArch64 (ARMv8) and ARMv7 (32-bit).
 */
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#define SIMD_ARCH_ARM
#define SIMD_ARCH_ARM64
#include <arm_neon.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SIMD_ARCH_ARM
#define SIMD_ARCH_ARM32
#include <arm_neon.h>

/* Fallback for generic C implementation */
#else
#define SIMD_ARCH_SCALAR
#endif

/* =========================================================
   Type Definitions
   ========================================================= */

/**
 * @typedef simd_vec_t
 * @brief Represents a 128-bit vector containing 4 floating point numbers.
 *
 * Memory Layout: [float x, float y, float z, float w]
 * Alignment: 16-byte alignment is recommended for performance, though
 * this library utilizes unaligned load/store instructions for safety.
 */
#if defined(SIMD_ARCH_X86)
/* Maps to __m128 (XMM registers) */
typedef __m128 simd_vec_t;
typedef __m128i simd_ivec_t;
#elif defined(SIMD_ARCH_ARM)
/* Maps to float32x4_t (Q registers) */
typedef float32x4_t simd_vec_t;
typedef int32x4_t simd_ivec_t;
#else
/* Maps to a struct of 4 floats */
typedef struct {
    float f[4];
} simd_vec_t;
typedef struct {
    int i[4];
} simd_ivec_t;
#endif

/* =========================================================
   Load / Store / Set
   ========================================================= */

/**
 * @brief Creates a vector with all components set to 0.0f.
 */
static inline simd_vec_t simd_set_zero(void) {
#if defined(SIMD_ARCH_X86)
    return _mm_setzero_ps();
#elif defined(SIMD_ARCH_ARM)
    return vdupq_n_f32(0.0f);
#else
    return (simd_vec_t){{0, 0, 0, 0}};
#endif
}

/**
 * @brief Creates a vector from 4 individual floats.
 *
 * @param x Component 0
 * @param y Component 1
 * @param z Component 2
 * @param w Component 3
 * @return Vector [x, y, z, w]
 */
static inline simd_vec_t simd_set(float x, float y, float z, float w) {
#if defined(SIMD_ARCH_X86)
    /*
     * _mm_set_ps inputs are reversed (w, z, y, x) to map to
     * registers [3, 2, 1, 0]. We flip them here for API consistency.
     */
    return _mm_set_ps(w, z, y, x);
#elif defined(SIMD_ARCH_ARM)
    float d[4] = {x, y, z, w};
    return vld1q_f32(d);
#else
    return (simd_vec_t){{x, y, z, w}};
#endif
}

/**
 * @brief Broadcasts a single float to all 4 components.
 * @return Vector [s, s, s, s]
 */
static inline simd_vec_t simd_set1(float s) {
#if defined(SIMD_ARCH_X86)
    return _mm_set1_ps(s);
#elif defined(SIMD_ARCH_ARM)
    return vdupq_n_f32(s);
#else
    return (simd_vec_t){{s, s, s, s}};
#endif
}

/**
 * @brief Loads 4 floats from memory.
 *
 * @note Uses unaligned loads (_mm_loadu_ps). This is safe for any pointer
 * but might be slightly slower than _mm_load_ps on older hardware if
 * the pointer is not 16-byte aligned. On modern CPUs, the penalty is negligible.
 */
static inline simd_vec_t simd_load(const float* ptr) {
#if defined(SIMD_ARCH_X86)
    return _mm_loadu_ps(ptr);
#elif defined(SIMD_ARCH_ARM)
    return vld1q_f32(ptr);
#else
    simd_vec_t r;
    memcpy(r.f, ptr, 4 * sizeof(float));
    return r;
#endif
}

/**
 * @brief Stores vector data into memory.
 * @note Uses unaligned stores.
 */
static inline void simd_store(float* ptr, simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    _mm_storeu_ps(ptr, v);
#elif defined(SIMD_ARCH_ARM)
    vst1q_f32(ptr, v);
#else
    memcpy(ptr, v.f, 4 * sizeof(float));
#endif
}

/* =========================================================
   Arithmetic
   ========================================================= */

/** @brief Component-wise addition: a + b */
static inline simd_vec_t simd_add(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_add_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vaddq_f32(a, b);
#else
    return (simd_vec_t){{a.f[0] + b.f[0], a.f[1] + b.f[1], a.f[2] + b.f[2], a.f[3] + b.f[3]}};
#endif
}

/** @brief Component-wise subtraction: a - b */
static inline simd_vec_t simd_sub(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_sub_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vsubq_f32(a, b);
#else
    return (simd_vec_t){{a.f[0] - b.f[0], a.f[1] - b.f[1], a.f[2] - b.f[2], a.f[3] - b.f[3]}};
#endif
}

/** @brief Component-wise multiplication: a * b */
static inline simd_vec_t simd_mul(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_mul_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vmulq_f32(a, b);
#else
    return (simd_vec_t){{a.f[0] * b.f[0], a.f[1] * b.f[1], a.f[2] * b.f[2], a.f[3] * b.f[3]}};
#endif
}

/**
 * @brief Component-wise division: a / b
 *
 * @note On ARMv7 (32-bit), this uses a Reciprocal Estimate + Newton-Raphson step
 * because the hardware lacks a direct vector division instruction.
 */
static inline simd_vec_t simd_div(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_div_ps(a, b);
#elif defined(SIMD_ARCH_ARM64)
    return vdivq_f32(a, b);  // Native division on AArch64
#elif defined(SIMD_ARCH_ARM32)
    /*
     * ARMv7 optimization: x / y = x * (1/y)
     * 1. Estimate reciprocal (vrecpeq)
     * 2. Refine estimate using Newton-Raphson step (vrecpsq)
     * 3. Multiply
     */
    simd_vec_t recip = vrecpeq_f32(b);
    recip = vmulq_f32(vrecpsq_f32(b, recip), recip);
    return vmulq_f32(a, recip);
#else
    return (simd_vec_t){{a.f[0] / b.f[0], a.f[1] / b.f[1], a.f[2] / b.f[2], a.f[3] / b.f[3]}};
#endif
}

/**
 * @brief Fused Multiply Add: result = (a * b) + c
 *
 * @note Uses hardware FMA where available (SSE4.1/FMA3/NEON).
 * Falls back to mul + add if hardware support is missing.
 */
static inline simd_vec_t simd_madd(simd_vec_t a, simd_vec_t b, simd_vec_t c) {
#if defined(SIMD_ARCH_X86)
#if defined(__FMA__)
    return _mm_fmadd_ps(a, b, c);
#else
    return _mm_add_ps(_mm_mul_ps(a, b), c);
#endif
#elif defined(SIMD_ARCH_ARM)
    return vmlaq_f32(c, a, b);  // NEON instruction acts as accumulator: c += a*b
#else
    return simd_add(simd_mul(a, b), c);
#endif
}

/** @brief Negates components: -x */
static inline simd_vec_t simd_neg(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    return _mm_sub_ps(_mm_setzero_ps(), v);
#elif defined(SIMD_ARCH_ARM)
    return vnegq_f32(v);
#else
    return (simd_vec_t){{-v.f[0], -v.f[1], -v.f[2], -v.f[3]}};
#endif
}

/** @brief Absolute value: |x| */
static inline simd_vec_t simd_abs(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    /* Clear the sign bit (MSB) using a mask */
    static const __m128 sign_mask = {-0.0f, -0.0f, -0.0f, -0.0f};  // 0x80000000
    return _mm_andnot_ps(sign_mask, v);
#elif defined(SIMD_ARCH_ARM)
    return vabsq_f32(v);
#else
    return (simd_vec_t){{fabsf(v.f[0]), fabsf(v.f[1]), fabsf(v.f[2]), fabsf(v.f[3])}};
#endif
}

/* =========================================================
   Min / Max
   ========================================================= */

/** @brief Component-wise minimum */
static inline simd_vec_t simd_min(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_min_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vminq_f32(a, b);
#else
    return (simd_vec_t){{a.f[0] < b.f[0] ? a.f[0] : b.f[0], a.f[1] < b.f[1] ? a.f[1] : b.f[1],
                         a.f[2] < b.f[2] ? a.f[2] : b.f[2], a.f[3] < b.f[3] ? a.f[3] : b.f[3]}};
#endif
}

/** @brief Component-wise maximum */
static inline simd_vec_t simd_max(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_max_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vmaxq_f32(a, b);
#else
    return (simd_vec_t){{a.f[0] > b.f[0] ? a.f[0] : b.f[0], a.f[1] > b.f[1] ? a.f[1] : b.f[1],
                         a.f[2] > b.f[2] ? a.f[2] : b.f[2], a.f[3] > b.f[3] ? a.f[3] : b.f[3]}};
#endif
}

/* =========================================================
   Math Functions
   ========================================================= */

/** @brief Component-wise Square Root */
static inline simd_vec_t simd_sqrt(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    return _mm_sqrt_ps(v);
#elif defined(SIMD_ARCH_ARM64)
    return vsqrtq_f32(v);
#elif defined(SIMD_ARCH_ARM32)
    /* ARMv7 NEON lacks native vector sqrt, fallback to scalar */
    simd_vec_t result;
    float temp[4];
    vst1q_f32(temp, v);
    for (int i = 0; i < 4; ++i) temp[i] = sqrtf(temp[i]);
    return vld1q_f32(temp);
#else
    return (simd_vec_t){{sqrtf(v.f[0]), sqrtf(v.f[1]), sqrtf(v.f[2]), sqrtf(v.f[3])}};
#endif
}

/**
 * @brief Reciprocal Square Root (Approximation): 1.0 / sqrt(x)
 *
 * @note This is much faster than 1.0f/sqrtf(x), but has lower precision
 * (typically ~12 bits). Suitable for lighting calculations or normalizing
 * vectors where perfect precision isn't required.
 */
static inline simd_vec_t simd_rsqrt(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    return _mm_rsqrt_ps(v);
#elif defined(SIMD_ARCH_ARM)
    return vrsqrteq_f32(v);
#else
    return (simd_vec_t){{1.0f / sqrtf(v.f[0]), 1.0f / sqrtf(v.f[1]), 1.0f / sqrtf(v.f[2]), 1.0f / sqrtf(v.f[3])}};
#endif
}

/**
 * @brief Reciprocal (Approximation): 1.0 / x
 * @note Lower precision (~12 bits), faster than division.
 */
static inline simd_vec_t simd_rcp(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    return _mm_rcp_ps(v);
#elif defined(SIMD_ARCH_ARM)
    return vrecpeq_f32(v);
#else
    return (simd_vec_t){{1.0f / v.f[0], 1.0f / v.f[1], 1.0f / v.f[2], 1.0f / v.f[3]}};
#endif
}

/** @brief Round down to nearest integer (returns float) */
static inline simd_vec_t simd_floor(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
#ifdef SIMD_HAS_SSE41
    return _mm_floor_ps(v);
#else
    /* SSE2 Fallback: conversion via scalar math */
    float temp[4];
    _mm_storeu_ps(temp, v);
    for (int i = 0; i < 4; ++i) temp[i] = floorf(temp[i]);
    return _mm_loadu_ps(temp);
#endif
#elif defined(SIMD_ARCH_ARM64)
    return vrndmq_f32(v);  // Round towards minus infinity
#elif defined(SIMD_ARCH_ARM32)
    float temp[4];
    vst1q_f32(temp, v);
    for (int i = 0; i < 4; ++i) temp[i] = floorf(temp[i]);
    return vld1q_f32(temp);
#else
    return (simd_vec_t){{floorf(v.f[0]), floorf(v.f[1]), floorf(v.f[2]), floorf(v.f[3])}};
#endif
}

/** @brief Round up to nearest integer (returns float) */
static inline simd_vec_t simd_ceil(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
#ifdef SIMD_HAS_SSE41
    return _mm_ceil_ps(v);
#else
    float temp[4];
    _mm_storeu_ps(temp, v);
    for (int i = 0; i < 4; ++i) temp[i] = ceilf(temp[i]);
    return _mm_loadu_ps(temp);
#endif
#elif defined(SIMD_ARCH_ARM64)
    return vrndpq_f32(v);  // Round towards plus infinity
#elif defined(SIMD_ARCH_ARM32)
    float temp[4];
    vst1q_f32(temp, v);
    for (int i = 0; i < 4; ++i) temp[i] = ceilf(temp[i]);
    return vld1q_f32(temp);
#else
    return (simd_vec_t){{ceilf(v.f[0]), ceilf(v.f[1]), ceilf(v.f[2]), ceilf(v.f[3])}};
#endif
}

/** @brief Round to nearest integer */
static inline simd_vec_t simd_round(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
#ifdef SIMD_HAS_SSE41
    return _mm_round_ps(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
#else
    float temp[4];
    _mm_storeu_ps(temp, v);
    for (int i = 0; i < 4; ++i) temp[i] = roundf(temp[i]);
    return _mm_loadu_ps(temp);
#endif
#elif defined(SIMD_ARCH_ARM64)
    return vrndnq_f32(v);  // Round nearest, ties to even
#elif defined(SIMD_ARCH_ARM32)
    float temp[4];
    vst1q_f32(temp, v);
    for (int i = 0; i < 4; ++i) temp[i] = roundf(temp[i]);
    return vld1q_f32(temp);
#else
    return (simd_vec_t){{roundf(v.f[0]), roundf(v.f[1]), roundf(v.f[2]), roundf(v.f[3])}};
#endif
}

/* =========================================================
   Comparison Operations
   ========================================================= */

/*
 * NOTE ON COMPARISONS:
 * SIMD comparisons do NOT return true/false booleans.
 * They return a bitmask:
 * - If True:  All bits set to 1 (0xFFFFFFFF or NaN)
 * - If False: All bits set to 0 (0x00000000 or 0.0f)
 *
 * This allows "Branchless Programming" via bitwise masking (simd_and, simd_blend).
 */

static inline simd_vec_t simd_cmpeq(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_cmpeq_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vceqq_f32(a, b));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t mask = (a.f[i] == b.f[i]) ? 0xFFFFFFFF : 0;
        memcpy(&result.f[i], &mask, sizeof(uint32_t));
    }
    return result;
#endif
}

static inline simd_vec_t simd_cmpneq(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_cmpneq_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vmvnq_u32(vceqq_f32(a, b)));  // Bitwise NOT of Equals
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t mask = (a.f[i] != b.f[i]) ? 0xFFFFFFFF : 0;
        memcpy(&result.f[i], &mask, sizeof(uint32_t));
    }
    return result;
#endif
}

static inline simd_vec_t simd_cmplt(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_cmplt_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vcltq_f32(a, b));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t mask = (a.f[i] < b.f[i]) ? 0xFFFFFFFF : 0;
        memcpy(&result.f[i], &mask, sizeof(uint32_t));
    }
    return result;
#endif
}

static inline simd_vec_t simd_cmple(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_cmple_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vcleq_f32(a, b));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t mask = (a.f[i] <= b.f[i]) ? 0xFFFFFFFF : 0;
        memcpy(&result.f[i], &mask, sizeof(uint32_t));
    }
    return result;
#endif
}

static inline simd_vec_t simd_cmpgt(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_cmpgt_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vcgtq_f32(a, b));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t mask = (a.f[i] > b.f[i]) ? 0xFFFFFFFF : 0;
        memcpy(&result.f[i], &mask, sizeof(uint32_t));
    }
    return result;
#endif
}

static inline simd_vec_t simd_cmpge(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_cmpge_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vcgeq_f32(a, b));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t mask = (a.f[i] >= b.f[i]) ? 0xFFFFFFFF : 0;
        memcpy(&result.f[i], &mask, sizeof(uint32_t));
    }
    return result;
#endif
}

/* =========================================================
   Bitwise Operations
   ========================================================= */

static inline simd_vec_t simd_and(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_and_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    /* NEON float types require reinterpretation to integer types for bitwise ops */
    return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t ai, bi, ri;
        memcpy(&ai, &a.f[i], sizeof(uint32_t));
        memcpy(&bi, &b.f[i], sizeof(uint32_t));
        ri = ai & bi;
        memcpy(&result.f[i], &ri, sizeof(uint32_t));
    }
    return result;
#endif
}

static inline simd_vec_t simd_or(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_or_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t ai, bi, ri;
        memcpy(&ai, &a.f[i], sizeof(uint32_t));
        memcpy(&bi, &b.f[i], sizeof(uint32_t));
        ri = ai | bi;
        memcpy(&result.f[i], &ri, sizeof(uint32_t));
    }
    return result;
#endif
}

static inline simd_vec_t simd_xor(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_xor_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t ai, bi, ri;
        memcpy(&ai, &a.f[i], sizeof(uint32_t));
        memcpy(&bi, &b.f[i], sizeof(uint32_t));
        ri = ai ^ bi;
        memcpy(&result.f[i], &ri, sizeof(uint32_t));
    }
    return result;
#endif
}

/** @brief Bitwise NOT-AND: (~a) & b */
static inline simd_vec_t simd_andnot(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    return _mm_andnot_ps(a, b);
#elif defined(SIMD_ARCH_ARM)
    return vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(b), vreinterpretq_u32_f32(a)));
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t ai, bi, ri;
        memcpy(&ai, &a.f[i], sizeof(uint32_t));
        memcpy(&bi, &b.f[i], sizeof(uint32_t));
        ri = (~ai) & bi;
        memcpy(&result.f[i], &ri, sizeof(uint32_t));
    }
    return result;
#endif
}

/* =========================================================
   Shuffles / Permutations
   ========================================================= */

/* Macros to duplicate a specific component across the vector */
#if defined(SIMD_ARCH_X86)
/* _MM_SHUFFLE parameters are (w, z, y, x) indices */
#define simd_dup_x(v) _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0))
#define simd_dup_y(v) _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1))
#define simd_dup_z(v) _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2))
#define simd_dup_w(v) _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3))
#elif defined(SIMD_ARCH_ARM)
/* NEON lane extraction requires compile-time constant indices */
#define simd_dup_x(v) vdupq_n_f32(vgetq_lane_f32(v, 0))
#define simd_dup_y(v) vdupq_n_f32(vgetq_lane_f32(v, 1))
#define simd_dup_z(v) vdupq_n_f32(vgetq_lane_f32(v, 2))
#define simd_dup_w(v) vdupq_n_f32(vgetq_lane_f32(v, 3))
#else
#define simd_dup_x(v) simd_set1(v.f[0])
#define simd_dup_y(v) simd_set1(v.f[1])
#define simd_dup_z(v) simd_set1(v.f[2])
#define simd_dup_w(v) simd_set1(v.f[3])
#endif

/**
 * @brief Component-wise Blend (Select).
 *
 * Equivalent to ternary operator: mask ? true_vec : false_vec.
 * The mask is expected to be the result of a comparison function
 * (all 1s or all 0s per component).
 *
 * @param false_vec Result if mask is 0
 * @param true_vec Result if mask is 1
 * @param mask Comparison mask
 */
static inline simd_vec_t simd_blend(simd_vec_t false_vec, simd_vec_t true_vec, simd_vec_t mask) {
#if defined(SIMD_ARCH_X86)
#ifdef SIMD_HAS_SSE41
    /* SSE4.1 dedicated blend instruction */
    return _mm_blendv_ps(false_vec, true_vec, mask);
#else
    /* SSE2 Fallback: (mask & true) | (~mask & false) */
    return _mm_or_ps(_mm_and_ps(mask, true_vec), _mm_andnot_ps(mask, false_vec));
#endif
#elif defined(SIMD_ARCH_ARM)
    /* vbslq: Bitwise Select. Selects from true_vec if mask bit is 1, else false_vec */
    uint32x4_t uint_mask = vreinterpretq_u32_f32(mask);
    return vbslq_f32(uint_mask, true_vec, false_vec);
#else
    simd_vec_t result;
    for (int i = 0; i < 4; i++) {
        uint32_t mask_int;
        memcpy(&mask_int, &mask.f[i], sizeof(uint32_t));
        /* Check sign bit for quick selection */
        result.f[i] = (mask_int & 0x80000000) ? true_vec.f[i] : false_vec.f[i];
    }
    return result;
#endif
}

/* =========================================================
   Reductions (Horizontal operations)
   ========================================================= */

/**
 * @brief Horizontal Addition: x + y + z + w
 *
 * @note Horizontal operations generally require shuffling and are
 * slower than vertical arithmetic.
 */
static inline float simd_hadd(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    /*
     * Butterfly reduction:
     * 1. Shuffle high half to low half: [z, w, x, y]
     * 2. Add: [x+z, y+w, ...]
     * 3. Shuffle result's high to low: [y+w, ...]
     * 4. Add: [x+z+y+w, ...]
     */
    __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(v, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ps(sums, shuf);
    return _mm_cvtss_f32(sums);
#elif defined(SIMD_ARCH_ARM64)
    return vaddvq_f32(v); /* AArch64 hardware instruction */
#elif defined(SIMD_ARCH_ARM32)
    /* Pairwise add: [x+y, z+w] -> [x+y+z+w] */
    float32x2_t r = vadd_f32(vget_high_f32(v), vget_low_f32(v));
    r = vpadd_f32(r, r);
    return vget_lane_f32(r, 0);
#else
    return v.f[0] + v.f[1] + v.f[2] + v.f[3];
#endif
}

/** @brief Horizontal Minimum: min(x, y, z, w) */
static inline float simd_hmin(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 mins = _mm_min_ps(v, shuf);
    shuf = _mm_movehl_ps(shuf, mins);
    mins = _mm_min_ps(mins, shuf);
    return _mm_cvtss_f32(mins);
#elif defined(SIMD_ARCH_ARM64)
    return vminvq_f32(v);
#elif defined(SIMD_ARCH_ARM32)
    float32x2_t r = vmin_f32(vget_high_f32(v), vget_low_f32(v));
    r = vpmin_f32(r, r);
    return vget_lane_f32(r, 0);
#else
    float min = v.f[0];
    if (v.f[1] < min) min = v.f[1];
    if (v.f[2] < min) min = v.f[2];
    if (v.f[3] < min) min = v.f[3];
    return min;
#endif
}

/** @brief Horizontal Maximum: max(x, y, z, w) */
static inline float simd_hmax(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 maxs = _mm_max_ps(v, shuf);
    shuf = _mm_movehl_ps(shuf, maxs);
    maxs = _mm_max_ps(maxs, shuf);
    return _mm_cvtss_f32(maxs);
#elif defined(SIMD_ARCH_ARM64)
    return vmaxvq_f32(v);
#elif defined(SIMD_ARCH_ARM32)
    float32x2_t r = vmax_f32(vget_high_f32(v), vget_low_f32(v));
    r = vpmax_f32(r, r);
    return vget_lane_f32(r, 0);
#else
    float max = v.f[0];
    if (v.f[1] > max) max = v.f[1];
    if (v.f[2] > max) max = v.f[2];
    if (v.f[3] > max) max = v.f[3];
    return max;
#endif
}

/* =========================================================
   Dot Product Operations
   ========================================================= */

/**
 * @brief 3D Dot Product.
 * @return (x*x + y*y + z*z) as a scalar float.
 * @note Ignores the w component.
 */
/* 3D dot product (ignores W component) */
static inline float simd_dot3(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
#ifdef SIMD_HAS_SSE41
    return _mm_cvtss_f32(_mm_dp_ps(a, b, 0x71));  // mask: xxxx0111
#else
    __m128 mul = _mm_mul_ps(a, b);

    /*
     * Fix: Use Multiplication to mask out W.
     * Previous version incorrectly used _mm_and_ps with 1.0f, which corrupts bits.
     */
    __m128 mask = _mm_set_ps(0.0f, 1.0f, 1.0f, 1.0f);  // x=1, y=1, z=1, w=0
    mul = _mm_mul_ps(mul, mask);                       // Multiply, don't AND

    return simd_hadd(mul);
#endif
#elif defined(SIMD_ARCH_ARM)
    simd_vec_t mul = vmulq_f32(a, b);
    mul = vsetq_lane_f32(0.0f, mul, 3);  // Set W lane to 0
#if defined(SIMD_ARCH_ARM64)
    return vaddvq_f32(mul);
#else
    float32x2_t r = vadd_f32(vget_high_f32(mul), vget_low_f32(mul));
    r = vpadd_f32(r, r);
    return vget_lane_f32(r, 0);
#endif
#else
    return a.f[0] * b.f[0] + a.f[1] * b.f[1] + a.f[2] * b.f[2];
#endif
}

/**
 * @brief 4D Dot Product.
 * @return (x*x + y*y + z*z + w*w) as a scalar float.
 */
static inline float simd_dot4(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
#ifdef SIMD_HAS_SSE41
    /* _mm_dp_ps mask 0xF1: SrcMask=1111 (xyzw), DstMask=0001 (store in x) */
    return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xF1));
#else
    return simd_hadd(_mm_mul_ps(a, b));
#endif
#elif defined(SIMD_ARCH_ARM)
    simd_vec_t mul = vmulq_f32(a, b);
#if defined(SIMD_ARCH_ARM64)
    return vaddvq_f32(mul);
#else
    float32x2_t r = vadd_f32(vget_high_f32(mul), vget_low_f32(mul));
    r = vpadd_f32(r, r);
    return vget_lane_f32(r, 0);
#endif
#else
    return a.f[0] * b.f[0] + a.f[1] * b.f[1] + a.f[2] * b.f[2] + a.f[3] * b.f[3];
#endif
}

/* =========================================================
   3D Vector Operations
   ========================================================= */

/**
 * @brief 3D Cross Product: a x b
 *
 * Formula:
 * x = ay*bz - az*by
 * y = az*bx - ax*bz
 * z = ax*by - ay*bx
 * w = 0.0
 */
static inline simd_vec_t simd_cross(simd_vec_t a, simd_vec_t b) {
#if defined(SIMD_ARCH_X86)
    /*
     * Efficient shuffle method for SSE:
     * 1. Shuffle A to [y, z, x]
     * 2. Shuffle B to [z, x, y]
     * 3. Mul
     * 4. Subtract the reverse shuffle multiplication
     */
    __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));

    __m128 mul1 = _mm_mul_ps(a_yzx, b_zxy);
    __m128 mul2 = _mm_mul_ps(a_zxy, b_yzx);

    return _mm_sub_ps(mul1, mul2);
#elif defined(SIMD_ARCH_ARM)
    /*
     * ARM implementation uses a manual float shuffle approach for clarity
     * and because NEON shuffles (tbl) can be high latency on older chips.
     */
    float temp_a[4], temp_b[4];
    vst1q_f32(temp_a, a);
    vst1q_f32(temp_b, b);

    float result[4];
    result[0] = temp_a[1] * temp_b[2] - temp_a[2] * temp_b[1];
    result[1] = temp_a[2] * temp_b[0] - temp_a[0] * temp_b[2];
    result[2] = temp_a[0] * temp_b[1] - temp_a[1] * temp_b[0];
    result[3] = 0.0f;

    return vld1q_f32(result);
#else
    return (simd_vec_t){{a.f[1] * b.f[2] - a.f[2] * b.f[1], a.f[2] * b.f[0] - a.f[0] * b.f[2],
                         a.f[0] * b.f[1] - a.f[1] * b.f[0], 0.0f}};
#endif
}

/** @brief Length squared (3D, ignores w) */
static inline float simd_length_sq3(simd_vec_t v) { return simd_dot3(v, v); }

/** @brief Length (3D, ignores w) */
static inline float simd_length3(simd_vec_t v) { return sqrtf(simd_dot3(v, v)); }

/** @brief Length squared (4D) */
static inline float simd_length_sq4(simd_vec_t v) { return simd_dot4(v, v); }

/** @brief Length (4D) */
static inline float simd_length4(simd_vec_t v) { return sqrtf(simd_dot4(v, v)); }

/**
 * @brief Normalize Vector (3D).
 * @return Unit vector. Preserves the original W component.
 * @note Returns original vector if length is 0.
 */
static inline simd_vec_t simd_normalize3(simd_vec_t v) {
    float len_sq = simd_dot3(v, v);
    if (len_sq > 0.0f) {
        float inv_len = 1.0f / sqrtf(len_sq);
        simd_vec_t scale = simd_set1(inv_len);
        simd_vec_t result = simd_mul(v, scale);

        /* Restore original W component */
#if defined(SIMD_ARCH_X86)
        /* SSE4.1 blend is ideal, fallback is shuffle or logic */
        return _mm_blend_ps(result, v, 0x8);  // Mask 1000: copy W from v
#elif defined(SIMD_ARCH_ARM)
        return vsetq_lane_f32(vgetq_lane_f32(v, 3), result, 3);
#else
        result.f[3] = v.f[3];
        return result;
#endif
    }
    return v;
}

/** @brief Normalize Vector (4D) */
static inline simd_vec_t simd_normalize4(simd_vec_t v) {
    float len_sq = simd_dot4(v, v);
    if (len_sq > 0.0f) {
        float inv_len = 1.0f / sqrtf(len_sq);
        return simd_mul(v, simd_set1(inv_len));
    }
    return v;
}

/**
 * @brief Fast Normalize (3D) using rsqrt approximation.
 * @return Unit vector (approximate).
 */
static inline simd_vec_t simd_normalize3_fast(simd_vec_t v) {
    simd_vec_t len_sq = simd_set1(simd_dot3(v, v));
    simd_vec_t inv_len = simd_rsqrt(len_sq);
    simd_vec_t result = simd_mul(v, inv_len);

    /* Restore W */
#if defined(SIMD_ARCH_X86)
    return _mm_blend_ps(result, v, 0x8);
#elif defined(SIMD_ARCH_ARM)
    return vsetq_lane_f32(vgetq_lane_f32(v, 3), result, 3);
#else
    result.f[3] = v.f[3];
    return result;
#endif
}

/* =========================================================
   Comparison Helper
   ========================================================= */

/**
 * @brief Checks if two vectors are equal within a small epsilon.
 *
 * @param epsilon The error tolerance (e.g., 0.0001f)
 * @return true if all components are close, false otherwise.
 */
static inline bool simd_equals_eps(simd_vec_t a, simd_vec_t b, float epsilon) {
    simd_vec_t sub = simd_sub(a, b);

#if defined(SIMD_ARCH_X86)
    /* Abs(sub) < epsilon */
    static const __m128 sign_mask = {-0.0f, -0.0f, -0.0f, -0.0f};
    __m128 abs_diff = _mm_andnot_ps(sign_mask, sub);
    __m128 eps_vec = _mm_set1_ps(epsilon);
    __m128 cmp = _mm_cmplt_ps(abs_diff, eps_vec);

    /* _mm_movemask_ps returns an int where bits 0-3 correspond to vector lanes */
    return _mm_movemask_ps(cmp) == 0xF;
#elif defined(SIMD_ARCH_ARM)
    simd_vec_t abs_diff = vabsq_f32(sub);
    simd_vec_t eps_vec = vdupq_n_f32(epsilon);
    uint32x4_t cmp = vcltq_f32(abs_diff, eps_vec);

    /* Check if all bits are set in the comparison result */
    uint32x2_t min = vmin_u32(vget_low_u32(cmp), vget_high_u32(cmp));
    return vget_lane_u32(min, 0) == 0xFFFFFFFF && vget_lane_u32(min, 1) == 0xFFFFFFFF;
#else
    return fabsf(a.f[0] - b.f[0]) < epsilon && fabsf(a.f[1] - b.f[1]) < epsilon && fabsf(a.f[2] - b.f[2]) < epsilon &&
           fabsf(a.f[3] - b.f[3]) < epsilon;
#endif
}

/* =========================================================
   Swizzling (Rearranging Components)
   ========================================================= */

/* Indices for readability */
#define SIMD_X 0
#define SIMD_Y 1
#define SIMD_Z 2
#define SIMD_W 3

/**
 * @brief Rearranges vector components.
 *
 * Usage:
 *   simd_vec_t result = simd_swizzle(v, SIMD_Y, SIMD_X, SIMD_W, SIMD_Z);
 *
 * @note This MUST be a macro because x86 intrinsics require compile-time constants.
 */
#if defined(SIMD_ARCH_X86)

/*
 * _MM_SHUFFLE(w, z, y, x) takes indices in reverse order.
 * We pass v for both inputs to shuffle from the same vector.
 */
#define simd_swizzle(v, x, y, z, w) _mm_shuffle_ps((v), (v), _MM_SHUFFLE((w), (z), (y), (x)))

#elif defined(SIMD_ARCH_ARM)

/*
 * GCC and Clang (used for 99% of Android/iOS/Linux ARM) support
 * __builtin_shufflevector. This maps to the optimal NEON instruction
 * (usually vtbl, vext, or simple moves).
 */
#if defined(__clang__) || defined(__GNUC__)
#define simd_swizzle(v, x, y, z, w) __builtin_shufflevector((v), (v), (x), (y), (z), (w))
#else
/* MSVC ARM or other compilers: Fallback to slow extract/set */
static inline simd_vec_t simd_swizzle_fallback(simd_vec_t v, int i0, int i1, int i2, int i3) {
    float d[4];
    vst1q_f32(d, v);
    float r[4] = {d[i0], d[i1], d[i2], d[i3]};
    return vld1q_f32(r);
}
#define simd_swizzle(v, x, y, z, w) simd_swizzle_fallback((v), (x), (y), (z), (w))
#endif

#else

/* Scalar Fallback */
#define simd_swizzle(v, x, y, z, w) ((simd_vec_t){{(v).f[(x)], (v).f[(y)], (v).f[(z)], (v).f[(w)]}})

#endif

/* Broadcast macros */
#define simd_splat_x(v) simd_swizzle(v, 0, 0, 0, 0)
#define simd_splat_y(v) simd_swizzle(v, 1, 1, 1, 1)
#define simd_splat_z(v) simd_swizzle(v, 2, 2, 2, 2)
#define simd_splat_w(v) simd_swizzle(v, 3, 3, 3, 3)

/* Common rotations */
#define simd_yzxw(v) simd_swizzle(v, 1, 2, 0, 3)  // Useful for cross product
#define simd_zxyw(v) simd_swizzle(v, 2, 0, 1, 3)  // Useful for cross product

/* =========================================================
   Matrix Helpers
   ========================================================= */

/** @brief Extracts the first component (X) as a scalar float. */
static inline float simd_get_x(simd_vec_t v) {
#if defined(SIMD_ARCH_X86)
    return _mm_cvtss_f32(v);
#elif defined(SIMD_ARCH_ARM)
    return vgetq_lane_f32(v, 0);
#else
    return v.f[0];
#endif
}

/** @brief Checks if all bits in the mask are set (True). */
static inline bool simd_check_all(simd_vec_t mask) {
#if defined(SIMD_ARCH_X86)
    return _mm_movemask_ps(mask) == 0xF;
#elif defined(SIMD_ARCH_ARM)
    // Check if minimum of all lanes is NOT 0
    uint32x4_t u = vreinterpretq_u32_f32(mask);
    uint32x2_t min = vmin_u32(vget_low_u32(u), vget_high_u32(u));
    return (vget_lane_u32(min, 0) & vget_lane_u32(min, 1)) == 0xFFFFFFFF;
#else
    uint32_t* i = (uint32_t*)&mask;
    return i[0] && i[1] && i[2] && i[3];
#endif
}

/** @brief Transposes a 4x4 matrix defined by 4 row/col vectors in-place. */
#if defined(SIMD_ARCH_X86)
#define simd_transpose4(r0, r1, r2, r3) _MM_TRANSPOSE4_PS(r0, r1, r2, r3)
#elif defined(SIMD_ARCH_ARM)
#define simd_transpose4(r0, r1, r2, r3)                                        \
    do {                                                                       \
        float32x4x2_t t0 = vtrnq_f32(r0, r1);                                  \
        float32x4x2_t t1 = vtrnq_f32(r2, r3);                                  \
        r0 = vcombine_f32(vget_low_f32(t0.val[0]), vget_low_f32(t1.val[0]));   \
        r1 = vcombine_f32(vget_low_f32(t0.val[1]), vget_low_f32(t1.val[1]));   \
        r2 = vcombine_f32(vget_high_f32(t0.val[0]), vget_high_f32(t1.val[0])); \
        r3 = vcombine_f32(vget_high_f32(t0.val[1]), vget_high_f32(t1.val[1])); \
    } while (0)
#else
#define simd_transpose4(r0, r1, r2, r3)                          \
    do {                                                         \
        simd_vec_t t0 = r0, t1 = r1, t2 = r2, t3 = r3;           \
        r0 = (simd_vec_t){{t0.f[0], t1.f[0], t2.f[0], t3.f[0]}}; \
        r1 = (simd_vec_t){{t0.f[1], t1.f[1], t2.f[1], t3.f[1]}}; \
        r2 = (simd_vec_t){{t0.f[2], t1.f[2], t2.f[2], t3.f[2]}}; \
        r3 = (simd_vec_t){{t0.f[3], t1.f[3], t2.f[3], t3.f[3]}}; \
    } while (0)
#endif

#endif /* SIMD_H */
