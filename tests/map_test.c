#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define MAX_THREADS 4

#include "../include/map.h"
#include "../include/macros.h"
#include "../include/threadpool.h"

#include <time.h>

#define MAP_SIZE 1000000

typedef struct {
    int* key;
    int* value;
    HashMap* map;
} ThreadData;

void concurrent_insert(void* arg) {
    ThreadData* thread_data = (ThreadData*)arg;
    map_set_safe(thread_data->map, thread_data->key, sizeof(int), thread_data->value);
}

void test_concurrent_map() {
    Threadpool* pool = threadpool_create(4);
    ASSERT(pool);

    MapConfig* config  = MapConfigInt;
    config->key_free   = free;
    config->value_free = free;

    HashMap* m = map_create(config);
    ASSERT(m);

    ThreadData args[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i) {
        args[i].key = malloc(sizeof(int));
        ASSERT(args[i].key);
        args[i].value = malloc(sizeof(int));
        ASSERT(args[i].value);
        *args[i].key   = i;
        *args[i].value = i;
        args[i].map    = m;
        threadpool_submit(pool, concurrent_insert, &args[i]);
    }

    threadpool_destroy(pool, -1);

    // check if all values are inserted
    for (int i = 0; i < MAX_THREADS; ++i) {
        const int* value = map_get(m, args[i].key, sizeof(int));
        ASSERT(value);
        ASSERT(*value == i);
    }

    map_destroy(m);
}

int main(void) {

    int* arr = malloc(MAP_SIZE * sizeof(int));
    ASSERT(arr);

    // Explicit configuration.
    MapConfig cfg = {
        .initial_capacity = MAP_SIZE,
        .key_compare      = key_compare_int,
        .key_free         = NULL,
        .value_free       = NULL,
    };

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    HashMap* m = map_create(&cfg);
    ASSERT(m);

    for (int i = 0; i < MAP_SIZE; ++i) {
        arr[i] = i;
        map_set(m, &arr[i], sizeof(int), &arr[i]);
    }

    const int* one = map_get(m, &arr[1], sizeof(int));
    ASSERT(one);

    // test map_get_safe
    const int* two = map_get_safe(m, &arr[2], sizeof(int));
    ASSERT(two);

    map_iterator it = map_iter(m);
    int *key, *value;
    while (map_next(&it, (void**)&key, (void**)&value)) {
        ASSERT(key);
        ASSERT(value);
    }

    // free array
    free(arr);

    map_destroy(m);

    test_concurrent_map();

    clock_gettime(CLOCK_MONOTONIC, &end);

    double took = (((double)(end.tv_sec - start.tv_sec) * 1000) +
                   (double)(end.tv_nsec - start.tv_nsec) / 1e6);
    printf("Took: %.2f ms\n", took);
}
