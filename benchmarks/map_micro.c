#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * map_micro.c — overhead-isolating benchmarks for the HashMap.
 *
 * Scenarios (all report ns/op unless noted):
 *   int-set      — insert N sequential int keys (identity-hash friendly).
 *   int-get-hit  — successful lookups of every inserted key.
 *   int-get-miss — lookups of absent keys (full probe-to-empty cost).
 *   int-remove   — remove half the keys, then the rest.
 *   mixed        — interleaved 50% set / 25% get / 25% remove churn
 *                  (the pattern that used to degrade tombstone maps).
 *   str-set/get  — heap-string keys routed through xxhash3 + strcmp.
 *
 * A random sequence (xorshift64) drives mixed workloads; identical seed
 * across runs keeps before/after comparisons honest.
 */

#include "../include/map.h"

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
static inline uint64_t xrng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

#define N 1000000

/* Keys MUST live at stable addresses for the lifetime of the entry:
 * the map stores the caller's pointer, so a &stack_var that changes
 * content between calls aliases previously inserted entries and
 * degenerates the table into duplicate-key chains. */
static int* make_keys(int n) {
    int* keys = malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) keys[i] = i;
    return keys;
}

static double bench_int_set(void) {
    int* keys = make_keys(N);
    HashMap* m = map_create(MapConfigInt);
    uint64_t t0 = ns_now();
    for (int i = 0; i < N; i++) {
        if (!map_set(m, &keys[i], sizeof(int), (void*)(intptr_t)i)) { fprintf(stderr, "set failed\n"); exit(1); }
    }
    double d = (double)(ns_now() - t0) / N;
    map_destroy(m);
    free(keys);
    return d;
}

static double bench_int_get_hit(HashMap* m, const int* keys) {
    uint64_t t0 = ns_now();
    uintptr_t sink = 0;
    for (int i = 0; i < N; i++) {
        sink += (uintptr_t)map_get(m, (void*)&keys[i], sizeof(int));
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    return (double)(ns_now() - t0) / N;
}

static double bench_int_get_miss(const int* keys, int* miss_keys) {
    HashMap* m = map_create(MapConfigInt);
    for (int i = 0; i < N; i++) map_set(m, (void*)&keys[i], sizeof(int), NULL);
    uint64_t t0 = ns_now();
    uintptr_t sink = 0;
    for (int i = 0; i < N; i++) {
        /* keys are [0,N); miss_keys hold [N,2N) as guaranteed misses.
         * Lookup keys need only be valid DURING the call. */
        sink += (uintptr_t)map_get(m, (void*)&miss_keys[i], sizeof(int));
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    double d = (double)(ns_now() - t0) / N;
    map_destroy(m);
    return d;
}

static double bench_int_remove(int frac_denom, const int* keys) {
    HashMap* m = map_create(MapConfigInt);
    for (int i = 0; i < N; i++) map_set(m, (void*)&keys[i], sizeof(int), NULL);
    uint64_t removed = 0;
    uint64_t t0 = ns_now();
    for (int i = 0; i < N; i += frac_denom) {
        removed += map_remove(m, (void*)&keys[i], sizeof(int)) ? 1 : 0;
    }
    double d = (double)(ns_now() - t0) / removed;
    map_destroy(m);
    return d;
}

/* Mixed churn uses a pool of stable heap keys; the working-set size (N/2)
 * keeps most operations hitting existing entries. */
static double bench_mixed(void) {
    int* keys = make_keys(N);
    HashMap* m = map_create(MapConfigInt);
    /* pre-populate half the space */
    for (int i = 0; i < N / 2; i++) map_set(m, (void*)&keys[i], sizeof(int), NULL);

    uint64_t ops = 0;
    uint64_t t0 = ns_now();
    for (int i = 0; i < N; i++) {
        uint64_t r = xrng();
        int idx = (int)(r % (N / 2));
        void* k = &keys[idx];
        switch (r % 4) {
            case 0:
            case 1: {
                map_set(m, k, sizeof(int), (void*)(intptr_t)r);
                ops++;
                break;
            }
            case 2: {
                void* v = map_get(m, k, sizeof(int));
                (void)v;
                ops++;
                break;
            }
            default: {
                map_remove(m, k, sizeof(int));
                ops++;
                break;
            }
        }
    }
    double d = (double)(ns_now() - t0) / ops;
    map_destroy(m);
    free(keys);
    return d;
}

static double bench_str(size_t n, bool query) {
    char(*keys)[32] = malloc(n * 32);
    for (size_t i = 0; i < n; i++) snprintf(keys[i], 32, "key-%zu-%zu", i, i * 7919 % 1000);

    HashMap* m = map_create(MapConfigStr);
    for (size_t i = 0; i < n; i++) map_set(m, keys[i], 32, (void*)(intptr_t)i);

    uint64_t t0 = ns_now();
    uintptr_t sink = 0;
    for (size_t i = 0; i < n; i++) {
        if (query) {
            sink += (uintptr_t)map_get(m, keys[(i * 7919) % n], 32);
        } else {
            map_set(m, keys[i], 32, (void*)(intptr_t)(i + 1)); /* update-in-place */
        }
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    double d = (double)(ns_now() - t0) / n;
    map_destroy(m);
    free(keys);
    return d;
}

int main(void) {
    printf("Map Micro-Benchmarks (N=%d, best practice: lower is better)\n\n", N);

    printf("  int set          : %7.1f ns/op\n", bench_int_set());

    int* keys = make_keys(N);
    HashMap* m = map_create(MapConfigInt);
    for (int i = 0; i < N; i++) map_set(m, (void*)&keys[i], sizeof(int), (void*)(intptr_t)i);
    printf("  int get (hit)    : %7.1f ns/op\n", bench_int_get_hit(m, keys));
    map_destroy(m);

    int* miss_keys = malloc((size_t)N * sizeof(int));
    for (int i = 0; i < N; i++) miss_keys[i] = N + i;
    printf("  int get (miss)   : %7.1f ns/op\n", bench_int_get_miss(keys, miss_keys));
    free(miss_keys);
    printf("  int remove 1/2   : %7.1f ns/op\n", bench_int_remove(2, keys));
    free(keys);
    printf("  mixed 50/25/25   : %7.1f ns/op\n", bench_mixed());
    printf("  str set 100k     : %7.1f ns/op\n", bench_str(100000, false));
    printf("  str get 100k     : %7.1f ns/op\n", bench_str(100000, true));
    return 0;
}
