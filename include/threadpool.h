#ifndef C489A98F_8260_4B5A_98A2_432E6DFC62C5
#define C489A98F_8260_4B5A_98A2_432E6DFC62C5

// Must have C++ linkage. We can't put this in extern "C"
#ifdef __cplusplus
#include <atomic>
typedef std::atomic_flag atomic_flag;
#else
#include <stdatomic.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifndef INITIAL_TASK_CAPACITY
// Initial capacity of the task queue
#define INITIAL_TASK_CAPACITY 64
#endif

#ifndef MAX_THREADS
// Maximum number of worker threads
#define MAX_THREADS 8
#endif

// Cross-platform definition of the ThreadPool structure.
// Contains an array of threads, a queue of tasks, and synchronization primitives
// to coordinate the execution of tasks by the worker threads.
// To add a task to the thread pool, the user calls threadpool_add_task, which
// adds the task to the queue and signals one of the worker threads to execute it.
// The default number of worker threads is 8, but this can be changed by modifying
// the MAX_THREADS macro.
typedef struct ThreadPool ThreadPool;

// Create a thread pool with a fixed number of threads
ThreadPool* threadpool_create(void) __attribute__((warn_unused_result()));

// Add a task to the thread pool. The task will be executed by one of the worker threads
void threadpool_add_task(ThreadPool* pool, void (*function)(void*), void* argument);

// Wait for all tasks to be completed
void threadpool_wait(ThreadPool* pool);

// Destroy the thread pool and free all associated memory
void threadpool_destroy(ThreadPool* pool);

#if defined(__cplusplus)
}
#endif

#endif /* C489A98F_8260_4B5A_98A2_432E6DFC62C5 */
