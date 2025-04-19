#include <time.h>
#define MAX_THREADS 4  // override the default value in threadpool.h

#include "../include/macros.h"
#include "../include/map.h"
#include "../include/threadpool.h"

// Uncomment this to test with 100m rows
// Be sure to have enough memory and storage space > 2GB
#define MAP_SIZE 1000000  // 1m rows take about 100ms

typedef struct {
    int* key;
    int* value;
    Map* m;
} Arg;

void concurrent_insert(void* arg) {
    Arg* a = (Arg*)arg;
    map_set_safe(a->m, a->key, a->value);
}

void test_concurrent_map() {
    ThreadPool* pool = threadpool_create(4);
    ASSERT(pool);

    Map* m = map_create(MapConfigInt);
    ASSERT(m);

    Arg args[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i) {
        args[i].key = malloc(sizeof(int));
        ASSERT(args[i].key);
        args[i].value = malloc(sizeof(int));
        ASSERT(args[i].value);
        *args[i].key   = i;
        *args[i].value = i;
        args[i].m      = m;
        threadpool_submit(pool, concurrent_insert, &args[i]);
    }

    threadpool_destroy(pool);

    // check if all values are inserted
    for (int i = 0; i < MAX_THREADS; ++i) {
        const int* value = map_get(m, args[i].key);
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
        .key_len_func     = key_len_int,
        .key_free         = NOFREE,
        .value_free       = NOFREE,
    };

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    Map* m = map_create(&cfg);
    ASSERT(m);

    for (int i = 0; i < MAP_SIZE; ++i) {
        arr[i] = i;
        map_set(m, &arr[i], &arr[i]);
    }

    const int* one = map_get(m, &arr[1]);
    ASSERT(one);

    // test map_get_safe
    const int* two = map_get_safe(m, &arr[2]);
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

    double took = (((double)(end.tv_sec - start.tv_sec) * 1000) + (double)(end.tv_nsec - start.tv_nsec) / 1e6);
    printf("Took: %.2f ms\n", took);
}
