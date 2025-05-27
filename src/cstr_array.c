#include "../include/cstr_array.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 8

cstr_array* cstr_array_new(size_t initial_cap) {
    if (initial_cap == 0)
        initial_cap = INITIAL_CAPACITY;

    cstr_array* arr = malloc(sizeof(cstr_array));
    if (!arr)
        return NULL;

    arr->data = calloc(initial_cap, sizeof(cstr*));
    if (!arr->data) {
        free(arr);
        return NULL;
    }

    arr->len = 0;
    arr->cap = initial_cap;
    return arr;
}

void cstr_array_free(cstr_array* arr) {
    if (!arr)
        return;
    for (size_t i = 0; i < arr->len; ++i) {
        cstr_free(arr->data[i]);
    }
    free(arr->data);
    free(arr);
}

static int cstr_array_grow(cstr_array* arr) {
    size_t new_cap  = arr->cap * 2;
    cstr** new_data = realloc(arr->data, new_cap * sizeof(cstr*));
    if (!new_data)
        return -1;

    arr->data = new_data;
    arr->cap  = new_cap;
    return 0;
}

int cstr_array_push(cstr_array* arr, cstr* s) {
    if (!arr || !s)
        return -1;
    if (arr->len == arr->cap && cstr_array_grow(arr) != 0)
        return -1;
    arr->data[arr->len++] = s;
    return 0;
}

int cstr_array_push_str(cstr_array* arr, const char* str) {
    return cstr_array_push(arr, cstr_new(str));
}

cstr* cstr_array_get(cstr_array* arr, size_t index) {
    if (!arr || index >= arr->len)
        return NULL;
    return arr->data[index];
}

void cstr_array_remove(cstr_array* arr, size_t index) {
    if (!arr || index >= arr->len)
        return;

    cstr_free(arr->data[index]);

    if (index < arr->len - 1) {
        memmove(&arr->data[index], &arr->data[index + 1], (arr->len - index - 1) * sizeof(cstr*));
    }

    arr->len--;
}
