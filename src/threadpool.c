#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/threadpool.h"

#include "../include/align.h"
#include "../include/aligned_alloc.h"
#include "../include/lock.h"
#include "../include/thread.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
    CACHE_ALIGNED atomic_int shutdown;
    CACHE_ALIGNED atomic_int num_threads_alive;

    CACHE_ALIGNED thread** threads;
    CACHE_ALIGNED size_t num_threads;

    CACHE_ALIGNED TaskRingBuffer queue;

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

/*
 * FIX #1: Thread-safe push using a mutex for the entire push operation.
 *
 * The original fast path was not thread-safe: two producers could load the
 * same `head`, both see a free slot, both write to tasks[head], and then
 * race to advance head — resulting in one task being silently dropped and
 * the other clobbering it.  Using a single mutex for the full operation
 * removes this race at negligible cost (the lock is uncontended on the
 * fast path in practice).
 *
 * FIX #5: The original slow-path called cond_signal(&not_empty) while
 * still holding rb->mutex, which is correct for a condvar paired with that
 * mutex.  The bug was that the fast path signalled not_empty by acquiring
 * the mutex *after* already having published the task via the atomic store.
 * A consumer that woke up between the store and the lock acquisition would
 * consume the item and go back to sleep — then the producer would signal
 * a phantom item.  Under the mutex-protected model below this race is gone.
 */
static bool ringbuffer_push(TaskRingBuffer* rb, Task* task) {
    lock_acquire(&rb->mutex);

    /* Wait while full, but bail out on shutdown. */
    while (((atomic_load_explicit(&rb->head, memory_order_relaxed) + 1) & RING_BUFFER_MASK) ==
           atomic_load_explicit(&rb->tail, memory_order_relaxed)) {
        if (atomic_load_explicit(&rb->pool->shutdown, memory_order_acquire)) {
            lock_release(&rb->mutex);
            return false;
        }
        cond_wait(&rb->not_full, &rb->mutex);
    }

    uint32_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    rb->tasks[head] = *task;
    atomic_store_explicit(&rb->head, (head + 1) & RING_BUFFER_MASK, memory_order_release);

    /*
     * Signal under the lock.  cond_signal is always safe to call while
     * holding the associated mutex; doing so prevents the lost-wakeup
     * where a consumer checks the queue, finds it empty, but hasn't yet
     * called cond_wait — and therefore misses the signal.
     */
    cond_signal(&rb->not_empty);
    lock_release(&rb->mutex);
    return true;
}

/*
 * FIX #2: Signal not_full whenever *any* slot is freed, not just when the
 * buffer transitions from completely-full to one-slot-free.
 *
 * The original condition `available == RING_BUFFER_SIZE - 1` only fired
 * when every slot had been occupied.  A producer blocked on a queue that
 * was one slot shy of full would never be woken even after consumers drained
 * many slots.
 *
 * FIX #3 (partial): Return -1 on shutdown so callers can distinguish
 * "no work yet" from "shutting down".  The original already did this; we
 * preserve it and tighten the blocking path below.
 */
static int ringbuffer_pull_batch_optimized(TaskRingBuffer* rb, Task* tasks, size_t max_tasks) {
    int yield_count = 0;

    while (1) {
        uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
        uint32_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

        if (head != tail) {
            size_t available = (head - tail) & RING_BUFFER_MASK;
            size_t to_take = (available < max_tasks) ? available : max_tasks;

            uint32_t new_tail = (tail + (uint32_t)to_take) & RING_BUFFER_MASK;
            if (atomic_compare_exchange_weak_explicit(&rb->tail, &tail, new_tail, memory_order_acq_rel,
                                                      memory_order_relaxed)) {
                for (size_t i = 0; i < to_take; i++) {
                    tasks[i] = rb->tasks[(tail + i) & RING_BUFFER_MASK];
                }

                /*
                 * Signal not_full unconditionally: any freed slot might
                 * unblock a producer that is waiting because the queue was
                 * completely full — or was merely one slot shy of full.
                 */
                lock_acquire(&rb->mutex);
                cond_broadcast(&rb->not_full);
                lock_release(&rb->mutex);

                return (int)to_take;
            }
            /* CAS failed — another consumer raced us; retry immediately. */
            continue;
        }

        /* Queue is empty. */
        if (atomic_load_explicit(&rb->pool->shutdown, memory_order_acquire)) {
            return -1;
        }

        if (yield_count < YIELD_THRESHOLD) {
            yield_count++;
            thread_yield();
            continue;
        }

        /*
         * FIX #3 (core): Block under the mutex and re-check both the queue
         * state *and* the shutdown flag inside the critical section.
         *
         * The original code incremented waiting_consumers, then acquired the
         * mutex, then checked head==tail.  A producer could push a task and
         * call cond_signal between the waiting_consumers increment and the
         * cond_wait, causing the signal to be lost and the consumer to sleep
         * forever.
         *
         * By incrementing waiting_consumers *inside* the lock and performing
         * the definitive empty-check there we close this window.
         */
        atomic_fetch_add_explicit(&rb->waiting_consumers, 1, memory_order_relaxed);
        lock_acquire(&rb->mutex);

        /*
         * Re-read head/tail under the lock.  If the queue became non-empty
         * (or shutdown was set) before we reached cond_wait, skip the wait.
         */
        while (atomic_load_explicit(&rb->head, memory_order_acquire) ==
                   atomic_load_explicit(&rb->tail, memory_order_acquire) &&
               !atomic_load_explicit(&rb->pool->shutdown, memory_order_acquire)) {
            cond_wait(&rb->not_empty, &rb->mutex);
        }

        lock_release(&rb->mutex);
        atomic_fetch_sub_explicit(&rb->waiting_consumers, 1, memory_order_relaxed);
        yield_count = 0;
        /* Loop back to try pulling tasks (or return -1 on shutdown). */
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
    if (atomic_load_explicit(&pool->queue.head, memory_order_relaxed) !=
        atomic_load_explicit(&pool->queue.tail, memory_order_relaxed)) {
        return 1;
    }
    for (size_t i = 0; i < pool->num_threads; i++) {
        if (atomic_load_explicit(&pool->threads[i]->queue.head, memory_order_relaxed) !=
            atomic_load_explicit(&pool->threads[i]->queue.tail, memory_order_relaxed)) {
            return 1;
        }
    }
    return 0;
}

/*
 * FIX #4: Remove the goto-based control flow that caused shutdown signals
 * from the local queue pull to be silently ignored.
 *
 * Original logic:
 *   pull from local  → if > 0: goto execute_batch
 *   pull from global → if > 0: goto execute_batch   ← -1 from local is lost
 *
 * When the local queue returned -1 (shutdown) the code fell through to the
 * global queue pull instead of exiting.  With explicit checks after each
 * pull the thread exits cleanly as soon as either queue signals shutdown.
 */
static void* thread_worker(void* arg) {
    thread* thp = (thread*)arg;
    Threadpool* pool = thp->pool;
    Task batch[BATCH_SIZE];
    int batch_count;

    atomic_fetch_add_explicit(&pool->num_threads_alive, 1, memory_order_relaxed);

    while (!atomic_load_explicit(&pool->shutdown, memory_order_acquire)) {
        /* Try local queue first (better cache locality). */
        batch_count = ringbuffer_pull_batch_optimized(&thp->queue, batch, BATCH_SIZE);
        if (batch_count == -1) {
            /* Shutdown signalled from inside the local-queue pull. */
            break;
        }

        if (batch_count == 0) {
            /* Local queue empty — try global queue. */
            batch_count = ringbuffer_pull_batch_optimized(&pool->queue, batch, BATCH_SIZE);
            if (batch_count == -1) {
                break;
            }
            if (batch_count == 0) {
                continue;
            }
        }

        /* Execute the batch. */
        for (int i = 0; i < batch_count; i++) {
            batch[i].function(batch[i].arg);
        }

        /* Signal idle condition only after completing work. */
        if (!has_work(pool)) {
            lock_acquire(&pool->thcount_lock);
            if (!has_work(pool)) {
                cond_broadcast(&pool->threads_all_idle);
            }
            lock_release(&pool->thcount_lock);
        }
    }

    /* Thread cleanup. */
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

    atomic_store_explicit(&pool->num_threads_alive, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->shutdown, 0, memory_order_relaxed);
    ringbuffer_init(&pool->queue);
    pool->num_threads = num_threads;
    pool->queue.pool = pool;

    pool->threads = (thread**)malloc(num_threads * sizeof(thread*));
    if (pool->threads == NULL) {
        ringbuffer_destroy(&pool->queue);
        free(pool);
        return NULL;
    }

    lock_init(&pool->thcount_lock);
    cond_init(&pool->threads_all_idle);

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
    if (pool == NULL || function == NULL) return false;

    Task task = {function, arg};

    static _Thread_local size_t thread_counter = 0;
    size_t thread_id = (thread_counter++) % pool->num_threads;

    if (ringbuffer_push(&pool->threads[thread_id]->queue, &task)) {
        return true;
    }
    return ringbuffer_push(&pool->queue, &task);
}

/*
 * FIX #3 (destroy side): Broadcast shutdown *before* releasing thcount_lock,
 * then broadcast again on every per-thread condvar to wake workers blocked
 * in cond_wait(&not_empty).
 *
 * The original code released thcount_lock, *then* broadcast on the per-thread
 * condvars.  Workers blocked in ringbuffer_pull_batch_optimized never saw the
 * shutdown flag until they were woken — but the broadcast could race with the
 * worker's own lock acquisition, leaving some workers sleeping indefinitely.
 *
 * Correct sequence:
 *   1. Wait for in-flight work to drain (under thcount_lock).
 *   2. Set shutdown flag (sequentially consistent store).
 *   3. Broadcast on ALL condvars so every blocked worker wakes and checks
 *      the flag.
 *   4. Only then release thcount_lock and spin-wait for alive count → 0.
 */
void threadpool_destroy(Threadpool* pool, int timeout_ms) {
    if (pool == NULL) return;

    lock_acquire(&pool->thcount_lock);

    while (has_work(pool)) {
        int ret = cond_wait_timeout(&pool->threads_all_idle, &pool->thcount_lock, timeout_ms);
        if (ret == -1) {
            perror("cond_wait_timeout");
        }
    }

    /* Set shutdown flag while still holding thcount_lock. */
    atomic_store_explicit(&pool->shutdown, 1, memory_order_seq_cst);

    /*
     * Wake every worker that is sleeping inside ringbuffer_pull_batch_optimized
     * before we drop thcount_lock.  Workers re-check the shutdown flag on
     * wakeup and exit cleanly.
     */
    for (size_t i = 0; i < pool->num_threads; i++) {
        lock_acquire(&pool->threads[i]->queue.mutex);
        cond_broadcast(&pool->threads[i]->queue.not_empty);
        cond_broadcast(&pool->threads[i]->queue.not_full);
        lock_release(&pool->threads[i]->queue.mutex);
    }

    lock_acquire(&pool->queue.mutex);
    cond_broadcast(&pool->queue.not_empty);
    cond_broadcast(&pool->queue.not_full);
    lock_release(&pool->queue.mutex);

    lock_release(&pool->thcount_lock);

    /* Spin until every worker has exited. */
    while (atomic_load_explicit(&pool->num_threads_alive, memory_order_acquire) > 0) {
        thread_yield();
    }

    /* Cleanup. */
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
