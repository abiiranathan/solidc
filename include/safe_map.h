#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SAFE_MAP_SIZE
// Default size of the map is 100 entries
// Customize this value by defining SAFE_MAP_SIZE before including this header
#define SAFE_MAP_SIZE 100
#endif

// Define the macro for creating a new map type
#define DEFINE_MAP(map_name, key_type, value_type)                                                 \
    typedef struct map_name##_node {                                                               \
        key_type key;                                                                              \
        value_type* value;                                                                         \
        struct map_name##_node* next;                                                              \
    } map_name##_node;                                                                             \
                                                                                                   \
    typedef struct {                                                                               \
        map_name##_node* nodes[SAFE_MAP_SIZE];                                                     \
        size_t size;                                                                               \
    } map_name;                                                                                    \
                                                                                                   \
    map_name* map_name##_create() {                                                                \
        map_name* map = (map_name*)malloc(sizeof(map_name));                                       \
        if (!map) {                                                                                \
            perror("Failed to allocate memory for map");                                           \
            return NULL;                                                                           \
        }                                                                                          \
        memset(map->nodes, 0, sizeof(map->nodes));                                                 \
        map->size = 0;                                                                             \
        return map;                                                                                \
    }                                                                                              \
                                                                                                   \
    void map_name##_destroy(map_name* map) {                                                       \
        for (size_t i = 0; i < SAFE_MAP_SIZE; ++i) {                                               \
            map_name##_node* node = map->nodes[i];                                                 \
            while (node) {                                                                         \
                map_name##_node* next = node->next;                                                \
                free(node->value);                                                                 \
                free(node);                                                                        \
                node = next;                                                                       \
            }                                                                                      \
        }                                                                                          \
        free(map);                                                                                 \
    }                                                                                              \
                                                                                                   \
    size_t map_name##_hash(key_type key) {                                                         \
        return (size_t)key % SAFE_MAP_SIZE;                                                        \
    }                                                                                              \
                                                                                                   \
    void map_name##_insert(map_name* map, key_type key, value_type value) {                        \
        if (map->size >= SAFE_MAP_SIZE) {                                                          \
            fprintf(stderr, "Error: Map is full. Cannot insert.\n");                               \
            return;                                                                                \
        }                                                                                          \
        size_t index = map_name##_hash(key);                                                       \
        map_name##_node* new_node = (map_name##_node*)malloc(sizeof(map_name##_node));             \
        if (!new_node) {                                                                           \
            perror("Failed to allocate memory for new map node");                                  \
            return;                                                                                \
        }                                                                                          \
        new_node->key = key;                                                                       \
        new_node->value = (value_type*)malloc(sizeof(value_type));                                 \
        if (!new_node->value) {                                                                    \
            free(new_node);                                                                        \
            perror("Failed to allocate memory for new map value");                                 \
            return;                                                                                \
        }                                                                                          \
        *(new_node->value) = value;                                                                \
        new_node->next = map->nodes[index];                                                        \
        map->nodes[index] = new_node;                                                              \
        map->size++;                                                                               \
    }                                                                                              \
                                                                                                   \
    value_type* map_name##_get(map_name* map, key_type key) {                                      \
        size_t index = map_name##_hash(key);                                                       \
        map_name##_node* node = map->nodes[index];                                                 \
        while (node) {                                                                             \
            if (node->key == key) {                                                                \
                return node->value;                                                                \
            }                                                                                      \
            node = node->next;                                                                     \
        }                                                                                          \
        return NULL;                                                                               \
    }                                                                                              \
    bool map_name##_contains(map_name* map, key_type key) {                                        \
        return map_name##_get(map, key) != NULL;                                                   \
    }                                                                                              \
                                                                                                   \
    void map_name##_clear(map_name* map) {                                                         \
        for (size_t i = 0; i < SAFE_MAP_SIZE; ++i) {                                               \
            map_name##_node* node = map->nodes[i];                                                 \
            while (node) {                                                                         \
                map_name##_node* next = node->next;                                                \
                free(node->value);                                                                 \
                free(node);                                                                        \
                node = next;                                                                       \
            }                                                                                      \
            map->nodes[i] = NULL;                                                                  \
        }                                                                                          \
        map->size = 0;                                                                             \
    }                                                                                              \
                                                                                                   \
    void map_name##_remove(map_name* map, key_type key) {                                          \
        size_t index = map_name##_hash(key);                                                       \
        map_name##_node* node = map->nodes[index];                                                 \
        map_name##_node* prev = NULL;                                                              \
        while (node) {                                                                             \
            if (node->key == key) {                                                                \
                if (prev) {                                                                        \
                    prev->next = node->next;                                                       \
                } else {                                                                           \
                    map->nodes[index] = node->next;                                                \
                }                                                                                  \
                free(node->value);                                                                 \
                free(node);                                                                        \
                map->size--;                                                                       \
                return;                                                                            \
            }                                                                                      \
            prev = node;                                                                           \
            node = node->next;                                                                     \
        }                                                                                          \
    }

#define MAP_FOR_EACH(map_name, map, iterator)                                                      \
    for (size_t i = 0; i < SAFE_MAP_SIZE; ++i)                                                     \
        for (map_name##_node* iterator = (map)->nodes[i]; iterator; iterator = iterator->next)