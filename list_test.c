#define LIST_IMPLEMENTATION
#include "list.h"

#include <stdio.h>

int main(void) {
    list_t* list = list_new(sizeof(int));
    int a        = 1;
    int b        = 2;
    int c        = 3;
    int d        = 4;
    int e        = 5;
    list_push_back(list, &a);
    list_push_back(list, &b);
    list_push_front(list, &c);
    list_push_front(list, &d);
    list_insert(list, 1, &e);
    list_insert_after(list, &e, &a);
    list_insert_before(list, &e, &b);
    list_remove(list, &e);
    list_remove(list, &b);

    // list_insert(list, 0, &e);
    printf("Size of list: %zu\n", list_size(list));
    printf("Index of 3: %zu\n", list_index_of(list, &d));

    for (size_t i = 0; i < list_size(list); i++) {
        int* val = list_get(list, i);
        printf("Value at index %zu: %d\n", i, *val);
    }
    list_free(list);
}