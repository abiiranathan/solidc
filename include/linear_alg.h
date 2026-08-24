/**
 * @file linear_alg.h
 * @brief Linear algebra operations and utilities.
 */

#ifndef LINEAR_ALG_H
#define LINEAR_ALG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "matrix.h"
#include "vec.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Orthonormal basis consisting of three mutually perpendicular unit vectors.
 * Forms a right-handed coordinate system.
 */
typedef struct {
    Vec3 v0; /**< Right vector. */
    Vec3 v1; /**< Up vector. */
    Vec3 v2; /**< Forward vector. */
} OrthonormalBasis;

/**
 * Constructs an orthonormal basis from two input vectors using Gram-Schmidt orthogonalization.
 */
static inline OrthonormalBasis orthonormalize(Vec3 v0, Vec3 v1) {
    SimdVec3 sv0 = vec3_load(v0);
    SimdVec3 sv1 = vec3_load(v1);

    sv0 = vec3_normalize(sv0);

    float dot = vec3_dot(sv0, sv1);
    sv1 = vec3_sub(sv1, vec3_mul(sv0, dot));
    sv1 = vec3_normalize(sv1);

    SimdVec3 sv2 = vec3_cross(sv0, sv1);

    return (OrthonormalBasis){vec3_store(sv0), vec3_store(sv1), vec3_store(sv2)};
}

/**
 * Result of eigenvalue decomposition for a 3x3 matrix.
 */
typedef struct {
    Vec3 eigenvalues;  /**< Eigenvalues (x, y, z). */
    Mat3 eigenvectors; /**< Eigenvectors stored as columns. */
} EigenDecomposition;

/**
 * Computes eigenvalues/vectors of a symmetric 3x3 matrix using Jacobi iteration.
 */
static inline EigenDecomposition mat3_eigen_symmetric(Mat3 A) {
    EigenDecomposition result;
    Mat3 V = mat3_identity();

    const int MAX_ITERS = 32;
    const float EPSILON = 1e-10f;

    for (int iter = 0; iter < MAX_ITERS; ++iter) {
        int p = 0, q = 1;
        float max = fabsf(A.m[0][1]);

        if (fabsf(A.m[0][2]) > max) {
            p = 0;
            q = 2;
            max = fabsf(A.m[0][2]);
        }
        if (fabsf(A.m[1][2]) > max) {
            p = 1;
            q = 2;
            max = fabsf(A.m[1][2]);
        }

        if (max < EPSILON) break;

        float app = A.m[p][p];
        float aqq = A.m[q][q];
        float apq = A.m[p][q];

        float phi = 0.5f * atanf((2.0f * apq) / (aqq - app + 1e-20f));
        float c = cosf(phi);
        float s = sinf(phi);

        for (int r = 0; r < 3; ++r) {
            float arp = A.m[r][p];
            float arq = A.m[r][q];
            A.m[r][p] = c * arp - s * arq;
            A.m[r][q] = s * arp + c * arq;
        }

        for (int r = 0; r < 3; ++r) {
            float arp = A.m[p][r];
            float arq = A.m[q][r];
            A.m[p][r] = c * arp - s * arq;
            A.m[q][r] = s * arp + c * arq;
        }

        A.m[p][p] = c * c * app - 2 * s * c * apq + s * s * aqq;
        A.m[q][q] = s * s * app + 2 * s * c * apq + c * c * aqq;
        A.m[p][q] = A.m[q][p] = 0.0f;

        for (int r = 0; r < 3; ++r) {
            float vrp = V.m[r][p];
            float vrq = V.m[r][q];
            V.m[r][p] = c * vrp - s * vrq;
            V.m[r][q] = s * vrp + c * vrq;
        }
    }

    result.eigenvalues = (Vec3){A.m[0][0], A.m[1][1], A.m[2][2]};
    result.eigenvectors = V;
    return result;
}

/**
 * Computes Singular Value Decomposition (SVD) of a 3x3 matrix.
 */
static inline void mat3_svd(Mat3 A, Mat3* U, Vec3* S, Mat3* V) {
    Mat3 ATA = {0};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            ATA.m[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                ATA.m[i][j] += A.m[i][k] * A.m[j][k];
            }
        }
    }

    EigenDecomposition ed = mat3_eigen_symmetric(ATA);
    *V = ed.eigenvectors;

    int order[3] = {0, 1, 2};
    float vals[3] = {ed.eigenvalues.x, ed.eigenvalues.y, ed.eigenvalues.z};

    for (int i = 0; i < 2; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (vals[order[i]] < vals[order[j]]) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    Vec3 sorted_eigen = {vals[order[0]], vals[order[1]], vals[order[2]]};

    Mat3 V_sorted = {0};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            V_sorted.m[i][j] = V->m[order[i]][j];
        }
    }
    *V = V_sorted;

    S->x = sqrtf(fmaxf(0.0f, sorted_eigen.x));
    S->y = sqrtf(fmaxf(0.0f, sorted_eigen.y));
    S->z = sqrtf(fmaxf(0.0f, sorted_eigen.z));

    SimdVec3 colA[3] = {vec3_load((Vec3){A.m[0][0], A.m[0][1], A.m[0][2]}),
                        vec3_load((Vec3){A.m[1][0], A.m[1][1], A.m[1][2]}),
                        vec3_load((Vec3){A.m[2][0], A.m[2][1], A.m[2][2]})};

    float* s_arr = (float*)S;
    for (int i = 0; i < 3; ++i) {
        float sigma = s_arr[i];
        if (sigma > 1e-6f) {
            Vec3 v_col = {V->m[i][0], V->m[i][1], V->m[i][2]};
            SimdVec3 Av =
                vec3_add(vec3_add(vec3_mul(colA[0], v_col.x), vec3_mul(colA[1], v_col.y)), vec3_mul(colA[2], v_col.z));
            SimdVec3 u_col = vec3_mul(Av, 1.0f / sigma);
            Vec3 res = vec3_store(u_col);
            U->m[i][0] = res.x;
            U->m[i][1] = res.y;
            U->m[i][2] = res.z;
        } else {
            U->m[i][0] = U->m[i][1] = U->m[i][2] = 0.0f;
        }
    }

    if (S->y > 1e-6f && S->z < 1e-6f) {
        SimdVec3 u0 = vec3_load((Vec3){U->m[0][0], U->m[0][1], U->m[0][2]});
        SimdVec3 u1 = vec3_load((Vec3){U->m[1][0], U->m[1][1], U->m[1][2]});
        SimdVec3 u2 = vec3_cross(u0, u1);
        Vec3 res = vec3_store(u2);
        U->m[2][0] = res.x;
        U->m[2][1] = res.y;
        U->m[2][2] = res.z;
    }

    if (mat3_determinant(*U) < 0.0f) {
        U->m[2][0] = -U->m[2][0];
        U->m[2][1] = -U->m[2][1];
        U->m[2][2] = -U->m[2][2];
    }
}

/**
 * Computes QR decomposition of a 4x4 matrix using SIMD Gram-Schmidt.
 * A = Q * R
 */
static inline void mat4_qr(Mat4 A, Mat4* Q, Mat4* R) {
    SimdVec4 v[4], q[4];
    for (int i = 0; i < 4; ++i) {
        v[i].v = A.cols[i];
    }

    for (int i = 0; i < 4; ++i) {
        q[i] = v[i];
        for (int j = 0; j < i; ++j) {
            float r = vec4_dot(q[j], v[i]);
            // Write to Upper Triangle (Col i, Row j)
            // Assuming Mat4 m[col][row], this is m[i][j]
            R->m[i][j] = r;
            q[i] = vec4_sub(q[i], vec4_mul(q[j], r));
        }

        float norm = vec4_length(q[i]);
        if (norm < 1e-6f) norm = 1e-6f;

        R->m[i][i] = norm;
        q[i] = vec4_mul(q[i], 1.0f / norm);

        // Zero the Lower Triangle (Col j, Row i where i > j)
        for (int j = 0; j < i; ++j) {
            R->m[j][i] = 0.0f;
        }
    }

    for (int i = 0; i < 4; ++i) {
        Q->cols[i] = q[i].v;
    }
}

/**
 * Computes the dominant eigenpair of a 4x4 matrix using SIMD power iteration.
 */
static inline void mat4_power_iteration(Mat4 A, Vec4* eigenvector, float* eigenvalue, int max_iter, float tol) {
    SimdVec4 v = vec4_load((Vec4){1.0f, 0.0f, 0.0f, 0.0f});
    SimdVec4 Av;
    float lambda_old = 0.0f;

    for (int iter = 0; iter < max_iter; ++iter) {
        Vec4 v_scalar = vec4_store(v);
        Vec4 Av_scalar = mat4_mul_vec4(A, v_scalar);
        Av = vec4_load(Av_scalar);

        *eigenvalue = vec4_dot(Av, v);
        Av = vec4_normalize(Av);

        if (fabsf(*eigenvalue - lambda_old) < tol) {
            break;
        }
        lambda_old = *eigenvalue;
        v = Av;
    }

    *eigenvector = vec4_store(v);
}

static inline float mat4_norm_frobenius(Mat4 A) {
    SimdVec4 c0 = {.v = A.cols[0]};
    SimdVec4 c1 = {.v = A.cols[1]};
    SimdVec4 c2 = {.v = A.cols[2]};
    SimdVec4 c3 = {.v = A.cols[3]};

    float sum = 0.0f;
    sum += vec4_dot(c0, c0);
    sum += vec4_dot(c1, c1);
    sum += vec4_dot(c2, c2);
    sum += vec4_dot(c3, c3);

    return sqrtf(sum);
}

static inline bool mat3_is_positive_definite(Mat3 A) {
    if (A.m[0][0] <= 0.0f) return false;
    float det2 = A.m[0][0] * A.m[1][1] - A.m[1][0] * A.m[0][1];
    if (det2 <= 0.0f) return false;
    if (mat3_determinant(A) <= 0.0f) return false;
    return true;
}

static inline float mat4_condition_number(Mat4 A) {
    float norm_A = mat4_norm_frobenius(A);
    Mat4 A_inv = mat4_inverse(A);
    float norm_A_inv = mat4_norm_frobenius(A_inv);
    return norm_A * norm_A_inv;
}

// --- LU Decomposition & Solving ---

static inline bool mat4_lu(Mat4 A, Mat4* L, Mat4* U, Mat4* P) {
    const float tolerance = 1e-6f;
    *U = A;
    *L = mat4_identity();
    *P = mat4_identity();

    for (int k = 0; k < 4; ++k) {
        int pivot_row = k;
        float max_val = fabsf(U->m[k][k]);
        for (int i = k + 1; i < 4; ++i) {
            float val = fabsf(U->m[k][i]);
            if (val > max_val) {
                max_val = val;
                pivot_row = i;
            }
        }

        if (max_val < tolerance) return false;

        if (pivot_row != k) {
            for (int j = 0; j < 4; ++j) {
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

        for (int i = k + 1; i < 4; ++i) {
            float factor = U->m[k][i] / U->m[k][k];
            L->m[k][i] = factor;
            for (int j = k; j < 4; ++j) {
                U->m[j][i] -= factor * U->m[j][k];
            }
        }
    }
    return true;
}

static inline Vec4 forward_substitution_mat4(Mat4 L, Vec4 b) {
    Vec4 x;
    float* x_arr = &x.x;
    x_arr[0] = b.x / L.m[0][0];
    x_arr[1] = (b.y - L.m[0][1] * x_arr[0]) / L.m[1][1];
    x_arr[2] = (b.z - (L.m[0][2] * x_arr[0] + L.m[1][2] * x_arr[1])) / L.m[2][2];
    x_arr[3] = (b.w - (L.m[0][3] * x_arr[0] + L.m[1][3] * x_arr[1] + L.m[2][3] * x_arr[2])) / L.m[3][3];
    return x;
}

static inline Vec4 backward_substitution_mat4(Mat4 U, Vec4 b) {
    Vec4 x;
    float* x_arr = &x.x;
    x_arr[3] = b.w / U.m[3][3];
    x_arr[2] = (b.z - U.m[3][2] * x_arr[3]) / U.m[2][2];
    x_arr[1] = (b.y - (U.m[2][1] * x_arr[2] + U.m[3][1] * x_arr[3])) / U.m[1][1];
    x_arr[0] = (b.x - (U.m[1][0] * x_arr[1] + U.m[2][0] * x_arr[2] + U.m[3][0] * x_arr[3])) / U.m[0][0];
    return x;
}

static inline Vec4 mat4_solve(Mat4 A, Vec4 b) {
    Mat4 L, U, P;
    if (!mat4_lu(A, &L, &U, &P)) {
        return (Vec4){0};
    }
    Vec4 Pb;
    Pb.x = P.m[0][0] * b.x + P.m[1][0] * b.y + P.m[2][0] * b.z + P.m[3][0] * b.w;
    Pb.y = P.m[0][1] * b.x + P.m[1][1] * b.y + P.m[2][1] * b.z + P.m[3][1] * b.w;
    Pb.z = P.m[0][2] * b.x + P.m[1][2] * b.y + P.m[2][2] * b.z + P.m[3][2] * b.w;
    Pb.w = P.m[0][3] * b.x + P.m[1][3] * b.y + P.m[2][3] * b.z + P.m[3][3] * b.w;

    Vec4 y = forward_substitution_mat4(L, Pb);
    return backward_substitution_mat4(U, y);
}

static inline Vec3 mat3_solve(Mat3 A, Vec3 b) {
    Mat3 L, U, P;
    if (!mat3_lu(A, &L, &U, &P)) return (Vec3){0};

    Vec3 Pb;
    Pb.x = P.m[0][0] * b.x + P.m[1][0] * b.y + P.m[2][0] * b.z;
    Pb.y = P.m[0][1] * b.x + P.m[1][1] * b.y + P.m[2][1] * b.z;
    Pb.z = P.m[0][2] * b.x + P.m[1][2] * b.y + P.m[2][2] * b.z;

    Vec3 y = forward_substitution_mat3(L, Pb);
    return backward_substitution_mat3(U, y);
}

/* ==================================================
   Graphics Extensions
   ================================================== */

/**
 * @brief Decomposes a TRS model matrix into scale, rotation, and translation.
 *
 * Inverse of mat4_compose(). The matrix is assumed to have the form T*R*S
 * (no skew/shear), which is what scene graphs produce.
 *
 * @param[in]  m           Model matrix to decompose.
 * @param[out] out_scale   Absolute scale per axis. Negative X indicates a
 *                         mirrored transform (negative determinant).
 * @param[out] out_rotation Unit rotation quaternion.
 * @param[out] out_translation World position.
 */
static inline void mat4_decompose(Mat4 m, Vec3* out_scale, Quat* out_rotation, Vec3* out_translation) {
    SimdVec3 c0 = {.v = m.cols[0]};
    SimdVec3 c1 = {.v = m.cols[1]};
    SimdVec3 c2 = {.v = m.cols[2]};
    SimdVec3 c3 = {.v = m.cols[3]};

    *out_translation = vec3_store(c3);

    // Scale is the length of each rotated basis column.
    float sx = vec3_length(c0);
    float sy = vec3_length(c1);
    float sz = vec3_length(c2);

    // Mirror detection via the sign of the upper-3x3 determinant.
    Mat3 rot_only;
    if (sx > 1e-12f && sy > 1e-12f && sz > 1e-12f) {
        rot_only = (Mat3){{{c0.x / sx, c0.y / sx, c0.z / sx},
                           {c1.x / sy, c1.y / sy, c1.z / sy},
                           {c2.x / sz, c2.y / sz, c2.z / sz}}};
        if (mat3_determinant(rot_only) < 0.0f) {
            sx = -sx;
            // Re-normalize column 0 with the corrected sign.
            rot_only.m[0][0] = -rot_only.m[0][0];
            rot_only.m[0][1] = -rot_only.m[0][1];
            rot_only.m[0][2] = -rot_only.m[0][2];
        }
    } else {
        rot_only = mat3_identity();
    }

    *out_scale = (Vec3){sx, sy, sz};

    Mat4 rot4 = mat4_identity();
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 3; r++) {
            rot4.m[c][r] = rot_only.m[c][r];
        }
    }
    *out_rotation = quat_from_mat4(rot4);
}

/**
 * @brief Builds a tangent-space orthonormal basis around a surface normal.
 *
 * Returns two unit vectors perpendicular to @p normal (and to each other),
 * suitable for tangent/bitangent frames in normal mapping, TBN matrices,
 * and hemisphere sampling.
 *
 * @param normal Surface normal (normalized internally; zero-safe).
 * @return OrthonormalBasis with v0 = tangent, v1 = bitangent, v2 = normal.
 */
static inline OrthonormalBasis basis_from_normal(Vec3 normal) {
    SimdVec3 n = vec3_load(normal);
    float len_sq = vec3_length_sq(n);
    if (len_sq < 1e-12f) {
        return (OrthonormalBasis){(Vec3){1, 0, 0}, (Vec3){0, 1, 0}, (Vec3){0, 0, 1}};
    }
    n = vec3_mul(n, 1.0f / sqrtf(len_sq));

    // Pick the world axis the normal is LEAST parallel to so the cross
    // product is well-conditioned.
    SimdVec3 helper = (fabsf(n.x) < 0.9f) ? vec3_load((Vec3){1, 0, 0}) : vec3_load((Vec3){0, 1, 0});

    SimdVec3 tangent = vec3_normalize(vec3_cross(helper, n));
    SimdVec3 bitangent = vec3_cross(n, tangent);

    return (OrthonormalBasis){vec3_store(tangent), vec3_store(bitangent), vec3_store(n)};
}

/* ==================================================
   General Dense Matrix (row-major) & SVD
   ================================================== */

/**
 * @struct FMat
 * @brief Dynamically allocated dense float matrix in row-major order.
 *
 * Complements the fixed-size Mat3/Mat4 types when dimensions are only
 * known at runtime. Storage is a single malloc'd block; free it with
 * fmat_destroy(). All fmat_* functions are NOT thread-safe on the same
 * instance (like most container types here).
 */
typedef struct {
    size_t rows;
    size_t cols;
    float* data; /**< Row-major element block, rows * cols floats. */
} FMat;

/**
 * @brief Creates a zero-initialized rows x cols matrix.
 * @return The matrix; check .data == NULL to detect allocation failure.
 */
static inline FMat fmat_create(size_t rows, size_t cols) {
    FMat m = {rows, cols, NULL};
    if (rows > 0 && cols > 0) {
        m.data = (float*)calloc(rows * cols, sizeof(float));
    }
    return m;
}

/** @brief Frees the matrix's storage and resets it to an empty state. Safe on empty matrices. */
static inline void fmat_destroy(FMat* m) {
    if (m) {
        free(m->data);
        m->data = NULL;
        m->rows = 0;
        m->cols = 0;
    }
}

/** @brief True if the matrix holds allocated storage. */
static inline bool fmat_valid(const FMat* m) { return m && m->rows > 0 && m->cols > 0 && m->data != NULL; }

/**
 * @brief Element access (row, col), zero-indexed.
 * @warning No bounds checking — caller must ensure r < rows, c < cols.
 */
static inline float fmat_get(const FMat* m, size_t r, size_t c) { return m->data[r * m->cols + c]; }

/** @brief Element write (row, col), zero-indexed. */
static inline void fmat_set(FMat* m, size_t r, size_t c, float v) { m->data[r * m->cols + c] = v; }

/**
 * @brief Creates a new matrix initialized from a row-major value array.
 * @param values Must contain at least rows * cols floats.
 */
static inline FMat fmat_from_array(size_t rows, size_t cols, const float* values) {
    FMat m = fmat_create(rows, cols);
    if (m.data && values) {
        memcpy(m.data, values, rows * cols * sizeof(float));
    }
    return m;
}

/** @brief Deep copy of a matrix. Returns an invalid matrix if src is invalid or allocation fails. */
static inline FMat fmat_copy(const FMat* src) {
    if (!fmat_valid(src)) {
        FMat empty = {0, 0, NULL};
        return empty;
    }
    return fmat_from_array(src->rows, src->cols, src->data);
}

/** @brief n x n identity matrix. */
static inline FMat fmat_identity(size_t n) {
    FMat m = fmat_create(n, n);
    for (size_t i = 0; i < n; i++) {
        fmat_set(&m, i, i, 1.0f);
    }
    return m;
}

/** @brief Transpose of a matrix. Returns an invalid matrix if input is invalid. */
static inline FMat fmat_transpose(const FMat* m) {
    FMat t = {0, 0, NULL};
    if (!fmat_valid(m)) return t;
    t = fmat_create(m->cols, m->rows);
    if (!t.data) return t;
    for (size_t r = 0; r < m->rows; r++) {
        for (size_t c = 0; c < m->cols; c++) {
            fmat_set(&t, c, r, fmat_get(m, r, c));
        }
    }
    return t;
}

/**
 * @brief Matrix product a * b.
 * @return New matrix, or an invalid matrix ({NULL}) on dimension mismatch
 *         or allocation failure.
 */
static inline FMat fmat_mul(const FMat* a, const FMat* b) {
    FMat out = {0, 0, NULL};
    if (!fmat_valid(a) || !fmat_valid(b) || a->cols != b->rows) return out;
    out = fmat_create(a->rows, b->cols);
    if (!out.data) return out;
    for (size_t r = 0; r < a->rows; r++) {
        for (size_t k = 0; k < a->cols; k++) {
            const float aik = fmat_get(a, r, k);
            if (aik == 0.0f) continue;
            for (size_t c = 0; c < b->cols; c++) {
                fmat_set(&out, r, c, fmat_get(&out, r, c) + aik * fmat_get(b, k, c));
            }
        }
    }
    return out;
}

/* --- One-sided Jacobi SVD internals ------------------------------------- */

#define FMAT_SVD_MAX_SWEEPS 60

/** Computes the dot product of columns i and j of a p x q row-major matrix. */
static inline float fmat_svd_col_dot(const float* data, size_t q, size_t p, size_t i, size_t j) {
    float sum = 0.0f;
    for (size_t r = 0; r < p; r++) {
        sum += data[r * q + i] * data[r * q + j];
    }
    return sum;
}

/** Rotates columns i and j of both matrices by the Givens rotation [c, s; -s, c]. */
static inline void fmat_svd_rotate_cols(float* restrict b, float* restrict v, size_t q, size_t p, size_t i, size_t j,
                                        float c, float s) {
    for (size_t r = 0; r < p; r++) {
        const float bi = b[r * q + i];
        const float bj = b[r * q + j];
        b[r * q + i] = c * bi - s * bj;
        b[r * q + j] = s * bi + c * bj;
    }
    for (size_t r = 0; r < q; r++) {
        // v is q x q: column rotations touch full columns.
        const float vi = v[r * q + i];
        const float vj = v[r * q + j];
        v[r * q + i] = c * vi - s * vj;
        v[r * q + j] = s * vi + c * vj;
    }
}

/**
 * @brief Singular Value Decomposition of an arbitrary M x N matrix.
 *
 * Computes the thin SVD A = U * diag(S) * V^T using one-sided Jacobi
 * orthogonalization, which is backward stable, accurate to ~machine
 * epsilon relative to the largest singular value, and handles rank
 * deficiency gracefully.
 *
 * Output shapes (thin SVD, k = min(m, n)):
 *   - U: m x k with orthonormal columns (left singular vectors)
 *   - S: k x 1 singular values in descending order
 *   - V: n x k with orthonormal columns (right singular vectors)
 *
 * For rank-deficient inputs, columns of U whose singular value is below
 * max_sigma * 1e-6 (or zero sigma entirely) are zeroed instead of being
 * arbitrary completions of the basis.
 *
 * @param[in]  A Input matrix (must be valid).
 * @param[out] U_out Receives the m x k left factor.
 * @param[out] S_out Receives the k x 1 singular-value vector.
 * @param[out] V_out Receives the n x n right factor.
 * @return true on success; false on invalid input, allocation failure,
 *         or failure to converge within FMAT_SVD_MAX_SWEEPS sweeps.
 *
 * @note Complexity is O(sweeps * m * n * min(m,n)); fine for moderate
 *       sizes but not a substitute for LAPACK on very large matrices.
 *
 * @example
 *   FMat a = fmat_from_array(2, 2, (float[]){4, 0, 3, -5});
 *   FMat u, s, v;
 *   fmat_svd(&a, &u, &s, &v);      // a = u diag(s) v^T
 *   ...
 *   fmat_destroy(&a); fmat_destroy(&u); fmat_destroy(&s); fmat_destroy(&v);
 */
static inline bool fmat_svd(const FMat* A, FMat* U_out, FMat* S_out, FMat* V_out) {
    if (U_out) *U_out = (FMat){0, 0, NULL};
    if (S_out) *S_out = (FMat){0, 0, NULL};
    if (V_out) *V_out = (FMat){0, 0, NULL};
    if (!fmat_valid(A) || !U_out || !S_out || !V_out) return false;

    const size_t m = A->rows;
    const size_t n = A->cols;
    const size_t k = (m < n) ? m : n;

    /*
     * One-sided Jacobi needs tall-or-square input. For wide matrices,
     * decompose A^T instead: A^T = Ub S Vb^T  =>  A = Vb S Ub^T.
     */
    const bool transposed = (m < n);
    const size_t p = transposed ? n : m;  // working row count (>= q)
    const size_t q = k;                   // working column count

    FMat B = {0, 0, NULL};      // working copy of A (or A^T), p x q
    FMat Vacc = {0, 0, NULL};   // accumulated right rotations, q x q
    FMat work_U = {0, 0, NULL}, work_S = {0, 0, NULL}, work_V = {0, 0, NULL};
    FMat Uf = {0, 0, NULL}, Sf = {0, 0, NULL}, Vf = {0, 0, NULL}; // sorted factors
    size_t* order = (size_t*)malloc(q * sizeof(size_t));

    bool ok = false;
    do {
        if (!order) break;
        if (transposed) {
            FMat At = fmat_transpose(A);
            if (!fmat_valid(&At)) break;
            B = At;  // ownership moves to B
        } else {
            B = fmat_copy(A);
            if (!fmat_valid(&B)) break;
        }

        Vacc = fmat_identity(q);
        if (!fmat_valid(&Vacc)) break;

        /* Jacobi sweeps: rotate column pairs until all are mutually
         * orthogonal (off-diagonal entries of B^T B vanish). */
        const float ortho_tol = 1e-7f;
        bool converged = false;
        for (unsigned sweep = 0; sweep < FMAT_SVD_MAX_SWEEPS && !converged; sweep++) {
            converged = true;
            for (size_t i = 0; i + 1 < q; i++) {
                for (size_t j = i + 1; j < q; j++) {
                    const float alpha = fmat_svd_col_dot(B.data, q, p, i, i);
                    const float beta = fmat_svd_col_dot(B.data, q, p, j, j);
                    const float gamma = fmat_svd_col_dot(B.data, q, p, i, j);

                    // Zero (or denormal-tiny) columns are already orthogonal
                    // to everything; rotating them would stall convergence.
                    if (alpha < 1e-20f || beta < 1e-20f) continue;
                    if (fabsf(gamma) <= ortho_tol * sqrtf(alpha * beta)) continue;

                    converged = false;
                    // Rotation angle that zeroes gamma (Rutishauser).
                    const float zeta = (beta - alpha) / (2.0f * gamma);
                    const float t =
                        ((zeta >= 0.0f) ? 1.0f : -1.0f) / (fabsf(zeta) + sqrtf(1.0f + zeta * zeta));
                    const float c = 1.0f / sqrtf(1.0f + t * t);
                    const float s = c * t;
                    fmat_svd_rotate_cols(B.data, Vacc.data, q, p, i, j, c, s);
                }
            }
        }
        if (!converged) break;

        /* Extract singular values and normalize the working columns into U. */
        work_U = fmat_create(p, q);
        work_S = fmat_create(q, 1);
        work_V = fmat_copy(&Vacc);
        if (!fmat_valid(&work_U) || !fmat_valid(&work_S) || !fmat_valid(&work_V)) break;

        float sigma_max = 0.0f;
        for (size_t j = 0; j < q; j++) {
            float sum = 0.0f;
            for (size_t r = 0; r < p; r++) {
                const float val = B.data[r * q + j];
                sum += val * val;
                work_U.data[r * q + j] = val;
            }
            const float sigma = sqrtf(sum);
            work_S.data[j] = sigma;
            if (sigma > sigma_max) sigma_max = sigma;
        }

        // Normalize; near-zero directions get zeroed rather than arbitrary fill.
        const float rank_eps = sigma_max * 1e-6f;
        for (size_t j = 0; j < q; j++) {
            const float sigma = work_S.data[j];
            if (sigma <= rank_eps) {
                // Row-major storage: a column is strided, so zero element-wise.
                for (size_t r = 0; r < p; r++) {
                    work_U.data[r * q + j] = 0.0f;
                }
                continue;
            }
            const float inv = 1.0f / sigma;
            for (size_t r = 0; r < p; r++) {
                work_U.data[r * q + j] *= inv;
            }
        }

        /* Sort singular values (and matching vectors) in descending order. */
        for (size_t j = 0; j < q; j++) order[j] = j;
        for (size_t i = 0; i + 1 < q; i++) {
            size_t best = i;
            for (size_t j = i + 1; j < q; j++) {
                if (work_S.data[order[j]] > work_S.data[order[best]]) best = j;
            }
            if (best != i) {
                const size_t tswap = order[i];
                order[i] = order[best];
                order[best] = tswap;
            }
        }

        // Permute into final thin factors.
        Uf = fmat_create(p, q);
        Sf = fmat_create(q, 1);
        Vf = fmat_create(q, q);
        if (!fmat_valid(&Uf) || !fmat_valid(&Sf) || !fmat_valid(&Vf)) {
            break;
        }
        for (size_t jc = 0; jc < q; jc++) {
            const size_t src = order[jc];
            Sf.data[jc] = work_S.data[src];
            for (size_t r = 0; r < p; r++) {
                Uf.data[r * q + jc] = work_U.data[r * q + src];
            }
            for (size_t r = 0; r < q; r++) {
                Vf.data[r * q + jc] = work_V.data[r * q + src];
            }
        }

        if (transposed) {
            // A^T = Ub S Vb^T  =>  A = Vb S Ub^T.
            // Matching against A = U S V^T: U = Vb, V^T = Ub^T => V = Ub.
            *U_out = Vf;
            Vf = (FMat){0, 0, NULL};  // moved
            *S_out = Sf;
            Sf = (FMat){0, 0, NULL};  // moved
            *V_out = Uf;
            Uf = (FMat){0, 0, NULL};  // moved
        } else {
            *U_out = Uf;
            Uf = (FMat){0, 0, NULL};  // moved
            *S_out = Sf;
            Sf = (FMat){0, 0, NULL};  // moved
            *V_out = Vf;
            Vf = (FMat){0, 0, NULL};  // moved
        }
        ok = true;
    } while (0);

    /* Single unconditional cleanup. Every successful transfer zeroed its
     * source struct, so destroying the locals here can never double-free. */
    free(order);
    fmat_destroy(&B);
    fmat_destroy(&Vacc);
    fmat_destroy(&work_U);
    fmat_destroy(&work_S);
    fmat_destroy(&work_V);
    fmat_destroy(&Uf);
    fmat_destroy(&Sf);
    fmat_destroy(&Vf);
    return ok;
}

#ifdef __cplusplus
}
#endif

#endif  // LINEAR_ALG_H
