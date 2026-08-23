#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * threadpool_micro.c — overhead-isolating benchmarks for the threadpool.
 *
 * The full-size benchmark (threadpool_bench.c) measures end-to-end
 * throughput with compute-heavy tasks, which hides the pool itself.  This
 * harness instead isolates individual synchronization paths:
 *
 *   pingpong    — submit one no-op task and wait() for it, in a tight loop.
 *                 Measures the full wake-up chain: unpark -> futex wake ->
 *                 worker dispatch -> execute -> completion flush -> idle
 *                 broadcast -> waiter wake.  Reported as ns/round-trip.
 *
 *   burst       — submit a large batch of no-op tasks once, then measure
 *                 ONLY the time threadpool_wait() takes after the final
 *                 submit returns.  Measures drain + idle-detection tail.
 *
 *   mproducers  — K external threads each stream tiny tasks concurrently,
 *                 stressing global-queue mutex traffic and pending counter
 *                 contention from the submission side.
 *
 *   granular    — fixed total work, swept task duration (~25ns..~3us), so
 *                 crossover points between submission-bound / sync-bound /
 *                 compute-bound regimes are visible.
 *
 * Each scenario prints median-of-runs nanoseconds per logical operation;
 * lower is better everywhere (no "bigger is better" mixing).
 */

#include "../include/macros.h"
#include "../include/threadpool.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int cmp_u64(const void* a, const void* b) {
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

static uint64_t median(uint64_t* v, size_t n) {
    qsort(v, n, sizeof(uint64_t), cmp_u64);
    return v[n / 2];
}

/* ── tasks ──────────────────────────────────────────────────────────────── */

static void noop_task(void* arg) {
    (void)arg;
#if defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#endif
}

/* ~25ns of dependent arithmetic per "unit" on a modern x86 core */
typedef struct {
    unsigned units;
} sized_arg_t;

static void sized_task(void* arg) {
    sized_arg_t* s = arg;
    uint64_t x = 0x9E3779B97F4A7C15ull;
    for (unsigned i = 0; i < s->units * 8; i++) {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 29;
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(x) : "memory");
#endif
}

/* ── scenario: pingpong ─────────────────────────────────────────────────── */

static double bench_pingpong(size_t nthreads, int iters, int reps) {
    uint64_t* samples = malloc(sizeof(uint64_t) * (size_t)reps);
    Threadpool* pool = threadpool_create(nthreads);

    for (int r = 0; r < reps; r++) {
        uint64_t t0 = ns_now();
        for (int i = 0; i < iters; i++) {
            threadpool_submit(pool, noop_task, NULL);
            threadpool_wait(pool);
        }
        samples[r] = (ns_now() - t0) / (uint64_t)iters;
    }

    threadpool_destroy(pool, -1);
    double med = (double)median(samples, (size_t)reps);
    free(samples);
    return med; /* ns per submit+complete round trip */
}

/* ── scenario: burst drain tail ─────────────────────────────────────────── */

static double bench_burst(size_t nthreads, size_t count, int reps) {
    uint64_t* samples = malloc(sizeof(uint64_t) * (size_t)reps);
    void (**fns)(void*) = malloc(sizeof(void (*)()) * count);

    for (size_t i = 0; i < count; i++) fns[i] = noop_task;

    Threadpool* pool = threadpool_create(nthreads);
    for (int r = 0; r < reps; r++) {
        threadpool_submit_batch(pool, fns, NULL, count);
        uint64_t t0 = ns_now();
        threadpool_wait(pool);
        samples[r] = ns_now() - t0;
    }
    threadpool_destroy(pool, -1);

    free(fns);
    free(samples);
    return (double)median(samples, (size_t)reps); /* ns for the drain tail */
}

/* ── scenario: concurrent producers ─────────────────────────────────────── */

typedef struct {
    Threadpool* pool;
    size_t count;      /* tasks per producer */
    pthread_barrier_t* bar;
    double result_ns;
} producer_ctx_t;

static void* producer_thread(void* p) {
    producer_ctx_t* c = p;
    void (**fns)(void*) = malloc(sizeof(void (*)()) * 256);
    for (size_t i = 0; i < 256; i++) fns[i] = noop_task;

    pthread_barrier_wait(c->bar);
    uint64_t t0 = ns_now();
    for (size_t rem = c->count; rem > 0;) {
        size_t batch = rem < 256 ? rem : 256;
        if (threadpool_submit_batch(c->pool, fns, NULL, batch) != batch) break;
        rem -= batch;
    }
    c->result_ns = (double)(ns_now() - t0);
    free(fns);
    return NULL;
}

static double bench_mproducers(size_t nthreads, size_t nproducers, size_t per_producer, int reps) {
    uint64_t* samples = malloc(sizeof(uint64_t) * (size_t)reps);
    Threadpool* pool = threadpool_create(nthreads);
    pthread_barrier_t bar;
    pthread_barrier_init(&bar, NULL, (unsigned)nproducers);
    producer_ctx_t* ctxs = calloc(nproducers, sizeof(producer_ctx_t));
    pthread_t* ths = calloc(nproducers, sizeof(pthread_t));

    for (int r = 0; r < reps; r++) {
        for (size_t k = 0; k < nproducers; k++) {
            ctxs[k].pool = pool;
            ctxs[k].count = per_producer;
            ctxs[k].bar = &bar;
            pthread_create(&ths[k], NULL, producer_thread, &ctxs[k]);
        }
        for (size_t k = 0; k < nproducers; k++) pthread_join(ths[k], NULL);
        threadpool_wait(pool);
        /* wall time of slowest producer */
        double worst = 0;
        for (size_t k = 0; k < nproducers; k++)
            if (ctxs[k].result_ns > worst) worst = ctxs[k].result_ns;
        samples[r] = (uint64_t)worst;
    }

    threadpool_destroy(pool, -1);
    pthread_barrier_destroy(&bar);
    double med_wall = (double)median(samples, (size_t)reps);
    free(ctxs);
    free(ths);
    free(samples);
    return med_wall / (double)(nproducers * per_producer); /* ns/task effective */
}

/* ── scenario: granularity sweep ────────────────────────────────────────── */

#define GRAN_TOTAL_UNITS 40000000u

static double bench_granular(size_t nthreads, unsigned units_per_task, size_t* count_out, int reps) {
    size_t count = GRAN_TOTAL_UNITS / units_per_task;
    if (count > 2000000) count = 2000000;
    *count_out = count;

    sized_arg_t arg = {.units = units_per_task};
    void** args = malloc(sizeof(void*) * count);
    void (**fns)(void*) = malloc(sizeof(void (*)()) * count);
    for (size_t i = 0; i < count; i++) {
        fns[i] = sized_task;
        args[i] = &arg;
    }

    uint64_t best = UINT64_MAX;
    Threadpool* pool = threadpool_create(nthreads);
    for (int r = 0; r < reps; r++) {
        uint64_t t0 = ns_now();
        threadpool_submit_batch(pool, fns, args, count);
        threadpool_wait(pool);
        uint64_t d = ns_now() - t0;
        if (d < best) best = d;
    }
    threadpool_destroy(pool, -1);
    free(args);
    free(fns);
    return (double)best; /* wall ns for the whole batch */
}

/* ── output ─────────────────────────────────────────────────────────────── */

int main(void) {
    printf("Threadpool Micro-Benchmarks (median of runs, lower is better)\n");
    printf("cpus=%d\n\n", (int)sysconf(_SC_NPROCESSORS_ONLN));

    puts("== pingpong: submit 1 task + wait, ns/round-trip ==");
    for (size_t t = 1; t <= 8; t <<= 1)
        printf("  workers=%zu : %8.0f ns\n", t, bench_pingpong(t, 2000, 5));

    puts("\n== burst: wait()-tail after 65536 no-op tasks, ns ==");
    for (size_t t = 1; t <= 8; t <<= 1)
        printf("  workers=%zu : %10.0f ns\n", t, bench_burst(t, 65536, 20));

    puts("\n== mproducers: 4 producers x 262144 no-op tasks, ns/task ==");
    for (size_t w = 2; w <= 8; w <<= 1)
        printf("  workers=%zu : %8.1f ns\n", w, bench_mproducers(w, 4, 262144, 7));

    puts("\n== granular: fixed 40M units of work, wall ms by task size ==");
    printf("  %-14s %-10s %10s %10s %10s %12s\n", "task(units)", "~per-task", "1T(ms)", "2T(ms)", "8T(ms)", "speedup");
    unsigned sizes[] = {1, 16, 128, 1024};
    for (unsigned s = 0; s < 4; s++) {
        size_t c1, c8;
        double t1 = bench_granular(1, sizes[s], &c1, 5);
        double t2 = bench_granular(2, sizes[s], &(size_t){0}, 5);
        double t8 = bench_granular(8, sizes[s], &c8, 5);
        printf("  %-14u %10.1fns %10.2f %10.2f %10.2f %11.2fx\n", sizes[s],
               (double)t1 / (double)c1, t1 / 1e6, t2 / 1e6, t8 / 1e6, t1 / t8);
    }

    return 0;
}
