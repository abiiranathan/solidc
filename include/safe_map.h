#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define the macro for creating a new map type
#define DEFINE_MAP(map_name, key_type, value_type)                                                 \
    typedef struct map_name##_node {                                                               \
        key_type key;                                                                              \
        value_type value;                                                                          \
        struct map_name##_node* next;                                                              \
    } map_name##_node;                                                                             \
                                                                                                   \
    typedef struct {                                                                               \
        map_name##_node** buckets;                                                                 \
        size_t bucket_count;                                                                       \
        size_t size;                                                                               \
    } map_name;                                                                                    \
                                                                                                   \
    map_name* map_name##_create(size_t bucket_count) {                                             \
        map_name* map = (map_name*)malloc(sizeof(map_name));                                       \
        if (!map) {                                                                                \
            perror("Failed to allocate memory for map");                                           \
            return NULL;                                                                           \
        }                                                                                          \
        map->buckets = (map_name##_node**)calloc(bucket_count, sizeof(map_name##_node*));          \
        if (!map->buckets) {                                                                       \
            free(map);                                                                             \
            perror("Failed to allocate memory for map buckets");                                   \
            return NULL;                                                                           \
        }                                                                                          \
        map->bucket_count = bucket_count;                                                          \
        map->size = 0;                                                                             \
        return map;                                                                                \
    }                                                                                              \
                                                                                                   \
    void map_name##_destroy(map_name* map) {                                                       \
        for (size_t i = 0; i < map->bucket_count; ++i) {                                           \
            map_name##_node* node = map->buckets[i];                                               \
            while (node) {                                                                         \
                map_name##_node* next = node->next;                                                \
                free(node);                                                                        \
                node = next;                                                                       \
            }                                                                                      \
        }                                                                                          \
        free(map->buckets);                                                                        \
        free(map);                                                                                 \
    }                                                                                              \
                                                                                                   \
    size_t map_name##_hash(key_type key, size_t bucket_count) {                                    \
        return (size_t)key % bucket_count;                                                         \
    }                                                                                              \
                                                                                                   \
    bool map_name##_resize(map_name* map, size_t new_bucket_count) {                               \
        map_name##_node** new_buckets =                                                            \
            (map_name##_node**)calloc(new_bucket_count, sizeof(map_name##_node*));                 \
        if (!new_buckets) {                                                                        \
            perror("Failed to allocate memory for new map buckets");                               \
            return false;                                                                          \
        }                                                                                          \
                                                                                                   \
        for (size_t i = 0; i < map->bucket_count; ++i) {                                           \
            map_name##_node* node = map->buckets[i];                                               \
            while (node) {                                                                         \
                size_t new_index = map_name##_hash(node->key, new_bucket_count);                   \
                map_name##_node* next = node->next;                                                \
                node->next = new_buckets[new_index];                                               \
                new_buckets[new_index] = node;                                                     \
                node = next;                                                                       \
            }                                                                                      \
        }                                                                                          \
        free(map->buckets);                                                                        \
        map->buckets = new_buckets;                                                                \
        map->bucket_count = new_bucket_count;                                                      \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    void map_name##_insert(map_name* map, key_type key, value_type value) {                        \
        if (map->size >= map->bucket_count) {                                                      \
            bool ok = map_name##_resize(map, map->bucket_count * 2);                               \
            if (!ok) {                                                                             \
                perror("Failed to resize map");                                                    \
                return;                                                                            \
            }                                                                                      \
        }                                                                                          \
        size_t index = map_name##_hash(key, map->bucket_count);                                    \
        map_name##_node* new_node = (map_name##_node*)malloc(sizeof(map_name##_node));             \
        if (!new_node) {                                                                           \
            perror("Failed to allocate memory for new map node");                                  \
            return;                                                                                \
        }                                                                                          \
        new_node->key = key;                                                                       \
        new_node->value = value;                                                                   \
        new_node->next = map->buckets[index];                                                      \
        map->buckets[index] = new_node;                                                            \
        map->size++;                                                                               \
    }                                                                                              \
                                                                                                   \
    value_type* map_name##_get(map_name* map, key_type key) {                                      \
        size_t index = map_name##_hash(key, map->bucket_count);                                    \
        map_name##_node* node = map->buckets[index];                                               \
        while (node) {                                                                             \
            if (node->key == key) {                                                                \
                return &node->value;                                                               \
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
        for (size_t i = 0; i < map->bucket_count; ++i) {                                           \
            map_name##_node* node = map->buckets[i];                                               \
            while (node) {                                                                         \
                map_name##_node* next = node->next;                                                \
                free(node);                                                                        \
                node = next;                                                                       \
            }                                                                                      \
            map->buckets[i] = NULL;                                                                \
        }                                                                                          \
        map->size = 0;                                                                             \
    }                                                                                              \
                                                                                                   \
    void map_name##_remove(map_name* map, key_type key) {                                          \
        size_t index = map_name##_hash(key, map->bucket_count);                                    \
        map_name##_node* node = map->buckets[index];                                               \
        map_name##_node* prev = NULL;                                                              \
        while (node) {                                                                             \
            if (node->key == key) {                                                                \
                if (prev) {                                                                        \
                    prev->next = node->next;                                                       \
                } else {                                                                           \
                    map->buckets[index] = node->next;                                              \
                }                                                                                  \
                free(node);                                                                        \
                map->size--;                                                                       \
                return;                                                                            \
            }                                                                                      \
            prev = node;                                                                           \
            node = node->next;                                                                     \
        }                                                                                          \
    }

#define MAP_FOR_EACH(map_name, map, iterator)                                                      \
    for (size_t i = 0; i < (map)->bucket_count; ++i)                                               \
        for (map_name##_node* iterator = (map)->buckets[i]; iterator; iterator = iterator->next)
