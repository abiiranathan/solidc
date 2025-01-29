#include "../include/lfqueue.h"
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

typedef struct Node {
    void* data;
    alignas(64) _Atomic(struct Node*) next;  // Prevent false sharing
} Node;

typedef struct LfQueue {
    alignas(64) _Atomic(Node*) head;  // Cache line alignment
    alignas(64) _Atomic(Node*) tail;
    size_t element_size;
    size_t capacity;
    alignas(64) atomic_size_t size;  // Separate cache line
} LfQueue;

LfQueue* queue_init(size_t element_size, size_t capacity) {
    LfQueue* queue = malloc(sizeof(LfQueue));
    if (!queue)
        return NULL;

    Node* sentinel = malloc(sizeof(Node));
    if (!sentinel) {
        free(queue);
        return NULL;
    }

    sentinel->data = NULL;
    atomic_init(&sentinel->next, NULL);

    atomic_init(&queue->head, sentinel);
    atomic_init(&queue->tail, sentinel);
    atomic_init(&queue->size, 0);
    queue->element_size = element_size;
    queue->capacity = capacity;

    return queue;
}

bool queue_enqueue(LfQueue* queue, const void* element) {
    if (!queue || !element)
        return false;

    // Reserve slot atomically
    size_t current_size;
    do {
        current_size = atomic_load_explicit(&queue->size, memory_order_relaxed);
        if (current_size >= queue->capacity)
            return false;
    } while (!atomic_compare_exchange_weak_explicit(&queue->size, &current_size, current_size + 1,
                                                    memory_order_acquire, memory_order_relaxed));

    Node* new_node = malloc(sizeof(Node));
    if (!new_node) {
        atomic_fetch_sub_explicit(&queue->size, 1, memory_order_relaxed);
        return false;
    }

    new_node->data = malloc(queue->element_size);
    if (!new_node->data) {
        free(new_node);
        atomic_fetch_sub_explicit(&queue->size, 1, memory_order_relaxed);
        return false;
    }
    memcpy(new_node->data, element, queue->element_size);
    atomic_init(&new_node->next, NULL);

    Node* tail;
    while (1) {
        tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
        Node* next = atomic_load_explicit(&tail->next, memory_order_acquire);

        if (!next) {
            if (atomic_compare_exchange_weak_explicit(&tail->next, &next, new_node,
                                                      memory_order_release, memory_order_acquire))
                break;
        } else {
            atomic_compare_exchange_weak_explicit(&queue->tail, &tail, next, memory_order_release,
                                                  memory_order_acquire);
        }
    }

    atomic_compare_exchange_weak_explicit(&queue->tail, &tail, new_node, memory_order_release,
                                          memory_order_acquire);
    return true;
}

bool queue_dequeue(LfQueue* queue, void* element) {
    if (!queue || !element)
        return false;

    while (1) {
        Node* head = atomic_load_explicit(&queue->head, memory_order_acquire);
        Node* tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
        Node* next = atomic_load_explicit(&head->next, memory_order_acquire);

        if (head == tail) {
            if (!next)
                return false;
            atomic_compare_exchange_weak_explicit(&queue->tail, &tail, next, memory_order_release,
                                                  memory_order_acquire);
        } else {
            memcpy(element, next->data, queue->element_size);
            if (atomic_compare_exchange_weak_explicit(&queue->head, &head, next,
                                                      memory_order_release, memory_order_acquire)) {
                free(head->data);
                free(head);
                atomic_fetch_sub_explicit(&queue->size, 1, memory_order_release);
                return true;
            }
        }
    }
}

// Other functions remain similar with memory_order adjustments
// Free the queue and all remaining elements.
// This is not thread-safe.
void queue_destroy(LfQueue* queue) {
    if (!queue)
        return;

    // Free all remaining nodes
    Node* current = atomic_load(&queue->head);
    while (current) {
        Node* next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    free(queue);
    queue = NULL;
}

// Returns the number of elements in the queue.
size_t queue_size(LfQueue* queue) {
    return atomic_load(&queue->size);
}

// Empty queue.
// Resets queue size to zero.
void queue_clear(LfQueue* queue) {
    // set the size to zero
    atomic_fetch_sub(&queue->size, 0);
}

// Returns the element size of the items in queue.
size_t queue_element_size(LfQueue* queue) {
    return queue->element_size;
}
