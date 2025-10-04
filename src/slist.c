#include "../include/slist.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

// round size up to alignment
static inline size_t aligned_size(size_t n) {
    size_t a = alignof(max_align_t);
    return (n + a - 1) & ~(a - 1);
}

slist* slist_new(size_t elem_size) {
    slist* list = (slist*)malloc(sizeof(slist));
    if (!list) return NULL;

    list->head      = NULL;
    list->tail      = NULL;
    list->size      = 0;
    list->elem_size = elem_size;
    return list;
}

void slist_free(slist* list) {
    if (!list) return;
    slist_clear(list);
    free(list);
}

size_t slist_size(const slist* list) {
    return list ? list->size : 0;
}

void slist_clear(slist* list) {
    if (!list) return;

    slist_node_t* current = list->head;
    while (current) {
        slist_node_t* next = current->next;
        slist_node_free(current);
        current = next;
    }
    list->head = list->tail = NULL;
    list->size              = 0;
}

slist_node_t* slist_node_new(size_t elem_size, void* data) {
    size_t total_size  = sizeof(slist_node_t) + aligned_size(elem_size);
    slist_node_t* node = (slist_node_t*)malloc(total_size);
    if (!node) return NULL;

    node->next = NULL;
    node->data = (void*)(node + 1);  // data lives right after struct
    memcpy(node->data, data, elem_size);
    return node;
}

void slist_node_free(slist_node_t* node) {
    free(node);  // single allocation
}

void slist_push_front(slist* list, void* elem) {
    if (!list) return;
    slist_node_t* node = slist_node_new(list->elem_size, elem);
    if (!node) return;

    node->next = list->head;
    list->head = node;
    if (!list->tail) list->tail = node;  // first element
    list->size++;
}

void slist_push_back(slist* list, void* elem) {
    if (!list) return;
    slist_node_t* node = slist_node_new(list->elem_size, elem);
    if (!node) return;

    if (!list->head) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail       = node;
    }
    list->size++;
}

void slist_pop_front(slist* list) {
    if (!list || !list->head) return;
    slist_node_t* temp = list->head;
    list->head         = temp->next;
    if (!list->head) list->tail = NULL;
    slist_node_free(temp);
    list->size--;
}

void slist_insert(slist* list, size_t index, void* elem) {
    if (!list || index > list->size) return;

    if (index == 0) {
        slist_push_front(list, elem);
        return;
    }
    if (index == list->size) {
        slist_push_back(list, elem);
        return;
    }

    slist_node_t* current = list->head;
    for (size_t i = 0; i < index - 1; i++) {
        current = current->next;
    }

    slist_node_t* node = slist_node_new(list->elem_size, elem);
    if (!node) return;

    node->next    = current->next;
    current->next = node;
    list->size++;
}

void slist_remove(slist* list, size_t index) {
    if (!list || index >= list->size) return;

    if (index == 0) {
        slist_pop_front(list);
        return;
    }

    slist_node_t* current = list->head;
    for (size_t i = 0; i < index - 1; i++) {
        current = current->next;
    }

    slist_node_t* temp = current->next;
    current->next      = temp->next;
    if (temp == list->tail) list->tail = current;
    slist_node_free(temp);
    list->size--;
}

void* slist_get(const slist* list, size_t index) {
    if (!list || index >= list->size) return NULL;

    slist_node_t* current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }
    return current->data;
}

int slist_index_of(const slist* list, void* elem) {
    if (!list) return -1;

    slist_node_t* current = list->head;
    int index             = 0;
    while (current) {
        if (memcmp(current->data, elem, list->elem_size) == 0) return index;
        current = current->next;
        index++;
    }
    return -1;
}

void slist_insert_after(slist* list, void* elem, void* after) {
    int idx = slist_index_of(list, after);
    if (idx >= 0 && (size_t)idx < list->size) slist_insert(list, (size_t)idx + 1, elem);
}

void slist_insert_before(slist* list, void* elem, void* before) {
    int idx = slist_index_of(list, before);
    if (idx > 0 && (size_t)idx < list->size)
        slist_insert(list, (size_t)idx - 1, elem);
    else if (idx == 0)
        slist_push_front(list, elem);
}

void slist_print_asint(const slist* list) {
    SLIST_FOR_EACH(list, node) {
        printf("%d ", *(int*)node->data);
    }
    printf("\n");
}

void slist_print_aschar(const slist* list) {
    SLIST_FOR_EACH(list, node) {
        printf("%c ", *(char*)node->data);
    }
    printf("\n");
}
