#include <stdio.h>
#include <stdlib.h>

// Generic LinkedList Node structure
typedef struct Node {
    void* data;
    struct Node* next;
} Node;

// Create a new LinkedList node
Node* createNode(void* data) {
    Node* node = (Node*)malloc(sizeof(Node));
    node->data = data;
    node->next = NULL;
    return node;
}

// Free a LinkedList node
void freeNode(Node* node) {
    free(node);
}

// Append data to the end of the LinkedList
void appendNode(Node** head, void* data) {
    Node* newNode = createNode(data);
    if (*head == NULL) {
        *head = newNode;
    } else {
        Node* temp = *head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = newNode;
    }
}

// Traverse the LinkedList and perform an action on each node
void traverseList(Node* head, void (*action)(void*)) {
    Node* temp = head;
    while (temp != NULL) {
        action(temp->data);
        temp = temp->next;
    }
}

// Free the entire LinkedList
void freeList(Node** head) {
    Node* curr = *head;
    while (curr != NULL) {
        Node* temp = curr;
        curr = curr->next;
        freeNode(temp);
    }
    *head = NULL;
}

// Prepend data to the beginning of the LinkedList
void prependNode(Node** head, void* data) {
    Node* newNode = createNode(data);
    newNode->next = *head;
    *head = newNode;
}

// Insert data at a specific position in the LinkedList
void insertNode(Node** head, void* data, int position) {
    if (position <= 0) {
        prependNode(head, data);
    } else {
        Node* newNode = createNode(data);
        Node* temp = *head;
        int count = 0;
        while (temp != NULL && count < position - 1) {
            temp = temp->next;
            count++;
        }
        if (temp != NULL) {
            newNode->next = temp->next;
            temp->next = newNode;
        } else {
            freeNode(newNode);
            fprintf(stderr, "Invalid position for node insertion!\n");
        }
    }
}

// Remove the first occurrence of data from the LinkedList
void removeNode(Node** head, void* data) {
    Node* temp = *head;
    Node* prev = NULL;
    while (temp != NULL && temp->data != data) {
        prev = temp;
        temp = temp->next;
    }
    if (temp != NULL) {
        if (prev != NULL) {
            prev->next = temp->next;
        } else {
            *head = temp->next;
        }
        freeNode(temp);
    }
}

// Get the length (number of nodes) in the LinkedList
int getLength(Node* head) {
    int count = 0;
    Node* temp = head;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

// Get the data at a specific position in the LinkedList
void* getDataAtPosition(Node* head, int position) {
    if (position >= 0) {
        Node* temp = head;
        int count = 0;
        while (temp != NULL && count < position) {
            temp = temp->next;
            count++;
        }
        if (temp != NULL) {
            return temp->data;
        }
    }
    return NULL;
}