#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/array.h"

DARRAY_DEFINE(intarray, int)
DARRAY_DEFINE(strarray, char*)

// Comparison function for integers
int int_cmp(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

// Comparison function for strings
int str_qsort_cmp(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

void test_init() {
    intarray* arr;
    arr = intarray_new();
    assert(arr != NULL);
    assert(arr->items != NULL);
    assert(arr->count == 0);
    assert(arr->capacity == DARRAY_INIT_CAPACITY);
    intarray_free(arr);
    printf("test_init passed\n");
}

void test_with_capacity() {
    intarray* arr;
    arr = intarray_with_capacity(100);
    assert(arr != NULL);
    assert(arr->items != NULL);
    assert(arr->count == 0);
    assert(arr->capacity == 100);
    intarray_free(arr);
    printf("test_with_capacity passed\n");
}

void test_append() {
    intarray* arr;
    arr = intarray_new();
    for (int i = 0; i < 100; i++) {
        assert(intarray_push(arr, i) == true);
    }
    assert(arr->count == 100);
    assert(arr->capacity >= 100);
    for (int i = 0; i < 100; i++) {
        assert(arr->items[i] == i);
    }
    intarray_free(arr);
    printf("test_append passed\n");
}

void test_get() {
    intarray* arr;
    arr = intarray_new();
    for (int i = 0; i < 10; i++) {
        intarray_push(arr, i * 10);
    }

    // Test get by index
    assert(intarray_get(arr, 5) == 50);

    // Test get pointer
    int* ptr = intarray_get_ptr(arr, 3);
    assert(ptr != NULL);
    assert(*ptr == 30);
    *ptr = 99;
    assert(arr->items[3] == 99);

    // Test get const pointer
    const intarray* carr = arr;
    const int* cptr      = intarray_get_const_ptr(carr, 7);
    assert(cptr != NULL);
    assert(*cptr == 70);

    intarray_free(arr);
    printf("test_get passed\n");
}

void test_set() {
    intarray* arr;
    arr = intarray_new();
    for (int i = 0; i < 10; i++) {
        intarray_push(arr, i);
    }
    assert(intarray_set(arr, 5, 50) == true);
    assert(arr->items[5] == 50);

    // Test out of bounds
    assert(intarray_set(arr, 15, 100) == false);

    intarray_free(arr);
    printf("test_set passed\n");
}

void test_insert() {
    intarray* arr;
    arr = intarray_new();
    for (int i = 0; i < 5; i++) {
        intarray_push(arr, i);
    }
    assert(intarray_insert(arr, 2, 100) == true);
    assert(arr->count == 6);
    assert(arr->items[2] == 100);
    assert(arr->items[3] == 2);

    // Test insert at end
    assert(intarray_insert(arr, arr->count, 200) == true);
    assert(arr->items[6] == 200);

    // Test out of bounds
    assert(intarray_insert(arr, 10, 300) == false);

    intarray_free(arr);
    printf("test_insert passed\n");
}

void test_remove() {
    intarray* arr;
    arr = intarray_new();
    for (int i = 0; i < 5; i++) {
        intarray_push(arr, i);
    }
    assert(intarray_remove(arr, 2) == true);
    assert(arr->count == 4);
    assert(arr->items[2] == 3);

    // Test out of bounds
    assert(intarray_remove(arr, 10) == false);

    intarray_free(arr);
    printf("test_remove passed\n");
}

void test_pop() {
    intarray* arr;
    arr = intarray_new();
    for (int i = 0; i < 5; i++) {
        intarray_push(arr, i);
    }

    int value;
    assert(intarray_pop(arr, &value) == true);
    assert(value == 4);
    assert(arr->count == 4);

    // Test pop without storing value
    assert(intarray_pop(arr, NULL) == true);
    assert(arr->count == 3);

    // Pop until empty
    while (intarray_pop(arr, NULL)) {}
    assert(arr->count == 0);

    // Test pop from empty array
    assert(intarray_pop(arr, &value) == false);

    intarray_free(arr);
    printf("test_pop passed\n");
}

void test_clear() {
    intarray* arr;
    arr = intarray_new();
    for (int i = 0; i < 5; i++) {
        intarray_push(arr, i);
    }

    intarray_clear(arr);
    assert(arr->count == 0);
    assert(arr->capacity > 0);  // Capacity should remain unchanged

    intarray_free(arr);
    printf("test_clear passed\n");
}

void test_copy() {
    intarray *arr1, *arr2;
    arr1 = intarray_new();
    for (int i = 0; i < 5; i++) {
        intarray_push(arr1, i);
    }

    arr2 = intarray_copy(arr1);
    assert(arr2 != NULL);
    assert(arr2->count == arr1->count);
    assert(arr2->capacity == arr1->capacity);

    for (size_t i = 0; i < arr1->count; i++) {
        assert(arr2->items[i] == arr1->items[i]);
    }

    // Modify original and ensure copy is unchanged
    intarray_set(arr1, 0, 99);
    assert(arr2->items[0] == 0);  // Should still be original value

    intarray_free(arr1);
    intarray_free(arr2);
    printf("test_copy passed\n");
}

void test_swap() {
    intarray *arr1, *arr2;

    arr1 = intarray_new();
    arr2 = intarray_new();

    for (int i = 0; i < 5; i++) {
        intarray_push(arr1, i);
    }

    for (int i = 5; i < 10; i++) {
        intarray_push(arr2, i);
    }

    size_t count1 = arr1->count;
    size_t count2 = arr2->count;
    size_t cap1   = arr1->capacity;
    size_t cap2   = arr2->capacity;

    intarray_swap(arr1, arr2);

    assert(arr1->count == count2);
    assert(arr2->count == count1);
    assert(arr1->capacity == cap2);
    assert(arr2->capacity == cap1);
    assert(arr1->items[0] == 5);
    assert(arr2->items[0] == 0);

    intarray_free(arr1);
    intarray_free(arr2);

    printf("test_swap passed\n");
}

void test_reverse() {
    intarray* arr;
    arr = intarray_new();

    // Test empty array
    intarray_reverse(arr);

    // Test single element
    intarray_push(arr, 42);
    intarray_reverse(arr);
    assert(arr->items[0] == 42);

    // Clear array and test multiple elements
    intarray_clear(arr);
    for (int i = 0; i < 5; i++) {  // Start from 0, not 1
        intarray_push(arr, i);
    }
    intarray_reverse(arr);
    for (int i = 0; i < 5; i++) {
        assert(arr->items[i] == 4 - i);
    }

    intarray_free(arr);
    printf("test_reverse passed\n");
}

void test_sort() {
    intarray* arr;
    arr = intarray_new();

    // Test empty array
    assert(intarray_sort(arr, int_cmp) == true);

    // Test single element
    intarray_push(arr, 42);
    assert(intarray_sort(arr, int_cmp) == true);
    assert(arr->items[0] == 42);

    // Test multiple elements
    intarray_push(arr, 3);
    intarray_push(arr, 1);
    intarray_push(arr, 4);
    intarray_push(arr, 1);
    intarray_push(arr, 5);
    assert(intarray_sort(arr, int_cmp) == true);

    for (size_t i = 1; i < arr->count; i++) {
        assert(arr->items[i - 1] <= arr->items[i]);
    }

    intarray_free(arr);
    printf("test_sort passed\n");
}

void test_resize() {
    intarray* arr;
    arr = intarray_new();

    // Add some elements
    for (int i = 0; i < 10; i++) {
        intarray_push(arr, i);
    }

    size_t original_count = arr->count;

    // Resize to larger capacity
    assert(intarray_resize(arr, 50) == true);
    assert(arr->capacity == 50);
    assert(arr->count == original_count);  // Count should remain the same

    // Verify elements are preserved
    for (int i = 0; i < 10; i++) {
        assert(arr->items[i] == i);
    }

    // Resize to smaller capacity
    assert(intarray_resize(arr, 5) == true);
    assert(arr->capacity == 5);
    assert(arr->count == 5);  // Count should be truncated

    // Verify first 5 elements are preserved
    for (int i = 0; i < 5; i++) {
        assert(arr->items[i] == i);
    }

    intarray_free(arr);
    printf("test_resize passed\n");
}

void test_reserve() {
    intarray* arr;
    arr = intarray_new();

    // Reserve more capacity
    assert(intarray_reserve(arr, 100) == true);
    assert(arr->capacity >= 100);
    assert(arr->count == 0);  // Count should remain unchanged

    // Add elements to verify the reserved space works
    for (int i = 0; i < 100; i++) {
        assert(intarray_push(arr, i) == true);
    }
    assert(arr->count == 100);

    intarray_free(arr);
    printf("test_reserve passed\n");
}

void test_shrink_to_fit() {
    intarray* arr;
    arr = intarray_new();

    // Add many elements
    for (int i = 0; i < 100; i++) {
        intarray_push(arr, i);
    }

    size_t old_capacity = arr->capacity;

    // Remove most elements
    for (int i = 0; i < 90; i++) {
        intarray_remove(arr, 0);
    }

    assert(arr->count == 10);
    assert(arr->capacity == old_capacity);  // Capacity should still be large

    // Shrink to fit
    assert(intarray_shrink_to_fit(arr) == true);
    assert(arr->capacity == arr->count);
    assert(arr->capacity == 10);

    // Verify elements are preserved
    for (int i = 0; i < 10; i++) {
        assert(arr->items[i] == 90 + i);
    }

    intarray_free(arr);
    printf("test_shrink_to_fit passed\n");
}

void test_find() {
    intarray* arr;
    arr = intarray_new();

    for (int i = 0; i < 10; i++) {
        intarray_push(arr, i * 2);  // Even numbers: 0, 2, 4, ..., 18
    }

    // Test finding existing elements
    int target1     = 6;
    ptrdiff_t index = intarray_find(arr, &target1, int_cmp);
    assert(index == 3);

    int target2 = 0;
    index       = intarray_find(arr, &target2, int_cmp);
    assert(index == 0);

    int target3 = 18;
    index       = intarray_find(arr, &target3, int_cmp);
    assert(index == 9);

    // Test finding non-existing elements
    int target4 = 7;
    index       = intarray_find(arr, &target4, int_cmp);
    assert(index == -1);

    // Test with NULL comparison function (should use memcmp)
    int target5 = 8;
    index       = intarray_find(arr, &target5, NULL);
    assert(index == 4);

    intarray_free(arr);
    printf("test_find passed\n");
}

void test_string_array() {
    strarray* arr;
    arr = strarray_new();

    char* str1 = strdup("Hello");
    char* str2 = strdup("World");
    char* str3 = strdup("Test");

    assert(strarray_push(arr, str1) == true);
    assert(strarray_push(arr, str2) == true);
    assert(strarray_push(arr, str3) == true);

    assert(arr->count == 3);
    assert(strcmp(arr->items[0], "Hello") == 0);
    assert(strcmp(arr->items[1], "World") == 0);
    assert(strcmp(arr->items[2], "Test") == 0);

    // Test string sorting
    assert(strarray_sort(arr, str_qsort_cmp) == true);
    assert(strcmp(arr->items[0], "Hello") == 0);
    assert(strcmp(arr->items[1], "Test") == 0);
    assert(strcmp(arr->items[2], "World") == 0);

    // Test string finding
    const char* target = "Test";
    ptrdiff_t index    = strarray_find(arr, &target, str_qsort_cmp);
    assert(index == 1);

    // Clean up
    for (size_t i = 0; i < arr->count; i++) {
        free(arr->items[i]);
    }
    strarray_free(arr);
    printf("test_string_array passed\n");
}

void test_utility_functions() {
    intarray* arr;
    arr = intarray_new();

    assert(intarray_empty(arr) == true);
    assert(intarray_size(arr) == 0);
    assert(intarray_capacity(arr) == DARRAY_INIT_CAPACITY);
    assert(intarray_data(arr) == arr->items);
    assert(intarray_const_data(arr) == arr->items);

    for (int i = 0; i < 5; i++) {
        intarray_push(arr, i);
    }

    assert(intarray_empty(arr) == false);
    assert(intarray_size(arr) == 5);
    assert(intarray_capacity(arr) >= 5);
    assert(intarray_data(arr) == arr->items);
    assert(intarray_const_data(arr) == arr->items);

    intarray_free(arr);
    printf("test_utility_functions passed\n");
}

void test_null_safety() {
    intarray* arr = NULL;

    // All these should handle NULL gracefully
    assert(intarray_is_valid(arr) == false);
    assert(intarray_resize(arr, 10) == false);
    assert(intarray_reserve(arr, 10) == false);
    assert(intarray_shrink_to_fit(arr) == false);
    assert(intarray_push(arr, 42) == false);
    assert(intarray_get(arr, 0) == 0);
    assert(intarray_get_ptr(arr, 0) == NULL);
    assert(intarray_get_const_ptr(arr, 0) == NULL);
    assert(intarray_set(arr, 0, 42) == false);
    assert(intarray_insert(arr, 0, 42) == false);
    assert(intarray_remove(arr, 0) == false);
    assert(intarray_pop(arr, NULL) == false);
    intarray_clear(arr);  // Should not crash
    intarray_free(arr);   // Should not crash
    assert(intarray_copy(arr) == NULL);
    intarray_swap(arr, arr);  // Should not crash
    intarray_reverse(arr);    // Should not crash
    assert(intarray_sort(arr, int_cmp) == false);
    assert(intarray_size(arr) == 0);
    assert(intarray_capacity(arr) == 0);
    assert(intarray_empty(arr) == true);
    assert(intarray_data(arr) == NULL);
    assert(intarray_const_data(arr) == NULL);

    int target = 42;
    assert(intarray_find(arr, &target, int_cmp) == -1);

    printf("test_null_safety passed\n");
}

int main() {
    test_init();
    test_with_capacity();
    test_append();
    test_get();
    test_set();
    test_insert();
    test_remove();
    test_pop();
    test_clear();
    test_copy();
    test_swap();
    test_reverse();
    test_sort();
    test_resize();
    test_reserve();
    test_shrink_to_fit();
    test_find();
    test_string_array();
    test_utility_functions();
    test_null_safety();

    printf("All tests passed!\n");
    return 0;
}
