#include "bplustree.h"
#include <stdio.h>
#include <stdlib.h>

// Main function for testing the B+ tree implementation
int main() {
    BPTreeNode* root = NULL;

    // Arrays for keys and associated data
    int keys[] = {10, 20, 5, 15, 25, 30, 35, 40, 45, 50};
    int data[] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
    int num_elements = sizeof(keys) / sizeof(keys[0]);

    // Insert keys with associated data into the B+ tree
    for (int i = 0; i < num_elements; i++) {
        bt_insert(&root, keys[i], &data[i]);
    }

    bt_delete_node(&root, 30);
    bt_delete_node(&root, 35);
    bt_delete_node(&root, 10);

    // Search for keys and retrieve associated data from the B+ tree
    printf("Searching for keys and associated data:\n");
    for (int i = 0; i < num_elements; i++) {
        int key = keys[i];
        int* result = (int*)bt_search(root, key);
        if (result != NULL) {
            printf("Key %d: %d\n", key, *result);
        } else {
            printf("Key %d: Not found\n", key);
        }
    }

    // Print the keys in the B+ tree in ascending order
    printf("\nKeys in the B+ tree in ascending order:\n");
    bt_print(root);
    printf("\n");

    // Free the memory occupied by the B+ tree
    bt_free(root);
    return 0;
}
