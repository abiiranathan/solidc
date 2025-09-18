#include "../include/vec.h"
#include <math.h>
#include <stdio.h>
#include "../include/cmp.h"
#include "../include/macros.h"

#define EPISILON (cmp_config_t){.epsilon = 1e2}

void test_vec2_operations() {
    Vec2 a = {1.0f, 2.0f};
    Vec2 b = {3.0f, 4.0f};
    Vec2 result;

    result = vec2_add(a, b);
    ASSERT(cmp_float(result.x, 4.0, EPISILON));
    ASSERT(cmp_float(result.y, 6.0, EPISILON));

    result = vec2_sub(b, a);
    ASSERT(cmp_float(result.x, 2.0, EPISILON));
    ASSERT(cmp_float(result.y, 2.0, EPISILON));

    result = vec2_mul(a, 2.0f);
    ASSERT(cmp_float(result.x, 2.0, EPISILON));
    ASSERT(cmp_float(result.y, 4.0, EPISILON));

    result = vec2_div(a, 2.0f);
    ASSERT(cmp_float(result.x, 0.5, EPISILON));
    ASSERT(cmp_float(result.y, 1.0, EPISILON));

    float dot = vec2_dot(a, b);
    ASSERT(cmp_float(dot, 11.0, EPISILON));

    float len = vec2_length(a);
    ASSERT(cmp_float(len, sqrtf(5.0f), EPISILON));

    result = vec2_normalize(a);
    ASSERT(cmp_float(vec2_length(result), 1.0, EPISILON));

    ASSERT(vec2_equals(a, (Vec2){1.0f, 2.0f}, FLT_EPSILON));

    result = vec2_lerp(a, b, 0.5f);
    ASSERT(vec2_equals(result, (Vec2){2.0f, 3.0f}, FLT_EPSILON));
}

void test_vec3_operations() {
    Vec3 a = {1.0f, 2.0f, 3.0f};
    Vec3 b = {4.0f, 5.0f, 6.0f};
    Vec3 result;

    result = vec3_add(a, b);
    ASSERT(cmp_float(result.x, 5.0, EPISILON));
    ASSERT(cmp_float(result.y, 7.0, EPISILON));
    ASSERT(cmp_float(result.z, 9.0, EPISILON));

    result = vec3_sub(b, a);
    ASSERT(cmp_float(result.x, 3.0, EPISILON));
    ASSERT(cmp_float(result.y, 3.0, EPISILON));
    ASSERT(cmp_float(result.z, 3.0, EPISILON));

    result = vec3_mul(a, 2.0f);
    ASSERT(cmp_float(result.x, 2.0, EPISILON));
    ASSERT(cmp_float(result.y, 4.0, EPISILON));
    ASSERT(cmp_float(result.z, 6.0, EPISILON));

    result = vec3_div(a, 2.0f);
    ASSERT(cmp_float(result.x, 0.5, EPISILON));
    ASSERT(cmp_float(result.y, 1.0, EPISILON));
    ASSERT(cmp_float(result.z, 1.5, EPISILON));

    float dot = vec3_dot(a, b);
    ASSERT(cmp_float(dot, 32.0, EPISILON));

    result = vec3_cross(a, b);
    ASSERT(vec3_equals(result, (Vec3){-3.0f, 6.0f, -3.0f}, FLT_EPSILON));

    float len = vec3_length(a);
    ASSERT(cmp_float(len, sqrtf(14.0f), EPISILON));

    result = vec3_normalize(a);
    ASSERT(cmp_float(vec3_length(result), 1.0, EPISILON));

    ASSERT(vec3_equals(a, (Vec3){1.0f, 2.0f, 3.0f}, FLT_EPSILON));

    result = vec3_lerp(a, b, 0.5f);
    ASSERT(vec3_equals(result, (Vec3){2.5f, 3.5f, 4.5f}, FLT_EPSILON));
}

void test_vec4_operations() {
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    Vec4 result;

    result = vec4_add(a, b);
    ASSERT(cmp_float(result.x, 6.0, EPISILON));
    ASSERT(cmp_float(result.y, 8.0, EPISILON));
    ASSERT(cmp_float(result.z, 10.0, EPISILON));
    ASSERT(cmp_float(result.w, 12.0, EPISILON));

    result = vec4_sub(b, a);
    ASSERT(cmp_float(result.x, 4.0, EPISILON));
    ASSERT(cmp_float(result.y, 4.0, EPISILON));
    ASSERT(cmp_float(result.z, 4.0, EPISILON));
    ASSERT(cmp_float(result.w, 4.0, EPISILON));

    result = vec4_mul(a, 2.0f);
    ASSERT(cmp_float(result.x, 2.0, EPISILON));
    ASSERT(cmp_float(result.y, 4.0, EPISILON));
    ASSERT(cmp_float(result.z, 6.0, EPISILON));
    ASSERT(cmp_float(result.w, 8.0, EPISILON));

    result = vec4_div(a, 2.0f);
    ASSERT(cmp_float(result.x, 0.5, EPISILON));
    ASSERT(cmp_float(result.y, 1.0, EPISILON));
    ASSERT(cmp_float(result.z, 1.5, EPISILON));
    ASSERT(cmp_float(result.w, 2.0, EPISILON));

    float dot = vec4_dot(a, b);
    ASSERT(cmp_float(dot, 70.0, EPISILON));

    float len = vec4_length(a);
    ASSERT(cmp_float(len, sqrtf(30.0f), EPISILON));

    result = vec4_normalize(a);
    ASSERT(cmp_float(vec4_length(result), 1.0, EPISILON));

    ASSERT(vec4_equals(a, (Vec4){1.0f, 2.0f, 3.0f, 4.0f}, FLT_EPSILON));

    result = vec4_lerp(a, b, 0.5f);
    ASSERT(vec4_equals(result, (Vec4){3.0f, 4.0f, 5.0f, 6.0f}, FLT_EPSILON));
}

void test_simd_vec2_operations() {
    Vec2 a = {1.0f, 2.0f};
    Vec2 b = {3.0f, 4.0f};

    SimdVec2 sa = simd_vec2_load(a);
    SimdVec2 sb = simd_vec2_load(b);

    SimdVec2 result = simd_vec2_add(sa, sb);
    Vec2 r          = simd_vec2_store(result);
    ASSERT(vec2_equals(r, (Vec2){4.0f, 6.0f}, FLT_EPSILON));

    result = simd_vec2_sub(sb, sa);
    r      = simd_vec2_store(result);
    ASSERT(vec2_equals(r, (Vec2){2.0f, 2.0f}, FLT_EPSILON));

    result = simd_vec2_mul(sa, 2.0f);
    r      = simd_vec2_store(result);
    ASSERT(vec2_equals(r, (Vec2){2.0f, 4.0f}, FLT_EPSILON));

    float dot = simd_vec2_dot(sa, sb);
    ASSERT(cmp_float(dot, 11.0, EPISILON));
}

void test_simd_vec3_operations() {
    Vec3 a = {1.0f, 2.0f, 3.0f};
    Vec3 b = {4.0f, 5.0f, 6.0f};

    SimdVec3 sa = simd_vec3_load(a);
    SimdVec3 sb = simd_vec3_load(b);

    SimdVec3 result = simd_vec3_add(sa, sb);
    Vec3 r          = simd_vec3_store(result);
    ASSERT(vec3_equals(r, (Vec3){5.0f, 7.0f, 9.0f}, FLT_EPSILON));

    result = simd_vec3_sub(sb, sa);
    r      = simd_vec3_store(result);
    ASSERT(vec3_equals(r, (Vec3){3.0f, 3.0f, 3.0f}, FLT_EPSILON));

    result = simd_vec3_mul(sa, 2.0f);
    r      = simd_vec3_store(result);
    ASSERT(vec3_equals(r, (Vec3){2.0f, 4.0f, 6.0f}, FLT_EPSILON));

    float dot = simd_vec3_dot(sa, sb);
    ASSERT(cmp_float(dot, 32.0, EPISILON));

    SimdVec3 cross = simd_vec3_cross(sa, sb);
    r              = simd_vec3_store(cross);
    ASSERT(vec3_equals(r, (Vec3){-3.0f, 6.0f, -3.0f}, FLT_EPSILON));
}

void test_simd_vec4_operations() {
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};

    SimdVec4 sa = simd_vec4_load(a);
    SimdVec4 sb = simd_vec4_load(b);

    SimdVec4 result = simd_vec4_add(sa, sb);
    Vec4 r          = simd_vec4_store(result);
    ASSERT(vec4_equals(r, (Vec4){6.0f, 8.0f, 10.0f, 12.0f}, FLT_EPSILON));

    result = simd_vec4_sub(sb, sa);
    r      = simd_vec4_store(result);
    ASSERT(vec4_equals(r, (Vec4){4.0f, 4.0f, 4.0f, 4.0f}, FLT_EPSILON));

    result = simd_vec4_mul(sa, 2.0f);
    r      = simd_vec4_store(result);
    ASSERT(vec4_equals(r, (Vec4){2.0f, 4.0f, 6.0f, 8.0f}, FLT_EPSILON));

    float dot = simd_vec4_dot(sa, sb);
    ASSERT(cmp_float(dot, 70.0, EPISILON));

    SimdVec4 norm = simd_vec4_normalize(sa);
    r             = simd_vec4_store(norm);
    ASSERT(cmp_float(vec4_length(r), 1.0f, EPISILON));  // check that its length is, EPISILON 1.

    // Test simd_vec4_rotate_x
    Vec4 initial_vector   = {1.0f, 0.0f, 0.0f, 1.0f};
    SimdVec4 initial_simd = simd_vec4_load(initial_vector);
    SimdVec4 rotated_x    = simd_vec4_rotate_x(initial_simd, M_PI / 2.0f);  // Rotate 90 degrees around X axis
    Vec4 rotated_vec_x    = simd_vec4_store(rotated_x);

    ASSERT(cmp_float(rotated_vec_x.x, 1.0f, EPISILON));  // x component should stay the s, EPISILOName
    ASSERT(cmp_float(rotated_vec_x.y, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_x.z, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_x.w, 1.0f, EPISILON));

    initial_vector = (Vec4){0.0f, 1.0f, 0.0f, 1.0f};
    initial_simd   = simd_vec4_load(initial_vector);
    rotated_x      = simd_vec4_rotate_x(initial_simd, M_PI / 2.0f);  // Rotate 90 degrees around X axis
    rotated_vec_x  = simd_vec4_store(rotated_x);

    ASSERT(cmp_float(rotated_vec_x.x, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_x.y, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_x.z, 1.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_x.w, 1.0f, EPISILON));

    // Test simd_vec4_rotate_y
    initial_vector     = (Vec4){0.0f, 1.0f, 0.0f, 1.0f};
    initial_simd       = simd_vec4_load(initial_vector);
    SimdVec4 rotated_y = simd_vec4_rotate_y(initial_simd, M_PI / 2.0f);  // Rotate 90 degrees around Y axis
    Vec4 rotated_vec_y = simd_vec4_store(rotated_y);

    ASSERT(cmp_float(rotated_vec_y.x, 0.0f, EPISILON));  // y component should stay the s, EPISILOName
    ASSERT(cmp_float(rotated_vec_y.y, 1.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_y.z, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_y.w, 1.0f, EPISILON));

    initial_vector = (Vec4){1.0f, 0.0f, 0.0f, 1.0f};
    initial_simd   = simd_vec4_load(initial_vector);
    rotated_y      = simd_vec4_rotate_y(initial_simd, M_PI / 2.0f);  // Rotate 90 degrees around Y axis
    rotated_vec_y  = simd_vec4_store(rotated_y);

    ASSERT(cmp_float(rotated_vec_y.x, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_y.y, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_y.z, -1.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_y.w, 1.0f, EPISILON));

    // Test simd_vec4_rotate_z
    initial_vector     = (Vec4){1.0f, 0.0f, 0.0f, 1.0f};
    initial_simd       = simd_vec4_load(initial_vector);
    SimdVec4 rotated_z = simd_vec4_rotate_z(initial_simd, M_PI / 2.0f);  // Rotate 90 degrees around Z axis
    Vec4 rotated_vec_z = simd_vec4_store(rotated_z);

    ASSERT(cmp_float(rotated_vec_z.x, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_z.y, 1.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_z.z, 0.0f, EPISILON));  // z component should stay the s, EPISILOName
    ASSERT(cmp_float(rotated_vec_z.w, 1.0f, EPISILON));

    initial_vector = (Vec4){0.0f, 1.0f, 0.0f, 1.0f};
    initial_simd   = simd_vec4_load(initial_vector);
    rotated_z      = simd_vec4_rotate_z(initial_simd, M_PI / 2.0f);  // Rotate 90 degrees around Z axis
    rotated_vec_z  = simd_vec4_store(rotated_z);

    ASSERT(cmp_float(rotated_vec_z.x, -1.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_z.y, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_z.z, 0.0f, EPISILON));
    ASSERT(cmp_float(rotated_vec_z.w, 1.0f, EPISILON));
}

void test_vec2_rotation_and_more() {
    Vec2 v = {1.0f, 0.0f};
    Vec2 result;

    // Test 90 degree rotation
    result = vec2_rotate(v, M_PI_2);
    ASSERT(vec2_equals(result, (Vec2){0.0f, 1.0f}, 1e-6f));

    // Test 180 degree rotation
    result = vec2_rotate(v, M_PI);
    ASSERT(vec2_equals(result, (Vec2){-1.0f, 0.0f}, 1e-6f));

    // Test perpendicular
    result = vec2_perpendicular(v);
    ASSERT(vec2_equals(result, (Vec2){0.0f, 1.0f}, FLT_EPSILON));

    // Test distance
    Vec2 a     = {1.0f, 1.0f};
    Vec2 b     = {4.0f, 5.0f};
    float dist = vec2_distance(a, b);
    ASSERT(cmp_float(dist, 5.0f, EPISILON));
}

void test_vec3_rotation_and_more() {
    Vec3 v = {1.0f, 0.0f, 0.0f};
    Vec3 result;

    // Test X-axis rotation
    result = vec3_rotate_x(v, M_PI_2);
    ASSERT(vec3_equals(result, (Vec3){1.0f, 0.0f, 0.0f}, 1e-6f));

    // Test Y-axis rotation
    result = vec3_rotate_y(v, M_PI_2);
    ASSERT(vec3_equals(result, (Vec3){0.0f, 0.0f, -1.0f}, 1e-6f));

    // Test Z-axis rotation
    result = vec3_rotate_z(v, M_PI_2);
    ASSERT(vec3_equals(result, (Vec3){0.0f, 1.0f, 0.0f}, 1e-6f));

    // Test arbitrary axis rotation
    Vec3 axis = {1.0f, 1.0f, 1.0f};
    result    = vec3_rotate(v, vec3_normalize(axis), (float)M_PI * 2.0f / 3.0f);
    ASSERT(vec3_equals(result, (Vec3){0.0f, 1.0f, 0.0f}, 1e-6f));

    // Test projection
    Vec3 a = {1.0f, 1.0f, 0.0f};
    Vec3 b = {1.0f, 0.0f, 0.0f};
    result = vec3_project(a, b);
    ASSERT(vec3_equals(result, (Vec3){1.0f, 0.0f, 0.0f}, FLT_EPSILON));

    // Test rejection
    result = vec3_reject(a, b);
    ASSERT(vec3_equals(result, (Vec3){0.0f, 1.0f, 0.0f}, FLT_EPSILON));

    // Test reflection
    Vec3 incident = {1.0f, -1.0f, 0.0f};
    Vec3 normal   = {0.0f, 1.0f, 0.0f};
    result        = vec3_reflect(incident, normal);
    ASSERT(vec3_equals(result, (Vec3){1.0f, 1.0f, 0.0f}, FLT_EPSILON));

    // Test distance
    Vec3 p1    = {1.0f, 2.0f, 3.0f};
    Vec3 p2    = {4.0f, 6.0f, 8.0f};
    float dist = vec3_distance(p1, p2);
    ASSERT(cmp_float(dist, sqrtf(50.0f), EPISILON));
}

void test_simd_vec2_rotation() {
    Vec2 v      = {1.0f, 0.0f};
    SimdVec2 sv = simd_vec2_load(v);

    // Test 90 degree rotation
    SimdVec2 result = simd_vec2_rotate(sv, M_PI_2);
    Vec2 r          = simd_vec2_store(result);
    ASSERT(vec2_equals(r, (Vec2){0.0f, 1.0f}, 1e-6f));

    // Test 180 degree rotation
    result = simd_vec2_rotate(sv, M_PI);
    r      = simd_vec2_store(result);
    ASSERT(vec2_equals(r, (Vec2){-1.0f, 0.0f}, 1e-6f));
}

void test_simd_vec3_rotation() {
    Vec3 v          = {1.0f, 0.0f, 0.0f};
    Vec3 axis       = {0.0f, 1.0f, 0.0f};
    SimdVec3 sv     = simd_vec3_load(v);
    SimdVec3 s_axis = simd_vec3_load(axis);

    // Test 90 degree rotation around Y-axis
    SimdVec3 result = simd_vec3_rotate(sv, s_axis, M_PI_2);
    Vec3 r          = simd_vec3_store(result);
    ASSERT(vec3_equals(r, (Vec3){0.0f, 0.0f, -1.0f}, 1e-6f));

    // Test arbitrary axis rotation
    axis   = (Vec3){1.0f, 1.0f, 1.0f};
    s_axis = simd_vec3_load(vec3_normalize(axis));
    result = simd_vec3_rotate(sv, s_axis, (float)(M_PI * 2.0 / 3.0));
    r      = simd_vec3_store(result);
    ASSERT(vec3_equals(r, (Vec3){0.0f, 1.0f, 0.0f}, 1e-6f));
}

int main(void) {
    printf("Running tests for SIMD vector library...\n");

    test_vec2_operations();
    printf("Vec2 basic operations passed ✅\n");

    test_vec2_rotation_and_more();
    printf("Vec2 rotation and additional operations passed ✅\n");

    test_vec3_operations();
    printf("Vec3 basic operations passed ✅\n");

    test_vec3_rotation_and_more();
    printf("Vec3 rotation and additional operations passed ✅\n");

    test_simd_vec2_operations();
    printf("SIMD Vec2 basic operations passed ✅\n");

    test_simd_vec2_rotation();
    printf("SIMD Vec2 rotation operations passed ✅\n");

    test_simd_vec3_operations();
    printf("SIMD Vec3 basic operations passed ✅\n");

    test_simd_vec3_rotation();
    printf("SIMD Vec3 rotation operations passed ✅\n");

    test_vec4_operations();
    printf("Vec4 basic operations passed ✅\n");

    test_simd_vec4_operations();
    printf("SIMD Vec4 basic operations passed ✅\n");

    printf("All tests passed! 🎉\n");
    return 0;
}
