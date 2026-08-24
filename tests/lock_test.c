#include "../include/lock.h"
#include "../include/macros.h"
#include "../include/rwlock.h"
#include "../include/spinlock.h"
#include "../include/thread.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NUM_THREADS 10

static Lock lock;

struct Summer {
    int* sum;
    int i;
};

static void* thread_func(void* arg) {
    lock_acquire(&lock);
    struct Summer* summer = (struct Summer*)arg;
    *summer->sum += summer->i;
    lock_release(&lock);
    return NULL;
}

int shared_var = 0;

void* modify_shared_var(void* arg) {
    Condition* condition = (Condition*)arg;

    lock_acquire(&lock);
    shared_var = 1;
    cond_signal(condition);
    lock_release(&lock);

    return NULL;
}

void test_condition_variables() {
    Condition condition;
    cond_init(&condition);

    Thread thread = {0};
    thread_create(&thread, modify_shared_var, &condition);

    lock_acquire(&lock);

    while (shared_var == 0) {
        cond_wait(&condition, &lock);
    }
    lock_release(&lock);

    thread_join(thread, NULL);
    cond_free(&condition);

    assert(shared_var == 1);
    printf("Condition variables test passed\n");
}

int counter = 0;
void* add_one(void* arg) {
    Condition* condition = (Condition*)arg;

    lock_acquire(&lock);
    counter++;
    cond_broadcast(condition);  // Wake up all threads
    lock_release(&lock);
    return NULL;
}

void test_cond_brodcast() {
    Condition condition;
    cond_init(&condition);

    Thread threads[NUM_THREADS];

    lock_acquire(&lock);  // Acquire the lock before starting the threads

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_create(&threads[i], add_one, &condition);
    }

    while (counter < NUM_THREADS) {
        cond_wait(&condition, &lock);
    }
    lock_release(&lock);  // Release the lock after the condition is met

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], NULL);
    }

    cond_free(&condition);
    assert(counter == NUM_THREADS);
    printf("Cond Broadcast variables test passed\n");
}

/* ------------------------------------------------------------------ */
/* Timeout hardening tests (CLOCK_MONOTONIC conds)                     */
/* ------------------------------------------------------------------ */

static atomic_int g_signalled;
static Condition g_timeout_cond;

static void* timeout_signaller(void* arg) {
    (void)arg;
    struct timespec ts = {0, 10 * 1000 * 1000}; /* 10ms */
    nanosleep(&ts, NULL);

    lock_acquire(&lock);
    atomic_store(&g_signalled, 1);
    cond_signal(&g_timeout_cond);
    lock_release(&lock);
    return NULL;
}

static long elapsed_ms(const struct timespec* start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) * 1000L + (now.tv_nsec - start->tv_nsec) / 1000000L;
}

void test_cond_wait_timeout(void) {
    ASSERT_EQ(cond_init(&g_timeout_cond), 0);
    atomic_store(&g_signalled, 0);

    /* Timeout path: nobody signals; wait must exceed ~60ms but not hang,
     * even if the wall clock were stepped (monotonic guarantee). */
    ASSERT_EQ(lock_acquire(&lock), 0);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int rc = cond_wait_timeout(&g_timeout_cond, &lock, 80);
    long took = elapsed_ms(&start);
    ASSERT_EQ(rc, -1);
    ASSERT(took >= 60);
    ASSERT(took < 5000);
    ASSERT_EQ(lock_release(&lock), 0);

    /* Signal path: another thread signals after ~10ms; we should wake
     * well before the 5s timeout. */
    Thread t = {0};
    thread_create(&t, timeout_signaller, NULL);

    ASSERT_EQ(lock_acquire(&lock), 0);
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (!atomic_load(&g_signalled)) {
        rc = cond_wait_timeout(&g_timeout_cond, &lock, 5000);
        ASSERT_EQ(rc, 0); /* must be signalled, not timed out */
    }
    took = elapsed_ms(&start);
    ASSERT(took >= 5);
    ASSERT(took < 3000);
    ASSERT_EQ(lock_release(&lock), 0);

    thread_join(t, NULL);
    cond_free(&g_timeout_cond);
    printf("Condition timeout test passed\n");
}

/* try_acquire: uncontended succeeds; contended fails without blocking. */

typedef struct TryArg {
    Lock* l;
    atomic_int* got;
} TryArg;

static void* trylock_child_impl(void* p) {
    TryArg* arg = p;
    atomic_store(arg->got, lock_try_acquire(arg->l)); /* expect -1 */
    return NULL;
}

void test_try_acquire(void) {
    Lock l2;
    ASSERT_EQ(lock_init(&l2), 0);

    /* Uncontended: acquires. */
    ASSERT_EQ(lock_try_acquire(&l2), 0);

    /* Contended: child thread must fail to take it while we hold it. */
    atomic_int child_got = 0;
    TryArg arg = {&l2, &child_got};

    Thread t = {0};
    thread_create(&t, trylock_child_impl, &arg);
    thread_join(t, NULL);

    ASSERT_EQ(atomic_load(&child_got), -1); /* failed as expected */

    ASSERT_EQ(lock_release(&l2), 0);

    /* After release it can be acquired again. */
    ASSERT_EQ(lock_try_acquire(&l2), 0);
    ASSERT_EQ(lock_release(&l2), 0);

    lock_free(&l2);
    printf("try_acquire test passed\n");
}

/* NULL-safety contract of every wrapper. */
void test_null_safety(void) {
    ASSERT_EQ(lock_init(NULL), -1);
    ASSERT_EQ(lock_acquire(NULL), -1);
    ASSERT_EQ(lock_release(NULL), -1);
    ASSERT_EQ(lock_try_acquire(NULL), -1);
    ASSERT_EQ(lock_free(NULL), 0); /* cleanup fns accept NULL */
    ASSERT_EQ(cond_init(NULL), -1);
    ASSERT_EQ(cond_signal(NULL), -1);
    ASSERT_EQ(cond_broadcast(NULL), -1);
    ASSERT_EQ(cond_wait(NULL, NULL), -1);
    ASSERT_EQ(cond_wait_timeout(NULL, NULL, 10), -1);
    ASSERT_EQ(cond_free(NULL), 0);
    printf("NULL safety test passed\n");
}

/* pthread-backed rwlock wrappers keep counts consistent under contention. */
static rwlock_t g_rwlock;
static atomic_long g_rw_value;
static atomic_long g_rw_writes;

static void* rw_reader(void* arg) {
    (void)arg;
    for (int i = 0; i < 10000; i++) {
        rwlock_rdlock(&g_rwlock);
        long v = atomic_load(&g_rw_value);
        (void)v; /* read-only: value must simply be stable under rdlock */
        rwlock_unlock_rd(&g_rwlock);
    }
    return NULL;
}

static void* rw_writer(void* arg) {
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        rwlock_wrlock(&g_rwlock);
        atomic_store(&g_rw_value, atomic_load(&g_rw_value) + 1);
        atomic_store(&g_rw_writes, atomic_load(&g_rw_writes) + 1);
        rwlock_unlock_wr(&g_rwlock);
    }
    return NULL;
}

void test_rwlock_wrappers(void) {
    ASSERT(rwlock_init(&g_rwlock));
    atomic_store(&g_rw_value, 0);
    atomic_store(&g_rw_writes, 0);

    enum { R = 4, W = 2 };
    Thread readers[R], writers[W];
    for (int i = 0; i < R; i++) thread_create(&readers[i], rw_reader, NULL);
    for (int i = 0; i < W; i++) thread_create(&writers[i], rw_writer, NULL);

    for (int i = 0; i < R; i++) thread_join(readers[i], NULL);
    for (int i = 0; i < W; i++) thread_join(writers[i], NULL);

    ASSERT_EQ(atomic_load(&g_rw_value), (long)W * 1000);
    ASSERT_EQ(atomic_load(&g_rw_writes), (long)W * 1000);

    rwlock_destroy(&g_rwlock);
    printf("rwlock wrapper test passed\n");
}

/* RW-spinlock: exclusive writers, concurrent readers, no lost updates. */
static fast_rwlock_t g_spin;
static atomic_long g_spin_value;

static void* spin_writer(void* arg) {
    (void)arg;
    for (int i = 0; i < 5000; i++) {
        fast_rwlock_wrlock(&g_spin);
        atomic_store(&g_spin_value, atomic_load(&g_spin_value) + 1);
        fast_rwlock_unlock_wr(&g_spin);
    }
    return NULL;
}

static void* spin_reader(void* arg) {
    (void)arg;
    for (int i = 0; i < 20000; i++) {
        fast_rwlock_rdlock(&g_spin);
        volatile long v = atomic_load(&g_spin_value);
        (void)v;
        fast_rwlock_unlock_rd(&g_spin);
    }
    return NULL;
}

void test_fast_rwlock(void) {
    fast_rwlock_init(&g_spin);
    atomic_store(&g_spin_value, 0);

    enum { R = 4, W = 4 };
    Thread readers[R], writers[W];
    for (int i = 0; i < W; i++) thread_create(&writers[i], spin_writer, NULL);
    for (int i = 0; i < R; i++) thread_create(&readers[i], spin_reader, NULL);

    for (int i = 0; i < W; i++) thread_join(writers[i], NULL);
    for (int i = 0; i < R; i++) thread_join(readers[i], NULL);

    ASSERT_EQ(atomic_load(&g_spin_value), (long)W * 5000);
    printf("fast_rwlock spinlock test passed\n");
}

/* Mutex vs rwlock micro-benchmark printed for manual perf comparison. */
void bench_locks_summary(void) {
    enum { ITER = 200000 };
    volatile long sink = 0;

    Lock m;
    lock_init(&m);
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int i = 0; i < ITER; i++) {
        lock_acquire(&m);
        sink += i;
        lock_release(&m);
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    long mutex_ns = ((b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec)) / ITER;
    lock_free(&m);
    printf("uncontended mutex: %ld ns/op\n", mutex_ns);
    (void)sink;
}

int main(void) {
    ASSERT_EQ(lock_init(&lock), 0);

    Thread threads[NUM_THREADS];
    struct Summer summers[NUM_THREADS];
    int sum = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        summers[i].sum = &sum;
        summers[i].i   = i;
        thread_create(&threads[i], thread_func, &summers[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], NULL);
    }

    ASSERT_EQ(sum, 45);
    printf("Sum: %d\n", sum);

    /* NOTE: keep the global lock alive for the condition-variable tests
     * below; destroying it here would make every later lock_acquire /
     * cond_wait on it undefined (EINVAL). */

    test_condition_variables();
    test_cond_brodcast();
    test_null_safety();
    test_try_acquire();
    test_cond_wait_timeout();
    test_rwlock_wrappers();
    test_fast_rwlock();
    bench_locks_summary();

    lock_free(&lock);
    return 0;
}
