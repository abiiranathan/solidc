#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * sort_micro.c — glibc qsort vs sol_qsort vs specialized sorts.
 *
 * Patterns: random, sorted, reverse, all-equal, few-unique (4 values).
 * Reports ns/element; "speedup" is glibc-qsort time divided by ours.
 */

#include "../include/sort.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t rng_state = 0x243F6A8885A308D3ull;
static uint64_t xrng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

#define N 1000000

typedef int (*qcmp)(const void*, const void*);

static int cmp_i32(const void* a, const void* b) {
    int32_t x = *(const int32_t*)a, y = *(const int32_t*)b;
    return (x > y) - (x < y);
}
static int cmp_i64(const void* a, const void* b) {
    int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
    return (x > y) - (x < y);
}
static int cmp_f64(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

enum pat { P_RANDOM, P_SORTED, P_REVERSE, P_EQUAL, P_FEWUNIQ };
static const char* pat_name[] = {"random", "sorted", "reverse", "all-equal", "few-uniq"};

static void fill_i32(int32_t* a, size_t n, enum pat p) {
    for (size_t i = 0; i < n; i++) {
        switch (p) {
            case P_RANDOM: a[i] = (int32_t)(xrng() >> 33); break;
            case P_SORTED: a[i] = (int32_t)i; break;
            case P_REVERSE: a[i] = (int32_t)(n - i); break;
            case P_EQUAL: a[i] = 7; break;
            case P_FEWUNIQ: a[i] = (int32_t)(i % 4); break;
        }
    }
}
static void fill_i64(int64_t* a, size_t n, enum pat p) {
    for (size_t i = 0; i < n; i++) {
        switch (p) {
            case P_RANDOM: a[i] = (int64_t)xrng(); break;
            case P_SORTED: a[i] = (int64_t)i; break;
            case P_REVERSE: a[i] = (int64_t)(n - i); break;
            case P_EQUAL: a[i] = 7; break;
            case P_FEWUNIQ: a[i] = (int64_t)(i % 4); break;
        }
    }
}
static void fill_f64(double* a, size_t n, enum pat p) {
    for (size_t i = 0; i < n; i++) {
        switch (p) {
            case P_RANDOM: a[i] = (double)(int32_t)(xrng() >> 33) / 1024.0; break;
            case P_SORTED: a[i] = (double)i; break;
            case P_REVERSE: a[i] = -(double)i; break;
            case P_EQUAL: a[i] = 3.14; break;
            case P_FEWUNIQ: a[i] = (double)(i % 4); break;
        }
    }
}

typedef void (*sort_fn)(void*, size_t, size_t, qcmp);

/* Time glibc qsort vs sol_qsort over identical copies of one pattern. */
static void report_pair(const char* ty, const char* pat, double t_q, double t_o) {
    printf("  %-9s %-10s qsort %8.2fns vs sol %8.2fns  -> %5.2fx\n", ty, pat, t_q, t_o, t_q / t_o);
}

static void bench_generic_i32(enum pat p) {
    static int32_t orig[N], work[N];
    fill_i32(orig, N, p);

    memcpy(work, orig, sizeof(work));
    uint64_t t0 = ns_now();
    qsort(work, N, 4, cmp_i32);
    double t_q = (double)(ns_now() - t0) / N;

    memcpy(work, orig, sizeof(work));
    t0 = ns_now();
    sol_qsort(work, N, 4, cmp_i32);
    double t_o = (double)(ns_now() - t0) / N;
    report_pair("i32", pat_name[p], t_q, t_o);
}

static void bench_generic_f64(enum pat p) {
    static double orig[N], work[N];
    fill_f64(orig, N, p);

    memcpy(work, orig, sizeof(work));
    uint64_t t0 = ns_now();
    qsort(work, N, 8, cmp_f64);
    double t_q = (double)(ns_now() - t0) / N;

    memcpy(work, orig, sizeof(work));
    t0 = ns_now();
    sol_qsort(work, N, 8, cmp_f64);
    double t_o = (double)(ns_now() - t0) / N;
    report_pair("f64", pat_name[p], t_q, t_o);
}

static void bench_specialized(void) {
    static int32_t a32[N];
    static int64_t a64[N];
    static double af64[N];

    puts("\n== sol_sort_i32 (no comparator, branchless) ==");
    for (enum pat p = P_RANDOM; p <= P_FEWUNIQ; p++) {
        fill_i32(a32, N, p);
        uint64_t t0 = ns_now();
        sol_sort_i32(a32, N);
        double t_ours = (double)(ns_now() - t0) / N;

        fill_i32(a32, N, p);
        t0 = ns_now();
        qsort(a32, N, 4, cmp_i32);
        double t_q = (double)(ns_now() - t0) / N;
        printf("  %-9s qsort %8.2fns vs sol_sort_i32 %8.2fns  -> %5.2fx\n", pat_name[p], t_q, t_ours, t_q / t_ours);
    }

    puts("\n== sol_sort_i64 ==");
    for (enum pat p = P_RANDOM; p <= P_FEWUNIQ; p++) {
        fill_i64(a64, N, p);
        uint64_t t0 = ns_now();
        sol_sort_i64(a64, N);
        double t_ours = (double)(ns_now() - t0) / N;

        fill_i64(a64, N, p);
        t0 = ns_now();
        qsort(a64, N, 8, cmp_i64);
        double t_q = (double)(ns_now() - t0) / N;
        printf("  %-9s qsort %8.2fns vs sol_sort_i64 %8.2fns  -> %5.2fx\n", pat_name[p], t_q, t_ours, t_q / t_ours);
    }

    puts("\n== sol_sort_f64 ==");
    for (enum pat p = P_RANDOM; p <= P_FEWUNIQ; p++) {
        fill_f64(af64, N, p);
        uint64_t t0 = ns_now();
        sol_sort_f64(af64, N);
        double t_ours = (double)(ns_now() - t0) / N;

        fill_f64(af64, N, p);
        t0 = ns_now();
        qsort(af64, N, 8, cmp_f64);
        double t_q = (double)(ns_now() - t0) / N;
        printf("  %-9s qsort %8.2fns vs sol_sort_f64 %8.2fns  -> %5.2fx\n", pat_name[p], t_q, t_ours, t_q / t_ours);
    }
}

int main(void) {
    printf("Sort benchmark (N=%d)\n\n== generic drop-in sol_qsort ==\n", N);
    for (enum pat p = P_RANDOM; p <= P_FEWUNIQ; p++) bench_generic_i32(p);
    for (enum pat p = P_RANDOM; p <= P_FEWUNIQ; p++) bench_generic_f64(p);

    bench_specialized();
    return 0;
}
