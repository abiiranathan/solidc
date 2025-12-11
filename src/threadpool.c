#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/align.h"
#include "../include/aligned_alloc.h"
#include "../include/lock.h"
#include "../include/thread.h"
#include "../include/threadpool.h"

#ifdef _WIN32
#include <windows.h>
#define thread_yield() SwitchToThread()
#else
#include <sched.h>
#include <unistd.h>
#define thread_yield() sched_yield()
#endif

#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)
#ifndef BATCH_SIZE
#define BATCH_SIZE 64
#endif

#define CACHE_LINE_SIZE 64
#define YIELD_THRESHOLD 4

/** Align to cache line to prevent false sharing. */
#define CACHE_ALIGNED ALIGN(CACHE_LINE_SIZE)

typedef struct TaskRingBuffer {
    CACHE_ALIGNED atomic_uint head;
    CACHE_ALIGNED atomic_uint tail;
    CACHE_ALIGNED Task tasks[RING_BUFFER_SIZE];
    CACHE_ALIGNED Lock mutex;
    CACHE_ALIGNED atomic_int waiting_consumers;
    CACHE_ALIGNED struct Threadpool* pool;
    Condition not_empty;
    Condition not_full;
} TaskRingBuffer;

typedef struct thread {
    CACHE_ALIGNED TaskRingBuffer queue;
    CACHE_ALIGNED Thread pthread;
    struct Threadpool* pool;
} thread;

/** Threadpool with minimal cache footprint. */
struct Threadpool {
    // Core control atomics - minimal set
    CACHE_ALIGNED atomic_int shutdown;
    CACHE_ALIGNED atomic_int num_threads_alive;

    // Thread array and count (read-mostly)
    CACHE_ALIGNED thread** threads;
    CACHE_ALIGNED size_t num_threads;

    // Global queue
    CACHE_ALIGNED TaskRingBuffer queue;

    // Synchronization (cold path)
    CACHE_ALIGNED Lock thcount_lock;
    Condition threads_all_idle;
};

/** Initialize ring buffer with optimized defaults. */
static void ringbuffer_init(TaskRingBuffer* rb) {
    atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
    atomic_store_explicit(&rb->tail, 0, memory_order_relaxed);
    atomic_store_explicit(&rb->waiting_consumers, 0, memory_order_relaxed);

    lock_init(&rb->mutex);
    cond_init(&rb->not_empty);
    cond_init(&rb->not_full);
    rb->pool = NULL;
}

/** Lock-free push with minimal blocking fallback. */
static bool ringbuffer_push(TaskRingBuffer* rb, Task* task) {
    // Fast path: try lock-free push
    uint32_t head      = atomic_load_explicit(&rb->head, memory_order_acquire);
    uint32_t next_head = (head + 1) & RING_BUFFER_MASK;
    uint32_t tail      = atomic_load_explicit(&rb->tail, memory_order_acquire);

    if (next_head != tail) {
        rb->tasks[head] = *task;
        atomic_store_explicit(&rb->head, next_head, memory_order_release);

        // Only signal if consumers are actually waiting
        if (atomic_load_explicit(&rb->waiting_consumers, memory_order_acquire) > 0) {
            lock_acquire(&rb->mutex);
            cond_signal(&rb->not_empty);
            lock_release(&rb->mutex);
        }
        return true;
    }

    // Slow path: blocking push (queue full)
    lock_acquire(&rb->mutex);

    // Re-check condition under lock
    while (((atomic_load_explicit(&rb->head, memory_order_relaxed) + 1) & RING_BUFFER_MASK) ==
           atomic_load_explicit(&rb->tail, memory_order_relaxed)) {

        if (atomic_load_explicit(&rb->pool->shutdown, memory_order_acquire)) {
            lock_release(&rb->mutex);
            return false;
        }
        cond_wait(&rb->not_full, &rb->mutex);
    }

    head            = atomic_load_explicit(&rb->head, memory_order_relaxed);
    rb->tasks[head] = *task;
    atomic_store_explicit(&rb->head, (head + 1) & RING_BUFFER_MASK, memory_order_release);
    cond_signal(&rb->not_empty);
    lock_release(&rb->mutex);
    return true;
}

static int ringbuffer_pull_batch_optimized(TaskRingBuffer* rb, Task* tasks, size_t max_tasks) {
    int yield_count = 0;

    while (1) {
        // Single memory barrier for both loads
        uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
        uint32_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);  // relaxed since tail has acquire

        if (head != tail) {
            // Calculate available tasks
            size_t available = (head - tail) & RING_BUFFER_MASK;
            size_t to_take   = (available < max_tasks) ? available : max_tasks;

            // Atomic reservation with single CAS
            uint32_t new_tail = (tail + to_take) & RING_BUFFER_MASK;
            if (atomic_compare_exchange_weak_explicit(&rb->tail, &tail, new_tail, memory_order_release,
                                                      memory_order_relaxed)) {

                // Bulk copy tasks (optimized for cache)
                for (size_t i = 0; i < to_take; i++) {
                    tasks[i] = rb->tasks[(tail + i) & RING_BUFFER_MASK];
                }

                // Signal producers only if buffer was full
                if (available == RING_BUFFER_SIZE - 1) {
                    lock_acquire(&rb->mutex);
                    cond_signal(&rb->not_full);
                    lock_release(&rb->mutex);
                }

                return (int)to_take;
            }
            // CAS failed, retry without delay
            continue;
        }

        // Check shutdown (single load)
        if (atomic_load_explicit(&rb->pool->shutdown, memory_order_acquire)) {
            return -1;
        }

        // Reduced yielding to minimize cache pollution
        if (yield_count < YIELD_THRESHOLD) {
            yield_count++;
            thread_yield();
            continue;
        }

        // Block efficiently
        atomic_fetch_add_explicit(&rb->waiting_consumers, 1, memory_order_relaxed);

        lock_acquire(&rb->mutex);
        // Single check under lock
        if (atomic_load_explicit(&rb->head, memory_order_relaxed) ==
                atomic_load_explicit(&rb->tail, memory_order_relaxed) &&
            !atomic_load_explicit(&rb->pool->shutdown, memory_order_relaxed)) {
            cond_wait(&rb->not_empty, &rb->mutex);
        }
        lock_release(&rb->mutex);

        atomic_fetch_sub_explicit(&rb->waiting_consumers, 1, memory_order_relaxed);
        yield_count = 0;
    }
}

/** Destroy ring buffer. */
static void ringbuffer_destroy(TaskRingBuffer* rb) {
    lock_free(&rb->mutex);
    cond_free(&rb->not_empty);
    cond_free(&rb->not_full);
}

/** Check if pool has any work (minimal overhead). */
static inline int has_work(Threadpool* pool) {
    // Check global queue first (most common case)
    if (atomic_load_explicit(&pool->queue.head, memory_order_relaxed) !=
        atomic_load_explicit(&pool->queue.tail, memory_order_relaxed)) {
        return 1;
    }

    // Check thread queues
    for (size_t i = 0; i < pool->num_threads; i++) {
        if (atomic_load_explicit(&pool->threads[i]->queue.head, memory_order_relaxed) !=
            atomic_load_explicit(&pool->threads[i]->queue.tail, memory_order_relaxed)) {
            return 1;
        }
    }
    return 0;
}

/** Streamlined worker thread with minimal atomic operations. */
static void* thread_worker(void* arg) {
    thread* thp            = (thread*)arg;
    Threadpool* pool       = thp->pool;
    Task batch[BATCH_SIZE] = {0};
    int batch_count        = 0;

    atomic_fetch_add_explicit(&pool->num_threads_alive, 1, memory_order_relaxed);

    while (!atomic_load_explicit(&pool->shutdown, memory_order_acquire)) {
        // Try local queue first (better cache locality)
        batch_count = ringbuffer_pull_batch_optimized(&thp->queue, batch, BATCH_SIZE);
        if (batch_count > 0) {
            goto execute_batch;
        }

        // Try global queue
        batch_count = ringbuffer_pull_batch_optimized(&pool->queue, batch, BATCH_SIZE);
        if (batch_count > 0) {
            goto execute_batch;
        }

        // No work found - continue loop
        continue;

    execute_batch:
        // Execute batch without tracking
        for (int i = 0; i < batch_count; i++) {
            batch[i].function(batch[i].arg);
        }

        // Signal idle condition only after completing work
        if (!has_work(pool)) {
            lock_acquire(&pool->thcount_lock);
            // Double-check under lock
            if (!has_work(pool)) {
                cond_broadcast(&pool->threads_all_idle);
            }
            lock_release(&pool->thcount_lock);
        }
    }

    // Thread cleanup
    int alive = atomic_fetch_sub_explicit(&pool->num_threads_alive, 1, memory_order_acq_rel) - 1;
    if (alive == 0) {
        lock_acquire(&pool->thcount_lock);
        cond_broadcast(&pool->threads_all_idle);
        lock_release(&pool->thcount_lock);
    }

    return NULL;
}

/** Initialize a worker thread. */
static int thread_init(Threadpool* pool, thread** t) {
    *t = (thread*)ALIGNED_ALLOC(CACHE_LINE_SIZE, sizeof(thread));
    if (*t == NULL) return -1;

    (*t)->pool = pool;

    ringbuffer_init(&(*t)->queue);
    (*t)->queue.pool = pool;

    return thread_create(&(*t)->pthread, thread_worker, *t);
}

/** Create thread pool with specified number of threads. */
Threadpool* threadpool_create(size_t num_threads) {
    if (num_threads == 0) num_threads = 1;

    Threadpool* pool = (Threadpool*)ALIGNED_ALLOC(CACHE_LINE_SIZE, sizeof(Threadpool));
    if (pool == NULL) return NULL;

    // Initialize minimal atomics
    atomic_store_explicit(&pool->num_threads_alive, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->shutdown, 0, memory_order_relaxed);
    ringbuffer_init(&pool->queue);
    pool->num_threads = num_threads;
    pool->queue.pool  = pool;

    pool->threads = (thread**)malloc(num_threads * sizeof(thread*));
    if (pool->threads == NULL) {
        ringbuffer_destroy(&pool->queue);
        free(pool);
        return NULL;
    }

    lock_init(&pool->thcount_lock);
    cond_init(&pool->threads_all_idle);

    // Create worker threads
    for (size_t i = 0; i < num_threads; i++) {
        if (thread_init(pool, &pool->threads[i]) != 0) {
            threadpool_destroy(pool, -1);
            return NULL;
        }
    }
    return pool;
}

/** Submit task with simplified load balancing. */
bool threadpool_submit(Threadpool* pool, void (*function)(void*), void* arg) {
    if (pool == NULL || function == NULL) return -1;

    Task task = {function, arg};

    // Simple round-robin (no atomic counter)
    static _Thread_local size_t thread_counter = 0;
    size_t thread_id                           = (thread_counter++) % pool->num_threads;

    // Try local queue first
    if (ringbuffer_push(&pool->threads[thread_id]->queue, &task)) {
        return true;
    }

    // Fallback to global queue
    return ringbuffer_push(&pool->queue, &task);
}

/** Destroy thread pool efficiently. */
void threadpool_destroy(Threadpool* pool, int timeout_ms) {
    if (pool == NULL) return;

    // Wait for work completion
    lock_acquire(&pool->thcount_lock);

    int ret = 0;
    while (has_work(pool)) {
        ret = cond_wait_timeout(&pool->threads_all_idle, &pool->thcount_lock, timeout_ms);
        if (ret == -1) {
            perror("cond_wait_timeout");
        }
        break;
    }

    // Signal shutdown
    atomic_store_explicit(&pool->shutdown, 1, memory_order_release);
    lock_release(&pool->thcount_lock);

    // Wake all threads
    for (size_t i = 0; i < pool->num_threads; i++) {
        cond_broadcast(&pool->threads[i]->queue.not_empty);
        cond_broadcast(&pool->threads[i]->queue.not_full);
    }

    cond_broadcast(&pool->queue.not_empty);
    cond_broadcast(&pool->queue.not_full);

    // Wait for thread termination
    while (atomic_load_explicit(&pool->num_threads_alive, memory_order_acquire) > 0) {
        thread_yield();
    }

    // Cleanup
    for (size_t i = 0; i < pool->num_threads; i++) {
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
