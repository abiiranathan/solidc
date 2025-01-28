#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Include the lock-free queue header.
#include "../include/lfqueue.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 1000
#define QUEUE_CAPACITY 100

// Structure to hold thread arguments
typedef struct {
    LfQueue* queue;
    int thread_id;
    int* total_produced;  // For producers
    int* total_consumed;  // For consumers
    _Atomic(bool)* done;  // Signals completion of producers
} ThreadArgs;

// Producer thread function
void* producer(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int items_produced = 0;

    // Seed random number generator differently for each thread
    srand(time(NULL) + args->thread_id);

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        int value = (args->thread_id * ITEMS_PER_PRODUCER) + i;

        // Try to enqueue until successful
        while (!queue_enqueue(args->queue, &value)) {
            // Small sleep if queue is full
            usleep(rand() % 1000);
        }

        items_produced++;

        // Simulate some work
        usleep(rand() % 1000);
    }

    // Atomically add to total_produced
    __atomic_fetch_add(args->total_produced, items_produced, __ATOMIC_SEQ_CST);

    printf("Producer %d finished, produced %d items\n", args->thread_id, items_produced);
    return NULL;
}

// Consumer thread function
void* consumer(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int items_consumed = 0;
    int value;

    // Seed random number generator differently for each thread
    srand(time(NULL) + args->thread_id + 100);

    while (true) {
        if (queue_dequeue(args->queue, &value)) {
            items_consumed++;

            // Simulate some work
            usleep(rand() % 2000);
        } else {
            // If queue is empty, check if producers are done
            if (atomic_load(args->done) && queue_size(args->queue) == 0) {
                break;
            }
            // Small sleep if queue is empty
            usleep(rand() % 1000);
        }
    }

    // Atomically add to total_consumed
    __atomic_fetch_add(args->total_consumed, items_consumed, __ATOMIC_SEQ_CST);

    printf("Consumer %d finished, consumed %d items\n", args->thread_id, items_consumed);
    return NULL;
}

int main() {
    // Initialize the queue
    LfQueue* queue = queue_init(sizeof(int), QUEUE_CAPACITY);
    if (!queue) {
        printf("Failed to create queue\n");
        return 1;
    }

    // Create thread arrays
    pthread_t producer_threads[NUM_PRODUCERS];
    pthread_t consumer_threads[NUM_CONSUMERS];
    ThreadArgs producer_args[NUM_PRODUCERS];
    ThreadArgs consumer_args[NUM_CONSUMERS];

    // Shared counters and done flag
    int total_produced = 0;
    int total_consumed = 0;
    _Atomic(bool) producers_done = false;

    // Start producer threads
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_args[i].queue = queue;
        producer_args[i].thread_id = i;
        producer_args[i].total_produced = &total_produced;
        producer_args[i].done = &producers_done;

        if (pthread_create(&producer_threads[i], NULL, producer, &producer_args[i]) != 0) {
            printf("Failed to create producer thread %d\n", i);
            return 1;
        }
    }

    // Start consumer threads
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_args[i].queue = queue;
        consumer_args[i].thread_id = i;
        consumer_args[i].total_consumed = &total_consumed;
        consumer_args[i].done = &producers_done;

        if (pthread_create(&consumer_threads[i], NULL, consumer, &consumer_args[i]) != 0) {
            printf("Failed to create consumer thread %d\n", i);
            return 1;
        }
    }

    // Wait for producers to finish
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producer_threads[i], NULL);
    }

    // Signal consumers that producers are done
    atomic_store(&producers_done, true);

    // Wait for consumers to finish
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumer_threads[i], NULL);
    }

    // Print results
    printf("\nFinal Results:\n");
    printf("Total items produced: %d\n", total_produced);
    printf("Total items consumed: %d\n", total_consumed);
    printf("Items should match:   %d\n", NUM_PRODUCERS * ITEMS_PER_PRODUCER);

    // Cleanup
    queue_destroy(queue);

    assert(total_consumed == total_produced);

    return 0;
}
