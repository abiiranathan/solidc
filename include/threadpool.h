#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

// Opaque threadpool structure
typedef struct ThreadPool ThreadPool;

// Create and initialize a new threadpool with the given number of threads.
ThreadPool* threadpool_create(size_t num_threads);

// Add a new task to the threadpool. The function will be executed by one of the threads in the pool.
bool threadpool_add_task(ThreadPool* pool, void (*task)(void*), void* arg_p);

// Stop all threads immediately.
void threadpool_stop(ThreadPool* pool);

// Destroy the threadpool and free all resources associated with it.
// It will wait for all threads to finish their tasks before freeing them.
void threadpool_destroy(ThreadPool* pool);

#ifdef __cplusplus
}
#endif

#endif  // __THREADPOOL_H__
