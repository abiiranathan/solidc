#include "priority_queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test the basic functionality of the priority queue
void testPriorityQueue() {
    PriorityQueue* queue = queue_create(10);

    assert(queue_is_empty(queue));
    assert(queue_size(queue) == 0);

    // Push elements with different priorities
    queue_push(queue, "Data 1", 3);
    queue_push(queue, "Data 2", 1);
    queue_push(queue, "Data 3", 5);
    queue_push(queue, "Data 4", 2);
    queue_push(queue, "Data 5", 4);

    assert(!queue_is_empty(queue));
    assert(queue_size(queue) == 5);

    // Test top() and pop()
    assert(strcmp(queue_top(queue), "Data 2") == 0);
    queue_pop(queue);
    assert(strcmp(queue_top(queue), "Data 4") == 0);
    queue_pop(queue);

    // Push more elements
    queue_push(queue, "Data 6", 1);
    queue_push(queue, "Data 7", 6);

    assert(queue_size(queue) == 5);

    // Test top() and pop() again
    assert(strcmp(queue_top(queue), "Data 6") == 0);
    queue_pop(queue);
    assert(strcmp(queue_top(queue), "Data 1") == 0);
    queue_pop(queue);

    // Clear the queue
    queue_destroy(queue);

    queue = queue_create(10);

    assert(queue_is_empty(queue));
    assert(queue_size(queue) == 0);
    queue_destroy(queue);
    printf("All tests passed!\n");
}

int main() {
    testPriorityQueue();
    return 0;
}
