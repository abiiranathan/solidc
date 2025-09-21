#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/threadpool.h"

#define NUM_TASKS 10000000
#define NUM_RUNS  1

/** CPU work simulation functions. */
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

    volatile uint64_t result = 0;

    // Simulate light computation
    for (uint64_t i = 0; i < 16; i++) {
        result = fast_hash(result + i);
    }
}

typedef struct {
    double throughput;
    double latency;
    double elapsed_time;
} benchmark_result;

benchmark_result run_benchmark(size_t num_threads) {
    Threadpool* pool = threadpool_create(num_threads);
    if (pool == NULL) {
        fprintf(stderr, "Failed to create thread pool with %ld threads\n", num_threads);
        exit(1);
    }

    // Start timing
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Submit tasks
    for (int i = 0; i < NUM_TASKS; i++) {
        if (threadpool_submit(pool, dummy_task, NULL) != 0) {
            fprintf(stderr, "Failed to add task after submitting: %d tasks\n", i);
            exit(1);
        }
    }

    // Wait for threads and clean up
    threadpool_destroy(pool, -1);

    // Stop timing
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate metrics
    double elapsed =
        (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput  = NUM_TASKS / elapsed;
    double avg_latency = elapsed / NUM_TASKS * 1e6;

    benchmark_result result = {throughput, avg_latency, elapsed};
    return result;
}

void print_table_header() {
    printf(
        "┌─────────────┬─────────────────┬─────────────────┬─────────────────┬─────────────────┐"
        "\n");
    printf(
        "│   Threads   │   Throughput    │   Avg Latency   │   Elapsed Time  │   Efficiency    "
        "│\n");
    printf(
        "│             │   (tasks/sec)   │     (µs/task)   │      (sec)      │       (%%)       "
        "│\n");
    printf(
        "├─────────────┼─────────────────┼─────────────────┼─────────────────┼─────────────────┤"
        "\n");
}

void print_table_row(int threads, benchmark_result* results, double baseline_throughput) {
    // Calculate averages across runs
    double avg_throughput = 0, avg_latency = 0, avg_elapsed = 0;
    for (int i = 0; i < NUM_RUNS; i++) {
        avg_throughput += results[i].throughput;
        avg_latency += results[i].latency;
        avg_elapsed += results[i].elapsed_time;
    }
    avg_throughput /= NUM_RUNS;
    avg_latency /= NUM_RUNS;
    avg_elapsed /= NUM_RUNS;

    double efficiency = (avg_throughput / baseline_throughput) / threads * 100.0;

    printf("│     %2d      │  %15.2f     │     %10.4f    │     %10.4f    │     %10.2f    │\n",
           threads, avg_throughput, avg_latency, avg_elapsed, efficiency);
}

void print_table_separator() {
    printf(
        "├─────────────┼─────────────────┼─────────────────┼─────────────────┼─────────────────┤"
        "\n");
}

void print_table_footer() {
    printf(
        "└─────────────┴─────────────────┴─────────────────┴─────────────────┴─────────────────┘"
        "\n");
}

void print_run_details(int run, benchmark_result result) {
    printf("  Run %d: %.2f tasks/sec, %.4f µs/task, %.4f sec\n", run + 1, result.throughput,
           result.latency, result.elapsed_time);
}

int main() {
    size_t thread_counts[] = {1, 2, 4, 8};
    int num_thread_configs = sizeof(thread_counts) / sizeof(thread_counts[0]);

    printf("Threadpool Benchmark Results\n");
    printf("============================\n");
    printf("Tasks: %d, Runs per configuration: %d\n\n", NUM_TASKS, NUM_RUNS);

    benchmark_result all_results[4][NUM_RUNS];
    double baseline_throughput = 0;

    // Run benchmarks
    for (int t = 0; t < num_thread_configs; t++) {
        size_t threads = thread_counts[t];
        printf("Testing with %ld thread%s:\n", threads, threads == 1 ? "" : "s");

        for (int run = 0; run < NUM_RUNS; run++) {
            printf("  Running benchmark %d/%d...", run + 1, NUM_RUNS);
            fflush(stdout);

            all_results[t][run] = run_benchmark(threads);

            printf(" Complete\n");
            print_run_details(run, all_results[t][run]);
        }

        // Set baseline from single thread average
        if (threads == 1) {
            for (int i = 0; i < NUM_RUNS; i++) {
                baseline_throughput += all_results[t][i].throughput;
            }
            baseline_throughput /= NUM_RUNS;
        }

        printf("\n");
    }

    // Print summary table
    printf("Summary Results:\n");
    print_table_header();

    for (int t = 0; t < num_thread_configs; t++) {
        print_table_row(thread_counts[t], all_results[t], baseline_throughput);
        if (t < num_thread_configs - 1) {
            print_table_separator();
        }
    }

    print_table_footer();

    // Print scaling analysis
    printf("\nScaling Analysis:\n");
    printf("================\n");

    double single_thread_throughput = 0;
    for (int i = 0; i < NUM_RUNS; i++) {
        single_thread_throughput += all_results[0][i].throughput;
    }
    single_thread_throughput /= NUM_RUNS;

    for (int t = 1; t < num_thread_configs; t++) {
        double avg_throughput = 0;
        for (int i = 0; i < NUM_RUNS; i++) {
            avg_throughput += all_results[t][i].throughput;
        }
        avg_throughput /= NUM_RUNS;

        double speedup    = avg_throughput / single_thread_throughput;
        double efficiency = speedup / thread_counts[t] * 100.0;

        printf("%ld threads: %.2fx speedup (%.2f%% efficiency)\n", thread_counts[t], speedup,
               efficiency);
    }

    return 0;
}
