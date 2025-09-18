#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/fast_mem.h"

// Configuration
#define NUM_THREADS    8       // Number of threads
#define NUM_OPERATIONS 100000  // Operations per thread
#define MAX_ALLOC_SIZE 4096    // Maximum allocation size (bytes)

// Workload structure for each thread
typedef struct {
    void* (*alloc_fn)(size_t);  // Allocation function (malloc or FMALLOC)
    void (*free_fn)(void*);     // Free function (free or FFREE)
    int thread_id;              // Thread identifier
    size_t total_ops;           // Total operations completed
    double elapsed_time;        // Time taken (seconds)
} thread_data_t;

// Thread workload function
void* thread_workload(void* arg) {
    thread_data_t* data       = (thread_data_t*)arg;
    void* (*alloc_fn)(size_t) = data->alloc_fn;
    void (*free_fn)(void*)    = data->free_fn;

    // Seed random number generator per thread
    srand((unsigned int)time(NULL) ^ data->thread_id);

    // Array to store pointers for freeing later
    void** pointers = malloc(NUM_OPERATIONS * sizeof(void*));
    if (!pointers) {
        fprintf(stderr, "Failed to allocate pointer array for thread %d\n", data->thread_id);
        return NULL;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Perform allocations and store pointers
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        size_t size = (rand() % MAX_ALLOC_SIZE) + 1;  // Random size between 1 and MAX_ALLOC_SIZE
        pointers[i] = alloc_fn(size);
        if (pointers[i] == NULL && size > 0) {
            fprintf(stderr, "Allocation failed in thread %d at op %d (size %zu)\n", data->thread_id, i, size);
            break;
        }
    }

    // Free all allocated memory
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        if (pointers[i]) {
            free_fn(pointers[i]);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    data->elapsed_time = (double)((end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)) / 1e9;
    data->total_ops    = NUM_OPERATIONS * 2;  // Each alloc + free counts as an operation

    free(pointers);
    return NULL;
}

// Run benchmark for a given allocator
void run_benchmark(const char* name, void* (*alloc_fn)(size_t), void (*free_fn)(void*)) {
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    // Initialize thread data
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].alloc_fn     = alloc_fn;
        thread_data[i].free_fn      = free_fn;
        thread_data[i].thread_id    = i;
        thread_data[i].total_ops    = 0;
        thread_data[i].elapsed_time = 0.0;
    }

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_workload, &thread_data[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d for %s\n", i, name);
            exit(EXIT_FAILURE);
        }
    }

    // Wait for threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Aggregate results
    size_t total_ops  = 0;
    double total_time = 0.0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_ops += thread_data[i].total_ops;
        total_time += thread_data[i].elapsed_time;
    }
    double avg_time_per_thread = total_time / NUM_THREADS;
    double throughput          = (double)total_ops / total_time;

    printf("\nBenchmark Results for %s:\n", name);
    printf("  Total Operations: %zu (alloc + free)\n", total_ops);
    printf("  Total Time: %.3f seconds\n", total_time);
    printf("  Avg Time per Thread: %.3f seconds\n", avg_time_per_thread);
    printf("  Throughput: %.0f ops/second\n", throughput);
}

int main() {
    printf("Starting Memory Allocator Benchmark\n");
    printf("Threads: %d, Operations per Thread: %d, Max Alloc Size: %d bytes\n", NUM_THREADS, NUM_OPERATIONS,
           MAX_ALLOC_SIZE);

    // Benchmark standard malloc/free
    run_benchmark("malloc/free", malloc, free);

    // Benchmark custom FMALLOC/FFREE
    run_benchmark("FMALLOC/FFREE", FMALLOC, FFREE);

    // Optional: Debug memory state after FMALLOC benchmark
    // printf("\nFinal FMALLOC Memory State:\n");
    // FDEBUG_MEMORY();

    return 0;
}
