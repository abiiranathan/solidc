#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef pthread_mutex_t Lock;
typedef pthread_cond_t Condition;
typedef pthread_t Thread;

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

// Helper functions
static void task_queue_init(TaskQueue* queue, threadpool* pool) {
    pthread_mutex_init(&queue->rwmutex, NULL);
    pthread_cond_init(&queue->has_jobs, NULL);
    queue->front = queue->rear = NULL;
    queue->len = 0;
    queue->pool = pool;
}

static void task_queue_push(TaskQueue* queue, Task* newtask) {
    pthread_mutex_lock(&queue->rwmutex);
    newtask->next = NULL;
    if (queue->rear == NULL) {
        queue->front = queue->rear = newtask;
    } else {
        queue->rear->next = newtask;
        queue->rear = newtask;
    }
    queue->len++;
    pthread_cond_signal(&queue->has_jobs);
    pthread_mutex_unlock(&queue->rwmutex);
}

static Task* task_queue_pop(TaskQueue* queue) {
    pthread_mutex_lock(&queue->rwmutex);
    Task* task = queue->front;
    if (task == NULL) {
        pthread_mutex_unlock(&queue->rwmutex);
        return NULL;
    }
    queue->front = task->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    queue->len--;
    pthread_mutex_unlock(&queue->rwmutex);
    return task;
}

static Task* task_queue_steal(TaskQueue* queue) {
    if (queue == NULL)
        return NULL;

    pthread_mutex_lock(&queue->rwmutex);
    Task* task = queue->rear;
    if (task == NULL) {
        pthread_mutex_unlock(&queue->rwmutex);
        return NULL;
    }
    if (queue->front == queue->rear) {
        queue->front = queue->rear = NULL;
    } else {
        Task* prev = queue->front;
        while (prev->next != queue->rear) {
            prev = prev->next;
        }
        queue->rear = prev;
        prev->next = NULL;
    }
    queue->len--;
    pthread_mutex_unlock(&queue->rwmutex);
    return task;
}

static void* worker(void* arg) {
    thread* this_thread = (thread*)arg;
    threadpool* pool = this_thread->pool;

    while (!pool->shutdown) {
        Task* task = NULL;

        // Try to get a task from the thread's local queue
        task = task_queue_pop(&this_thread->queue);

        // If local queue is empty, try to steal from global queue
        if (task == NULL) {
            task = task_queue_pop(&pool->queue);
        }

        // If global queue is empty, try to steal from other threads
        if (task == NULL) {
            for (int i = 0; i < pool->num_threads; i++) {
                if (i == this_thread->id)
                    continue;
                if (pool->threads[i] != NULL) {
                    task = task_queue_steal(&pool->threads[i]->queue);
                    if (task != NULL)
                        break;
                }
            }
        }

        if (task != NULL) {
            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_threads_working++;
            pthread_mutex_unlock(&pool->thcount_lock);

            // Execute the task
            task->function(task->arg);
            free(task);

            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_threads_working--;
            if (pool->num_threads_working == 0) {
                pthread_cond_signal(&pool->threads_all_idle);
            }
            pthread_mutex_unlock(&pool->thcount_lock);
        } else {
            // No tasks to execute, yield the CPU
            sched_yield();
        }
    }

    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads_alive--;
    pthread_mutex_unlock(&pool->thcount_lock);

    return NULL;
}

threadpool* threadpool_create(int num_threads) {
    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (pool == NULL) {
        return NULL;
    }

    srand(time(NULL));
    pool->num_threads = num_threads;
    pool->num_threads_alive = 0;
    pool->num_threads_working = 0;
    pool->shutdown = 0;

    pthread_mutex_init(&pool->thcount_lock, NULL);
    pthread_cond_init(&pool->threads_all_idle, NULL);
    task_queue_init(&pool->queue, pool);

    pool->threads = (thread**)malloc(num_threads * sizeof(thread*));
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }

    for (int i = 0; i < num_threads; i++) {
        pool->threads[i] = (thread*)malloc(sizeof(thread));
        if (pool->threads[i] == NULL) {
            // Clean up previously allocated threads
            for (int j = 0; j < i; j++) {
                free(pool->threads[j]);
            }
            free(pool->threads);
            free(pool);
            return NULL;
        }

        pool->threads[i]->id = i;
        pool->threads[i]->pool = pool;
        task_queue_init(&pool->threads[i]->queue, pool);

        if (pthread_create(&pool->threads[i]->pthread, NULL, worker, pool->threads[i]) != 0) {
            // Clean up on failure
            for (int j = 0; j <= i; j++) {
                free(pool->threads[j]);
            }
            free(pool->threads);
            free(pool);
            return NULL;
        }
        pool->num_threads_alive++;
    }

    return pool;
}

int threadpool_add_task(threadpool* pool, void (*function)(void*), void* arg) {
    Task* newtask = (Task*)malloc(sizeof(Task));
    if (newtask == NULL) {
        return -1;
    }

    newtask->function = function;
    newtask->arg = arg;

    // Randomly choose between global queue and thread-local queues
    int queue_choice = rand() % (pool->num_threads + 1);  // +1 for the global queue

    if (queue_choice == pool->num_threads) {
        // Add to global queue
        task_queue_push(&pool->queue, newtask);
        // printf("Added to global queue\n");
    } else {
        // Add to a specific thread's queue
        task_queue_push(&pool->threads[queue_choice]->queue, newtask);
        // printf("Added to thread %d's queue\n", queue_choice);
    }

    return 0;
}

void threadpool_wait(threadpool* pool) {
    pthread_mutex_lock(&pool->thcount_lock);
    while (pool->queue.len > 0 || pool->num_threads_working > 0) {
        pthread_cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    }
    pthread_mutex_unlock(&pool->thcount_lock);
}

void threadpool_destroy(threadpool* pool) {
    if (pool == NULL)
        return;

    pool->shutdown = 1;

    // Wake up all worker threads
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_cond_broadcast(&pool->threads[i]->queue.has_jobs);
    }

    // Wait for worker threads to finish
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i]->pthread, NULL);
        pthread_mutex_destroy(&pool->threads[i]->queue.rwmutex);
        pthread_cond_destroy(&pool->threads[i]->queue.has_jobs);
        free(pool->threads[i]);
    }

    // Clean up the pool
    pthread_mutex_destroy(&pool->thcount_lock);
    pthread_cond_destroy(&pool->threads_all_idle);
    pthread_mutex_destroy(&pool->queue.rwmutex);
    pthread_cond_destroy(&pool->queue.has_jobs);
    free(pool->threads);
    free(pool);
}
