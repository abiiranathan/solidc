#ifndef D1A994E3_5DD3_4405_A395_67CCF6C87622
#define D1A994E3_5DD3_4405_A395_67CCF6C87622

#if defined(__cplusplus)
extern "C" {
#endif

// ordered map
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ordered_map_node {
    void* key;
    void* value;
    struct ordered_map_node* next;
    struct ordered_map_node* prev;
} ordered_map_node_t;

typedef struct {
    ordered_map_node_t* head;
    ordered_map_node_t* tail;
    size_t size;
    size_t key_size;
    size_t value_size;
} ordered_map_t;

#define ordermap_foreach(map, node_var)                                                            \
    for (ordered_map_node_t* node_var = map->head; node_var != NULL; node_var = node_var->next)

ordered_map_t* ordered_map_new(size_t key_size, size_t value_size);
void ordered_map_free(ordered_map_t* map);
size_t ordered_map_size(ordered_map_t* map);
void ordered_map_clear(ordered_map_t* map);
ordered_map_node_t* ordered_map_node_new(size_t key_size, size_t value_size, void* key,
                                         void* value);
void ordered_map_node_free(ordered_map_node_t* node);
void ordered_map_insert(ordered_map_t* map, void* key, void* value);
void ordered_map_remove(ordered_map_t* map, void* key);
const void* ordered_map_get(ordered_map_t* map, void* key);
void ordered_map_print_aschar(ordered_map_t* map);

/*
#define map_foreach(map, entry_var)                                                                \
    for (size_t i = 0; i < map->size; i++)                                                         \
        for (int cont = 1; cont; cont = 0)                                                         \
            for (entry* entry_var = &map->entries[i]; cont; cont = 0)
*/
#define ordered_map_foreach(map, key, value)                                                       \
    for (ordered_map_node_t* node = map->head; node != NULL; node = node->next)                    \
        for (int cont = 1; cont; cont = 0)                                                         \
            for (void *key = node->key, *value = node->value; cont; cont = 0)

#if defined(__cplusplus)
}
#endif

#ifdef ORDERED_MAP_IMPL
ordered_map_t* ordered_map_new(size_t key_size, size_t value_size) {
    ordered_map_t* map = (ordered_map_t*)malloc(sizeof(ordered_map_t));
    if (map == NULL) {
        return NULL;
    }
    map->head       = NULL;
    map->tail       = NULL;
    map->size       = 0;
    map->key_size   = key_size;
    map->value_size = value_size;
    return map;
}

void ordered_map_free(ordered_map_t* map) {
    if (map == NULL) {
        return;
    }
    ordered_map_clear(map);
    free(map);
    map = NULL;
}

void ordered_map_clear(ordered_map_t* map) {
    if (map == NULL) {
        return;
    }

    ordered_map_node_t* current = map->head;
    while (current) {
        ordered_map_node_t* next = current->next;
        ordered_map_node_free(current);
        current = next;
    }
    map->head = NULL;
    map->tail = NULL;
    map->size = 0;
}

ordered_map_node_t* ordered_map_node_new(size_t key_size, size_t value_size, void* key,
                                         void* value) {
    ordered_map_node_t* node = (ordered_map_node_t*)malloc(sizeof(ordered_map_node_t));
    if (node == NULL) {
        return NULL;
    }
    node->key = malloc(key_size);
    if (node->key == NULL) {
        free(node);
        return NULL;
    }
    node->value = malloc(value_size);
    if (node->value == NULL) {
        free(node->key);
        free(node);
        return NULL;
    }

    node->next = NULL;
    node->prev = NULL;
    memcpy(node->key, key, key_size);
    memcpy(node->value, value, value_size);
    return node;
}

void ordered_map_node_free(ordered_map_node_t* node) {
    if (node == NULL) {
        return;
    }

    if (node->key) {
        free(node->key);
        node->key = NULL;
    }
    if (node->value) {
        free(node->value);
        node->value = NULL;
    }
    free(node);
}

void ordered_map_insert(ordered_map_t* map, void* key, void* value) {
    ordered_map_node_t* new_node = ordered_map_node_new(map->key_size, map->value_size, key, value);
    if (new_node == NULL) {
        fprintf(stderr, "error createing new node\n");
        return;
    }

    ordered_map_node_t* current = map->head;
    while (current) {
        if (memcmp(current->key, key, map->key_size) == 0) {
            memcpy(current->value, value, map->value_size);
            free(new_node);
            return;
        }
        current = current->next;
    }

    if (map->head == NULL) {
        map->head = new_node;
        map->tail = new_node;
    } else {
        new_node->next  = map->head;
        map->head->prev = new_node;
        map->head       = new_node;
    }
    map->size++;
}

void ordered_map_remove(ordered_map_t* map, void* key) {
    if (map == NULL) {
        return;
    }

    ordered_map_node_t* current = map->head;
    while (current) {
        if (memcmp(current->key, key, map->key_size) == 0) {
            if (current->prev) {
                current->prev->next = current->next;
            } else {
                map->head = current->next;
            }
            if (current->next) {
                current->next->prev = current->prev;
            } else {
                map->tail = current->prev;
            }
            ordered_map_node_free(current);
            map->size--;
            return;
        }
        current = current->next;
    }
}

const void* ordered_map_get(ordered_map_t* map, void* key) {
    if (map == NULL) {
        return NULL;
    }

    ordered_map_node_t* current = map->head;
    while (current) {
        if (memcmp(current->key, key, map->key_size) == 0) {
            return (const void*)current->value;
        }
        current = current->next;
    }
    return NULL;
}

void ordered_map_print_aschar(ordered_map_t* map) {
    if (map == NULL) {
        return;
    }

    ordered_map_node_t* current = map->head;
    while (current) {
        if (current->key && current->value)
            printf("%s: %s\n", (char*)current->key, (char*)current->value);
        else
            printf("NULL: NULL\n");
        current = current->next;
    }
}

size_t ordered_map_size(ordered_map_t* map) {
    if (map == NULL) {
        return 0;
    }
    return map->size;
}

#endif  // ORDERED_MAP_IMPL

#endif /* D1A994E3_5DD3_4405_A395_67CCF6C87622 */
