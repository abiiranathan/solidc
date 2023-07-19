#ifndef __PRIORITY_QUEUE_H__
#define __PRIORITY_QUEUE_H__

// Define the structure for a generic priority queue node
typedef struct PriorityQueueNode PriorityQueueNode;

// Define the structure for the priority queue
typedef struct PriorityQueue PriorityQueue;

// Create a new priority queue
PriorityQueue* queue_create(int capacity);

// Destroy the priority queue
void queue_destroy(PriorityQueue* queue);

// Insert an element into the priority queue
void queue_push(PriorityQueue* queue, void* data, int priority);

// Remove the element with the highest priority from the priority queue
void queue_pop(PriorityQueue* queue);

// Get the element with the highest priority from the priority queue
void* queue_top(const PriorityQueue* queue);

// Check if the priority queue is empty
int queue_is_empty(const PriorityQueue* queue);

// Returns the size of the priority queue
int queue_size(const PriorityQueue* queue);

#endif /* __PRIORITY_QUEUE_H__ */
