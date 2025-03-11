#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef THREADLOCAL_QUEUE_SIZE
#define THREADLOCAL_QUEUE_SIZE 4096
#endif

#ifndef RING_BUFFER_SIZE
#define RING_BUFFER_SIZE 4096  // Must be power of 2
#endif

#define IS_POWER_OF_2(n) ((n) > 0 && ((n) & ((n) - 1)) == 0)
#define STATIC_CHECK_POWER_OF_2(n) _Static_assert(IS_POWER_OF_2(n), #n " is not a power of 2")

STATIC_CHECK_POWER_OF_2(RING_BUFFER_SIZE);
STATIC_CHECK_POWER_OF_2(THREADLOCAL_QUEUE_SIZE);

// Opaque threadpool structure
typedef struct threadpool ThreadPool;

// Create and initialize a new threadpool with the given number of threads.
ThreadPool* threadpool_create(int num_threads);

// Add a new task to the threadpool. The function will be executed by one of
// the threads in the pool.
int threadpool_submit(ThreadPool* pool, void (*task)(void*), void* arg_p);

// Destroy the threadpool and free all resources associated with it.
// It will wait for all threads to finish their tasks before freeing them.
void threadpool_destroy(ThreadPool* pool);

// Get the number of active threads in the threadpool.
int threadpool_num_threads_working(ThreadPool* pool);

#ifdef __cplusplus
}
#endif

#endif  // __THREADPOOL_H__
