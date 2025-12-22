#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/arena.h"

// --- Configuration ---
#define WARMUP_ITERATIONS         100
#define MEASUREMENT_ITERATIONS    2000
#define ALLOCATIONS_PER_ITERATION 20000
#define MIN_ALLOC_SIZE            16
#define MAX_ALLOC_SIZE            4096

// --- Statistics & timing ---

typedef struct {
    double min_ns;
    double max_ns;
    double avg_ns;
    double median_ns;
    double stddev_ns;
    double throughput;  // Allocations per second
} BenchmarkStats;

// Cross-platform memory touch using inline assembly
static inline void touch_memory(volatile void* ptr) {
#if defined(__GNUC__) || defined(__clang__)
// GCC/Clang inline assembly (works on Linux, macOS, *BSD)
#if defined(__x86_64__) || defined(__i386__)
    // x86/x64: Use a dummy read to force memory access
    __asm__ volatile("movb (%0), %%al" : : "r"(ptr) : "al", "memory");
#elif defined(__aarch64__) || defined(__arm__)
    // ARM64/ARM: Load byte to force memory access
    __asm__ volatile("ldrb wzr, [%0]" : : "r"(ptr) : "memory");
#else
    // Fallback for other architectures
    __asm__ volatile("" : : "r"(*(const volatile char*)ptr) : "memory");
#endif
#elif defined(_MSC_VER)
// MSVC inline assembly
#if defined(_M_X64) || defined(_M_IX86)
    // x86/x64: Use intrinsic instead (MSVC doesn't support inline asm on x64)
    _ReadWriteBarrier();
    (void)(*(volatile char*)ptr);
    _ReadWriteBarrier();
#elif defined(_M_ARM64) || defined(_M_ARM)
    // ARM: Use intrinsic
    __dmb(_ARM64_BARRIER_SY);
    (void)(*(volatile char*)ptr);
    __dmb(_ARM64_BARRIER_SY);
#else
    // Fallback
    _ReadWriteBarrier();
    (void)(*(volatile char*)ptr);
#endif
#else
    // Portable fallback using volatile
    (void)(*(volatile char*)ptr);
#endif
}

static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int compare_double(const void* a, const void* b) {
    double diff = *(const double*)a - *(const double*)b;
    return (diff > 0) - (diff < 0);
}

static void calculate_stats(double* samples, size_t count, BenchmarkStats* stats) {
    if (count == 0) return;

    qsort(samples, count, sizeof(double), compare_double);

    stats->min_ns    = samples[0];
    stats->max_ns    = samples[count - 1];
    stats->median_ns = samples[count / 2];

    double sum = 0.0;
    for (size_t i = 0; i < count; i++)
        sum += samples[i];
    stats->avg_ns = sum / (double)count;

    double variance_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = samples[i] - stats->avg_ns;
        variance_sum += diff * diff;
    }
    stats->stddev_ns  = sqrt(variance_sum / (double)count);
    stats->throughput = (ALLOCATIONS_PER_ITERATION * 1e9) / stats->avg_ns;
}

// --- Benchmark Logic ---

// Pre-calculated sizes to ensure both benchmarks run the exact same workload
static size_t sizes[ALLOCATIONS_PER_ITERATION];
static size_t total_required_size = 0;

static void init_sizes() {
    srand(42);  // Deterministic seed
    total_required_size = 0;
    for (int i = 0; i < ALLOCATIONS_PER_ITERATION; i++) {
        // Random size between MIN and MAX
        sizes[i] = MIN_ALLOC_SIZE + (size_t)(rand() % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE + 1));

        // Account for alignment in total size calculation (assuming 8-byte align for safety)
        size_t aligned = (sizes[i] + 7) & ~7U;
        total_required_size += aligned;
    }
    // Add buffer for block headers and padding
    total_required_size += (sizeof(size_t) * ALLOCATIONS_PER_ITERATION);
}

static void benchmark_arena(double* samples, size_t num_samples, bool warmup) {
    Arena* arena = arena_create(0);

    for (size_t iter = 0; iter < num_samples; iter++) {
        uint64_t start = get_time_ns();

        // 1. Reset is part of the lifecycle
        arena_reset(arena);

        // 2. Allocation Loop
        for (size_t i = 0; i < ALLOCATIONS_PER_ITERATION; i++) {
            void* ptr = arena_alloc(arena, sizes[i]);
            touch_memory(ptr);
        }

        uint64_t end = get_time_ns();
        if (!warmup) samples[iter] = (double)(end - start);
    }

    arena_destroy(arena);
}

static void benchmark_malloc(double* samples, size_t num_samples, bool warmup) {
    // Array to hold pointers for freeing
    void* ptrs[ALLOCATIONS_PER_ITERATION];

    for (size_t iter = 0; iter < num_samples; iter++) {
        uint64_t start = get_time_ns();

        // 1. Allocation Loop
        for (size_t i = 0; i < ALLOCATIONS_PER_ITERATION; i++) {
            volatile char* ptr = (volatile char*)malloc(sizes[i]);
            ptrs[i]            = (void*)ptr;

            // Critical: Touch memory exactly the same way arena did
            *ptr = 1;
        }

        // 2. Free Loop (This is the cost equivalent to arena_reset)
        for (size_t i = 0; i < ALLOCATIONS_PER_ITERATION; i++) {
            free(ptrs[i]);
        }

        uint64_t end = get_time_ns();

        if (!warmup) samples[iter] = (double)(end - start);
    }
}

// --- Output formatting ---

static void print_header(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                          Arena Allocator Benchmark Results                                   ║\n");
    printf("╠══════════════╦════════════╦════════════╦════════════╦════════════╦════════════╦════════════════╣\n");
    printf("║  Allocator   ║  Min (μs)  ║  Avg (μs)  ║  Med (μs)  ║  Max (μs)  ║ StdDev (μs)║  Throughput    ║\n");
    printf("╠══════════════╬════════════╬════════════╬════════════╬════════════╬════════════╬════════════════╣\n");
}

static void print_row(const char* allocator, const BenchmarkStats* stats) {
    printf("│ %-12s │ %10.2f │ %10.2f │ %10.2f │ %10.2f │ %10.2f │ %14.0f │\n", allocator, stats->min_ns / 1000.0,
           stats->avg_ns / 1000.0, stats->median_ns / 1000.0, stats->max_ns / 1000.0, stats->stddev_ns / 1000.0,
           stats->throughput);
}

int main(void) {
    init_sizes();

    printf("Starting benchmark...\n");
    printf("  • Total Allocations: %d\n", ALLOCATIONS_PER_ITERATION);
    printf("  • Total Volume:      %.2f MB\n", total_required_size / (1024.0 * 1024.0));
    printf("Performing warmup runs...\n");

    benchmark_arena(NULL, WARMUP_ITERATIONS, true);
    benchmark_malloc(NULL, WARMUP_ITERATIONS, true);

    double* arena_samples  = malloc(sizeof(double) * MEASUREMENT_ITERATIONS);
    double* malloc_samples = malloc(sizeof(double) * MEASUREMENT_ITERATIONS);

    printf("Running measurements...\n");

    benchmark_arena(arena_samples, MEASUREMENT_ITERATIONS, false);
    benchmark_malloc(malloc_samples, MEASUREMENT_ITERATIONS, false);

    BenchmarkStats arena_stats, malloc_stats;
    calculate_stats(arena_samples, MEASUREMENT_ITERATIONS, &arena_stats);
    calculate_stats(malloc_samples, MEASUREMENT_ITERATIONS, &malloc_stats);

    print_header();
    print_row("Arena", &arena_stats);
    printf("╟──────────────┼────────────┼────────────┼────────────┼────────────┼────────────┼────────────────╢\n");
    print_row("malloc/free", &malloc_stats);

    printf("╚══════════════╩════════════╩════════════╩════════════╩════════════╩════════════╩════════════════╝\n");

    double speedup = malloc_stats.avg_ns / arena_stats.avg_ns;
    printf("\nSummary: Arena is %.2fx faster than malloc/free for this workload.\n", speedup);

    free(arena_samples);
    free(malloc_samples);

    return 0;
}
