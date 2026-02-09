#include "../include/linear_alg.h"
#include "../include/matrix.h"
#include "../include/vec.h"
#include "../include/thread.h" // For sleep_ms
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define WIDTH 80
#define HEIGHT 40

// Screen buffer
char buffer[HEIGHT][WIDTH];
float z_buffer[HEIGHT][WIDTH]; // Simple Z-buffer for depth testing

const char SHADES[] = ".,-~:;=!*#$@"; // Ascii gradients

void clear_buffer() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            buffer[y][x] = ' ';
            z_buffer[y][x] = 1000.0f; // Far away
        }
    }
}

void print_buffer() {
    printf("\033[H"); // Home cursor
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            putchar(buffer[y][x]);
        }
        putchar('\n');
    }
}

// Bresenham's Line Algorithm with Z-interpolation (uncorrected)
void draw_line(int x0, int y0, float z0, int x1, int y1, float z1, char c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    float total_dist = sqrtf((float)(dx*dx + dy*dy));
    if (total_dist < 1e-5f) total_dist = 1.0f;

    while (1) {
        if (x0 >= 0 && x0 < WIDTH && y0 >= 0 && y0 < HEIGHT) {
            // Calculate current Z (Linear interpolation)
            float current_dist = sqrtf((float)((x0-x1)*(x0-x1) + (y0-y1)*(y0-y1)));
            float t = 1.0f - (current_dist / total_dist);
            float z = z0 + (z1 - z0) * t;

            // Z-Test
            if (z < z_buffer[y0][x0]) {
                z_buffer[y0][x0] = z;
                buffer[y0][x0] = c;
            }
        }

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

typedef struct {
    Vec3* vertices;
    int vertex_count;
    int (*edges)[2];
    int edge_count;
} Mesh;

// Generate a Torus (Donut) mesh
Mesh create_torus(float r1, float r2, int segments, int sides) {
    Mesh m;
    m.vertex_count = segments * sides;
    m.vertices = (Vec3*)malloc(sizeof(Vec3) * (size_t)m.vertex_count);
    
    m.edge_count = m.vertex_count * 2; // Vertical + Horizontal lines
    m.edges = (int(*)[2])malloc(sizeof(int[2]) * (size_t)m.edge_count);

    int v_idx = 0;
    for (int i = 0; i < segments; i++) {
        float theta = (float)i / (float)segments * 2.0f * (float)M_PI;
        for (int j = 0; j < sides; j++) {
            float phi = (float)j / (float)sides * 2.0f * (float)M_PI;
            
            // Torus parametric equation
            float x = (r1 + r2 * cosf(phi)) * cosf(theta);
            float z = (r1 + r2 * cosf(phi)) * sinf(theta); // Up is Y in our math, but let's make Z depth
            float y = r2 * sinf(phi);

            // Rotate to lie flat on XZ plane initially
            m.vertices[v_idx] = (Vec3){x, y, z};
            v_idx++;
        }
    }

    // Generate edges
    int e_idx = 0;
    for (int i = 0; i < segments; i++) {
        for (int j = 0; j < sides; j++) {
            int current = i * sides + j;
            int next_side = i * sides + ((j + 1) % sides);
            int next_seg = ((i + 1) % segments) * sides + j;

            m.edges[e_idx][0] = current;
            m.edges[e_idx][1] = next_side;
            e_idx++;

            m.edges[e_idx][0] = current;
            m.edges[e_idx][1] = next_seg;
            e_idx++;
        }
    }

    return m;
}

int main() {
    printf("\033[2J"); // Clear screen

    // Create Mesh
    Mesh donut = create_torus(2.0f, 1.0f, 32, 16);

    float angleX = 0.0f;
    float angleY = 0.0f;

    // Fixed Projection
    float aspect = (float)WIDTH / (float)HEIGHT * 0.5f; // 0.5 for char aspect ratio
    Mat4 proj = mat4_perspective(1.5f, aspect, 0.1f, 100.0f);

    // Camera
    SimdVec3 eye = vec3_load((Vec3){0.0f, 0.0f, 6.0f});
    SimdVec3 target = vec3_load((Vec3){0.0f, 0.0f, 0.0f});
    SimdVec3 up = vec3_load((Vec3){0.0f, 1.0f, 0.0f});
    Mat4 view = mat4_look_at(eye, target, up);

    while (1) {
        clear_buffer();

        // Update Rotation
        Mat4 rotX = mat4_rotate_x(angleX);
        Mat4 rotY = mat4_rotate_y(angleY);
        Mat4 model = mat4_mul(rotX, rotY);

        Mat4 mvp = mat4_mul(proj, mat4_mul(view, model));

        // Transform Vertices
        Vec3* screen_coords = (Vec3*)malloc(sizeof(Vec3) * (size_t)donut.vertex_count);
        bool* visible = (bool*)malloc(sizeof(bool) * (size_t)donut.vertex_count);

        for (int i = 0; i < donut.vertex_count; i++) {
            SimdVec4 v_in = vec4_load((Vec4){donut.vertices[i].x, donut.vertices[i].y, donut.vertices[i].z, 1.0f});
            SimdVec4 v_out_s = vec4_load(mat4_mul_vec4(mvp, vec4_store(v_in)));
            Vec4 v_out = vec4_store(v_out_s);

            // Clip behind camera
            if (v_out.w <= 0.0f) {
                visible[i] = false;
                continue;
            }

            visible[i] = true;
            
            // Perspective Divide
            float inv_w = 1.0f / v_out.w;
            v_out.x *= inv_w;
            v_out.y *= inv_w;
            
            // Viewport Transform
            screen_coords[i].x = (v_out.x + 1.0f) * 0.5f * WIDTH;
            screen_coords[i].y = (1.0f - v_out.y) * 0.5f * HEIGHT;
            screen_coords[i].z = v_out.w; // Depth
        }

        // Draw Edges
        for (int i = 0; i < donut.edge_count; i++) {
            int idx0 = donut.edges[i][0];
            int idx1 = donut.edges[i][1];

            if (visible[idx0] && visible[idx1]) {
                // Determine shading based on depth (closer = brighter)
                float avg_z = (screen_coords[idx0].z + screen_coords[idx1].z) * 0.5f;
                int shade_idx = (int)((avg_z - 4.0f) * 4.0f); // Map depth 4..8 to 0..12 roughly
                if (shade_idx < 0) shade_idx = 0;
                if ((size_t)shade_idx >= sizeof(SHADES)-2) shade_idx = (int)(sizeof(SHADES)-3);
                
                // Inverse: closer is smaller Z, we want brighter char.
                // Actually, let's just use a fixed char for wireframe, 
                // or use normal based shading if we calculated normals.
                // Simple depth shading:
                char c = SHADES[sizeof(SHADES) - 2 - (size_t)shade_idx];

                draw_line((int)screen_coords[idx0].x, (int)screen_coords[idx0].y, screen_coords[idx0].z,
                          (int)screen_coords[idx1].x, (int)screen_coords[idx1].y, screen_coords[idx1].z,
                          c);
            }
        }

        print_buffer();
        printf("SolidC Graphics Demo | Angle: %.2f | Z-Buffered Wireframe Torus\n", angleX);

        free(screen_coords);
        free(visible);

        angleX += 0.05f;
        angleY += 0.03f;
        sleep_ms(33);
    }

    return 0;
}
