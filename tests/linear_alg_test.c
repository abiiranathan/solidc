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

int main() {
    test_orthonormalize();
    test_eigen_symmetric();
    test_svd();
    test_qr();
    test_power_iteration();
    test_matrix_properties();
    test_solve_linear();
    test_graphics_extensions();

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
