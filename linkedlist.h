#ifndef __LINKED_LIST_H__
#define __LINKED_LIST_H__

#include <stdlib.h>

// Generic LinkedList Node structure
typedef struct Node Node;

// Create a new LinkedList node
Node* createNode(void* data);

// Free a LinkedList node
void freeNode(Node* node);

// Append data to the end of the LinkedList
void appendNode(Node** head, void* data);

// Traverse the LinkedList and perform an action on each node
void traverseList(Node* head, void (*action)(void*));

// Free the entire LinkedList
void freeList(Node** head);

// Get the data at a specific position in the LinkedList
void* getDataAtPosition(Node* head, int position);

// Get the length (number of nodes) in the LinkedList
int getLength(Node* head);

// Remove the first occurrence of data from the LinkedList
void removeNode(Node** head, void* data);

// Insert data at a specific position in the LinkedList
void insertNode(Node** head, void* data, int position);

// Prepend data to the beginning of the LinkedList
void prependNode(Node** head, void* data);

#endif /* __LINKED_LIST_H__ */
