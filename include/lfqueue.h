#ifndef __LF_QUEUE__
#define __LF_QUEUE__

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Lock-free Queue.
typedef struct LfQueue LfQueue;

// Initialize a new lock-free queue
LfQueue* queue_init(size_t element_size, size_t capacity);

// Enqueue an element (producer). Returns true on success
// or false if queue is full or memory allocation fails.
bool queue_enqueue(LfQueue* queue, const void* element);

// Dequeue an element (consumer)
bool queue_dequeue(LfQueue* queue, void* element);

// Free the queue and all remaining elements
void queue_destroy(LfQueue* queue);

// Returns the number of elements in the queue.
size_t queue_size(LfQueue* queue);

// Empty queue.
// Resets queue size to zero.
void queue_clear(LfQueue* queue);

// Returns the element size of the items in queue.
size_t queue_element_size(LfQueue* queue);

#endif
