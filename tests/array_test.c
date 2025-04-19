#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/array.h"

ARRAY_DEFINE(IntArray, int)
ARRAY_DEFINE(StrArray, char*)

void test_init() {
    IntArray arr;
    IntArray_init(&arr);
    assert(arr.items == NULL);
    assert(arr.count == 0);
    assert(arr.capacity == 0);
    printf("test_init passed\n");
}

void test_append() {
    IntArray arr;
    IntArray_init(&arr);
    for (int i = 0; i < 100; i++) {
        IntArray_append(&arr, i);
    }
    assert(arr.count == 100);
    assert(arr.capacity >= 100);
    for (int i = 0; i < 100; i++) {
        assert(arr.items[i] == i);
    }
    IntArray_free(&arr);
    printf("test_append passed\n");
}

void test_set() {
    IntArray arr;
    IntArray_init(&arr);
    for (int i = 0; i < 10; i++) {
        IntArray_append(&arr, i);
    }
    IntArray_set(&arr, 5, 50);
    assert(arr.items[5] == 50);
    IntArray_free(&arr);
    printf("test_set passed\n");
}

void test_insert() {
    IntArray arr;
    IntArray_init(&arr);
    for (int i = 0; i < 5; i++) {
        IntArray_append(&arr, i);
    }
    IntArray_insert(&arr, 2, 100);
    assert(arr.count == 6);
    assert(arr.items[2] == 100);
    assert(arr.items[3] == 2);
    IntArray_free(&arr);
    printf("test_insert passed\n");
}

void test_remove() {
    IntArray arr;
    IntArray_init(&arr);
    for (int i = 0; i < 5; i++) {
        IntArray_append(&arr, i);
    }
    IntArray_remove(&arr, 2);
    assert(arr.count == 4);
    assert(arr.items[2] == 3);
    IntArray_free(&arr);
    printf("test_remove passed\n");
}

void test_clear() {
    IntArray arr;
    IntArray_init(&arr);
    for (int i = 0; i < 5; i++) {
        IntArray_append(&arr, i);
    }

    IntArray_clear(&arr);
    assert(arr.count == 0);
    IntArray_free(&arr);
    printf("test_clear passed\n");
}

void test_copy() {
    IntArray arr1, arr2;
    IntArray_init(&arr1);
    IntArray_init(&arr2);
    for (int i = 0; i < 5; i++) {
        IntArray_append(&arr1, i);
    }
    IntArray_copy(&arr2, &arr1);
    assert(arr2.count == arr1.count);
    for (size_t i = 0; i < arr1.count; i++) {
        assert(arr2.items[i] == arr1.items[i]);
    }
    IntArray_free(&arr1);
    IntArray_free(&arr2);
    printf("test_copy passed\n");
}

void test_swap() {
    IntArray arr1, arr2;
    IntArray_init(&arr1);
    IntArray_init(&arr2);
    for (int i = 0; i < 5; i++) {
        IntArray_append(&arr1, i);
    }
    for (int i = 5; i < 10; i++) {
        IntArray_append(&arr2, i);
    }
    IntArray_swap(&arr1, &arr2);
    assert(arr1.count == 5);
    assert(arr2.count == 5);
    assert(arr1.items[0] == 5);
    assert(arr2.items[0] == 0);
    IntArray_free(&arr1);
    IntArray_free(&arr2);
    printf("test_swap passed\n");
}

void test_reverse() {
    IntArray arr;
    IntArray_init(&arr);
    for (int i = 0; i < 5; i++) {
        IntArray_append(&arr, i);
    }
    IntArray_reverse(&arr);
    for (int i = 0; i < 5; i++) {
        assert(arr.items[i] == 4 - i);
    }
    IntArray_free(&arr);
    printf("test_reverse passed\n");
}

int int_cmp(const int* a, const int* b) {
    return (*(int*)a - *(int*)b);
}

void test_sort() {
    IntArray arr;
    IntArray_init(&arr);
    IntArray_append(&arr, 3);
    IntArray_append(&arr, 1);
    IntArray_append(&arr, 4);
    IntArray_append(&arr, 1);
    IntArray_append(&arr, 5);
    IntArray_sort(&arr, int_cmp);
    for (size_t i = 1; i < arr.count; i++) {
        assert(arr.items[i - 1] <= arr.items[i]);
    }
    IntArray_free(&arr);
    printf("test_sort passed\n");
}

void test_shrink() {
    IntArray arr;
    IntArray_init(&arr);
    for (int i = 0; i < 100; i++) {
        IntArray_append(&arr, i);
    }
    size_t old_capacity = arr.capacity;
    for (int i = 0; i < 50; i++) {
        IntArray_remove(&arr, 0);
    }
    IntArrayvec_shrink(&arr);
    assert(arr.capacity == arr.count);
    assert(arr.capacity < old_capacity);
    IntArray_free(&arr);
    printf("test_shrink passed\n");
}

void test_string_array() {
    StrArray arr;
    StrArray_init(&arr);
    char* str1 = strdup("Hello");
    char* str2 = strdup("World");
    StrArray_append(&arr, str1);
    StrArray_append(&arr, str2);
    assert(strcmp(arr.items[0], "Hello") == 0);
    assert(strcmp(arr.items[1], "World") == 0);
    for (size_t i = 0; i < arr.count; i++) {
        free(arr.items[i]);
    }
    StrArray_free(&arr);
    printf("test_string_array passed\n");
}

int main() {
    test_init();
    test_append();
    test_set();
    test_insert();
    test_remove();
    test_clear();
    test_copy();
    test_swap();
    test_reverse();
    test_sort();
    test_shrink();
    test_string_array();
    printf("All tests passed!\n");
    return 0;
}
