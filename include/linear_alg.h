#ifndef LINEAR_ALG_H
#define LINEAR_ALG_H

#include "matrix.h"
#include "vec.h"

typedef struct {
    Vec3 v0, v1, v2;
} OrthonormalBasis;

// Gram-Schmidt orthogonalization
static inline OrthonormalBasis orthonormalize(Vec3 v0, Vec3 v1) {
    v0 = vec3_normalize(v0);

    float dot = vec3_dot(v0, v1);
    v1        = (Vec3){v1.x - dot * v0.x, v1.y - dot * v0.y, v1.z - dot * v0.z};
    v1        = vec3_normalize(v1);

    Vec3 v2 = vec3_cross(v0, v1);

    return (OrthonormalBasis){v0, v1, v2};
}

typedef struct {
    Vec3 eigenvalues;
    Mat3 eigenvectors;  // columns are eigenvectors
} EigenDecomposition;

/**
 * Computes eigenvalues/vectors of symmetric 3x3 matrix
 * @param A Input symmetric Mat3
 * @return  EigenDecomposition struct with eigen vectors and values.
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

        // Perform Jacobi rotation
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

static inline void mat3_svd(Mat3 A, Mat3* U, Vec3* S, Mat3* V) {
    // Step 1: Compute ATA = A^T * A (column-major aware)
    Mat3 ATA = {};
    for (int i = 0; i < 3; ++i) {      // column of A^T (row of A)
        for (int j = 0; j < 3; ++j) {  // row of A^T (column of A)
            ATA.m[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                ATA.m[i][j] += A.m[i][k] * A.m[j][k];  // A^T[i][k] * A[j][k]
            }
        }
    }

    // Step 2: Eigen decomposition of ATA
    EigenDecomposition ed = mat3_eigen_symmetric(ATA);
    *V                    = ed.eigenvectors;

    // Step 3: Sort eigenvalues and corresponding V columns (descending)
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

    Mat3 V_sorted = {};
    for (int i = 0; i < 3; ++i) {                  // output column
        for (int j = 0; j < 3; ++j) {              // row
            V_sorted.m[i][j] = V->m[order[i]][j];  // column-major: V[col][row]
        }
    }
    *V = V_sorted;

    // Step 4: Compute singular values
    S->x = sqrtf(fmaxf(0.0f, sorted_eigen.x));
    S->y = sqrtf(fmaxf(0.0f, sorted_eigen.y));
    S->z = sqrtf(fmaxf(0.0f, sorted_eigen.z));

    // Step 5: Compute U = A * V * S^-1
    for (int i = 0; i < 3; ++i) {  // For each singular value/vector
        float sigma = ((float*)S)[i];
        if (sigma > 1e-6f) {
            for (int j = 0; j < 3; ++j) {  // row
                U->m[i][j] =
                    (A.m[0][j] * V->m[i][0] + A.m[1][j] * V->m[i][1] + A.m[2][j] * V->m[i][2]) /
                    sigma;
            }
        } else {
            U->m[i][0] = U->m[i][1] = U->m[i][2] = 0.0f;
        }
    }

    // Step 6: Orthonormalize U if rank-deficient
    float sigma2 = S->y;
    float sigma3 = S->z;
    if (sigma2 > 1e-6f && sigma3 < 1e-6f) {
        // Compute U3 = U1 × U2
        Vec3 u0 = {U->m[0][0], U->m[0][1], U->m[0][2]};
        Vec3 u1 = {U->m[1][0], U->m[1][1], U->m[1][2]};
        Vec3 u2 = {u0.y * u1.z - u0.z * u1.y, u0.z * u1.x - u0.x * u1.z, u0.x * u1.y - u0.y * u1.x};
        U->m[2][0] = u2.x;
        U->m[2][1] = u2.y;
        U->m[2][2] = u2.z;
    }

    // Step 7: Fix det(U) = +1
    if (mat3_determinant(*U) < 0.0f) {
        for (int j = 0; j < 3; ++j)
            U->m[2][j] = -U->m[2][j];
    }
}

/**
 * QR Decomposition for 4x4 matrix
 * A = Q * R
 * @param A Input Mat4
 * @param Q Output orthogonal Mat4
 * @param R Output upper triangular Mat4
 */
static inline void mat4_qr(Mat4 A, Mat4* Q, Mat4* R) {
    Vec4 v[4], q[4];

    // Extract column vectors from A
    for (int i = 0; i < 4; ++i) {
        v[i].x = A.m[0][i];
        v[i].y = A.m[1][i];
        v[i].z = A.m[2][i];
        v[i].w = A.m[3][i];
    }

    for (int i = 0; i < 4; ++i) {
        q[i] = v[i];
        for (int j = 0; j < i; ++j) {
            float r    = vec4_dot(q[j], v[i]);
            R->m[j][i] = r;
            q[i]       = vec4_sub(q[i], vec4_scale(q[j], r));
        }
        float norm = vec4_length(q[i]);
        if (norm < 1e-6f) norm = 1e-6f;
        R->m[i][i] = norm;
        q[i]       = vec4_scale(q[i], 1.0f / norm);
    }

    // Form Q from orthonormal q vectors (as columns)
    for (int i = 0; i < 4; ++i) {
        Q->m[0][i] = q[i].x;
        Q->m[1][i] = q[i].y;
        Q->m[2][i] = q[i].z;
        Q->m[3][i] = q[i].w;
    }

    // Fill lower triangle of R with 0
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < i; ++j)
            R->m[i][j] = 0.0f;
}

/**
 * Power iteration for dominant eigenpair of 4x4 matrix
 * @param A Input Mat4
 * @param eigenvector Output Vec4
 * @param eigenvalue Output float
 * @param max_iter Maximum iterations
 * @param tol Tolerance
 */
static inline void mat4_power_iteration(Mat4 A, Vec4* eigenvector, float* eigenvalue, int max_iter,
                                        float tol) {
    Vec4 v = {1.0f, 0.0f, 0.0f, 0.0f};  // Start with an arbitrary vector
    Vec4 Av;
    float lambda_old = 0.0f;

    for (int iter = 0; iter < max_iter; ++iter) {
        // Multiply matrix A by the eigenvector
        Av = mat4_mul_vec4(A, v);

        // Compute the Rayleigh quotient (approximation of eigenvalue)
        *eigenvalue = vec4_dot(Av, v);  // Eigenvalue is the dot product of Av and v

        // Normalize the resulting vector
        vec4_normalize(Av);

        // Check for convergence
        if (fabsf(*eigenvalue - lambda_old) < tol) {
            break;
        }

        lambda_old = *eigenvalue;

        // Update the eigenvector
        v = Av;
    }

    // Final eigenvector is stored in v
    *eigenvector = v;
}

/**
 * Computes Frobenius norm of 4x4 matrix
 */
static inline float mat4_norm_frobenius(Mat4 A) {
    float norm = 0.0f;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            norm += A.m[i][j] * A.m[i][j];
        }
    }
    return sqrtf(norm);
}

/**
 * Checks if 3x3 matrix is positive definite
 */
static inline bool mat3_is_positive_definite(Mat3 A) {
    float det1 = mat3_determinant(A);
    if (det1 <= 0.0f) return false;

    Mat3 A1    = A;
    A1.m[2][0] = A1.m[2][1] = A1.m[2][2] = 0.0f;

    float det2 = mat3_determinant(A1);
    if (det2 <= 0.0f) return false;

    return true;
}

/**
 * Computes condition number of 4x4 matrix
 */
static inline float mat4_condition_number(Mat4 A) {
    Mat4 A_inv;
    float norm_A = 0, norm_A_inv = 0;

    // Compute the Frobenius norm of A
    norm_A = mat4_norm_frobenius(A);

    // Compute the inverse of A
    A_inv = mat4_inverse(A);

    if (mat4_equal(A_inv, mat4_identity())) {
        return FLT_MAX;  // Matrix is singular, condition number is infinite
    }

    // Compute the Frobenius norm of the inverse
    norm_A_inv = mat4_norm_frobenius(A_inv);

    return norm_A * norm_A_inv;
}

/**
 * Solves the linear system Ax = b using LU decomposition (PA = LU).
 * Assumes column-major matrices.
 *
 * 1. PA = LU
 * 2. Ax = b  =>  (P^-1 LU)x = b  =>  LUx = Pb
 * 3. Let y = Ux, then Ly = Pb
 * 4. Solve Ly = Pb for y using forward substitution
 * 5. Solve Ux = y for x using backward substitution
 *
 * @param A Input matrix (column-major)
 * @param b Input vector
 * @return  Solution vector x
 */
static inline Vec3 mat3_solve(Mat3 A, Vec3 b) {
    Mat3 L, U, P;
    if (!mat3_lu(A, &L, &U, &P)) {
        Vec3 zero = {0, 0, 0};
        return zero;
    }

    // Column-major correct permutation: Pb = P * b
    Vec3 Pb;
    Pb.x = P.m[0][0] * b.x + P.m[1][0] * b.y + P.m[2][0] * b.z;
    Pb.y = P.m[0][1] * b.x + P.m[1][1] * b.y + P.m[2][1] * b.z;
    Pb.z = P.m[0][2] * b.x + P.m[1][2] * b.y + P.m[2][2] * b.z;

    Vec3 y = forward_substitution_mat3(L, Pb);
    Vec3 x = backward_substitution_mat3(U, y);
    return x;
}

// --- Substitution Functions ---

/**
 * Forward substitution for solving Lx = b, where L is lower triangular.
 * Assumes L is column-major.
 * @param L Lower triangular matrix (column-major)
 * @param b Right-hand side vector
 * @return Solution vector x
 */
static inline Vec4 forward_substitution_mat4(Mat4 L, Vec4 b) {
    Vec4 x;
    float* x_arr = &x.x;  // Treat Vec4 as an array for loop

    // Solve Ly = Pb for y
    // L is lower triangular. Solve row by row.
    // x_i = (b_i - sum(L_i,j * x_j for j < i)) / L_i,i
    // In column-major, L_i,j is L.m[j][i]
    // x_i = (b_i - sum(L.m[j][i] * x_j for j from 0 to i-1)) / L.m[i][i]

    // i = 0
    x_arr[0] = b.x / L.m[0][0];

    // i = 1
    x_arr[1] = (b.y - L.m[0][1] * x_arr[0]) / L.m[1][1];

    // i = 2
    x_arr[2] = (b.z - (L.m[0][2] * x_arr[0] + L.m[1][2] * x_arr[1])) / L.m[2][2];

    // i = 3
    x_arr[3] =
        (b.w - (L.m[0][3] * x_arr[0] + L.m[1][3] * x_arr[1] + L.m[2][3] * x_arr[2])) / L.m[3][3];

    return x;
}

/**
 * Backward substitution for solving Ux = b, where U is upper triangular.
 * Assumes U is column-major.
 * @param U Upper triangular matrix (column-major)
 * @param b Right-hand side vector
 * @return Solution vector x
 */
static inline Vec4 backward_substitution_mat4(Mat4 U, Vec4 b) {
    Vec4 x;
    float* x_arr = &x.x;  // Treat Vec4 as an array for loop

    // Solve Ux = y for x
    // U is upper triangular. Solve row by row from bottom up.
    // x_i = (b_i - sum(U_i,j * x_j for j > i)) / U_i,i
    // In column-major, U_i,j is U.m[j][i]
    // x_i = (b_i - sum(U.m[j][i] * x_j for j from i+1 to 3)) / U.m[i][i]

    // i = 3
    x_arr[3] = b.w / U.m[3][3];  // U.m[col][row] -> U.m[3][3] is U_3,3

    // i = 2
    x_arr[2] = (b.z - U.m[3][2] * x_arr[3]) / U.m[2][2];  // U.m[3][2] is U_2,3; U.m[2][2] is U_2,2

    // i = 1
    x_arr[1] = (b.y - (U.m[2][1] * x_arr[2] + U.m[3][1] * x_arr[3])) /
               U.m[1][1];  // U.m[2][1] is U_1,2; U.m[3][1] is U_1,3; U.m[1][1] is U_1,1

    // i = 0
    x_arr[0] = (b.x - (U.m[1][0] * x_arr[1] + U.m[2][0] * x_arr[2] + U.m[3][0] * x_arr[3])) /
               U.m[0][0];  // U.m[1][0] is U_0,1; U.m[2][0] is U_0,2; U.m[3][0] is
                           // U_0,3; U.m[0][0] is U_0,0

    return x;
}

// --- LU Decomposition ---

/**
 * LU Decomposition with partial pivoting for a 4x4 matrix.
 * Assumes column-major matrices.
 *
 * @param A  Input matrix (column-major)
 * @param L  Output lower triangular matrix (column-major)
 * @param U  Output upper triangular matrix (column-major)
 * @param P  Output permutation matrix (column-major)
 * @return   True if decomposition successful, false if matrix is singular.
 */
static inline bool mat4_lu(Mat4 A, Mat4* L, Mat4* U, Mat4* P) {
    const float tolerance = 1e-6f;

    // Initialize U to A (assuming A is already column-major)
    *U = A;

    // Initialize L to identity (column-major identity: m[col][row] is 1 if
    // col==row)
    for (int i = 0; i < 4; ++i) {                 // row index
        for (int j = 0; j < 4; ++j) {             // column index
            L->m[j][i] = (i == j) ? 1.0f : 0.0f;  // m[col][row] = (row == col) ? 1 : 0
        }
    }

    // Initialize P to identity (column-major identity)
    for (int i = 0; i < 4; ++i) {                 // row index
        for (int j = 0; j < 4; ++j) {             // column index
            P->m[j][i] = (i == j) ? 1.0f : 0.0f;  // m[col][row] = (row == col) ? 1 : 0
        }
    }

    // LU Decomposition with partial pivoting
    for (int k = 0; k < 4; ++k) {  // k is the column index being processed

        // Find pivot row (find max absolute value in column k, from row k
        // downwards)
        int pivot_row = k;                  // mathematical row index
        float max_val = fabsf(U->m[k][k]);  // U.m[col][row] -> U.m[k][k] is U_k,k
        for (int i = k + 1; i < 4; ++i) {   // i is the mathematical row index below k
            float val = fabsf(U->m[k][i]);  // U.m[col][row] -> U.m[k][i] is U_i,k
            if (val > max_val) {
                max_val   = val;
                pivot_row = i;
            }
        }

        if (max_val < tolerance) {
            // mat4_print(*U, "Singular U"); // Debugging helper
            return false;  // Singular matrix
        }

        // Swap rows in U and P if needed
        if (pivot_row != k) {
            for (int j = 0; j < 4; ++j) {  // j is the column index
                // Swap U elements in math rows k and pivot_row, across all columns j
                // U.m[j][k] is U_k,j ; U.m[j][pivot_row] is U_pivot_row,j
                float tmp_U        = U->m[j][k];
                U->m[j][k]         = U->m[j][pivot_row];
                U->m[j][pivot_row] = tmp_U;

                // Swap P elements in math rows k and pivot_row, across all columns j
                // P.m[j][k] is P_k,j ; P.m[j][pivot_row] is P_pivot_row,j
                float tmp_P        = P->m[j][k];
                P->m[j][k]         = P->m[j][pivot_row];
                P->m[j][pivot_row] = tmp_P;

                // Swap L elements in math rows k and pivot_row, but ONLY for columns j
                // < k L.m[j][k] is L_k,j ; L.m[j][pivot_row] is L_pivot_row,j
                if (j < k) {
                    float tmp_L        = L->m[j][k];
                    L->m[j][k]         = L->m[j][pivot_row];
                    L->m[j][pivot_row] = tmp_L;
                }
            }
        }

        // Elimination for rows i below pivot row k
        for (int i = k + 1; i < 4; ++i) {  // i is the mathematical row index below k
            // Calculate factor for math row i in column k
            // U.m[k][i] is U_i,k ; U.m[k][k] is U_k,k
            float factor = U->m[k][i] / U->m[k][k];

            // Store factor in L (L_i,k = factor). L.m[col][row] -> L.m[k][i] stores
            // L_i,k
            L->m[k][i] = factor;

            // Perform row operation on U: U_i,j = U_i,j - factor * U_k,j for j from k
            // to 3
            for (int j = k; j < 4; ++j) {  // j is the column index
                // U.m[j][i] is U_i,j ; U.m[j][k] is U_k,j
                U->m[j][i] -= factor * U->m[j][k];
            }
        }
    }

    return true;
}

/**
 * Solves the linear system Ax = b using LU decomposition (PA = LU).
 * Assumes column-major matrices.
 *
 * 1. PA = LU
 * 2. Ax = b  =>  (P^-1 LU)x = b  =>  LUx = Pb
 * 3. Let y = Ux, then Ly = Pb
 * 4. Solve Ly = Pb for y using forward substitution
 * 5. Solve Ux = y for x using backward substitution
 *
 * @param A Input matrix (column-major)
 * @param b Input vector
 * @return  Solution vector x, or a zero vector if A is singular.
 */
static inline Vec4 mat4_solve(Mat4 A, Vec4 b) {
    Mat4 L, U, P;
    if (!mat4_lu(A, &L, &U, &P)) {
        Vec4 zero = {0.0f, 0.0f, 0.0f, 0.0f};
        return zero;  // Matrix is singular
    }

    // Optional: Print L, U, P for debugging
    // mat4_print(L, "L");
    // mat4_print(U, "U");
    // mat4_print(P, "P");

    // Permute the right-hand side vector: Pb = P * b
    // Assuming P is column-major: Pb_i = sum(P_i,j * b_j for j from 0 to 3)
    // P_i,j is stored at P.m[j][i]
    Vec4 Pb;
    Pb.x = P.m[0][0] * b.x + P.m[1][0] * b.y + P.m[2][0] * b.z +
           P.m[3][0] * b.w;  // Pb_0 = P_00*b0 + P_01*b1 + P_02*b2 + P_03*b3
    Pb.y = P.m[0][1] * b.x + P.m[1][1] * b.y + P.m[2][1] * b.z +
           P.m[3][1] * b.w;  // Pb_1 = P_10*b0 + P_11*b1 + P_12*b2 + P_13*b3
    Pb.z = P.m[0][2] * b.x + P.m[1][2] * b.y + P.m[2][2] * b.z +
           P.m[3][2] * b.w;  // Pb_2 = P_20*b0 + P_21*b1 + P_22*b2 + P_23*b3
    Pb.w = P.m[0][3] * b.x + P.m[1][3] * b.y + P.m[2][3] * b.z +
           P.m[3][3] * b.w;  // Pb_3 = P_30*b0 + P_31*b1 + P_32*b2 + P_33*b3

    // Solve Ly = Pb for y using forward substitution
    Vec4 y = forward_substitution_mat4(L, Pb);
    // vec4_print(y, "y"); // Optional: Print y for debugging

    // Solve Ux = y for x using backward substitution
    Vec4 x = backward_substitution_mat4(U, y);

    return x;
}

#endif /* LINEAR_ALG_H */
