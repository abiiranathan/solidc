#include "priority_queue.h"
#include <stdio.h>
#include <stdlib.h>

// Define the structure for a generic priority queue node
typedef struct PriorityQueueNode {
    void* data;
    int priority;
} PriorityQueueNode;

// Define the structure for the priority queue
typedef struct PriorityQueue {
    PriorityQueueNode* nodes;
    int capacity;
    int size;
} PriorityQueue;

// Function to create a new priority queue
PriorityQueue* queue_create(int capacity) {
    PriorityQueue* queue = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    queue->nodes =
        (PriorityQueueNode*)malloc(capacity * sizeof(PriorityQueueNode));
    queue->capacity = capacity;
    queue->size     = 0;
    return queue;
}

// Function to destroy the priority queue
void queue_destroy(PriorityQueue* queue) {
    free(queue->nodes);
    free(queue);
}

// Function to swap two priority queue nodes
void swap(PriorityQueueNode* a, PriorityQueueNode* b) {
    PriorityQueueNode temp = *a;
    *a                     = *b;
    *b                     = temp;
}

// Function to heapify the priority queue (downward operation)
void heapify(PriorityQueue* queue, int index) {
    int smallest = index;
    int left     = 2 * index + 1;
    int right    = 2 * index + 2;

    if (left < queue->size &&
        queue->nodes[left].priority < queue->nodes[smallest].priority) {
        smallest = left;
    }

    if (right < queue->size &&
        queue->nodes[right].priority < queue->nodes[smallest].priority) {
        smallest = right;
    }

    if (smallest != index) {
        swap(&queue->nodes[index], &queue->nodes[smallest]);
        heapify(queue, smallest);
    }
}

// Function to insert an element into the priority queue
void queue_push(PriorityQueue* queue, void* data, int priority) {
    if (queue->size == queue->capacity) {
        // Increase the capacity of the priority queue if it is full
        queue->capacity *= 2;
        queue->nodes = (PriorityQueueNode*)realloc(
            queue->nodes, queue->capacity * sizeof(PriorityQueueNode));

        if (queue->nodes == NULL) {
            queue->capacity /= 2;  //reverse capacity assignment
            printf("queue_push(): memory realloc() failed!");
            return;
        }
    }

    PriorityQueueNode node;
    node.data     = data;
    node.priority = priority;

    int i = queue->size;
    queue->size++;

    // Find the correct position for the new element
    while (i > 0 && node.priority < queue->nodes[(i - 1) / 2].priority) {
        queue->nodes[i] = queue->nodes[(i - 1) / 2];
        i               = (i - 1) / 2;
    }

    queue->nodes[i] = node;
}

// Function to remove the element with the highest priority from the priority queue
void queue_pop(PriorityQueue* queue) {
    if (queue->size == 0) {
        return;
    }

    if (queue->size == 1) {
        queue->size--;
        return;
    }

    queue->nodes[0] = queue->nodes[queue->size - 1];
    queue->size--;

    heapify(queue, 0);
}

// Function to get the element with the highest priority from the priority queue
void* queue_top(const PriorityQueue* queue) {
    if (queue->size == 0) {
        return NULL;
    }

    return queue->nodes[0].data;
}

// Function to check if the priority queue is empty
int queue_is_empty(const PriorityQueue* queue) {
    return queue->size == 0;
}

// Function to get the size of the priority queue
int queue_size(const PriorityQueue* queue) {
    return queue->size;
}
