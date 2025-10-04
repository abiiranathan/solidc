#include "../include/matrix.h"
#include "../include/linear_alg.h"
#include "../include/macros.h"
#include "../include/vec.h"

#include <math.h>
#include <stdio.h>

#define EPSILON (float)1e-6

int float_equal(float a, float b) {
    return fabsf(a - b) < EPSILON;
}

void test_mat3_identity() {
    printf("Testing mat3_identity()...\n");
    Mat3 m        = mat3_identity();
    Mat3 expected = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

    if (mat3_equal(m, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat3_print(expected, "EXPECTED");
        mat3_print(m, "GOT");
    }
    printf("\n");
}

void test_mat4_identity() {
    printf("Testing mat4_identity()...\n");
    Mat4 m = mat4_identity();

    Mat4 expected = {{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}}};

    if (mat4_equal(m, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(m, "GOT");
    }
    printf("\n");
}

void test_mat3_mul() {
    printf("Testing mat3_mul()...\n");
    Mat3 a        = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}};
    Mat3 b        = {{{9, 8, 7}, {6, 5, 4}, {3, 2, 1}}};
    Mat3 result   = mat3_mul(a, b);
    Mat3 expected = {{{30, 24, 18}, {84, 69, 54}, {138, 114, 90}}};

    if (mat3_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat3_print(expected, "EXPECTED");
        mat3_print(result, "GOT");
    }
    printf("\n");
}

void test_mat3_lu() {
    printf("Testing mat3_lu()...\n");

    // Define the test matrix (column-major)
    Mat3 A = {{{2, 1, -1}, {-1, 3, 2}, {1, -1, 2}}};

    // Output original matrix
    mat3_print(A, "Original Matrix A");

    // Perform LU decomposition
    Mat3 L, U, P;
    bool success = mat3_lu(A, &L, &U, &P);

    if (!success) {
        printf("LU decomposition failed! Matrix may be singular.\n");
        exit(1);
    }

    // Print the results
    mat3_print(L, "Lower Triangular L");
    mat3_print(U, "Upper Triangular U");
    mat3_print(P, "Permutation Matrix P");

    // Verify: P * A = L * U
    Mat3 PA = mat3_mul(P, A);
    Mat3 LU = mat3_mul(L, U);

    mat3_print(PA, "P * A");
    mat3_print(LU, "L * U");

    // Check if PA = LU
    if (mat3_equal(PA, LU)) {
        printf("VERIFICATION PASSED: P * A = L * U\n");
    } else {
        printf("VERIFICATION FAILED: P * A != L * U\n");
    }
}

void test_mat3_solve() {
    Mat3 A = mat3_new_column_major(5.0f, 6.0f,
                                   0.0f,  // Row 0
                                   0.0f, 1.0f,
                                   4.0f,  // Row 1
                                   1.0f, 2.0f,
                                   3.0f  // Row 2
    );

    // Define the right-hand side vector b = Ax
    Vec3 b = {17.0f, 14.0f, 14.0f};

    // Solve the system Ax = b
    Vec3 x = mat3_solve(A, b);
    vec3_print(x, "mat3_solve SOLUTION: x");
    if (vec3_equals(x, (Vec3){1.0f, 2.0f, 3.0f}, 1e-04f)) {
        puts("mat3_solve PASSED");
        printf("Eqation has a correct solution\n");
    } else {
        puts("mat3_solve FAILED");
        exit(1);
    }
}

void test_mat4_solve() {
    // Define the mathematical matrix A:
    // [ 2  1 -1  3 ]
    // [ 1 -2  3  1 ]
    // [-1  3  2  4 ]
    // [ 3  1  4  2 ]

    // Known solution x = {1, 1, 1, 1}
    // Calculate b = Ax:
    Mat4 A = mat4_new_column_major(2.0f, 1.0f, -1.0f,
                                   3.0f,  // Row 0
                                   1.0f, -2.0f, 3.0f,
                                   1.0f,  // Row 1
                                   -1.0f, 3.0f, 2.0f,
                                   4.0f,  // Row 2
                                   3.0f, 1.0f, 4.0f,
                                   2.0f  // Row 3
    );

    Vec4 b = {5.0f, 3.0f, 8.0f, 10.0f};

    printf("Solving Ax=b for:\n");
    mat4_print(A, "A");
    vec4_print(b, "b");

    // Solve the system Ax = b
    Vec4 x = mat4_solve(A, b);
    printf("\n");
    printf("Solution found:\n");
    vec4_print(x, "X");  // Expected solution: {1.0, 1.0, 1.0, 1.0}
    ASSERT(vec4_equals(x, (Vec4){1.0, 1.0, 1.0, 1.0}, 1e-04));

    Mat4 A2 = mat4_new_column_major(5.0f, 6.0f, 0.0f,
                                    1.0f,  // Row 0
                                    0.0f, 1.0f, 4.0f,
                                    2.0f,  // Row 1
                                    1.0f, 2.0f, 3.0f,
                                    0.0f,  // Row 2
                                    2.0f, 0.0f, 1.0f,
                                    3.0f  // Row 3
    );

    Vec4 b2 = {18.0f, 16.0f, 14.0f, 8.0f};
    printf("\nSolving A2x=b2 for:\n");
    mat4_print(A2, "A2");
    vec4_print(b2, "b2");

    Mat4 A2rm = mat4_transpose(A2);
    mat4_print(A2rm, "A2 row-major");

    Vec4 x2 = mat4_solve(A2, b2);
    printf("\n");
    printf("Solution found:\n");
    vec4_print(x2, "X2");  // Expected solution: {1.0, 2.0, 3.0, 1.0}
    ASSERT(vec4_equals(x2, (Vec4){1.0, 2.0, 3.0, 1.0}, 1e-04));
}

void test_mat4_mul() {
    printf("Testing mat4_mul()...\n");
    Mat4 a        = {{{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}}}};
    Mat4 b        = {{{{16, 15, 14, 13}, {12, 11, 10, 9}, {8, 7, 6, 5}, {4, 3, 2, 1}}}};
    Mat4 result   = mat4_mul(a, b);
    Mat4 expected = {
        {{{80, 70, 60, 50}, {240, 214, 188, 162}, {400, 358, 316, 274}, {560, 502, 444, 386}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat3_mul_vec3() {
    printf("Testing mat3_mul_vec3()...\n");
    Mat3 m        = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}};
    Vec3 v        = {1, 2, 3};
    Vec3 result   = mat3_mul_vec3(m, v);
    Vec3 expected = {30, 36, 42};

    if (float_equal(result.x, expected.x) && float_equal(result.y, expected.y) &&
        float_equal(result.z, expected.z)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        printf("Expected: [%.2f, %.2f, %.2f]\n", expected.x, expected.y, expected.z);
        printf("Got:      [%.2f, %.2f, %.2f]\n", result.x, result.y, result.z);
    }
    printf("\n");
}

void test_mat4_mul_vec4() {
    printf("Testing mat4_mul_vec4()...\n");
    Mat4 m        = {{{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}}}};
    Vec4 v        = {1, 2, 3, 4};
    Vec4 result   = mat4_mul_vec4(m, v);
    Vec4 expected = {90, 100, 110, 120};

    if (float_equal(result.x, expected.x) && float_equal(result.y, expected.y) &&
        float_equal(result.z, expected.z) && float_equal(result.w, expected.w)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        printf("Expected: [%.2f, %.2f, %.2f, %.2f]\n", expected.x, expected.y, expected.z,
               expected.w);
        printf("Got:      [%.2f, %.2f, %.2f, %.2f]\n", result.x, result.y, result.z, result.w);
    }
    printf("\n");
}

void test_mat4_translate() {
    printf("Testing mat4_translate()...\n");
    Vec3 translation = {1, 2, 3};
    Mat4 result      = mat4_translate(translation);
    Mat4 expected    = {{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {1, 2, 3, 1}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_scale() {
    printf("Testing mat4_scale()...\n");
    Vec3 scale    = {2, 3, 4};
    Mat4 result   = mat4_scale(scale);
    Mat4 expected = {{{{2, 0, 0, 0}, {0, 3, 0, 0}, {0, 0, 4, 0}, {0, 0, 0, 1}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_rotate_x() {
    printf("Testing mat4_rotate_x()...\n");
    float angle   = M_PI / 2;  // 90 degrees
    Mat4 result   = mat4_rotate_x(angle);
    Mat4 expected = {{{{1, 0, 0, 0}, {0, 0, 1, 0}, {0, -1, 0, 0}, {0, 0, 0, 1}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_rotate_y() {
    printf("Testing mat4_rotate_y()...\n");
    float angle   = M_PI / 2;  // 90 degrees
    Mat4 result   = mat4_rotate_y(angle);
    Mat4 expected = {{{{0, 0, -1, 0}, {0, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 1}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_rotate_z() {
    printf("Testing mat4_rotate_z()...\n");
    float angle   = M_PI / 2;  // 90 degrees
    Mat4 result   = mat4_rotate_z(angle);
    Mat4 expected = {{{{0, 1, 0, 0}, {-1, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_rotate() {
    printf("Testing mat4_rotate()...\n");
    Vec3 axis   = {1, 1, 1};
    float angle = 2.0f * M_PIf / 3.0f;  // 120 degrees
    Mat4 result = mat4_rotate(axis, angle);

    // Expected result for 120° rotation around (1,1,1)
    Mat4 expected = {{{{0, 0, 1, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_transpose() {
    printf("Testing mat4_transpose()...\n");
    Mat4 m        = {{{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}}}};
    Mat4 result   = mat4_transpose(m);
    Mat4 expected = {{{{1, 5, 9, 13}, {2, 6, 10, 14}, {3, 7, 11, 15}, {4, 8, 12, 16}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_determinant() {
    printf("Testing mat4_determinant()...\n");
    Mat4 m         = {{{{1, 0, 0, 0}, {0, 2, 0, 0}, {0, 0, 3, 0}, {0, 0, 0, 4}}}};
    float result   = mat4_determinant(m);
    float expected = 24.0f;  // 1*2*3*4

    if (float_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        printf("Expected: %.2f\n", expected);
        printf("Got:      %.2f\n", result);
    }
    printf("\n");
}

void test_mat4_inverse() {
    printf("Testing mat4_inverse()...\n");
    Mat4 m        = {{{{2, 0, 0, 0}, {0, 3, 0, 0}, {0, 0, 4, 0}, {0, 0, 0, 5}}}};
    Mat4 result   = mat4_inverse(m);
    Mat4 expected = {{{{0.5f, 0, 0, 0}, {0, 1.0f / 3, 0, 0}, {0, 0, 0.25f, 0}, {0, 0, 0, 0.2f}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_ortho() {
    printf("Testing mat4_ortho()...\n");
    Mat4 result   = mat4_ortho(-1, 1, -1, 1, -1, 1);
    Mat4 expected = {{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, -1, 0}, {0, 0, 0, 1}}}};

    if (mat4_equal(result, expected)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        mat4_print(expected, "EXPECTED");
        mat4_print(result, "GOT");
    }
    printf("\n");
}

void test_mat4_perspective() {
    printf("Testing mat4_perspective()...\n");
    Mat4 result = mat4_perspective(M_PI / 2, 1.0f, 0.1f, 100.0f);
    // We can't easily predict exact values, so we'll check key properties
    int passed = 1;

    // Check near plane (z = -0.1)
    Vec4 near        = {0, 0, -0.1f, 1};
    Vec4 near_result = mat4_mul_vec4(result, near);
    near_result      = vec4_div(near_result, near_result.w);
    if (!float_equal(near_result.z, -1.0f)) {
        printf("FAILED: Near plane not mapped to -1\n");
        passed = 0;
    }

    // Check far plane (z = -100)
    Vec4 far        = {0, 0, -100.0f, 1};
    Vec4 far_result = mat4_mul_vec4(result, far);
    far_result      = vec4_div(far_result, far_result.w);
    if (!float_equal(far_result.z, 1.0f)) {
        printf("FAILED: Far plane not mapped to 1\n");
        passed = 0;
    }

    if (passed) {
        printf("PASSED\n");
    }
    printf("\n");
}

void test_mat4_look_at() {
    printf("Testing mat4_look_at()...\n");
    Vec3 eye    = {0, 0, 5};
    Vec3 target = {0, 0, 0};
    Vec3 up     = {0, 1, 0};
    Mat4 result = mat4_look_at(eye, target, up);

    // Should transform eye to origin
    Vec4 eye_homogeneous = {eye.x, eye.y, eye.z, 1};
    Vec4 transformed     = mat4_mul_vec4(result, eye_homogeneous);

    if (float_equal(transformed.x, 0) && float_equal(transformed.y, 0) &&
        float_equal(transformed.z, 0)) {
        printf("PASSED\n");
    } else {
        printf("FAILED\n");
        printf("Expected eye to map to [0, 0, 0], got [%.2f, %.2f, %.2f]\n", transformed.x,
               transformed.y, transformed.z);
    }
    printf("\n");
}

int main() {
    test_mat3_identity();
    test_mat4_identity();
    test_mat3_mul();
    test_mat4_mul();
    test_mat3_lu();
    test_mat3_solve();

    test_mat3_mul_vec3();
    test_mat4_mul_vec4();
    test_mat4_translate();
    test_mat4_scale();
    test_mat4_rotate_x();
    test_mat4_rotate_y();
    test_mat4_rotate_z();
    test_mat4_rotate();
    test_mat4_transpose();
    test_mat4_determinant();
    test_mat4_inverse();
    test_mat4_ortho();
    test_mat4_perspective();
    test_mat4_look_at();
    test_mat4_solve();

    return 0;
}
