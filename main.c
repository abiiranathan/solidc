#include <omp.h>
#include <stdio.h>
#include "include/arena.h"

#define COUNT 10000

int main(void) {
    int* arr[COUNT];
    Arena* a = arena_create(1 << 20);
    int sum  = 0;

    // Parallel allocation
#pragma omp parallel for
    for (int i = 0; i < COUNT; i++) {
        arr[i] = arena_alloc_int(a, i);
        if (arr[i] == NULL) {
            printf("Allocation failed at i=%d\n", i);
            exit(1);
        }
    }

    // Parallel sum with reduction
#pragma omp parallel for reduction(+ : sum)
    for (int i = 0; i < COUNT; i++) {
        if (arr[i] != NULL) {  // Safety check
            sum += *arr[i];
        }
    }

    printf("Sum of all numbers: %d\n", sum);

    // Clean up
    arena_destroy(a);
    return 0;
}
