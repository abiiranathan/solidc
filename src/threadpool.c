#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdatomic.h>
#include <stdint.h>
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
#ifndef BATCH_SIZE
// Increased batch size for better amortization
#define BATCH_SIZE 16
#endif

#define SPIN_LIMIT 1000  // Number of spins before yielding
#define YIELD_LIMIT 100  // Number of yields before sleeping

// Task represents a work item that should be executed by a thread
typedef struct Task {
    void (*function)(void* arg);  // Function to be executed
    void* arg;                    // Argument to be passed to the function
} Task;

// Lock-free ring buffer for tasks
typedef struct TaskRingBuffer {
    Task tasks[RING_BUFFER_SIZE];
    atomic_uint head;              // Producer index (atomic)
    atomic_uint tail;              // Consumer index (atomic)
    Lock mutex;                    // Only for condition variables
    Condition not_empty;           // Signaled when items are added
    Condition not_full;            // Signaled when items are removed
    struct threadpool* pool;       // Pointer to owning threadpool
    atomic_int waiting_consumers;  // Number of threads waiting for tasks
} TaskRingBuffer;

// Thread represents a cross-platform wrapper around a thread
typedef struct thread {
    int id;                    // Thread ID
    struct threadpool* pool;   // Pointer to the thread pool
    Thread pthread;            // Thread handle
    TaskRingBuffer queue;      // Per-thread ring buffer
    uint32_t steal_seed;       // Fast random seed for work stealing
    atomic_int local_pending;  // Cache for local queue size
} thread;

// Threadpool represents a thread pool that manages a group of threads
typedef struct threadpool {
    thread** threads;                // Array of threads
    int num_threads;                 // Total number of threads
    atomic_int num_threads_alive;    // Number of threads currently alive
    atomic_int num_threads_working;  // Number of threads currently working
    Lock thcount_lock;               // Protects thread counts (reduced usage)
    Condition threads_all_idle;      // Signaled when all threads are idle
    TaskRingBuffer queue;            // Global task queue
    atomic_int shutdown;             // Shutdown flag (atomic)
    atomic_int total_pending_tasks;  // Total tasks across all queues
} threadpool;

// Fast random number generator (xorshift32)
static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// Initialize ring buffer
static int ringbuffer_init(TaskRingBuffer* rb) {
    atomic_store(&rb->head, 0);
    atomic_store(&rb->tail, 0);
    atomic_store(&rb->waiting_consumers, 0);
    lock_init(&rb->mutex);
    cond_init(&rb->not_empty);
    cond_init(&rb->not_full);
    rb->pool = NULL;
    return 0;
}

// Lock-free push task to ring buffer (with fallback to blocking)
static int ringbuffer_push(TaskRingBuffer* rb, Task* task) {
    uint32_t head      = atomic_load(&rb->head);
    uint32_t next_head = (head + 1) & RING_BUFFER_MASK;

    // Try lock-free push first
    if (next_head != atomic_load(&rb->tail)) {
        rb->tasks[head] = *task;
        atomic_store(&rb->head, next_head);

        // Only signal if there are waiting consumers
        if (atomic_load(&rb->waiting_consumers) > 0) {
            lock_acquire(&rb->mutex);
            cond_signal(&rb->not_empty);
            lock_release(&rb->mutex);
        }
        return 0;
    }

    // Fallback to blocking push
    lock_acquire(&rb->mutex);
    while (((atomic_load(&rb->head) + 1) & RING_BUFFER_MASK) == atomic_load(&rb->tail)) {
        if (atomic_load(&rb->pool->shutdown)) {
            lock_release(&rb->mutex);
            return -1;
        }
        cond_wait(&rb->not_full, &rb->mutex);
    }

    head            = atomic_load(&rb->head);
    rb->tasks[head] = *task;
    atomic_store(&rb->head, (head + 1) & RING_BUFFER_MASK);
    cond_signal(&rb->not_empty);
    lock_release(&rb->mutex);
    return 0;
}

// Lock-free batch pull with spinning and yielding
static int ringbuffer_pull_batch_optimized(TaskRingBuffer* rb, Task* tasks, int max_tasks) {
    int spin_count  = 0;
    int yield_count = 0;

    while (1) {
        uint32_t tail = atomic_load(&rb->tail);
        uint32_t head = atomic_load(&rb->head);

        if (head != tail) {
            // Calculate available tasks
            int available = (int)((head - tail) & RING_BUFFER_MASK);
            int to_take   = (available < max_tasks) ? available : max_tasks;

            // Try to reserve tasks atomically
            uint32_t new_tail = (tail + to_take) & RING_BUFFER_MASK;
            if (atomic_compare_exchange_weak(&rb->tail, &tail, new_tail)) {
                // Successfully reserved tasks, now copy them
                for (int i = 0; i < to_take; i++) {
                    tasks[i] = rb->tasks[(tail + i) & RING_BUFFER_MASK];
                }

                // Signal producers if buffer was full
                if (((tail + RING_BUFFER_SIZE - 1) & RING_BUFFER_MASK) == head) {
                    lock_acquire(&rb->mutex);
                    cond_signal(&rb->not_full);
                    lock_release(&rb->mutex);
                }

                return to_take;
            }
            // CAS failed, retry
            continue;
        }

        // No tasks available, check shutdown
        if (atomic_load(&rb->pool->shutdown)) {
            return -1;
        }

        // Progressive backoff: spin -> yield -> block
        if (spin_count < SPIN_LIMIT) {
            spin_count++;
// CPU pause/yield instruction
#if defined(__x86_64__) || defined(__i386__)
            __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield" ::: "memory");
#elif defined(__powerpc__) || defined(__ppc__)
            __asm__ __volatile__("or 27,27,27" ::: "memory");  // PowerPC yield hint
#elif defined(__arm__)
            __asm__ __volatile__("yield" ::: "memory");
#endif

            continue;
        }

        if (yield_count < YIELD_LIMIT) {
            yield_count++;
#ifdef _WIN32
            SwitchToThread();
#else
            sched_yield();
#endif
            continue;
        }

        // Block and wait
        atomic_fetch_add(&rb->waiting_consumers, 1);
        lock_acquire(&rb->mutex);

        // Double-check condition
        if (atomic_load(&rb->head) == atomic_load(&rb->tail) && !atomic_load(&rb->pool->shutdown)) {
            cond_wait(&rb->not_empty, &rb->mutex);
        }

        lock_release(&rb->mutex);
        atomic_fetch_sub(&rb->waiting_consumers, 1);

        // Reset counters for next iteration
        spin_count = yield_count = 0;
    }
}

// Destroy ring buffer
static void ringbuffer_destroy(TaskRingBuffer* rb) {
    lock_free(&rb->mutex);
    cond_free(&rb->not_empty);
    cond_free(&rb->not_full);
}

// Fast check if any thread queues have tasks (lock-free)
static inline int has_pending_tasks(threadpool* pool) {
    return atomic_load(&pool->total_pending_tasks) > 0;
}

// Update pending task count
static inline void update_pending_tasks(threadpool* pool, int delta) {
    atomic_fetch_add(&pool->total_pending_tasks, delta);
}

// Fast work stealing with minimal locking
static int thread_steal_tasks_fast(threadpool* pool, thread* thief, Task* tasks, int max_tasks) {
    // Use fast random to pick starting thread
    int start_idx = xorshift32(&thief->steal_seed) % pool->num_threads;

    for (int i = 0; i < pool->num_threads; i++) {
        int idx        = (start_idx + i) % pool->num_threads;
        thread* target = pool->threads[idx];

        if (target == thief)
            continue;

        // Quick check without locking
        if (atomic_load(&target->local_pending) == 0)
            continue;

        // Try lock-free steal first
        uint32_t tail = atomic_load(&target->queue.tail);
        uint32_t head = atomic_load(&target->queue.head);

        if (head == tail)
            continue;

        // Calculate how many to steal (at most half)
        int available   = (int)((head - tail) & RING_BUFFER_MASK);
        int steal_count = (available + 1) / 2;
        if (steal_count > max_tasks)
            steal_count = max_tasks;
        if (steal_count == 0)
            continue;

        // Try to steal atomically
        uint32_t new_tail = (tail + steal_count) & RING_BUFFER_MASK;
        if (atomic_compare_exchange_weak(&target->queue.tail, &tail, new_tail)) {
            // Successfully stole tasks
            for (int j = 0; j < steal_count; j++) {
                tasks[j] = target->queue.tasks[(tail + j) & RING_BUFFER_MASK];
            }

            // Update pending count
            atomic_fetch_sub(&target->local_pending, steal_count);
            update_pending_tasks(pool, -steal_count);

            return steal_count;
        }
    }
    return 0;
}

// Optimized thread worker function
static void* thread_worker(void* arg) {
    thread* thp      = (thread*)arg;
    threadpool* pool = thp->pool;
    Task batch[BATCH_SIZE];
    int batch_count = 0;

    // Initialize fast random seed
    thp->steal_seed = (uint32_t)(time(NULL) ^ (uintptr_t)thp);

    atomic_fetch_add(&pool->num_threads_alive, 1);

    while (!atomic_load(&pool->shutdown)) {
        batch_count = 0;

        // Try local queue first (lock-free batch)
        batch_count = ringbuffer_pull_batch_optimized(&thp->queue, batch, BATCH_SIZE);
        if (batch_count > 0) {
            atomic_fetch_sub(&thp->local_pending, batch_count);
            update_pending_tasks(pool, -batch_count);
            goto execute_batch;
        }

        // Try global queue (lock-free batch)
        batch_count = ringbuffer_pull_batch_optimized(&pool->queue, batch, BATCH_SIZE);
        if (batch_count > 0) {
            update_pending_tasks(pool, -batch_count);
            goto execute_batch;
        }

        // Try work stealing (fast)
        batch_count = thread_steal_tasks_fast(pool, thp, batch, BATCH_SIZE);
        if (batch_count > 0) {
            goto execute_batch;
        }

        // No work found, continue to next iteration
        continue;

    execute_batch:
        atomic_fetch_add(&pool->num_threads_working, 1);

        // Execute all tasks in the batch
        for (int i = 0; i < batch_count; i++) {
            batch[i].function(batch[i].arg);
        }

        int working = atomic_fetch_sub(&pool->num_threads_working, 1) - 1;

        // Only check for idle condition if this was the last working thread
        if (working == 0 && !has_pending_tasks(pool)) {
            lock_acquire(&pool->thcount_lock);
            // Double-check under lock
            if (atomic_load(&pool->num_threads_working) == 0 && !has_pending_tasks(pool)) {
                cond_broadcast(&pool->threads_all_idle);
            }
            lock_release(&pool->thcount_lock);
        }
    }

    int alive = atomic_fetch_sub(&pool->num_threads_alive, 1) - 1;
    if (alive == 0) {
        lock_acquire(&pool->thcount_lock);
        cond_broadcast(&pool->threads_all_idle);
        lock_release(&pool->thcount_lock);
    }
    return NULL;
}

// Initialize thread
static int thread_init(threadpool* pool, thread** t, int id) {
    *t = (thread*)malloc(sizeof(thread));
    if (*t == NULL)
        return -1;

    (*t)->pool = pool;
    (*t)->id   = id;
    atomic_store(&(*t)->local_pending, 0);

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

    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (pool == NULL)
        return NULL;

    atomic_store(&pool->num_threads_alive, 0);
    atomic_store(&pool->num_threads_working, 0);
    atomic_store(&pool->shutdown, 0);
    atomic_store(&pool->total_pending_tasks, 0);
    pool->num_threads = num_threads;

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

// Optimized task submission
int threadpool_submit(threadpool* pool, void (*function)(void*), void* arg) {
    Task task = {function, arg};

    // Use fast random to pick thread (avoid expensive system random)
    static atomic_uint submit_counter = 0;
    int thread_id                     = atomic_fetch_add(&submit_counter, 1) % pool->num_threads;
    thread* thp                       = pool->threads[thread_id];

    // Try local queue first
    if (ringbuffer_push(&thp->queue, &task) == 0) {
        atomic_fetch_add(&thp->local_pending, 1);
        update_pending_tasks(pool, 1);
        return 0;
    }

    // Try global queue
    if (ringbuffer_push(&pool->queue, &task) == 0) {
        update_pending_tasks(pool, 1);
        return 0;
    }

    return -1;  // All queues full
}

// Destroy thread pool
void threadpool_destroy(threadpool* pool) {
    if (pool == NULL)
        return;

    // Wait for all tasks to complete
    lock_acquire(&pool->thcount_lock);
    while (atomic_load(&pool->num_threads_working) > 0 || has_pending_tasks(pool)) {
        cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    }

    // Signal shutdown
    atomic_store(&pool->shutdown, 1);
    lock_release(&pool->thcount_lock);

    // Wake up all threads
    for (int i = 0; i < pool->num_threads; i++) {
        cond_broadcast(&pool->threads[i]->queue.not_empty);
        cond_broadcast(&pool->threads[i]->queue.not_full);
    }
    cond_broadcast(&pool->queue.not_empty);
    cond_broadcast(&pool->queue.not_full);

    // Wait for threads to exit
    while (atomic_load(&pool->num_threads_alive) > 0) {
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
