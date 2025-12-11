#ifndef LINEAR_ALG_H
#define LINEAR_ALG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "matrix.h"
#include "vec.h"

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
 *
 * @param v0 First input vector (will be primary axis).
 * @param v1 Second input vector (will be projected to be orthogonal).
 * @return OrthonormalBasis (Right-handed).
 */
static inline OrthonormalBasis orthonormalize(Vec3 v0, Vec3 v1) {
    // Load into SIMD registers
    SimdVec3 sv0 = vec3_load(v0);
    SimdVec3 sv1 = vec3_load(v1);

    // Normalize v0
    sv0 = vec3_normalize(sv0);

    // Project v1 onto v0 and subtract to make orthogonal: v1 = v1 - dot(v1, v0) * v0
    float dot = vec3_dot(sv0, sv1);
    sv1       = vec3_sub(sv1, vec3_mul(sv0, dot));
    sv1       = vec3_normalize(sv1);

    // Cross product for v2
    SimdVec3 sv2 = vec3_cross(sv0, sv1);

    // Store results
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
 *
 * @note Jacobi iteration is inherently scalar due to the need to access
 * pivoting elements at random indices (p, q). Vectorization overhead
 * usually exceeds benefits for 3x3.
 */
static inline EigenDecomposition mat3_eigen_symmetric(Mat3 A) {
    EigenDecomposition result;
    Mat3 V = mat3_identity();

    const int MAX_ITERS = 32;
    const float EPSILON = 1e-10f;

    for (int iter = 0; iter < MAX_ITERS; ++iter) {
        // Find largest off-diagonal absolute value
        int p = 0, q = 1;
        float max = fabsf(A.m[0][1]);

        if (fabsf(A.m[0][2]) > max) {
            p   = 0;
            q   = 2;
            max = fabsf(A.m[0][2]);
        }
        if (fabsf(A.m[1][2]) > max) {
            p   = 1;
            q   = 2;
            max = fabsf(A.m[1][2]);
        }

        if (max < EPSILON) break;

        float app = A.m[p][p];
        float aqq = A.m[q][q];
        float apq = A.m[p][q];

        float phi = 0.5f * atanf((2.0f * apq) / (aqq - app + 1e-20f));
        float c   = cosf(phi);
        float s   = sinf(phi);

        // Perform Jacobi rotation (Scalar)
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

        // Update eigenvectors
        for (int r = 0; r < 3; ++r) {
            float vrp = V.m[r][p];
            float vrq = V.m[r][q];
            V.m[r][p] = c * vrp - s * vrq;
            V.m[r][q] = s * vrp + c * vrq;
        }
    }

    result.eigenvalues  = (Vec3){A.m[0][0], A.m[1][1], A.m[2][2]};
    result.eigenvectors = V;
    return result;
}

/**
 * Computes Singular Value Decomposition (SVD) of a 3x3 matrix.
 * A = U * S * V^T
 */
static inline void mat3_svd(Mat3 A, Mat3* U, Vec3* S, Mat3* V) {
    // Step 1: Compute ATA = A^T * A
    // Manual multiplication is fastest for 3x3 here
    Mat3 ATA = {0};
    for (int i = 0; i < 3; ++i) {      // col of A^T (row of A)
        for (int j = 0; j < 3; ++j) {  // row of A^T (col of A)
            ATA.m[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                ATA.m[i][j] += A.m[i][k] * A.m[j][k];
            }
        }
    }

    // Step 2: Eigen decomposition of ATA
    EigenDecomposition ed = mat3_eigen_symmetric(ATA);
    *V                    = ed.eigenvectors;

    // Step 3: Sort eigenvalues/vectors (descending)
    int order[3]  = {0, 1, 2};
    float vals[3] = {ed.eigenvalues.x, ed.eigenvalues.y, ed.eigenvalues.z};

    for (int i = 0; i < 2; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (vals[order[i]] < vals[order[j]]) {
                int tmp  = order[i];
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

    // Step 4: Compute Singular Values
    S->x = sqrtf(fmaxf(0.0f, sorted_eigen.x));
    S->y = sqrtf(fmaxf(0.0f, sorted_eigen.y));
    S->z = sqrtf(fmaxf(0.0f, sorted_eigen.z));

    // Step 5: Compute U = A * V * S^-1
    // We treat U columns as SimdVec3 for calculation
    SimdVec3 colA[3] = {vec3_load((Vec3){A.m[0][0], A.m[0][1], A.m[0][2]}),
                        vec3_load((Vec3){A.m[1][0], A.m[1][1], A.m[1][2]}),
                        vec3_load((Vec3){A.m[2][0], A.m[2][1], A.m[2][2]})};

    float* s_arr = (float*)S;
    for (int i = 0; i < 3; ++i) {  // For each column of U
        float sigma = s_arr[i];
        if (sigma > 1e-6f) {
            // U_col_i = (A * V_col_i) / sigma
            Vec3 v_col = {V->m[i][0], V->m[i][1], V->m[i][2]};
            SimdVec3 Av =
                vec3_add(vec3_add(vec3_mul(colA[0], v_col.x), vec3_mul(colA[1], v_col.y)), vec3_mul(colA[2], v_col.z));
            SimdVec3 u_col = vec3_mul(Av, 1.0f / sigma);

            // Store back to matrix
            Vec3 res   = vec3_store(u_col);
            U->m[i][0] = res.x;
            U->m[i][1] = res.y;
            U->m[i][2] = res.z;
        } else {
            U->m[i][0] = U->m[i][1] = U->m[i][2] = 0.0f;
        }
    }

    // Step 6: Orthonormalize U if rank-deficient (using Cross Product)
    if (S->y > 1e-6f && S->z < 1e-6f) {
        SimdVec3 u0 = vec3_load((Vec3){U->m[0][0], U->m[0][1], U->m[0][2]});
        SimdVec3 u1 = vec3_load((Vec3){U->m[1][0], U->m[1][1], U->m[1][2]});
        SimdVec3 u2 = vec3_cross(u0, u1);  // U2 = U0 x U1

        Vec3 res   = vec3_store(u2);
        U->m[2][0] = res.x;
        U->m[2][1] = res.y;
        U->m[2][2] = res.z;
    }

    // Step 7: Fix det(U) = +1
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
    // Load columns into SIMD registers
    SimdVec4 v[4], q[4];
    for (int i = 0; i < 4; ++i) {
        v[i].v = A.cols[i];  // Direct access to underlying SIMD type
    }

    for (int i = 0; i < 4; ++i) {
        q[i] = v[i];
        for (int j = 0; j < i; ++j) {
            float r    = vec4_dot(q[j], v[i]);
            R->m[j][i] = r;

            // q[i] = q[i] - r * q[j]
            q[i] = vec4_sub(q[i], vec4_mul(q[j], r));
        }

        float norm = vec4_length(q[i]);
        if (norm < 1e-6f) norm = 1e-6f;

        R->m[i][i] = norm;
        q[i]       = vec4_mul(q[i], 1.0f / norm);
    }

    // Store Q columns
    for (int i = 0; i < 4; ++i) {
        Q->cols[i] = q[i].v;
    }

    // Zero lower triangle of R
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < i; ++j)
            R->m[i][j] = 0.0f;
}

/**
 * Computes the dominant eigenpair of a 4x4 matrix using SIMD power iteration.
 */
static inline void mat4_power_iteration(Mat4 A, Vec4* eigenvector, float* eigenvalue, int max_iter, float tol) {
    SimdVec4 v = vec4_load((Vec4){1.0f, 0.0f, 0.0f, 0.0f});
    SimdVec4 Av;
    float lambda_old = 0.0f;

    for (int iter = 0; iter < max_iter; ++iter) {
        // Av = A * v (SIMD matrix-vector mul)
        // We cast A to Mat4 to use the function from matrix.h
        Vec4 v_scalar  = vec4_store(v);
        Vec4 Av_scalar = mat4_mul_vec4(A, v_scalar);
        Av             = vec4_load(Av_scalar);

        // lambda = dot(Av, v)
        *eigenvalue = vec4_dot(Av, v);

        // Normalize
        Av = vec4_normalize(Av);

        if (fabsf(*eigenvalue - lambda_old) < tol) {
            break;
        }
        lambda_old = *eigenvalue;
        v          = Av;
    }

    *eigenvector = vec4_store(v);
}

/**
 * Computes the Frobenius norm of a 4x4 matrix using SIMD.
 */
static inline float mat4_norm_frobenius(Mat4 A) {
    // Sum of squares of all elements = Sum of dot product of each column with itself
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
    // 1. Leading principal minor 1: A[0][0]
    if (A.m[0][0] <= 0.0f) return false;

    // 2. Leading principal minor 2: det 2x2
    float det2 = A.m[0][0] * A.m[1][1] - A.m[1][0] * A.m[0][1];
    if (det2 <= 0.0f) return false;

    // 3. Leading principal minor 3: det 3x3
    if (mat3_determinant(A) <= 0.0f) return false;

    return true;
}

static inline float mat4_condition_number(Mat4 A) {
    float norm_A = mat4_norm_frobenius(A);
    Mat4 A_inv   = mat4_inverse(A);

    if (mat4_equal(A_inv, mat4_identity())) {
        return FLT_MAX;  // Singular
    }

    float norm_A_inv = mat4_norm_frobenius(A_inv);
    return norm_A * norm_A_inv;
}

// --- LU Decomposition & Solving (Scalar fallback for stability) ---

static inline bool mat4_lu(Mat4 A, Mat4* L, Mat4* U, Mat4* P) {
    const float tolerance = 1e-6f;
    *U                    = A;

    // Identity L and P
    *L = mat4_identity();
    *P = mat4_identity();

    for (int k = 0; k < 4; ++k) {
        // Pivot
        int pivot_row = k;
        float max_val = fabsf(U->m[k][k]);
        for (int i = k + 1; i < 4; ++i) {
            float val = fabsf(U->m[k][i]);
            if (val > max_val) {
                max_val   = val;
                pivot_row = i;
            }
        }

        if (max_val < tolerance) return false;

        // Swap Rows
        if (pivot_row != k) {
            for (int j = 0; j < 4; ++j) {
                float tmp;
                tmp                = U->m[j][k];
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

        // Eliminate
        for (int i = k + 1; i < 4; ++i) {
            float factor = U->m[k][i] / U->m[k][k];
            L->m[k][i]   = factor;
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
    x_arr[0]     = b.x / L.m[0][0];
    x_arr[1]     = (b.y - L.m[0][1] * x_arr[0]) / L.m[1][1];
    x_arr[2]     = (b.z - (L.m[0][2] * x_arr[0] + L.m[1][2] * x_arr[1])) / L.m[2][2];
    x_arr[3]     = (b.w - (L.m[0][3] * x_arr[0] + L.m[1][3] * x_arr[1] + L.m[2][3] * x_arr[2])) / L.m[3][3];
    return x;
}

static inline Vec4 backward_substitution_mat4(Mat4 U, Vec4 b) {
    Vec4 x;
    float* x_arr = &x.x;
    x_arr[3]     = b.w / U.m[3][3];
    x_arr[2]     = (b.z - U.m[3][2] * x_arr[3]) / U.m[2][2];
    x_arr[1]     = (b.y - (U.m[2][1] * x_arr[2] + U.m[3][1] * x_arr[3])) / U.m[1][1];
    x_arr[0]     = (b.x - (U.m[1][0] * x_arr[1] + U.m[2][0] * x_arr[2] + U.m[3][0] * x_arr[3])) / U.m[0][0];
    return x;
}

static inline Vec4 mat4_solve(Mat4 A, Vec4 b) {
    Mat4 L, U, P;
    if (!mat4_lu(A, &L, &U, &P)) {
        return (Vec4){0};
    }

    // Permute b: Pb
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

#ifdef __cplusplus
}
#endif

#endif  // LINEAR_ALG_H
