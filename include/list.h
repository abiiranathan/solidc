#ifndef F3A813AA_9A33_453A_8622_F656C6A089F1
#define F3A813AA_9A33_453A_8622_F656C6A089F1

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

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
// Returns -1 if the element is not found
int list_index_of(list_t* list, void* elem);

// Insert an element at the given index
void list_insert(list_t* list, size_t index, void* elem);

// Insert element after the given element
void list_insert_after(list_t* list, void* elem, void* after);

// Insert element before the given element
void list_insert_before(list_t* list, void* elem, void* before);

// Get the size of the list
size_t list_size(list_t* list);

// Remove an element at the given index
void list_remove(list_t* list, void* elem);

// Clear the list
void list_clear(list_t* list);

// Iteration
#define list_foreach(list, node_data)                                                                        \
    for (list_node_t* node = list->head; node != NULL; node = node->next)                                    \
        for (int cont = 1; cont; cont = 0)                                                                   \
            for (void* node_data = node->data; cont; cont = 0)

#if defined(__cplusplus)
}
#endif

#endif /* F3A813AA_9A33_453A_8622_F656C6A089F1 */
