#ifndef MATRIX_SIMD_H
#define MATRIX_SIMD_H

#include <float.h>
#include <immintrin.h>
#include <stdbool.h>
#include "vec.h"

// Matrix types with SIMD alignment
typedef struct __attribute__((aligned(16))) Mat3 {
    float m[3][3];  // Column-major storage
} Mat3;

typedef struct __attribute__((aligned(32))) Mat4 {
    union {
        float m[4][4];   // Column-major storage
        __m128 cols[4];  // SIMD columns
    };
} Mat4;

// Debugging
//================

/// @brief Prints a Mat3 matrix to the standard output.
/// @param m The Mat3 matrix to print.
static inline void mat3_print(Mat3 m) {
    for (int i = 0; i < 3; i++) {
        printf("[ ");
        for (int j = 0; j < 3; j++) {
            printf("%6.3f ", m.m[j][i]);
        }
        printf("]\n");
    }
}

/// @brief Prints a Mat4 matrix to the standard output.
/// @param m The Mat4 matrix to print.
static inline void mat4_print(Mat4 m) {
    for (int i = 0; i < 4; i++) {
        printf("[ ");
        for (int j = 0; j < 4; j++) {
            printf("%6.3f ", m.m[j][i]);
        }
        printf("]\n");
    }
}

// ======================
// Matrix Initialization
// ======================

/// @brief Creates a 3x3 identity matrix.
/// @return A Mat3 identity matrix.
static inline Mat3 mat3_identity(void) {
    Mat3 m = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    return m;
}

/// @brief Creates a 4x4 identity matrix using SIMD (SSE intrinsics).
///
/// @return A Mat4 identity matrix.
static inline Mat4 mat4_identity(void) {
    Mat4 m;
    m.cols[0] = _mm_setr_ps(1.0f, 0.0f, 0.0f, 0.0f);
    m.cols[1] = _mm_setr_ps(0.0f, 1.0f, 0.0f, 0.0f);
    m.cols[2] = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);
    m.cols[3] = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
    return m;
}

// ======================
// Matrix Operations
// ======================

/// @brief Multiplies two 3x3 matrices.
/// @param a The first Mat3 matrix.
/// @param b The second Mat3 matrix.
/// @return The result of the matrix multiplication a * b.
static inline Mat3 mat3_mul(Mat3 a, Mat3 b) {
    Mat3 result;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.m[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return result;
}

/// @brief Multiplies two 4x4 matrices using SIMD (SSE intrinsics).
/// @param a The first Mat4 matrix.
/// @param b The second Mat4 matrix.
/// @return The result of the matrix multiplication a * b.
static inline Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 result;
    __m128 b0 = b.cols[0];
    __m128 b1 = b.cols[1];
    __m128 b2 = b.cols[2];
    __m128 b3 = b.cols[3];

    for (int i = 0; i < 4; i++) {
        __m128 a0 = _mm_set1_ps(a.m[i][0]);
        __m128 a1 = _mm_set1_ps(a.m[i][1]);
        __m128 a2 = _mm_set1_ps(a.m[i][2]);
        __m128 a3 = _mm_set1_ps(a.m[i][3]);

        __m128 r = _mm_add_ps(_mm_add_ps(_mm_mul_ps(a0, b0), _mm_mul_ps(a1, b1)),
                              _mm_add_ps(_mm_mul_ps(a2, b2), _mm_mul_ps(a3, b3)));

        result.cols[i] = r;
    }
    return result;
}

// ======================
// Matrix-Vector Operations
// ======================

/// @brief Multiplies a 3x3 matrix by a 3D vector.
/// @param m The Mat3 matrix.
/// @param v The Vec3 vector.
/// @return The result of the matrix-vector multiplication m * v.
static inline Vec3 mat3_mul_vec3(Mat3 m, Vec3 v) {
    __m128 vec  = _mm_setr_ps(v.x, v.y, v.z, 0.0f);
    __m128 row0 = _mm_loadu_ps(&m.m[0][0]);
    __m128 row1 = _mm_loadu_ps(&m.m[1][0]);
    __m128 row2 = _mm_loadu_ps(&m.m[2][0]);

    __m128 result = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0)), row0),
                                          _mm_mul_ps(_mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1)), row1)),
                               _mm_mul_ps(_mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 2, 2, 2)), row2));

    return (Vec3){_mm_cvtss_f32(result),
                  _mm_cvtss_f32(_mm_shuffle_ps(result, result, _MM_SHUFFLE(1, 1, 1, 1))),
                  _mm_cvtss_f32(_mm_shuffle_ps(result, result, _MM_SHUFFLE(2, 2, 2, 2)))};
}

/// @brief Multiplies a 4x4 matrix by a 4D vector using SIMD (SSE intrinsics).
/// @param m The Mat4 matrix.
/// @param v The Vec4 vector.
/// @return The result of the matrix-vector multiplication m * v.
static inline Vec4 mat4_mul_vec4(Mat4 m, Vec4 v) {
    __m128 vec = _mm_loadu_ps((const float*)&v);
    __m128 x   = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 y   = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 z   = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 2, 2, 2));
    __m128 w   = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(3, 3, 3, 3));

    __m128 result = _mm_add_ps(_mm_add_ps(_mm_mul_ps(x, m.cols[0]), _mm_mul_ps(y, m.cols[1])),
                               _mm_add_ps(_mm_mul_ps(z, m.cols[2]), _mm_mul_ps(w, m.cols[3])));

    Vec4 res;
    _mm_storeu_ps((float*)&res, result);
    return res;
}

/// @brief Divides a 4x4 matrix by a scalar value using SIMD (SSE intrinsics).
/// @param a The Mat4 matrix.
/// @param b The scalar divisor.
/// @return The result of the matrix division a / b.
static inline Mat4 mat4_div(Mat4 a, float b) {
    Mat4 result;
    __m128 divisor = _mm_set1_ps(b);

    for (int i = 0; i < 4; i++) {
        result.cols[i] = _mm_div_ps(a.cols[i], divisor);
    }
    return result;
}

// ======================
// Transformation Matrices
// ======================

/// @brief Creates a 4x4 translation matrix.
/// @param translation A Vec3 representing the translation vector.
/// @return A Mat4 translation matrix.
static inline Mat4 mat4_translate(Vec3 translation) {
    Mat4 m    = mat4_identity();
    m.m[3][0] = translation.x;
    m.m[3][1] = translation.y;
    m.m[3][2] = translation.z;
    return m;
}

/// Creates a 4x4 scaling matrix using SIMD (SSE intrinsics).
///
/// Given a scaling vector `scale` with components (x, y, z), this function
/// returns a 4x4 matrix that scales points or vectors along the X, Y, and Z axes.
///
/// The resulting matrix `M` looks like this:
///
///     | x  0  0  0 |
///     | 0  y  0  0 |
///     | 0  0  z  0 |
///     | 0  0  0  1 |
///
/// This is useful for scaling transformations in 3D graphics.
///
/// @param scale A Vec3 with the scaling factors for each axis.
/// @return A Mat4 scale matrix.
static inline Mat4 mat4_scale(Vec3 scale) {
    Mat4 m = mat4_identity();  // Start with identity matrix

    // Multiply the X column of the identity matrix by scale.x
    m.cols[0] = _mm_mul_ps(m.cols[0], _mm_set1_ps(scale.x));

    // Multiply the Y column by scale.y
    m.cols[1] = _mm_mul_ps(m.cols[1], _mm_set1_ps(scale.y));

    // Multiply the Z column by scale.z
    m.cols[2] = _mm_mul_ps(m.cols[2], _mm_set1_ps(scale.z));

    return m;
}

/// @brief Creates a 4x4 rotation matrix for rotation around the X axis.
/// @param angle The rotation angle in radians.
/// @return A Mat4 rotation matrix.
static inline Mat4 mat4_rotate_x(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);

    Mat4 m    = mat4_identity();
    m.cols[1] = _mm_setr_ps(0.0f, c, s, 0.0f);
    m.cols[2] = _mm_setr_ps(0.0f, -s, c, 0.0f);
    return m;
}

/// @brief Creates a 4x4 rotation matrix for rotation around the Y axis.
/// @param angle The rotation angle in radians.
/// @return A Mat4 rotation matrix.
static inline Mat4 mat4_rotate_y(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);

    Mat4 m    = mat4_identity();
    m.cols[0] = _mm_setr_ps(c, 0.0f, -s, 0.0f);
    m.cols[2] = _mm_setr_ps(s, 0.0f, c, 0.0f);
    return m;
}

/// @brief Creates a 4x4 rotation matrix for rotation around the Z axis.
/// @param angle The rotation angle in radians.
/// @return A Mat4 rotation matrix.
static inline Mat4 mat4_rotate_z(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);

    Mat4 m    = mat4_identity();
    m.cols[0] = _mm_setr_ps(c, s, 0.0f, 0.0f);
    m.cols[1] = _mm_setr_ps(-s, c, 0.0f, 0.0f);
    return m;
}

/// @brief Creates a 4x4 rotation matrix for rotation around an arbitrary axis.
/// @param axis A Vec3 representing the rotation axis.
/// @param angle The rotation angle in radians.
/// @return A Mat4 rotation matrix.
static inline Mat4 mat4_rotate(Vec3 axis, float angle) {
    Vec3 a  = vec3_normalize(axis);
    float c = cosf(angle);
    float s = sinf(angle);
    float t = 1.0f - c;

    Mat4 m = mat4_identity();

    m.m[0][0] = t * a.x * a.x + c;
    m.m[0][1] = t * a.x * a.y - s * a.z;
    m.m[0][2] = t * a.x * a.z + s * a.y;

    m.m[1][0] = t * a.x * a.y + s * a.z;
    m.m[1][1] = t * a.y * a.y + c;
    m.m[1][2] = t * a.y * a.z - s * a.x;

    m.m[2][0] = t * a.x * a.z - s * a.y;
    m.m[2][1] = t * a.y * a.z + s * a.x;
    m.m[2][2] = t * a.z * a.z + c;

    return m;
}

// ======================
// Matrix Inversion
// ======================

/// @brief Transposes a 4x4 matrix using SIMD (SSE intrinsics).
/// @param m The Mat4 matrix to transpose.
/// @return The transposed Mat4 matrix.
static inline Mat4 mat4_transpose(Mat4 m) {
    __m128 tmp0 = _mm_shuffle_ps(m.cols[0], m.cols[1], 0x44);
    __m128 tmp2 = _mm_shuffle_ps(m.cols[0], m.cols[1], 0xEE);
    __m128 tmp1 = _mm_shuffle_ps(m.cols[2], m.cols[3], 0x44);
    __m128 tmp3 = _mm_shuffle_ps(m.cols[2], m.cols[3], 0xEE);

    Mat4 result;
    result.cols[0] = _mm_shuffle_ps(tmp0, tmp1, 0x88);
    result.cols[1] = _mm_shuffle_ps(tmp0, tmp1, 0xDD);
    result.cols[2] = _mm_shuffle_ps(tmp2, tmp3, 0x88);
    result.cols[3] = _mm_shuffle_ps(tmp2, tmp3, 0xDD);
    return result;
}

/// @brief Calculates the determinant of a 4x4 matrix using SIMD (SSE intrinsics).
/// @param m The Mat4 matrix.
/// @return The determinant of the matrix.
static inline float mat4_determinant(Mat4 m) {
    // Calculate sub-determinants for first row
    __m128 s0 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3]))))));

    __m128 s1 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s2 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s3 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[2][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 det = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]), s0), _mm_mul_ps(_mm_set1_ps(m.m[1][0]), s1)),
                            _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]), s2), _mm_mul_ps(_mm_set1_ps(m.m[3][0]), s3)));

    return _mm_cvtss_f32(det);
}

/// @brief Inverts a 4x4 matrix using SIMD (SSE intrinsics).
/// @param m The Mat4 matrix to invert.
/// @return The inverted Mat4 matrix. Returns an identity matrix if the input matrix is singular.
static inline Mat4 mat4_inverse(Mat4 m) {
    Mat4 inv;
    // Calculate sub-determinants for first row
    __m128 s0 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3]))))));

    __m128 s1 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s2 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s3 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[2][1]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 det = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]), s0), _mm_mul_ps(_mm_set1_ps(m.m[1][0]), s1)),
                            _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]), s2), _mm_mul_ps(_mm_set1_ps(m.m[3][0]), s3)));

    if (fabsf(_mm_cvtss_f32(det)) < FLT_EPSILON) {
        return mat4_identity();  // Fallback for singular matrix
    }

    __m128 inv_det = _mm_div_ps(_mm_set1_ps(1.0f), det);

    // Calculate all cofactors
    inv.cols[0] = _mm_mul_ps(s0, inv_det);
    inv.cols[1] = _mm_mul_ps(_mm_set1_ps(-1.0f), _mm_mul_ps(s1, inv_det));
    inv.cols[2] = _mm_mul_ps(s2, inv_det);
    inv.cols[3] = _mm_mul_ps(_mm_set1_ps(-1.0f), _mm_mul_ps(s3, inv_det));

    // Calculate remaining cofactors
    __m128 c0 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3]))))));

    __m128 c1 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c2 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c3 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c4 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][3]))))));

    __m128 c5 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c6 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c7 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][3])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][3])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[0][3])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[1][3])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[0][3]))))));

    // Store the cofactors in the inverse matrix
    inv.m[0][1] = -_mm_cvtss_f32(_mm_mul_ps(c0, inv_det));
    inv.m[1][1] = _mm_cvtss_f32(_mm_mul_ps(c1, inv_det));
    inv.m[2][1] = -_mm_cvtss_f32(_mm_mul_ps(c2, inv_det));
    inv.m[3][1] = _mm_cvtss_f32(_mm_mul_ps(c3, inv_det));

    inv.m[0][2] = _mm_cvtss_f32(_mm_mul_ps(c4, inv_det));
    inv.m[1][2] = -_mm_cvtss_f32(_mm_mul_ps(c5, inv_det));
    inv.m[2][2] = _mm_cvtss_f32(_mm_mul_ps(c6, inv_det));
    inv.m[3][2] = -_mm_cvtss_f32(_mm_mul_ps(c7, inv_det));

    // Calculate remaining cofactors for last row
    __m128 c8 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][2])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][2])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][2])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][2]))))));

    __m128 c9 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][2])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][2])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][2])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[0][2]))))));

    __m128 c10 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][2])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][2])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][2])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[1][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[0][2]))))));

    __m128 c11 =
        _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]),
                              _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][2])),
                                         _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][2])))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[0][2])))),
                              _mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                                         _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[1][2])),
                                                    _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[0][2]))))));

    // Store the remaining cofactors
    inv.m[0][3] = -_mm_cvtss_f32(_mm_mul_ps(c8, inv_det));
    inv.m[1][3] = _mm_cvtss_f32(_mm_mul_ps(c9, inv_det));
    inv.m[2][3] = -_mm_cvtss_f32(_mm_mul_ps(c10, inv_det));
    inv.m[3][3] = _mm_cvtss_f32(_mm_mul_ps(c11, inv_det));

    return inv;
}

// ======================
// Projection Matrices
// ======================

/// @brief Creates a 4x4 orthographic projection matrix.
/// @param left The left clipping plane coordinate.
/// @param right The right clipping plane coordinate.
/// @param bottom The bottom clipping plane coordinate.
/// @param top The top clipping plane coordinate.
/// @param near The near clipping plane distance.
/// @param far The far clipping plane distance.
/// @return A Mat4 orthographic projection matrix.
static inline Mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far) {
    Mat4 m = mat4_identity();

    __m128 diag = _mm_setr_ps(2.0f / (right - left), 2.0f / (top - bottom), -2.0f / (far - near), 1.0f);

    m.cols[0] = _mm_mul_ps(m.cols[0], _mm_shuffle_ps(diag, diag, _MM_SHUFFLE(0, 0, 0, 0)));
    m.cols[1] = _mm_mul_ps(m.cols[1], _mm_shuffle_ps(diag, diag, _MM_SHUFFLE(1, 1, 1, 1)));
    m.cols[2] = _mm_mul_ps(m.cols[2], _mm_shuffle_ps(diag, diag, _MM_SHUFFLE(2, 2, 2, 2)));

    m.m[3][0] = -(right + left) / (right - left);
    m.m[3][1] = -(top + bottom) / (top - bottom);
    m.m[3][2] = -(far + near) / (far - near);

    return m;
}

/// @brief Creates a 4x4 perspective projection matrix.
/// @param fov_radians The field of view angle in radians.
/// @param aspect The aspect ratio (width/height).
/// @param near The near clipping plane distance.
/// @param far The far clipping plane distance.
/// @return A Mat4 perspective projection matrix.
static inline Mat4 mat4_perspective(float fov_radians, float aspect, float near, float far) {
    float tan_half_fov = tanf(fov_radians / 2.0f);

    Mat4 m;
    m.cols[0] = _mm_setr_ps(1.0f / (aspect * tan_half_fov), 0.0f, 0.0f, 0.0f);
    m.cols[1] = _mm_setr_ps(0.0f, 1.0f / tan_half_fov, 0.0f, 0.0f);
    m.cols[2] = _mm_setr_ps(0.0f, 0.0f, -(far + near) / (far - near), -1.0f);
    m.cols[3] = _mm_setr_ps(0.0f, 0.0f, -(2.0f * far * near) / (far - near), 0.0f);

    return m;
}

// ======================
// View Matrix
// ======================

/// @brief Creates a 4x4 view matrix (camera matrix).
/// @param eye The Vec3 representing the camera position.
/// @param target The Vec3 representing the point the camera is looking at.
/// @param up The Vec3 representing the up direction.
/// @return A Mat4 view matrix.
static inline Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 z = vec3_normalize(vec3_sub(eye, target));
    Vec3 x = vec3_normalize(vec3_cross(up, z));
    Vec3 y = vec3_cross(z, x);

    Mat4 view;
    view.cols[0] = _mm_setr_ps(x.x, x.y, x.z, 0.0f);
    view.cols[1] = _mm_setr_ps(y.x, y.y, y.z, 0.0f);
    view.cols[2] = _mm_setr_ps(z.x, z.y, z.z, 0.0f);
    view.cols[3] = _mm_setr_ps(-vec3_dot(x, eye), -vec3_dot(y, eye), -vec3_dot(z, eye), 1.0f);

    return view;
}

#endif /* MATRIX_SIMD_H */
