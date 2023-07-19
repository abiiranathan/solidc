#ifndef BTREE_H
#define BTREE_H

#ifndef MAX_KEYS
#define MAX_KEYS 4
#endif

// B+ tree node.
typedef struct BPTreeNode BPTreeNode;

// Insert key with data into B+ tree.
void bt_insert(BPTreeNode** root, int key, void* data);

// Delete key from B+ tree.
void bt_delete_node(BPTreeNode** root, int key);

// Search for data from tree. If key not found
// it returns a NULL pointer.
void* bt_search(BPTreeNode* root, int key);

// Free memory allocated by the whole B+ tree.
void bt_free(BPTreeNode* root);

// Print B+ tree keys in alphabetical order.
void bt_print(BPTreeNode* root);

#endif  // BTREE_H
