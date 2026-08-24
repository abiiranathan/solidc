#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * swiss_micro.c — head-to-head A/B benchmark:
 *
 *   HashMap  (src/map.c)      Robin Hood, linear probing, size_t DIB array
 *   SwissMap (src/map_swiss.c) SIMD group probing, 1-byte control metadata
 *
 * Identical workloads, identical key sets and RNG seed; the two tables
 * differ only in internal layout/probing.  Reports ns/op for each side
 * plus the ratio.
 */

#include "../include/map.h"
#include "../include/swiss_map.h"

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
static inline uint64_t xrng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

#define N 1000000

static int* make_keys(int n) {
    int* keys = malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) keys[i] = i;
    return keys;
}

/* Deterministic shuffled access order (Fisher-Yates, fixed seed) so both
 * tables see identical random workloads. */
static int* make_perm(int n) {
    int* p = malloc((size_t)n * sizeof(int));
    uint64_t s = 0x9E3779B97F4A7C15ull;
    for (int i = 0; i < n; i++) p[i] = i;
    for (int i = n - 1; i > 0; i--) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        int j = (int)(s % (uint64_t)(i + 1));
        int t = p[i]; p[i] = p[j]; p[j] = t;
    }
    return p;
}

/* ------------------------------------------------------------------ */
/* Robin Hood (current map.c)                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    double set, get_hit, get_miss, remove_half, mixed, str_set, str_get;
    double rnd_set, rnd_get;
} Result;

static void rh_all(Result* r) {
    int* keys = make_keys(N);

    HashMap* m = map_create(MapConfigInt);
    uint64_t t0 = ns_now();
    for (int i = 0; i < N; i++) map_set(m, &keys[i], sizeof(int), (void*)(intptr_t)i);
    r->set = (double)(ns_now() - t0) / N;

    uintptr_t sink = 0;
    t0 = ns_now();
    for (int i = 0; i < N; i++) sink += (uintptr_t)map_get(m, &keys[i], sizeof(int));
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->get_hit = (double)(ns_now() - t0) / N;

    /* misses */
    int* miss_keys = malloc((size_t)N * sizeof(int));
    for (int i = 0; i < N; i++) miss_keys[i] = N + i;
    t0 = ns_now();
    sink = 0;
    for (int i = 0; i < N; i++) sink += (uintptr_t)map_get(m, &miss_keys[i], sizeof(int));
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->get_miss = (double)(ns_now() - t0) / N;
    free(miss_keys);

    /* remove half */
    uint64_t removed = 0;
    t0 = ns_now();
    for (int i = 0; i < N; i += 2) removed += map_remove(m, &keys[i], sizeof(int)) ? 1 : 0;
    r->remove_half = (double)(ns_now() - t0) / removed;

    map_destroy(m);

    /* mixed churn */
    keys[0] = 0;
    m = map_create(MapConfigInt);
    for (int i = 0; i < N / 2; i++) map_set(m, &keys[i], sizeof(int), NULL);
    uint64_t ops = 0;
    t0 = ns_now();
    for (int i = 0; i < N; i++) {
        uint64_t rv = xrng();
        int idx = (int)(rv % (N / 2));
        void* k = &keys[idx];
        switch (rv % 4) {
            case 0:
            case 1: map_set(m, k, sizeof(int), (void*)(intptr_t)rv); break;
            case 2: sink += (uintptr_t)map_get(m, k, sizeof(int)); break;
            default: map_remove(m, k, sizeof(int)); break;
        }
        ops++;
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->mixed = (double)(ns_now() - t0) / ops;
    map_destroy(m);

    /* random-order int insert + lookup */
    {
        int* perm = make_perm(N);
        m = map_create(MapConfigInt);
        uint64_t t0 = ns_now();
        for (int i = 0; i < N; i++) map_set(m, &keys[perm[i]], sizeof(int), (void*)(intptr_t)i);
        r->rnd_set = (double)(ns_now() - t0) / N;

        uintptr_t sink2 = 0;
        t0 = ns_now();
        for (int i = 0; i < N; i++) sink2 += (uintptr_t)map_get(m, &keys[perm[(int)(((int64_t)i * 7919 + 13) % N)]], sizeof(int));
#if defined(__GNUC__)
        __asm__ volatile("" ::"r"(sink2) : "memory");
#endif
        r->rnd_get = (double)(ns_now() - t0) / N;
        map_destroy(m);
        free(perm);
    }

    /* strings */
    enum { SN = 100000 };
    char(*skeys)[32] = malloc(SN * 32);
    for (size_t i = 0; i < SN; i++) snprintf(skeys[i], 32, "key-%zu-%zu", i, i * 7919 % 1000);

    HashMap* sm = map_create(MapConfigStr);
    t0 = ns_now();
    for (size_t i = 0; i < SN; i++) map_set(sm, skeys[i], 32, (void*)(intptr_t)i);
    r->str_set = (double)(ns_now() - t0) / (double)SN;

    t0 = ns_now();
    sink = 0;
    for (size_t i = 0; i < SN; i++) sink += (uintptr_t)map_get(sm, skeys[(i * 7919) % SN], 32);
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->str_get = (double)(ns_now() - t0) / (double)SN;

    map_destroy(sm);
    free(skeys);
    free(keys);
}

/* ------------------------------------------------------------------ */
/* Swiss table                                                          */
/* ------------------------------------------------------------------ */

static void sw_all(Result* r) {
    int* keys = make_keys(N);

    SwissMap* m = swiss_create(SwissConfigInt);
    uint64_t t0 = ns_now();
    for (int i = 0; i < N; i++) {
        if (!swiss_set(m, &keys[i], sizeof(int), (void*)(intptr_t)i)) {
            fprintf(stderr, "swiss set failed at %d\n", i);
            exit(1);
        }
    }
    r->set = (double)(ns_now() - t0) / N;

    uintptr_t sink = 0;
    t0 = ns_now();
    for (int i = 0; i < N; i++) sink += (uintptr_t)swiss_get(m, &keys[i], sizeof(int));
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->get_hit = (double)(ns_now() - t0) / N;

    if ((size_t)N != swiss_length(m)) fprintf(stderr, "SWISS LENGTH MISMATCH %zu\n", swiss_length(m));

    int* miss_keys = malloc((size_t)N * sizeof(int));
    for (int i = 0; i < N; i++) miss_keys[i] = N + i;
    t0 = ns_now();
    sink = 0;
    for (int i = 0; i < N; i++) sink += (uintptr_t)swiss_get(m, &miss_keys[i], sizeof(int));
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->get_miss = (double)(ns_now() - t0) / N;
    free(miss_keys);

    uint64_t removed = 0;
    t0 = ns_now();
    for (int i = 0; i < N; i += 2) removed += swiss_remove(m, &keys[i], sizeof(int)) ? 1 : 0;
    r->remove_half = (double)(ns_now() - t0) / removed;

    /* verify removals actually landed */
    if ((size_t)N / 2 != swiss_length(m)) fprintf(stderr, "SWISS REMOVE MISMATCH %zu\n", swiss_length(m));

    swiss_destroy(m);

    keys[0] = 0;
    m = swiss_create(SwissConfigInt);
    for (int i = 0; i < N / 2; i++) swiss_set(m, &keys[i], sizeof(int), NULL);
    uint64_t ops = 0;
    t0 = ns_now();
    for (int i = 0; i < N; i++) {
        uint64_t rv = xrng();
        int idx = (int)(rv % (N / 2));
        void* k = &keys[idx];
        switch (rv % 4) {
            case 0:
            case 1: swiss_set(m, k, sizeof(int), (void*)(intptr_t)rv); break;
            case 2: sink += (uintptr_t)swiss_get(m, k, sizeof(int)); break;
            default: swiss_remove(m, k, sizeof(int)); break;
        }
        ops++;
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->mixed = (double)(ns_now() - t0) / ops;
    swiss_destroy(m);

    /* random-order int insert + lookup */
    {
        int* perm = make_perm(N);
        m = swiss_create(SwissConfigInt);
        uint64_t t0 = ns_now();
        for (int i = 0; i < N; i++) {
            if (!swiss_set(m, &keys[perm[i]], sizeof(int), (void*)(intptr_t)i)) exit(1);
        }
        r->rnd_set = (double)(ns_now() - t0) / N;

        uintptr_t sink2 = 0;
        t0 = ns_now();
        for (int i = 0; i < N; i++) sink2 += (uintptr_t)swiss_get(m, &keys[perm[(int)(((int64_t)i * 7919 + 13) % N)]], sizeof(int));
#if defined(__GNUC__)
        __asm__ volatile("" ::"r"(sink2) : "memory");
#endif
        r->rnd_get = (double)(ns_now() - t0) / N;
        swiss_destroy(m);
        free(perm);
    }

    enum { SN = 100000 };
    char(*skeys)[32] = malloc(SN * 32);
    for (size_t i = 0; i < SN; i++) snprintf(skeys[i], 32, "key-%zu-%zu", i, i * 7919 % 1000);

    SwissMap* sm = swiss_create(SwissConfigStr);
    t0 = ns_now();
    for (size_t i = 0; i < SN; i++) swiss_set(sm, skeys[i], 32, (void*)(intptr_t)i);
    r->str_set = (double)(ns_now() - t0) / (double)SN;

    t0 = ns_now();
    sink = 0;
    for (size_t i = 0; i < SN; i++) sink += (uintptr_t)swiss_get(sm, skeys[(i * 7919) % SN], 32);
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->str_get = (double)(ns_now() - t0) / (double)SN;

    swiss_destroy(sm);
    free(skeys);
    free(keys);
}

int main(void) {
    printf("Robin Hood (map.c) vs Swiss table — N=%d\n\n", N);
    printf("%-18s %12s %12s %8s\n", "scenario", "robin-hood", "swiss", "ratio");

    Result rh = {0}, sw = {0};
    rh_all(&rh);
    sw_all(&sw);

    struct {
        const char* name;
        double rh, sw;
    } rows[] = {
        {"int set", rh.set, sw.set},
        {"int get hit", rh.get_hit, sw.get_hit},
        {"int get miss", rh.get_miss, sw.get_miss},
        {"int remove 1/2", rh.remove_half, sw.remove_half},
        {"int rnd set", rh.rnd_set, sw.rnd_set},
        {"int rnd get hit", rh.rnd_get, sw.rnd_get},
        {"mixed 50/25/25", rh.mixed, sw.mixed},
        {"str set 100k", rh.str_set, sw.str_set},
        {"str get 100k", rh.str_get, sw.str_get},
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        printf("%-18s %10.1fns %10.1fns %7.2fx %s\n", rows[i].name, rows[i].rh, rows[i].sw,
               rows[i].rh / rows[i].sw, rows[i].rh / rows[i].sw > 1 ? "(swiss wins)" : "");
    }
    return 0;
}
