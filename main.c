// #include <omp.h>
#include <stdio.h>
#include "include/larena.h"

#define COUNT 10000

int main(void) {
    int* arr[COUNT];
    LArena* a = larena_create(1 << 20);
    int sum   = 0;

    // Parallel allocation
    // #pragma omp parallel for
    for (int i = 0; i < COUNT; i++) {
        arr[i] = larena_alloc(a, sizeof(int));
        if (arr[i] == NULL) {
            printf("Allocation failed at i=%d\n", i);
            exit(1);
        }
        *arr[i] = i;
    }

    // Parallel sum with reduction
    // #pragma omp parallel for reduction(+ : sum)
    for (int i = 0; i < COUNT; i++) {
        if (arr[i] != NULL) {  // Safety check
            sum += *arr[i];
        }
    }

    printf("Sum of all numbers: %d\n", sum);

    for (int i = 0; i < 100; i++) {
        int* ptr = larena_alloc(a, sizeof(int));
        if (ptr) {
            *ptr = i;
            printf("Int: %d\n", *ptr);
        }
    }

    // Clean up
    larena_destroy(a);
    return 0;
}
