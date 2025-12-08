#ifndef MATRIX_H
#define MATRIX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <float.h>
#include <immintrin.h>
#include <stdbool.h>
#include "vec.h"

/**
 * @struct Mat3
 * @brief A 3x3 matrix for 2D transformations and 3D rotations.
 *
 * Matrix elements are stored in column-major order (OpenGL style):
 * | m[0] m[3] m[6] |
 * | m[1] m[4] m[7] |
 * | m[2] m[5] m[8] |
 */
typedef struct __attribute__((aligned(16))) Mat3 {
    float m[3][3];  // Column-major storage as m[col][row]
} Mat3;

/**
 * @struct Mat4
 * @brief A 4x4 matrix for 3D transformations.
 *
 * Matrix elements are stored in column-major order (OpenGL style):
 * | m[0]  m[4]  m[8]  m[12] |
 * | m[1]  m[5]  m[9]  m[13] |
 * | m[2]  m[6]  m[10] m[14] |
 * | m[3]  m[7]  m[11] m[15] |
 */
typedef struct __attribute__((aligned(32))) Mat4 {
    union {
        float m[4][4];   // Column-major storage as m[col][row]
        __m128 cols[4];  // SIMD columns
    };
} Mat4;

// Debugging
//================

/**
 * Creates a Mat3 with elements stored in column-major order.
 * This means m[col][row] in the struct holds the element at mathematical row,
 * col. Use this if your functions (like mat3_lu, solve_mat3) expect
 * column-major storage.
 * @param m00, m01, ... element values in row-major order (as you'd write the
 * matrix).
 * @return  A Mat3 struct with elements stored column-major.
 */
static inline Mat3 mat3_new_column_major(float m00, float m01, float m02, float m10, float m11, float m12, float m20,
                                         float m21, float m22) {
    Mat3 mat;
    // Store element A_row,col at mat.m[col][row]
    mat.m[0][0] = m00;
    mat.m[1][0] = m01;
    mat.m[2][0] = m02;  // Row 0 values go into col 0, 1, 2 at row 0 of array
    mat.m[0][1] = m10;
    mat.m[1][1] = m11;
    mat.m[2][1] = m12;  // Row 1 values go into col 0, 1, 2 at row 1 of array
    mat.m[0][2] = m20;
    mat.m[1][2] = m21;
    mat.m[2][2] = m22;  // Row 2 values go into col 0, 1, 2 at row 2 of array
    return mat;
}

// Helper function to create a Mat4 with column-major storage
static inline Mat4 mat4_new_column_major(float m00, float m01, float m02, float m03, float m10, float m11, float m12,
                                         float m13, float m20, float m21, float m22, float m23, float m30, float m31,
                                         float m32, float m33) {
    Mat4 mat;
    // Store A_row,col at mat.m[col][row]
    mat.m[0][0] = m00;
    mat.m[1][0] = m01;
    mat.m[2][0] = m02;
    mat.m[3][0] = m03;  // Row 0 values
    mat.m[0][1] = m10;
    mat.m[1][1] = m11;
    mat.m[2][1] = m12;
    mat.m[3][1] = m13;  // Row 1 values
    mat.m[0][2] = m20;
    mat.m[1][2] = m21;
    mat.m[2][2] = m22;
    mat.m[3][2] = m23;  // Row 2 values
    mat.m[0][3] = m30;
    mat.m[1][3] = m31;
    mat.m[2][3] = m32;
    mat.m[3][3] = m33;  // Row 3 values
    return mat;
}

/// @brief Prints a Mat3 matrix to the standard output.
/// @param m The Mat3 matrix to print.
static inline void mat3_print(Mat3 mat, const char* name) {
    printf("%s = \n", name);
    for (int i = 0; i < 3; i++) {
        printf("  [");
        for (int j = 0; j < 3; j++) {
            printf("%8.4f", mat.m[j][i]);  // Transpose for row-major display
            if (j < 2) printf(", ");
        }
        printf("]\n");
    }
    printf("\n");
}

/// @brief Prints a Mat4 matrix to the standard output.
/// @param m The Mat4 matrix to print.
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

/// @brief Creates a 3x3 identity matrix.
/// @return A Mat3 identity matrix.
static inline Mat3 mat3_identity(void) {
    Mat3 m = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    return m;
}

static inline bool mat3_equal(Mat3 a, Mat3 b) {
    static float EPSILON = 1e-6f;

    const __m128 epsilon  = _mm_set1_ps(EPSILON);
    const __m128 neg_zero = _mm_set1_ps(-0.0f);  // for fabs

    const float* pa = &a.m[0][0];
    const float* pb = &b.m[0][0];

    // Compare first 4 floats
    __m128 diff1 = _mm_sub_ps(_mm_loadu_ps(pa), _mm_loadu_ps(pb));
    __m128 abs1  = _mm_andnot_ps(neg_zero, diff1);
    __m128 cmp1  = _mm_cmplt_ps(abs1, epsilon);
    if (_mm_movemask_ps(cmp1) != 0xF) return false;

    // Compare next 4 floats
    __m128 diff2 = _mm_sub_ps(_mm_loadu_ps(pa + 4), _mm_loadu_ps(pb + 4));
    __m128 abs2  = _mm_andnot_ps(neg_zero, diff2);
    __m128 cmp2  = _mm_cmplt_ps(abs2, epsilon);
    if (_mm_movemask_ps(cmp2) != 0xF) return false;

    // Compare final element (9th float)
    float da = pa[8] - pb[8];
    if (da < -EPSILON || da > EPSILON) return false;

    return true;
}

static inline bool mat4_equal(Mat4 a, Mat4 b) {
    static float EPSILON = 1e-6f;

    __m128 epsilon  = _mm_set1_ps(EPSILON);
    __m128 neg_zero = _mm_set1_ps(-0.0f);  // for fabs via bitmask

    for (int i = 0; i < 4; ++i) {
        __m128 diff     = _mm_sub_ps(a.cols[i], b.cols[i]);
        __m128 abs_diff = _mm_andnot_ps(neg_zero, diff);  // fabs

        __m128 cmp = _mm_cmplt_ps(abs_diff, epsilon);
        int mask   = _mm_movemask_ps(cmp);
        if (mask != 0xF) return false;
    }
    return true;
}

/// @brief Returns the diagonal matrix extracted from matrix m.
/// @param m The input matrix
/// @return A Mat3 diagonal matrix with the diagonal elements from m.
static inline Mat3 mat3_diag(Mat3 m) {
    return (Mat3){{{m.m[0][0], 0.0f, 0.0f}, {0.0f, m.m[1][1], 0.0f}, {0.0f, 0.0f, m.m[2][2]}}};
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

/// @brief Returns the diagonal matrix extracted from matrix m.
/// @param m The input matrix
/// @return A Mat4 diagonal matrix with the diagonal elements from m.
/// @brief Returns the diagonal matrix extracted from matrix m.
/// @param m The input 4x4 matrix
/// @return A Mat4 diagonal matrix with the diagonal elements from m.
static inline Mat4 mat4_diag(Mat4 m) {
    return (Mat4){{{{m.cols[0][0], 0.0f, 0.0f, 0.0f},
                    {0.0f, m.cols[1][1], 0.0f, 0.0f},
                    {0.0f, 0.0f, m.cols[2][2], 0.0f},
                    {0.0f, 0.0f, 0.0f, m.m[3][3]}}}};
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

    __m128 scalar_vec = _mm_set1_ps(scalar);

    const float* src = &m.m[0][0];
    float* dst       = &result.m[0][0];

    // Add scalar to first 4 elements
    __m128 a = _mm_loadu_ps(src);
    a        = _mm_add_ps(a, scalar_vec);
    _mm_storeu_ps(dst, a);

    // Add scalar to next 4 elements
    __m128 b = _mm_loadu_ps(src + 4);
    b        = _mm_add_ps(b, scalar_vec);
    _mm_storeu_ps(dst + 4, b);

    // Add scalar to the 9th element
    dst[8] = src[8] + scalar;

    return result;
}

Mat3 mat3_add(Mat3 a, Mat3 b) {
    Mat3 result;

    const float* pa = &a.m[0][0];
    const float* pb = &b.m[0][0];
    float* pr       = &result.m[0][0];

    // Add first 4 elements
    __m128 va1  = _mm_loadu_ps(pa);
    __m128 vb1  = _mm_loadu_ps(pb);
    __m128 sum1 = _mm_add_ps(va1, vb1);
    _mm_storeu_ps(pr, sum1);

    // Add next 4 elements
    __m128 va2  = _mm_loadu_ps(pa + 4);
    __m128 vb2  = _mm_loadu_ps(pb + 4);
    __m128 sum2 = _mm_add_ps(va2, vb2);
    _mm_storeu_ps(pr + 4, sum2);

    // Add final element
    pr[8] = pa[8] + pb[8];

    return result;
}

static inline Mat3 mat3_scalar_mul(Mat3 m, float scalar) {
    Mat3 result;
    const float* src = &m.m[0][0];
    float* dst       = &result.m[0][0];

    __m128 s = _mm_set1_ps(scalar);

    // Multiply first 4 floats
    __m128 a = _mm_loadu_ps(src);
    a        = _mm_mul_ps(a, s);
    _mm_storeu_ps(dst, a);

    // Multiply next 4 floats
    __m128 b = _mm_loadu_ps(src + 4);
    b        = _mm_mul_ps(b, s);
    _mm_storeu_ps(dst + 4, b);

    // Multiply the 9th float
    dst[8] = src[8] * scalar;

    return result;
}

/// @brief Calculates the determinant of a 3x3 matrix
/// @param m The Mat3 matrix
/// @return The determinant of the matrix
static inline float mat3_determinant(Mat3 m) {
    return m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[2][1] * m.m[1][2]) -
           m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[2][0] * m.m[1][2]) +
           m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[2][0] * m.m[1][1]);
}

/// @brief Calculates the inverse of a 3x3 matrix
/// @param m The Mat3 matrix to invert
/// @param[out] inv Output inverted matrix
/// @return true if successful, false if matrix is singular
static inline bool mat3_inverse(Mat3 m, Mat3* inv) {
    const float det = mat3_determinant(m);

    // Check for singularity (using absolute value)
    if (fabsf(det) < 1e-8f) {
        return false;
    }

    const float inv_det = 1.0f / det;

    inv->m[0][0] = (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) * inv_det;
    inv->m[0][1] = (m.m[0][2] * m.m[2][1] - m.m[0][1] * m.m[2][2]) * inv_det;
    inv->m[0][2] = (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * inv_det;

    inv->m[1][0] = (m.m[1][2] * m.m[2][0] - m.m[1][0] * m.m[2][2]) * inv_det;
    inv->m[1][1] = (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * inv_det;
    inv->m[1][2] = (m.m[0][2] * m.m[1][0] - m.m[0][0] * m.m[1][2]) * inv_det;

    inv->m[2][0] = (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]) * inv_det;
    inv->m[2][1] = (m.m[0][1] * m.m[2][0] - m.m[0][0] * m.m[2][1]) * inv_det;
    inv->m[2][2] = (m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[1][0]) * inv_det;

    return true;
}

/**
 * LU Decomposition with partial pivoting for a 3x3 matrix.
 * Decomposes A into PA = LU, where P is a permutation matrix,
 * L is lower triangular with ones on diagonal, and U is upper triangular.
 * Assumes column-major matrices.
 *
 * @param A Input 3x3 matrix (column-major).
 * @param L Output lower triangular matrix (column-major). Must not be NULL.
 * @param U Output upper triangular matrix (column-major). Must not be NULL.
 * @param P Output permutation matrix (column-major). Must not be NULL.
 * @return true if decomposition successful, false if matrix is singular.
 * @note Uses partial pivoting for numerical stability.
 * @note Tolerance for singularity is 1e-6.
 */
static inline bool mat3_lu(Mat3 A, Mat3* L, Mat3* U, Mat3* P) {
    const float tolerance = 1e-6f;

    // Initialize U to A
    *U = A;

    // Initialize L to identity
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            L->m[j][i] = (i == j) ? 1.0f : 0.0f;

    // Initialize P to identity
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            P->m[j][i] = (i == j) ? 1.0f : 0.0f;

    // LU Decomposition with partial pivoting
    for (int k = 0; k < 3; ++k) {
        // Find pivot row
        int pivot_row = k;
        float max_val = fabsf(U->m[k][k]);
        for (int i = k + 1; i < 3; ++i) {
            float val = fabsf(U->m[k][i]);
            if (val > max_val) {
                max_val   = val;
                pivot_row = i;
            }
        }

        if (max_val < tolerance) return false;  // Singular matrix

        // Swap rows in U and P if needed
        if (pivot_row != k) {
            for (int j = 0; j < 3; ++j) {
                float tmp          = U->m[j][k];
                U->m[j][k]         = U->m[j][pivot_row];
                U->m[j][pivot_row] = tmp;

                tmp                = P->m[j][k];
                P->m[j][k]         = P->m[j][pivot_row];
                P->m[j][pivot_row] = tmp;

                if (j < k) {
                    tmp                = L->m[j][k];
                    L->m[j][k]         = L->m[j][pivot_row];
                    L->m[j][pivot_row] = tmp;
                }
            }
        }

        // Elimination
        for (int i = k + 1; i < 3; ++i) {
            float factor = U->m[k][i] / U->m[k][k];
            L->m[k][i]   = factor;

            for (int j = k; j < 3; ++j) {
                U->m[j][i] -= factor * U->m[j][k];
            }
        }
    }

    return true;
}

/**
 * Forward substitution for solving Lx = b, where L is a lower triangular 3x3 matrix.
 * Assumes L is column-major.
 *
 * @param L Lower triangular 3x3 matrix (column-major). Diagonal elements must be non-zero.
 * @param b Right-hand side vector.
 * @return Solution vector x.
 * @note Does not check for zero diagonal elements. Caller must ensure L is valid.
 */
static inline Vec3 forward_substitution_mat3(Mat3 L, Vec3 b) {
    Vec3 x;
    x.x = b.x / L.m[0][0];                                        // L[0][0]
    x.y = (b.y - L.m[0][1] * x.x) / L.m[1][1];                    // L[1][0]*x.x / L[1][1]
    x.z = (b.z - L.m[0][2] * x.x - L.m[1][2] * x.y) / L.m[2][2];  // L[2][0]*x.x + L[2][1]*x.y
    return x;
}

/**
 * Backward substitution for solving Ux = b, where U is an upper triangular 3x3 matrix.
 * Assumes U is column-major.
 *
 * @param U Upper triangular 3x3 matrix (column-major). Diagonal elements must be non-zero.
 * @param b Right-hand side vector.
 * @return Solution vector x.
 * @note Does not check for zero diagonal elements. Caller must ensure U is valid.
 */
static inline Vec3 backward_substitution_mat3(Mat3 U, Vec3 b) {
    Vec3 x;
    x.z = b.z / U.m[2][2];                                        // U[2][2]
    x.y = (b.y - U.m[2][1] * x.z) / U.m[1][1];                    // U[1][2]*x.z / U[1][1]
    x.x = (b.x - U.m[1][0] * x.y - U.m[2][0] * x.z) / U.m[0][0];  // U[0][1]*x.y + U[0][2]*x.z
    return x;
}

/**
 * Matrix exponential for 3x3 using Taylor series approximation.
 * @param A      Input 3x3 matrix
 * @param terms  Number of terms to use in the Taylor series expansion
 */
static inline Mat3 mat3_exp(Mat3 A, int terms) {
    Mat3 I       = mat3_identity();  // Identity matrix
    Mat3 result  = I;                // Start with I
    Mat3 power_A = I;                // A^0 = I

    float factorial = 1.0f;

    for (int n = 1; n < terms; ++n) {
        factorial *= (float)n;
        power_A   = mat3_mul(power_A, A);                        // A^n
        Mat3 term = mat3_scalar_mul(power_A, 1.0f / factorial);  // A^n / n!
        result    = mat3_add(result, term);
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

    return (Vec3){_mm_cvtss_f32(result), _mm_cvtss_f32(_mm_shuffle_ps(result, result, _MM_SHUFFLE(1, 1, 1, 1))),
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
/// returns a 4x4 matrix that scales points or vectors along the X, Y, and Z
/// axes.
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

/// @brief Calculates the determinant of a 4x4 matrix using SIMD (SSE
/// intrinsics).
/// @param m The Mat4 matrix.
/// @return The determinant of the matrix.
static inline float mat4_determinant(Mat4 m) {
    // Calculate sub-determinants for first row
    __m128 s0 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3]))))));

    __m128 s1 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s2 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s3 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
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
/// @return The inverted Mat4 matrix. Returns an identity matrix if the input
/// matrix is singular.
static inline Mat4 mat4_inverse(Mat4 m) {
    Mat4 inv;
    // Calculate sub-determinants for first row
    __m128 s0 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3]))))));

    __m128 s1 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s2 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][1]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 s3 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
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
    __m128 c0 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3]))))));

    __m128 c1 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c2 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][2]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c3 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[2][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[2][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[2][2]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[2][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), _mm_set1_ps(m.m[1][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[1][2]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c4 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][3]))))));

    __m128 c5 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c6 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][3])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][3])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[1][3])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[0][3]))))));

    __m128 c7 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][3])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][3])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][3])),
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
    __m128 c8 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][2])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][2])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][2])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][2])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][2])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][2]))))));

    __m128 c9 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[3][2])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[2][2])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[2][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][2])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][2])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][2])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[0][2]))))));

    __m128 c10 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[3][2])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[1][2])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[3][2])),
                                                          _mm_mul_ps(_mm_set1_ps(m.m[3][1]), _mm_set1_ps(m.m[0][2])))),
            _mm_mul_ps(_mm_set1_ps(m.m[3][0]),
                       _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[1][2])),
                                  _mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[0][2]))))));

    __m128 c11 = _mm_sub_ps(
        _mm_mul_ps(_mm_set1_ps(m.m[0][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][1]), _mm_set1_ps(m.m[2][2])),
                                                      _mm_mul_ps(_mm_set1_ps(m.m[2][1]), _mm_set1_ps(m.m[1][2])))),
        _mm_add_ps(
            _mm_mul_ps(_mm_set1_ps(m.m[1][0]), _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][1]), _mm_set1_ps(m.m[2][2])),
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

#ifdef __cplusplus
}
#endif

#endif  // MATRIX_H
