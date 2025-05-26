#include <stdio.h>
#include "../include/cstr_array.h"

int main() {
    cstr_array* arr = cstr_array_new(0);
    cstr_array_push_str(arr, "Hello");
    cstr_array_push_str(arr, "World");
    cstr_array_push_str(arr, "My");
    cstr_array_push_str(arr, "Dear");
    cstr_array_push_str(arr, "Friends");
    cstr_array_push_str(arr, "Friendsjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");

    for (size_t i = 0; i < arr->len; ++i) {
        printf("[%zu] %s\n", i, str_data(cstr_array_get(arr, i)));
    }

    cstr_array_remove(arr, 2);
    cstr_array_remove(arr, 4);

    printf("After removal:\n");
    for (size_t i = 0; i < arr->len; ++i) {
        printf("[%zu] %s\n", i, str_data(cstr_array_get(arr, i)));
    }

    cstr_array_free(arr);
    return 0;
}
