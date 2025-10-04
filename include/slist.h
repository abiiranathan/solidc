#ifndef E3BD52A6_7A93_4758_8633_F1005A651260
#define E3BD52A6_7A93_4758_8633_F1005A651260

#include <stddef.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

// A node in a singly-linked list.
typedef struct slist_node {
    void* data;  // pointer to element data (inline in node)
    struct slist_node* next;
} slist_node_t;

// A singly-linked list.
typedef struct {
    slist_node_t* head;
    slist_node_t* tail;  // tail pointer for O(1) append
    size_t size;
    size_t elem_size;
} slist;

slist* slist_new(size_t elem_size);
void slist_free(slist* list);

size_t slist_size(const slist* list);
void slist_clear(slist* list);

slist_node_t* slist_node_new(size_t elem_size, void* data);
void slist_node_free(slist_node_t* node);

void slist_push_front(slist* list, void* elem);
void slist_push_back(slist* list, void* elem);
void slist_pop_front(slist* list);

void slist_insert(slist* list, size_t index, void* elem);
void slist_remove(slist* list, size_t index);

void* slist_get(const slist* list, size_t index);
int slist_index_of(const slist* list, void* elem);

void slist_insert_after(slist* list, void* elem, void* after);
void slist_insert_before(slist* list, void* elem, void* before);

void slist_print_asint(const slist* list);
void slist_print_aschar(const slist* list);

// Iteration macro
#define SLIST_FOR_EACH(list, node)                                                                 \
    for (slist_node_t * (node) = (list)->head; (node); (node) = (node)->next)

#if defined(__cplusplus)
}
#endif

#endif /* E3BD52A6_7A93_4758_8633_F1005A651260 */
