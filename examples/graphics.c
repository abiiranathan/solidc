#include <solidc/linear_alg.h>
#include <solidc/matrix.h>
#include <solidc/vec.h>
#include <solidc/thread.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h> // For abs

#define WIDTH 80
#define HEIGHT 40

// ASCII characters for different depths or just line drawing
const char PIXEL = '#';
const char EMPTY = ' ';

char buffer[HEIGHT][WIDTH];

void clear_buffer() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            buffer[y][x] = EMPTY;
        }
    }
}

void print_buffer() {
    // Move cursor to top-left (ANSI escape code) to redraw
    printf("\033[H"); 
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            putchar(buffer[y][x]);
        }
        putchar('\n');
    }
}

void draw_point(int x, int y, char c) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        buffer[y][x] = c;
    }
}

// Bresenham's Line Algorithm
void draw_line(int x0, int y0, int x1, int y1, char c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        draw_point(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Cube vertices
Vec3 cube_vertices[] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
};

// Cube edges (pairs of indices)
int cube_edges[][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Front face
    {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Back face
    {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connecting edges
};

int main() {
    // Clear screen once
    printf("\033[2J");

    float angle = 0.0f;

    // Projection Matrix
    float aspect = (float)WIDTH / (float)HEIGHT;
    // Aspect ratio correction for terminal characters (approx 2:1 height:width)
    aspect *= 0.5f; 
    
    Mat4 proj = mat4_perspective(1.57f, aspect, 0.1f, 100.0f); // 90 degree FOV

    // Camera setup
    SimdVec3 eye = vec3_load((Vec3){0.0f, 0.0f, 4.0f});
    SimdVec3 target = vec3_load((Vec3){0.0f, 0.0f, 0.0f});
    SimdVec3 up = vec3_load((Vec3){0.0f, 1.0f, 0.0f});
    Mat4 view = mat4_look_at(eye, target, up);

    // Animation Loop
    for (int f = 0; f < 300; f++) {
        clear_buffer();

        // Model Matrix (Rotate cube)
        Mat4 model = mat4_rotate_y(angle);
        model = mat4_mul(mat4_rotate_x(angle * 0.5f), model);

        // MVP Matrix
        Mat4 mvp = mat4_mul(proj, mat4_mul(view, model));

        // Transform and Project Vertices
        Vec2 screen_points[8];

        for (int i = 0; i < 8; i++) {
            // Load vertex
            SimdVec4 v_in = vec4_load((Vec4){cube_vertices[i].x, cube_vertices[i].y, cube_vertices[i].z, 1.0f});
            
            // Transform
            SimdVec4 v_out = vec4_load(mat4_mul_vec4(mvp, vec4_store(v_in)));
            
            Vec4 res = vec4_store(v_out);

            // Perspective Divide
            if (res.w != 0.0f) {
                res.x /= res.w;
                res.y /= res.w;
            }

            // Map to Screen Coordinates (-1..1 -> 0..WIDTH/HEIGHT)
            screen_points[i].x = (res.x + 1.0f) * 0.5f * WIDTH;
            screen_points[i].y = (1.0f - res.y) * 0.5f * HEIGHT; // Flip Y
        }

        // Draw Edges
        for (int i = 0; i < 12; i++) {
            int i0 = cube_edges[i][0];
            int i1 = cube_edges[i][1];
            draw_line((int)screen_points[i0].x, (int)screen_points[i0].y,
                      (int)screen_points[i1].x, (int)screen_points[i1].y, PIXEL);
        }

        print_buffer();
        printf("Frame: %d | Angle: %.2f\n", f, angle);
        
        angle += 0.05f;
        
        // Sleep approx 33ms (30fps)
        sleep_ms(33);
    }

    return 0;
}
