// Path: bench_arena.c
// Compile: gcc bench_arena.c src/arena.c src/lock.c -o bench_arena -lpthread -O3 -std=c11 -Wall -Wextra -Wpedantic
// Run: ./bench_arena 1024 1000000
// Arena is about 2-3x faster than malloc for 1M allocations of 1K bytes.

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "include/arena.h"

// bench arena_alloc
void bench_arena_alloc(Arena* arena, size_t size, size_t n) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < n; i++) {
        void* ptr = arena_alloc(arena, size);
        if (ptr == NULL) {
            fprintf(stderr, "arena_alloc failed\n");
            exit(1);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("arena_alloc: %.6f ms\n", elapsed * 1e3);
    arena_destroy(arena);
}

// bench malloc
void bench_malloc(size_t size, size_t n) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < n; i++) {
        void* ptr = malloc(size);
        if (ptr == NULL) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("malloc: %.6f ms\n", elapsed * 1e3);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <size> <n>\n", argv[0]);
        return 1;
    }

    size_t size = atoi(argv[1]);
    size_t n = atoi(argv[2]);

    Arena* arena = arena_create(ARENA_DEFAULT_CHUNKSIZE);
    bench_arena_alloc(arena, size, n);
    bench_malloc(size, n);

    return 0;
}
