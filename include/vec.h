#ifndef _VEC_LIB_H_
#define _VEC_LIB_H_

#include <float.h>
#include <immintrin.h>
#include <math.h>
#include <stdbool.h>

/*
==================================================
 Basic Scalar Vector Types (Unaligned)
==================================================
*/

/**
 * @struct Vec2
 * @brief A simple 2D float vector for basic math operations.
 */
typedef struct Vec2 {
    float x, y;
} Vec2;

/**
 * @struct Vec3
 * @brief A simple 3D float vector for spatial computations.
 */
typedef struct Vec3 {
    float x, y, z;
} Vec3;

/**
 * @struct Vec4
 * @brief A simple 4D float vector for spatial computations.
 */
typedef struct Vec4 {
    float x, y, z, w;
} Vec4;

/**
 * @union SimdVec2
 * @brief A 2D float vector backed by a 128-bit SIMD register (SSE).
 * Can be accessed as:
 * - `__m128` for SIMD operations
 * - `float f32[4]` for array-like access
 * - `x, y` for named component access
 *
 * The memory is aligned to 16 bytes for SSE compatibility.
 */
typedef union __attribute__((aligned(16))) SimdVec2 {
    __m128 m128;
    float f32[4];
    struct {
        float x, y;
    };
} SimdVec2;

/**
 * @union SimdVec3
 * @brief A 3D float vector backed by a 256-bit SIMD register (AVX).
 * Can be accessed as:
 * - `__m256` for AVX-wide operations
 * - `__m128 m128[2]` for splitting into low/high halves
 * - `float f32[8]` for indexed access
 * - `x, y, z, w` for component-level access
 *
 * The memory is aligned to 32 bytes for AVX compatibility.
 */
typedef union __attribute__((aligned(32))) SimdVec3 {
    __m256 m256;
    __m128 m128[2];
    float f32[8];
    struct {
        float x, y, z, w;
    };
} SimdVec3;

/**
 * @union SimdVec4
 * @brief A 4D float vector backed by a 128-bit SIMD register (SSE).
 * Can be accessed as:
 * - `__m128` for SIMD operations
 * - `float f32[4]` for array-like access
 * - `x, y, z, w` for component-level access
 *
 * The memory is aligned to 16 bytes for SSE compatibility.
 */
typedef union __attribute__((aligned(16))) SimdVec4 {
    __m128 m128;
    float f32[4];
    struct {
        float x, y, z, w;
    };
} SimdVec4;

// ===============
// DEBUG
//================
#include <stdio.h>

/**
 * @brief Prints a Vec2 to stdout
 * @param v The vector to print
 * @param name Optional name to display (can be NULL)
 */
void vec2_print(const Vec2 v, const char* name) {
    if (name) {
        printf("%s: ", name);
    }
    printf("Vec2(%f, %f)\n", v.x, v.y);
}

/**
 * @brief Prints a Vec3 to stdout
 * @param v The vector to print
 * @param name Optional name to display (can be NULL)
 */
void vec3_print(const Vec3 v, const char* name) {
    if (name) {
        printf("%s: ", name);
    }
    printf("Vec3(%f, %f, %f)\n", v.x, v.y, v.z);
}

/**
 * @brief Prints a Vec4 to stdout
 * @param v The vector to print
 * @param name Optional name to display (can be NULL)
 */
void vec4_print(const Vec4 v, const char* name) {
    if (name) {
        printf("%s: ", name);
    }
    printf("Vec4(%f, %f, %f, %f)\n", v.x, v.y, v.z, v.w);
}

// ======================
// Vec2 Operations
// ======================

/**
 * @brief Adds two Vec2 vectors component-wise.
 */
static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

/**
 * @brief Subtracts two Vec2 vectors component-wise.
 */
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2){a.x - b.x, a.y - b.y};
}

/**
 * @brief Multiplies a Vec2 vector by a scalar.
 */
static inline Vec2 vec2_mul(Vec2 a, float s) {
    return (Vec2){a.x * s, a.y * s};
}

/**
 * @brief Divides a Vec2 vector by a scalar.
 */
static inline Vec2 vec2_div(Vec2 a, float s) {
    return vec2_mul(a, 1.0f / s);
}

/**
 * @brief Computes the dot product of two Vec2 vectors.
 */
static inline float vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

/**
 * @brief Returns the Euclidean length of the Vec2.
 */
static inline float vec2_length(Vec2 v) {
    return sqrtf(vec2_dot(v, v));
}

/**
 * @brief Returns a normalized (unit length) version of the Vec2.
 */
static inline Vec2 vec2_normalize(Vec2 v) {
    return vec2_div(v, vec2_length(v));
}

/**
 * @brief Rotates vec2 vector on an angle (in radians).
 * Returns the new angle.
 */
static inline Vec2 vec2_rotate(Vec2 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    return (Vec2){v.x * cos_angle - v.y * sin_angle, v.x * sin_angle + v.y * cos_angle};
}

/**
 * @brief Performs linear interpolation between two Vec2 vectors.
 *
 * @param a The start vector.
 * @param b The end vector.
 * @param t The interpolation factor (typically between 0 and 1).
 * @return Vec2 The interpolated vector.
 */
static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
    return (Vec2){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

/**
 * @brief Returns a vector that is perpendicular (rotated 90 degrees counter-clockwise) to the input Vec2.
 *
 * @param v The input vector.
 * @return Vec2 The perpendicular vector.
 */
static inline Vec2 vec2_perpendicular(Vec2 v) {
    return (Vec2){-v.y, v.x};  // 90 degree rotation
}

/**
 * @brief Computes the Euclidean distance between two Vec2 vectors.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @return float The distance between vectors.
 */
static inline float vec2_distance(Vec2 a, Vec2 b) {
    return vec2_length(vec2_sub(a, b));
}

// ======================
// Vec3 Operations
// ======================

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}
static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
static inline Vec3 vec3_mul(Vec3 a, float s) {
    return (Vec3){a.x * s, a.y * s, a.z * s};
}
static inline Vec3 vec3_div(Vec3 a, float s) {
    return vec3_mul(a, 1.0f / s);
}
static inline float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static inline float vec3_length(Vec3 v) {
    return sqrtf(vec3_dot(v, v));
}
static inline Vec3 vec3_normalize(Vec3 v) {
    return vec3_div(v, vec3_length(v));
}

static inline Vec3 vec3_rotate_x(Vec3 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    return (Vec3){v.x, v.y * cos_angle - v.z * sin_angle, v.y * sin_angle + v.z * cos_angle};
}

static inline Vec3 vec3_rotate_y(Vec3 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    return (Vec3){v.x * cos_angle + v.z * sin_angle, v.y, -v.x * sin_angle + v.z * cos_angle};
}

static inline Vec3 vec3_rotate_z(Vec3 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    return (Vec3){v.x * cos_angle - v.y * sin_angle, v.x * sin_angle + v.y * cos_angle, v.z};
}

static inline Vec3 vec3_rotate(Vec3 v, Vec3 axis, float angle) {
    // Rodrigues' rotation formula
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    Vec3 norm_axis  = vec3_normalize(axis);

    Vec3 term1 = vec3_mul(v, cos_angle);
    Vec3 term2 = vec3_mul(vec3_cross(norm_axis, v), sin_angle);
    Vec3 term3 = vec3_mul(norm_axis, vec3_dot(norm_axis, v) * (1 - cos_angle));

    return vec3_add(vec3_add(term1, term2), term3);
}

/**
 * @brief Performs linear interpolation between two Vec3 vectors.
 *
 * @param a The start vector.
 * @param b The end vector.
 * @param t The interpolation factor (typically between 0 and 1).
 * @return Vec3 The interpolated vector.
 */
static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
    return (Vec3){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

/**
 * @brief Computes the Euclidean distance between two Vec3 vectors.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @return float The distance between vectors.
 */
static inline float vec3_distance(Vec3 a, Vec3 b) {
    return vec3_length(vec3_sub(a, b));
}

/**
 * @brief Reflects an incident vector about a given normal.
 *
 * @param incident The incident vector.
 * @param normal The normal vector.
 * @return Vec3 The reflected vector.
 */
static inline Vec3 vec3_reflect(Vec3 incident, Vec3 normal) {
    return vec3_sub(incident, vec3_mul(normal, 2.0f * vec3_dot(incident, normal)));
}

// ======================
// Vec4 Operations
// ======================
static inline Vec4 vec4_add(Vec4 a, Vec4 b) {
    return (Vec4){a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

static inline Vec4 vec4_sub(Vec4 a, Vec4 b) {
    return (Vec4){a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

static inline Vec4 vec4_mul(Vec4 a, float s) {
    return (Vec4){a.x * s, a.y * s, a.z * s, a.w * s};
}

static inline Vec4 vec4_div(Vec4 a, float s) {
    return vec4_mul(a, 1.0f / s);
}

static inline float vec4_dot(Vec4 a, Vec4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static inline float vec4_length(Vec4 v) {
    return sqrtf(vec4_dot(v, v));
}

static inline Vec4 vec4_normalize(Vec4 v) {
    return vec4_div(v, vec4_length(v));
}

/**
 * @brief Performs linear interpolation between two Vec4 vectors.
 *
 * @param a The start vector.
 * @param b The end vector.
 * @param t The interpolation factor (typically between 0 and 1).
 * @return Vec4 The interpolated vector.
 */
static inline Vec4 vec4_lerp(Vec4 a, Vec4 b, float t) {
    return (Vec4){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

/**
 * @brief Computes the Euclidean distance between two Vec4 vectors.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @return float The distance between vectors.
 */
static inline float vec4_distance(Vec4 a, Vec4 b) {
    return vec4_length(vec4_sub(a, b));
}

// ======================
// SIMD Operations
// ======================

/**
 * @brief Loads a Vec2 into a SimdVec2 (SIMD register).
 *
 * @param v The Vec2 to load.
 * @return SimdVec2 The SIMD representation of the vector.
 */
static inline SimdVec2 simd_vec2_load(Vec2 v) {
    SimdVec2 result;
    result.m128 = _mm_set_ps(0.0f, 0.0f, v.y, v.x);
    return result;
}

/**
 * @brief Stores the first two components of a SimdVec2 into a Vec2.
 *
 * @param v The SimdVec2 to store from.
 * @return Vec2 The regular vector representation.
 */
static inline Vec2 simd_vec2_store(SimdVec2 v) {
    return (Vec2){v.f32[0], v.f32[1]};
}

/**
 * @brief Rotates a SimdVec2 by a given angle using SIMD operations.
 *
 * @param v The input vector.
 * @param angle The angle in radians.
 * @return SimdVec2 The rotated vector.
 */
static inline SimdVec2 simd_vec2_rotate(SimdVec2 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);

    // Create SIMD vectors for cos and sin
    __m128 cos_vec = _mm_set1_ps(cos_angle);
    __m128 sin_vec = _mm_set1_ps(sin_angle);

    // Extract components
    __m128 x = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 y = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(1, 1, 1, 1));

    // Compute rotated components
    __m128 x_rot = _mm_sub_ps(_mm_mul_ps(x, cos_vec), _mm_mul_ps(y, sin_vec));
    __m128 y_rot = _mm_add_ps(_mm_mul_ps(x, sin_vec), _mm_mul_ps(y, cos_vec));

    // Combine results
    SimdVec2 result;
    result.m128 = _mm_unpacklo_ps(x_rot, y_rot);
    return result;
}

/**
 * @brief Adds two SimdVec2 vectors using SIMD addition.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @return SimdVec2 The result of the addition.
 */
static inline SimdVec2 simd_vec2_add(SimdVec2 a, SimdVec2 b) {
    SimdVec2 result;
    result.m128 = _mm_add_ps(a.m128, b.m128);  // SIMD component-wise addition
    return result;
}

/**
 * @brief Subtracts two SimdVec2 vectors using SIMD subtraction.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @return SimdVec2 The result of the subtraction.
 */
static inline SimdVec2 simd_vec2_sub(SimdVec2 a, SimdVec2 b) {
    SimdVec2 result;
    result.m128 = _mm_sub_ps(a.m128, b.m128);
    return result;
}

/**
 * @brief Multiplies a SimdVec2 by a scalar using SIMD operations.
 *
 * @param a The vector to scale.
 * @param scalar The scalar value.
 * @return SimdVec2 The scaled vector.
 */
static inline SimdVec2 simd_vec2_mul(SimdVec2 a, float scalar) {
    SimdVec2 result;
    __m128 s    = _mm_set1_ps(scalar);  // Broadcast scalar across all lanes
    result.m128 = _mm_mul_ps(a.m128, s);
    return result;
}

/**
 * @brief Computes the dot product of two SimdVec2 vectors using SIMD multiplication.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @return float The dot product.
 */
static inline float simd_vec2_dot(SimdVec2 a, SimdVec2 b) {
    // Perform element-wise multiplication of the x and y components
    __m128 mul = _mm_mul_ps(a.m128, b.m128);  // Multiply the entire 128-bit register (containing 4 floats)

    // Store the result in a float array to sum up the first two components
    float result[4];
    _mm_storeu_ps(result, mul);  // Store the result of the multiplication

    // Only sum the x and y components (index 0 and 1)
    return result[0] + result[1];  // Dot product is just x1 * x2 + y1 * y2
}

// ============= vec3 ===============

static inline SimdVec3 simd_vec3_load(Vec3 v) {
    SimdVec3 result;
    result.m256 = _mm256_set_ps(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, v.z, v.y, v.x);
    return result;
}

static inline Vec3 simd_vec3_store(SimdVec3 v) {
    return (Vec3){v.f32[0], v.f32[1], v.f32[2]};
}

/**
 * @brief Performs SIMD addition of two SimdVec3 vectors using `_mm256_add_ps`.
 */
static inline SimdVec3 simd_vec3_add(SimdVec3 a, SimdVec3 b) {
    SimdVec3 result;
    result.m256 = _mm256_add_ps(a.m256, b.m256);
    return result;
}

/**
 * @brief Performs SIMD subtraction of two SimdVec3 vectors using `_mm256_sub_ps`.
 */
static inline SimdVec3 simd_vec3_sub(SimdVec3 a, SimdVec3 b) {
    SimdVec3 result;
    result.m256 = _mm256_sub_ps(a.m256, b.m256);
    return result;
}

/**
 * @brief Multiplies a SimdVec3 by a scalar using `_mm256_mul_ps`.
 */
static inline SimdVec3 simd_vec3_mul(SimdVec3 a, float scalar) {
    SimdVec3 result;
    __m256 s    = _mm256_set1_ps(scalar);  // Broadcast scalar
    result.m256 = _mm256_mul_ps(a.m256, s);
    return result;
}

/**
 * @brief Computes dot product of two SimdVec3 using horizontal add.
 */
static inline float simd_vec3_dot(SimdVec3 a, SimdVec3 b) {
    __m256 mul  = _mm256_mul_ps(a.m256, b.m256);
    __m128 low  = _mm256_extractf128_ps(mul, 0);
    __m128 high = _mm256_extractf128_ps(mul, 1);
    __m128 sum  = _mm_add_ps(low, high);
    sum         = _mm_hadd_ps(sum, sum);
    sum         = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

/**
 * @brief Computes cross product of two SimdVec3 using SIMD shuffle and math.
 */
static inline SimdVec3 simd_vec3_cross(SimdVec3 a, SimdVec3 b) {
    __m256 a_yzx = _mm256_shuffle_ps(a.m256, a.m256, _MM_SHUFFLE(3, 0, 2, 1));
    __m256 b_zxy = _mm256_shuffle_ps(b.m256, b.m256, _MM_SHUFFLE(3, 1, 0, 2));
    __m256 a_zxy = _mm256_shuffle_ps(a.m256, a.m256, _MM_SHUFFLE(3, 1, 0, 2));
    __m256 b_yzx = _mm256_shuffle_ps(b.m256, b.m256, _MM_SHUFFLE(3, 0, 2, 1));
    SimdVec3 result;
    result.m256 = _mm256_sub_ps(_mm256_mul_ps(a_yzx, b_zxy), _mm256_mul_ps(a_zxy, b_yzx));
    return result;
}

/**
 * @brief Rotates a SimdVec3 vector around a given axis by a specified angle using Rodrigues' rotation formula.
 *
 * @param v The vector to rotate.
 * @param axis The axis to rotate around (will be normalized).
 * @param angle The angle of rotation in radians.
 * @return SimdVec3 The rotated vector.
 */
static inline SimdVec3 simd_vec3_rotate(SimdVec3 v, SimdVec3 axis, float angle) {
    // Compute cos and sin of angle
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);

    // Create SIMD vectors for cos and sin
    __m256 cos_vec = _mm256_set1_ps(cos_angle);
    __m256 sin_vec = _mm256_set1_ps(sin_angle);

    // Normalize the axis vector
    __m256 axis_len  = _mm256_sqrt_ps(_mm256_dp_ps(axis.m256, axis.m256, 0xFF));
    __m256 norm_axis = _mm256_div_ps(axis.m256, axis_len);

    // Compute dot product (v · axis)
    __m256 dot = _mm256_dp_ps(v.m256, norm_axis, 0xFF);

    // Rodrigues' rotation formula:
    // v_rot = v*cosθ + (axis × v)*sinθ + axis*(axis·v)(1 - cosθ)

    // Term 1: v*cosθ
    __m256 term1 = _mm256_mul_ps(v.m256, cos_vec);

    // Term 2: (axis × v)*sinθ
    __m256 cross = _mm256_sub_ps(_mm256_mul_ps(_mm256_shuffle_ps(norm_axis, norm_axis, _MM_SHUFFLE(3, 0, 2, 1)),
                                               _mm256_shuffle_ps(v.m256, v.m256, _MM_SHUFFLE(3, 1, 0, 2))),
                                 _mm256_mul_ps(_mm256_shuffle_ps(norm_axis, norm_axis, _MM_SHUFFLE(3, 1, 0, 2)),
                                               _mm256_shuffle_ps(v.m256, v.m256, _MM_SHUFFLE(3, 0, 2, 1))));
    __m256 term2 = _mm256_mul_ps(cross, sin_vec);

    // Term 3: axis*(axis·v)(1 - cosθ)
    __m256 one_minus_cos = _mm256_sub_ps(_mm256_set1_ps(1.0f), cos_vec);
    __m256 term3         = _mm256_mul_ps(norm_axis, _mm256_mul_ps(dot, one_minus_cos));

    // Combine all terms
    SimdVec3 result;
    result.m256 = _mm256_add_ps(_mm256_add_ps(term1, term2), term3);
    return result;
}

// ============= vec4 ===============

static inline SimdVec4 simd_vec4_load(Vec4 v) {
    SimdVec4 result;
    result.m128 = _mm_set_ps(v.w, v.z, v.y, v.x);
    return result;
}

static inline Vec4 simd_vec4_store(SimdVec4 v) {
    return (Vec4){v.x, v.y, v.z, v.w};
}

/**
 * @brief Performs SIMD addition of two SimdVec4 vectors using `_mm_add_ps`.
 */
static inline SimdVec4 simd_vec4_add(SimdVec4 a, SimdVec4 b) {
    SimdVec4 result;
    result.m128 = _mm_add_ps(a.m128, b.m128);
    return result;
}

/**
 * @brief Performs SIMD subtraction of two SimdVec4 vectors using `_mm_sub_ps`.
 */
static inline SimdVec4 simd_vec4_sub(SimdVec4 a, SimdVec4 b) {
    SimdVec4 result;
    result.m128 = _mm_sub_ps(a.m128, b.m128);
    return result;
}

/**
 * @brief Multiplies a SimdVec4 by a scalar using `_mm_mul_ps`.
 */
static inline SimdVec4 simd_vec4_mul(SimdVec4 a, float scalar) {
    SimdVec4 result;
    __m128 s    = _mm_set1_ps(scalar);  // Broadcast scalar
    result.m128 = _mm_mul_ps(a.m128, s);
    return result;
}

/**
 * @brief Computes dot product of two SimdVec4 using horizontal add.
 */
static inline float simd_vec4_dot(SimdVec4 a, SimdVec4 b) {
    __m128 mul = _mm_mul_ps(a.m128, b.m128);
    mul        = _mm_hadd_ps(mul, mul);
    mul        = _mm_hadd_ps(mul, mul);
    return _mm_cvtss_f32(mul);
}

/**
 * @brief Normalizes SimdVec4 using SIMD.
 */
static inline SimdVec4 simd_vec4_normalize(SimdVec4 v) {
    float len = sqrtf(simd_vec4_dot(v, v));
    return simd_vec4_mul(v, 1.0f / len);
}

/**
 * @brief Rotates a SimdVec4 around X axis
 */
static inline SimdVec4 simd_vec4_rotate_x(SimdVec4 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);

    __m128 cos_vec = _mm_set1_ps(cos_angle);
    __m128 sin_vec = _mm_set1_ps(sin_angle);

    // Extract y and z components
    __m128 y = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 z = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(2, 2, 2, 2));

    // Perform rotation: y' = y*cos - z*sin; z' = y*sin + z*cos
    __m128 y_rot = _mm_sub_ps(_mm_mul_ps(y, cos_vec), _mm_mul_ps(z, sin_vec));
    __m128 z_rot = _mm_add_ps(_mm_mul_ps(y, sin_vec), _mm_mul_ps(z, cos_vec));

    // Create masks for y and z components
    const __m128 y_mask = _mm_castsi128_ps(_mm_set_epi32(0, 0, ~0, 0));
    const __m128 z_mask = _mm_castsi128_ps(_mm_set_epi32(0, ~0, 0, 0));

    // Combine results
    SimdVec4 result;
    result.m128 = _mm_blendv_ps(v.m128, y_rot, y_mask);
    result.m128 = _mm_blendv_ps(result.m128, z_rot, z_mask);
    return result;
}

/**
 * @brief Rotates a SimdVec4 around Y axis
 */
static inline SimdVec4 simd_vec4_rotate_y(SimdVec4 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);

    __m128 cos_vec = _mm_set1_ps(cos_angle);
    __m128 sin_vec = _mm_set1_ps(sin_angle);

    // Extract x and z components
    __m128 x = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 z = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(2, 2, 2, 2));

    // Perform rotation: x' = x*cos + z*sin; z' = z*cos - x*sin
    __m128 x_rot = _mm_add_ps(_mm_mul_ps(x, cos_vec), _mm_mul_ps(z, sin_vec));
    __m128 z_rot = _mm_sub_ps(_mm_mul_ps(z, cos_vec), _mm_mul_ps(x, sin_vec));

    // Create masks for x and z components
    const __m128 x_mask = _mm_castsi128_ps(_mm_set_epi32(0, 0, 0, ~0));
    const __m128 z_mask = _mm_castsi128_ps(_mm_set_epi32(0, ~0, 0, 0));

    // Combine results
    SimdVec4 result;
    result.m128 = _mm_blendv_ps(v.m128, x_rot, x_mask);
    result.m128 = _mm_blendv_ps(result.m128, z_rot, z_mask);
    return result;
}

/**
 * @brief Rotates a SimdVec4 around Z axis
 */
static inline SimdVec4 simd_vec4_rotate_z(SimdVec4 v, float angle) {
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);

    __m128 cos_vec = _mm_set1_ps(cos_angle);
    __m128 sin_vec = _mm_set1_ps(sin_angle);

    // Extract x and y components
    __m128 x = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 y = _mm_shuffle_ps(v.m128, v.m128, _MM_SHUFFLE(1, 1, 1, 1));

    // Perform rotation: x' = x*cos - y*sin; y' = x*sin + y*cos
    __m128 x_rot = _mm_sub_ps(_mm_mul_ps(x, cos_vec), _mm_mul_ps(y, sin_vec));
    __m128 y_rot = _mm_add_ps(_mm_mul_ps(x, sin_vec), _mm_mul_ps(y, cos_vec));

    // Create masks for x and y components
    const __m128 x_mask = _mm_castsi128_ps(_mm_set_epi32(0, 0, 0, ~0));
    const __m128 y_mask = _mm_castsi128_ps(_mm_set_epi32(0, 0, ~0, 0));

    // Combine results
    SimdVec4 result;
    result.m128 = _mm_blendv_ps(v.m128, x_rot, x_mask);
    result.m128 = _mm_blendv_ps(result.m128, y_rot, y_mask);
    return result;
}

// ======================
// Utility Functions
// ======================

/**
 * @brief Compares two Vec2 vectors for approximate equality within a given epsilon.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @param epsilon The tolerance for equality.
 * @return true If the vectors are approximately equal.
 */
static inline bool vec2_equals(Vec2 a, Vec2 b, float epsilon) {
    return fabsf(a.x - b.x) < epsilon && fabsf(a.y - b.y) < epsilon;
}

/**
 * @brief Compares two Vec3 vectors for approximate equality within a given epsilon.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @param epsilon The tolerance for equality.
 * @return true If the vectors are approximately equal.
 * @return false Otherwise.
 */
static inline bool vec3_equals(Vec3 a, Vec3 b, float epsilon) {
    return fabsf(a.x - b.x) < epsilon && fabsf(a.y - b.y) < epsilon && fabsf(a.z - b.z) < epsilon;
}

/**
 * @brief Projects vector a onto vector b.
 *
 * @param a The vector to project.
 * @param b The vector to project onto.
 * @return Vec3 The projection of a onto b. If b has near-zero length, returns a zero vector.
 */
static inline Vec3 vec3_project(Vec3 a, Vec3 b) {
    float b_len_sq = vec3_dot(b, b);
    if (b_len_sq < FLT_EPSILON)
        return (Vec3){0};
    float scale = vec3_dot(a, b) / b_len_sq;
    return vec3_mul(b, scale);
}

/**
 * @brief Rejects vector a from vector b (i.e., computes the component of a perpendicular to b).
 *
 * @param a The vector to reject.
 * @param b The vector to reject from.
 * @return Vec3 The rejection of a from b.
 */
static inline Vec3 vec3_reject(Vec3 a, Vec3 b) {
    return vec3_sub(a, vec3_project(a, b));
}

/**
 * @brief Compares two Vec4 vectors for approximate equality within a given epsilon.
 *
 * @param a The first vector.
 * @param b The second vector.
 * @param epsilon The tolerance for equality.
 * @return true If the vectors are approximately equal.
 * @return false Otherwise.
 */
static inline bool vec4_equals(Vec4 a, Vec4 b, float epsilon) {
    return fabsf(a.x - b.x) < epsilon && fabsf(a.y - b.y) < epsilon && fabsf(a.z - b.z) < epsilon &&
           fabsf(a.w - b.w) < epsilon;
}

#endif /* _VEC_LIB_H_ */
