#ifndef BENCHMARK_H
#define BENCHMARK_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "include/threadpool.h"

typedef struct {
    struct timespec start_time;
    struct timespec end_time;
    uint64_t elapsed_ns;
} Benchmark;

void start_benchmark(Benchmark* bench);
void stop_benchmark(Benchmark* bench);
double latency_ms(Benchmark* bench);
double throughput(Benchmark* bench, uint64_t operations);

#endif  // BENCHMARK_H

void start_benchmark(Benchmark* bench) {
    clock_gettime(CLOCK_MONOTONIC, &bench->start_time);
}

void stop_benchmark(Benchmark* bench) {
    clock_gettime(CLOCK_MONOTONIC, &bench->end_time);
    bench->elapsed_ns = ((unsigned long)(bench->end_time.tv_sec - bench->start_time.tv_sec) * 1000000000UL +
                         ((unsigned long)bench->end_time.tv_nsec - (unsigned long)bench->start_time.tv_nsec));
}

double latency_ms(Benchmark* bench) {
    return (double)bench->elapsed_ns / 1000000.0;
}

double throughput(Benchmark* bench, uint64_t operations) {
    if (bench->elapsed_ns == 0)
        return 0.0;
    return (double)operations / ((double)bench->elapsed_ns / 1000000000.0);
}

// Perform a dummy operation with configurable workload
static void sample_operation(void* arg) {
    size_t workload   = *(size_t*)arg;
    volatile size_t s = 0;
    for (size_t i = 0; i < workload; i++) {
        s += i;
    }

    // Prevent compiler from optimizing out the loop
    if (s == 0) {
        printf("This should never happen\n");
    }
}

// Function to get current CPU time of the process
double get_cpu_time() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return (double)usage.ru_utime.tv_sec + (double)usage.ru_utime.tv_usec / 1000000.0 +
               (double)usage.ru_stime.tv_sec + (double)usage.ru_stime.tv_usec / 1000000.0;
    }
    return 0.0;
}

// Benchmark function
void run_benchmark(int num_threads, size_t iterations, size_t workload) {
    Benchmark bench  = {0};
    ThreadPool* pool = threadpool_create(num_threads);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        return;
    }

    double cpu_time_start = get_cpu_time();
    start_benchmark(&bench);

    for (size_t i = 0; i < iterations; i++) {
        threadpool_submit(pool, sample_operation, &workload);
    }

    stop_benchmark(&bench);
    double cpu_time_end = get_cpu_time();

    threadpool_destroy(pool);

    double wall_time        = latency_ms(&bench) / 1000.0;  // Convert to seconds
    double cpu_time         = cpu_time_end - cpu_time_start;
    double tasks_per_second = throughput(&bench, iterations);

    printf("Threads: %d, Iterations: %zu, Workload: %zu\n", num_threads, iterations, workload);
    printf("Wall Time: %.3f seconds\n", wall_time);
    printf("CPU Time: %.3f seconds\n", cpu_time);
    printf("CPU Utilization: %.2f%%\n", (cpu_time / wall_time / num_threads) * 100);
    printf("Throughput: %.2f tasks/second\n", tasks_per_second);
    printf("Average Latency: %.3f ms\n", wall_time * 1000 / (double)iterations);
    printf("\n");
}

int main() {
    srand((unsigned int)time(NULL));

    // Test different thread counts
    int thread_counts[] = {1, 2, 4, 8, 16};
    size_t iterations   = 100000;
    size_t workload     = 10000;

    for (size_t i = 0; i < sizeof(thread_counts) / sizeof(thread_counts[0]); i++) {
        int num_threads = thread_counts[i];
        run_benchmark(num_threads, iterations, workload);
    }

    // Test different workloads
    size_t workloads[] = {1000, 10000, 100000};
    int num_threads    = 8;

    for (size_t i = 0; i < sizeof(workloads) / sizeof(workloads[0]); i++) {
        workload = workloads[i];
        run_benchmark(num_threads, iterations, workload);
    }

    return 0;
}
