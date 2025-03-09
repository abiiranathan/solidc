#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "include/arena.h"
#include "include/memory_pool.h"

// Number of iterations for each benchmark
#define NUM_ITERATIONS 5

// Structure to pass data to threads
typedef struct ThreadData {
    Arena* arena;
    MemoryPool* pool;
    size_t size;
    size_t n;
    double* elapsed_time;
    double* throughput;
    void* (*allocator_func)(struct ThreadData*);
} ThreadData;

void* arena_allocator(ThreadData* data) {
    // use thread-local arena.
    arena_threadlocal(data->arena);
    void* ptr = arena_alloc(NULL, data->size);
    arena_reset(data->arena);
    return ptr;
}

void* malloc_allocator(ThreadData* data) {
    return malloc(data->size);
}

void* memory_pool_allocator(ThreadData* data) {
    return mpool_alloc(data->pool, data->size);
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
    double elapsed        = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    *(data->elapsed_time) = elapsed;
    *(data->throughput)   = data->n / elapsed;  // Allocations per second

    return NULL;
}

// Benchmarking function
double benchmark(int num_threads, ThreadData* thread_data, void* (*allocator_func)(ThreadData*),
                 double* total_throughput) {
    pthread_t threads[num_threads];
    double elapsed_times[num_threads];
    double throughputs[num_threads];

    memset(elapsed_times, 0, sizeof(double) * num_threads);
    memset(throughputs, 0, sizeof(double) * num_threads);

    // Run threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].elapsed_time   = &elapsed_times[i];
        thread_data[i].throughput     = &throughputs[i];
        thread_data[i].allocator_func = allocator_func;
        if (pthread_create(&threads[i], NULL, (void* (*)(void*))thread_runner, &thread_data[i]) !=
            0) {
            perror("pthread_create failed");
            return -1.0;
        }
    }

    // Join threads
    for (int i = 0; i < num_threads; i++) {
        void* result;
        if (pthread_join(threads[i], &result) != 0) {
            perror("pthread_join failed");
            return -1.0;
        }
    }

    // Calculate total elapsed time and throughput
    double total_elapsed = 0;
    *total_throughput    = 0;
    for (int i = 0; i < num_threads; i++) {
        total_elapsed += elapsed_times[i];
        *total_throughput += throughputs[i];
    }
    return total_elapsed / num_threads;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <size> <n> <threads>\n", argv[0]);
        return 1;
    }

    size_t size     = atoi(argv[1]);
    size_t n        = atoi(argv[2]);
    int num_threads = atoi(argv[3]);

    if (num_threads <= 0) {
        fprintf(stderr, "Number of threads must be positive.\n");
        return 1;
    }

    ThreadData thread_data[num_threads];

    // Benchmark configurations
    double total_arena_time        = 0;
    double total_malloc_time       = 0;
    double total_pool_time         = 0;
    double total_arena_throughput  = 0;
    double total_malloc_throughput = 0;
    double total_pool_throughput   = 0;

    Arena* arenas[num_threads];

    for (int i = 0; i < num_threads; i++) {
        arenas[i] = arena_create(ARENA_DEFAULT_CHUNKSIZE * 5);
        if (arenas[i] == NULL) {
            printf("Error allocating arenas\n");
            return 1;
        }
    }

    // Benchmark memory_pool_alloc
    MemoryPool* pool = mpool_create(500 * 1024 * 1024);
    if (pool == NULL) {
        fprintf(stderr, "memory_pool_create failed\n");
        return 1;
    }

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for (int i = 0; i < num_threads; i++) {
            thread_data[i].arena = arenas[i];
            thread_data[i].size  = size;
            thread_data[i].n     = n / num_threads;  // Distribute allocations among threads
        }
        total_arena_time +=
            benchmark(num_threads, thread_data, arena_allocator, &total_arena_throughput);

        // Benchmark malloc
        for (int i = 0; i < num_threads; i++) {
            thread_data[i].arena = NULL;  // Not used for malloc
            thread_data[i].size  = size;
            thread_data[i].n     = n / num_threads;
        }
        total_malloc_time +=
            benchmark(num_threads, thread_data, malloc_allocator, &total_malloc_throughput);

        for (int i = 0; i < num_threads; i++) {
            thread_data[i].pool = pool;
            thread_data[i].size = size;
            thread_data[i].n    = n / num_threads;
        }
        total_pool_time +=
            benchmark(num_threads, thread_data, memory_pool_allocator, &total_pool_throughput);
    }

    for (int i = 0; i < num_threads; i++) {
        arena_destroy(arenas[i]);
    }

    mpool_destroy(pool);

    // Print average results
    printf("arena_alloc (average over %d runs): %.6f ms, throughput: %.2f allocations/s\n",
           NUM_ITERATIONS, (total_arena_time / NUM_ITERATIONS) * 1e3,
           total_arena_throughput / NUM_ITERATIONS);
    printf("malloc (average over %d runs): %.6f ms, throughput: %.2f allocations/s\n",
           NUM_ITERATIONS, (total_malloc_time / NUM_ITERATIONS) * 1e3,
           total_malloc_throughput / NUM_ITERATIONS);
    printf("memory_pool_alloc (average over %d runs): %.6f ms, throughput: %.2f allocations/s\n",
           NUM_ITERATIONS, (total_pool_time / NUM_ITERATIONS) * 1e3,
           total_pool_throughput / NUM_ITERATIONS);

    return 0;
}
