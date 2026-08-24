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

void assert_float_eq(const char* name, float expected, float actual, float tol) {
    if (fabsf(expected - actual) > tol) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Expected %f, got %f\n" ANSI_COLOR_RESET, name, expected, actual);
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

void test_extensions_ml() {
    print_header("ML Extensions (Add, Sub, Scalar Mul)");

    Mat4 A = mat4_identity();
    Mat4 B = mat4_identity();

    // Add
    Mat4 sum = mat4_add(A, B);
    Mat4 expected_sum = mat4_diag(mat4_new_column_major(2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2));
    assert_mat4_eq("Mat4 Add", expected_sum, sum);

    // Sub
    Mat4 diff = mat4_sub(A, B);
    Mat4 expected_diff = mat4_new_column_major(0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    assert_mat4_eq("Mat4 Sub", expected_diff, diff);

    // Scalar Mul
    Mat4 scaled = mat4_scalar_mul(A, 3.0f);
    Mat4 expected_scaled = mat4_diag(mat4_new_column_major(3,0,0,0, 0,3,0,0, 0,0,3,0, 0,0,0,3));
    assert_mat4_eq("Mat4 Scalar Mul", expected_scaled, scaled);
}

void test_graphics_extensions() {
    print_header("Graphics Extensions (transform helpers, projections, quaternions)");

    const float PI = 3.14159265358979323846f;

    // --- Point vs direction transforms ---
    Mat4 T = mat4_translate((Vec3){10.0f, 20.0f, 30.0f});
    assert_vec3_eq("Transform Point Applies Translation", (Vec3){11.0f, 22.0f, 33.0f},
                   mat4_transform_point(T, (Vec3){1.0f, 2.0f, 3.0f}));
    assert_vec3_eq("Transform Direction Ignores Translation", (Vec3){1.0f, 2.0f, 3.0f},
                   mat4_transform_direction(T, (Vec3){1.0f, 2.0f, 3.0f}));

    // Point transform matches manual homogeneous math
    Vec4 h = mat4_mul_vec4(T, vec4_from_point((Vec3){1.0f, 2.0f, 3.0f}));
    assert_bool("Transform Point Matches Homogeneous", fabsf(h.w - 1.0f) < EPSILON);

    // --- Mat3 transpose / inverse ---
    Mat3 A = mat3_new_column_major(2, 0, 0, 1, 3, 0, 0, 4, 1);
    Mat3 At = mat3_transpose(A);
    assert_bool("Mat3 Transpose", fabsf(At.m[0][1] - A.m[1][0]) < EPSILON &&
                                     fabsf(At.m[2][0] - A.m[0][2]) < EPSILON && fabsf(At.m[1][2] - A.m[2][1]) < EPSILON);
    Mat3 Ainv = mat3_inverse(A);
    Mat3 I = mat3_mul(A, Ainv);
    assert_bool("Mat3 Inverse A*A^-1 = I", mat3_equal(I, mat3_identity()));

    // --- Normal matrix keeps normals perpendicular under non-uniform scale ---
    {
        Mat4 model = mat4_translate((Vec3){5.0f, 0.0f, 0.0f});
        model = mat4_mul(mat4_scale((Vec3){2.0f, 1.0f, 1.0f}), model);
        Mat3 N = mat3_normal_matrix(model);

        Vec3 tangent_w = mat4_transform_direction(model, (Vec3){1.0f, 0.0f, 0.0f});
        Vec3 normal_w = mat3_mul_vec3(N, (Vec3){0.0f, 0.0f, 1.0f});
        float d = tangent_w.x * normal_w.x + tangent_w.y * normal_w.y + tangent_w.z * normal_w.z;
        assert_bool("Normal Matrix Preserves Orthogonality", fabsf(d) < EPSILON);
    }

    // --- Ortho 2D equals ortho over pixel rect ---
    assert_mat4_eq("Ortho2D", mat4_ortho(0.0f, 800.0f, 0.0f, 600.0f, -1.0f, 1.0f), mat4_ortho_2d(800.0f, 600.0f));

    // --- Infinite perspective maps near plane to ndc z = -1 ---
    {
        Mat4 P = mat4_infinite_perspective(90.0f * PI / 180.0f, 16.0f / 9.0f, 0.1f);
        Vec3 eye_near = {0.0f, 0.0f, -0.1f};
        Vec3 ndc = mat4_transform_point(P, eye_near);
        assert_bool("Infinite Perspective Near -> z=-1", fabsf(ndc.z + 1.0f) < EPSILON);
    }

    // --- Quaternions ---
    Quat qz90 = quat_from_axis_angle((Vec3){0.0f, 0.0f, 1.0f}, PI / 2.0f);
    SimdVec3 rx = quat_rotate_vec3(qz90, vec3_load((Vec3){1.0f, 0.0f, 0.0f}));
    assert_float_eq("Quat Axis-Angle Rotates X to Y", 1.0f, vec3_store(rx).y, EPSILON);
    assert_bool("Quat Rotation Preserves X->Y", fabsf(vec3_store(rx).x) < EPSILON);

    // Euler X-rotation by 90deg sends Y to Z
    Quat qx90e = quat_from_euler(PI / 2.0f, 0.0f, 0.0f);
    SimdVec3 ry = quat_rotate_vec3(qx90e, vec3_load((Vec3){0.0f, 1.0f, 0.0f}));
    Vec3 rys = vec3_store(ry);
    assert_bool("Quat From Euler X-90 Y->Z", fabsf(rys.z - 1.0f) < EPSILON && fabsf(rys.y) < EPSILON);

    // Quaternion multiply composes like matrix multiply
    Quat qa = quat_from_axis_angle((Vec3){0.0f, 1.0f, 0.0f}, PI / 3.0f);
    Quat qb = quat_from_axis_angle((Vec3){1.0f, 0.0f, 0.0f}, PI / 5.0f);
    Mat4 m_ab = mat4_from_quat(quat_mul(qa, qb));
    Mat4 m_ba = mat4_mul(mat4_from_quat(qa), mat4_from_quat(qb));
    assert_mat4_eq("Quat Mul Composes Like Mat Mul", m_ab, m_ba);

    // Conjugate inverts the rotation
    SimdVec3 q_inv_applied =
        quat_rotate_vec3(quat_conjugate(qz90), quat_rotate_vec3(qz90, vec3_load((Vec3){1, 2, 3})));
    Vec3 back = vec3_store(q_inv_applied);
    assert_bool("Conjugate Undoes Rotation", fabsf(back.x - 1.0f) < EPSILON && fabsf(back.y - 2.0f) < EPSILON &&
                                                fabsf(back.z - 3.0f) < EPSILON);

    // Matrix -> quaternion roundtrip
    Quat qr = quat_from_mat4(mat4_from_quat(qa));
    assert_mat4_eq("Quat/Mat Roundtrip", mat4_from_quat(qa), mat4_from_quat(qr));

    // Slerp endpoints and midpoint
    Quat qs_same = quat_slerp(qz90, qz90, 0.37f);
    assert_bool("Slerp Same Quaternion Is Fixed Point",
                mat4_equal(mat4_from_quat(qs_same), mat4_from_quat(qz90)));
    Quat qs_half = quat_slerp(quat_identity(), qz90, 0.5f);
    SimdVec3 rmid = quat_rotate_vec3(qs_half, vec3_load((Vec3){1.0f, 0.0f, 0.0f}));
    Vec3 rmids = vec3_store(rmid);
    assert_bool("Slerp Halfway Is 45 Degrees", fabsf(rmids.x - sqrtf(0.5f)) < 1e-4f &&
                                                   fabsf(rmids.y - sqrtf(0.5f)) < 1e-4f);

    // Nlerp endpoints
    Quat qn = quat_nlerp(quat_identity(), qz90, 0.0f);
    assert_bool("Nlerp t=0 Returns Start", mat4_equal(mat4_from_quat(qn), mat4_identity()));

    // --- Compose / Decompose roundtrip ---
    {
        Vec3 tr = {3.0f, -7.0f, 11.0f};
        Quat rot = quat_from_axis_angle((Vec3){1.0f, 2.0f, -0.5f}, 1.1f);
        Vec3 sc = {2.0f, 3.0f, 0.5f};

        Mat4 M = mat4_compose(tr, rot, sc);
        Vec3 out_t, out_s;
        Quat out_r;
        mat4_decompose(M, &out_s, &out_r, &out_t);

        assert_bool("Decompose Translation", fabsf(out_t.x - tr.x) < EPSILON && fabsf(out_t.y - tr.y) < EPSILON &&
                                                 fabsf(out_t.z - tr.z) < EPSILON);
        assert_bool("Decompose Scale", fabsf(out_s.x - sc.x) < EPSILON && fabsf(out_s.y - sc.y) < EPSILON &&
                                           fabsf(out_s.z - sc.z) < EPSILON);
        // Compare rotations through their matrices (q and -q are equivalent)
        assert_mat4_eq("Decompose Rotation", mat4_from_quat(rot), mat4_from_quat(out_r));

        // Recomposed matrix matches original
        assert_mat4_eq("Compose/Decompose Roundtrip", M, mat4_compose(out_t, out_r, out_s));
    }

    // --- Unproject roundtrip through a full camera pipeline ---
    {
        Mat4 view = mat4_look_at(vec3_load((Vec3){0.0f, 0.0f, 5.0f}), vec3_load((Vec3){0.0f, 0.0f, 0.0f}),
                                 vec3_load((Vec3){0.0f, 1.0f, 0.0f}));
        Mat4 proj = mat4_perspective(60.0f * PI / 180.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        Mat4 vp = mat4_mul(proj, view);
        Mat4 inv_vp = mat4_inverse(vp);

        Vec3 world = {1.2f, -0.7f, -2.0f};
        Vec3 ndc = mat4_transform_point(vp, world);
        Vec3 world_back = mat4_unproject(inv_vp, ndc);
        assert_vec3_eq("Unproject Roundtrip", world, world_back);

        // Screen-to-NDC conversion is consistent with unproject
        Vec3 s = screen_to_ndc(400.0f, 300.0f, ndc.z, 800.0f, 600.0f);
        assert_bool("Screen To NDC Center", fabsf(s.x) < EPSILON && fabsf(s.y) < EPSILON);
    }
}

int main() {
    test_initialization();
    test_multiplication();
    test_transforms();
    test_inverse_det();
    test_linear_systems();
    test_extensions_ml();

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
