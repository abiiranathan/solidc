#include <stdio.h>
#include <stdlib.h>

#include "bplustree.h"

#define MIN_KEYS ((MAX_KEYS + 1) / 2)
#define MAX_CHILDREN (MAX_KEYS + 1)

// BPTreeNode structure
typedef struct BPTreeNode {
    int num_keys;
    int keys[MAX_KEYS];
    void* data[MAX_KEYS];  // Array to store associated data
    struct BPTreeNode* children[MAX_CHILDREN];
    struct BPTreeNode* parent;
    int is_leaf;
} BPTreeNode;

BPTreeNode* createNode();
void freeNode(BPTreeNode* node);
BPTreeNode* findLeafNode(BPTreeNode* node, int key);
void splitLeafNode(BPTreeNode** root, BPTreeNode* node, void* data);
void insertKeyInLeaf(BPTreeNode* node, int key, void* data);
void insertKeyInParent(BPTreeNode** root, BPTreeNode* node, int key,
                       BPTreeNode* child, int* data);
void splitInternalNode(BPTreeNode** root, BPTreeNode* node, void* data);
void mergeLeafNodes(BPTreeNode** root, BPTreeNode* node1, BPTreeNode* node2);

// Function to create a new node
BPTreeNode* createNode() {
    BPTreeNode* newNode = (BPTreeNode*)malloc(sizeof(BPTreeNode));
    if (newNode == NULL) {
        printf("Error: Memory allocation failed!\n");
        exit(1);
    }

    newNode->num_keys = 0;
    newNode->parent   = NULL;
    newNode->is_leaf  = 0;

    // Initialize keys and associated data
    for (int i = 0; i < MAX_KEYS; i++) {
        newNode->keys[i]     = 0;
        newNode->data[i]     = NULL;
        newNode->children[i] = NULL;
    }

    newNode->children[MAX_CHILDREN - 1] = NULL;
    return newNode;
}

// Function to free a node
void freeNode(BPTreeNode* node) {
    if (node) {
        free(node);
    }
}

// Function to free the memory occupied by the B+ tree
void bt_free(BPTreeNode* root) {
    if (root != NULL) {
        if (!root->is_leaf) {
            for (int i = 0; i < root->num_keys + 1; i++) {
                bt_free(root->children[i]);
            }
        }
        freeNode(root);
    }
}

// Function to insert a key into the B+ tree
void bt_insert(BPTreeNode** root, int key, void* data) {
    if (*root == NULL) {
        // Create root node if the tree is empty
        *root            = createNode();
        (*root)->keys[0] = key;
        (*root)->data[0] = data;
        (*root)->num_keys++;
    } else {
        // Find the leaf node where the key should be inserted
        BPTreeNode* leafNode = findLeafNode(*root, key);

        // If the leaf node is full, split it
        if (leafNode->num_keys == MAX_KEYS) {
            splitLeafNode(root, leafNode, data);
            leafNode = findLeafNode(*root, key);
        }

        // Insert the key into the leaf node
        insertKeyInLeaf(leafNode, key, data);
    }
}

// Function to find the leaf node where the key should be inserted
BPTreeNode* findLeafNode(BPTreeNode* node, int key) {
    if (node->is_leaf) {
        return node;
    } else {
        int i = 0;
        while (i < node->num_keys && key >= node->keys[i]) {
            i++;
        }

        if (node->children[i] == NULL) {
            return node;  // Return the current node as leaf node
        } else {
            return findLeafNode(node->children[i], key);
        }
    }
}

// Function to split a leaf node
void splitLeafNode(BPTreeNode** root, BPTreeNode* node, void* data) {
    // Create a new sibling node
    BPTreeNode* sibling = createNode();
    sibling->is_leaf    = 1;
    sibling->parent     = node->parent;

    // Copy the upper half of keys to the sibling node
    int i;
    int splitIndex = (node->num_keys + 1) / 2;  // Find the split index
    for (i = splitIndex; i < node->num_keys; i++) {
        sibling->keys[i - splitIndex]     = node->keys[i];
        sibling->data[i - splitIndex]     = node->data[i];
        sibling->children[i - splitIndex] = node->children[i];
        node->keys[i]     = 0;     // Clear the keys in the original node
        node->data[i]     = NULL;  // Clear the keys in the original node
        node->children[i] = NULL;  // Clear the children in the original node
    }

    sibling->num_keys                 = node->num_keys - splitIndex;
    sibling->children[i - splitIndex] = node->children[i];
    node->num_keys                    = splitIndex;

    // Insert the new key into the parent node
    insertKeyInParent(root, node, sibling->keys[0], sibling, data);
}

// Function to insert a key into a leaf node
void insertKeyInLeaf(BPTreeNode* node, int key, void* data) {
    int i;
    for (i = node->num_keys; i > 0 && key < node->keys[i - 1]; i--) {
        node->keys[i] = node->keys[i - 1];
        node->data[i] = node->data[i - 1];
    }

    node->keys[i] = key;
    node->data[i] = data;
    node->num_keys++;
}

// Function to insert a key into the parent node
// Function to insert a key and associated data into the parent node
void insertKeyInParent(BPTreeNode** root, BPTreeNode* node, int key,
                       BPTreeNode* child, int* data) {
    BPTreeNode* parent = node->parent;

    if (parent == NULL) {
        // Create a new root node if the current node is the root
        BPTreeNode* newRoot  = createNode();
        newRoot->keys[0]     = key;
        newRoot->data[0]     = data;
        newRoot->children[0] = node;
        newRoot->children[1] = child;
        newRoot->num_keys++;
        node->parent  = newRoot;
        child->parent = newRoot;
        *root         = newRoot;
    } else {
        // Insert the key and associated data into the parent node
        int i;
        for (i = parent->num_keys; i > 0 && key < parent->keys[i - 1]; i--) {
            parent->keys[i]         = parent->keys[i - 1];
            parent->data[i]         = parent->data[i - 1];
            parent->children[i + 1] = parent->children[i];
        }
        parent->keys[i]         = key;
        parent->data[i]         = data;
        parent->children[i + 1] = child;
        parent->num_keys++;

        // If the parent node is full, split it recursively
        if (parent->num_keys == MAX_KEYS) {
            splitInternalNode(root, parent, data);
        }
    }
}

// Function to split an internal node
void splitInternalNode(BPTreeNode** root, BPTreeNode* node, void* data) {
    // Create a new sibling node
    BPTreeNode* sibling = createNode();
    sibling->parent     = node->parent;

    // Copy the upper half of keys and children to the sibling node
    int i;
    int splitIndex = (node->num_keys + 1) / 2;  // Find the split index
    for (i = splitIndex; i < node->num_keys; i++) {
        sibling->keys[i - splitIndex]     = node->keys[i];
        sibling->data[i - splitIndex]     = node->data[i];
        sibling->children[i - splitIndex] = node->children[i];
        node->keys[i]     = 0;     // Clear the keys in the original node
        node->data[i]     = NULL;  // Clear the data in the original node
        node->children[i] = NULL;  // Clear the children in the original node
    }
    sibling->children[i - splitIndex] = node->children[i];
    node->num_keys                    = splitIndex;

    // Insert the new key into the parent node
    insertKeyInParent(root, node->parent, node->keys[splitIndex - 1], sibling,
                      data);
}

// Function to search for a key in the B+ tree
void* bt_search(BPTreeNode* root, int key) {
    BPTreeNode* leafNode = findLeafNode(root, key);

    // Check if the key exists in the leaf node
    for (int i = 0; i < leafNode->num_keys; i++) {
        if (leafNode->keys[i] == key) {
            // Key found, return associated data
            return leafNode->data[i];
        }
    }
    return NULL;  // Key not found
}

// Function to print the keys in the B+ tree in ascending order
void bt_print(BPTreeNode* root) {
    if (root != NULL) {
        if (root->is_leaf) {
            // Print the unique keys in the leaf node
            printf("%d ", root->keys[0]);
            for (int i = 1; i < root->num_keys; i++) {
                if (root->keys[i] != root->keys[i - 1]) {
                    printf("%d ", root->keys[i]);
                }
            }
        } else {
            for (int i = 0; i < root->num_keys; i++) {
                bt_print(root->children[i]);
                if (i > 0 && root->keys[i] != root->keys[i - 1]) {
                    printf("%d ", root->keys[i]);
                }
            }
            bt_print(root->children[root->num_keys]);
        }
    }
}

void bt_delete_node(BPTreeNode** root, int key) {
    // Find the leaf node where the key is stored
    BPTreeNode* leafNode = findLeafNode(*root, key);

    // Check if the key exists in the leaf node
    int index = -1;
    for (int i = 0; i < leafNode->num_keys; i++) {
        if (leafNode->keys[i] == key) {
            index = i;
            break;
        }
    }

    // If the key does not exist, return
    if (index == -1) {
        return;
    }

    // Delete the key from the leaf node
    for (int i = index; i < leafNode->num_keys - 1; i++) {
        leafNode->keys[i] = leafNode->keys[i + 1];
        leafNode->data[i] = leafNode->data[i + 1];
    }
    leafNode->num_keys--;

    // If the leaf node is underflowed, merge it with a sibling node
    if (leafNode->num_keys < MIN_KEYS) {
        BPTreeNode* sibling = leafNode->parent;
        if (sibling != NULL && sibling->num_keys > MIN_KEYS) {
            // Merge leafNode and sibling
            mergeLeafNodes(root, leafNode, sibling);
        } else if (sibling == NULL) {
            // Merge leafNode with its parent
            mergeLeafNodes(root, leafNode, leafNode->parent);
        }
    }
}

void mergeLeafNodes(BPTreeNode** root, BPTreeNode* node1, BPTreeNode* node2) {
    // Check if the two nodes are siblings
    if (node1->parent != node2->parent) {
        return;
    }

    // Move the rightmost key from node2 to node1
    int key                      = node2->keys[0];
    void* data                   = node2->data[0];
    node1->keys[node1->num_keys] = key;
    node1->data[node1->num_keys] = data;
    node1->num_keys++;

    // Shift all keys and data in node2 one position to the left
    for (int i = 1; i < node2->num_keys; i++) {
        node2->keys[i - 1] = node2->keys[i];
        node2->data[i - 1] = node2->data[i];
    }
    node2->num_keys--;

    // If the right sibling of node2 is not NULL, then borrow a key from it
    if (node2->children[0]) {
        // Borrow the leftmost key from the right sibling
        key                          = node2->children[0]->keys[0];
        data                         = node2->children[0]->data[0];
        node2->keys[node2->num_keys] = key;
        node2->data[node2->num_keys] = data;
        node2->num_keys++;

        // Shift all keys and data in the right sibling one position to the right
        for (int i = 1; i < node2->children[0]->num_keys; i++) {
            node2->children[0]->keys[i - 1] = node2->children[0]->keys[i];
            node2->children[0]->data[i - 1] = node2->children[0]->data[i];
        }
        node2->children[0]->num_keys--;
    } else {
        // Set the parent of node1 to be the parent of node2
        node1->parent = node2->parent;

        // Remove node2 from the parent node
        for (int i = 0; i < node2->parent->num_keys; i++) {
            if (node2->parent->keys[i] == node2->keys[0]) {
                for (int j = i; j < node2->parent->num_keys - 1; j++) {
                    node2->parent->keys[j] = node2->parent->keys[j + 1];
                }
                break;
            }
        }
        node2->parent->num_keys--;

        // Free node2
        freeNode(node2);
    }
}
