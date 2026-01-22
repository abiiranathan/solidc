/*
 * simd_raytracer.c
 * A real-time ASCII ray tracer to demonstrate vec_simd.h performance.
 *
 * Compile:
 *   gcc simd_raytracer.c -o raytracer -lm -O3
 *
 * Controls:
 *   Ctrl+C to exit.
 */

#include "include/vec.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  // For usleep

// --- Configuration ---
#define WIDTH 80
#define HEIGHT 40
#define FPS 30

// Aspect ratio correction for terminal characters (they are usually 2x taller than wide)
#define ASPECT_RATIO (0.5f)
#define GRADIENT_SIZE 12
const char GRADIENT[] = " .:;=+*#%@";

// --- Scene Objects ---
typedef struct Sphere {
    SimdVec3 center;
    float radius;
    float radius_sq;  // Cached for performance
} Sphere;

// --- Ray Tracing Logic ---

/*
 * intersect_sphere
 * solves t^2*d.d + 2*t*(o-c).d + (o-c).(o-c) - r^2 = 0
 * Returns distance to intersection or -1.0 if miss.
 */
float intersect_sphere(SimdVec3 ray_origin, SimdVec3 ray_dir, Sphere* sphere) {
    // oc = ray_origin - sphere.center
    SimdVec3 oc = vec3_sub(ray_origin, sphere->center);

    // b = dot(oc, ray_dir)
    float b = vec3_dot(oc, ray_dir);

    // c = dot(oc, oc) - radius^2
    float c = vec3_dot(oc, oc) - sphere->radius_sq;

    // Discriminant = b*b - c (assuming ray_dir is normalized, so a=1)
    float h = b * b - c;

    if (h < 0.0f) {
        return -1.0f;  // No intersection
    }

    // Return distance (t)
    // We want the closest hit, which is -b - sqrt(h)
    return -b - sqrtf(h);
}

int main() {
    // 1. Setup Scene
    SimdVec3 camera_pos = vec3_load((Vec3){0.0f, 0.0f, -2.0f});

    Sphere sphere;
    sphere.center = vec3_load((Vec3){0.0f, 0.0f, 2.0f});  // Sphere is at Z=2
    sphere.radius = 1.2f;
    sphere.radius_sq = sphere.radius * sphere.radius;

    // A rotating light source
    float time = 0.0f;

    // Screen buffer (plus newlines and null terminator)
    char buffer[WIDTH * HEIGHT + HEIGHT + 1];

    printf("\x1b[2J");  // Clear screen once

    while (1) {
        // --- Animation ---
        time += 0.05f;

        // Calculate Light Direction based on time (Orbiting)
        // x = cos(t), z = sin(t)
        SimdVec3 light_pos = vec3_load((Vec3){cosf(time) * 3.0f, 2.0f, sinf(time) * 3.0f});

        // Pointer to current character in buffer
        char* b_ptr = buffer;

        // --- Rendering Loop (Top to Bottom, Left to Right) ---
        for (int y = 0; y < HEIGHT; y++) {

            // Map Y to [-1, 1] space
            float uv_y = -((float)y / HEIGHT * 2.0f - 1.0f) * ASPECT_RATIO;

            for (int x = 0; x < WIDTH; x++) {
                // Map X to [-1, 1] space (adjusted for aspect)
                float uv_x = ((float)x / WIDTH * 2.0f - 1.0f);

                // 2. Setup Ray
                // Ray Direction = normalize(PixelPos - CameraPos)
                // Since Camera is roughly at Z=-2 looking at Z=0, and pixel is virtual plane at Z=-1
                Vec3 pixel_local = {uv_x, uv_y, 1.0f};  // Forward is +Z in this setup
                SimdVec3 ray_dir = vec3_normalize(vec3_load(pixel_local));

                // 3. Trace
                float t = intersect_sphere(camera_pos, ray_dir, &sphere);

                if (t > 0.0f) {
                    // --- Hit Processing ---

                    // Hit Position = Origin + Dir * t
                    SimdVec3 hit_pos = vec3_add(camera_pos, vec3_mul(ray_dir, t));

                    // Normal = normalize(HitPos - SphereCenter)
                    SimdVec3 normal = vec3_normalize(vec3_sub(hit_pos, sphere.center));

                    // Light Direction = normalize(LightPos - HitPos)
                    SimdVec3 light_dir = vec3_normalize(vec3_sub(light_pos, hit_pos));

                    // Diffuse Factor = dot(Normal, LightDir)
                    float diffuse = vec3_dot(normal, light_dir);

                    // Clamp and map to char
                    if (diffuse < 0.0f) diffuse = 0.0f;
                    if (diffuse > 1.0f) diffuse = 1.0f;

                    int char_idx = (int)(diffuse * (float)(GRADIENT_SIZE - 2));
                    *b_ptr++ = GRADIENT[char_idx];

                } else {
                    // --- Miss (Background) ---
                    *b_ptr++ = ' ';
                }
            }
            *b_ptr++ = '\n';
        }
        *b_ptr = '\0';

        // 4. Display
        // Move cursor to top-left (VT100 code) then write buffer
        printf("\x1b[H");
        fwrite(buffer, 1, sizeof(buffer) - 1, stdout);

        // Simple debug stats
        // printf("SIMD Light Pos: %.2f %.2f %.2f\n", cosf(time)*3, 2.0f, sinf(time)*3);

        usleep(1000000 / FPS);
    }

    return 0;
}
