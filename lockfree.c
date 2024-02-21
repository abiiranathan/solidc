#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Node for the task queue
typedef struct TaskNode {
    void (*function)(void*);
    void* arg;
    struct TaskNode* next;
} TaskNode;

// Task queue
typedef struct {
    TaskNode* head;
    TaskNode* tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TaskQueue;

// ThreadPool structure
typedef struct {
    unsigned int num_threads;
    unsigned int num_working_threads;
    bool shutdown;
    TaskQueue* task_queue;
    pthread_t* threads;
} ThreadPool;

// Initialize a new task queue
TaskQueue* task_queue_init();
void task_queue_destroy(TaskQueue* queue);
void task_queue_push(TaskQueue* queue, void (*function)(void*), void* arg);
TaskNode* task_queue_pop(TaskQueue* queue);

// Thread function for ThreadPool
void* thread_function(void* arg);

// Create a new ThreadPool with the specified number of threads
ThreadPool* threadpool_create(unsigned int num_threads);
// Add a task to the ThreadPool
int threadpool_add_task(ThreadPool* pool, void (*function)(void*), void* arg);

// Wait for all tasks to complete and destroy the ThreadPool
void threadpool_wait(ThreadPool* pool);
// Destroy the ThreadPool, waiting for tasks to complete
void threadpool_destroy(ThreadPool* pool);

// Initialize a new task queue
TaskQueue* task_queue_init() {
    TaskQueue* queue = malloc(sizeof(TaskQueue));
    if (!queue) {
        perror("Failed to allocate memory for TaskQueue");
        exit(EXIT_FAILURE);
    }
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

// Destroy the task queue
void task_queue_destroy(TaskQueue* queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}

// Push a task into the task queue
void task_queue_push(TaskQueue* queue, void (*function)(void*), void* arg) {
    TaskNode* new_task = malloc(sizeof(TaskNode));
    if (!new_task) {
        perror("Failed to allocate memory for TaskNode");
        exit(EXIT_FAILURE);
    }
    new_task->function = function;
    new_task->arg      = arg;
    new_task->next     = NULL;

    pthread_mutex_lock(&queue->mutex);
    if (queue->tail) {
        queue->tail->next = new_task;
    } else {
        queue->head = new_task;
    }
    queue->tail = new_task;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

// Pop a task from the task queue
TaskNode* task_queue_pop(TaskQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    TaskNode* task = queue->head;
    if (task) {
        queue->head = task->next;
        if (!queue->head) {
            queue->tail = NULL;
        }
    }
    pthread_mutex_unlock(&queue->mutex);
    return task;
}

// Thread function for ThreadPool
void* thread_function(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    while (true) {
        pthread_mutex_lock(&pool->task_queue->mutex);
        while (!pool->shutdown && pool->task_queue->head == NULL) {
            pthread_cond_wait(&pool->task_queue->cond, &pool->task_queue->mutex);
        }
        if (pool->shutdown && pool->task_queue->head == NULL) {
            pthread_mutex_unlock(&pool->task_queue->mutex);
            break;
        }
        TaskNode* task = task_queue_pop(pool->task_queue);
        pthread_mutex_unlock(&pool->task_queue->mutex);
        if (task) {
            task->function(task->arg);
            free(task);
            __sync_fetch_and_sub(&pool->num_working_threads, 1);
        }
    }
    return NULL;
}

// Create a new ThreadPool with the specified number of threads
ThreadPool* threadpool_create(unsigned int num_threads) {
    ThreadPool* pool = malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("Failed to allocate memory for ThreadPool");
        exit(EXIT_FAILURE);
    }
    pool->num_threads         = num_threads;
    pool->num_working_threads = 0;
    pool->shutdown            = false;
    pool->threads             = malloc(num_threads * sizeof(pthread_t));
    if (!pool->threads) {
        perror("Failed to allocate memory for threads");
        exit(EXIT_FAILURE);
    }

    pool->task_queue = task_queue_init();

    for (unsigned int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_function, pool) != 0) {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
        __sync_fetch_and_add(&pool->num_working_threads, 1);
    }

    return pool;
}

// Add a task to the ThreadPool
int threadpool_add_task(ThreadPool* pool, void (*function)(void*), void* arg) {
    if (!pool || !function) {
        return -1;  // Invalid arguments
    }

    pthread_mutex_lock(&pool->task_queue->mutex);
    task_queue_push(pool->task_queue, function, arg);
    pthread_cond_signal(&pool->task_queue->cond);
    pthread_mutex_unlock(&pool->task_queue->mutex);

    return 0;
}

// Wait for all tasks to complete and destroy the ThreadPool
void threadpool_wait(ThreadPool* pool) {
    while (pool->num_working_threads > 0) {
        // Wait for all tasks to complete
    }
    threadpool_destroy(pool);
}

// Destroy the ThreadPool, waiting for tasks to complete
void threadpool_destroy(ThreadPool* pool) {
    if (!pool) {
        return;
    }

    pool->shutdown = true;
    pthread_cond_broadcast(&pool->task_queue->cond);

    for (unsigned int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    task_queue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool);
}

static void printf_wrapper(void* arg) {
    printf("%s", (char*)arg);
}

int main() {
    ThreadPool* pool = threadpool_create(4);

    for (int i = 0; i < 10; i++) {
        threadpool_add_task(pool, printf_wrapper, (void*)"Hello from task\n");
    }

    threadpool_wait(pool);

    return 0;
}
