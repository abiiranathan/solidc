#ifndef __SORT_H__
#define __SORT_H__

//Not recommended for large arrays or performance-critical
// applications due to its higher time complexity.
// O(n^2) in worst-case scenario. Space complexity: O(1)
void bubbleSort(int arr[], int size);

// Suitable for small input sizes or when
// minimizing the number of swaps is important
// O(n^2) in worst-case scenario. Space complexity: O(1)
void selectionSort(int arr[], int size);

// Suitable for small input sizes or when the array is
// almost sorted or partially sorted
// O(n^2) in worst-case scenario. Space complexity: O(1)
void insertionSort(int arr[], int size);

/*
Offers a guaranteed worst-case time complexity of O(n log n), 
making it efficient for large input sizes.
Provides stability, preserving the relative order of equal elements.
Well-suited for external sorting when data doesn't fit entirely in memory.

O(n log n) in worst-case scenario. Space complexity: O(n)
*/
void mergeSort(int arr[], int size);

/*
In radixSort(), the algorithm sorts the array by processing digits 
from the least significant to the most significant. 
It uses counting sort as a subroutine to sort the array based 
on each digit position.

O(kn), space: O(n+k) where
 - 'n' represents the number of elements in the array.
 - 'k' represents the maximum number of digits or bits in the input elements.
*/
void radixSort(int arr[], int size);

// Merge Sort implementation for array of strings
// from start index to end.
// O(n log n)
void mergeSortStr(char** arr, int start, int end);

#endif /* __SORT_H__ */
