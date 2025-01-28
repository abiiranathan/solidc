#include "../include/lfqueue.h"
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

// Define the queue node structure
typedef struct Node {
    void* data;
    _Atomic(struct Node*) next;
} Node;

// Lock-free Queue structure.
typedef struct LfQueue {
    _Atomic(Node*) head;  // Head pointer for dequeue
    _Atomic(Node*) tail;  // Tail pointer for enqueue
    size_t element_size;  // Size of each element
    size_t capacity;      // Maximum number of elements
    atomic_size_t size;   // Current number of elements
} LfQueue;

// Initialize a new lock-free queue
LfQueue* queue_init(size_t element_size, size_t capacity) {
    LfQueue* queue = (LfQueue*)malloc(sizeof(LfQueue));
    if (!queue)
        return NULL;

    // Create sentinel node
    Node* sentinel = (Node*)malloc(sizeof(Node));
    if (!sentinel) {
        free(queue);
        return NULL;
    }

    sentinel->data = NULL;
    sentinel->next = NULL;

    // Initialize atomic pointers
    atomic_init(&queue->head, sentinel);
    atomic_init(&queue->tail, sentinel);
    atomic_init(&queue->size, 0);

    queue->element_size = element_size;
    queue->capacity = capacity;

    return queue;
}

// Enqueue an element (producer)
bool queue_enqueue(LfQueue* queue, const void* element) {
    if (!queue || !element)
        return false;

    // Check if queue is full
    size_t current_size = atomic_load(&queue->size);
    if (current_size >= queue->capacity)
        return false;

    // Create new node
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node)
        return false;

    // Allocate and copy data
    new_node->data = malloc(queue->element_size);
    if (!new_node->data) {
        free(new_node);
        return false;
    }
    memcpy(new_node->data, element, queue->element_size);
    new_node->next = NULL;

    // Add node to the queue
    Node* tail;
    while (true) {
        tail = atomic_load(&queue->tail);
        Node* next = atomic_load(&tail->next);

        if (next == NULL) {
            // Try to link the new node
            if (atomic_compare_exchange_weak(&tail->next, &next, new_node)) {
                break;
            }
        } else {
            // advance the tail pointer
            atomic_compare_exchange_weak(&queue->tail, &tail, next);
        }
    }

    // Advance tail pointer
    atomic_compare_exchange_weak(&queue->tail, &tail, new_node);
    atomic_fetch_add(&queue->size, 1);

    return true;
}

// Dequeue an element (consumer)
bool queue_dequeue(LfQueue* queue, void* element) {
    if (!queue || !element)
        return false;

    // Check if queue is empty
    if (atomic_load(&queue->size) == 0)
        return false;

    while (true) {
        Node* head = atomic_load(&queue->head);
        Node* tail = atomic_load(&queue->tail);
        Node* next = atomic_load(&head->next);

        if (head == tail) {
            if (next == NULL) {
                return false;  // Queue is empty
            }

            // Help advance the tail pointer
            atomic_compare_exchange_weak(&queue->tail, &tail, next);
            continue;
        }

        if (next) {
            // Copy data and advance head
            memcpy(element, next->data, queue->element_size);
            if (atomic_compare_exchange_weak(&queue->head, &head, next)) {
                // Free the dequeued node
                free(head->data);
                free(head);
                atomic_fetch_sub(&queue->size, 1);
                return true;
            }
        }
    }
}

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
