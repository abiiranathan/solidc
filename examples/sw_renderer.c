/**
 * @file sw_renderer.c
 * @brief A compact software 3D rasterizer built entirely on solidc's math.
 *
 * Renders a rotating torus through the full real-time graphics pipeline:
 *
 *   mesh -> batch vertex transform (one GEMM via fmat_batch_transform)
 *        -> quaternion-driven animation (mat4_compose)
 *        -> view/projection matrices (mat4_look_at + infinite perspective)
 *        -> backface culling (vec3_cross sign)
 *        -> flat Lambert shading (vec3_dot with a directional light)
 *        -> perspective-correct rasterization (barycentric weights,
 *           depth-buffered)
 *
 * The final frame is written to torus.ppm; throughput statistics are
 * printed to stdout. This is the same pipeline shape a game engine or
 * GPU driver implements, just on the CPU - which makes it easy to see
 * where each library primitive fits.
 *
 * Build & run from the repo root:
 *   gcc -std=gnu11 -O2 -D_GNU_SOURCE -msse4.1 -Iinclude \
 *       examples/sw_renderer.c -o sw_renderer -lm && ./sw_renderer
 */

#include "../include/linear_alg.h"
#include "../include/xtime.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Render target */
#define WIDTH  480
#define HEIGHT 270
#define FRAMES 120

/* Torus geometry */
#define TORUS_R   1.6f  /* distance from center of tube to center of torus */
#define TORUS_r   0.55f /* tube radius */
#define SEG_MAJOR 48
#define SEG_MINOR 20

static const float NEAR_PLANE = 0.1f;
static const float FOV_RADIANS = 60.0f * 3.14159265358979323846f / 180.0f;

/* ------------------------------------------------------------------ */
/* Framebuffer                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    int width, height;
    uint8_t* color; /* RGB triplets */
    float* depth;   /* per-pixel 1/clip.w; larger = closer to camera */
} Framebuffer;

static void fb_create(Framebuffer* fb, int w, int h) {
    fb->width = w;
    fb->height = h;
    fb->color = calloc((size_t)w * (size_t)h * 3, 1);
    fb->depth = calloc((size_t)w * (size_t)h, sizeof(float));
}

static void fb_destroy(Framebuffer* fb) {
    free(fb->color);
    free(fb->depth);
}

static void fb_clear(Framebuffer* fb) {
    memset(fb->color, 18, (size_t)fb->width * (size_t)fb->height * 3); /* dark background */
    memset(fb->depth, 0, (size_t)fb->width * (size_t)fb->height * sizeof(float));
}

/* ------------------------------------------------------------------ */
/* Torus mesh generation                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    size_t vertex_count;
    FMat positions; /* N x 4 homogeneous rows */
    size_t index_count;
    size_t* indices; /* 3 per triangle */
} Mesh;

static void torus_generate(Mesh* mesh) {
    const size_t cols = SEG_MAJOR, rows = SEG_MINOR;
    mesh->vertex_count = cols * rows;
    mesh->positions = fmat_create(mesh->vertex_count, 4);

    for (size_t j = 0; j < rows; j++) {
        const float v = (float)j / rows * 2.0f * 3.14159265358979323846f;
        for (size_t i = 0; i < cols; i++) {
            const float u = (float)i / cols * 2.0f * 3.14159265358979323846f;
            const size_t idx = j * cols + i;
            fmat_set(&mesh->positions, idx, 0, (TORUS_R + TORUS_r * cosf(v)) * cosf(u));
            fmat_set(&mesh->positions, idx, 1, TORUS_r * sinf(v));
            fmat_set(&mesh->positions, idx, 2, (TORUS_R + TORUS_r * cosf(v)) * sinf(u));
            fmat_set(&mesh->positions, idx, 3, 1.0f);
        }
    }

    /* Two triangles per grid quad, wound counter-clockwise when viewed
     * from outside the torus (required for backface culling). */
    mesh->index_count = cols * rows * 6;
    mesh->indices = malloc(mesh->index_count * sizeof(size_t));
    size_t k = 0;
    for (size_t j = 0; j < rows; j++) {
        for (size_t i = 0; i < cols; i++) {
            const size_t a = j * cols + i;
            const size_t b = j * cols + (i + 1) % cols;
            const size_t c = ((j + 1) % rows) * cols + i;
            const size_t d = ((j + 1) % rows) * cols + (i + 1) % cols;
            mesh->indices[k++] = a;
            mesh->indices[k++] = c;
            mesh->indices[k++] = b;
            mesh->indices[k++] = b;
            mesh->indices[k++] = c;
            mesh->indices[k++] = d;
        }
    }
}

static void mesh_destroy(Mesh* m) {
    fmat_destroy(&m->positions);
    free(m->indices);
}

/* ------------------------------------------------------------------ */
/* Rasterization                                                       */
/* ------------------------------------------------------------------ */

/** Writes the framebuffer as a binary PPM (P6). Returns 0 on success. */
static int fb_write_ppm(const Framebuffer* fb, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", fb->width, fb->height);
    fwrite(fb->color, 1, (size_t)fb->width * (size_t)fb->height * 3, f);
    fclose(f);
    return 0;
}

int main(void) {
    // Mesh and buffers ---------------------------------------------------
    Mesh mesh;
    torus_generate(&mesh);

    Framebuffer fb;
    fb_create(&fb, WIDTH, HEIGHT);

    // World-space vertex cache reused every frame (N x 4).
    FMat world = fmat_create(mesh.vertex_count, 4);
    // Clip-space cache (N x 4).
    FMat clip = fmat_create(mesh.vertex_count, 4);

    // Camera: elevated enough to show the torus hole; torus spins in place.
    const Vec3 eye = {0.0f, 2.6f, -3.4f};
    const Vec3 target = {0.0f, 0.0f, 0.0f};
    const Vec3 up = {0.0f, 1.0f, 0.0f};
    const Mat4 view = mat4_look_at(vec3_load(eye), vec3_load(target), vec3_load(up));
    const Mat4 proj = mat4_infinite_perspective(FOV_RADIANS, (float)WIDTH / (float)HEIGHT, NEAR_PLANE);
    const Mat4 view_proj = mat4_mul(proj, view);

    // Directional light in world space, pointing from surfaces TOWARD the
    // light source (up and to the viewer's left).
    const SimdVec3 light_dir = vec3_normalize(vec3_load((Vec3){0.25f, 0.8f, -0.55f}));

    printf("software renderer: %dx%d, %zu vertices, %zu triangles/frame, %d frames\n", WIDTH, HEIGHT, mesh.vertex_count,
           mesh.index_count / 3, FRAMES);

    xtime_t ts_start, ts_end;
    xtime_now(&ts_start);

    size_t triangles_drawn = 0;
    size_t triangles_culled = 0;
    double pixels_shaded = 0.0;

    for (int frame = 0; frame < FRAMES; frame++) {
        // --- Animation: smooth spin around Y with a fixed tilt around X ---
        // Quaternion composition avoids gimbal issues and interpolates cleanly.
        const float angle = (float)frame / FRAMES * 2.0f * 3.14159265358979323846f;
        const Quat spin = quat_from_axis_angle((Vec3){0.0f, 1.0f, 0.0f}, angle);
        const Quat tilt = quat_from_axis_angle((Vec3){1.0f, 0.0f, 0.0f}, 0.45f);
        const Quat orientation = quat_mul(spin, tilt);

        const Mat4 model = mat4_compose((Vec3){0.0f, 0.0f, 0.0f}, orientation, (Vec3){1.0f, 1.0f, 1.0f});
        const Mat4 mvp = mat4_mul(view_proj, model);

        // --- Batch vertex transform: entire mesh in one matrix product ------
        FMat w_out = fmat_batch_transform(&model, &mesh.positions);
        fmat_destroy(&world);
        world = w_out; /* move */

        FMat c_out = fmat_batch_transform(&mvp, &mesh.positions);
        fmat_destroy(&clip);
        clip = c_out; /* move */

        fb_clear(&fb);

        // --- Triangle loop ---------------------------------------------------
        for (size_t t = 0; t < mesh.index_count; t += 3) {
            const size_t ia = mesh.indices[t];
            const size_t ib = mesh.indices[t + 1];
            const size_t ic = mesh.indices[t + 2];

            const float wa = fmat_get(&clip, ia, 3);
            const float wb = fmat_get(&clip, ib, 3);
            const float wc = fmat_get(&clip, ic, 3);

            // Simplification: reject any triangle crossing the near plane.
            if (wa < NEAR_PLANE || wb < NEAR_PLANE || wc < NEAR_PLANE) {
                triangles_culled++;
                continue;
            }

            // Perspective divide + viewport mapping (y flipped for screen space).
            const float ndc_x[3] = {fmat_get(&clip, ia, 0) / wa, fmat_get(&clip, ib, 0) / wb,
                                    fmat_get(&clip, ic, 0) / wc};
            const float ndc_y[3] = {fmat_get(&clip, ia, 1) / wa, fmat_get(&clip, ib, 1) / wb,
                                    fmat_get(&clip, ic, 1) / wc};
            float sx[3], sy[3];
            for (int k = 0; k < 3; k++) {
                sx[k] = (ndc_x[k] * 0.5f + 0.5f) * (float)WIDTH;
                sy[k] = (1.0f - (ndc_y[k] * 0.5f + 0.5f)) * (float)HEIGHT;
            }

            // Backface culling via screen-space signed area (cross product z).
            {
                const SimdVec3 s0 = vec3_load((Vec3){sx[0], sy[0], 0.0f});
                const SimdVec3 e1 = vec3_sub(vec3_load((Vec3){sx[1], sy[1], 0.0f}), s0);
                const SimdVec3 e2 = vec3_sub(vec3_load((Vec3){sx[2], sy[2], 0.0f}), s0);
                if (vec3_cross(e1, e2).z <= 0.0f) {
                    triangles_culled++;
                    continue;
                }
            }

            // Face normal from world-space edges (uniform scale => valid).
            Vec3 p0 = {fmat_get(&world, ia, 0), fmat_get(&world, ia, 1), fmat_get(&world, ia, 2)};
            Vec3 p1 = {fmat_get(&world, ib, 0), fmat_get(&world, ib, 1), fmat_get(&world, ib, 2)};
            Vec3 p2 = {fmat_get(&world, ic, 0), fmat_get(&world, ic, 1), fmat_get(&world, ic, 2)};
            SimdVec3 n = vec3_normalize(
                vec3_cross(vec3_sub(vec3_load(p1), vec3_load(p0)), vec3_sub(vec3_load(p2), vec3_load(p0))));

            // Half-Lambert flat shading: keeps the visible top from going
            // muddy-dark while still giving a clear light gradient.
            const float diffuse = vec3_dot(n, light_dir);
            const float shade = 0.18f + 0.82f * (0.5f + 0.5f * diffuse);
            const uint8_t r = (uint8_t)(shade * 90.0f);
            const uint8_t g = (uint8_t)(shade * 170.0f);
            const uint8_t b = (uint8_t)(shade * 235.0f);

            // Bounding box, clamped to screen.
            int min_x = (int)fmaxf(0.0f, floorf(fminf(fminf(sx[0], sx[1]), sx[2])));
            int max_x = (int)fminf((float)WIDTH - 1, ceilf(fmaxf(fmaxf(sx[0], sx[1]), sx[2])));
            int min_y = (int)fmaxf(0.0f, floorf(fminf(fminf(sy[0], sy[1]), sy[2])));
            int max_y = (int)fminf((float)HEIGHT - 1, ceilf(fmaxf(fmaxf(sy[0], sy[1]), sy[2])));
            if (min_x > max_x || min_y > max_y) {
                triangles_culled++;
                continue;
            }
            triangles_drawn++;

            // Screen-space barycentric coordinates from the library.
            const SimdVec3 sv0 = vec3_load((Vec3){sx[0], sy[0], 0.0f});
            const SimdVec3 sv1 = vec3_load((Vec3){sx[1], sy[1], 0.0f});
            const SimdVec3 sv2 = vec3_load((Vec3){sx[2], sy[2], 0.0f});

            const float inv_w[3] = {1.0f / wa, 1.0f / wb, 1.0f / wc};

            for (int y = min_y; y <= max_y; y++) {
                for (int x = min_x; x <= max_x; x++) {
                    const Vec3 bary =
                        vec3_barycentric(sv0, sv1, sv2, vec3_load((Vec3){(float)x + 0.5f, (float)y + 0.5f, 0.0f}));
                    if (bary.x < 0.0f || bary.y < 0.0f || bary.z < 0.0f) continue;

                    // Perspective-correct depth: interpolate 1/w linearly in
                    // screen space, larger 1/w means closer to the camera.
                    const float inv_w_interp = bary.x * inv_w[0] + bary.y * inv_w[1] + bary.z * inv_w[2];

                    float* depth_ptr = &fb.depth[y * WIDTH + x];
                    if (inv_w_interp > *depth_ptr) {
                        *depth_ptr = inv_w_interp;
                        uint8_t* px = &fb.color[(y * WIDTH + x) * 3];
                        px[0] = r;
                        px[1] = g;
                        px[2] = b;
                        pixels_shaded += 1.0;
                    }
                }
            }
        }
    }

    xtime_now(&ts_end);
    int64_t elapsed_nanos = 0;
    if (xtime_diff_nanos(&ts_end, &ts_start, &elapsed_nanos) != XTIME_OK) {
        fprintf(stderr, "xtime_diff_nanos failed\n");
        return 1;
    }
    const double elapsed = (double)elapsed_nanos / 1e9;

    // Statistics -----------------------------------------------------------
    printf("render time       : %.3f s (%d frames, %.2f ms/frame)\n", elapsed, FRAMES, elapsed * 1000.0 / FRAMES);
    printf("triangles/frame   : %zu rasterized, %zu culled (%.0f%% backface/skip rate)\n", triangles_drawn / FRAMES,
           triangles_culled / FRAMES, 100.0 * triangles_culled / (triangles_drawn + triangles_culled));
    printf("throughput        : %.1f Mtriangle-tests/s, %.2f Mpixels shaded/s\n",
           (double)FRAMES * (mesh.index_count / 3.0f) / elapsed / 1e6, pixels_shaded / elapsed / 1e6);

    if (fb_write_ppm(&fb, "torus.ppm") == 0) {
        printf("wrote torus.ppm (%dx%d)\n", WIDTH, HEIGHT);
    } else {
        printf("failed to write torus.ppm\n");
    }

    // Cleanup ---------------------------------------------------------------
    mesh_destroy(&mesh);
    fb_destroy(&fb);
    fmat_destroy(&world);
    fmat_destroy(&clip);
    return 0;
}
