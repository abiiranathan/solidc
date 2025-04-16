#include "../include/slist.h"
#include <stdlib.h>
#include <string.h>

slist_t* slist_new(size_t elem_size) {
    slist_t* list = (slist_t*)malloc(sizeof(slist_t));
    if (list == NULL) {
        return NULL;
    }
    list->head      = NULL;
    list->size      = 0;
    list->elem_size = elem_size;
    return list;
}

void slist_free(slist_t* list) {
    if (list == NULL) {
        return;
    }
    slist_clear(list);
    free(list);
    list = NULL;
}

size_t slist_size(slist_t* list) {
    if (list == NULL) {
        return 0;
    }
    return list->size;
}

void slist_clear(slist_t* list) {
    if (list == NULL) {
        return;
    }
    slist_node_t* current = list->head;
    while (current) {
        slist_node_t* next = current->next;
        slist_node_free(current);
        current = next;
    }
    list->head = NULL;
    list->size = 0;
}

slist_node_t* slist_node_new(size_t elem_size, void* data) {
    slist_node_t* node = (slist_node_t*)malloc(sizeof(slist_node_t));
    if (node == NULL) {
        return NULL;
    }
    node->data = malloc(elem_size);
    if (node->data == NULL) {
        free(node);
        return NULL;
    }
    node->next = NULL;
    memcpy(node->data, data, elem_size);
    return node;
}

void slist_node_free(slist_node_t* node) {
    if (node == NULL) {
        return;
    }
    if (node->data) {
        free(node->data);
        node->data = NULL;
    }
    free(node);
    node = NULL;
}

void slist_push(slist_t* list, void* elem) {
    slist_node_t* new_node = slist_node_new(list->elem_size, elem);
    if (new_node == NULL) {
        return;
    }
    new_node->next = list->head;
    list->head     = new_node;
    list->size++;
}

void slist_pop(slist_t* list) {
    if (list == NULL || list->head == NULL) {
        return;
    }
    slist_node_t* temp = list->head;
    list->head         = list->head->next;
    slist_node_free(temp);
    list->size--;
}

void slist_insert(slist_t* list, size_t index, void* elem) {
    if (index > list->size) {
        return;
    }

    if (index == 0) {
        slist_push(list, elem);
        return;
    }

    // Remember nodes are in reverse order
    // i.e last push is the first node
    slist_node_t* current = list->head;
    for (size_t i = 0; i < index - 1; i++) {
        if (current == NULL) {
            return;
        }
        current = current->next;
    }

    slist_node_t* new_node = slist_node_new(list->elem_size, elem);
    if (new_node == NULL) {
        return;
    }
    new_node->next = current->next;
    current->next  = new_node;
    list->size++;
}

void slist_remove(slist_t* list, size_t index) {
    if (index == 0) {
        slist_pop(list);
        return;
    }

    slist_node_t* current = list->head;
    for (size_t i = 0; i < index - 1; i++) {
        if (current == NULL) {
            return;
        }
        current = current->next;
    }
    slist_node_t* temp = current->next;
    current->next      = temp->next;
    slist_node_free(temp);
    list->size--;
}

void* slist_get(slist_t* list, size_t index) {
    if (list == NULL) {
        return NULL;
    }

    slist_node_t* current = list->head;
    for (size_t i = 0; i < index; i++) {
        if (current == NULL) {
            return NULL;
        }
        current = current->next;
    }
    return current->data;
}

int slist_index_of(slist_t* list, void* elem) {
    if (list == NULL) {
        return -1;
    }

    slist_node_t* current = list->head;
    int index             = 0;
    while (current) {
        if (memcmp(current->data, elem, list->elem_size) == 0) {
            return index;
        }
        current = current->next;
        index++;
    }
    return -1;
}

void slist_insert_after(slist_t* list, void* elem, void* after) {
    int index = slist_index_of(list, after);
    if (index >= 0 && index < (int)list->size) {
        slist_insert(list, (size_t)index + 1, elem);
    }
}

void slist_insert_before(slist_t* list, void* elem, void* before) {
    int index = slist_index_of(list, before);
    if (index > 0 && index < (int)list->size) {
        slist_insert(list, (size_t)index - 1, elem);
    } else if (index == 0) {
        slist_push(list, elem);
    }
}

void slist_print_asint(slist_t* list) {
    if (list == NULL) {
        return;
    }
    slist_node_t* current = list->head;
    while (current) {
        printf("%d ", *(int*)current->data);
        current = current->next;
    }
    printf("\n");
}

void slist_print_aschar(slist_t* list) {
    if (list == NULL) {
        return;
    }
    slist_node_t* current = list->head;
    while (current) {
        printf("%c ", *(char*)current->data);
        current = current->next;
    }
    printf("\n");
}
