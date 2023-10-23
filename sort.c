#include "sort.h"

// Sorting strings
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Bubble Sort implementation
void bubbleSort(int arr[], int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

// Selection Sort implementation
void selectionSort(int arr[], int size) {
    for (int i = 0; i < size - 1; i++) {
        int minIndex = i;
        for (int j = i + 1; j < size; j++) {
            if (arr[j] < arr[minIndex]) {
                minIndex = j;
            }
        }

        int temp = arr[i];
        arr[i] = arr[minIndex];
        arr[minIndex] = temp;
    }
}

// Insertion Sort implementation
void insertionSort(int arr[], int size) {
    for (int i = 1; i < size; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// Merge two sorted subarrays into one sorted subarray
void merge(int arr[], int left[], int leftSize, int right[], int rightSize) {
    int i = 0, j = 0, k = 0;

    // Merge elements from left and right subarrays in sorted order
    while (i < leftSize && j < rightSize) {
        if (left[i] <= right[j]) {
            arr[k++] = left[i++];
        } else {
            arr[k++] = right[j++];
        }
    }

    // Copy remaining elements from left subarray, if any
    while (i < leftSize) {
        arr[k++] = left[i++];
    }

    // Copy remaining elements from right subarray, if any
    while (j < rightSize) {
        arr[k++] = right[j++];
    }
}

// Merge Sort implementation
void mergeSort(int arr[], int size) {
    // Base case: array with one or zero elements is already sorted
    if (size <= 1) {
        return;
    }

    // Divide the array into two halves
    int mid = size / 2;
    int left[mid];
    int right[size - mid];

    // Populate the left and right subarrays
    for (int i = 0; i < mid; i++) {
        left[i] = arr[i];
    }
    for (int i = mid; i < size; i++) {
        right[i - mid] = arr[i];
    }

    // Recursively sort the left and right subarrays
    mergeSort(left, mid);
    mergeSort(right, size - mid);

    // Merge the sorted subarrays
    merge(arr, left, mid, right, size - mid);
}

// Helper function to get the maximum value in the array
int getMax(int arr[], int size) {
    int max = arr[0];
    for (int i = 1; i < size; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}

// Radix Sort implementation
void radixSort(int arr[], int size) {
    int max = getMax(arr, size);

    for (int exp = 1; max / exp > 0; exp *= 10) {
        int count[10] = {0};
        int output[size];

        // Count the occurrence of each digit
        for (int i = 0; i < size; i++) {
            count[(arr[i] / exp) % 10]++;
        }

        // Calculate the cumulative count
        for (int i = 1; i < 10; i++) {
            count[i] += count[i - 1];
        }

        // Build the sorted output array
        for (int i = size - 1; i >= 0; i--) {
            output[count[(arr[i] / exp) % 10] - 1] = arr[i];
            count[(arr[i] / exp) % 10]--;
        }

        // Copy the sorted output array to the original array
        for (int i = 0; i < size; i++) {
            arr[i] = output[i];
        }
    }
}

// Merge two sorted subarrays of strings into one sorted subarray
void mergeStrings(char** arr, int leftStart, int leftEnd, int rightStart, int rightEnd) {
    int leftSize = leftEnd - leftStart + 1;
    int rightSize = rightEnd - rightStart + 1;
    char* left[leftSize];
    char* right[rightSize];

    // Copy elements to the temporary left and right subarrays
    memcpy(left, &arr[leftStart], leftSize * sizeof(char*));
    memcpy(right, &arr[rightStart], rightSize * sizeof(char*));

    int i = 0, j = 0, k = leftStart;

    // Merge elements from left and right subarrays in sorted order
    while (i < leftSize && j < rightSize) {
        int comparisonResult = strcmp(left[i], right[j]);
        if (comparisonResult <= 0) {
            arr[k++] = left[i++];
        } else {
            arr[k++] = right[j++];
        }
    }

    // Copy remaining elements from left subarray, if any
    memcpy(&arr[k], &left[i], (leftSize - i) * sizeof(char*));

    // Copy remaining elements from right subarray, if any
    memcpy(&arr[k], &right[j], (rightSize - j) * sizeof(char*));
}

// Merge Sort implementation for array of strings
void mergeSortStr(char** arr, int start, int end) {
    if (start < end) {
        int mid = start + (end - start) / 2;

        // Recursively sort the left and right halves
        mergeSortStr(arr, start, mid);
        mergeSortStr(arr, mid + 1, end);

        // Merge the sorted halves
        mergeStrings(arr, start, mid, mid + 1, end);
    }
}
