#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/macros.h"
#include "../include/threadpool.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_TASKS 1000000
#define NUM_RUNS  1

/*
 * Batch size for submit_batch calls.  Tunable independently of the
 * threadpool's internal BATCH_SIZE (which controls how many tasks workers
 * pull from the global queue at once).  Larger values here reduce mutex
 * acquisitions on the submission side; smaller values reduce memory pressure.
 * 1024 is a good default: 1M tasks / 1024 = ~977 mutex acquisitions total.
 */
#define SUBMIT_BATCH_SIZE 1024

/** CPU work simulation. */
static inline uint64_t fast_hash(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccd;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53;
    x ^= x >> 33;
    return x;
}

void dummy_task(void* arg) {
    (void)arg;
    uint64_t result = 0;
    for (uint64_t i = 0; i < 1000; i++) {
        result = fast_hash(result + i);
    }

    // Cross-platform optimizer defeat
#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("" : : "r"(result) : "memory");
#else
    // MSVC: Force evaluation by assigning to a local volatile.
    // The overhead is a single stack write after the loop, which is negligible.
    volatile uint64_t compiler_sink = result;
    (void)compiler_sink;  // Silence "unused variable" warning
#endif
}

typedef struct {
    double throughput;
    double latency;
    double elapsed_time;
} benchmark_result;

benchmark_result run_benchmark(size_t num_threads) {
    Threadpool* pool = threadpool_create(num_threads);
    if (pool == NULL) {
        fprintf(stderr, "Failed to create thread pool with %zu threads\n", num_threads);
        exit(1);
    }

    /*
     * Pre-build the function pointer array once.  Every task calls the same
     * function with a NULL arg, so we reuse a single stack-allocated array of
     * function pointers.  For heterogeneous workloads, build a matching args[]
     * array and pass it as the third argument.
     */
    void (**fns)(void*) = (void (**)(void*))malloc(SUBMIT_BATCH_SIZE * sizeof(void (*)(void*)));
    if (!fns) {
        threadpool_destroy(pool, -1);
        exit(1);
    }
    for (int i = 0; i < SUBMIT_BATCH_SIZE; i++) fns[i] = dummy_task;

    uint64_t start = get_time_ns();

    int remaining = NUM_TASKS;
    while (remaining > 0) {
        int batch = remaining < SUBMIT_BATCH_SIZE ? remaining : SUBMIT_BATCH_SIZE;
        size_t submitted = threadpool_submit_batch(pool, fns, NULL, (size_t)batch);
        if ((int)submitted != batch) {
            fprintf(stderr, "Failed to submit batch: wanted %d got %zu\n", batch, submitted);
            free(fns);
            threadpool_destroy(pool, -1);
            exit(1);
        }
        remaining -= batch;
    }

    free(fns);
    threadpool_destroy(pool, -1);

    uint64_t end = get_time_ns();

    double elapsed = (end - start) / 1e9;
    double throughput = NUM_TASKS / elapsed;
    double latency = elapsed / NUM_TASKS * 1e6;
    return (benchmark_result){throughput, latency, elapsed};
}

void print_table_header() {
    printf("┌─────────────┬─────────────────┬─────────────────┬─────────────────┬─────────────────┐\n");
    printf("│   Threads   │   Throughput    │   Avg Latency   │   Elapsed Time  │   Efficiency    │\n");
    printf("│             │   (tasks/sec)   │     (µs/task)   │      (sec)      │       (%%)       │\n");
    printf("├─────────────┼─────────────────┼─────────────────┼─────────────────┼─────────────────┤\n");
}

void print_table_row(size_t threads, benchmark_result* results, double baseline) {
    double tp = 0, lat = 0, el = 0;
    for (size_t i = 0; i < NUM_RUNS; i++) {
        tp += results[i].throughput;
        lat += results[i].latency;
        el += results[i].elapsed_time;
    }
    tp /= NUM_RUNS;
    lat /= NUM_RUNS;
    el /= NUM_RUNS;
    double eff = (tp / baseline) / (double)threads * 100.0;
    printf("│     %2zu      │  %15.2f     │     %10.4f    │     %10.4f    │     %10.2f    │\n", threads, tp, lat, el,
           eff);
}

void print_table_separator() {
    printf("├─────────────┼─────────────────┼─────────────────┼─────────────────┼─────────────────┤\n");
}

void print_table_footer() {
    printf("└─────────────┴─────────────────┴─────────────────┴─────────────────┴─────────────────┘\n");
}

void print_run_details(int run, benchmark_result r) {
    printf("  Run %d: %.2f tasks/sec, %.4f µs/task, %.4f sec\n", run + 1, r.throughput, r.latency, r.elapsed_time);
}

int main() {
    size_t thread_counts[] = {1, 2, 4, 8};
    int num_configs = (int)(sizeof(thread_counts) / sizeof(thread_counts[0]));

    printf("Threadpool Benchmark Results\n");
    printf("============================\n");
    printf("Tasks: %d, Runs: %d, Submit batch size: %d\n\n", NUM_TASKS, NUM_RUNS, SUBMIT_BATCH_SIZE);

    benchmark_result all_results[4][NUM_RUNS];
    double baseline = 0;

    for (int t = 0; t < num_configs; t++) {
        size_t threads = thread_counts[t];
        printf("Testing with %zu thread%s:\n", threads, threads == 1 ? "" : "s");

        for (int run = 0; run < NUM_RUNS; run++) {
            printf("  Running benchmark %d/%d...", run + 1, NUM_RUNS);
            fflush(stdout);
            all_results[t][run] = run_benchmark(threads);
            printf(" Complete\n");
            print_run_details(run, all_results[t][run]);
        }

        if (threads == 1) {
            for (int i = 0; i < NUM_RUNS; i++) baseline += all_results[t][i].throughput;
            baseline /= NUM_RUNS;
        }
        printf("\n");
    }

    printf("Summary Results:\n");
    print_table_header();
    for (int t = 0; t < num_configs; t++) {
        print_table_row(thread_counts[t], all_results[t], baseline);
        if (t < num_configs - 1) print_table_separator();
    }
    print_table_footer();

    printf("\nScaling Analysis:\n");
    printf("================\n");
    double single = 0;
    for (int i = 0; i < NUM_RUNS; i++) single += all_results[0][i].throughput;
    single /= NUM_RUNS;

    for (int t = 1; t < num_configs; t++) {
        double tp = 0;
        for (int i = 0; i < NUM_RUNS; i++) tp += all_results[t][i].throughput;
        tp /= NUM_RUNS;
        double speedup = tp / single;
        printf("%zu threads: %.2fx speedup (%.2f%% efficiency)\n", thread_counts[t], speedup,
               speedup / (double)thread_counts[t] * 100.0);
    }

    return 0;
}
