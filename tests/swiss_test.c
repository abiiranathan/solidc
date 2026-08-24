#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define MAX_THREADS 4

#include "../include/macros.h"
#include "../include/swiss_map.h"
#include "../include/threadpool.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAP_SIZE 1000000

/* ------------------------------------------------------------------ */
/* Basic set/get/update/remove                                         */
/* ------------------------------------------------------------------ */

void test_basic_ops(void) {
    SwissMap* m = swiss_create(SwissConfigInt);
    ASSERT(m);

    static int keys[10000];
    for (int i = 0; i < 10000; i++) keys[i] = i;

    /* insert with distinct values (never 0, so NULL checks are valid) */
    for (int i = 0; i < 10000; i++) {
        ASSERT(swiss_set(m, &keys[i], sizeof(int), (void*)(intptr_t)(i + 1)));
    }
    ASSERT_EQ(swiss_length(m), 10000);

    /* verify values */
    for (int i = 0; i < 10000; i++) {
        intptr_t v = (intptr_t)swiss_get(m, &keys[i], sizeof(int));
        ASSERT(v == i + 1);
    }

    /* update in place must not grow the map */
    size_t before = swiss_length(m);
    ASSERT(swiss_set(m, &keys[5], sizeof(int), (void*)(intptr_t)42));
    ASSERT_EQ(swiss_length(m), before);
    ASSERT((intptr_t)swiss_get(m, &keys[5], sizeof(int)) == 42);

    /* misses */
    int absent = 999999;
    ASSERT(swiss_get(m, &absent, sizeof(int)) == NULL);

    swiss_destroy(m);
}

/* Force many resizes from a tiny table; verify nothing is lost. */
void test_resize_storm(void) {
    SwissMap* m = swiss_create(SwissConfigInt); /* default capacity 16 */
    ASSERT(m);

    enum { N = 200000 };
    static int keys[N];
    for (int i = 0; i < N; i++) keys[i] = i ^ 0x55555555; /* scattered */

    for (int i = 0; i < N; i++) {
        ASSERT(swiss_set(m, &keys[i], sizeof(int), (void*)(intptr_t)(i + 1)));
    }
    ASSERT_EQ(swiss_length(m), N);

    for (int i = 0; i < N; i++) {
        intptr_t v = (intptr_t)swiss_get(m, &keys[i], sizeof(int));
        if (v != i + 1) {
            fprintf(stderr, "resize storm: key %#x expected %d got %td\n", keys[i], i + 1, v);
            ASSERT(0);
        }
    }
    swiss_destroy(m);
}

/* Tombstone pressure: remove/insert churn must stay correct and bounded. */
void test_tombstone_churn(void) {
    SwissMap* m = swiss_create(SwissConfigInt);
    ASSERT(m);

    enum { N = 50000 };
    static int keys[N];
    for (int i = 0; i < N; i++) keys[i] = i * 7 + 3;

    for (int i = 0; i < N; i++) swiss_set(m, &keys[i], sizeof(int), (void*)(intptr_t)(i + 1));

    /* remove every other entry, then re-insert different values */
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < N; i += 2) {
            ASSERT(swiss_remove(m, &keys[i], sizeof(int)));
        }
        for (int i = 0; i < N; i += 2) {
            ASSERT(swiss_set(m, &keys[i], sizeof(int), (void*)(intptr_t)(i + round + 2)));
        }
        /* even entries survive each round */
        for (int i = 1; i < N; i += 2) {
            ASSERT(swiss_get(m, &keys[i], sizeof(int)) != NULL);
        }
    }

    ASSERT_EQ(swiss_length(m), N);
    swiss_destroy(m);
}

/* remove() of an absent key returns false and does not corrupt. */
void test_remove_absent(void) {
    SwissMap* m = swiss_create(SwissConfigInt);
    ASSERT(m);
    int k = 1;
    ASSERT(!swiss_remove(m, &k, sizeof(int)));
    ASSERT(swiss_set(m, &k, sizeof(int), (void*)(intptr_t)1));
    int k2 = 2;
    ASSERT(!swiss_remove(m, &k2, sizeof(int)));
    ASSERT(swiss_remove(m, &k, sizeof(int)));
    ASSERT(!swiss_remove(m, &k, sizeof(int))); /* already gone */
    ASSERT(swiss_get(m, &k, sizeof(int)) == NULL);
    /* reinsert after removal works */
    ASSERT(swiss_set(m, &k, sizeof(int), (void*)(intptr_t)9));
    ASSERT((intptr_t)swiss_get(m, &k, sizeof(int)) == 9);
    ASSERT_EQ(swiss_length(m), 1);
    swiss_destroy(m);
}

/* Iterator visits exactly the live entries. */
void test_iterator(void) {
    SwissMap* m = swiss_create(SwissConfigInt);
    ASSERT(m);

    enum { N = 5000 };
    static int keys[N];
    for (int i = 0; i < N; i++) keys[i] = i * 31;

    for (int i = 0; i < N; i++) swiss_set(m, &keys[i], sizeof(int), (void*)(intptr_t)(i + 1));

    /* delete a third */
    for (int i = 0; i < N; i += 3) swiss_remove(m, &keys[i], sizeof(int));

    size_t count = 0;
    swiss_iterator it = swiss_iter(m);
    void *k, *v;
    while (swiss_next(&it, &k, &v)) {
        count++;
        ASSERT(k && v);
    }
    ASSERT_EQ(count, N - (N + 2) / 3 + N / 3 - N / 3); /* sanity below */
    ASSERT_EQ(count, swiss_length(m));

    swiss_destroy(m);
}

/* String keys via XXH3 path.
 *
 * Unlike map.c, SwissMap stores each entry's key_len, so resizes rehash
 * with the ORIGINAL length — strlen()-style variable keys are safe here
 * even when they trigger growth. */
void test_string_keys(void) {
    SwissMap* m = swiss_create(SwissConfigStr);
    ASSERT(m);

    char buf[16][32];
    for (int i = 0; i < 16; i++) snprintf(buf[i], sizeof(buf[i]), "key-%d", i * 7919);

    /* variable-length keys, inserted from an empty table so the table
     * grows and rehashes mid-way */
    for (int i = 0; i < 16; i++) ASSERT(swiss_set(m, buf[i], strlen(buf[i]), (void*)(intptr_t)(i + 1)));
    for (int i = 0; i < 16; i++) {
        ASSERT_EQ((intptr_t)swiss_get(m, buf[i], strlen(buf[i])), i + 1);
    }

    swiss_destroy(m);
}

/* Ownership contract: key_free/value_free run on remove and destroy. */
static int freed_count = 0;
static void counting_free(void* p) {
    freed_count++;
    free(p);
}

void test_ownership_callbacks(void) {
    SwissMap* m = swiss_create(&(SwissConfig){.key_compare = key_compare_int, .key_free = free, .value_free = free});
    ASSERT(m);

    /* NOTE on the ownership contract (same as map_set): an update
     * matches by key CONTENT and keeps the ORIGINAL stored key pointer;
     * a caller-supplied duplicate key would leak.  Correct usage is to
     * reuse stable key pointers and only replace values. */
    int* keyptrs[100];
    for (int i = 0; i < 100; i++) {
        int* k = malloc(sizeof(int));
        int* v = malloc(sizeof(int));
        *k = i;
        *v = i;
        keyptrs[i] = k;
        ASSERT(swiss_set(m, k, sizeof(int), v));
    }

    /* update replaces values through value_free */
    for (int i = 0; i < 50; i++) {
        int* vv = malloc(sizeof(int));
        *vv = i * 10;
        ASSERT(swiss_set(m, keyptrs[i], sizeof(int), vv)); /* updates existing */
    }
    ASSERT_EQ(swiss_length(m), 100);

    swiss_destroy(m); /* frees all remaining keys+values */
}

/* ------------------------------------------------------------------ */
/* Concurrency                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    int* key;
    int* value;
    SwissMap* map;
} ThreadData;

void concurrent_insert(void* arg) {
    ThreadData* td = (ThreadData*)arg;
    swiss_set_safe(td->map, td->key, sizeof(int), td->value);
}

void test_concurrent_map(void) {
    Threadpool* pool = threadpool_create(4);
    ASSERT(pool);

    SwissConfig* config = SwissConfigInt;
    config->key_free = free;
    config->value_free = free;

    SwissMap* m = swiss_create(config);
    ASSERT(m);

    ThreadData args[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i) {
        args[i].key = malloc(sizeof(int));
        args[i].value = malloc(sizeof(int));
        *args[i].key = i;
        *args[i].value = i;
        args[i].map = m;
        threadpool_submit(pool, concurrent_insert, &args[i]);
    }

    threadpool_destroy(pool, -1);

    for (int i = 0; i < MAX_THREADS; ++i) {
        const int* value = swiss_get(m, args[i].key, sizeof(int));
        ASSERT(value);
        ASSERT(*value == i);
    }

    swiss_destroy(m);
}

/* NULL-safety. */
void test_null_safety(void) {
    ASSERT(swiss_create(NULL) == NULL);
    ASSERT(swiss_create(&(SwissConfig){0}) == NULL); /* no key_compare */
    ASSERT(swiss_set(NULL, NULL, 0, NULL) == false);
    ASSERT(swiss_get(NULL, NULL, 0) == NULL);
    bool r = swiss_remove(NULL, NULL, 0);
    ASSERT(!r);
    swiss_destroy(NULL);
}

int main(void) {
    test_basic_ops();
    printf("PASS basic ops\n");
    test_resize_storm();
    printf("PASS resize storm\n");
    test_tombstone_churn();
    printf("PASS tombstone churn\n");
    test_remove_absent();
    printf("PASS remove absent\n");
    test_iterator();
    printf("PASS iterator\n");
    test_string_keys();
    printf("PASS string keys\n");
    test_ownership_callbacks();
    printf("PASS ownership callbacks\n");
    test_concurrent_map();
    printf("PASS concurrent\n");
    test_null_safety();
    printf("PASS null safety\n");
    return 0;
}
