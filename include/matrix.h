/**
 * @file matrix.h
 * @brief Matrix operations and mathematical utilities.
 */

#ifndef MATRIX_H
#define MATRIX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vec.h"

#include <float.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * @struct Mat3
 * @brief A 3x3 matrix for 2D transformations and 3D rotations.
 *
 * Matrix elements are stored in column-major order (OpenGL style):
 * | m[0] m[3] m[6] |
 * | m[1] m[4] m[7] |
 * | m[2] m[5] m[8] |
 */
typedef struct ALIGN(16) Mat3 {
    float m[3][3];  // Column-major storage as m[col][row]
} Mat3;

/**
 * @struct Mat4
 * @brief A 4x4 matrix for 3D transformations.
 *
 * Matrix elements are stored in column-major order (OpenGL style).
 * The union allows accessing data as raw floats or SIMD registers.
 */
typedef struct ALIGN(16) Mat4 {
    union {
        float m[4][4];       // Column-major storage as m[col][row]
        simd_vec_t cols[4];  // SIMD columns
    };
} Mat4;

// Debugging
//================

/**
 * Creates a Mat3 with elements stored in column-major order.
 * @param m00, m01, ... element values in row-major order (as visualised).
 */
static inline Mat3 mat3_new_column_major(float m00, float m01, float m02, float m10, float m11, float m12, float m20,
                                         float m21, float m22) {
    Mat3 mat;
    mat.m[0][0] = m00;
    mat.m[1][0] = m01;
    mat.m[2][0] = m02;
    mat.m[0][1] = m10;
    mat.m[1][1] = m11;
    mat.m[2][1] = m12;
    mat.m[0][2] = m20;
    mat.m[1][2] = m21;
    mat.m[2][2] = m22;
    return mat;
}

// Helper function to create a Mat4 with column-major storage
static inline Mat4 mat4_new_column_major(float m00, float m01, float m02, float m03, float m10, float m11, float m12,
                                         float m13, float m20, float m21, float m22, float m23, float m30, float m31,
                                         float m32, float m33) {
    Mat4 mat;
    mat.m[0][0] = m00;
    mat.m[1][0] = m01;
    mat.m[2][0] = m02;
    mat.m[3][0] = m03;
    mat.m[0][1] = m10;
    mat.m[1][1] = m11;
    mat.m[2][1] = m12;
    mat.m[3][1] = m13;
    mat.m[0][2] = m20;
    mat.m[1][2] = m21;
    mat.m[2][2] = m22;
    mat.m[3][2] = m23;
    mat.m[0][3] = m30;
    mat.m[1][3] = m31;
    mat.m[2][3] = m32;
    mat.m[3][3] = m33;
    return mat;
}

static inline void mat3_print(Mat3 mat, const char* name) {
    printf("%s = \n", name);
    for (int i = 0; i < 3; i++) {
        printf("  [");
        for (int j = 0; j < 3; j++) {
            printf("%8.4f", mat.m[j][i]);
            if (j < 2) printf(", ");
        }
        printf("]\n");
    }
    printf("\n");
}

static inline void mat4_print(Mat4 m, const char* name) {
    printf("%s = \n", name);
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

static inline Mat3 mat3_identity(void) { return (Mat3){{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}; }

static inline bool mat3_equal(Mat3 a, Mat3 b) {
    static float EPSILON = 1e-6f;
    const float* pa = &a.m[0][0];
    const float* pb = &b.m[0][0];

    // Compare first 4 floats (Col 0 and part of Col 1)
    if (!simd_equals_eps(simd_load(pa), simd_load(pb), EPSILON)) return false;

    // Compare next 4 floats
    if (!simd_equals_eps(simd_load(pa + 4), simd_load(pb + 4), EPSILON)) return false;

    // Compare final element (9th float)
    float da = pa[8] - pb[8];
    return fabsf(da) <= EPSILON;
}

static inline bool mat4_equal(Mat4 a, Mat4 b) {
    static float EPSILON = 1e-6f;
    // Unroll comparison for all 4 columns
    return simd_equals_eps(a.cols[0], b.cols[0], EPSILON) && simd_equals_eps(a.cols[1], b.cols[1], EPSILON) &&
           simd_equals_eps(a.cols[2], b.cols[2], EPSILON) && simd_equals_eps(a.cols[3], b.cols[3], EPSILON);
}

static inline Mat3 mat3_diag(Mat3 m) {
    return (Mat3){{{m.m[0][0], 0.0f, 0.0f}, {0.0f, m.m[1][1], 0.0f}, {0.0f, 0.0f, m.m[2][2]}}};
}

static inline Mat4 mat4_identity(void) {
    Mat4 m;
    m.cols[0] = simd_set(1.0f, 0.0f, 0.0f, 0.0f);
    m.cols[1] = simd_set(0.0f, 1.0f, 0.0f, 0.0f);
    m.cols[2] = simd_set(0.0f, 0.0f, 1.0f, 0.0f);
    m.cols[3] = simd_set(0.0f, 0.0f, 0.0f, 1.0f);
    return m;
}

static inline Mat4 mat4_diag(Mat4 m) {
    // Extract diagonals and reform matrix
    // Note: To be purely SIMD efficient we could shuffle, but scalar fallback is clean here.
    Mat4 r;
    r.cols[0] = simd_set(simd_get_x(m.cols[0]), 0, 0, 0);  // X from Col0

    // For other lanes, scalar access is often cleaner than complex shuffles without AVX2
    r.cols[1] = simd_set(0, m.m[1][1], 0, 0);
    r.cols[2] = simd_set(0, 0, m.m[2][2], 0);
    r.cols[3] = simd_set(0, 0, 0, m.m[3][3]);
    return r;
}

// ======================
// Matrix Operations
// ======================

static inline Mat3 mat3_mul(Mat3 a, Mat3 b) {
    Mat3 result;
    // Standard cubic complexity multiplication
    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 3; row++) {
            result.m[col][row] = 0.0f;
            for (int k = 0; k < 3; k++) {
                result.m[col][row] += a.m[k][row] * b.m[col][k];
            }
        }
    }
    return result;
}

static inline Mat3 mat3_add_scalar(Mat3 m, float scalar) {
    Mat3 result;
    simd_vec_t s_vec = simd_set1(scalar);

    // Vectorized add for first 8 elements
    const float* src = &m.m[0][0];
    float* dst = &result.m[0][0];

    simd_store(dst, simd_add(simd_load(src), s_vec));
    simd_store(dst + 4, simd_add(simd_load(src + 4), s_vec));

    // Tail
    dst[8] = src[8] + scalar;
    return result;
}

static inline Mat3 mat3_add(Mat3 a, Mat3 b) {
    Mat3 result;
    const float* pa = &a.m[0][0];
    const float* pb = &b.m[0][0];
    float* pr = &result.m[0][0];

    // Batch 1 (floats 0-3)
    simd_store(pr, simd_add(simd_load(pa), simd_load(pb)));

    // Batch 2 (floats 4-7)
    simd_store(pr + 4, simd_add(simd_load(pa + 4), simd_load(pb + 4)));

    // Tail
    pr[8] = pa[8] + pb[8];
    return result;
}

static inline Mat3 mat3_scalar_mul(Mat3 m, float scalar) {
    Mat3 result;
    simd_vec_t s_vec = simd_set1(scalar);

    const float* src = &m.m[0][0];
    float* dst = &result.m[0][0];

    simd_store(dst, simd_mul(simd_load(src), s_vec));
    simd_store(dst + 4, simd_mul(simd_load(src + 4), s_vec));
    dst[8] = src[8] * scalar;

    return result;
}

static inline float mat3_determinant(Mat3 m) {
    // Sarrus rule / Co-factor expansion
    return m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[2][1] * m.m[1][2]) -
           m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[2][0] * m.m[1][2]) +
           m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[2][0] * m.m[1][1]);
}

// Matrix LU, Forward Sub, Backward Sub, Exp kept scalar as they are algorithmically complex
// to vectorize without AVX scatter/gather, and usually called infrequently.
static inline bool mat3_lu(Mat3 A, Mat3* L, Mat3* U, Mat3* P) {
    const float tolerance = 1e-6f;
    *U = A;
    // Identity L and P
    *L = mat3_identity();
    *P = mat3_identity();

    for (int k = 0; k < 3; ++k) {
        // Pivot
        int pivot_row = k;
        float max_val = fabsf(U->m[k][k]);
        for (int i = k + 1; i < 3; ++i) {
            float val = fabsf(U->m[k][i]);
            if (val > max_val) {
                max_val = val;
                pivot_row = i;
            }
        }
        if (max_val < tolerance) return false;

        // Swap
        if (pivot_row != k) {
            for (int j = 0; j < 3; ++j) {
                float tmp;
                tmp = U->m[j][k];
                U->m[j][k] = U->m[j][pivot_row];
                U->m[j][pivot_row] = tmp;
                tmp = P->m[j][k];
                P->m[j][k] = P->m[j][pivot_row];
                P->m[j][pivot_row] = tmp;
                if (j < k) {
                    tmp = L->m[j][k];
                    L->m[j][k] = L->m[j][pivot_row];
                    L->m[j][pivot_row] = tmp;
                }
            }
        }
        // Eliminate
        for (int i = k + 1; i < 3; ++i) {
            float factor = U->m[k][i] / U->m[k][k];
            L->m[k][i] = factor;
            for (int j = k; j < 3; ++j) {
                U->m[j][i] -= factor * U->m[j][k];
            }
        }
    }
    return true;
}

static inline Vec3 forward_substitution_mat3(Mat3 L, Vec3 b) {
    Vec3 x;
    x.x = b.x / L.m[0][0];
    x.y = (b.y - L.m[0][1] * x.x) / L.m[1][1];
    x.z = (b.z - L.m[0][2] * x.x - L.m[1][2] * x.y) / L.m[2][2];
    return x;
}

static inline Vec3 backward_substitution_mat3(Mat3 U, Vec3 b) {
    Vec3 x;
    x.z = b.z / U.m[2][2];
    x.y = (b.y - U.m[2][1] * x.z) / U.m[1][1];
    x.x = (b.x - U.m[1][0] * x.y - U.m[2][0] * x.z) / U.m[0][0];
    return x;
}

static inline Mat3 mat3_exp(Mat3 A, int terms) {
    Mat3 result = mat3_identity();
    Mat3 power_A = mat3_identity();
    float factorial = 1.0f;
    for (int n = 1; n < terms; ++n) {
        factorial *= (float)n;
        power_A = mat3_mul(power_A, A);
        Mat3 term = mat3_scalar_mul(power_A, 1.0f / factorial);
        result = mat3_add(result, term);
    }
    return result;
}

/// @brief Multiplies two 4x4 matrices using SIMD.
static inline Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 result;
    for (int i = 0; i < 4; i++) {
        // We are calculating Column i of the result.
        // Res_Col_i = A * B_Col_i
        // This is a Matrix * Vector operation.
        // B_Col_i is a vector (b.cols[i]).

        simd_vec_t b_col = b.cols[i];

        // Splat components of B's column
        simd_vec_t x = simd_splat_x(b_col);
        simd_vec_t y = simd_splat_y(b_col);
        simd_vec_t z = simd_splat_z(b_col);
        simd_vec_t w = simd_splat_w(b_col);

        // Linear combination of A's columns
        // Res = x*A0 + y*A1 + z*A2 + w*A3
        simd_vec_t r = simd_mul(a.cols[0], x);
        r = simd_add(r, simd_mul(a.cols[1], y));
        r = simd_add(r, simd_mul(a.cols[2], z));
        r = simd_add(r, simd_mul(a.cols[3], w));

        result.cols[i] = r;
    }
    return result;
}

// ======================
// Matrix-Vector Operations
// ======================

static inline Vec3 mat3_mul_vec3(Mat3 m, Vec3 v) {
    simd_vec_t vx = simd_set1(v.x);
    simd_vec_t vy = simd_set1(v.y);
    simd_vec_t vz = simd_set1(v.z);

    // Load columns. We must be careful about memory boundaries.
    // Col 0: OK (4 floats)
    simd_vec_t c0 = simd_load(&m.m[0][0]);
    // Col 1: OK (4 floats)
    simd_vec_t c1 = simd_load(&m.m[1][0]);
    // Col 2: OK (4 floats)
    simd_vec_t c2 = simd_load(&m.m[2][0]);

    // Sum: c0*x + c1*y + c2*z
    simd_vec_t res = simd_mul(c0, vx);
    res = simd_add(res, simd_mul(c1, vy));
    res = simd_add(res, simd_mul(c2, vz));

    // We have {Rx, Ry, Rz, Garbage}
    Vec3 out;
    simd_vec_t temp = res;  // Holder
    // To extract, we can store or use simd_get functions if available
    float f[4];
    simd_store(f, temp);
    out.x = f[0];
    out.y = f[1];
    out.z = f[2];
    return out;
}

static inline Vec4 mat4_mul_vec4(Mat4 m, Vec4 v) {
    // Result = v.x * Col0 + v.y * Col1 + v.z * Col2 + v.w * Col3
    simd_vec_t vec = simd_load((const float*)&v);

    simd_vec_t x = simd_splat_x(vec);
    simd_vec_t y = simd_splat_y(vec);
    simd_vec_t z = simd_splat_z(vec);
    simd_vec_t w = simd_splat_w(vec);

    simd_vec_t res = simd_mul(m.cols[0], x);
    res = simd_add(res, simd_mul(m.cols[1], y));
    res = simd_add(res, simd_mul(m.cols[2], z));
    res = simd_add(res, simd_mul(m.cols[3], w));

    Vec4 out;
    simd_store((float*)&out, res);
    return out;
}

static inline Mat4 mat4_div(Mat4 a, float b) {
    Mat4 result;
    simd_vec_t div = simd_set1(b);
    result.cols[0] = simd_div(a.cols[0], div);
    result.cols[1] = simd_div(a.cols[1], div);
    result.cols[2] = simd_div(a.cols[2], div);
    result.cols[3] = simd_div(a.cols[3], div);
    return result;
}

// ======================
// Transformation Matrices
// ======================

static inline Mat4 mat4_translate(Vec3 translation) {
    Mat4 m = mat4_identity();
    m.cols[3] = simd_set(translation.x, translation.y, translation.z, 1.0f);
    return m;
}

static inline Mat4 mat4_scale(Vec3 scale) {
    Mat4 m = mat4_identity();
    m.cols[0] = simd_mul(m.cols[0], simd_set1(scale.x));
    m.cols[1] = simd_mul(m.cols[1], simd_set1(scale.y));
    m.cols[2] = simd_mul(m.cols[2], simd_set1(scale.z));
    return m;
}

static inline Mat4 mat4_rotate_x(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 m = mat4_identity();
    m.cols[1] = simd_set(0.0f, c, s, 0.0f);
    m.cols[2] = simd_set(0.0f, -s, c, 0.0f);
    return m;
}

static inline Mat4 mat4_rotate_y(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 m = mat4_identity();
    m.cols[0] = simd_set(c, 0.0f, -s, 0.0f);
    m.cols[2] = simd_set(s, 0.0f, c, 0.0f);
    return m;
}

static inline Mat4 mat4_rotate_z(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 m = mat4_identity();
    m.cols[0] = simd_set(c, s, 0.0f, 0.0f);
    m.cols[1] = simd_set(-s, c, 0.0f, 0.0f);
    return m;
}

static inline Mat4 mat4_rotate(Vec3 axis, float angle) {
    SimdVec3 a = vec3_normalize(vec3_load(axis));
    float c = cosf(angle);
    float s = sinf(angle);
    float t = 1.0f - c;

    Mat4 m = mat4_identity();

    // Diagonal (Row == Col, so no swap needed)
    m.m[0][0] = t * a.x * a.x + c;
    m.m[1][1] = t * a.y * a.y + c;
    m.m[2][2] = t * a.z * a.z + c;

    // Off-diagonals
    // Standard Math: Row 0, Col 1 => t*x*y - s*z
    // Storage: m[1][0]
    m.m[1][0] = t * a.x * a.y - s * a.z;

    // Standard Math: Row 0, Col 2 => t*x*z + s*y
    // Storage: m[2][0]
    m.m[2][0] = t * a.x * a.z + s * a.y;

    // Standard Math: Row 1, Col 0 => t*x*y + s*z
    // Storage: m[0][1]
    m.m[0][1] = t * a.x * a.y + s * a.z;

    // Standard Math: Row 1, Col 2 => t*y*z - s*x
    // Storage: m[2][1]
    m.m[2][1] = t * a.y * a.z - s * a.x;

    // Standard Math: Row 2, Col 0 => t*x*z - s*y
    // Storage: m[0][2]
    m.m[0][2] = t * a.x * a.z - s * a.y;

    // Standard Math: Row 2, Col 1 => t*y*z + s*x
    // Storage: m[1][2]
    m.m[1][2] = t * a.y * a.z + s * a.x;

    return m;
}

// ======================
// Matrix Inversion
// ======================

static inline Mat4 mat4_transpose(Mat4 m) {
    // Requires simd_transpose4 macro in simd.h
    simd_transpose4(m.cols[0], m.cols[1], m.cols[2], m.cols[3]);
    return m;
}

/* =========================================================
   Determinant & Inverse (Fixed)
   ========================================================= */

static inline float mat4_determinant(Mat4 m) {
    // Column 0 elements
    float m00 = m.m[0][0], m10 = m.m[0][1], m20 = m.m[0][2], m30 = m.m[0][3];
    // Column 1 elements
    float m01 = m.m[1][0], m11 = m.m[1][1], m21 = m.m[1][2], m31 = m.m[1][3];
    // Column 2 elements
    float m02 = m.m[2][0], m12 = m.m[2][1], m22 = m.m[2][2], m32 = m.m[2][3];
    // Column 3 elements
    float m03 = m.m[3][0], m13 = m.m[3][1], m23 = m.m[3][2], m33 = m.m[3][3];

    // Compute determinant using expansion along the first column
    // This is verbose but allows the compiler to optimize the arithmetic tree

    float det = m00 * (m11 * (m22 * m33 - m32 * m23) - m21 * (m12 * m33 - m32 * m13) + m31 * (m12 * m23 - m22 * m13)) -
                m10 * (m01 * (m22 * m33 - m32 * m23) - m21 * (m02 * m33 - m32 * m03) + m31 * (m02 * m23 - m22 * m03)) +
                m20 * (m01 * (m12 * m33 - m32 * m13) - m11 * (m02 * m33 - m32 * m03) + m31 * (m02 * m13 - m12 * m03)) -
                m30 * (m01 * (m12 * m23 - m22 * m13) - m11 * (m02 * m23 - m22 * m03) + m21 * (m02 * m13 - m12 * m03));

    return det;
}

static inline Mat4 mat4_inverse(Mat4 m) {
    // 1. Load matrix elements (Column-Major access: m[col][row])
    float m00 = m.m[0][0], m01 = m.m[1][0], m02 = m.m[2][0], m03 = m.m[3][0];
    float m10 = m.m[0][1], m11 = m.m[1][1], m12 = m.m[2][1], m13 = m.m[3][1];
    float m20 = m.m[0][2], m21 = m.m[1][2], m22 = m.m[2][2], m23 = m.m[3][2];
    float m30 = m.m[0][3], m31 = m.m[1][3], m32 = m.m[2][3], m33 = m.m[3][3];

    // 2. Compute 2x2 Sub-determinants (Factors)
    //    Pairs for Rows 2,3
    float Fac0 = m22 * m33 - m32 * m23;
    float Fac1 = m21 * m33 - m31 * m23;
    float Fac2 = m21 * m32 - m31 * m22;
    float Fac3 = m20 * m33 - m30 * m23;
    float Fac4 = m20 * m32 - m30 * m22;
    float Fac5 = m20 * m31 - m30 * m21;
    //    Pairs for Rows 0,1
    float Fac6 = m02 * m13 - m12 * m03;
    float Fac7 = m01 * m13 - m11 * m03;
    float Fac8 = m01 * m12 - m11 * m02;
    float Fac9 = m00 * m13 - m10 * m03;
    float Fac10 = m00 * m12 - m10 * m02;
    float Fac11 = m00 * m11 - m10 * m01;

    // 3. Compute Adjugate Matrix Columns (Transpose of Cofactors)

    // Inverse Column 0
    simd_vec_t col0 = simd_set((m11 * Fac0 - m12 * Fac1 + m13 * Fac2), -(m10 * Fac0 - m12 * Fac3 + m13 * Fac4),
                               (m10 * Fac1 - m11 * Fac3 + m13 * Fac5), -(m10 * Fac2 - m11 * Fac4 + m12 * Fac5));

    // Inverse Column 1
    simd_vec_t col1 = simd_set(-(m01 * Fac0 - m02 * Fac1 + m03 * Fac2), (m00 * Fac0 - m02 * Fac3 + m03 * Fac4),
                               -(m00 * Fac1 - m01 * Fac3 + m03 * Fac5), (m00 * Fac2 - m01 * Fac4 + m02 * Fac5));

    // Inverse Column 2
    simd_vec_t col2 = simd_set((m31 * Fac6 - m32 * Fac7 + m33 * Fac8), -(m30 * Fac6 - m32 * Fac9 + m33 * Fac10),
                               (m30 * Fac7 - m31 * Fac9 + m33 * Fac11), -(m30 * Fac8 - m31 * Fac10 + m32 * Fac11));

    // Inverse Column 3
    simd_vec_t col3 = simd_set(-(m21 * Fac6 - m22 * Fac7 + m23 * Fac8), (m20 * Fac6 - m22 * Fac9 + m23 * Fac10),
                               -(m20 * Fac7 - m21 * Fac9 + m23 * Fac11), (m20 * Fac8 - m21 * Fac10 + m22 * Fac11));

    // 4. Calculate Determinant
    //    Dot product of first column of inputs with first column of adjugate
    //    Det = m00*Adj00 + m10*Adj10 + m20*Adj20 + m30*Adj30
    simd_vec_t input_col0 = simd_set(m00, m10, m20, m30);
    float det = simd_dot4(input_col0, col0);

    // 5. Check Singularity
    if (fabsf(det) < 1e-8f) {
        return mat4_identity();
    }

    // 6. Scale by 1/Det
    simd_vec_t inv_det_vec = simd_set1(1.0f / det);

    Mat4 inv;
    inv.cols[0] = simd_mul(col0, inv_det_vec);
    inv.cols[1] = simd_mul(col1, inv_det_vec);
    inv.cols[2] = simd_mul(col2, inv_det_vec);
    inv.cols[3] = simd_mul(col3, inv_det_vec);

    return inv;
}

// ======================
// Projection Matrices
// ======================

static inline Mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far) {
    Mat4 m = mat4_identity();
    // Diagonals
    m.cols[0] = simd_set(2.0f / (right - left), 0, 0, 0);
    m.cols[1] = simd_set(0, 2.0f / (top - bottom), 0, 0);
    m.cols[2] = simd_set(0, 0, -2.0f / (far - near), 0);

    // Last column
    m.cols[3] = simd_set(-(right + left) / (right - left), -(top + bottom) / (top - bottom),
                         -(far + near) / (far - near), 1.0f);
    return m;
}

static inline Mat4 mat4_perspective(float fov_radians, float aspect, float near, float far) {
    float tan_half_fov = tanf(fov_radians / 2.0f);
    Mat4 m;
    m.cols[0] = simd_set(1.0f / (aspect * tan_half_fov), 0, 0, 0);
    m.cols[1] = simd_set(0, 1.0f / tan_half_fov, 0, 0);
    m.cols[2] = simd_set(0, 0, -(far + near) / (far - near), -1.0f);
    m.cols[3] = simd_set(0, 0, -(2.0f * far * near) / (far - near), 0.0f);
    return m;
}

static inline Mat4 mat4_look_at(SimdVec3 eye, SimdVec3 target, SimdVec3 up) {
    SimdVec3 z = vec3_normalize(vec3_sub(eye, target));
    SimdVec3 x = vec3_normalize(vec3_cross(up, z));
    SimdVec3 y = vec3_cross(z, x);

    Mat4 view;
    view.cols[0] = simd_set(x.x, y.x, z.x, 0.0f);  // Col 0 (Row 0 of rotation)
    view.cols[1] = simd_set(x.y, y.y, z.y, 0.0f);  // Col 1
    view.cols[2] = simd_set(x.z, y.z, z.z, 0.0f);  // Col 2

    // Dot products for translation
    view.cols[3] = simd_set(-vec3_dot(x, eye), -vec3_dot(y, eye), -vec3_dot(z, eye), 1.0f);
    return view;
}

#ifdef __cplusplus
}
#endif

#endif  // MATRIX_H
