#include "../include/list.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to print list of ints
static void print_list(list_t* list) {
    printf("[");
    for (size_t i = 0; i < list_size(list); i++) {
        int* val = (int*)list_get(list, i);
        if (val) {
            printf("%d", *val);
            if (i < list_size(list) - 1) printf(", ");
        }
    }
    printf("]\n");
}

int main(void) {
    printf("Running list tests...\n");

    list_t* list = list_new(sizeof(int));
    assert(list != NULL);

    // Push back
    for (int i = 1; i <= 5; i++) {
        list_push_back(list, &i);
    }
    printf("After push_back 1..5: ");
    print_list(list);
    assert(list_size(list) == 5);

    // Push front
    int zero = 0;
    list_push_front(list, &zero);
    printf("After push_front 0: ");
    print_list(list);
    assert(*(int*)list_get(list, 0) == 0);

    // Pop back
    list_pop_back(list);
    printf("After pop_back: ");
    print_list(list);
    assert(list_size(list) == 5);

    // Pop front
    list_pop_front(list);
    printf("After pop_front: ");
    print_list(list);
    assert(*(int*)list_get(list, 0) == 1);

    // Insert in middle
    int x = 99;
    list_insert(list, 2, &x);  // insert at index 2
    printf("After insert 99 at index 2: ");
    print_list(list);

    // Insert before element
    int before = 99, valA = 77;
    list_insert_before(list, &valA, &before);
    printf("After insert_before 99 -> 77: ");
    print_list(list);

    // Insert after element
    int after = 77, valB = 88;
    list_insert_after(list, &valB, &after);
    printf("After insert_after 77 -> 88: ");
    print_list(list);

    // Remove element
    int remove_val = 99;
    list_remove(list, &remove_val);
    printf("After remove 99: ");
    print_list(list);

    // Index lookup
    int target = 88;
    int idx    = list_index_of(list, &target);
    printf("Index of 88: %d\n", idx);
    assert(idx != -1);

    // Clear
    list_clear(list);
    printf("After clear, size = %zu\n", list_size(list));
    assert(list_size(list) == 0);

    // Free
    list_free(list);

    printf("All tests passed!\n");
    return 0;
}
