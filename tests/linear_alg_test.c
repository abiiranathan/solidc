#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/linear_alg.h"
#include "../include/matrix.h"
#include "../include/simd.h"

// Tolerances
#define TIGHT_EPSILON 1e-5f
#define LOOSE_EPSILON 1e-3f  // iterative algos (Eigen/SVD) need looser tolerance

// Colors
#define ANSI_COLOR_RED    "\x1b[31m"
#define ANSI_COLOR_GREEN  "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE   "\x1b[34m"
#define ANSI_COLOR_RESET  "\x1b[0m"

int g_tests_passed = 0;
int g_tests_failed = 0;

/* ==================================================
   Helpers
   ================================================== */

void print_header(const char* name) {
    printf("\n" ANSI_COLOR_YELLOW "=== Testing %s ===" ANSI_COLOR_RESET "\n", name);
}

void assert_bool(const char* name, bool condition) {
    if (!condition) {
        printf(ANSI_COLOR_RED "[FAIL] %s\n" ANSI_COLOR_RESET, name);
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

void assert_float_eq(const char* name, float expected, float actual, float tol) {
    if (fabsf(expected - actual) > tol) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Expected %.4f, Got %.4f (Diff: %.4f)\n" ANSI_COLOR_RESET, name, expected,
               actual, fabsf(expected - actual));
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

#define EPSILON 0.0001f
void assert_vec3_eq(const char* name, Vec3 expected, SimdVec3 actual_simd) {
    Vec3 actual = vec3_store(actual_simd);
    if (fabsf(expected.x - actual.x) > EPSILON || fabsf(expected.y - actual.y) > EPSILON ||
        fabsf(expected.z - actual.z) > EPSILON) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Expected (%.2f, %.2f, %.2f), got (%.2f, %.2f, %.2f)\n" ANSI_COLOR_RESET, name,
               expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

void assert_vec4_eq(const char* name, Vec4 expected, SimdVec4 actual_simd) {
    Vec4 actual = vec4_store(actual_simd);
    if (fabsf(expected.x - actual.x) > EPSILON || fabsf(expected.y - actual.y) > EPSILON ||
        fabsf(expected.z - actual.z) > EPSILON || fabsf(expected.w - actual.w) > EPSILON) {
        printf(ANSI_COLOR_RED
               "[FAIL] %s: Expected (%.2f, %.2f, %.2f, %.2f), got (%.2f, %.2f, %.2f, %.2f)\n" ANSI_COLOR_RESET,
               name, expected.x, expected.y, expected.z, expected.w, actual.x, actual.y, actual.z, actual.w);
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

bool vec3_is_close(Vec3 a, Vec3 b, float tol) {
    return fabsf(a.x - b.x) < tol && fabsf(a.y - b.y) < tol && fabsf(a.z - b.z) < tol;
}

bool mat3_is_close(Mat3 a, Mat3 b, float tol) {
    for (int i = 0; i < 3; i++)      // col
        for (int j = 0; j < 3; j++)  // row
            if (fabsf(a.m[i][j] - b.m[i][j]) > tol) return false;
    return true;
}

bool mat4_is_close(Mat4 a, Mat4 b, float tol) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (fabsf(a.m[i][j] - b.m[i][j]) > tol) return false;
    return true;
}

void assert_mat4_eq(const char* name, Mat4 expected, Mat4 actual) {
    if (!mat4_is_close(expected, actual, EPSILON)) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Matrix mismatch\n" ANSI_COLOR_RESET, name);
        printf("Expected:\n");
        mat4_print(expected, "  E");
        printf("Got:\n");
        mat4_print(actual, "  A");
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

/* ==================================================
   Tests
   ================================================== */

void test_orthonormalize() {
    print_header("Orthonormal Basis (Gram-Schmidt)");

    // Input: Forward and a generic "Up" that isn't perfectly perpendicular
    Vec3 v0 = {0.0f, 0.0f, 1.0f};  // Forward
    Vec3 v1 = {0.1f, 1.0f, 0.5f};  // Roughly Up, but skewed

    OrthonormalBasis basis = orthonormalize(v0, v1);

    // 1. Check lengths
    assert_float_eq("Basis v0 Length", 1.0f, vec3_length(vec3_load(basis.v0)), TIGHT_EPSILON);
    assert_float_eq("Basis v1 Length", 1.0f, vec3_length(vec3_load(basis.v1)), TIGHT_EPSILON);
    assert_float_eq("Basis v2 Length", 1.0f, vec3_length(vec3_load(basis.v2)), TIGHT_EPSILON);

    // 2. Check Orthogonality (Dot products should be 0)
    float d01 = vec3_dot(vec3_load(basis.v0), vec3_load(basis.v1));
    float d02 = vec3_dot(vec3_load(basis.v0), vec3_load(basis.v2));
    float d12 = vec3_dot(vec3_load(basis.v1), vec3_load(basis.v2));

    assert_float_eq("Dot(v0, v1) == 0", 0.0f, d01, TIGHT_EPSILON);
    assert_float_eq("Dot(v0, v2) == 0", 0.0f, d02, TIGHT_EPSILON);
    assert_float_eq("Dot(v1, v2) == 0", 0.0f, d12, TIGHT_EPSILON);
}

void test_eigen_symmetric() {
    print_header("Eigen Decomposition (Symmetric 3x3)");

    Mat3 A                = mat3_new_column_major(2, 1, 0, 1, 2, 0, 0, 0, 3);
    EigenDecomposition ed = mat3_eigen_symmetric(A);

    // FIX: Extract column 0 for the eigenvector, not row 0.
    // Eigenvectors are stored as columns in V.
    Vec3 v0       = {ed.eigenvectors.m[0][0], ed.eigenvectors.m[1][0], ed.eigenvectors.m[2][0]};
    float lambda0 = ed.eigenvalues.x;

    Vec3 Av0     = mat3_mul_vec3(A, v0);
    SimdVec3 Lv0 = vec3_mul(vec3_load(v0), lambda0);  // lambda * v
    Vec3 Lv0Vec  = vec3_store(Lv0);

    if (vec3_is_close(Av0, Lv0Vec, LOOSE_EPSILON)) {
        printf(ANSI_COLOR_GREEN "[PASS] A*v0 == lambda*v0\n" ANSI_COLOR_RESET);
        g_tests_passed++;
    } else {
        printf(ANSI_COLOR_RED "[FAIL] Eigen verification failed.\n" ANSI_COLOR_RESET);
        printf("Lambda: %f\n", lambda0);
        vec3_print(Av0, "A*v");
        vec3_print(Lv0Vec, "L*v");
        g_tests_failed++;
    }

    Vec3 v1 = {ed.eigenvectors.m[0][1], ed.eigenvectors.m[1][1], ed.eigenvectors.m[2][1]};  // Also fix v1 extraction
    assert_float_eq("Eigenvectors Orthogonal", 0.0f, vec3_dot(vec3_load(v0), vec3_load(v1)), LOOSE_EPSILON);
}

void test_svd() {
    print_header("SVD (3x3)");

    Mat3 A = mat3_new_column_major(1, 2, 3, 4, 5, 6, 7, 8, 9);

    Mat3 U, V;
    Vec3 S;
    mat3_svd(A, &U, &S, &V);

    // Reconstruct: A_recon = U * Diagonal(S) * V^T
    Mat3 S_mat = mat3_new_column_major(S.x, 0, 0, 0, S.y, 0, 0, 0, S.z);

    // Transpose V manually for check
    Mat3 Vt;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            Vt.m[i][j] = V.m[j][i];

    Mat3 US    = mat3_mul(U, S_mat);
    Mat3 Recon = mat3_mul(US, Vt);

    if (mat3_is_close(A, Recon, LOOSE_EPSILON)) {
        printf(ANSI_COLOR_GREEN "[PASS] SVD Reconstruction (U*S*Vt == A)\n" ANSI_COLOR_RESET);
        g_tests_passed++;
    } else {
        printf(ANSI_COLOR_RED "[FAIL] SVD Reconstruction\n" ANSI_COLOR_RESET);
        mat3_print(A, "Original");
        mat3_print(Recon, "Reconstructed");
        g_tests_failed++;
    }
}

void test_qr() {
    print_header("QR Decomposition (4x4)");

    Mat4 A = mat4_new_column_major(12, -51, 4, 1, 6, 167, -68, 2, -4, 24, -41, 3, 1, 1, 1, 1);

    Mat4 Q, R;
    mat4_qr(A, &Q, &R);

    // 1. Check Reconstruction Q * R = A
    Mat4 Recon = mat4_mul(Q, R);
    if (mat4_is_close(A, Recon, LOOSE_EPSILON)) {
        printf(ANSI_COLOR_GREEN "[PASS] QR Reconstruction (Q*R == A)\n" ANSI_COLOR_RESET);
        g_tests_passed++;
    } else {
        printf(ANSI_COLOR_RED "[FAIL] QR Reconstruction\n" ANSI_COLOR_RESET);
        mat4_print(A, "Original");
        mat4_print(Recon, "Reconstructed");
        g_tests_failed++;
    }

    // 2. Check Q Orthogonality (Q^T * Q = I)
    Mat4 Qt  = mat4_transpose(Q);
    Mat4 QtQ = mat4_mul(Qt, Q);
    if (mat4_is_close(QtQ, mat4_identity(), LOOSE_EPSILON)) {
        printf(ANSI_COLOR_GREEN "[PASS] Q is Orthogonal\n" ANSI_COLOR_RESET);
        g_tests_passed++;
    } else {
        printf(ANSI_COLOR_RED "[FAIL] Q is NOT Orthogonal\n" ANSI_COLOR_RESET);
        mat4_print(QtQ, "Qt * Q");
        g_tests_failed++;
    }

    // 3. Check R is Upper Triangular (Lower part is 0)
    bool upper_tri = fabsf(R.m[0][1]) < TIGHT_EPSILON && fabsf(R.m[0][2]) < TIGHT_EPSILON &&
                     fabsf(R.m[1][2]) < TIGHT_EPSILON;  // Check a few lower indices
    assert_bool("R is Upper Triangular", upper_tri);
}

void test_power_iteration() {
    print_header("Power Iteration (4x4 Eigen)");

    // Scale matrix (Diagonal) has clear eigenvalues: 10, 5, 2, 1
    // The dominant one is 10, corresponding to eigenvector (1, 0, 0, 0)
    Mat4 A = mat4_scale((Vec3){10.0f, 5.0f, 2.0f});

    Vec4 eig_vec;
    float eig_val;
    mat4_power_iteration(A, &eig_vec, &eig_val, 100, 1e-6f);

    assert_float_eq("Dominant Eigenvalue", 10.0f, eig_val, LOOSE_EPSILON);

    // Check vector direction (should be X axis)
    // Could be (1,0,0,0) or (-1,0,0,0)
    assert_float_eq("Eigenvector X component magnitude", 1.0f, fabsf(eig_vec.x), LOOSE_EPSILON);
    assert_float_eq("Eigenvector Y component", 0.0f, eig_vec.y, LOOSE_EPSILON);
}

void test_matrix_properties() {
    print_header("Matrix Properties");

    Mat4 I = mat4_identity();
    assert_float_eq("Frobenius Norm (Identity)", 2.0f, mat4_norm_frobenius(I), TIGHT_EPSILON);

    // Expect 4.0 for Frobenius Condition Number of Identity (sqrt(4) * sqrt(4))
    assert_float_eq("Condition Number (Identity)", 4.0f, mat4_condition_number(I), TIGHT_EPSILON);

    Mat3 I3 = mat3_identity();
    assert_bool("Identity is Positive Definite", mat3_is_positive_definite(I3));

    Mat3 Neg = mat3_new_column_major(-1, 0, 0, 0, 1, 0, 0, 0, 1);
    assert_bool("Negative Matrix is NOT Positive Definite", !mat3_is_positive_definite(Neg));
}

void test_solve_linear() {
    print_header("Linear Solve (3x3 & 4x4)");

    // 3x3 System
    // 2x = 4 -> x=2
    // y = 3
    // z = 1
    Mat3 A3    = mat3_identity();
    A3.m[0][0] = 2.0f;
    Vec3 b3    = {4.0f, 3.0f, 1.0f};
    Vec3 x3    = mat3_solve(A3, b3);
    assert_vec3_eq("Mat3 Solve", (Vec3){2.0f, 3.0f, 1.0f}, vec3_load(x3));

    // 4x4 System (Permutation)
    // Swap row 0 and 1
    // [0 1 0 0] x = 2  -> y = 2
    // [1 0 0 0] y = 1  -> x = 1
    Mat4 A4 = mat4_new_column_major(0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    Vec4 b4 = {2.0f, 1.0f, 3.0f, 4.0f};
    Vec4 x4 = mat4_solve(A4, b4);

    // Result should be x=1, y=2, z=3, w=4
    assert_vec4_eq("Mat4 Solve (Permuted)", (Vec4){1.0f, 2.0f, 3.0f, 4.0f}, vec4_load(x4));
}

void test_graphics_extensions() {
    print_header("Graphics Extensions (decompose, tangent basis)");

    // --- basis_from_normal ---
    {
        OrthonormalBasis b = basis_from_normal((Vec3){0.0f, 0.0f, 1.0f});
        assert_float_eq("Basis Normal Preserved", 1.0f, b.v2.z, EPSILON);
        assert_float_eq("Basis Tangent Unit", 1.0f,
                        sqrtf(b.v0.x * b.v0.x + b.v0.y * b.v0.y + b.v0.z * b.v0.z), EPSILON);

        // All pairs mutually orthogonal
        float d01 = b.v0.x * b.v1.x + b.v0.y * b.v1.y + b.v0.z * b.v1.z;
        float d02 = b.v0.x * b.v2.x + b.v0.y * b.v2.y + b.v0.z * b.v2.z;
        float d12 = b.v1.x * b.v2.x + b.v1.y * b.v2.y + b.v1.z * b.v2.z;
        assert_bool("Basis Orthogonal", fabsf(d01) < EPSILON && fabsf(d02) < EPSILON && fabsf(d12) < EPSILON);

        // Right-handed: v0 x v1 == v2
        SimdVec3 cx = vec3_cross(vec3_load(b.v0), vec3_load(b.v1));
        assert_float_eq("Basis Right-Handed", 1.0f, vec3_dot(cx, vec3_load(b.v2)), EPSILON);

        // Works for a normal parallel to the X helper axis too
        OrthonormalBasis bx = basis_from_normal((Vec3){1.0f, 0.0f, 0.0f});
        assert_float_eq("Basis Degenerate Axis", 1.0f, bx.v2.x, EPSILON);

        // Zero normal falls back to canonical basis
        OrthonormalBasis bz = basis_from_normal((Vec3){0.0f, 0.0f, 0.0f});
        assert_float_eq("Basis Zero-Normal Fallback", 1.0f, bz.v2.z, EPSILON);
    }

    // --- mat4_decompose on a known TRS ---
    {
        Vec3 t = {3.0f, -7.0f, 11.0f};
        Quat r = quat_from_axis_angle((Vec3){1.0f, 2.0f, -0.5f}, 1.1f);
        Vec3 s = {2.0f, 3.0f, 0.5f};

        Mat4 M = mat4_compose(t, r, s);
        Vec3 out_s, out_t;
        Quat out_r;
        mat4_decompose(M, &out_s, &out_r, &out_t);

        // Compare against matrix_test-style expectations via roundtrip
        assert_mat4_eq("Decompose Roundtrip", M, mat4_compose(out_t, out_r, out_s));

        Vec3 got_t = out_t;
        assert_vec3_eq("Decompose Translation", (Vec3){3.0f, -7.0f, 11.0f}, vec3_load(got_t));
        assert_bool("Decompose Scale", fabsf(out_s.x - 2.0f) < EPSILON && fabsf(out_s.y - 3.0f) < EPSILON &&
                                           fabsf(out_s.z - 0.5f) < EPSILON);

        // Rotation comparison through matrices (q ~ -q)
        assert_mat4_eq("Decompose Rotation", mat4_from_quat(r), mat4_from_quat(out_r));

        // Decompose matches manual extraction from the raw matrix
        Mat4 manual = mat4_mul(mat4_translate(t), mat4_mul(mat4_from_quat(r), mat4_scale(s)));
        assert_mat4_eq("Compose Equals T*R*S", manual, M);
    }

    // --- Mirrored transform keeps scale sign information ---
    {
        Vec3 t = {0.0f, 0.0f, 0.0f};
        Quat id = quat_identity();
        Vec3 s = {-2.0f, 1.0f, 1.0f};  // Mirror across YZ plane
        Mat4 M = mat4_compose(t, id, s);
        Vec3 out_s, out_t;
        Quat out_r;
        mat4_decompose(M, &out_s, &out_r, &out_t);
        assert_bool("Decompose Mirror Scale", out_s.x < 0.0f);
    }
}

/* ==================================================
   General SVD (fmat_*) helpers & tests
   ================================================== */

// Deterministic PRNG so failures are reproducible.
static uint32_t svd_rand_state = 0x12345678u;
static float svd_rand(float lo, float hi) {
    svd_rand_state = svd_rand_state * 1664525u + 1013904223u;
    const float unit = (float)(svd_rand_state >> 8) / (float)(1 << 24);
    return lo + unit * (hi - lo);
}

static bool fmat_all_close(const FMat* a, const FMat* b, float tol) {
    if (a->rows != b->rows || a->cols != b->cols) return false;
    for (size_t r = 0; r < a->rows; r++) {
        for (size_t c = 0; c < a->cols; c++) {
            if (fabsf(fmat_get(a, r, c) - fmat_get(b, r, c)) > tol) return false;
        }
    }
    return true;
}

/** Standard deviation of a single column. */
static float fcol_std(const FMat* m, size_t col) {
    double mean = 0.0;
    for (size_t r = 0; r < m->rows; r++) mean += fmat_get(m, r, col);
    mean /= (double)m->rows;
    double var = 0.0;
    for (size_t r = 0; r < m->rows; r++) {
        const double d = (double)fmat_get(m, r, col) - mean;
        var += d * d;
    }
    return sqrtf((float)(var / (double)m->rows));
}

/** Checks reconstruction, orthonormality, and descending order of a decomposition. */static void svd_check_decomposition(const char* name, const FMat* a, const FMat* u, const FMat* s, const FMat* v,
                                    float tol) {
    const size_t m = a->rows, n = a->cols, k = (m < n) ? m : n;

    // Shapes
    assert_bool(name, u && u->rows == m && u->cols == k && s && s->rows == k && s->cols == 1 && v &&
                          v->rows == n && v->cols == k);

    // Singular values strictly non-negative and descending
    bool ordered = true;
    for (size_t j = 0; j < k; j++) {
        if (fmat_get(s, j, 0) < -tol) ordered = false;
        if (j + 1 < k && fmat_get(s, j, 0) < fmat_get(s, j + 1, 0) - tol) ordered = false;
    }
    assert_bool(name, ordered);

    // Reconstruction: U * diag(S) * V^T == A
    FMat ud = fmat_create(m, k);
    for (size_t r = 0; r < m; r++) {
        for (size_t c = 0; c < k; c++) {
            fmat_set(&ud, r, c, fmat_get(u, r, c) * fmat_get(s, c, 0));
        }
    }
    FMat vt = fmat_transpose(v);
    FMat recon = fmat_mul(&ud, &vt);
    assert_bool(name, recon.data && fmat_all_close(a, &recon, tol));
    fmat_destroy(&ud);
    fmat_destroy(&vt);
    fmat_destroy(&recon);
}

static void svd_check_columns_orthonormal(const char* name, const FMat* mat, float tol) {
    // M^T M == I
    FMat mt = fmat_transpose(mat);
    FMat mtm = fmat_mul(&mt, mat);
    bool ok = mtm.data != NULL;
    for (size_t i = 0; ok && i < mtm.rows; i++) {
        for (size_t j = 0; j < mtm.cols; j++) {
            const float expected = (i == j) ? 1.0f : 0.0f;
            if (fabsf(fmat_get(&mtm, i, j) - expected) > tol) {
                ok = false;
                break;
            }
        }
    }
    assert_bool(name, ok);
    fmat_destroy(&mt);
    fmat_destroy(&mtm);
}

void test_general_svd() {
    print_header("General SVD (FMat)");

    // --- Known analytic case: diag(3,2,1) ---
    {
        FMat a = fmat_from_array(3, 3, (float[]){3, 0, 0, 0, 2, 0, 0, 0, 1});
        FMat u, s, v;
        assert_bool("SVD Diag Runs", fmat_svd(&a, &u, &s, &v));
        assert_bool("SVD Diag Values", fabsf(fmat_get(&s, 0, 0) - 3.0f) < EPSILON &&
                                           fabsf(fmat_get(&s, 1, 0) - 2.0f) < EPSILON &&
                                           fabsf(fmat_get(&s, 2, 0) - 1.0f) < EPSILON);
        svd_check_decomposition("SVD Diag Reconstruction", &a, &u, &s, &v, EPSILON);
        fmat_destroy(&a);
        fmat_destroy(&u);
        fmat_destroy(&s);
        fmat_destroy(&v);
    }

    // --- Tall random matrix (m > n): thin U, orthonormal columns ---
    {
        FMat a = fmat_create(6, 3);
        for (size_t i = 0; i < 6 * 3; i++) a.data[i] = svd_rand(-1.0f, 1.0f);
        FMat u, s, v;
        assert_bool("SVD Tall Runs", fmat_svd(&a, &u, &s, &v));
        svd_check_decomposition("SVD Tall Reconstruction", &a, &u, &s, &v, 5e-4f);
        svd_check_columns_orthonormal("SVD Tall U Orthonormal", &u, 1e-4f);
        svd_check_columns_orthonormal("SVD Tall V Orthonormal", &v, 1e-4f);
        fmat_destroy(&a);
        fmat_destroy(&u);
        fmat_destroy(&s);
        fmat_destroy(&v);
    }

    // --- Wide random matrix (n > m): exercises the transposed path ---
    {
        FMat a = fmat_create(3, 6);
        for (size_t i = 0; i < 3 * 6; i++) a.data[i] = svd_rand(-1.0f, 1.0f);
        FMat u, s, v;
        assert_bool("SVD Wide Runs", fmat_svd(&a, &u, &s, &v));
        svd_check_decomposition("SVD Wide Reconstruction", &a, &u, &s, &v, 5e-4f);
        svd_check_columns_orthonormal("SVD Wide U Orthonormal", &u, 1e-4f);
        fmat_destroy(&a);
        fmat_destroy(&u);
        fmat_destroy(&s);
        fmat_destroy(&v);
    }

    // --- Square random matrix ---
    {
        FMat a = fmat_create(4, 4);
        for (size_t i = 0; i < 16; i++) a.data[i] = svd_rand(-2.0f, 2.0f);
        FMat u, s, v;
        assert_bool("SVD Square Runs", fmat_svd(&a, &u, &s, &v));
        svd_check_decomposition("SVD Square Reconstruction", &a, &u, &s, &v, 5e-4f);
        fmat_destroy(&a);
        fmat_destroy(&u);
        fmat_destroy(&s);
        fmat_destroy(&v);
    }

    // --- Rank-deficient (rank 1 outer product): trailing sigmas ~ 0 ---
    {
        const Vec3 col = {0.3f, -0.5f, 0.8f};
        const Vec3 row = {2.0f, 1.0f, -1.0f};
        FMat a = fmat_create(3, 3);
        for (size_t r = 0; r < 3; r++) {
            for (size_t c = 0; c < 3; c++) {
                const float* cc = &col.x;
                const float* rr = &row.x;
                fmat_set(&a, r, c, cc[r] * rr[c]);
            }
        }
        FMat u, s, v;
        assert_bool("SVD Rank1 Runs", fmat_svd(&a, &u, &s, &v));
        assert_bool("SVD Rank1 Trailing Zeros",
                    fmat_get(&s, 0, 0) > 1e-3f && fmat_get(&s, 1, 0) < 1e-4f && fmat_get(&s, 2, 0) < 1e-4f);
        svd_check_decomposition("SVD Rank1 Reconstruction", &a, &u, &s, &v, 5e-4f);
        fmat_destroy(&a);
        fmat_destroy(&u);
        fmat_destroy(&s);
        fmat_destroy(&v);
    }

    // --- Degenerate inputs: identity and zero matrices must not crash ---
    {
        FMat a = fmat_identity(3);
        FMat u, s, v;
        assert_bool("SVD Identity Runs", fmat_svd(&a, &u, &s, &v));
        assert_bool("SVD Identity Sigmas", fabsf(fmat_get(&s, 0, 0) - 1.0f) < EPSILON &&
                                               fabsf(fmat_get(&s, 1, 0) - 1.0f) < EPSILON &&
                                               fabsf(fmat_get(&s, 2, 0) - 1.0f) < EPSILON);
        fmat_destroy(&a);
        fmat_destroy(&u);
        fmat_destroy(&s);
        fmat_destroy(&v);

        FMat z = fmat_create(2, 4);  // all zeros
        FMat uz, sz, vz;
        assert_bool("SVD Zero Runs", fmat_svd(&z, &uz, &sz, &vz));
        assert_bool("SVD Zero Sigmas", fmat_get(&sz, 0, 0) == 0.0f);
        fmat_destroy(&z);
        fmat_destroy(&uz);
        fmat_destroy(&sz);
        fmat_destroy(&vz);

        FMat bad = {2, 2, NULL};  // invalid (no storage)
        FMat ub, sb, vb;
        assert_bool("SVD Invalid Input Rejected", !fmat_svd(&bad, &ub, &sb, &vb));
        assert_bool("SVD Invalid Outputs Stay Empty", ub.data == NULL && sb.data == NULL && vb.data == NULL);
    }

    // --- Classic textbook example [[4,0],[3,-5]] ---
    {
        // A^T A = [[25,-15],[-15,25]] has eigenvalues 40 and 10.
        FMat a = fmat_from_array(2, 2, (float[]){4.0f, 0.0f, 3.0f, -5.0f});
        FMat u, s, v;
        assert_bool("SVD Textbook Runs", fmat_svd(&a, &u, &s, &v));
        assert_bool("SVD Textbook Sigma1", fabsf(fmat_get(&s, 0, 0) - sqrtf(40.0f)) < 1e-4f);
        assert_bool("SVD Textbook Sigma2", fabsf(fmat_get(&s, 1, 0) - sqrtf(10.0f)) < 1e-4f);
        svd_check_decomposition("SVD Textbook Reconstruction", &a, &u, &s, &v, 1e-4f);
        fmat_destroy(&a);
        fmat_destroy(&u);
        fmat_destroy(&s);
        fmat_destroy(&v);
    }
}

void test_ml_extensions() {
    print_header("ML Primitives (rng, init, elementwise, activations, losses, products)");

    // --- RNG determinism ---
    {
        FMatRng r1, r2;
        fmat_rng_seed(&r1, 42);
        fmat_rng_seed(&r2, 42);
        bool same = true;
        for (int i = 0; i < 100; i++) {
            if (fabsf(fmat_rng_uniform(&r1) - fmat_rng_uniform(&r2)) > 0.0f) same = false;
        }
        assert_bool("RNG Deterministic", same);
        fmat_rng_seed(&r1, 7);
        assert_bool("RNG Uniform In Range", fmat_rng_uniform(&r1) >= 0.0f && fmat_rng_uniform(&r1) < 1.0f);

        // Normal samples should stay within a sane band
        fmat_rng_seed(&r1, 9);
        bool sane = true;
        for (int i = 0; i < 1000; i++) {
            const float x = fmat_rng_normal(&r1);
            if (isnan(x) || isinf(x) || fabsf(x) > 12.0f) sane = false;
        }
        assert_bool("RNG Normal Sane", sane);
    }

    // --- Elementwise arithmetic ---
    {
        FMat a = fmat_from_array(2, 2, (float[]){1, 2, 3, 4});
        FMat b = fmat_from_array(2, 2, (float[]){5, 6, 7, 8});

        FMat sum = fmat_add(&a, &b);
        FMat diff = fmat_sub(&b, &a);
        FMat had = fmat_hadamard(&a, &b);
        FMat scaled = fmat_scale(&a, 2.0f);

        // Direct value checks
        bool ok_sum = fmat_get(&sum, 0, 0) == 6.0f && fmat_get(&sum, 1, 1) == 12.0f;
        bool ok_diff = fmat_get(&diff, 0, 1) == 4.0f && fmat_get(&diff, 1, 0) == 4.0f;
        bool ok_had = fmat_get(&had, 1, 1) == 32.0f && fmat_get(&had, 0, 0) == 5.0f;
        bool ok_scaled = fmat_get(&scaled, 1, 0) == 6.0f && fmat_get(&scaled, 0, 1) == 4.0f;
        assert_bool("Arithmetic Values", ok_sum && ok_diff && ok_had && ok_scaled);

        // Bias broadcast
        FMat z = fmat_copy(&a);
        FMat bias = fmat_from_array(1, 2, (float[]){10.0f, -10.0f});
        assert_bool("Bias Broadcast Runs", fmat_add_row_vector(&z, &bias));
        assert_bool("Bias Broadcast Values",
                    fmat_get(&z, 0, 0) == 11.0f && fmat_get(&z, 1, 0) == 13.0f &&
                    fmat_get(&z, 0, 1) == -8.0f && fmat_get(&z, 1, 1) == -6.0f);

        fmat_destroy(&a);
        fmat_destroy(&b);
        fmat_destroy(&sum);
        fmat_destroy(&diff);
        fmat_destroy(&had);
        fmat_destroy(&scaled);
        fmat_destroy(&z);
        fmat_destroy(&bias);
    }

    // --- Reductions ---
    {
        FMat m = fmat_from_array(3, 2, (float[]){1, 2, 3, 4, 5, 6});
        FMat sc = fmat_sum_cols(&m);
        FMat mc = fmat_mean_cols(&m);
        FMat am = fmat_argmax_rows(&m);

        assert_bool("Sum Cols", fmat_get(&sc, 0, 0) == 9.0f && fmat_get(&sc, 0, 1) == 12.0f);
        assert_bool("Mean Cols", fabsf(fmat_get(&mc, 0, 0) - 3.0f) < EPSILON &&
                                     fabsf(fmat_get(&mc, 0, 1) - 4.0f) < EPSILON);
        assert_bool("Argmax Rows", fmat_get(&am, 0, 0) == 1.0f && fmat_get(&am, 1, 0) == 1.0f &&
                                       fmat_get(&am, 2, 0) == 1.0f);

        fmat_destroy(&m);
        fmat_destroy(&sc);
        fmat_destroy(&mc);
        fmat_destroy(&am);
    }

    // --- Activations ---
    {
        FMat m = fmat_from_array(2, 2, (float[]){-1.0f, 0.0f, 2.0f, -3.0f});

        FMat r = fmat_relu(&m);
        assert_bool("ReLU", fmat_get(&r, 0, 0) == 0.0f && fmat_get(&r, 0, 1) == 0.0f &&
                                fmat_get(&r, 1, 0) == 2.0f && fmat_get(&r, 1, 1) == 0.0f);

        FMat sg = fmat_sigmoid(&m);
        assert_bool("Sigmoid(0)=0.5", fabsf(fmat_get(&sg, 0, 1) - 0.5f) < EPSILON);

        FMat th = fmat_tanh(&m);
        assert_bool("Tanh Known", fabsf(fmat_get(&th, 1, 0) - tanhf(2.0f)) < EPSILON);

        // In-place variants agree with out-of-place
        FMat r_ip = fmat_copy(&m);
        fmat_relu_ip(&r_ip);
        assert_bool("ReLU IP Matches", fmat_all_close(&r, &r_ip, 0));

        fmat_destroy(&m);
        fmat_destroy(&r);
        fmat_destroy(&sg);
        fmat_destroy(&th);
        fmat_destroy(&r_ip);
    }

    // --- Softmax ---
    {
        FMat logits = fmat_from_array(2, 3, (float[]){1000.0f, 1000.0f, 1000.0f, 1.0f, 2.0f, 3.0f});
        FMat p = fmat_softmax_rows(&logits);
        FMat row_sums = fmat_sum_rows(&p);
        assert_bool("Softmax Rows Sum To 1", fabsf(fmat_get(&row_sums, 0, 0) - 1.0f) < EPSILON &&
                                                 fabsf(fmat_get(&row_sums, 1, 0) - 1.0f) < EPSILON);
        // Shift invariance
        FMat shifted = fmat_from_array(2, 3, (float[]){2000.0f, 2000.0f, 2000.0f, 101.0f, 102.0f, 103.0f});
        FMat p2 = fmat_softmax_rows(&shifted);
        assert_bool("Softmax Shift Invariant", fmat_all_close(&p, &p2, EPSILON));
        // Uniform logits give uniform probabilities
        assert_bool("Softmax Uniform", fabsf(fmat_get(&p, 0, 0) - 1.0f / 3.0f) < EPSILON);

        fmat_destroy(&logits);
        fmat_destroy(&p);
        fmat_destroy(&row_sums);
        fmat_destroy(&shifted);
        fmat_destroy(&p2);
    }

    // --- Losses ---
    {
        FMat pred = fmat_from_array(1, 2, (float[]){1.0f, 0.0f});  // perfect one-hot prediction
        FMat label = fmat_from_array(1, 2, (float[]){1.0f, 0.0f});
        assert_bool("CE Perfect Prediction ~ 0", fmat_cross_entropy(&pred, &label) < 1e-6f);

        FMat pred_bad = fmat_from_array(1, 2, (float[]){0.5f, 0.5f});
        const float ce = fmat_cross_entropy(&pred_bad, &label);
        assert_bool("CE Uniform = ln2", fabsf(ce - 0.6931471805599453f) < EPSILON);

        FMat t1 = fmat_from_array(2, 2, (float[]){0, 0, 0, 0});
        FMat t2 = fmat_from_array(2, 2, (float[]){1, 2, 3, 4});
        assert_bool("MSE Known", fabsf(fmat_mse(&t1, &t2) - 7.5f) < EPSILON);

        fmat_destroy(&pred);
        fmat_destroy(&label);
        fmat_destroy(&pred_bad);
        fmat_destroy(&t1);
        fmat_destroy(&t2);
    }

    // --- Transpose products match explicit transposes ---
    {
        FMat a = fmat_create(3, 4), b = fmat_create(3, 2);
        for (size_t i = 0; i < 12; i++) a.data[i] = svd_rand(-1.0f, 1.0f);
        for (size_t i = 0; i < 6; i++) b.data[i] = svd_rand(-1.0f, 1.0f);

        FMat at = fmat_transpose(&a);
        FMat ref = fmat_mul(&at, &b);
        FMat fast = fmat_mul_ta(&a, &b);
        assert_bool("Mul TA Matches A^T B", ref.data && fast.data && fmat_all_close(&ref, &fast, 1e-4f));

        // Proper tb check: a(3x4) * c^T where c is 2x4 -> 3x2
        FMat c = fmat_create(2, 4);
        for (size_t i = 0; i < 8; i++) c.data[i] = svd_rand(-1.0f, 1.0f);
        FMat ct = fmat_transpose(&c);
        FMat ref3 = fmat_mul(&a, &ct);
        FMat fast3 = fmat_mul_tb(&a, &c);
        assert_bool("Mul TB Matches A B^T", ref3.data && fast3.data && fmat_all_close(&ref3, &fast3, 1e-4f));

        fmat_destroy(&a);
        fmat_destroy(&b);
        fmat_destroy(&at);
        fmat_destroy(&ref);
        fmat_destroy(&fast);
        fmat_destroy(&c);
        fmat_destroy(&ct);
        fmat_destroy(&ref3);
        fmat_destroy(&fast3);
    }

    // --- Weight init ---
    {
        FMat w = fmat_create(16, 16);
        FMatRng rng;
        fmat_rng_seed(&rng, 123);
        assert_bool("He Init Runs", fmat_he_init(&w, 784, &rng));
        float mn = 1e30f, mx = -1e30f;
        for (size_t i = 0; i < 256; i++) {
            if (w.data[i] < mn) mn = w.data[i];
            if (w.data[i] > mx) mx = w.data[i];
        }
        assert_bool("He Init Spread", mn < -0.05f && mx > 0.05f);

        // Reproducibility
        FMat w2 = fmat_create(16, 16);
        fmat_rng_seed(&rng, 123);
        fmat_he_init(&w2, 784, &rng);
        assert_bool("Init Reproducible", memcmp(w.data, w2.data, 256 * sizeof(float)) == 0);

        assert_bool("Xavier Init Runs", fmat_xavier_init(&w, 16, 16, &rng));
        fmat_destroy(&w);
        fmat_destroy(&w2);
    }
}

void test_pinv_pca() {
    print_header("Pseudo-inverse, Least Squares & PCA");

    // --- Pseudo-inverse of an invertible matrix behaves like the inverse ---
    {
        FMat a = fmat_from_array(2, 2, (float[]){2, 1, 1, 3});
        FMat apinv;
        assert_bool("Pinv Runs", fmat_pinv(&a, &apinv));
        FMat prod = fmat_mul(&apinv, &a);
        FMat eye = fmat_identity(2);
        assert_bool("Pinv Square Is Inverse", fmat_all_close(&prod, &eye, 1e-4f));
        fmat_destroy(&a);
        fmat_destroy(&apinv);
        fmat_destroy(&prod);
        fmat_destroy(&eye);
    }

    // --- Least squares: consistent overdetermined system solved exactly ---
    {
        // Points on the line y = 2x + 1
        FMat a = fmat_from_array(4, 2, (float[]){0, 1, 1, 1, 2, 1, 3, 1});
        FMat b = fmat_from_array(4, 1, (float[]){1, 3, 5, 7});
        FMat x;
        assert_bool("Lstsq Runs", fmat_lstsq(&a, &b, &x));
        assert_bool("Lstsq Exact Fit", fabsf(fmat_get(&x, 0, 0) - 2.0f) < 1e-3f &&
                                          fabsf(fmat_get(&x, 1, 0) - 1.0f) < 1e-3f);
        fmat_destroy(&a);
        fmat_destroy(&b);
        fmat_destroy(&x);
    }

    // --- Least squares: noisy system recovers near-true parameters ---
    {
        // y = 3x - 2 plus small noise
        FMat a = fmat_from_array(5, 2, (float[]){0, 1, 1, 1, 2, 1, 3, 1, 4, 1});
        FMat b = fmat_from_array(5, 1, (float[]){-2.1f, 1.2f, 3.8f, 7.1f, 9.8f});
        FMat x;
        assert_bool("Lstsq Noisy Runs", fmat_lstsq(&a, &b, &x));
        assert_bool("Lstsq Noisy Recovered", fabsf(fmat_get(&x, 0, 0) - 3.0f) < 0.15f &&
                                                 fabsf(fmat_get(&x, 1, 0) - (-2.0f)) < 0.15f);
        fmat_destroy(&a);
        fmat_destroy(&b);
        fmat_destroy(&x);
    }

    // --- PCA on strongly correlated 2D data ---
    {
        // Samples lie near the line y = x: PC1 should explain nearly all variance.
        FMat X = fmat_create(50, 2);
        FMatRng rng;
        fmat_rng_seed(&rng, 2024);
        for (size_t i = 0; i < 50; i++) {
            const float t = (float)i / 50.0f * 10.0f - 5.0f;
            fmat_set(&X, i, 0, t + fmat_rng_normal(&rng) * 0.01f);
            fmat_set(&X, i, 1, t + fmat_rng_normal(&rng) * 0.01f);
        }
        PCAResult pca;
        assert_bool("PCA Fits", fmat_pca(&X, 2, &pca));

        const float r0 = fmat_get(&pca.explained_ratio, 0, 0);
        const float r1 = fmat_get(&pca.explained_ratio, 1, 0);
        assert_bool("PCA Ratio Dominant", r0 > 0.99f);
        assert_bool("PCA Ratios Sum To 1", fabsf(r0 + r1 - 1.0f) < 1e-4f);
        assert_bool("PCA Ratios Descending", r0 >= r1);

        // First component direction ~ (1/sqrt2, 1/sqrt2)
        const float c00 = fabsf(fmat_get(&pca.components, 0, 0));
        assert_bool("PCA Direction Along Diagonal", fabsf(c00 - 0.70710678f) < 1e-3f);

        // Transform produces 50 x 2 scores
        FMat proj = fmat_pca_transform(&pca, &X);
        assert_bool("PCA Transform Shape", proj.rows == 50 && proj.cols == 2);

        // Variance along PC1 >> variance along PC2
        const float s0 = fcol_std(&proj, 0);
        const float s1 = fcol_std(&proj, 1);
        assert_bool("PCA Variance Ordering", s0 > s1 * 10.0f);

        fmat_destroy(&X);
        fmat_destroy(&proj);
        pca_result_destroy(&pca);
    }

    // --- PCA rejects too many components ---
    {
        FMat X = fmat_create(3, 2);
        PCAResult pca;
        assert_bool("PCA Component Cap", !fmat_pca(&X, 3, &pca));  // min(m,n) = 2
        fmat_destroy(&X);
    }
}

int main() {
    test_orthonormalize();
    test_eigen_symmetric();
    test_svd();
    test_qr();
    test_power_iteration();
    test_matrix_properties();
    test_solve_linear();
    test_graphics_extensions();
    test_general_svd();
    test_ml_extensions();
    test_pinv_pca();

    print_header("Summary");
    printf("Total Tests: %d\n", g_tests_passed + g_tests_failed);

    if (g_tests_failed > 0) {
        printf(ANSI_COLOR_RED "FAILED: %d\n" ANSI_COLOR_RESET, g_tests_failed);
        return 1;
    } else {
        printf(ANSI_COLOR_GREEN "ALL TESTS PASSED\n" ANSI_COLOR_RESET);
        return 0;
    }
}
