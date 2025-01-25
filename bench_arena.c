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
    void* (*allocator_func)(struct ThreadData*);
} ThreadData;

void* arena_allocator(ThreadData* data) {
    return arena_alloc(data->arena, data->size);
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
            fprintf(stderr, "Allocation failed in thread\n");
            pthread_exit(NULL);
        }

        if (data->allocator_func == malloc_allocator) {
            free(ptr);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    *(data->elapsed_time) = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    return NULL;
}

// Benchmarking function
double benchmark(int num_threads, ThreadData* thread_data, void* (*allocator_func)(ThreadData*)) {
    pthread_t threads[num_threads];
    double elapsed_times[num_threads];

    memset(elapsed_times, 0, sizeof(double) * num_threads);

    // Run threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].elapsed_time = &elapsed_times[i];
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

    // Calculate total elapsed time
    double total_elapsed = 0;
    for (int i = 0; i < num_threads; i++) {
        total_elapsed += elapsed_times[i];
    }
    return total_elapsed / num_threads;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <size> <n> <threads>\n", argv[0]);
        return 1;
    }

    size_t size = atoi(argv[1]);
    size_t n = atoi(argv[2]);
    int num_threads = atoi(argv[3]);

    if (num_threads <= 0) {
        fprintf(stderr, "Number of threads must be positive.\n");
        return 1;
    }

    ThreadData thread_data[num_threads];

    // Benchmark configurations
    double total_arena_time = 0;
    double total_malloc_time = 0;
    double total_pool_time = 0;

    Arena* arena = arena_create(ARENA_DEFAULT_CHUNKSIZE * 100);

    // Benchmark memory_pool_alloc
    MemoryPool* pool = mpool_create(500 * 1024 * 1024);
    if (pool == NULL) {
        fprintf(stderr, "memory_pool_create failed\n");
        return 1;
    }

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        // Benchmark arena_alloc
        if (arena == NULL) {
            fprintf(stderr, "arena_create failed\n");
            return 1;
        }
        for (int i = 0; i < num_threads; i++) {
            thread_data[i].arena = arena;
            thread_data[i].size = size;
            thread_data[i].n = n / num_threads;  // Distribute allocations among threads
        }
        total_arena_time += benchmark(num_threads, thread_data, arena_allocator);

        // Benchmark malloc
        for (int i = 0; i < num_threads; i++) {
            thread_data[i].arena = NULL;  // Not used for malloc
            thread_data[i].size = size;
            thread_data[i].n = n / num_threads;
        }
        total_malloc_time += benchmark(num_threads, thread_data, malloc_allocator);

        for (int i = 0; i < num_threads; i++) {
            thread_data[i].pool = pool;
            thread_data[i].size = size;
            thread_data[i].n = n / num_threads;
        }
        total_pool_time += benchmark(num_threads, thread_data, memory_pool_allocator);
    }

    arena_destroy(arena);
    mpool_destroy(pool);

    // Print average results
    printf("arena_alloc (average over %d runs): %.6f ms\n", NUM_ITERATIONS,
           (total_arena_time / NUM_ITERATIONS) * 1e3);
    printf("malloc (average over %d runs): %.6f ms\n", NUM_ITERATIONS,
           (total_malloc_time / NUM_ITERATIONS) * 1e3);
    printf("memory_pool_alloc (average over %d runs): %.6f ms\n", NUM_ITERATIONS,
           (total_pool_time / NUM_ITERATIONS) * 1e3);

    return 0;
}

/*

+------------+------------+----------+---------------+------------------+--------------------+
| Block Size | Iterations | Threads  | arena_alloc   | malloc           | memory_pool_alloc  |
+------------+------------+----------+---------------+------------------+--------------------+
|    4,096   |   10,000   |    4     |    0.291 ms   |    8.305 ms      |     0.036 ms       |
+------------+------------+----------+---------------+------------------+--------------------+
|    4,096   |  100,000   |    4     |    3.137 ms   |   69.310 ms      |     0.222 ms       |
+------------+------------+----------+---------------+------------------+--------------------+
|    4,096   |  100,000   |    8     |    3.924 ms   |   80.153 ms      |     0.124 ms       |
+------------+------------+----------+---------------+------------------+--------------------+
|   40,960   |  100,000   |    8     |    7.884 ms   |   83.703 ms      |     0.130 ms       |
+------------+------------+----------+---------------+------------------+--------------------+
|  409,600   |   10,000   |    8     |    5.320 ms   |   18.295 ms      |     0.035 ms       |
+------------+------------+----------+---------------+------------------+--------------------+
|  409,600   |  100,000   |    8     |   35.202 ms   |  179.224 ms      |     2.913 ms       |
+------------+------------+----------+---------------+------------------+--------------------+

Performance Observations:
1. memory_pool_alloc consistently outperforms malloc
2. Speedup ranges from 60x to 600x faster than malloc
3. Performance remains efficient across various block sizes and thread counts

*/
