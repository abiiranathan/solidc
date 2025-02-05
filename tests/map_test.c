#define MAX_THREADS 4  // override the default value in threadpool.h

#include "../include/map.h"
#include <assert.h>
#include "../include/threadpool.h"

// Uncomment this to test with 100m rows
// Be sure to have enough memory and storage space > 2GB
// #define MAP_SIZE 100000000  // 100m rows
#define MAP_SIZE 1000000  // 1m rows

typedef struct {
    int* key;
    int* value;
    map* m;
} Arg;

void concurrent_insert(void* arg) {
    Arg* a = (Arg*)arg;
    map_set_safe(a->m, a->key, a->value);
    printf("Inserted key: %d, value: %d\n", *a->key, *a->value);
}

void test_concurrent_map() {
    ThreadPool* pool = threadpool_create(4);
    assert(pool);

    map* m = map_create(MAP_SIZE, key_compare_int);
    assert(m);

    Arg args[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i) {
        args[i].key = malloc(sizeof(int));
        assert(args[i].key);
        args[i].value = malloc(sizeof(int));
        assert(args[i].value);
        *args[i].key = i;
        *args[i].value = i;
        args[i].m = m;
        threadpool_add_task(pool, concurrent_insert, &args[i]);
    }

    threadpool_wait(pool);
    threadpool_destroy(pool);

    // check if all values are inserted
    for (int i = 0; i < MAX_THREADS; ++i) {
        const int* value = map_get_safe(m, args[i].key);
        assert(value);
        assert(*value == i);
    }

    map_destroy(m, true);
}

int main(void) {
    int* arr = malloc(MAP_SIZE * sizeof(int));
    assert(arr);

    map* m = map_create(MAP_SIZE, key_compare_int);
    assert(m);

    for (int i = 0; i < MAP_SIZE; ++i) {
        arr[i] = i;
        map_set(m, &arr[i], &arr[i]);
    }

    const int* one = map_get(m, &arr[1]);
    assert(one);
    // test map_get_safe
    const int* two = map_get_safe(m, &arr[2]);
    assert(two);

    FILE* fp = fopen("map_test.txt", "w");
    assert(fp);

    // print all map values with map_for_each
    map_foreach(m, e) {
        fprintf(fp, "Key: %d, Value: %d\n", *(int*)e->key, *(int*)e->value);
    }

    fclose(fp);

    // free array
    free(arr);

    // free map
    // entries are not freed because they are allocated on the stack
    map_destroy(m, false);
    test_concurrent_map();
}