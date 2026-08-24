#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/macros.h"
#include "../include/sort.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int cmp_i32(const void* a, const void* b) {
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);
}
static int cmp_i64(const void* a, const void* b) {
    int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
    return (x > y) - (x < y);
}
static int cmp_f64(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y); /* qsort semantics: NaNs compare equal -- our
                                 reference data contains none */
}

typedef struct {
    int32_t key;
    char payload[12]; /* 16 bytes total */
} Rec16;

static int big_cmp_helper(const void* a, const void* b);

static int cmp_rec16(const void* a, const void* b) {
    const Rec16 *x = a, *y = b;
    return (x->key > y->key) - (x->key < y->key);
}

static uint64_t rng_state = 0x243F6A8885A308D3ull;
static uint64_t xrng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static bool is_sorted_i32(const int32_t* a, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return false;
    return true;
}
static bool is_sorted_i64(const int64_t* a, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return false;
    return true;
}

static bool same_multiset_i32(const int32_t* a, const int32_t* b, size_t n) {
    /* sorted arrays: element-wise equality iff same multiset */
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return false;
    return true;
}

/* Fill with a chosen pattern. */
enum pat { PAT_RANDOM, PAT_SORTED, PAT_REVERSE, PAT_ALLEQUAL, PAT_FEWUNIQ, PAT_SAWTOOTH };

static void fill_pattern(int32_t* a, size_t n, enum pat p) {
    switch (p) {
        case PAT_RANDOM:
            for (size_t i = 0; i < n; i++) a[i] = (int32_t)(xrng() >> 33);
            break;
        case PAT_SORTED:
            for (size_t i = 0; i < n; i++) a[i] = (int32_t)i;
            break;
        case PAT_REVERSE:
            for (size_t i = 0; i < n; i++) a[i] = (int32_t)(n - i);
            break;
        case PAT_ALLEQUAL:
            memset(a, 0x2a, n * sizeof(int32_t));
            break;
        case PAT_FEWUNIQ:
            for (size_t i = 0; i < n; i++) a[i] = (int32_t)(xrng() % 4);
            break;
        case PAT_SAWTOOTH:
            for (size_t i = 0; i < n; i++) a[i] = (int32_t)(i % 97);
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Generic sol_qsort                                                   */
/* ------------------------------------------------------------------ */

static void test_qsort_matches_reference(void) {
    enum { N = 5000 };
    static int32_t a[N], b[N];
    for (size_t trial = 0; trial < 6; trial++) {
        fill_pattern(a, N, (enum pat)trial);
        memcpy(b, a, sizeof(a));

        sol_qsort(a, N, sizeof(int32_t), cmp_i32);
        qsort(b, N, sizeof(int32_t), cmp_i32);

        ASSERT(is_sorted_i32(a, N));
        ASSERT(same_multiset_i32(a, b, N));
    }
    printf("PASS qsort matches reference (6 patterns)\n");
}

/* Exhaustive-ish small sizes catch off-by-ones in partition boundaries. */
static void test_qsort_small_sizes(void) {
    enum { MAXN = 200 };
    static int32_t a[MAXN], b[MAXN];
    for (size_t n = 0; n <= MAXN; n++) {
        for (size_t t = 0; t < 4; t++) {
            switch (t) {
                case 0: fill_pattern(a, n, PAT_RANDOM); break;
                case 1: fill_pattern(a, n, PAT_SORTED); break;
                case 2: fill_pattern(a, n, PAT_REVERSE); break;
                default: fill_pattern(a, n, PAT_ALLEQUAL); break;
            }
            memcpy(b, a, n * sizeof(int32_t));
            sol_qsort(a, n, sizeof(int32_t), cmp_i32);
            qsort(b, n, sizeof(int32_t), cmp_i32);
            if (!is_sorted_i32(a, n) || !same_multiset_i32(a, b, n)) {
                fprintf(stderr, "FAIL at n=%zu pattern=%zu\n", n, t);
                exit(1);
            }
        }
    }
    printf("PASS qsort small sizes 0..%d\n", MAXN);
}

/* Non-trivial element size + NULL/edge args. */
typedef struct {
    double d;
    int tag;
} PairD;

static int cmp_paird(const void* a, const void* b) {
    const PairD *x = a, *y = b;
    return (x->d > y->d) - (x->d < y->d);
}

static void test_qsort_structs_and_edges(void) {
    enum { N = 3000 };
    static PairD v[N];
    for (size_t i = 0; i < N; i++) {
        v[i].d = (double)(xrng() >> 40) / (double)(1 << 20);
        v[i].tag = (int)i;
    }
    sol_qsort(v, N, sizeof(PairD), cmp_paird);
    for (size_t i = 1; i < N; i++) ASSERT(v[i - 1].d <= v[i].d);

    /* edges: NULL base, zero size, zero count, single element */
    sol_qsort(NULL, 5, sizeof(int), cmp_i32);
    int one[2] = {7, 7};
    sol_qsort(one, 0, sizeof(int), cmp_i32);
    sol_qsort(one, 1, sizeof(int), cmp_i32);
    sol_qsort(one, 2, 0, cmp_i32); /* degenerate size: no-op */
    sol_qsort(one, 2, sizeof(int), NULL);

    /* large element (>64B stack tmp path forces heap tmp) */
    typedef struct {
        unsigned char blob[200];
        int key;
    } Big;
    static Big bigs[500];
    for (size_t i = 0; i < 500; i++) {
        memset(bigs[i].blob, (int)(i & 0xFF), sizeof(bigs[i].blob));
        bigs[i].key = (int)(xrng() >> 40);
    }
    sol_qsort(bigs, 500, sizeof(Big), cmp_rec16 == NULL ? cmp_paird : big_cmp_helper);
    for (size_t i = 1; i < 500; i++) ASSERT(bigs[i - 1].key <= bigs[i].key);

    printf("PASS qsort structs + edge cases + large elements\n");
}

/* helper defined after use via prototype to keep the above tidy */
static int big_cmp_helper(const void* a, const void* b) {
    const int* x = (const int*)((const unsigned char*)a + 200);
    const int* y = (const int*)((const unsigned char*)b + 200);
    return (*x > *y) - (*x < *y);
}

/* Adversarial inputs must not degrade (introsort depth bound). */
static void test_qsort_adversarial(void) {
    /* organ pipe and median-of-3 killers */
    enum { N = 100000 };
    static int32_t a[N];
    for (size_t i = 0; i < N; i++) {
        size_t k = i % 3;
        /* classic med3 killer family */
        a[i] = (int32_t)((k == 0) ? i : (k == 1 ? N - i : N / 2 + ((i % 2) ? i : -i)));
    }
    sol_qsort(a, N, sizeof(int32_t), cmp_i32);
    ASSERT(is_sorted_i32(a, N));

    /* few unique values stress partition balance */
    for (size_t i = 0; i < N; i++) a[i] = (int32_t)(i % 3);
    sol_qsort(a, N, sizeof(int32_t), cmp_i32);
    ASSERT(is_sorted_i32(a, N));

    printf("PASS qsort adversarial patterns\n");
}

/* ------------------------------------------------------------------ */
/* Specialized sorts                                                   */
/* ------------------------------------------------------------------ */

static void ref_sort_i32(int32_t* a, size_t n) { qsort(a, n, sizeof(int32_t), cmp_i32); }

static void check_specialized_i32(enum pat p, size_t n) {
    static int32_t a[200000], b[200000];
    fill_pattern(a, n, p);
    memcpy(b, a, n * sizeof(int32_t));
    sol_sort_i32(a, n);
    ref_sort_i32(b, n);
    ASSERT(is_sorted_i32(a, n));
    ASSERT(same_multiset_i32(a, b, n));
}

static void test_specialized_i32(void) {
    for (enum pat p = PAT_RANDOM; p <= PAT_SAWTOOTH; p++) {
        check_specialized_i32(p, 100000);
        check_specialized_i32(p, 63);   /* below insertion threshold */
        check_specialized_i32(p, 25);   /* just above */
        check_specialized_i32(p, 1009); /* prime, quicksort path */
        /* radix threshold boundaries */
        check_specialized_i32(p, 1023);
        check_specialized_i32(p, 1024);
        check_specialized_i32(p, 1025);
        check_specialized_i32(p, 4096);
    }
    printf("PASS sol_sort_i32 patterns + sizes\n");
}

static void test_specialized_i64(void) {
    enum { N = 150000 };
    static int64_t a[N], b[N];
    for (size_t t = 0; t < 5; t++) {
        for (size_t i = 0; i < N; i++) {
            switch (t) {
                case 0: a[i] = (int64_t)xrng(); break;
                case 1: a[i] = (int64_t)i; break;
                case 2: a[i] = (int64_t)(N - i); break;
                case 3: a[i] = 42; break;
                default: a[i] = (int64_t)(xrng() % 5); break;
            }
        }
        memcpy(b, a, sizeof(a));
        sol_sort_i64(a, N);
        qsort(b, N, sizeof(int64_t), cmp_i64);
        ASSERT(is_sorted_i64(a, N));
        for (size_t i = 0; i < N; i++) ASSERT(a[i] == b[i]);
    }
    printf("PASS sol_sort_i64\n");
}

static void test_specialized_f64_nan(void) {
    enum { N = 8 };
    /* NaNs must land at the END of the array */
    double a[N] = {5.0, NAN, 1.0, NAN, -3.5, 2.0, INFINITY, 0.0};
    sol_sort_f64(a, N);
    /* reals first, ascending */
    for (size_t i = 0; i < 6; i++) {
        ASSERT(!__builtin_isnan(a[i]));
    }
    ASSERT(a[0] == -3.5 && a[1] == 0.0 && a[2] == 1.0 && a[3] == 2.0 && a[4] == 5.0 && a[5] == INFINITY);
    ASSERT(__builtin_isnan(a[6]) && __builtin_isnan(a[7]));
    printf("PASS sol_sort_f64 NaN placement\n");
}

int main(void) {
    test_qsort_small_sizes();
    test_qsort_matches_reference();
    test_qsort_structs_and_edges();
    test_qsort_adversarial();
    test_specialized_i32();
    test_specialized_i64();
    test_specialized_f64_nan();
    printf("All sort tests passed!\n");
    return 0;
}
