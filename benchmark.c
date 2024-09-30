#ifndef BENCHMARK_H
#define BENCHMARK_H
#define _GNU_SOURCE

#include <solidc/threadpool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    struct timespec start_time;
    struct timespec end_time;
    uint64_t elapsed_ms;
} Benchmark;

void start_benchmark(Benchmark* bench);
void stop_benchmark(Benchmark* bench);
double latency_ms(Benchmark* bench);
double ops_per_ms(Benchmark* bench, uint64_t operations);

#endif  // BENCHMARK_H

void start_benchmark(Benchmark* bench) {
    clock_gettime(CLOCK_MONOTONIC, &bench->start_time);
}

void stop_benchmark(Benchmark* bench) {
    clock_gettime(CLOCK_MONOTONIC, &bench->end_time);
    bench->elapsed_ms = ((bench->end_time.tv_sec - bench->start_time.tv_sec) * 1000L) +
                        ((bench->end_time.tv_nsec - bench->start_time.tv_nsec) / 1000000L);
}

double latency_ms(Benchmark* bench) {
    return (double)bench->elapsed_ms;
}

double ops_per_ms(Benchmark* bench, uint64_t operations) {
    if (bench->elapsed_ms == 0)
        return 0.0;
    return (double)operations / (double)bench->elapsed_ms;
}

// Perform a dummy operation like summing numbers
static void sample_operation(void*) {
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += i;
    }

    int rand_ms = rand() % 5;  // Sleep for a random amount of time from 0 to 5 ms
    struct timespec sleep_time = {0, rand_ms * 1000000};
    nanosleep(&sleep_time, NULL);

    // printf("Sum: %d\n", sum);
    (void)sum;
}

int main() {
    Benchmark bench = {0};
    uint64_t iterations = 100000;

    ThreadPool pool = threadpool_create(8);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        return 1;
    }

    start_benchmark(&bench);
    for (size_t i = 0; i < iterations; i++) {
        threadpool_add_task(pool, sample_operation, NULL);
    }

    threadpool_wait(pool);

    stop_benchmark(&bench);
    threadpool_destroy(pool);

    double latency = latency_ms(&bench);
    // double ops_ms = ops_per_ms(&bench, iterations);

    printf("Latency: %.2f ms\n", latency);

    return 0;
}
