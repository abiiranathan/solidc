#include "../include/memory_pool.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void test_memory_pool_create(void) {
    MemoryPool* pool = mpool_create(0);
    assert(pool != NULL);
    mpool_destroy(pool);
    printf("test_memory_pool_create passed\n");
}

void test_memory_pool_alloc(void) {
    MemoryPool* pool = mpool_create(0);
    assert(pool != NULL);

    // Allocate 600 ints
    int** arr = (int**)mpool_alloc(pool, 600 * sizeof(int*));
    assert(arr != NULL);

    for (size_t i = 0; i < 600; i++) {
        arr[i] = mpool_alloc(pool, sizeof(int*));
        assert(arr[i] != NULL);
        *(arr[i]) = i;
    }

    // Validate that ints are correctly allocated
    for (size_t j = 0; j < 600; j++) {
        assert(*(arr[j]) == (int)j);
    }

    mpool_destroy(pool);
    printf("test_memory_pool_alloc passed\n");
}

int main(void) {
    test_memory_pool_create();
    test_memory_pool_alloc();
    printf("All tests passed\n");
    return 0;
}