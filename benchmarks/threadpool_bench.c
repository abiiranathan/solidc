#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/threadpool.h"

#define NUM_TASKS   1000000
#define NUM_THREADS 8

void dummy_task(void* arg) {
    (void)arg;

    // Simulate a CPU-bound task
    volatile int x = 0;
    for (int i = 0; i < 10000; i++) {
        x++;
    }
}

int main() {
    ThreadPool* pool = threadpool_create(NUM_THREADS);
    if (pool == NULL) {
        fprintf(stderr, "Failed to create thread pool\n");
        return 1;
    }

    // Start timing
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Submit tasks
    for (int i = 0; i < NUM_TASKS; i++) {
        if (threadpool_submit(pool, dummy_task, NULL) != 0) {
            fprintf(stderr, "Failed to add task after %d\n", i);
            break;
        }
    }

    // Wait for threads and clean up
    threadpool_destroy(pool);

    // Stop timing
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate throughput and latency
    double elapsed     = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput  = NUM_TASKS / elapsed;
    double avg_latency = elapsed / NUM_TASKS * 1e6;  // in microseconds

    printf("Throughput: %.2f tasks/sec\n", throughput);
    printf("Average Latency: %.2f µs/task\n", avg_latency);

    return 0;
}
