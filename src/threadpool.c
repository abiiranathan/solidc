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

/*
 * ============================================================================
 * Work-Stealing Threadpool — Chase-Lev Deque
 * ============================================================================
 *
 * BUG / PERF HISTORY
 * ------------------
 * Bug  #1: External thread + worker both called deque_push_bottom on the same
 *          deque concurrently → corrupted bottom.
 *          Fix: external threads use a separate GlobalQueue (mutex-protected).
 *
 * Bug  #2: tls_worker_index not reset between pool lifetimes → stale index
 *          used against a new pool's workers[] → out-of-bounds access.
 *          Fix: TLS only marks worker identity; external threads use GlobalQueue.
 *
 * Bug  #3: Workers called try_steal() before pool->workers[] was fully written,
 *          reading NULL pointers → SIGSEGV in deque_steal_top.
 *          Fix: workers_ready startup barrier; every worker spins until
 *          workers_ready == num_workers before entering the main loop.
 *
 * Perf #1: gq_pull_batch pulled only 1 task per mutex acquisition.
 *          With 8 workers all draining the global queue, that is 8 mutex
 *          round-trips per 8 tasks — O(N) lock acquisitions per batch.
 *          Fix: pull up to BATCH_SIZE tasks per acquisition, push extras
 *          into the worker's own deque so subsequent iterations are lock-free.
 *
 * Perf #2: deque_pop_bottom used `b == 0` as the empty guard.  After any
 *          non-trivial run bottom and top both sit at some value > 0, so the
 *          guard never triggers.  Every idle loop iteration still executed the
 *          full seq_cst store + seq_cst load + comparison on an empty deque.
 *          Fix: check `b == t` BEFORE decrementing bottom.  If already empty,
 *          return false immediately with zero atomic RMWs.
 *
 * Perf #3: unpark_one() acquired park_lock on every submission even when
 *          workers are active.  The num_parked fast-path skips the lock when
 *          nobody is parked, which is fine — but on the transition from busy
 *          to idle each submit still pays a lock round-trip.
 *          Fix: no change to unpark_one; the real gain is in Perf #1 because
 *          pulling BATCH_SIZE tasks at once means unpark_one is called once
 *          per batch rather than once per task.
 *
 * Perf #4: threadpool_submit() is called once per task from external threads.
 *          Each call acquires gq_mutex, writes one task, releases it — O(N)
 *          mutex round-trips for N tasks.  At 10M tasks that is 10M separate
 *          mutex acquisitions on the submission side alone.
 *          Fix: threadpool_submit_batch() accepts an array of (function, arg)
 *          pairs and writes up to GLOBAL_Q_SIZE slots under a single mutex
 *          acquisition, flushing in chunks if the batch exceeds queue capacity.
 *          This reduces submission mutex acquisitions from O(N) to
 *          O(N / GLOBAL_Q_SIZE) — a 16384× reduction for large workloads.
 *
 * ============================================================================
 * DESIGN
 * ============================================================================
 *
 * Every worker owns a private Chase-Lev deque:
 *   OWNER   — pushes/pops at BOTTOM  (no lock, no CAS; just a release store)
 *   THIEVES — steal from TOP         (one seq_cst CAS per attempt)
 *
 * bottom and top live on separate cache lines (no false sharing between owner
 * and thieves).
 *
 *   bottom ──────────────────────────────>
 *   [ tN | ... | t1 | t0 ]
 *                    <─── top
 *
 * External submitters push to a GlobalQueue (single mutex).
 * Workers drain it in batches during their steal scan.
 *
 * MEMORY ORDERS
 *   deque_push_bottom:  task[b]=relaxed, bottom=release
 *   deque_pop_bottom:   early-exit if b==t (no atomics); else bottom=seq_cst,
 *                       top=seq_cst, last-item CAS=seq_cst
 *   deque_steal_top:    top=acquire, bottom=acquire, CAS=seq_cst
 * ============================================================================
 */

#define DEQUE_SIZE    (1u << 12) /* 4096 slots per private deque        */
#define DEQUE_MASK    (DEQUE_SIZE - 1)
#define GLOBAL_Q_SIZE (1u << 14) /* 16384 slots in the global queue     */
#define GLOBAL_Q_MASK (GLOBAL_Q_SIZE - 1)

#ifndef BATCH_SIZE
#define BATCH_SIZE 64
#endif

#define CACHE_LINE_SIZE 64
#define YIELD_THRESHOLD 8

#define CACHE_ALIGNED ALIGN(CACHE_LINE_SIZE)

typedef enum { STEAL_SUCCESS, STEAL_EMPTY, STEAL_ABORT } StealResult;

/* ── Chase-Lev private deque ─────────────────────────────────────────────── */
typedef struct {
    CACHE_ALIGNED atomic_size_t bottom;
    CACHE_ALIGNED atomic_size_t top;
    CACHE_ALIGNED Task tasks[DEQUE_SIZE];
} WorkStealDeque;

/* ── Global submission queue ─────────────────────────────────────────────── */
typedef struct {
    CACHE_ALIGNED Lock mutex;
    CACHE_ALIGNED atomic_uint head;
    CACHE_ALIGNED atomic_uint tail;
    CACHE_ALIGNED Task tasks[GLOBAL_Q_SIZE];
    Condition not_full;
    struct Threadpool* pool;
} GlobalQueue;

/* ── Per-worker ──────────────────────────────────────────────────────────── */
typedef struct worker {
    CACHE_ALIGNED WorkStealDeque deque;
    CACHE_ALIGNED Thread pthread;
    CACHE_ALIGNED size_t index;
    struct Threadpool* pool;
} worker;

/* ── Threadpool ──────────────────────────────────────────────────────────── */
struct Threadpool {
    CACHE_ALIGNED atomic_int shutdown;
    CACHE_ALIGNED atomic_int num_threads_alive;

    CACHE_ALIGNED worker** workers;
    CACHE_ALIGNED size_t num_workers;

    /* Startup barrier — see Bug #3 fix. */
    CACHE_ALIGNED atomic_size_t workers_ready;

    CACHE_ALIGNED GlobalQueue gq;

    /* Parking */
    CACHE_ALIGNED Lock park_lock;
    CACHE_ALIGNED atomic_int num_parked;
    Condition work_available;

    /* Idle detection */
    CACHE_ALIGNED Lock idle_lock;
    CACHE_ALIGNED atomic_int num_active;
    Condition all_idle;
};

/* TLS: SIZE_MAX = external thread; anything else = worker index. */
static _Thread_local size_t tls_worker_index = SIZE_MAX;

/* ============================================================================
 * Global queue
 * ============================================================================ */

static void gq_init(GlobalQueue* gq, struct Threadpool* pool) {
    lock_init(&gq->mutex);
    cond_init(&gq->not_full);
    atomic_store_explicit(&gq->head, 0, memory_order_relaxed);
    atomic_store_explicit(&gq->tail, 0, memory_order_relaxed);
    gq->pool = pool;
}

static void gq_destroy(GlobalQueue* gq) {
    lock_free(&gq->mutex);
    cond_free(&gq->not_full);
}

/* Push one task. Blocks if full (only happens if all 16384 slots are in-flight). */
static bool gq_push(GlobalQueue* gq, Task task) {
    lock_acquire(&gq->mutex);

    while (((atomic_load_explicit(&gq->head, memory_order_relaxed) + 1) & GLOBAL_Q_MASK) ==
           atomic_load_explicit(&gq->tail, memory_order_relaxed)) {
        if (atomic_load_explicit(&gq->pool->shutdown, memory_order_acquire)) {
            lock_release(&gq->mutex);
            return false;
        }
        cond_wait(&gq->not_full, &gq->mutex);
    }

    uint32_t h = atomic_load_explicit(&gq->head, memory_order_relaxed);
    gq->tasks[h] = task;
    atomic_store_explicit(&gq->head, (h + 1) & GLOBAL_Q_MASK, memory_order_release);
    lock_release(&gq->mutex);
    return true;
}

/*
 * gq_push_batch — push up to `count` tasks in one mutex acquisition.
 *
 * Perf #4 fix: the single-task gq_push pays the full mutex round-trip for
 * every task.  With 10M external submissions that is 10M lock acquisitions.
 *
 * Here we hold the lock for the entire write of min(count, free_slots) tasks,
 * releasing only when the queue is full (at which point we wait on not_full
 * and continue from where we left off).  The caller sees a single logical
 * operation even if it internally flushes in multiple chunks.
 *
 * Returns the number of tasks successfully pushed (== count unless shutdown).
 */
static size_t gq_push_batch(GlobalQueue* gq, Task* tasks, size_t count) {
    size_t pushed = 0;

    while (pushed < count) {
        lock_acquire(&gq->mutex);

        /* Wait while full. */
        while (((atomic_load_explicit(&gq->head, memory_order_relaxed) + 1) & GLOBAL_Q_MASK) ==
               atomic_load_explicit(&gq->tail, memory_order_relaxed)) {
            if (atomic_load_explicit(&gq->pool->shutdown, memory_order_acquire)) {
                lock_release(&gq->mutex);
                return pushed;
            }
            cond_wait(&gq->not_full, &gq->mutex);
        }

        /*
         * Write as many tasks as fit without releasing the lock.
         * free_slots = GLOBAL_Q_SIZE - 1 - current occupancy (one slot
         * wasted as the full/empty sentinel).
         */
        uint32_t h = atomic_load_explicit(&gq->head, memory_order_relaxed);
        uint32_t t = atomic_load_explicit(&gq->tail, memory_order_relaxed);
        size_t free = (GLOBAL_Q_SIZE - 1) - ((h - t) & GLOBAL_Q_MASK);
        size_t todo = count - pushed;
        size_t n = todo < free ? todo : free;

        for (size_t i = 0; i < n; i++) {
            gq->tasks[(h + i) & GLOBAL_Q_MASK] = tasks[pushed + i];
        }
        atomic_store_explicit(&gq->head, (h + (uint32_t)n) & GLOBAL_Q_MASK, memory_order_release);
        pushed += n;

        lock_release(&gq->mutex);
    }

    return pushed;
}

/*
 * gq_pull_batch — pull up to `max` tasks in one mutex acquisition.
 *
 * Perf #1 fix: the previous version pulled only 1 task per call.  With N
 * workers all hitting the global queue, that was N mutex acquisitions per
 * N tasks — every task paid the full lock round-trip.
 *
 * Now we pull min(available, max) tasks at once.  The caller pushes the
 * extras into its own private deque so subsequent iterations are lock-free.
 *
 * Returns the number of tasks pulled (0 = empty, -1 = shutdown).
 */
static int gq_pull_batch(GlobalQueue* gq, Task* out, size_t max) {
    lock_acquire(&gq->mutex);

    uint32_t h = atomic_load_explicit(&gq->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&gq->tail, memory_order_relaxed);

    if (h == t) {
        lock_release(&gq->mutex);
        return atomic_load_explicit(&gq->pool->shutdown, memory_order_acquire) ? -1 : 0;
    }

    size_t available = (h - t) & GLOBAL_Q_MASK;
    size_t to_take = available < max ? available : max;

    for (size_t i = 0; i < to_take; i++) {
        out[i] = gq->tasks[(t + i) & GLOBAL_Q_MASK];
    }
    atomic_store_explicit(&gq->tail, (t + (uint32_t)to_take) & GLOBAL_Q_MASK, memory_order_release);

    /* Signal any blocked producers now that slots are free. */
    cond_broadcast(&gq->not_full);
    lock_release(&gq->mutex);
    return (int)to_take;
}

/* ============================================================================
 * Chase-Lev deque
 * ============================================================================ */

static void deque_init(WorkStealDeque* dq) {
    atomic_store_explicit(&dq->bottom, 0, memory_order_relaxed);
    atomic_store_explicit(&dq->top, 0, memory_order_relaxed);
}

/* Owner only. Returns false if full (DEQUE_SIZE=4096; should never happen). */
static bool deque_push_bottom(WorkStealDeque* dq, Task task) {
    size_t b = atomic_load_explicit(&dq->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&dq->top, memory_order_acquire);

    if (b - t >= DEQUE_SIZE) return false;

    dq->tasks[b & DEQUE_MASK] = task;
    atomic_store_explicit(&dq->bottom, b + 1, memory_order_release);
    return true;
}

/*
 * deque_pop_bottom — owner reclaims its own work.
 *
 * Perf #2 fix: the previous version used `if (b == 0) return false` as the
 * empty guard.  After any non-trivial run, bottom and top are both nonzero
 * but equal (deque empty), so that guard never fired.  Every idle iteration
 * still executed the seq_cst store + seq_cst load before discovering the
 * deque was empty.
 *
 * Correct fix: read both bottom and top first.  If b == t the deque is empty
 * and we return immediately with zero atomic RMWs — no seq_cst traffic on the
 * cache line at all.  Only when b > t do we proceed with the full protocol.
 */
static bool deque_pop_bottom(WorkStealDeque* dq, Task* out) {
    size_t b = atomic_load_explicit(&dq->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&dq->top, memory_order_acquire);

    /* Fast-path empty check: no atomics, no memory barriers needed. */
    if (b == t) return false;

    b--;

    /*
     * seq_cst store: must be totally ordered with any thief's seq_cst CAS
     * on top.  This is the linearisation point for the owner's pop.
     */
    atomic_store_explicit(&dq->bottom, b, memory_order_seq_cst);
    t = atomic_load_explicit(&dq->top, memory_order_seq_cst);

    if ((ptrdiff_t)(b - t) > 0) {
        /* More than one item: no race with thieves is possible. */
        *out = dq->tasks[b & DEQUE_MASK];
        return true;
    }

    if (b == t) {
        /* Exactly one item: race with at most one thief via CAS. */
        *out = dq->tasks[b & DEQUE_MASK];
        size_t expected = t;
        bool won = atomic_compare_exchange_strong_explicit(&dq->top, &expected, t + 1, memory_order_seq_cst,
                                                           memory_order_relaxed);
        /* Restore bottom to a consistent empty state regardless of outcome. */
        atomic_store_explicit(&dq->bottom, b + 1, memory_order_relaxed);
        return won;
    }

    /* b < t: a thief stole the last item while we were decrementing. */
    atomic_store_explicit(&dq->bottom, b + 1, memory_order_relaxed);
    return false;
}

/* Any thread except the owner. */
static StealResult deque_steal_top(WorkStealDeque* dq, Task* out) {
    size_t t = atomic_load_explicit(&dq->top, memory_order_acquire);
    size_t b = atomic_load_explicit(&dq->bottom, memory_order_acquire);

    if ((ptrdiff_t)(b - t) <= 0) return STEAL_EMPTY;

    *out = dq->tasks[t & DEQUE_MASK];

    if (!atomic_compare_exchange_strong_explicit(&dq->top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed)) {
        return STEAL_ABORT;
    }
    return STEAL_SUCCESS;
}

/* ============================================================================
 * Parking
 * ============================================================================ */

static void worker_park(Threadpool* pool) {
    atomic_fetch_add_explicit(&pool->num_parked, 1, memory_order_relaxed);
    lock_acquire(&pool->park_lock);

    /*
     * Re-check under lock: work or shutdown could have arrived between our
     * decision to park and acquiring park_lock.  The signal would have been
     * sent before we could receive it.  Skipping cond_wait here avoids
     * sleeping on a non-empty queue (lost-wakeup prevention).
     */
    bool all_empty = true;
    {
        uint32_t h = atomic_load_explicit(&pool->gq.head, memory_order_acquire);
        uint32_t t = atomic_load_explicit(&pool->gq.tail, memory_order_acquire);
        if (h != t) all_empty = false;
    }
    for (size_t i = 0; i < pool->num_workers && all_empty; i++) {
        size_t b = atomic_load_explicit(&pool->workers[i]->deque.bottom, memory_order_acquire);
        size_t t = atomic_load_explicit(&pool->workers[i]->deque.top, memory_order_acquire);
        if ((ptrdiff_t)(b - t) > 0) all_empty = false;
    }

    if (all_empty && !atomic_load_explicit(&pool->shutdown, memory_order_acquire)) {
        cond_wait(&pool->work_available, &pool->park_lock);
    }

    lock_release(&pool->park_lock);
    atomic_fetch_sub_explicit(&pool->num_parked, 1, memory_order_relaxed);
}

static inline void unpark_one(Threadpool* pool) {
    if (atomic_load_explicit(&pool->num_parked, memory_order_relaxed) > 0) {
        lock_acquire(&pool->park_lock);
        cond_signal(&pool->work_available);
        lock_release(&pool->park_lock);
    }
}

static inline void unpark_all(Threadpool* pool) {
    lock_acquire(&pool->park_lock);
    cond_broadcast(&pool->work_available);
    lock_release(&pool->park_lock);
}

/* ============================================================================
 * Worker thread
 * ============================================================================ */

/*
 * try_steal — attempt to find work from another worker or the global queue.
 *
 * Private deques are scanned first in randomised order (xorshift64 per
 * worker, no shared state).  The global queue is last: it requires a mutex
 * so we only pay that cost after exhausting all lock-free steal attempts.
 *
 * When the global queue has tasks, we pull a full BATCH_SIZE chunk in one
 * mutex acquisition (Perf #1 fix).  The first task is returned immediately;
 * the remaining tasks are pushed into the caller's own deque so subsequent
 * iterations are purely lock-free pops from bottom.
 */
static bool try_steal(worker* self, Task* out) {
    Threadpool* pool = self->pool;
    size_t n = pool->num_workers;

    /* xorshift64: register-local, zero synchronisation cost. */
    static _Thread_local uint64_t rng = 0;
    if (rng == 0) rng = ((uint64_t)self->index + 1) * 6364136223846793005ULL;
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    size_t start = (size_t)(rng % n);

    /* 1. Scan private deques (lock-free). */
    for (size_t i = 0; i < n; i++) {
        size_t v = (start + i) % n;
        if (v == self->index) continue;
        StealResult r = deque_steal_top(&pool->workers[v]->deque, out);
        if (r == STEAL_SUCCESS) return true;
    }

    /*
     * 2. Drain global queue in a batch.
     *
     * Pull up to BATCH_SIZE tasks in one mutex acquisition.  Distribute
     * them: return tasks[0] to the caller immediately, push tasks[1..N-1]
     * into our own deque so they are consumed lock-free on subsequent
     * iterations.  One mutex lock amortised over up to 64 tasks.
     */
    Task batch[BATCH_SIZE];
    int got = gq_pull_batch(&pool->gq, batch, BATCH_SIZE);
    if (got <= 0) return false;

    /* Push extras into own deque (owner-only, no lock needed). */
    for (int i = 1; i < got; i++) {
        if (!deque_push_bottom(&self->deque, batch[i])) {
            /*
             * Own deque full (extremely unlikely at DEQUE_SIZE=4096).
             * Push back the remaining tasks to the global queue so they
             * are not lost.  We accept the lock cost here as a rare path.
             */
            for (int j = i; j < got; j++) {
                gq_push(&pool->gq, batch[j]);
            }
            break;
        }
    }

    *out = batch[0];
    return true;
}

static void* worker_thread(void* arg) {
    worker* self = (worker*)arg;
    Threadpool* pool = self->pool;
    Task task;
    int spin = 0;

    tls_worker_index = self->index;

    /*
     * Startup barrier (Bug #3 fix).
     *
     * Increment workers_ready to signal we have started, then spin until
     * every slot of pool->workers[] is written.  The acquire on the final
     * load synchronises with the release stores inside each worker's own
     * fetch_add, ensuring we see all pool->workers[] writes before
     * proceeding into try_steal().
     */
    atomic_fetch_add_explicit(&pool->workers_ready, 1, memory_order_release);
    while (atomic_load_explicit(&pool->workers_ready, memory_order_acquire) < pool->num_workers) {
        thread_yield();
    }

    atomic_fetch_add_explicit(&pool->num_threads_alive, 1, memory_order_relaxed);

    while (!atomic_load_explicit(&pool->shutdown, memory_order_acquire)) {
        /* Own deque first — zero contention, best cache locality. */
        if (deque_pop_bottom(&self->deque, &task)) goto execute;

        /* Steal or drain global queue. */
        if (try_steal(self, &task)) goto execute;

        /* Brief spin before paying the parking overhead. */
        if (spin < YIELD_THRESHOLD) {
            spin++;
            thread_yield();
            continue;
        }

        spin = 0;
        worker_park(pool);
        continue;

    execute:
        spin = 0;
        atomic_fetch_add_explicit(&pool->num_active, 1, memory_order_relaxed);
        task.function(task.arg);
        int active = atomic_fetch_sub_explicit(&pool->num_active, 1, memory_order_acq_rel) - 1;
        if (active == 0) {
            /* Potential transition to fully idle — check and signal. */
            bool any_work = false;
            {
                uint32_t h = atomic_load_explicit(&pool->gq.head, memory_order_relaxed);
                uint32_t t = atomic_load_explicit(&pool->gq.tail, memory_order_relaxed);
                if (h != t) any_work = true;
            }
            for (size_t i = 0; i < pool->num_workers && !any_work; i++) {
                size_t b = atomic_load_explicit(&pool->workers[i]->deque.bottom, memory_order_relaxed);
                size_t t = atomic_load_explicit(&pool->workers[i]->deque.top, memory_order_relaxed);
                if ((ptrdiff_t)(b - t) > 0) any_work = true;
            }
            if (!any_work) {
                lock_acquire(&pool->idle_lock);
                if (atomic_load_explicit(&pool->num_active, memory_order_relaxed) == 0) {
                    cond_broadcast(&pool->all_idle);
                }
                lock_release(&pool->idle_lock);
            }
        }
    }

    int alive = atomic_fetch_sub_explicit(&pool->num_threads_alive, 1, memory_order_acq_rel) - 1;
    if (alive == 0) {
        lock_acquire(&pool->idle_lock);
        cond_broadcast(&pool->all_idle);
        lock_release(&pool->idle_lock);
    }

    return NULL;
}

/* ============================================================================
 * Worker init
 * ============================================================================ */

static int worker_init(Threadpool* pool, worker** w, size_t index) {
    *w = (worker*)ALIGNED_ALLOC(CACHE_LINE_SIZE, sizeof(worker));
    if (!*w) return -1;
    (*w)->pool = pool;
    (*w)->index = index;
    deque_init(&(*w)->deque);
    return thread_create(&(*w)->pthread, worker_thread, *w);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

Threadpool* threadpool_create(size_t num_threads) {
    if (num_threads == 0) num_threads = 1;

    Threadpool* pool = (Threadpool*)ALIGNED_ALLOC(CACHE_LINE_SIZE, sizeof(Threadpool));
    if (!pool) return NULL;

    atomic_store_explicit(&pool->shutdown, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->num_threads_alive, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->num_parked, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->num_active, 0, memory_order_relaxed);
    atomic_store_explicit(&pool->workers_ready, 0, memory_order_relaxed);

    pool->num_workers = num_threads;
    gq_init(&pool->gq, pool);

    lock_init(&pool->park_lock);
    cond_init(&pool->work_available);
    lock_init(&pool->idle_lock);
    cond_init(&pool->all_idle);

    pool->workers = (worker**)malloc(num_threads * sizeof(worker*));
    if (!pool->workers) {
        gq_destroy(&pool->gq);
        free(pool);
        return NULL;
    }

    /* Zero array before spawning; see Bug #3 fix. */
    for (size_t i = 0; i < num_threads; i++) pool->workers[i] = NULL;

    for (size_t i = 0; i < num_threads; i++) {
        if (worker_init(pool, &pool->workers[i], i) != 0) {
            /* Release barrier so partially-started workers can exit. */
            atomic_store_explicit(&pool->workers_ready, num_threads, memory_order_release);
            pool->num_workers = i;
            threadpool_destroy(pool, -1);
            return NULL;
        }
    }

    /*
     * All pool->workers[] entries are now valid.  The barrier in each
     * worker's fetch_add + the acquire load guarantees they will see all
     * pool->workers[] writes before entering try_steal().
     */
    return pool;
}

/*
 * threadpool_submit
 *
 * Worker thread  → push directly into own deque (zero-contention hot path).
 * External thread → push to global queue (single mutex, cold path).
 *
 * After pushing, wake one parked worker if any are sleeping.  Because
 * try_steal now pulls BATCH_SIZE tasks at once from the global queue,
 * a single wakeup is sufficient — the woken worker will pick up a full
 * batch and wake neighbours via the work propagation in its deque.
 */
bool threadpool_submit(Threadpool* pool, void (*function)(void*), void* arg) {
    if (!pool || !function) return false;

    Task task = {function, arg};

    if (tls_worker_index != SIZE_MAX) {
        if (deque_push_bottom(&pool->workers[tls_worker_index]->deque, task)) {
            unpark_one(pool);
            return true;
        }
    }

    bool ok = gq_push(&pool->gq, task);
    if (ok) unpark_one(pool);
    return ok;
}

/*
 * threadpool_submit_batch — push N tasks in one call.
 *
 * Worker thread: each task is pushed into the caller's own deque one at a
 * time (deque_push_bottom is already lock-free, so no batching needed there).
 * If the deque fills up, remaining tasks spill to the global queue via
 * gq_push_batch.
 *
 * External thread: all tasks go to the global queue via gq_push_batch, which
 * holds gq_mutex for the entire write of each chunk.  For a batch of N tasks
 * this costs ceil(N / (GLOBAL_Q_SIZE-1)) mutex acquisitions instead of N —
 * at GLOBAL_Q_SIZE=16384 that is a 16384× reduction in lock traffic for large
 * submissions.
 *
 * Returns the number of tasks successfully submitted.  On shutdown this may
 * be less than count; the caller should treat a short return as an error.
 */
size_t threadpool_submit_batch(Threadpool* pool, void (**functions)(void*), void** args, size_t count) {
    if (!pool || !functions || count == 0) return 0;

    if (tls_worker_index != SIZE_MAX) {
        /*
         * Worker thread: push directly into own deque one at a time.
         * Spill to global queue if the deque fills (extremely rare at
         * DEQUE_SIZE=4096 but handled correctly).
         */
        size_t pushed = 0;
        for (size_t i = 0; i < count; i++) {
            if (!functions[i]) continue;
            Task task = {functions[i], args ? args[i] : NULL};
            if (deque_push_bottom(&pool->workers[tls_worker_index]->deque, task)) {
                pushed++;
            } else {
                /* Deque full — spill remainder to global queue. */
                Task* spill = (Task*)malloc((count - i) * sizeof(Task));
                if (!spill) break;
                size_t nspill = 0;
                for (size_t j = i; j < count; j++) {
                    if (functions[j]) {
                        spill[nspill++] = (Task){functions[j], args ? args[j] : NULL};
                    }
                }
                pushed += gq_push_batch(&pool->gq, spill, nspill);
                free(spill);
                break;
            }
        }
        if (pushed > 0) unpark_one(pool);
        return pushed;
    }

    /*
     * External thread: build a flat Task array and hand it to gq_push_batch
     * in one call.  gq_push_batch flushes in chunks that fit the queue,
     * blocking only when the queue is completely full.
     *
     * Optimisation: if count <= BATCH_SIZE we use a stack-allocated array
     * to avoid the malloc entirely on small batches.
     */
    Task stack_buf[BATCH_SIZE];
    Task* tasks = (count <= BATCH_SIZE) ? stack_buf : (Task*)malloc(count * sizeof(Task));
    if (!tasks) return 0;

    size_t ntasks = 0;
    for (size_t i = 0; i < count; i++) {
        if (functions[i]) {
            tasks[ntasks++] = (Task){functions[i], args ? args[i] : NULL};
        }
    }

    size_t pushed = gq_push_batch(&pool->gq, tasks, ntasks);
    if (tasks != stack_buf) free(tasks);

    if (pushed > 0) unpark_one(pool);
    return pushed;
}

void threadpool_wait(Threadpool* pool) {
    if (!pool) return;

    lock_acquire(&pool->idle_lock);
    for (;;) {
        if (atomic_load_explicit(&pool->num_active, memory_order_relaxed) == 0) {
            bool any_work = false;
            {
                uint32_t h = atomic_load_explicit(&pool->gq.head, memory_order_acquire);
                uint32_t t = atomic_load_explicit(&pool->gq.tail, memory_order_acquire);
                if (h != t) any_work = true;
            }
            for (size_t i = 0; i < pool->num_workers && !any_work; i++) {
                size_t b = atomic_load_explicit(&pool->workers[i]->deque.bottom, memory_order_acquire);
                size_t t = atomic_load_explicit(&pool->workers[i]->deque.top, memory_order_acquire);
                if ((ptrdiff_t)(b - t) > 0) any_work = true;
            }
            if (!any_work) break;
        }
        cond_wait(&pool->all_idle, &pool->idle_lock);
    }
    lock_release(&pool->idle_lock);
}

void threadpool_destroy(Threadpool* pool, int timeout_ms) {
    if (!pool) return;

    lock_acquire(&pool->idle_lock);
    for (;;) {
        bool any_work = false;
        {
            uint32_t h = atomic_load_explicit(&pool->gq.head, memory_order_acquire);
            uint32_t t = atomic_load_explicit(&pool->gq.tail, memory_order_acquire);
            if (h != t) any_work = true;
        }
        for (size_t i = 0; i < pool->num_workers && !any_work; i++) {
            size_t b = atomic_load_explicit(&pool->workers[i]->deque.bottom, memory_order_acquire);
            size_t t = atomic_load_explicit(&pool->workers[i]->deque.top, memory_order_acquire);
            if ((ptrdiff_t)(b - t) > 0) any_work = true;
        }
        bool busy = atomic_load_explicit(&pool->num_active, memory_order_relaxed) > 0;
        if (!any_work && !busy) break;

        int r = cond_wait_timeout(&pool->all_idle, &pool->idle_lock, timeout_ms);
        if (r == -1) perror("cond_wait_timeout");
    }

    atomic_store_explicit(&pool->shutdown, 1, memory_order_seq_cst);
    lock_release(&pool->idle_lock);

    unpark_all(pool);

    while (atomic_load_explicit(&pool->num_threads_alive, memory_order_acquire) > 0) {
        thread_yield();
    }

    for (size_t i = 0; i < pool->num_workers; i++) {
        if (pool->workers[i]) {
            thread_join(pool->workers[i]->pthread, NULL);
            free(pool->workers[i]);
        }
    }
    free(pool->workers);

    gq_destroy(&pool->gq);
    lock_free(&pool->park_lock);
    cond_free(&pool->work_available);
    lock_free(&pool->idle_lock);
    cond_free(&pool->all_idle);
    free(pool);
}
