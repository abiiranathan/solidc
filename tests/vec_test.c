#include "../include/vec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    vec_t* v = vec_new(100, int_cmp);
    assert(v != NULL);

    for (int i = 0; i < 10; ++i) {
        int* val = malloc(sizeof(int));
        assert(val);
        *val = i * 10;

        vec_push(v, val);
    }

    // allocate a new int
    int* k = malloc(sizeof(int));
    assert(k);
    *k = 120;

    vec_set(v, 0, k);

    for (size_t i = 0; i < vec_size(v); i++) {

        // get an element at index
        int* val = vec_get(v, i);
        assert(val);
        printf("%d ", *val);
    }
    puts("\n");

    // Remove unused capacity
    vec_shrink(v);
    assert(vec_capacity(v) == vec_size(v));

    // contains
    int* first = vec_get(v, 0);
    assert(vec_contains(v, first));
    assert(*first == *k);

    // reverse
    vec_reverse(v);

    for (size_t i = 0; i < vec_size(v); i++) {
        // get an element at index
        int* val = vec_get(v, i);
        assert(val);
        printf("%d ", *val);
    }
    puts("\n");

    // find_index
    assert(vec_find_index(v, k) == (int)vec_size(v) - 1);

    vec_free(v);
}