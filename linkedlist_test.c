#include "linkedlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sample structure
typedef struct {
    int id;
    char name[20];
} Person;

// Action to perform on each node while traversing
void printPerson(void* data) {
    Person* person = (Person*)data;
    printf("ID: %d, Name: %s\n", person->id, person->name);
}

int main() {
    // Create LinkedList
    Node* head = NULL;

    // Append nodes to the LinkedList
    Person* person1 = (Person*)malloc(sizeof(Person));
    person1->id     = 1;
    strcpy(person1->name, "John");
    appendNode(&head, person1);

    Person* person2 = (Person*)malloc(sizeof(Person));
    person2->id     = 2;
    strcpy(person2->name, "Alice");
    appendNode(&head, person2);

    Person* person3 = (Person*)malloc(sizeof(Person));
    person3->id     = 3;
    strcpy(person3->name, "Bob");
    appendNode(&head, person3);

    // Prepend a new node to the LinkedList
    Person* person4 = (Person*)malloc(sizeof(Person));
    person4->id     = 4;
    strcpy(person4->name, "Eve");
    prependNode(&head, person4);

    // Insert a new node at position 2 in the LinkedList
    Person* person5 = (Person*)malloc(sizeof(Person));
    person5->id     = 5;
    strcpy(person5->name, "Michael");
    insertNode(&head, person5, 2);

    // Remove a node from the LinkedList
    removeNode(&head, person3);

    // Traverse the LinkedList and print each node
    printf("LinkedList:\n");
    traverseList(head, printPerson);

    // Get the length of the LinkedList
    int length = getLength(head);
    printf("Length of LinkedList: %d\n", length);

    // Get the data at position 1 in the LinkedList
    void* data = getDataAtPosition(head, 1);
    if (data != NULL) {
        Person* person = (Person*)data;
        printf("Data at position 1: ID: %d, Name: %s\n", person->id,
               person->name);
    }

    // Free the LinkedList
    freeList(&head);
    return 0;
}
