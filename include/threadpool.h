#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef THREADLOCAL_QUEUE_SIZE
#define THREADLOCAL_QUEUE_SIZE 1024
#endif

#ifndef RING_BUFFER_SIZE
#define RING_BUFFER_SIZE 4096  // Must be power of 2
#endif

STATIC_CHECK_POWER_OF_2(RING_BUFFER_SIZE);
STATIC_CHECK_POWER_OF_2(THREADLOCAL_QUEUE_SIZE);

// Opaque threadpool structure
typedef struct Threadpool Threadpool;

/** Task represents a work item that should be executed by a thread. */
typedef struct Task {
    void (*function)(void* arg); /** Function to be executed. */
    void* arg;                   /** Argument to be passed to the function. */
} Task;

// Create and initialize a new threadpool with the given number of threads.
Threadpool* threadpool_create(size_t num_threads);

// Add a new task to the threadpool. The function will be executed by one of
// the threads in the pool.
bool threadpool_submit(Threadpool* pool, void (*task)(void*), void* arg_p);

/**
 * Zero-allocation batch task submission API.
 * Caller provides pre-allocated task array, eliminating all internal allocations.
 * @param pool The threadpool.
 * @param tasks Array of tasks to submit.
 * @param count Number of tasks in the array.
 * @return Number of tasks successfully submitted, -1 on error.
 */
int threadpool_submit_batch(Threadpool* pool, const Task* tasks, size_t count);

// Destroy the threadpool and free all resources associated with it.
// It will wait for all threads to finish their tasks before freeing them.
// @param pool Thread pool instance
// @param timeout_ms Number of milliseconds to wait for before forceful exit (-1) to wait
// indefinitely.
void threadpool_destroy(Threadpool* pool, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif  // __THREADPOOL_H__
