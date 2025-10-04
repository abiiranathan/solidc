#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

// A node in a doubly-linked list
typedef struct list_node {
    void* data;  // pointer to element (stored inline after node)
    struct list_node* next;
    struct list_node* prev;
} list_node_t;

// A doubly-linked list
typedef struct {
    list_node_t* head;
    list_node_t* tail;
    size_t size;
    size_t elem_size;
} list_t;

list_t* list_new(size_t elem_size);
void list_free(list_t* list);

void list_clear(list_t* list);
size_t list_size(const list_t* list);

void list_push_back(list_t* list, void* elem);
void list_pop_back(list_t* list);

void list_push_front(list_t* list, void* elem);
void list_pop_front(list_t* list);

void* list_get(const list_t* list, size_t index);
int list_index_of(const list_t* list, void* elem);

void list_insert(list_t* list, size_t index, void* elem);
void list_remove(list_t* list, void* elem);

void list_insert_after(list_t* list, void* elem, void* after);
void list_insert_before(list_t* list, void* elem, void* before);

#define LIST_FOR_EACH(list, node)                                                                  \
    for (list_node_t * (node) = (list)->head; (node); (node) = (node)->next)

#define LIST_FOR_EACH_REVERSE(list, node)                                                          \
    for (list_node_t * (node) = (list)->tail; (node); (node) = (node)->prev)

#if defined(__cplusplus)
}
#endif

#endif /* LIST_H */
