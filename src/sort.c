/**
 * @file sort.c
 * @brief Implementation of sol_qsort (introsort) and the branchless
 *        type-specialized sorts declared in sort.h.
 *
 * Design notes
 * ------------
 * Generic path (sol_qsort):
 *   Introsort: quicksort with median-of-three pivots (ninther above 128
 *   elements), insertion sort below 24 elements, and a heapsort fallback
 *   once recursion depth exceeds 2*log2(n).  The depth bound guarantees
 *   O(n log n) worst case -- unlike plain quicksort, adversarial inputs
 *   cannot degrade it.  Hoare partitioning keeps duplicate-heavy inputs
 *   balanced (equal keys stop both pointers).
 *
 * Specialized paths (i32/i64/f64):
 *   Branchless Lomuto partition using conditional moves; the compiler
 *   emits cmov/csel so there is no data-dependent branching in the hot
 *   loop and no function-pointer traffic at all.
 */
#include "../include/sort.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* Generic introsort                                                         */
/* ========================================================================= */

#define SW_INSERTION_THRESHOLD 24
#define SW_NINTHER_THRESHOLD 128
#define SW_STACK_TMP_MAX 64

typedef int (*sw_cmp)(const void*, const void*);

static inline void sw_swap(char* a, char* b, size_t sz, char* tmp) {
    if (sz <= SW_STACK_TMP_MAX) {
        char buf[SW_STACK_TMP_MAX];
        memcpy(buf, a, sz);
        memcpy(a, b, sz);
        memcpy(b, buf, sz);
    } else {
        memcpy(tmp, a, sz);
        memcpy(a, b, sz);
        memcpy(b, tmp, sz);
    }
}

/* Insertion sort over [lo, hi] (inclusive pointers). O(n) when nearly sorted. */
static void sw_insertion_sort(char* lo, char* hi, size_t sz, sw_cmp cmp, char* tmp) {
    for (char* i = lo + sz; i <= hi; i += sz) {
        char* j = i;
        while (j > lo && cmp(j, j - sz) < 0) {
            sw_swap(j, j - sz, sz, tmp);
            j -= sz;
        }
    }
}

static void sw_sift_down(char* base, size_t root, size_t end, size_t sz, sw_cmp cmp, char* tmp) {
    for (;;) {
        size_t child = 2 * root + 1;
        if (child >= end) break;
        if (child + 1 < end && cmp(base + child * sz, base + (child + 1) * sz) < 0) child++;
        if (cmp(base + root * sz, base + child * sz) >= 0) break;
        sw_swap(base + root * sz, base + child * sz, sz, tmp);
        root = child;
    }
}

static void sw_heapsort(char* base, size_t n, size_t sz, sw_cmp cmp, char* tmp) {
    if (n < 2) return;
    for (size_t i = n / 2; i-- > 0;) sw_sift_down(base, i, n, sz, cmp, tmp);
    for (size_t e = n; e-- > 1;) {
        sw_swap(base, base + e * sz, sz, tmp);
        sw_sift_down(base, 0, e, sz, cmp, tmp);
    }
}

static inline char* sw_med3(char* a, char* b, char* c, sw_cmp cmp) {
    return cmp(a, b) < 0 ? (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a))
                         : (cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c));
}

/* Ninther: median of three medians of three spaced samples. */
static char* sw_ninther(char* base, size_t n, size_t sz, sw_cmp cmp) {
    size_t step = n / 8;
    char* a = sw_med3(base, base + step * sz, base + 2 * step * sz, cmp);
    char* b = sw_med3(base + 3 * step * sz, base + 4 * step * sz, base + 5 * step * sz, cmp);
    char* c = sw_med3(base + 6 * step * sz, base + 7 * step * sz, base + (n - 1) * sz, cmp);
    return sw_med3(a, b, c, cmp);
}

/*
 * Hoare partition over base[lo..hi] (indices inclusive); the pivot value
 * must already reside at index lo.  Returns the pivot's final position:
 * everything left is <= pivot, everything right is >= pivot.  Equal keys
 * stop BOTH pointers, so all-equal arrays split evenly instead of
 * degenerating to O(n^2).
 */
static size_t sw_partition(char* base, size_t lo, size_t hi, size_t sz, sw_cmp cmp, char* tmp) {
    size_t i = lo + 1;
    size_t j = hi;
    for (;;) {
        while (i <= j && cmp(base + i * sz, base + lo * sz) < 0) i++;
        while (i <= j && cmp(base + j * sz, base + lo * sz) > 0) j--;
        if (i >= j) break;
        sw_swap(base + i * sz, base + j * sz, sz, tmp);
        i++;
        j--;
    }
    /* j now indexes the last element <= pivot. */
    sw_swap(base + lo * sz, base + j * sz, sz, tmp);
    return j;
}

static void sw_qsort_rec(char* base, size_t n, size_t sz, sw_cmp cmp, char* tmp, size_t depth) {
    while (n > SW_INSERTION_THRESHOLD) {
        if (depth-- == 0) {
            sw_heapsort(base, n, sz, cmp, tmp);
            return;
        }

        /* Pivot selection: move the chosen median to index 0. */
        char* pivot;
        if (n > SW_NINTHER_THRESHOLD) {
            pivot = sw_ninther(base, n, sz, cmp);
        } else {
            pivot = sw_med3(base, base + (n / 2) * sz, base + (n - 1) * sz, cmp);
        }
        if (pivot != base) sw_swap(base, pivot, sz, tmp);

        size_t p = sw_partition(base, 0, n - 1, sz, cmp, tmp);
        size_t left = p;
        size_t right = n - p - 1;

        /* Recurse into the smaller side; iterate on the larger one. */
        if (left < right) {
            sw_qsort_rec(base, left, sz, cmp, tmp, depth);
            base += (p + 1) * sz;
            n = right;
        } else {
            sw_qsort_rec(base + (p + 1) * sz, right, sz, cmp, tmp, depth);
            n = left;
        }
    }

    if (n > 1) sw_insertion_sort(base, base + (n - 1) * sz, sz, cmp, tmp);
}

void sol_qsort(void* base, size_t n, size_t size, int (*compar)(const void*, const void*)) {
    if (!base || !compar || size == 0 || n < 2) return;

    char stack_tmp[SW_STACK_TMP_MAX];
    char* heap_tmp = NULL;
    if (size > SW_STACK_TMP_MAX) {
        heap_tmp = malloc(size);
        if (!heap_tmp) {
            /* Out-of-memory fallback: correctness preserved via libc. */
            qsort(base, n, size, compar);
            return;
        }
    }

    size_t depth = 0;
    for (size_t v = n; v; v >>= 1) depth++;
    depth *= 2;

    sw_qsort_rec((char*)base, n, size, compar, heap_tmp ? heap_tmp : stack_tmp, depth);
    free(heap_tmp);
}

/* ========================================================================= */
/* Branchless specialized sorts                                              */
/* ========================================================================= */

#define SOLIDC_SORT_DEF(suffix, T, LESS)                                                                              \
    static inline bool solidc_lt_##suffix(T a, T b);                                                                  \
    static inline bool solidc_lt_##suffix(T a, T b) { return LESS; }                                                  \
                                                                                                                      \
    static inline T solidc_med3_##suffix(T x, T y, T z) {                                                             \
        return solidc_lt_##suffix(x, y) ? (solidc_lt_##suffix(y, z) ? y : (solidc_lt_##suffix(x, z) ? z : x))         \
                                        : (solidc_lt_##suffix(z, y) ? y : (solidc_lt_##suffix(z, x) ? z : x));         \
    }                                                                                                                 \
                                                                                                                      \
    static inline T solidc_pivot_##suffix(const T* a, size_t n) {                                                      \
        if (n > 128) {                                                                                                \
            size_t s = n / 8;                                                                                        \
            T m1 = solidc_med3_##suffix(a[0], a[s], a[2 * s]);                                                        \
            T m2 = solidc_med3_##suffix(a[3 * s], a[4 * s], a[5 * s]);                                                \
            T m3 = solidc_med3_##suffix(a[6 * s], a[7 * s], a[n - 1]);                                                \
            return solidc_med3_##suffix(m1, m2, m3);                                                                   \
        }                                                                                                            \
        return solidc_med3_##suffix(a[0], a[n / 2], a[n - 1]);                                                        \
    }                                                                                                                 \
                                                                                                                      \
    /* Branchless Lomuto: every element conditionally swapped via cmov.                                               \
     * Returns the count of elements strictly less than pv, and reports                                                              \
     * through *saw_eq whether any element equals pv (used for the                                                                  \
     * all-equal early exit). */                                                                                     \
    static inline size_t solidc_partition_##suffix(T* a, size_t n, T pv, bool* saw_eq) {                               \
        size_t store = 0;                                                                                            \
        *saw_eq = false;                                                                                             \
        for (size_t i = 0; i < n; i++) {                                                                             \
            T v = a[i];                                                                                              \
            unsigned lt = (unsigned)solidc_lt_##suffix(v, pv);                                                        \
            bool eq = !lt && !solidc_lt_##suffix(pv, v);                                                              \
            *saw_eq |= eq;                                                                                           \
            T d = a[store];                                                                                          \
            a[store] = lt ? v : d;                                                                                   \
            if (i != store) a[i] = lt ? d : v;                                                                       \
            store += lt;                                                                                             \
        }                                                                                                            \
        return store;                                                                                                \
    }                                                                                                                 \
                                                                                                                      \
    static inline void solidc_isort_##suffix(T* a, size_t n) {                                                        \
        for (size_t i = 1; i < n; i++) {                                                                             \
            T v = a[i];                                                                                              \
            size_t j = i;                                                                                            \
            while (j > 0 && solidc_lt_##suffix(v, a[j - 1])) {                                                        \
                a[j] = a[j - 1];                                                                                     \
                j--;                                                                                                 \
            }                                                                                                        \
            a[j] = v;                                                                                                \
        }                                                                                                            \
    }                                                                                                                 \
                                                                                                                      \
    static inline void solidc_sift_##suffix(T* a, size_t root, size_t end) {                                           \
        for (;;) {                                                                                                   \
            size_t c = 2 * root + 1;                                                                                  \
            if (c >= end) break;                                                                                     \
            if (c + 1 < end && solidc_lt_##suffix(a[c], a[c + 1])) c++;                                               \
            if (!solidc_lt_##suffix(a[root], a[c])) break;                                                            \
            T t = a[root];                                                                                            \
            a[root] = a[c];                                                                                          \
            a[c] = t;                                                                                                \
            root = c;                                                                                                \
        }                                                                                                            \
    }                                                                                                                 \
                                                                                                                      \
    static void solidc_heapsort_##suffix(T* a, size_t n) {                                                            \
        if (n < 2) return;                                                                                           \
        for (size_t i = n / 2; i-- > 0;) solidc_sift_##suffix(a, i, n);                                               \
        for (size_t e = n; e-- > 1;) {                                                                               \
            T t = a[0];                                                                                              \
            a[0] = a[e];                                                                                             \
            a[e] = t;                                                                                                \
            solidc_sift_##suffix(a, 0, e);                                                                            \
        }                                                                                                            \
    }                                                                                                                 \
                                                                                                                      \
    static void solidc_sort_rec_##suffix(T* a, size_t n, size_t depth) {                                               \
        while (n > 24) {                                                                                             \
            if (depth-- == 0) {                                                                                      \
                solidc_heapsort_##suffix(a, n);                                                                       \
                return;                                                                                              \
            }                                                                                                        \
            T pv = solidc_pivot_##suffix(a, n);                                                                       \
            bool saw_eq = false;                                                                                     \
            size_t mid = solidc_partition_##suffix(a, n, pv, &saw_eq);                                                \
            /* Three-way partition: split [mid, n) further into == pv | > pv.                                        \
             * The equality band is then excluded from recursion, keeping                                            \
             * duplicate-heavy inputs balanced (all-equal becomes O(n)).                                             \
             * Skipped entirely when no element equals the pivot -- the                                              \
             * common case for high-entropy keys. */                                                                  \
            size_t gt_start = mid;                                                                                    \
            if (saw_eq) {                                                                                            \
                size_t st = mid;                                                                                      \
                for (size_t i = mid; i < n; i++) {                                                                    \
                    T v = a[i];                                                                                      \
                    unsigned gt = (unsigned)solidc_lt_##suffix(pv, v);                                                \
                    T d = a[st];                                                                                     \
                    a[st] = gt ? d : v;                                                                               \
                    if (i != st) a[i] = gt ? v : d;                                                                   \
                    st += (size_t)(1u - gt);                                                                          \
                }                                                                                                    \
                gt_start = st;                                                                                        \
            }                                                                                                        \
            if (mid == 0 && gt_start == n) return; /* every element == pv: sorted */                                 \
            /* Defensive: cannot happen (pv is sampled from the array), but                                          \
             * never risk a non-progressing loop. */                                                                  \
            if ((mid == 0 && gt_start == 0)) {                                                                        \
                solidc_heapsort_##suffix(a, n);                                                                       \
                return;                                                                                              \
            }                                                                                                        \
            size_t left = mid;                                                                                        \
            size_t right = n - gt_start;                                                                              \
            if (left <= right) {                                                                                      \
                solidc_sort_rec_##suffix(a, left, depth);                                                             \
                a += gt_start;                                                                                       \
                n = right;                                                                                            \
            } else {                                                                                                  \
                solidc_sort_rec_##suffix(a + gt_start, right, depth);                                                  \
                n = left;                                                                                            \
            }                                                                                                        \
        }                                                                                                            \
        if (n > 1) solidc_isort_##suffix(a, n);                                                                       \
    }                                                                                                                 \
                                                                                                                      \
    void sol_sort_##suffix(T* a, size_t n) {                                                                          \
        if (!a || n < 2) return;                                                                                      \
        size_t depth = 0;                                                                                             \
        for (size_t v = n; v; v >>= 1) depth++;                                                                        \
        solidc_sort_rec_##suffix(a, n, depth * 2);                                                                     \
    }

/* NOTE on the store==n guard: pv is sampled from the array itself, so at
 * least one element is NOT less than pv and store < n always holds; the
 * check merely documents the invariant defensively. */

/* f64 ordering: NaNs compare greater than every real value, so they end
 * up sorted to the tail of the array (order among themselves is
 * unspecified).  The expression below implements that without a
 * comparator callback. */
SOLIDC_SORT_DEF(i32, int32_t, a < b)
SOLIDC_SORT_DEF(i64, int64_t, a < b)
SOLIDC_SORT_DEF(f64, double, (!__builtin_isnan(a)) && (__builtin_isnan(b) || a < b))
