#include "../include/threadpool.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct {
    void (*function)(void*);  // Function pointer to the task function
    void* argument;           // Argument to pass to the function
} Task;

typedef struct ThreadPool {
    Task* tasks;                    // Task queue
    size_t task_capacity;           // Task queue capacity
    atomic_size_t task_count;       // Number of tasks in the queue
    atomic_size_t task_index;       // Index of the next task to be executed
    atomic_size_t tasks_completed;  // Number of completed tasks
#ifdef _WIN32
    HANDLE threads[MAX_THREADS];   // Worker threads
    CRITICAL_SECTION lock;         // Critical section to protect the task queue
    CONDITION_VARIABLE cond;       // Condition variable to notify worker threads
    CONDITION_VARIABLE completed;  // Condition variable to notify the waiting thread
    volatile LONG stop;            // Flag to indicate that the thread pool should stop
#else
    pthread_t threads[MAX_THREADS];  // Worker threads
    atomic_flag lock;                // Spinlock to protect the task queue
    pthread_mutex_t mutex;           // Mutex to protect the task queue and the stop flag
    pthread_cond_t cond;             // Condition variable to notify worker threads
    pthread_cond_t completed;        // Condition variable to notify the waiting thread
    atomic_int stop;                 // Flag to indicate that the thread pool should stop
#endif
} ThreadPool;

#ifdef _WIN32
#else
void spinlock_acquire(atomic_flag* lock) {
    while (atomic_flag_test_and_set(lock)) {
        // Spin-wait (busy-wait)
    }
}

void spinlock_release(atomic_flag* lock) {
    atomic_flag_clear(lock);
}
#endif

#ifdef _WIN32
DWORD WINAPI thread_worker(LPVOID arg) {
#else
void* thread_worker(void* arg) {
#endif
    ThreadPool* pool = (ThreadPool*)arg;

    while (1) {
#ifdef _WIN32
        EnterCriticalSection(&pool->lock);

        // Wait for a task to be added to the queue or for the thread pool to stop
        while (atomic_load(&pool->task_index) == atomic_load(&pool->task_count) &&
               !InterlockedCompareExchange(&pool->stop, 0, 0)) {
            SleepConditionVariableCS(&pool->cond, &pool->lock, INFINITE);
        }

        // Check if the thread should stop
        if (InterlockedCompareExchange(&pool->stop, 0, 0)) {
            LeaveCriticalSection(&pool->lock);
            break;
        }

        // Get the next task from the queue
        size_t index = atomic_fetch_add(&pool->task_index, 1);

        LeaveCriticalSection(&pool->lock);
#else
        pthread_mutex_lock(&pool->mutex);

        // Wait for a task to be added to the queue or for the thread pool to stop
        while (atomic_load(&pool->task_index) == atomic_load(&pool->task_count) &&
               !atomic_load(&pool->stop)) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        // Check if the thread should stop
        if (atomic_load(&pool->stop)) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        // Get the next task from the queue
        size_t index = atomic_fetch_add(&pool->task_index, 1);

        pthread_mutex_unlock(&pool->mutex);
#endif

        // Execute the task
        Task task = pool->tasks[index];
        task.function(task.argument);

        // Mark the task as completed
        atomic_fetch_add(&pool->tasks_completed, 1);

#ifdef _WIN32
        EnterCriticalSection(&pool->lock);
        WakeConditionVariable(&pool->completed);
        LeaveCriticalSection(&pool->lock);
#else
        pthread_mutex_lock(&pool->mutex);
        pthread_cond_signal(&pool->completed);
        pthread_mutex_unlock(&pool->mutex);
#endif
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

// Create a thread pool with a fixed number of threads.
ThreadPool* threadpool_create(void) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("Failed to allocate memory for thread pool");
        return NULL;
    }

    // Allocate memory for the task queue
    pool->tasks = (Task*)malloc(sizeof(Task) * INITIAL_TASK_CAPACITY);
    if (!pool->tasks) {
        perror("Failed to allocate memory for task queue");
        free(pool);
        return NULL;
    }

    // Initialize the task queue and synchronization primitives
    pool->task_capacity = INITIAL_TASK_CAPACITY;
    atomic_init(&pool->task_count, 0);
    atomic_init(&pool->task_index, 0);
    atomic_init(&pool->tasks_completed, 0);
#ifdef _WIN32
    InitializeCriticalSection(&pool->lock);         // Initialize the critical section
    InitializeConditionVariable(&pool->cond);       // Initialize the condition variable
    InitializeConditionVariable(&pool->completed);  // Initialize the condition variable
    pool->stop = 0;
#else
    atomic_flag_clear(&pool->lock);             // Clear the spinlock
    atomic_init(&pool->stop, 0);                // Initialize the stop flag
    pthread_mutex_init(&pool->mutex, NULL);     // Initialize the mutex
    pthread_cond_init(&pool->cond, NULL);       // Initialize the condition variable
    pthread_cond_init(&pool->completed, NULL);  // Initialize the condition variable
#endif

    for (int i = 0; i < MAX_THREADS; i++) {
#ifdef _WIN32
        pool->threads[i] = CreateThread(NULL, 0, thread_worker, pool, 0, NULL);
        if (pool->threads[i] == NULL) {
            perror("Failed to create thread");
            // Clean up previously created threads
            for (int j = 0; j < i; j++) {
                TerminateThread(pool->threads[j], 0);
                CloseHandle(pool->threads[j]);
            }
            free(pool->tasks);
            free(pool);
            return NULL;
        }
#else
        if (pthread_create(&pool->threads[i], NULL, thread_worker, pool)) {
            perror("Failed to create thread");
            // Clean up previously created threads
            for (int j = 0; j < i; j++) {
                pthread_cancel(pool->threads[j]);
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->tasks);
            free(pool);
            return NULL;
        }
#endif
    }

    return pool;
}

// Add a task to the thread pool.
void threadpool_add_task(ThreadPool* pool, void (*function)(void*), void* argument) {
#ifdef _WIN32
    EnterCriticalSection(&pool->lock);
#else
    spinlock_acquire(&pool->lock);
#endif

    // Check if the task queue is full and reallocate memory if needed
    size_t count = atomic_load(&pool->task_count);

    if (count == pool->task_capacity) {
        pool->task_capacity *= 2;
        pool->tasks = (Task*)realloc(pool->tasks, sizeof(Task) * pool->task_capacity);
        if (!pool->tasks) {
            perror("Failed to reallocate memory for task queue");
#ifdef _WIN32
            LeaveCriticalSection(&pool->lock);
#else
            spinlock_release(&pool->lock);
#endif
            return;
        }
    }

    // Add the task to the queue
    pool->tasks[count].function = function;
    pool->tasks[count].argument = argument;

    // Increment the task count atomically
    atomic_fetch_add(&pool->task_count, 1);

#ifdef _WIN32
    LeaveCriticalSection(&pool->lock);
    WakeConditionVariable(&pool->cond);
#else
    spinlock_release(&pool->lock);
    pthread_cond_signal(&pool->cond);
#endif
}

// Wait for all tasks to be completed by the worker threads.
void threadpool_wait(ThreadPool* pool) {
#ifdef _WIN32
    EnterCriticalSection(&pool->lock);
    while (atomic_load(&pool->tasks_completed) < atomic_load(&pool->task_count)) {
        SleepConditionVariableCS(&pool->completed, &pool->lock, INFINITE);
    }
    LeaveCriticalSection(&pool->lock);
#else
    pthread_mutex_lock(&pool->mutex);
    while (atomic_load(&pool->tasks_completed) < atomic_load(&pool->task_count)) {
        pthread_cond_wait(&pool->completed, &pool->mutex);
    }
    pthread_mutex_unlock(&pool->mutex);
#endif
}

// Destroy the thread pool and free all associated memory.
void threadpool_destroy(ThreadPool* pool) {
#ifdef _WIN32
    InterlockedExchange(&pool->stop, 1);    // Signal the worker threads to stop
    WakeAllConditionVariable(&pool->cond);  // Notify all worker threads

    for (int i = 0; i < MAX_THREADS; i++) {
        WaitForSingleObject(pool->threads[i], INFINITE);  // Wait for all worker threads to stop
        CloseHandle(pool->threads[i]);
    }

    DeleteCriticalSection(&pool->lock);
#else
    atomic_store(&pool->stop, 1);         // Signal the worker threads to stop
    pthread_cond_broadcast(&pool->cond);  // Notify all worker threads

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(pool->threads[i], NULL);  // Wait for all worker threads to stop
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    pthread_cond_destroy(&pool->completed);
#endif

    free(pool->tasks);
    free(pool);
}
