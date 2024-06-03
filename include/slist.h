#ifndef E3BD52A6_7A93_4758_8633_F1005A651260
#define E3BD52A6_7A93_4758_8633_F1005A651260

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>

// A node in a singly-linked list.
typedef struct slist_node {
    void* data;
    struct slist_node* next;
} slist_node_t;

// A singly-linked list.
typedef struct {
    slist_node_t* head;
    size_t size;
    size_t elem_size;
} slist_t;

slist_t* slist_new(size_t elem_size);
void slist_free(slist_t* list);

size_t slist_size(slist_t* list);
void slist_clear(slist_t* list);

slist_node_t* slist_node_new(size_t elem_size, void* data);
void slist_node_free(slist_node_t* node);

void slist_push(slist_t* list, void* elem);
void slist_pop(slist_t* list);

void slist_insert(slist_t* list, size_t index, void* elem);
void slist_remove(slist_t* list, size_t index);

void* slist_get(slist_t* list, size_t index);
int slist_index_of(slist_t* list, void* elem);

void slist_insert_after(slist_t* list, void* elem, void* after);

void slist_insert_before(slist_t* list, void* elem, void* before);

void slist_print_asint(slist_t* list);
void slist_print_aschar(slist_t* list);

#if defined(__cplusplus)
}
#endif

#endif /* E3BD52A6_7A93_4758_8633_F1005A651260 */
