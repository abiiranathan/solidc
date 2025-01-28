#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/lfqueue.h"
#include "../include/threadpool.h"

#define DEFAULT_TASK_CAPACITY 1024

// Task structure
typedef struct {
    void (*task)(void* arg);  // Task function pointer
    void* arg;                // Argument for the task
} Task;

// ThreadPool structure
typedef struct ThreadPool {
    pthread_mutex_t mutex;     // Mutex to protect shared resources
    pthread_cond_t cond;       // Condition variable for task availability
    pthread_cond_t idle_cond;  // Condition variable for idle threads
    LfQueue* queue;
    pthread_t* threads;     // Array of threads
    size_t num_threads;     // Number of threads in the pool
    size_t active_threads;  // Count of currently active threads
    int stop;               // Flag to indicate the pool is stopping
} ThreadPool;

// Worker function to execute tasks
void* threadpool_worker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        // Wait until there are tasks or the pool is stopping
        while (queue_size(pool->queue) == 0 && !pool->stop) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        // If stop is set and no tasks remain, exit the loop
        if (pool->stop) {
            pool->active_threads = 0;
            // empty the queue to satisfy waiting condition.
            // otherwise threadpool_destroy will wait forever.
            queue_clear(pool->queue);

            pthread_cond_signal(&pool->idle_cond);
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        // Increment active thread count to track busy threads
        pool->active_threads++;
        pthread_mutex_unlock(&pool->mutex);

        // Fetch the next task from the queue
        Task task;
        if (queue_dequeue(pool->queue, &task)) {
            // Execute the task
            task.task(task.arg);
        }

        pthread_mutex_lock(&pool->mutex);
        pool->active_threads--;

        // Signal idle condition if no tasks are left and all threads are idle
        if (queue_size(pool->queue) == 0 && pool->active_threads == 0) {
            pthread_cond_signal(&pool->idle_cond);
        }

        pthread_mutex_unlock(&pool->mutex);
        printf("Active threads: %zu\n", pool->active_threads);
    }

    return NULL;
}

// Create a thread pool
ThreadPool* threadpool_create(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = 1;
    }

    ThreadPool* pool = malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("Failed to allocate memory for thread pool");
        return NULL;
    }

    LfQueue* queue = queue_init(sizeof(Task), DEFAULT_TASK_CAPACITY);
    if (!queue) {
        perror("error creating task queue");
        free(pool);
        return NULL;
    }

    pool->queue = queue;
    pool->num_threads = num_threads;
    pool->active_threads = 0;
    pool->stop = 0;

    // Initialize mutex and condition variables
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
    pthread_cond_init(&pool->idle_cond, NULL);

    pool->threads = malloc(num_threads * sizeof(pthread_t));
    if (!pool->threads) {
        perror("Failed to allocate memory for threads");
        free(pool);
        queue_destroy(queue);
        return NULL;
    }

    // Create threads
    for (size_t i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, threadpool_worker, pool) != 0) {
            perror("Failed to create thread");
            // Free resources and return NULL if thread creation fails
            free(pool->threads);
            free(pool);
            queue_destroy(queue);
            return NULL;
        }
    }
    return pool;
}

// Add a task to the pool
bool threadpool_add_task(ThreadPool* pool, void (*task)(void*), void* arg) {
    Task t = {.task = task, .arg = arg};
    if (queue_enqueue(pool->queue, &t)) {
        // Signal one waiting thread to pick up the task
        pthread_mutex_lock(&pool->mutex);
        pthread_cond_signal(&pool->cond);
        pthread_mutex_unlock(&pool->mutex);
    };

    return true;
}

// Stop all threads immediately.
void threadpool_stop(ThreadPool* pool) {
    pool->stop = true;
}

// Destroy the thread pool
void threadpool_destroy(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutex);

    // Wait until no tasks remain(or pool was stopped) and all threads are idle
    while (pool->active_threads > 0 || queue_size(pool->queue) > 0) {
        pthread_cond_wait(&pool->idle_cond, &pool->mutex);
    }

    // Set stop flag and wake up all threads
    pool->stop = 1;
    pthread_cond_broadcast(&pool->cond);

    pthread_mutex_unlock(&pool->mutex);

    // Join all threads
    for (size_t i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // Free allocated resources
    free(pool->threads);
    queue_destroy(pool->queue);

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    pthread_cond_destroy(&pool->idle_cond);

    free(pool);
}
