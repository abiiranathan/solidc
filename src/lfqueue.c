#include "../include/lfqueue.h"

#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

/**
This is a bounded, lock-free, multi-producer, multi-consumer (MPMC) queue.
Uses a circular buffer with atomic indices.
One slot is always left empty to distinguish full/empty states.
Memory ordering ensures safe access across threads.
 */
typedef struct LfQueue {
    alignas(64) _Atomic size_t head;  // Align to prevent false sharing
    alignas(64) _Atomic size_t tail;
    size_t capacity;
    size_t element_size;
    void* buffer;  // Pre-allocated data buffer
} LfQueue;

LfQueue* queue_init(size_t element_size, size_t capacity) {
    LfQueue* queue = malloc(sizeof(LfQueue));
    if (!queue)
        return NULL;

    queue->buffer = malloc(element_size * capacity);
    if (!queue->buffer) {
        free(queue);
        return NULL;
    }

    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
    queue->capacity = capacity;
    queue->element_size = element_size;

    return queue;
}

bool queue_enqueue(LfQueue* queue, const void* element) {
    if (!queue || !element)
        return false;

    size_t current_tail, current_head, next_tail;
    do {
        current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
        current_head = atomic_load_explicit(&queue->head, memory_order_acquire);
        next_tail = (current_tail + 1) % queue->capacity;

        if (next_tail == current_head)
            return false;  // Full
    } while (!atomic_compare_exchange_weak_explicit(&queue->tail, &current_tail, next_tail,
                                                    memory_order_release, memory_order_relaxed));

    void* dest = (char*)queue->buffer + (current_tail * queue->element_size);
    memcpy(dest, element, queue->element_size);
    return true;
}

bool queue_dequeue(LfQueue* queue, void* element) {
    if (!queue || !element)
        return false;

    size_t current_head, current_tail, next_head;
    do {
        current_head = atomic_load_explicit(&queue->head, memory_order_relaxed);
        current_tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

        if (current_head == current_tail)
            return false;  // Empty

        next_head = (current_head + 1) % queue->capacity;
    } while (!atomic_compare_exchange_weak_explicit(&queue->head, &current_head, next_head,
                                                    memory_order_release, memory_order_relaxed));

    const void* src = (const char*)queue->buffer + (current_head * queue->element_size);
    memcpy(element, src, queue->element_size);
    return true;
}

void queue_destroy(LfQueue* queue) {
    if (!queue)
        return;
    free(queue->buffer);
    free(queue);
}

size_t queue_size(LfQueue* queue) {
    size_t head = atomic_load(&queue->head);
    size_t tail = atomic_load(&queue->tail);
    return (tail >= head) ? (tail - head) : (queue->capacity - head + tail);
}

size_t queue_element_size(LfQueue* queue) {
    return queue->element_size;
}

void queue_clear(LfQueue* queue) {
    atomic_store(&queue->head, 0);
    atomic_store(&queue->tail, 0);
}
