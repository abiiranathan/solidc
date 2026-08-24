#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * dynarray_micro.c — throughput benchmarks for the dynamic array.
 *
 * Reports ns/op for the hot paths across element sizes that exercise
 * the different copy widths (4/8/24/64 bytes).  Run before and after
 * changes to dynarray.c to keep optimizations honest.
 */

#include "../include/dynarray.h"

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

#define N 400000

typedef struct {
    double push, pop, get, insert_mid, swap_remove;
} Row;

static void bench_es(size_t es, Row* r) {
    dynarray_t arr;
    if (!dynarray_init(&arr, es, 16)) exit(1);

    unsigned char* elem = calloc(1, es);
    unsigned char* out = calloc(1, es);
    uintptr_t sink = 0;

    uint64_t t0 = ns_now();
    for (size_t i = 0; i < N; i++) {
        elem[i % es] = (unsigned char)i;
        if (!dynarray_push(&arr, elem)) exit(1);
    }
    r->push = (double)(ns_now() - t0) / N;

    t0 = ns_now();
    for (size_t i = 0; i < N; i++) {
        sink += (uintptr_t)dynarray_get(&arr, (i * 7919u) % N);
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->get = (double)(ns_now() - t0) / N;

    /* middle inserts/removes at a fixed position (worst-case memmove) */
    t0 = ns_now();
    for (int i = 0; i < 20000; i++) {
        if (!dynarray_insert(&arr, arr.size / 2, elem)) exit(1);
    }
    r->insert_mid = (double)(ns_now() - t0) / 20000;

    t0 = ns_now();
    for (int i = 0; i < 20000; i++) {
        if (!dynarray_swap_remove(&arr, arr.size / 2, NULL)) exit(1);
    }
    r->swap_remove = (double)(ns_now() - t0) / 20000;

    t0 = ns_now();
    while (arr.size > 0) {
        if (!dynarray_pop(&arr, out)) exit(1);
        sink += (uintptr_t)out[0];
    }
#if defined(__GNUC__)
    __asm__ volatile("" ::"r"(sink) : "memory");
#endif
    r->pop = (double)(ns_now() - t0) / N;

    free(elem);
    free(out);
    dynarray_free(&arr);
}

int main(void) {
    printf("dynarray micro-benchmarks (N=%d)\n\n", N);
    printf("%-10s %10s %10s %10s %12s %12s\n", "elem B", "push", "pop", "get", "insert-mid", "swap-remove");
    const size_t sizes[] = {4, 8, 24, 64};
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        Row r = {0};
        bench_es(sizes[s], &r);
        printf("%-10zu %8.1fns %8.1fns %8.1fns %10.1fns %12.1fns\n", sizes[s], r.push, r.pop, r.get, r.insert_mid,
               r.swap_remove);
    }
    return 0;
}
