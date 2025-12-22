// benchmark.c - Comprehensive arena allocator benchmark
// Compile: gcc -O3 -march=native -Wall -Wextra -o bench benchmark.c arena.c -lm
// Run: ./bench

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "arena.h"

// Prevent compiler from optimizing away allocations
#define BENCHMARK_USE(ptr) __asm__ __volatile__("" : : "r"(ptr) : "memory")

// High-resolution timing
typedef struct {
    struct timespec start;
    struct timespec end;
} Timer;

static void timer_start(Timer* t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

static double timer_end(Timer* t) {
    clock_gettime(CLOCK_MONOTONIC, &t->end);
    double start_ns = (double)t->start.tv_sec * 1e9 + (double)t->start.tv_nsec;
    double end_ns   = (double)t->end.tv_sec * 1e9 + (double)t->end.tv_nsec;
    return end_ns - start_ns;
}

// Statistics calculation
typedef struct {
    double mean;
    double median;
    double min;
    double max;
    double stddev;
} Stats;

static int compare_double(const void* a, const void* b) {
    double diff = *(const double*)a - *(const double*)b;
    return (diff > 0) - (diff < 0);
}

static Stats calculate_stats(double* times, size_t count) {
    Stats s = {0};

    // Sort for median
    double* sorted = malloc(count * sizeof(double));
    memcpy(sorted, times, count * sizeof(double));
    qsort(sorted, count, sizeof(double), compare_double);

    s.min    = sorted[0];
    s.max    = sorted[count - 1];
    s.median = sorted[count / 2];

    // Calculate mean
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += times[i];
    }
    s.mean = sum / (double)count;

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = times[i] - s.mean;
        variance += diff * diff;
    }
    s.stddev = sqrt(variance / (double)count);

    free(sorted);
    return s;
}

static void print_stats(const char* name, Stats s, size_t iterations) {
    printf("%-40s\n", name);
    printf("  Mean:   %.2f ns/op  (%.2f ops/sec)\n", s.mean / iterations, iterations / (s.mean / 1e9));
    printf("  Median: %.2f ns/op\n", s.median / iterations);
    printf("  Min:    %.2f ns/op\n", s.min / iterations);
    printf("  Max:    %.2f ns/op\n", s.max / iterations);
    printf("  StdDev: %.2f ns/op\n", s.stddev / iterations);
}

// ============================================================================
// Benchmark 1: Simple sequential allocations (most common case)
// ============================================================================
static void bench_sequential_simple(size_t iterations, size_t warmup, size_t trials) {
    printf("\n=== Benchmark 1: Sequential 32-byte allocations ===\n");

    double* arena_times  = malloc(trials * sizeof(double));
    double* malloc_times = malloc(trials * sizeof(double));

    for (size_t trial = 0; trial < trials + warmup; trial++) {
        bool is_warmup = trial < warmup;
        Timer t;

        // Arena version (reserve 128MB of address space)
        Arena* arena = arena_create(128 * 1024 * 1024);
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            void* ptr = arena_alloc(arena, 32);
            BENCHMARK_USE(ptr);
        }
        double arena_time = timer_end(&t);
        arena_destroy(arena);

        if (!is_warmup) {
            arena_times[trial - warmup] = arena_time;
        }

        // malloc version
        void** ptrs = malloc(iterations * sizeof(void*));
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            ptrs[i] = malloc(32);
            BENCHMARK_USE(ptrs[i]);
        }
        double malloc_time = timer_end(&t);
        for (size_t i = 0; i < iterations; i++) {
            free(ptrs[i]);
        }
        free(ptrs);

        if (!is_warmup) {
            malloc_times[trial - warmup] = malloc_time;
        }
    }

    Stats arena_stats  = calculate_stats(arena_times, trials);
    Stats malloc_stats = calculate_stats(malloc_times, trials);

    print_stats("Arena (sequential 32B)", arena_stats, iterations);
    print_stats("malloc (sequential 32B)", malloc_stats, iterations);
    printf("  Speedup: %.2fx\n", malloc_stats.mean / arena_stats.mean);

    free(arena_times);
    free(malloc_times);
}

// ============================================================================
// Benchmark 2: Mixed sizes (realistic workload)
// ============================================================================
static void bench_mixed_sizes(size_t iterations, size_t warmup, size_t trials) {
    printf("\n=== Benchmark 2: Mixed-size allocations (16-256 bytes) ===\n");

    // Realistic size distribution
    size_t sizes[]   = {16, 24, 32, 48, 64, 96, 128, 192, 256};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    double* arena_times  = malloc(trials * sizeof(double));
    double* malloc_times = malloc(trials * sizeof(double));

    for (size_t trial = 0; trial < trials + warmup; trial++) {
        bool is_warmup = trial < warmup;
        Timer t;

        // Arena version (reserve 128MB)
        Arena* arena = arena_create(128 * 1024 * 1024);
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            size_t size = sizes[i % num_sizes];
            void* ptr   = arena_alloc(arena, size);
            BENCHMARK_USE(ptr);
        }
        double arena_time = timer_end(&t);
        arena_destroy(arena);

        if (!is_warmup) {
            arena_times[trial - warmup] = arena_time;
        }

        // malloc version
        void** ptrs = malloc(iterations * sizeof(void*));
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            size_t size = sizes[i % num_sizes];
            ptrs[i]     = malloc(size);
            BENCHMARK_USE(ptrs[i]);
        }
        double malloc_time = timer_end(&t);
        for (size_t i = 0; i < iterations; i++) {
            free(ptrs[i]);
        }
        free(ptrs);

        if (!is_warmup) {
            malloc_times[trial - warmup] = malloc_time;
        }
    }

    Stats arena_stats  = calculate_stats(arena_times, trials);
    Stats malloc_stats = calculate_stats(malloc_times, trials);

    print_stats("Arena (mixed sizes)", arena_stats, iterations);
    print_stats("malloc (mixed sizes)", malloc_stats, iterations);
    printf("  Speedup: %.2fx\n", malloc_stats.mean / arena_stats.mean);

    free(arena_times);
    free(malloc_times);
}

// ============================================================================
// Benchmark 3: Tiny allocations (worst case for malloc)
// ============================================================================
static void bench_tiny_allocs(size_t iterations, size_t warmup, size_t trials) {
    printf("\n=== Benchmark 3: Tiny allocations (8 bytes) ===\n");

    double* arena_times  = malloc(trials * sizeof(double));
    double* malloc_times = malloc(trials * sizeof(double));

    for (size_t trial = 0; trial < trials + warmup; trial++) {
        bool is_warmup = trial < warmup;
        Timer t;

        // Arena version (reserve 128MB)
        Arena* arena = arena_create(128 * 1024 * 1024);
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            void* ptr = arena_alloc(arena, 8);
            BENCHMARK_USE(ptr);
        }
        double arena_time = timer_end(&t);
        arena_destroy(arena);

        if (!is_warmup) {
            arena_times[trial - warmup] = arena_time;
        }

        // malloc version
        void** ptrs = malloc(iterations * sizeof(void*));
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            ptrs[i] = malloc(8);
            BENCHMARK_USE(ptrs[i]);
        }
        double malloc_time = timer_end(&t);
        for (size_t i = 0; i < iterations; i++) {
            free(ptrs[i]);
        }
        free(ptrs);

        if (!is_warmup) {
            malloc_times[trial - warmup] = malloc_time;
        }
    }

    Stats arena_stats  = calculate_stats(arena_times, trials);
    Stats malloc_stats = calculate_stats(malloc_times, trials);

    print_stats("Arena (tiny 8B)", arena_stats, iterations);
    print_stats("malloc (tiny 8B)", malloc_stats, iterations);
    printf("  Speedup: %.2fx\n", malloc_stats.mean / arena_stats.mean);

    free(arena_times);
    free(malloc_times);
}

// ============================================================================
// Benchmark 4: Aligned allocations
// ============================================================================
static void bench_aligned_allocs(size_t iterations, size_t warmup, size_t trials) {
    printf("\n=== Benchmark 4: Aligned allocations (64-byte alignment) ===\n");

    double* arena_times  = malloc(trials * sizeof(double));
    double* malloc_times = malloc(trials * sizeof(double));

    for (size_t trial = 0; trial < trials + warmup; trial++) {
        bool is_warmup = trial < warmup;
        Timer t;

        // Arena version (reserve 128MB)
        Arena* arena = arena_create(128 * 1024 * 1024);
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            void* ptr = arena_alloc_align(arena, 128, 64);
            BENCHMARK_USE(ptr);
        }
        double arena_time = timer_end(&t);
        arena_destroy(arena);

        if (!is_warmup) {
            arena_times[trial - warmup] = arena_time;
        }

        // aligned_alloc version (POSIX)
        void** ptrs = malloc(iterations * sizeof(void*));
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            ptrs[i] = aligned_alloc(64, 128);
            BENCHMARK_USE(ptrs[i]);
        }
        double malloc_time = timer_end(&t);
        for (size_t i = 0; i < iterations; i++) {
            free(ptrs[i]);
        }
        free(ptrs);

        if (!is_warmup) {
            malloc_times[trial - warmup] = malloc_time;
        }
    }

    Stats arena_stats  = calculate_stats(arena_times, trials);
    Stats malloc_stats = calculate_stats(malloc_times, trials);

    print_stats("Arena (64-byte aligned)", arena_stats, iterations);
    print_stats("aligned_alloc (64-byte)", malloc_stats, iterations);
    printf("  Speedup: %.2fx\n", malloc_stats.mean / arena_stats.mean);

    free(arena_times);
    free(malloc_times);
}

// ============================================================================
// Benchmark 5: Reset and reuse (arena's killer feature)
// ============================================================================
static void bench_reset_reuse(size_t iterations, size_t warmup, size_t trials) {
    printf("\n=== Benchmark 5: Reset and reuse pattern ===\n");

    const size_t allocs_per_cycle = 10000;
    const size_t cycles           = iterations / allocs_per_cycle;

    double* arena_times  = malloc(trials * sizeof(double));
    double* malloc_times = malloc(trials * sizeof(double));

    for (size_t trial = 0; trial < trials + warmup; trial++) {
        bool is_warmup = trial < warmup;
        Timer t;

        // Arena version with reset (reserve 128MB)
        Arena* arena = arena_create(128 * 1024 * 1024);
        timer_start(&t);
        for (size_t cycle = 0; cycle < cycles; cycle++) {
            for (size_t i = 0; i < allocs_per_cycle; i++) {
                void* ptr = arena_alloc(arena, 32);
                BENCHMARK_USE(ptr);
            }
            arena_reset(arena);  // O(1) reset
        }
        double arena_time = timer_end(&t);
        arena_destroy(arena);

        if (!is_warmup) {
            arena_times[trial - warmup] = arena_time;
        }

        // malloc version with free
        timer_start(&t);
        for (size_t cycle = 0; cycle < cycles; cycle++) {
            void** ptrs = malloc(allocs_per_cycle * sizeof(void*));
            for (size_t i = 0; i < allocs_per_cycle; i++) {
                ptrs[i] = malloc(32);
                BENCHMARK_USE(ptrs[i]);
            }
            for (size_t i = 0; i < allocs_per_cycle; i++) {
                free(ptrs[i]);  // O(n) cleanup
            }
            free(ptrs);
        }
        double malloc_time = timer_end(&t);

        if (!is_warmup) {
            malloc_times[trial - warmup] = malloc_time;
        }
    }

    Stats arena_stats  = calculate_stats(arena_times, trials);
    Stats malloc_stats = calculate_stats(malloc_times, trials);

    print_stats("Arena (reset/reuse)", arena_stats, iterations);
    print_stats("malloc (alloc/free)", malloc_stats, iterations);
    printf("  Speedup: %.2fx\n", malloc_stats.mean / arena_stats.mean);

    free(arena_times);
    free(malloc_times);
}

// ============================================================================
// Benchmark 6: Large allocations (edge case)
// ============================================================================
static void bench_large_allocs(size_t iterations, size_t warmup, size_t trials) {
    printf("\n=== Benchmark 6: Large allocations (16KB each) ===\n");

    double* arena_times  = malloc(trials * sizeof(double));
    double* malloc_times = malloc(trials * sizeof(double));

    for (size_t trial = 0; trial < trials + warmup; trial++) {
        bool is_warmup = trial < warmup;
        Timer t;

        // Arena version (reserve 512MB for large allocs)
        Arena* arena = arena_create(512 * 1024 * 1024);
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            void* ptr = arena_alloc(arena, 16384);
            BENCHMARK_USE(ptr);
        }
        double arena_time = timer_end(&t);
        arena_destroy(arena);

        if (!is_warmup) {
            arena_times[trial - warmup] = arena_time;
        }

        // malloc version
        void** ptrs = malloc(iterations * sizeof(void*));
        timer_start(&t);
        for (size_t i = 0; i < iterations; i++) {
            ptrs[i] = malloc(16384);
            BENCHMARK_USE(ptrs[i]);
        }
        double malloc_time = timer_end(&t);
        for (size_t i = 0; i < iterations; i++) {
            free(ptrs[i]);
        }
        free(ptrs);

        if (!is_warmup) {
            malloc_times[trial - warmup] = malloc_time;
        }
    }

    Stats arena_stats  = calculate_stats(arena_times, trials);
    Stats malloc_stats = calculate_stats(malloc_times, trials);

    print_stats("Arena (large 16KB)", arena_stats, iterations);
    print_stats("malloc (large 16KB)", malloc_stats, iterations);
    printf("  Speedup: %.2fx\n", malloc_stats.mean / arena_stats.mean);

    free(arena_times);
    free(malloc_times);
}

// ============================================================================
// Main benchmark runner
// ============================================================================
int main(void) {
    printf("========================================\n");
    printf("Arena Allocator Benchmark Suite\n");
    printf("========================================\n");

    // System info
    printf("\nSystem Information:\n");
    printf("  sizeof(Arena): %zu bytes\n", sizeof(Arena));
    printf("  ARENA_DEFAULT_ALIGN: %d bytes\n", ARENA_DEFAULT_ALIGN);
    printf("  ARENA_COMMIT_CHUNK_SIZE: %d KB\n", ARENA_COMMIT_CHUNK_SIZE / 1024);

    // Benchmark configuration
    const size_t warmup = 3;
    const size_t trials = 10;

    printf("\nBenchmark Configuration:\n");
    printf("  Warmup runs: %zu\n", warmup);
    printf("  Measurement trials: %zu\n", trials);
    printf("  Statistics: mean, median, min, max, stddev\n");

    // Run benchmarks
    bench_sequential_simple(1000000, warmup, trials);
    bench_mixed_sizes(1000000, warmup, trials);
    bench_tiny_allocs(1000000, warmup, trials);
    bench_aligned_allocs(100000, warmup, trials);
    bench_reset_reuse(1000000, warmup, trials);
    bench_large_allocs(10000, warmup, trials);

    return 0;
}
