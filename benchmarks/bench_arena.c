#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/arena.h"

// Benchmark configuration
#define WARMUP_ITERATIONS         100
#define MEASUREMENT_ITERATIONS    10000
#define ALLOCATIONS_PER_ITERATION 10000
#define ALLOCATION_SIZE           1024

/** Statistics for a benchmark run. */
typedef struct {
    double min_ns;      // Minimum time in nanoseconds
    double max_ns;      // Maximum time in nanoseconds
    double avg_ns;      // Average time in nanoseconds
    double median_ns;   // Median time in nanoseconds
    double stddev_ns;   // Standard deviation in nanoseconds
    double throughput;  // Allocations per second
} BenchmarkStats;

/** Returns current time in nanoseconds. */
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/** Comparison function for qsort (for median calculation). */
static int compare_double(const void* a, const void* b) {
    double diff = *(const double*)a - *(const double*)b;
    return (diff > 0) - (diff < 0);
}

/**
 * Calculates statistics from an array of timing samples.
 * @param samples Array of timing samples in nanoseconds.
 * @param count Number of samples.
 * @param stats Output statistics structure.
 */
static void calculate_stats(double* samples, size_t count, BenchmarkStats* stats) {
    if (count == 0) {
        memset(stats, 0, sizeof(BenchmarkStats));
        return;
    }

    // Sort for median calculation
    qsort(samples, count, sizeof(double), compare_double);

    // Min, max, median
    stats->min_ns    = samples[0];
    stats->max_ns    = samples[count - 1];
    stats->median_ns = (count % 2 == 0) ? (samples[count / 2 - 1] + samples[count / 2]) / 2.0 : samples[count / 2];

    // Mean
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += samples[i];
    }
    stats->avg_ns = sum / (double)count;

    // Standard deviation
    double variance_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = samples[i] - stats->avg_ns;
        variance_sum += diff * diff;
    }
    stats->stddev_ns = sqrt(variance_sum / (double)count);

    // Throughput (allocations per second)
    stats->throughput = (ALLOCATIONS_PER_ITERATION * 1e9) / stats->avg_ns;
}

/**
 * Benchmark arena allocator.
 * @param samples Output array for timing samples (caller must allocate).
 * @param num_samples Number of measurements to take.
 * @param warmup If true, perform warmup without recording times.
 */
static void benchmark_arena(double* samples, size_t num_samples, bool warmup) {
    Arena* arena = arena_create(ALLOCATION_SIZE * ALLOCATIONS_PER_ITERATION * 2);
    if (!arena) {
        fprintf(stderr, "Failed to create arena\n");
        exit(1);
    }

    for (size_t iter = 0; iter < num_samples; iter++) {
        arena_reset(arena);

        uint64_t start = get_time_ns();

        // Perform allocations
        for (size_t i = 0; i < ALLOCATIONS_PER_ITERATION; i++) {
            void* ptr = arena_alloc(arena, ALLOCATION_SIZE);
            if (!ptr) {
                fprintf(stderr, "Arena allocation failed\n");
                exit(1);
            }
            // Touch the memory to ensure it's actually allocated
            ((volatile char*)ptr)[0]                   = 1;
            ((volatile char*)ptr)[ALLOCATION_SIZE - 1] = 1;
        }

        uint64_t end = get_time_ns();

        if (!warmup) {
            samples[iter] = (double)(end - start);
        }
    }

    arena_destroy(arena);
}

/**
 * Benchmark malloc/free allocator.
 * @param samples Output array for timing samples (caller must allocate).
 * @param num_samples Number of measurements to take.
 * @param warmup If true, perform warmup without recording times.
 */
static void benchmark_malloc(double* samples, size_t num_samples, bool warmup) {
    for (size_t iter = 0; iter < num_samples; iter++) {
        void* ptrs[ALLOCATIONS_PER_ITERATION];

        uint64_t start = get_time_ns();

        // Perform allocations
        for (size_t i = 0; i < ALLOCATIONS_PER_ITERATION; i++) {
            ptrs[i] = malloc(ALLOCATION_SIZE);
            if (!ptrs[i]) {
                fprintf(stderr, "malloc failed\n");
                exit(1);
            }
            // Touch the memory
            ((volatile char*)ptrs[i])[0]                   = 1;
            ((volatile char*)ptrs[i])[ALLOCATION_SIZE - 1] = 1;
        }

        // Free all allocations
        for (size_t i = 0; i < ALLOCATIONS_PER_ITERATION; i++) {
            free(ptrs[i]);
        }

        uint64_t end = get_time_ns();

        if (!warmup) {
            samples[iter] = (double)(end - start);
        }
    }
}

/**
 * Prints a formatted table row.
 */
static void print_row(const char* allocator, const BenchmarkStats* stats) {
    printf("│ %-12s │ %10.2f │ %10.2f │ %10.2f │ %10.2f │ %10.2f │ %14.0f │\n", allocator,
           stats->min_ns / 1000.0,  // Convert to microseconds
           stats->avg_ns / 1000.0, stats->median_ns / 1000.0, stats->max_ns / 1000.0, stats->stddev_ns / 1000.0,
           stats->throughput);
}

/**
 * Prints the benchmark header.
 */
static void print_header(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                          Arena Allocator Benchmark Results                                   ║\n");
    printf("╠══════════════╦════════════╦════════════╦════════════╦════════════╦════════════╦════════════════╣\n");
    printf("║  Allocator   ║  Min (μs)  ║  Avg (μs)  ║  Med (μs)  ║  Max (μs)  ║ StdDev (μs)║  Throughput    ║\n");
    printf("╠══════════════╬════════════╬════════════╬════════════╬════════════╬════════════╬════════════════╣\n");
}

/**
 * Prints the benchmark footer with speedup information.
 */
static void print_footer(double arena_avg, double malloc_avg) {
    double speedup = malloc_avg / arena_avg;

    printf("╚══════════════╩════════════╩════════════╩════════════╩════════════╩════════════╩════════════════╝\n");
    printf("\n");
    printf("Configuration:\n");
    printf("  • Warmup iterations:      %d\n", WARMUP_ITERATIONS);
    printf("  • Measurement iterations: %d\n", MEASUREMENT_ITERATIONS);
    printf("  • Allocations per iter:   %d\n", ALLOCATIONS_PER_ITERATION);
    printf("  • Allocation size:        %d bytes\n", ALLOCATION_SIZE);
    printf("\n");

    if (speedup > 1.0) {
        printf("Result: Arena is %.2fx faster than malloc\n", speedup);
    } else {
        printf("Result: malloc is %.2fx faster than Arena\n", 1.0 / speedup);
    }
    printf("\n");
}

int main(void) {
    printf("Starting benchmark...\n");
    printf("Performing warmup runs...\n");

    // Warmup phase
    benchmark_arena(NULL, WARMUP_ITERATIONS, true);
    benchmark_malloc(NULL, WARMUP_ITERATIONS, true);

    // Allocate sample arrays
    double* arena_samples  = malloc(sizeof(double) * MEASUREMENT_ITERATIONS);
    double* malloc_samples = malloc(sizeof(double) * MEASUREMENT_ITERATIONS);

    if (!arena_samples || !malloc_samples) {
        fprintf(stderr, "Failed to allocate sample arrays\n");
        return 1;
    }

    printf("Running measurements...\n");

    // Measurement phase
    benchmark_arena(arena_samples, MEASUREMENT_ITERATIONS, false);
    benchmark_malloc(malloc_samples, MEASUREMENT_ITERATIONS, false);

    // Calculate statistics
    BenchmarkStats arena_stats, malloc_stats;
    calculate_stats(arena_samples, MEASUREMENT_ITERATIONS, &arena_stats);
    calculate_stats(malloc_samples, MEASUREMENT_ITERATIONS, &malloc_stats);

    // Print results
    print_header();
    print_row("Arena", &arena_stats);
    printf("╟──────────────┼────────────┼────────────┼────────────┼────────────┼────────────┼────────────────╢\n");
    print_row("malloc/free", &malloc_stats);
    print_footer(arena_stats.avg_ns, malloc_stats.avg_ns);

    // Cleanup
    free(arena_samples);
    free(malloc_samples);

    return 0;
}
