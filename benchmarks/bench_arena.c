#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/arena.h"
#include "../include/macros.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * Configuration
 *
 * ALLOCS_PER_ITER controls how many allocations constitute one timed "round".
 * Each scenario uses a different size distribution so the total working-set
 * fits a specific cache tier — see scenario definitions below.
 * ---------------------------------------------------------------------- */
#define WARMUP_ITERATIONS      200
#define MEASUREMENT_ITERATIONS 2000
#define ALLOCS_PER_ITER        10000

/* -------------------------------------------------------------------------
 * Memory touch — identical for every allocator path.
 *
 * A volatile write is used in all cases so the compiler cannot elide it and
 * both the arena and malloc paths trigger the same first-touch page-fault
 * behaviour.  Using a read for one and a write for the other (as the
 * original benchmark did) produces incomparable timings on Linux due to
 * different CoW paths.
 * ---------------------------------------------------------------------- */
static inline void touch_memory(void* ptr) {
    *(volatile char*)ptr = 1;
}

/* -------------------------------------------------------------------------
 * Statistics
 * ---------------------------------------------------------------------- */
typedef struct {
    double min_ns;
    double max_ns;
    double avg_ns;
    double median_ns;
    double p99_ns;
    double stddev_ns;
    double throughput; /* Allocations per second. */
} BenchmarkStats;

static int compare_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

static void calculate_stats(double* samples, size_t count, BenchmarkStats* out) {
    if (count == 0) return;
    qsort(samples, count, sizeof(double), compare_double);

    out->min_ns = samples[0];
    out->max_ns = samples[count - 1];
    out->median_ns = samples[count / 2];
    out->p99_ns = samples[(size_t)(count * 0.99)];

    double sum = 0.0;
    for (size_t i = 0; i < count; i++)
        sum += samples[i];
    out->avg_ns = sum / (double)count;

    double var = 0.0;
    for (size_t i = 0; i < count; i++) {
        double d = samples[i] - out->avg_ns;
        var += d * d;
    }
    out->stddev_ns = sqrt(var / (double)count);
    out->throughput = ((double)ALLOCS_PER_ITER * 1e9) / out->avg_ns;
}

/* -------------------------------------------------------------------------
 * Workload scenarios
 *
 * Each scenario fixes the allocation size range so the total working-set
 * stays within a predictable cache tier.  All scenarios use the same
 * deterministic seed so arena and malloc see an identical sequence.
 *
 *   SMALL  — 16–256 B, mean ~136 B, total ~1.3 MB  → fits in most L3 caches.
 *             Representative of HTTP request/response header parsing,
 *             JSON tokenisation, AST nodes.
 *
 *   MIXED  — 16–512 B, mean ~264 B, total ~2.6 MB  → still L3-resident.
 *             Representative of mixed struct + string workloads.
 *
 *   LARGE  — 16–4096 B, mean ~2 KB, total ~20 MB   → L3-busting.
 *             Representative of document processing, image metadata, logs.
 *             Favours the arena's sequential-write pattern most strongly;
 *             interpret the speedup with that context in mind.
 * ---------------------------------------------------------------------- */
typedef struct {
    const char* name;
    size_t min_size;
    size_t max_size;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"Small  (16–256 B)", 16, 256},
    {"Mixed  (16–512 B)", 16, 512},
    {"Large  (16–4096 B)", 16, 4096},
};
#define NUM_SCENARIOS ((int)(sizeof(SCENARIOS) / sizeof(SCENARIOS[0])))

/* Pre-generated size sequence — same for every allocator in a scenario. */
static size_t g_sizes[ALLOCS_PER_ITER];

static void init_sizes(const Scenario* sc) {
    srand(42);
    size_t range = sc->max_size - sc->min_size + 1;
    for (int i = 0; i < ALLOCS_PER_ITER; i++) {
        g_sizes[i] = sc->min_size + (size_t)((unsigned)rand() % range);
    }
}

static size_t working_set_bytes(const Scenario* sc) {
    /* Approximate: use midpoint with 16-byte arena alignment rounding. */
    size_t mean = (sc->min_size + sc->max_size) / 2;
    size_t aligned = (mean + 15) & ~(size_t)15;
    return aligned * ALLOCS_PER_ITER;
}

/* -------------------------------------------------------------------------
 * Benchmark kernels
 *
 * Three arena variants are measured to give an honest picture:
 *
 *   arena_warm  — one arena reused across iterations via arena_reset().
 *                 Hot TLS buffer, no page faults after the first iteration.
 *                 Best-case arena performance.
 *
 *   arena_cold  — arena_create()/arena_destroy() every iteration.
 *                 Forces OS involvement on each block allocation.
 *                 Worst-case arena performance; comparable to malloc for
 *                 single-use arenas.
 *
 *   malloc      — standard malloc()/free() with pointer array stored on
 *                 the heap (not the stack) to avoid stack-overflow risk at
 *                 large ALLOCS_PER_ITER.
 *
 * The `warmup` flag suppresses sample recording but still runs the full
 * iteration loop, preventing NULL-dereference UB in the original code.
 * ---------------------------------------------------------------------- */

static void bench_arena_warm(double* samples, size_t n, bool warmup) {
    Arena* arena = arena_create(0);

    for (size_t iter = 0; iter < n; iter++) {
        arena_reset(arena);

        uint64_t start = get_time_ns();
        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            void* ptr = arena_alloc(arena, g_sizes[i]);
            touch_memory(ptr);
        }
        uint64_t end = get_time_ns();

        if (!warmup) samples[iter] = (double)(end - start);
    }

    arena_destroy(arena);
}

static void bench_arena_cold(double* samples, size_t n, bool warmup) {
    for (size_t iter = 0; iter < n; iter++) {
        /* Create and destroy inside the timed region so page-fault and
         * OS-allocation costs are fully accounted for. */
        uint64_t start = get_time_ns();

        Arena* arena = arena_create(0);
        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            void* ptr = arena_alloc(arena, g_sizes[i]);
            touch_memory(ptr);
        }
        arena_destroy(arena);

        uint64_t end = get_time_ns();
        if (!warmup) samples[iter] = (double)(end - start);
    }
}

static void bench_malloc(double* samples, size_t n, bool warmup) {
    /* Heap-allocate the pointer array to avoid 80 KB stack pressure at
     * ALLOCS_PER_ITER=10000 with 8-byte pointers. */
    void** ptrs = (void**)malloc(sizeof(void*) * ALLOCS_PER_ITER);
    if (!ptrs) {
        fprintf(stderr, "bench_malloc: failed to allocate pointer array\n");
        exit(1);
    }

    for (size_t iter = 0; iter < n; iter++) {
        uint64_t start = get_time_ns();

        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            ptrs[i] = malloc(g_sizes[i]);
            touch_memory(ptrs[i]);
        }
        /* Free loop is inside the timed region: equivalent cost to
         * arena_reset() or arena_destroy() on the arena side. */
        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            free(ptrs[i]);
        }

        uint64_t end = get_time_ns();
        if (!warmup) samples[iter] = (double)(end - start);
    }

    free(ptrs);
}

/* -------------------------------------------------------------------------
 * Single-allocation latency micro-benchmark
 *
 * Measures the cost of one arena_alloc() call in isolation (warm arena,
 * 64-byte allocation).  Prints the raw median and p99 in nanoseconds —
 * these are the headline numbers for "how fast is the allocator itself".
 * ---------------------------------------------------------------------- */
#define LATENCY_ITERATIONS 500000

static void bench_single_alloc_latency(void) {
    double* samples = (double*)malloc(sizeof(double) * LATENCY_ITERATIONS);
    if (!samples) return;

    Arena* arena = arena_create(0);

    /* Warmup: fill the arena enough to exercise the fast path. */
    for (int i = 0; i < 10000; i++) {
        void* p = arena_alloc(arena, 64);
        touch_memory(p);
    }
    arena_reset(arena);

    for (int i = 0; i < LATENCY_ITERATIONS; i++) {
        /* Reset periodically to keep the arena in the fast path. */
        if (i % 8000 == 0) arena_reset(arena);

        uint64_t t0 = get_time_ns();
        void* p = arena_alloc(arena, 64);
        uint64_t t1 = get_time_ns();

        touch_memory(p);
        samples[i] = (double)(t1 - t0);
    }

    arena_destroy(arena);

    BenchmarkStats s = {0};
    calculate_stats(samples, LATENCY_ITERATIONS, &s);
    free(samples);

    printf("\nSingle-allocation latency (64-byte, warm arena, %d samples):\n", LATENCY_ITERATIONS);
    printf("  Median  : %.1f ns\n", s.median_ns);
    printf("  p99     : %.1f ns\n", s.p99_ns);
    printf("  Avg     : %.1f ns\n", s.avg_ns);
    printf("  StdDev  : %.1f ns\n", s.stddev_ns);
}

/* -------------------------------------------------------------------------
 * Output formatting
 * ---------------------------------------------------------------------- */
static void print_table_header(void) {
    printf("\n");
    printf(
        "╔══════════════════════╦══════════════╦════════════╦════════════╦════════════╦════════════╦═══════════════╗"
        "\n");
    printf(
        "║ Scenario / Allocator ║  Min (μs)    ║  Avg (μs)  ║  Med (μs)  ║  p99 (μs)  ║ StdDev(μs) ║  Throughput   "
        "║\n");
    printf(
        "╠══════════════════════╬══════════════╬════════════╬════════════╬════════════╬════════════╬═══════════════╣"
        "\n");
}

static void print_row(const char* label, const BenchmarkStats* s) {
    printf("║ %-20s ║ %12.2f ║ %10.2f ║ %10.2f ║ %10.2f ║ %10.2f ║ %13.0f ║\n", label, s->min_ns / 1000.0,
           s->avg_ns / 1000.0, s->median_ns / 1000.0, s->p99_ns / 1000.0, s->stddev_ns / 1000.0, s->throughput);
}

static void print_scenario_divider(const char* scenario_name) {
    printf(
        "╠══════════════════════╩══════════════╩════════════╩════════════╩════════════╩════════════╩═══════════════╣"
        "\n");
    printf("║  %-87s║\n", scenario_name);
    printf(
        "╠══════════════════════╦══════════════╦════════════╦════════════╦════════════╦════════════╦═══════════════╣"
        "\n");
}

static void print_row_divider(void) {
    printf(
        "╟──────────────────────┼──────────────┼────────────┼────────────┼────────────┼────────────┼───────────────╢"
        "\n");
}

static void print_table_footer(void) {
    printf(
        "╚══════════════════════╩══════════════╩════════════╩════════════╩════════════╩════════════╩═══════════════╝"
        "\n");
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */
int main(void) {
    printf("Arena Allocator Benchmark\n");
    printf("  Allocations per iteration : %d\n", ALLOCS_PER_ITER);
    printf("  Measurement iterations    : %d (+ %d warmup)\n", MEASUREMENT_ITERATIONS, WARMUP_ITERATIONS);
    printf("\nScenarios\n");
    for (int s = 0; s < NUM_SCENARIOS; s++) {
        size_t ws = working_set_bytes(&SCENARIOS[s]);
        printf("  [%d] %-20s  working set ~%.1f MB\n", s + 1, SCENARIOS[s].name, (double)ws / (1024.0 * 1024.0));
    }

    double* arena_warm_s = (double*)malloc(sizeof(double) * MEASUREMENT_ITERATIONS);
    double* arena_cold_s = (double*)malloc(sizeof(double) * MEASUREMENT_ITERATIONS);
    double* malloc_s = (double*)malloc(sizeof(double) * MEASUREMENT_ITERATIONS);
    if (!arena_warm_s || !arena_cold_s || !malloc_s) {
        fprintf(stderr, "Failed to allocate sample buffers\n");
        return 1;
    }

    print_table_header();

    for (int sc = 0; sc < NUM_SCENARIOS; sc++) {
        const Scenario* scenario = &SCENARIOS[sc];
        init_sizes(scenario);

        /* Warmup — samples pointer unused but loop still executes fully. */
        bench_arena_warm(arena_warm_s, WARMUP_ITERATIONS, true);
        bench_arena_cold(arena_cold_s, WARMUP_ITERATIONS, true);
        bench_malloc(malloc_s, WARMUP_ITERATIONS, true);

        /* Measure. */
        bench_arena_warm(arena_warm_s, MEASUREMENT_ITERATIONS, false);
        bench_arena_cold(arena_cold_s, MEASUREMENT_ITERATIONS, false);
        bench_malloc(malloc_s, MEASUREMENT_ITERATIONS, false);

        BenchmarkStats ws = {0}, cs = {0}, ms = {0};
        calculate_stats(arena_warm_s, MEASUREMENT_ITERATIONS, &ws);
        calculate_stats(arena_cold_s, MEASUREMENT_ITERATIONS, &cs);
        calculate_stats(malloc_s, MEASUREMENT_ITERATIONS, &ms);

        /* Print scenario block. */
        char header[128];
        size_t bytes = working_set_bytes(scenario);
        snprintf(header, sizeof(header), "%s  (~%.1f MB working set)", scenario->name,
                 (double)bytes / (1024.0 * 1024.0));
        print_scenario_divider(header);

        print_row("Arena (warm)", &ws);
        print_row_divider();
        print_row("Arena (cold)", &cs);
        print_row_divider();
        print_row("malloc/free", &ms);

        double speedup_warm = ms.avg_ns / ws.avg_ns;
        double speedup_cold = ms.avg_ns / cs.avg_ns;
        printf(
            "╟──────────────────────────────────────────────────────────────────────────────────────────────────────╢"
            "\n");
        printf("║  Speedup vs malloc:  warm arena = %5.1fx    cold arena = %5.1fx%-43s║\n", speedup_warm, speedup_cold,
               "");
    }

    print_table_footer();

    /* Single-allocation latency — printed separately, not mixed into the
     * throughput table which measures batch workloads. */
    bench_single_alloc_latency();

    free(arena_warm_s);
    free(arena_cold_s);
    free(malloc_s);
    return 0;
}
