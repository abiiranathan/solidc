#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/lock.h"
#include "../include/thread.h"
#include "../include/threadpool.h"

#ifdef _WIN32
#include <windows.h>
#define usleep(microSec) Sleep(microSec / 1e6);
#else
#include <unistd.h>
#endif

#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

// Task represents a work item that should be executed by a thread
typedef struct Task {
    void (*function)(void* arg);  // Function to be executed
    void* arg;                    // Argument to be passed to the function
} Task;

// Ring buffer for tasks
typedef struct TaskRingBuffer {
    Task tasks[RING_BUFFER_SIZE];
    volatile unsigned int head;  // Producer index
    volatile unsigned int tail;  // Consumer index
    Lock mutex;                  // Protects ring buffer access
    Condition not_empty;         // Signaled when items are added
    Condition not_full;          // Signaled when items are removed
    struct threadpool* pool;     // Pointer to owning threadpool
} TaskRingBuffer;

// Thread represents a cross-platform wrapper around a thread
typedef struct thread {
    int id;                   // Thread ID
    struct threadpool* pool;  // Pointer to the thread pool
    Thread pthread;           // Thread handle
    TaskRingBuffer queue;     // Per-thread ring buffer
} thread;

// Threadpool represents a thread pool that manages a group of threads
typedef struct threadpool {
    thread** threads;                  // Array of threads
    int num_threads;                   // Total number of threads
    volatile int num_threads_alive;    // Number of threads currently alive
    volatile int num_threads_working;  // Number of threads currently working
    Lock thcount_lock;                 // Protects thread counts
    Condition threads_all_idle;        // Signaled when all threads are idle
    TaskRingBuffer queue;              // Global task queue
    volatile int shutdown;             // Shutdown flag
} threadpool;

// Initialize ring buffer
static int ringbuffer_init(TaskRingBuffer* rb) {
    rb->head = rb->tail = 0;
    lock_init(&rb->mutex);
    cond_init(&rb->not_empty);
    cond_init(&rb->not_full);
    rb->pool = NULL;
    return 0;
}

// Push task to ring buffer
static int ringbuffer_push(TaskRingBuffer* rb, Task* task) {
    lock_acquire(&rb->mutex);

    while (((rb->head + 1) & RING_BUFFER_MASK) == rb->tail) {
        // Buffer is full
        if (rb->pool->shutdown) {
            lock_release(&rb->mutex);
            return -1;
        }
        cond_wait(&rb->not_full, &rb->mutex);
    }

    rb->tasks[rb->head] = *task;
    rb->head            = (rb->head + 1) & RING_BUFFER_MASK;

    cond_signal(&rb->not_empty);
    lock_release(&rb->mutex);
    return 0;
}

// Pull task from ring buffer
static int ringbuffer_pull(TaskRingBuffer* rb, Task* task) {
    lock_acquire(&rb->mutex);

    while (rb->head == rb->tail) {
        // Buffer is empty
        if (rb->pool->shutdown) {
            lock_release(&rb->mutex);
            return -1;
        }
        cond_wait(&rb->not_empty, &rb->mutex);
    }

    *task    = rb->tasks[rb->tail];
    rb->tail = (rb->tail + 1) & RING_BUFFER_MASK;

    cond_signal(&rb->not_full);
    lock_release(&rb->mutex);
    return 0;
}

// Destroy ring buffer
static void ringbuffer_destroy(TaskRingBuffer* rb) {
    lock_free(&rb->mutex);
    cond_free(&rb->not_empty);
    cond_free(&rb->not_full);
}

// Check if any thread queues have tasks
static int has_tasks_in_thread_queues(threadpool* pool) {
    for (int i = 0; i < pool->num_threads; i++) {
        if (pool->threads[i]->queue.head != pool->threads[i]->queue.tail) {
            return 1;
        }
    }
    return 0;
}

// Try to steal a task from another thread's queue
static int thread_steal_task(threadpool* pool, thread* thief, Task* task) {
    int start_idx = rand() % pool->num_threads;

    for (int i = 0; i < pool->num_threads; i++) {
        int idx        = (start_idx + i) % pool->num_threads;
        thread* target = pool->threads[idx];

        if (target == thief)
            continue;

        lock_acquire(&target->queue.mutex);
        if (target->queue.head == target->queue.tail) {
            lock_release(&target->queue.mutex);
            continue;
        }

        *task              = target->queue.tasks[target->queue.tail];
        target->queue.tail = (target->queue.tail + 1) & RING_BUFFER_MASK;

        cond_signal(&target->queue.not_full);
        lock_release(&target->queue.mutex);
        return 0;
    }
    return -1;
}

// Thread worker function
static void* thread_worker(void* arg) {
    thread* thp      = (thread*)arg;
    threadpool* pool = thp->pool;
    Task task;

    lock_acquire(&pool->thcount_lock);
    pool->num_threads_alive++;
    lock_release(&pool->thcount_lock);

    while (1) {
        if (pool->shutdown)
            break;

        // Try local queue first
        if (ringbuffer_pull(&thp->queue, &task) == 0) {
            goto execute;
        }

        // Try global queue
        if (ringbuffer_pull(&pool->queue, &task) == 0) {
            goto execute;
        }

        // Try stealing
        if (thread_steal_task(pool, thp, &task) == 0) {
            goto execute;
        }

        continue;

    execute:
        lock_acquire(&pool->thcount_lock);
        pool->num_threads_working++;
        lock_release(&pool->thcount_lock);

        task.function(task.arg);

        lock_acquire(&pool->thcount_lock);
        pool->num_threads_working--;
        if (pool->num_threads_working == 0 && !has_tasks_in_thread_queues(pool) &&
            pool->queue.head == pool->queue.tail) {
            cond_broadcast(&pool->threads_all_idle);
        }
        lock_release(&pool->thcount_lock);
    }

    lock_acquire(&pool->thcount_lock);
    pool->num_threads_alive--;
    if (pool->num_threads_alive == 0) {
        cond_broadcast(&pool->threads_all_idle);
    }
    lock_release(&pool->thcount_lock);
    return NULL;
}

// Initialize thread
static int thread_init(threadpool* pool, thread** t, int id) {
    *t = (thread*)malloc(sizeof(thread));
    if (*t == NULL)
        return -1;

    (*t)->pool = pool;
    (*t)->id   = id;

    if (ringbuffer_init(&(*t)->queue) != 0) {
        free(*t);
        return -1;
    }
    (*t)->queue.pool = pool;

    return thread_create(&(*t)->pthread, thread_worker, *t);
}

// Create thread pool
threadpool* threadpool_create(int num_threads) {
    if (num_threads <= 0)
        num_threads = 1;

    srand((unsigned int)time(NULL));

    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (pool == NULL)
        return NULL;

    pool->num_threads_alive   = 0;
    pool->num_threads_working = 0;
    pool->shutdown            = 0;
    pool->num_threads         = num_threads;

    if (ringbuffer_init(&pool->queue) != 0) {
        free(pool);
        return NULL;
    }
    pool->queue.pool = pool;

    pool->threads = (thread**)malloc((size_t)num_threads * sizeof(thread*));
    if (pool->threads == NULL) {
        ringbuffer_destroy(&pool->queue);
        free(pool);
        return NULL;
    }

    lock_init(&pool->thcount_lock);
    cond_init(&pool->threads_all_idle);

    for (int i = 0; i < num_threads; i++) {
        if (thread_init(pool, &pool->threads[i], i) != 0) {
            threadpool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}

// Submit task to thread pool
int threadpool_submit(threadpool* pool, void (*function)(void*), void* arg) {
    Task task = {function, arg};

    // Try random thread's queue first
    int thread_id = rand() % pool->num_threads;
    thread* thp   = pool->threads[thread_id];

    if (ringbuffer_push(&thp->queue, &task) == 0) {
        return 0;
    }

    // If local queue is full, try global queue
    return ringbuffer_push(&pool->queue, &task);
}

// Destroy thread pool
void threadpool_destroy(threadpool* pool) {
    if (pool == NULL)
        return;

    lock_acquire(&pool->thcount_lock);
    while (pool->queue.head != pool->queue.tail || pool->num_threads_working > 0 || has_tasks_in_thread_queues(pool)) {
        cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    }
    pool->shutdown = 1;
    lock_release(&pool->thcount_lock);

    // Wake up all threads
    for (int i = 0; i < pool->num_threads; i++) {
        cond_broadcast(&pool->threads[i]->queue.not_empty);
        cond_broadcast(&pool->threads[i]->queue.not_full);
    }

    cond_broadcast(&pool->queue.not_empty);
    cond_broadcast(&pool->queue.not_full);

    // Wait for threads to exit
    while (pool->num_threads_alive > 0) {
        usleep(100);
    }

    // Clean up
    for (int i = 0; i < pool->num_threads; i++) {
        thread* thp = pool->threads[i];
        thread_join(thp->pthread, NULL);
        ringbuffer_destroy(&thp->queue);
        free(thp);
    }

    free(pool->threads);
    ringbuffer_destroy(&pool->queue);
    lock_free(&pool->thcount_lock);
    cond_free(&pool->threads_all_idle);
    free(pool);
}
