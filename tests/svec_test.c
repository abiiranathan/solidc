#include <assert.h>
#include <stdio.h>
#include "svec_types.h"

#define LOAD 10000000

void test_vec_under_load(void) {
    time_t start, end;
    start = time(NULL);

    intvec_t* vec = intvec_create(LOAD);
    assert(vec);

    for (int i = 0; i < LOAD; i++) {
        intvec_push_back(vec, i);
    }

    for (int i = 0; i < LOAD; i++) {
        int n = intvec_at(vec, i);
        assert(n == i);
    }

    intvec_destroy(vec);
    end = time(NULL);
    printf("Time taken to insert and fetch %d items: %ld ms\n", LOAD, (end - start) * 1000);
}

void test_float_vector(void) {
    floatvec_t* vec = floatvec_create(10);
    assert(vec);

    for (int i = 0; i < 10; i++) {
        floatvec_push_back(vec, i * 2.5);
    }

    assert(vec->size == 10);

    for (int i = 0; i < 10; i++) {
        assert(floatvec_at(vec, i) == i * 2.5);
    }

    floatvec_destroy(vec);
}

int main(void) {
    intvec_t* vec = intvec_create(10);
    assert(vec);

    // Test cases
    // Create
    assert(vec != NULL);
    assert(intvec_size(vec) == 0);
    assert(intvec_capacity(vec) == 10);

    // Push_back
    intvec_push_back(vec, 5);
    assert(intvec_size(vec) == 1);
    assert(intvec_at(vec, 0) == 5);

    intvec_push_back(vec, 10);
    assert(intvec_size(vec) == 2);
    assert(intvec_at(vec, 1) == 10);

    // Resize
    intvec_resize(vec, 20);
    assert(intvec_capacity(vec) == 20);

    // Reserve
    intvec_reserve(vec, 30);
    assert(intvec_capacity(vec) == 30);

    // Erase
    intvec_erase(vec, 0);
    assert(intvec_size(vec) == 1);
    assert(intvec_at(vec, 0) == 10);

    // Pop_back
    intvec_pop_back(vec);
    assert(intvec_size(vec) == 0);

    // Front, Back, At
    intvec_push_back(vec, 15);
    intvec_push_back(vec, 20);
    assert(intvec_front(vec) == 15);
    assert(intvec_back(vec) == 20);
    assert(intvec_at(vec, 1) == 20);

    // Empty
    assert(!intvec_empty(vec));

    intvec_pop_back(vec);
    intvec_pop_back(vec);

    assert(intvec_empty(vec));

    intvec_destroy(vec);

    test_vec_under_load();
    test_float_vector();
    printf("All svec tests passed\n");
    return 0;
}
