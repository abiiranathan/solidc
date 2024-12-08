#include <stdio.h>
#include <stdlib.h>

#include "../include/lock.h"
#include "../include/thread.h"
#include "../include/threadpool.h"

#ifdef _WIN32
#define SLEEP(sec) Sleep((sec) * 1000)
#else
#define SLEEP(sec) sleep(sec)
#endif

// Job represents a work item that should be executed by a thread in the thread pool
typedef struct Task {
    struct Task* next;            // Pointer to the next job
    void (*function)(void* arg);  // Function to be executed
    void* arg;                    // Argument to be passed to the function
} Task;

// Job queue is a linked list of jobs that are waiting to be executed
typedef struct Taskqueue {
    Lock rwmutex;             // Mutex for the job queue
    Task* front;              // Pointer to the front of the queue
    Task* rear;               // Pointer to the rear of the queue
    Condition has_jobs;       // Semaphore to signal that there are jobs in the queue
    volatile int len;         // Number of jobs in the queue
    struct threadpool* pool;  // Pointer to owning threadpool
} TaskQueue;

// Thread represents a cross-platform wrapper around a thread
typedef struct thread {
    int id;                   // Thread ID
    struct threadpool* pool;  // Pointer to the thread pool
    Thread pthread;           // Thread handle

    // per-thread queue
    TaskQueue queue;
} thread;

// Threadpool represents a thread pool that manages a group of threads
typedef struct threadpool {
    thread** threads;                  // Array of threads
    int num_threads;                   // The total number of threads
    volatile int num_threads_alive;    // Number of threads that are currently alive
    volatile int num_threads_working;  // Number of threads that are currently working
    Lock thcount_lock;                 // Mutex for num_threads_alive
    Condition threads_all_idle;        // Condition variable for signaling idle threads
    TaskQueue queue;                   // Linked list for the queue of jobs
    volatile int shutdown;             // Flag to indicate that the thread pool should be shut down
} threadpool;

// ================ static functions declarations ====================
static int thread_init(threadpool* pool, thread** thp, int id);

static void* threadWorker(void* arg);
static void thread_destroy(thread* thp);
static int jobqueue_init(TaskQueue* jobqueue_p);
static void jobqueue_clear(TaskQueue* jobqueue_p);
static void jobqueue_push(TaskQueue* jobqueue_p, Task* newjob);
static Task* jobqueue_pull(TaskQueue* jobqueue_p);
static void jobqueue_destroy(TaskQueue* jobqueue_p);

// ============== end of prototypes ==========================

static int jobqueue_init(TaskQueue* jobqueue_p) {
    jobqueue_p->len = 0;
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    lock_init(&jobqueue_p->rwmutex);
    cond_init(&jobqueue_p->has_jobs);
    return 0;
}

// Clear all tasks from the job queue
static void jobqueue_clear(TaskQueue* jobqueue_p) {
    Task* task;
    while ((task = jobqueue_pull(jobqueue_p)) != NULL) {
        free(task);
    }
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    jobqueue_p->len = 0;
}

static void jobqueue_push(TaskQueue* jobqueue_p, Task* newjob) {
    lock_acquire(&jobqueue_p->rwmutex);
    newjob->next = NULL;

    if (jobqueue_p->rear == NULL) {
        jobqueue_p->front = newjob;
        jobqueue_p->rear = newjob;
    } else {
        jobqueue_p->rear->next = newjob;
        jobqueue_p->rear = newjob;
    }

    jobqueue_p->len++;
    cond_signal(&jobqueue_p->has_jobs);  // Notify waiting threads
    lock_release(&jobqueue_p->rwmutex);  // Release the lock
}

// Add this helper function
static int has_tasks_in_thread_queues(threadpool* pool) {
    for (int i = 0; i < pool->num_threads; i++) {
        if (pool->threads[i]->queue.len > 0) {
            return 1;
        }
    }
    return 0;
}

static Task* jobqueue_pull(TaskQueue* q) {
    if (q->pool->shutdown) {
        return NULL;
    }

    lock_acquire(&q->rwmutex);
    while (q->len == 0 && !q->pool->shutdown) {
        // Wait for a signal that there are jobs in the queue
        cond_wait(&q->has_jobs, &q->rwmutex);
    }

    Task* task = q->front;
    if (task) {
        q->front = task->next;
        q->len--;

        if (q->len == 0) {
            q->rear = NULL;
        }
    }

    lock_release(&q->rwmutex);
    return task;
}

static void jobqueue_destroy(TaskQueue* jobqueue_p) {
    jobqueue_clear(jobqueue_p);
    lock_free(&jobqueue_p->rwmutex);
    cond_free(&jobqueue_p->has_jobs);
}

static int thread_init(threadpool* pool, thread** t, int id) {
    *t = (thread*)malloc(sizeof(thread));
    if (*t == NULL) {
        fprintf(stderr, "thread_init(): Could not allocate memory for thread\n");
        return -1;
    }

    (*t)->pool = pool;
    (*t)->id = id;
    (*t)->queue.pool = pool;

    // Initialize the per-thread queue
    jobqueue_init(&(*t)->queue);

    return thread_create(&(*t)->pthread, threadWorker, (void*)*t);
}

static void thread_destroy(thread* thp) {
    if (thp == NULL) {
        return;
    }

    thread_join(thp->pthread, NULL);
    free(thp);
}

void* threadWorker(void* arg) {
    thread* thp = (thread*)arg;
    threadpool* pool = thp->pool;

    lock_acquire(&pool->thcount_lock);
    pool->num_threads_alive++;
    lock_release(&pool->thcount_lock);

    while (1) {
        lock_acquire(&pool->thcount_lock);
        int should_shutdown = pool->shutdown;
        lock_release(&pool->thcount_lock);

        if (should_shutdown) {
            break;
        }

        Task* task = NULL;

        // try jobs from local queue first
        task = jobqueue_pull(&thp->queue);

        // otherwise, try to steal from the global queue
        if (task == NULL) {
            task = jobqueue_pull(&pool->queue);
        }

        if (task) {
            // We have some work to do
            lock_acquire(&pool->thcount_lock);
            pool->num_threads_working++;
            lock_release(&pool->thcount_lock);

            // Execute the task function, passing in the argument
            task->function(task->arg);
            free(task);

            lock_acquire(&pool->thcount_lock);
            pool->num_threads_working--;
            int no_tasks = (pool->num_threads_working == 0 && !has_tasks_in_thread_queues(pool));
            lock_release(&pool->thcount_lock);

            // If we are done, wake up all threads for a clean exit.
            if (no_tasks) {
                cond_broadcast(&pool->threads_all_idle);
            }
        }
    }

    lock_acquire(&pool->thcount_lock);
    pool->num_threads_alive--;
    if (pool->num_threads_alive == 0) {
        cond_broadcast(&pool->threads_all_idle);
    }
    lock_release(&pool->thcount_lock);
    return NULL;
}

threadpool* threadpool_create(int num_threads) {
    if (num_threads <= 0) {
        num_threads = 1;
    }

    srand(time(NULL));

    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (pool == NULL) {
        fprintf(stderr, "threadpool_create(): Could not allocate memory for thread pool\n");
        return NULL;
    }

    pool->num_threads_alive = 0;
    pool->num_threads_working = 0;
    pool->shutdown = 0;
    pool->num_threads = num_threads;

    if (jobqueue_init(&pool->queue) == -1) {
        free(pool);
        return NULL;
    }

    // Store the pool on Queue
    pool->queue.pool = pool;

    pool->threads = (thread**)malloc(num_threads * sizeof(thread*));
    if (pool->threads == NULL) {
        fprintf(stderr, "threadpool_create(): Could not allocate memory for threads\n");
        jobqueue_destroy(&pool->queue);
        free(pool);
        return NULL;
    }

    // Initialize the mutex and condition variables.
    lock_init(&pool->thcount_lock);
    cond_init(&pool->threads_all_idle);

    for (int n = 0; n < num_threads; n++) {
        if (thread_init(pool, &pool->threads[n], n) != 0) {
            threadpool_destroy(pool);
            return NULL;
        }
    }

    // Wait for all threads to boot up
    while (pool->num_threads_alive != num_threads) {
        SLEEP(1);
    }
    return pool;
}

void threadpool_wait(threadpool* pool) {
    if (pool == NULL)
        return;

    lock_acquire(&pool->thcount_lock);
    while (pool->queue.len > 0 || pool->num_threads_working > 0 ||
           has_tasks_in_thread_queues(pool)) {
        cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    }
    lock_release(&pool->thcount_lock);
}

void threadpool_destroy(threadpool* pool) {
    if (pool == NULL)
        return;

    lock_acquire(&pool->thcount_lock);
    pool->shutdown = 1;
    lock_release(&pool->thcount_lock);

    // Signal all waiting threads
    cond_broadcast(&pool->threads_all_idle);

    for (int i = 0; i < pool->num_threads; i++) {
        lock_acquire(&pool->threads[i]->queue.rwmutex);
        cond_signal(&pool->threads[i]->queue.has_jobs);
        lock_release(&pool->threads[i]->queue.rwmutex);
    }

    lock_acquire(&pool->queue.rwmutex);
    cond_signal(&pool->queue.has_jobs);
    lock_release(&pool->queue.rwmutex);

    // Wait for all threads to exit
    while (pool->num_threads_alive > 0) {
        SLEEP(1);
    }

    // Clean up thread resources
    for (int i = 0; i < pool->num_threads; ++i) {
        thread_destroy(pool->threads[i]);
    }

    free(pool->threads);
    jobqueue_destroy(&pool->queue);
    lock_free(&pool->thcount_lock);
    cond_free(&pool->threads_all_idle);
    free(pool);
}

int threadpool_add_task(threadpool* pool, void (*function)(void*), void* arg) {
    Task* task = (Task*)malloc(sizeof(Task));
    if (task == NULL) {
        fprintf(stderr, "threadpool_add_task(): Could not allocate memory for task\n");
        return -1;
    }

    task->function = function;
    task->arg = arg;
    task->next = NULL;

    // Get a random thread to add the task to
    int thread_id = rand() % pool->num_threads;
    thread* thp = pool->threads[thread_id];

    // Check if the thread's queue is full
    lock_acquire(&thp->queue.rwmutex);
    int is_full = thp->queue.len >= THREADLOCAL_QUEUE_SIZE;
    lock_release(&thp->queue.rwmutex);

    if (!is_full) {
        // Add the task to the thread's queue
        jobqueue_push(&thp->queue, task);
    } else {
        // Add the task to the global queue
        jobqueue_push(&pool->queue, task);
    }

    return 0;
}
