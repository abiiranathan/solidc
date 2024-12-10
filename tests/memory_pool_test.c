#include "../include/memory_pool.h"
#include <assert.h>
#include <stdio.h>

void test_memory_pool_create(void) {
    MemoryPool* pool = memory_pool_create(MEMORY_POOL_BLOCK_SIZE);
    assert(pool != NULL);
    assert(pool->block_size == MEMORY_POOL_BLOCK_SIZE);
    memory_pool_destroy(pool);
    printf("test_memory_pool_create passed\n");
}

void test_memory_pool_alloc(void) {
    MemoryPool* pool = memory_pool_create(MEMORY_POOL_BLOCK_SIZE);
    assert(pool != NULL);

    // Allocate 1000 ints
    int** arr = memory_pool_alloc(pool, 1000 * sizeof(int*));
    assert(arr != NULL);

    for (int i = 0; i < 1000; i++) {
        arr[i] = memory_pool_alloc(pool, sizeof(int));
        assert(arr[i] != NULL);
    }

    // Validate that ints are correctly allocated
    for (int i = 0; i < 1000; i++) {
        *arr[i] = i;
        assert(*arr[i] == i);
    }

    memory_pool_destroy(pool);
    printf("test_memory_pool_alloc passed\n");
}

void test_memory_pool_destroy(void) {
    MemoryPool* pool = memory_pool_create(MEMORY_POOL_BLOCK_SIZE);
    assert(pool != NULL);

    memory_pool_destroy(pool);
    // No explicit assertions here, just ensuring no crash
    printf("test_memory_pool_destroy passed\n");
}

int main(void) {
    test_memory_pool_create();
    test_memory_pool_alloc();
    test_memory_pool_destroy();
    printf("All tests passed\n");
    return 0;
}