#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef THREADLOCAL_QUEUE_SIZE
#define THREADLOCAL_QUEUE_SIZE 1024
#endif

// Opaque threadpool structure
typedef struct threadpool* ThreadPool;

// Create and initialize a new threadpool with the given number of threads.
ThreadPool threadpool_create(int num_threads);

// Add a new task to the threadpool. The function will be executed by one of the threads in the pool.
int threadpool_add_task(ThreadPool, void (*task)(void*), void* arg_p);

// Wait for all threads in the threadpool to finish the queued tasks.
void threadpool_wait(ThreadPool);

// Pause all threads in the threadpool.
// The threads will not execute any tasks until they are resumed.
void threadpool_pause(ThreadPool);

// Unpause all threads in the threadpool.
void threadpool_resume(ThreadPool);

// Destroy the threadpool and free all resources associated with it.
// It will wait for all threads to finish their tasks before freeing them.
void threadpool_destroy(ThreadPool);

// Get the number of active threads in the threadpool.
int threadpool_num_threads_working(ThreadPool);

#ifdef __cplusplus
}
#endif

#endif  // __THREADPOOL_H__
