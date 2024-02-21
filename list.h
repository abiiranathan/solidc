#ifndef F3A813AA_9A33_453A_8622_F656C6A089F1
#define F3A813AA_9A33_453A_8622_F656C6A089F1
#include <stdio.h>

// Node in the doubly linked list storing the data
// and pointers to the next and previous nodes
typedef struct list_node {
    void* data;
    struct list_node* next;
    struct list_node* prev;
} list_node_t;

// Doubly linked list type storing the head and tail nodes.
// Also stores the size of the list and the size of the elements.
typedef struct {
    list_node_t* head;
    list_node_t* tail;
    size_t size;
    size_t elem_size;
} list_t;

// Create a new list
list_t* list_new(size_t elem_size);

// Free the list
void list_free(list_t* list);

// Push an element to the end of the list
void list_push_back(list_t* list, void* elem);

// Pop an element from the end of the list
void list_pop_back(list_t* list);

// Push an element to the front of the list
void list_push_front(list_t* list, void* elem);

// Pop an element from the front of the list
void list_pop_front(list_t* list);

// Get the element at the given index
void* list_get(list_t* list, size_t index);

// Returns the index of the first occurrence of the element in the list
size_t list_index_of(list_t* list, void* elem);

// Insert an element at the given index
void list_insert(list_t* list, size_t index, void* elem);

// // Insert element after the given element
void list_insert_after(list_t* list, void* elem, void* after);

// // Insert element before the given element
void list_insert_before(list_t* list, void* elem, void* before);

// Get the size of the list
size_t list_size(list_t* list);

// Remove an element at the given index
void list_remove(list_t* list, void* elem);

// Clear the list
void list_clear(list_t* list);

// Create a new node
list_node_t* list_node_new(size_t elem_size, void* data);
void list_node_free(list_node_t* node);

#ifdef LIST_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

// Create a new list
list_t* list_new(size_t elem_size) {
    list_t* list = (list_t*)malloc(sizeof(list_t));
    if (!list)
        return NULL;
    list->head      = NULL;
    list->tail      = NULL;
    list->size      = 0;
    list->elem_size = elem_size;
    return list;
}

void list_remove(list_t* list, void* elem) {
    size_t index = list_index_of(list, elem);
    if (index == -1) {
        return;
    }

    list_node_t* current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }
    if (current->prev) {
        current->prev->next = current->next;
    } else {
        list->head = current->next;
    }
    if (current->next) {
        current->next->prev = current->prev;
    } else {
        list->tail = current->prev;
    }
    list_node_free(current);
    list->size--;
}

// Free the list
void list_free(list_t* list) {
    if (!list)
        return;
    list_clear(list);
    free(list);
}

// Push an element to the end of the list
void list_push_back(list_t* list, void* elem) {
    list_node_t* new_node = list_node_new(list->elem_size, elem);
    if (!new_node)
        return;
    if (list->tail) {
        new_node->prev   = list->tail;
        list->tail->next = new_node;
        list->tail       = new_node;
    } else {
        list->head = new_node;
        list->tail = new_node;
    }
    list->size++;
}

// Pop an element from the end of the list
void list_pop_back(list_t* list) {
    if (!list || !list->tail)
        return;
    list_node_t* to_remove = list->tail;
    list->tail             = to_remove->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }
    list->size--;
    list_node_free(to_remove);
}

// Push an element to the front of the list
void list_push_front(list_t* list, void* elem) {
    list_node_t* new_node = list_node_new(list->elem_size, elem);
    if (!new_node)
        return;
    if (list->head) {
        new_node->next   = list->head;
        list->head->prev = new_node;
        list->head       = new_node;
    } else {
        list->head = new_node;
        list->tail = new_node;
    }
    list->size++;
}

// Pop an element from the front of the list
void list_pop_front(list_t* list) {
    if (!list || !list->head)
        return;
    list_node_t* to_remove = list->head;
    list->head             = to_remove->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }
    list->size--;
    list_node_free(to_remove);
}

// Get the element at the given index
void* list_get(list_t* list, size_t index) {
    if (!list || index >= list->size)
        return NULL;
    list_node_t* current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }
    return current->data;
}

size_t list_index_of(list_t* list, void* elem) {
    if (!list)
        return -1;

    list_node_t* current = list->head;
    size_t index         = 0;
    while (current) {
        if (memcmp(current->data, elem, list->elem_size) == 0) {
            return index;
        }
        current = current->next;
        index++;
    }
    return -1;
}

// // Insert element after the given element
void list_insert_after(list_t* list, void* elem, void* after) {
    size_t index = list_index_of(list, after);
    if (index == -1) {
        return;
    }
    list_insert(list, index + 1, elem);
}

// // Insert element before the given element
void list_insert_before(list_t* list, void* elem, void* before) {
    size_t index = list_index_of(list, before);
    if (index == -1) {
        return;
    }
    list_insert(list, index, elem);
}

// Set the element at the given index
void list_insert(list_t* list, size_t index, void* elem) {
    if (!list || index >= list->size)
        return;
    list_node_t* new_node = list_node_new(list->elem_size, elem);
    if (!new_node)
        return;
    list_node_t* current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }

    new_node->next = current;
    new_node->prev = current->prev;
    if (current->prev) {
        current->prev->next = new_node;
    } else {
        list->head = new_node;
    }
    current->prev = new_node;
    list->size++;
}

// Get the size of the list
size_t list_size(list_t* list) {
    if (!list)
        return 0;
    return list->size;
}

// Clear the list
void list_clear(list_t* list) {
    if (!list)
        return;
    list_node_t* current = list->head;
    while (current) {
        list_node_t* next = current->next;
        list_node_free(current);
        current = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

// Implementation
list_node_t* list_node_new(size_t elem_size, void* data) {
    list_node_t* node = (list_node_t*)malloc(sizeof(list_node_t));
    if (!node)
        return NULL;
    node->data = malloc(elem_size);
    if (!node->data) {
        free(node);
        return NULL;
    }
    node->next = NULL;
    node->prev = NULL;
    memcpy(node->data, data, elem_size);
    return node;
}

void list_node_free(list_node_t* node) {
    if (!node)
        return;

    if (node->data) {
        free(node->data);
        node->data = NULL;
    }
    free(node);
    node = NULL;
}

#endif  // LIST_IMPLEMENTATION

#endif /* F3A813AA_9A33_453A_8622_F656C6A089F1 */
