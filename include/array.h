#ifndef DARRAY_H
#define DARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_INIT_CAPACITY 16

#define ARRAY_DEFINE(name, type)                                                                   \
    typedef struct {                                                                               \
        type* items;                                                                               \
        size_t count;                                                                              \
        size_t capacity;                                                                           \
    }(name);                                                                                       \
                                                                                                   \
    void name##_init(name* arr) {                                                                  \
        arr->items    = NULL;                                                                      \
        arr->count    = 0;                                                                         \
        arr->capacity = 0;                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_resize(name* arr, size_t new_capacity) {                                           \
        if (new_capacity < arr->count) {                                                           \
            arr->count = new_capacity;                                                             \
        }                                                                                          \
                                                                                                   \
        type* new_items = realloc(arr->items, new_capacity * sizeof(*arr->items));                 \
        if (new_items == NULL && new_capacity > 0) {                                               \
            perror("realloc");                                                                     \
            free(arr->items);                                                                      \
            exit(1);                                                                               \
        }                                                                                          \
                                                                                                   \
        arr->items    = new_items;                                                                 \
        arr->capacity = new_capacity;                                                              \
    }                                                                                              \
                                                                                                   \
    void name##vec_shrink(name* arr) {                                                             \
        if (arr->capacity > arr->count) {                                                          \
            name##_resize(arr, arr->count);                                                        \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void name##_append(name* arr, type x) {                                                        \
        if (arr->count >= arr->capacity) {                                                         \
            size_t new_capacity = arr->capacity == 0 ? ARRAY_INIT_CAPACITY : arr->capacity * 2;    \
            name##_resize(arr, new_capacity);                                                      \
        }                                                                                          \
        arr->items[arr->count++] = x;                                                              \
    }                                                                                              \
                                                                                                   \
    void name##_set(name* arr, size_t index, type value) {                                         \
        if (index >= arr->count) {                                                                 \
            fprintf(stderr, "Index out of bounds\n");                                              \
            exit(1);                                                                               \
        }                                                                                          \
        arr->items[index] = value;                                                                 \
    }                                                                                              \
                                                                                                   \
    void name##_insert(name* arr, size_t index, type value) {                                      \
        if (index > arr->count) {                                                                  \
            fprintf(stderr, "Index out of bounds\n");                                              \
            return;                                                                                \
        }                                                                                          \
        if (arr->count >= arr->capacity) {                                                         \
            size_t new_capacity = arr->capacity == 0 ? 1 : arr->capacity * 2;                      \
            name##_resize(arr, new_capacity);                                                      \
        }                                                                                          \
        memmove(&arr->items[index + 1], &arr->items[index], (arr->count - index) * sizeof(type));  \
        arr->items[index] = value;                                                                 \
        arr->count++;                                                                              \
    }                                                                                              \
    void name##_remove(name* arr, size_t index) {                                                  \
        if (index >= arr->count) {                                                                 \
            fprintf(stderr, "Index out of bounds\n");                                              \
            return;                                                                                \
        }                                                                                          \
        memmove(&arr->items[index], &arr->items[index + 1],                                        \
                (arr->count - index - 1) * sizeof(type));                                          \
        arr->count--;                                                                              \
    }                                                                                              \
                                                                                                   \
    void name##_clear(name* arr) {                                                                 \
        memset(arr->items, 0, arr->count * sizeof(*arr->items));                                   \
        arr->count = 0;                                                                            \
    }                                                                                              \
                                                                                                   \
    void name##_free(name* arr) {                                                                  \
        if (arr->items) {                                                                          \
            free(arr->items);                                                                      \
            arr->items = NULL;                                                                     \
        }                                                                                          \
        arr->count    = 0;                                                                         \
        arr->capacity = 0;                                                                         \
    }                                                                                              \
                                                                                                   \
    void name##_copy(name* dest, name* src) {                                                      \
        name##_resize(dest, src->capacity);                                                        \
        memcpy(dest->items, src->items, src->count * sizeof(*src->items));                         \
        dest->count = src->count;                                                                  \
    }                                                                                              \
    void name##_swap(name* arr1, name* arr2) {                                                     \
        name temp = *arr1;                                                                         \
        *arr1     = *arr2;                                                                         \
        *arr2     = temp;                                                                          \
    }                                                                                              \
    void name##_reverse(name* arr) {                                                               \
        for (size_t i = 0; i < arr->count / 2; ++i) {                                              \
            type temp                      = arr->items[i];                                        \
            arr->items[i]                  = arr->items[arr->count - i - 1];                       \
            arr->items[arr->count - i - 1] = temp;                                                 \
        }                                                                                          \
    }                                                                                              \
    void name##_sort(name* arr, int (*cmp)(const void*, const void*)) {                            \
        qsort(arr->items, arr->count, sizeof(*arr->items), cmp);                                   \
    }

ARRAY_DEFINE(IntArray, int)
ARRAY_DEFINE(StrArray, char*)
ARRAY_DEFINE(FloatArray, float)
ARRAY_DEFINE(DoubleArray, double)

#ifdef __cplusplus
}
#endif

#endif

#if 0
int int_cmp(const int* a, const int* b) {
    return *a - *b;
}

int main(void) {
    IntArray xs;
    IntArray_init(&xs);

    for (int i = 0; i < 1000; ++i) {
        IntArray_append(&xs, i);
    }

    printf("First element: %d\n", xs.items[0]);
    printf("Last element: %d\n", xs.items[xs.count - 1]);

    IntArray_set(&xs, 500, -1);
    printf("Element at index 500: %d\n", xs.items[500]);

    IntArray_remove(&xs, 500);
    printf("New element at index 500: %d\n", xs.items[500]);
    printf("Array size: %ld\n", xs.count);

    IntArray_insert(&xs, 10, 1000);
    printf("Element at index 10: %d\n", xs.items[10]);

    // IntArray_copy(&ys, &xs);
    IntArray_reverse(&xs);
    printf("First element after reverse: %d\n", xs.items[0]);
    printf("Last element after reverse: %d\n", xs.items[xs.count - 1]);

    IntArray_sort(&xs, (int (*)(const void*, const void*)) & int_cmp);
    printf("First element after sort: %d\n", xs.items[0]);

    // copy
    IntArray ordered;
    IntArray_init(&ordered);
    IntArray_copy(&ordered, &xs);
    printf("First element of ordered: %d\n", ordered.items[0]);
    printf("Last element of ordered: %d\n", ordered.items[ordered.count - 1]);

    IntArray_swap(&xs, &ordered);
    printf("First element after swap: %d\n", xs.items[0]);
    printf("First element of ordered after swap: %d\n", ordered.items[0]);

    IntArray_clear(&xs);
    printf("Array size after clear: %ld\n", xs.count);

    IntArray_free(&xs);

    StrArray ys;
    StrArray_init(&ys);

    for (int i = 0; i < 5; ++i) {
        char* name = malloc(32);
        snprintf(name, 32, "name_%d", i);
        StrArray_append(&ys, name);
    }

    for (size_t i = 0; i < ys.count; ++i) {
        printf("%s\n", ys.items[i]);
    }

    for (size_t i = 0; i < ys.count; ++i) {
        free(ys.items[i]);
    }
    StrArray_free(&ys);

    return 0;
}

#endif
