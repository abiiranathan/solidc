#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/arena.h"

// Number of iterations for each benchmark
#define NUM_ITERATIONS 1024

#ifndef NUM_THREADS
#define NUM_THREADS 8
#endif

// Structure to pass data to threads
typedef struct ThreadData {
    Arena* arena;
    size_t size;
    size_t n;
    double* elapsed_time;
    double* throughput;
    void* (*allocator_func)(struct ThreadData*);
} ThreadData;

void* arena_allocator(ThreadData* data) {
    volatile void* ptr = arena_alloc(data->arena, data->size);
    if (ptr == NULL) {
        fprintf(stderr, "larena_alloc failed\n");
        exit(1);
    }

    ptr = arena_strdup(data->arena, "Hello World");

    // Resets should be rare!
    arena_reset(data->arena);
    return (void*)ptr;
}

void* malloc_allocator(ThreadData* data) {
    char* ptr = malloc(data->size);
    if (ptr == NULL) {
        perror("malloc");
        exit(1);
    }
    strncpy(ptr, "Hello World", data->size - 1);
    ptr[data->size - 1] = '\0';
    return ptr;
}

// Function to run in each thread
void* thread_runner(void* arg) {
    ThreadData* data = (ThreadData*)arg;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < data->n; i++) {
        void* ptr = data->allocator_func(data);
        if (ptr == NULL) {
            pthread_exit(NULL);
        }

        if (data->allocator_func == malloc_allocator) {
            free(ptr);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed        = (double)((end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)) / 1e9;
    *(data->elapsed_time) = elapsed;
    *(data->throughput)   = (double)data->n / elapsed;  // Allocations per second

    return NULL;
}

// Benchmarking function
double benchmark(ThreadData* thread_data, void* (*allocator_func)(ThreadData*), double* total_throughput) {

    // Declare the arrays as usual
    pthread_t threads[NUM_THREADS];
    double elapsed_times[NUM_THREADS];
    double throughputs[NUM_THREADS];

    memset(elapsed_times, 0, sizeof(double) * NUM_THREADS);
    memset(throughputs, 0, sizeof(double) * NUM_THREADS);

    // Run threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].elapsed_time   = &elapsed_times[i];
        thread_data[i].throughput     = &throughputs[i];
        thread_data[i].allocator_func = allocator_func;
        if (pthread_create(&threads[i], NULL, (void* (*)(void*))thread_runner, &thread_data[i]) != 0) {
            perror("pthread_create failed");
            return -1.0;
        }
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result = NULL;
        if (pthread_join(threads[i], &result) != 0) {
            perror("pthread_join failed");
            return -1.0;
        }
    }

    // Calculate total elapsed time and throughput
    double total_elapsed = 0;
    *total_throughput    = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_elapsed += elapsed_times[i];
        *total_throughput += throughputs[i];
    }
    return total_elapsed / NUM_THREADS;
}

int main() {
    static const size_t size = 1024;
    static const size_t n    = 10000;

    // Benchmark configurations
    double total_arena_time        = 0;
    double total_malloc_time       = 0;
    double total_arena_throughput  = 0;
    double total_malloc_throughput = 0;

    ThreadData thread_data[NUM_THREADS];
    Arena* arenas[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        arenas[i] = arena_create(1024);
        if (arenas[i] == NULL) {
            printf("Error allocating arenas\n");
            return 1;
        }
    }

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for (int i = 0; i < NUM_THREADS; i++) {
            thread_data[i].arena = arenas[i];
            thread_data[i].size  = size;

            // Distribute allocations among threads
            thread_data[i].n = n / (size_t)NUM_THREADS;
        }

        total_arena_time += benchmark(thread_data, arena_allocator, &total_arena_throughput);

        // Benchmark malloc
        for (int i = 0; i < NUM_THREADS; i++) {
            thread_data[i].arena = NULL;
            thread_data[i].size  = size;
            thread_data[i].n     = n / (size_t)NUM_THREADS;
        }
        total_malloc_time += benchmark(thread_data, malloc_allocator, &total_malloc_throughput);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        arena_destroy(arenas[i]);
    }

    // Print average results
    printf(
        "larena_alloc (average over %d runs): %.6f ms, throughput: %.2f "
        "allocations/s\n",
        NUM_ITERATIONS, (total_arena_time / NUM_ITERATIONS) * 1e3, total_arena_throughput / NUM_ITERATIONS);

    printf(
        "malloc      (average over %d runs): %.6f ms, throughput: %.2f "
        "allocations/s\n",
        NUM_ITERATIONS, (total_malloc_time / NUM_ITERATIONS) * 1e3, total_malloc_throughput / NUM_ITERATIONS);

    return 0;
}
