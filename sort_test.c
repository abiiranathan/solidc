#include "sort.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void testSortingAlgorithms() {
    // Test bubbleSort
    {
        int arr[] = {5, 2, 7, 1, 9};
        int expected[] = {1, 2, 5, 7, 9};
        int size = sizeof(arr) / sizeof(arr[0]);

        bubbleSort(arr, size);

        for (int i = 0; i < size; i++) {
            assert(arr[i] == expected[i]);
        }
    }

    // Test selectionSort
    {
        int arr[] = {5, 2, 7, 1, 9};
        int expected[] = {1, 2, 5, 7, 9};
        int size = sizeof(arr) / sizeof(arr[0]);

        selectionSort(arr, size);

        for (int i = 0; i < size; i++) {
            assert(arr[i] == expected[i]);
        }
    }

    // Test insertionSort
    {
        int arr[] = {5, 2, 7, 1, 9};
        int expected[] = {1, 2, 5, 7, 9};
        int size = sizeof(arr) / sizeof(arr[0]);

        insertionSort(arr, size);

        for (int i = 0; i < size; i++) {
            assert(arr[i] == expected[i]);
        }
    }

    // Test mergeSort
    {
        int arr[] = {5, 2, 7, 1, 9};
        int expected[] = {1, 2, 5, 7, 9};
        int size = sizeof(arr) / sizeof(arr[0]);

        mergeSort(arr, size);

        for (int i = 0; i < size; i++) {
            assert(arr[i] == expected[i]);
        }
    }

    // Test radixSort
    {
        int arr[] = {170, 45, 75, 90, 802, 24, 2, 66};
        int expected[] = {2, 24, 45, 66, 75, 90, 170, 802};
        int size = sizeof(arr) / sizeof(arr[0]);

        radixSort(arr, size);

        for (int i = 0; i < size; i++) {
            assert(arr[i] == expected[i]);
        }
    }

    // Test mergeSortStr
    {
        char* arr[] = {"banana", "apple", "cherry", "date"};
        char* expected[] = {"apple", "banana", "cherry", "date"};
        int size = sizeof(arr) / sizeof(arr[0]);

        mergeSortStr(arr, 0, size - 1);

        for (int i = 0; i < size; i++) {
            assert(strcmp(arr[i], expected[i]) == 0);
        }
    }
    printf("All sorting algorithm tests passed!\n");
}

int main() {
    testSortingAlgorithms();
    return 0;
}
