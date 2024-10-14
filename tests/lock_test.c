#include "../include/lock.h"
#include "../include/thread.h"

#include <assert.h>
#include <stdio.h>

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
    printf("Thread %lu acquired the lock\n", thread_self());
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

    Thread thread;
    thread_create(&thread, modify_shared_var, &condition);

    lock_init(&lock);     // re-initialize the lock
    lock_acquire(&lock);  // Acquire the lock before waiting for the condition

    // Wait for shared_var to be modified
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

    lock_init(&lock);     // re-initialize the lock
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

int main(void) {
    lock_init(&lock);

    Thread threads[NUM_THREADS];
    struct Summer summers[NUM_THREADS];
    int sum = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        summers[i].sum = &sum;
        summers[i].i = i;
        thread_create(&threads[i], thread_func, &summers[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], NULL);
    }

    printf("Sum: %d\n", sum);
    assert(sum == 45);

    lock_free(&lock);

    test_condition_variables();
    test_cond_brodcast();
    return 0;
}