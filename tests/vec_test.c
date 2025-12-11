#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/vec.h"

#define EPSILON       0.0001f
#define LOOSE_EPSILON 0.002f  // For rsqrt approximation tests

// Colors for terminal output
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

void assert_float_eq(const char* name, float expected, float actual, float tol) {
    if (fabsf(expected - actual) > tol) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Expected %f, got %f (diff: %f)\n" ANSI_COLOR_RESET, name, expected, actual,
               fabsf(expected - actual));
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

void assert_vec2_eq(const char* name, Vec2 expected, SimdVec2 actual_simd) {
    Vec2 actual = vec2_store(actual_simd);
    if (fabsf(expected.x - actual.x) > EPSILON || fabsf(expected.y - actual.y) > EPSILON) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Expected (%.2f, %.2f), got (%.2f, %.2f)\n" ANSI_COLOR_RESET, name, expected.x,
               expected.y, actual.x, actual.y);
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

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

void assert_bool(const char* name, bool condition) {
    if (!condition) {
        printf(ANSI_COLOR_RED "[FAIL] %s\n" ANSI_COLOR_RESET, name);
        g_tests_failed++;
    } else {
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, name);
        g_tests_passed++;
    }
}

bool is_aligned(void* ptr, uintptr_t alignment) {
    return ((uintptr_t)ptr % alignment) == 0;
}

/* ==================================================
   Test Suites
   ================================================== */

void test_architecture() {
    print_header("Architecture & Alignment");

#if defined(SIMD_ARCH_X86)
    printf(ANSI_COLOR_BLUE "Info: Running in x86 Mode (SSE/AVX)\n" ANSI_COLOR_RESET);
#elif defined(SIMD_ARCH_ARM)
    printf(ANSI_COLOR_BLUE "Info: Running in ARM Mode (NEON)\n" ANSI_COLOR_RESET);
#else
    printf(ANSI_COLOR_BLUE "Info: Running in Scalar Fallback Mode\n" ANSI_COLOR_RESET);
#endif

    SimdVec4 v;
    assert_bool("SimdVec4 Memory Alignment (16-byte)", is_aligned(&v, 16));
}

void test_vec2_full() {
    print_header("Vec2 Operations");

    Vec2 a      = {10.0f, 20.0f};
    Vec2 b      = {2.0f, 5.0f};
    SimdVec2 sa = vec2_load(a);
    SimdVec2 sb = vec2_load(b);

    // Arithmetic
    assert_vec2_eq("Add", (Vec2){12.0f, 25.0f}, vec2_add(sa, sb));
    assert_vec2_eq("Sub", (Vec2){8.0f, 15.0f}, vec2_sub(sa, sb));
    assert_vec2_eq("Mul (Scalar)", (Vec2){20.0f, 40.0f}, vec2_mul(sa, 2.0f));

    // Dot
    assert_float_eq("Dot Product", 120.0f, vec2_dot(sa, sb), EPSILON);  // 20 + 100

    // Length / Normalize
    Vec2 pythag = {3.0f, 4.0f};
    SimdVec2 sp = vec2_load(pythag);
    assert_float_eq("Length Squared", 25.0f, vec2_length_sq(sp), EPSILON);
    assert_float_eq("Length", 5.0f, vec2_length(sp), EPSILON);
    assert_vec2_eq("Normalize", (Vec2){0.6f, 0.8f}, vec2_normalize(sp));

    // Zero Normalize check (Safety)
    SimdVec2 zero = vec2_load((Vec2){0, 0});
    assert_vec2_eq("Normalize Zero (Safety)", (Vec2){0, 0}, vec2_normalize(zero));

    // Rotate (90 degrees)
    // (1, 0) rotated 90 deg -> (0, 1)
    SimdVec2 x_axis = vec2_load((Vec2){1.0f, 0.0f});
    float PI_HALF   = 1.57079632679f;
    assert_vec2_eq("Rotate 90 deg", (Vec2){0.0f, 1.0f}, vec2_rotate(x_axis, PI_HALF));
}

void test_vec3_full() {
    print_header("Vec3 Operations");

    Vec3 a      = {1.0f, 2.0f, 3.0f};
    Vec3 b      = {4.0f, 5.0f, 6.0f};
    SimdVec3 sa = vec3_load(a);
    SimdVec3 sb = vec3_load(b);

    // Arithmetic
    assert_vec3_eq("Add", (Vec3){5.0f, 7.0f, 9.0f}, vec3_add(sa, sb));
    assert_vec3_eq("Sub", (Vec3){-3.0f, -3.0f, -3.0f}, vec3_sub(sa, sb));
    assert_vec3_eq("Mul (Scalar)", (Vec3){2.0f, 4.0f, 6.0f}, vec3_mul(sa, 2.0f));

    // Component-wise Scaling (Hadamard product)
    Vec3 scale_factors = {2.0f, 0.5f, 0.0f};
    SimdVec3 ss        = vec3_load(scale_factors);
    assert_vec3_eq("Scale (Component-wise)", (Vec3){2.0f, 1.0f, 0.0f}, vec3_scale(sa, ss));

    // Dot & Cross
    assert_float_eq("Dot Product", 32.0f, vec3_dot(sa, sb), EPSILON);  // 4+10+18

    Vec3 right       = {1, 0, 0};
    Vec3 up          = {0, 1, 0};
    SimdVec3 s_right = vec3_load(right);
    SimdVec3 s_up    = vec3_load(up);
    assert_vec3_eq("Cross Product (X x Y = Z)", (Vec3){0.0f, 0.0f, 1.0f}, vec3_cross(s_right, s_up));

    // Lengths
    assert_float_eq("Length Sq", 14.0f, vec3_length_sq(sa), EPSILON);  // 1+4+9
    assert_float_eq("Length", 3.741657f, vec3_length(sa), EPSILON);

    // Fast Normalize vs Precision Normalize
    // Fast normalize uses rsqrt, so we allow a looser epsilon
    Vec3 to_norm       = {10.0f, 0.0f, 0.0f};
    SimdVec3 s_to_norm = vec3_load(to_norm);

    assert_vec3_eq("Normalize (Precise)", (Vec3){1.0f, 0.0f, 0.0f}, vec3_normalize(s_to_norm));

    SimdVec3 s_fast = vec3_normalize_fast(s_to_norm);
    Vec3 r_fast     = vec3_store(s_fast);
    assert_float_eq("Normalize Fast X", 1.0f, r_fast.x, LOOSE_EPSILON);
    assert_float_eq("Normalize Fast Y", 0.0f, r_fast.y, LOOSE_EPSILON);
    assert_float_eq("Normalize Fast Z", 0.0f, r_fast.z, LOOSE_EPSILON);
}

void test_vec4_full() {
    print_header("Vec4 Operations");

    Vec4 a      = {1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b      = {5.0f, 5.0f, 5.0f, 5.0f};
    SimdVec4 sa = vec4_load(a);
    SimdVec4 sb = vec4_load(b);

    // Arithmetic
    assert_vec4_eq("Add", (Vec4){6.0f, 7.0f, 8.0f, 9.0f}, vec4_add(sa, sb));
    assert_vec4_eq("Sub", (Vec4){-4.0f, -3.0f, -2.0f, -1.0f}, vec4_sub(sa, sb));
    assert_vec4_eq("Mul (Scalar)", (Vec4){2.0f, 4.0f, 6.0f, 8.0f}, vec4_mul(sa, 2.0f));

    // Scaling
    Vec4 scale  = {1.0f, 0.0f, 1.0f, 2.0f};
    SimdVec4 ss = vec4_load(scale);
    assert_vec4_eq("Scale (Component-wise)", (Vec4){1.0f, 0.0f, 3.0f, 8.0f}, vec4_scale(sa, ss));

    // Dot & Length
    assert_float_eq("Dot Product", 50.0f, vec4_dot(sa, sb), EPSILON);  // 5+10+15+20
    assert_float_eq("Length Sq", 30.0f, vec4_length_sq(sa), EPSILON);  // 1+4+9+16
    assert_float_eq("Length", 5.47722f, vec4_length(sa), EPSILON);

    // Wait, vec4_normalize normalizes based on XYZW
    assert_vec4_eq("Normalize", (Vec4){0.0f, 0.0f, 0.0f, 1.0f}, vec4_normalize(vec4_load((Vec4){0, 0, 0, 10})));
}

void test_rotations() {
    print_header("Rotations (Axes)");

    float PI_HALF = 1.57079632679f;
    float PI      = 3.14159265359f;

    // Base vector (1, 0, 0, 1)
    SimdVec4 x_axis = vec4_load((Vec4){1.0f, 0.0f, 0.0f, 1.0f});
    // Base vector (0, 1, 0, 1)
    SimdVec4 y_axis = vec4_load((Vec4){0.0f, 1.0f, 0.0f, 1.0f});

    // 1. Rotate X-Axis vector around Z-Axis by 90deg -> should become Y-Axis
    // x' = x*c - y*s -> 0
    // y' = x*s + y*c -> 1
    assert_vec4_eq("Rotate Z (X->Y)", (Vec4){0.0f, 1.0f, 0.0f, 1.0f}, vec4_rotate_z(x_axis, PI_HALF));

    // 2. Rotate Y-Axis vector around X-Axis by 90deg -> should become Z-Axis
    // y' = y*c - z*s -> 0
    // z' = y*s + z*c -> 1
    assert_vec4_eq("Rotate X (Y->Z)", (Vec4){0.0f, 0.0f, 1.0f, 1.0f}, vec4_rotate_x(y_axis, PI_HALF));

    // 3. Rotate X-Axis vector around Y-Axis by 90deg -> should become -Z
    // x' = x*c + z*s -> 0
    // z' = -x*s + z*c -> -1
    // Note: Sign depends on coordinate system (Left vs Right Handed).
    // The implementation is: z' = -x*sin + z*cos. Sin(90)=1. So -1.
    assert_vec4_eq("Rotate Y (X-> -Z)", (Vec4){0.0f, 0.0f, -1.0f, 1.0f}, vec4_rotate_y(x_axis, PI_HALF));

    // 4. Rotate 180 degrees
    assert_vec4_eq("Rotate Z 180 (X-> -X)", (Vec4){-1.0f, 0.0f, 0.0f, 1.0f}, vec4_rotate_z(x_axis, PI));
}

void test_comparison_utils() {
    print_header("Utilities / Equality");

    Vec3 a = {1.0f, 1.0f, 1.0f};
    Vec3 b = {1.00005f, 1.0f, 1.0f};

    assert_bool("Vec3 Equals (Within Epsilon)", vec3_equals(a, b, 0.0001f) == true);
    assert_bool("Vec3 Not Equals (Outside Epsilon)", vec3_equals(a, b, 0.00001f) == false);

    Vec4 v4a = {1, 2, 3, 4};
    Vec4 v4b = {1, 2, 3, 4};
    assert_bool("Vec4 Equals Exact", vec4_equals(v4a, v4b, EPSILON));
}

void test_extensions() {
    print_header("Extensions (Lerp, Proj, Dist)");

    // --- Distance ---
    SimdVec3 p1 = vec3_load((Vec3){0, 0, 0});
    SimdVec3 p2 = vec3_load((Vec3){3, 4, 0});
    assert_float_eq("Vec3 Distance", 5.0f, vec3_distance(p1, p2), EPSILON);

    // --- Lerp ---
    // 50% between 0 and 10 is 5
    SimdVec3 l1     = vec3_load((Vec3){0, 0, 0});
    SimdVec3 l2     = vec3_load((Vec3){10, 20, 30});
    SimdVec3 lerped = vec3_lerp(l1, l2, 0.5f);
    assert_vec3_eq("Vec3 Lerp 0.5", (Vec3){5, 10, 15}, lerped);

    // --- Project ---
    // Project (1, 1) onto (1, 0) should be (1, 0)
    SimdVec2 vA   = vec2_load((Vec2){1.0f, 1.0f});
    SimdVec2 vB   = vec2_load((Vec2){1.0f, 0.0f});
    SimdVec2 proj = vec2_project(vA, vB);
    assert_vec2_eq("Vec2 Project", (Vec2){1.0f, 0.0f}, proj);

    // --- Reject ---
    // Reject (1, 1) from (1, 0) should be (0, 1)
    SimdVec2 rej = vec2_reject(vA, vB);
    assert_vec2_eq("Vec2 Reject", (Vec2){0.0f, 1.0f}, rej);

    // --- Perpendicular (2D) ---
    // Perp of (1, 0) is (0, 1) or (0, -1) depending on convention (-y, x) -> (0, 1)
    SimdVec2 perp2 = vec2_perpendicular(vB);
    assert_vec2_eq("Vec2 Perp", (Vec2){0.0f, 1.0f}, perp2);

    // Check dot product is zero (orthogonal)
    assert_float_eq("Vec2 Perp Dot is 0", 0.0f, vec2_dot(vB, perp2), EPSILON);

    // --- Perpendicular (3D) ---
    SimdVec3 vUp    = vec3_load((Vec3){0, 1, 0});
    SimdVec3 vOrtho = vec3_perpendicular(vUp);
    // Result implies dot product must be 0
    assert_float_eq("Vec3 Ortho Dot is 0", 0.0f, vec3_dot(vUp, vOrtho), EPSILON);
    // And length should be 1 (normalized)
    assert_float_eq("Vec3 Ortho Is Normalized", 1.0f, vec3_length(vOrtho), EPSILON);
}

int main() {
    test_architecture();
    test_vec2_full();
    test_vec3_full();
    test_vec4_full();
    test_rotations();
    test_comparison_utils();
    test_extensions();

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
