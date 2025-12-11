#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Include your library headers
#include "../include/linear_alg.h"
#include "../include/matrix.h"

#define EPSILON 1e-5f

// Colors
#define ANSI_COLOR_RED    "\x1b[31m"
#define ANSI_COLOR_GREEN  "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE   "\x1b[34m"
#define ANSI_COLOR_RESET  "\x1b[0m"

int g_tests_passed = 0;
int g_tests_failed = 0;

/* ==================================================
   Test Helpers
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

void assert_vec3_eq(const char* name, Vec3 expected, Vec3 actual) {
    if (fabsf(expected.x - actual.x) > EPSILON || fabsf(expected.y - actual.y) > EPSILON ||
        fabsf(expected.z - actual.z) > EPSILON) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Expected (%.3f, %.3f, %.3f), Got (%.3f, %.3f, %.3f)\n" ANSI_COLOR_RESET, name,
               expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

void assert_vec4_eq(const char* name, Vec4 expected, Vec4 actual) {
    if (fabsf(expected.x - actual.x) > EPSILON || fabsf(expected.y - actual.y) > EPSILON ||
        fabsf(expected.z - actual.z) > EPSILON || fabsf(expected.w - actual.w) > EPSILON) {
        printf(ANSI_COLOR_RED
               "[FAIL] %s: Expected (%.3f, %.3f, %.3f, %.3f), Got (%.3f, %.3f, %.3f, %.3f)\n" ANSI_COLOR_RESET,
               name, expected.x, expected.y, expected.z, expected.w, actual.x, actual.y, actual.z, actual.w);
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

void assert_mat4_eq(const char* name, Mat4 expected, Mat4 actual) {
    if (!mat4_equal(expected, actual)) {
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
   Basic Matrix Operations
   ================================================== */

void test_initialization() {
    print_header("Initialization");

    // Identity
    Mat4 id          = mat4_identity();
    Mat4 expected_id = mat4_new_column_major(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    assert_mat4_eq("Mat4 Identity", expected_id, id);

    // Diagonals
    Mat4 diag_src      = mat4_new_column_major(5, 1, 1, 1, 1, 6, 1, 1, 1, 1, 7, 1, 1, 1, 1, 8);
    Mat4 diag          = mat4_diag(diag_src);
    Mat4 expected_diag = mat4_new_column_major(5, 0, 0, 0, 0, 6, 0, 0, 0, 0, 7, 0, 0, 0, 0, 8);
    assert_mat4_eq("Mat4 Diagonal Extraction", expected_diag, diag);
}

void test_multiplication() {
    print_header("Matrix Multiplication");

    // A: Scale(2, 2, 2)
    Mat4 A = mat4_scale((Vec3){2.0f, 2.0f, 2.0f});

    // B: Translate(1, 2, 3)
    // In Column Major, Translation is in the last column [3][0..2]
    Mat4 B = mat4_translate((Vec3){1.0f, 2.0f, 3.0f});

    // C = A * B
    // Scaling a translation matrix results in the translation also being scaled
    // IF the scale is on the left.
    // Result should be:
    // [ 2 0 0 2 ]
    // [ 0 2 0 4 ]
    // [ 0 0 2 6 ]
    // [ 0 0 0 1 ]
    Mat4 C = mat4_mul(A, B);

    Mat4 expected = mat4_new_column_major(2, 0, 0, 2, 0, 2, 0, 4, 0, 0, 2, 6, 0, 0, 0, 1);

    assert_mat4_eq("Scale * Translate", expected, C);

    // D = B * A
    // Translation on left, Scale on right.
    // Result should be:
    // [ 2 0 0 1 ]
    // [ 0 2 0 2 ]
    // [ 0 0 2 3 ]
    // [ 0 0 0 1 ]
    Mat4 D          = mat4_mul(B, A);
    Mat4 expected_D = mat4_new_column_major(2, 0, 0, 1, 0, 2, 0, 2, 0, 0, 2, 3, 0, 0, 0, 1);
    assert_mat4_eq("Translate * Scale", expected_D, D);
}

void test_transforms() {
    print_header("Transformations");

    // 1. Matrix-Vector Multiplication
    // M = Translate(10, 20, 30)
    // v = (0, 0, 0, 1) -> Point at origin
    // Result = (10, 20, 30, 1)
    Mat4 T   = mat4_translate((Vec3){10.0f, 20.0f, 30.0f});
    Vec4 v   = {0.0f, 0.0f, 0.0f, 1.0f};
    Vec4 res = mat4_mul_vec4(T, v);
    assert_vec4_eq("Mat4 * Vec4 (Translate)", (Vec4){10, 20, 30, 1}, res);

    // 2. Rotation Z
    // Rotate 90 deg around Z. X axis (1,0,0) becomes Y axis (0,1,0).
    float pi_half = 1.5707963f;
    Mat4 Rz       = mat4_rotate_z(pi_half);
    Vec4 vx       = {1.0f, 0.0f, 0.0f, 1.0f};
    Vec4 vr       = mat4_mul_vec4(Rz, vx);
    assert_vec4_eq("Rotate Z (X->Y)", (Vec4){0.0f, 1.0f, 0.0f, 1.0f}, vr);

    // 3. Look At
    // Eye at (0,0,10), Target (0,0,0), Up (0,1,0).
    // View matrix should transform Eye to (0,0,0).
    // Note: Standard View matrix transforms World Origin to (0, 0, -dist).
    // Let's test that it transforms the Eye position to the origin (ish).
    // Actually, View * Eye = Origin is only true if w=1 and pure rotation/translation.
    Vec3 eye          = {0, 0, 10};
    Vec3 target       = {0, 0, 0};
    Vec3 up           = {0, 1, 0};
    SimdVec3 s_eye    = vec3_load(eye);
    SimdVec3 s_target = vec3_load(target);
    SimdVec3 s_up     = vec3_load(up);

    Mat4 view     = mat4_look_at(s_eye, s_target, s_up);
    Vec4 v_eye    = {0, 0, 10, 1};
    Vec4 v_viewed = mat4_mul_vec4(view, v_eye);
    // In view space, eye is at 0,0,0
    assert_vec4_eq("LookAt transforms Eye to Origin", (Vec4){0, 0, 0, 1}, v_viewed);
}

void test_inverse_det() {
    print_header("Inverse & Determinant");

    Mat4 S    = mat4_scale((Vec3){2.0f, 0.5f, 4.0f});
    float det = mat4_determinant(S);
    // Det = 2 * 0.5 * 4 * 1 = 4.0
    if (fabsf(det - 4.0f) < EPSILON) {
        printf(ANSI_COLOR_GREEN "[PASS] Determinant Scale\n" ANSI_COLOR_RESET);
        g_tests_passed++;
    } else {
        printf(ANSI_COLOR_RED "[FAIL] Determinant Scale: Expected 4.0, Got %f\n" ANSI_COLOR_RESET, det);
        g_tests_failed++;
    }

    Mat4 InvS = mat4_inverse(S);
    // Inverse of Scale(2, 0.5, 4) is Scale(0.5, 2, 0.25)
    Mat4 ExpectedInv = mat4_scale((Vec3){0.5f, 2.0f, 0.25f});
    assert_mat4_eq("Inverse Scale", ExpectedInv, InvS);

    // Check A * InvA = Identity
    Mat4 I = mat4_mul(S, InvS);
    assert_mat4_eq("A * InvA == Identity", mat4_identity(), I);
}

void test_linear_systems() {
    print_header("Linear Algebra (Solve)");

    // System:
    // 2x + y = 5
    // x + y = 3
    // Solution: x=2, y=1
    // Extended to 4x4 Identity for Z/W
    Mat4 A    = mat4_identity();
    A.m[0][0] = 2;
    A.m[1][0] = 1;  // Row 0: 2, 1
    A.m[0][1] = 1;
    A.m[1][1] = 1;  // Row 1: 1, 1

    Vec4 b = {5.0f, 3.0f, 0.0f, 1.0f};  // z=0, w=1 (dummy)

    // We expect z=0, w=1 because the rest is identity
    Vec4 sol = mat4_solve(A, b);

    assert_vec4_eq("Solve 2x2 System embedded in 4x4", (Vec4){2.0f, 1.0f, 0.0f, 1.0f}, sol);

    // Test LU Decomposition explicitly
    Mat4 L, U, P;
    bool success = mat4_lu(A, &L, &U, &P);
    assert_bool("LU Decomposition Success", success);

    // Check reconstruction P*A = L*U
    Mat4 PA = mat4_mul(P, A);
    Mat4 LU = mat4_mul(L, U);
    assert_mat4_eq("PA == LU", PA, LU);
}

int main() {
    test_initialization();
    test_multiplication();
    test_transforms();
    test_inverse_det();
    test_linear_systems();

    print_header("Summary");
    printf("Total Tests: %d\n", g_tests_passed + g_tests_failed);
    printf(ANSI_COLOR_GREEN "PASSED: %d\n" ANSI_COLOR_RESET, g_tests_passed);

    if (g_tests_failed > 0) {
        printf(ANSI_COLOR_RED "FAILED: %d\n" ANSI_COLOR_RESET, g_tests_failed);
        return 1;
    } else {
        printf(ANSI_COLOR_GREEN "ALL TESTS PASSED\n" ANSI_COLOR_RESET);
        return 0;
    }
}
