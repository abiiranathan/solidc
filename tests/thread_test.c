#include "../include/thread.h"
#include <stddef.h>
#include <stdio.h>

int fib(int n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

void* thrd_func_attr(void* arg) {
    int* n = arg;
    printf("Fibonacci of %d is %d\n", *n, fib(*n));
    return NULL;
}

void test_thread_with_attributes() {
    Thread thread;
    ThreadAttr attr;
    thread_attr_init(&attr);

    int n = 10;

// run this only on posix systems
#ifndef _WIN32
    // set the stack size to 1MB
    pthread_attr_setstacksize(&attr, (size_t)(1024 * 1024));

    // set the scheduling policy to SCHED_FIFO
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    // set the priority to 99
    struct sched_param param;
    param.sched_priority = 99;
    pthread_attr_setschedparam(&attr, &param);

    // // set as detached
    // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // pthread_attr_getdetachstate(&attr, &detached);
#else
    // set the stack size to 1MB
    attr.stackSize = 1024 * 1024;
#endif

    // create the thread with the specified attributes
    thread_create_attr(&thread, &attr, thrd_func_attr, &n);

    // join the thread only if it is not detached
    thread_join(thread, NULL);

    // destroy the thread attributes
    thread_attr_destroy(&attr);
}

void* thrd_func(void* arg) {
    int* n = arg;

    printf("Got: %d\n", *n);
    // sleep_ms(500);
    return NULL;
}

void test_system_thread_ident() {
    printf("PID: %d\n", get_pid());
    printf("TID: %d\n", get_tid());
    printf("Number of CPUs: %lu\n", get_ncpus());

#ifndef _WIN32
    printf("PPID: %d\n", get_ppid());
    printf("UID: %d\n", get_uid());
    printf("GID: %d\n", get_gid());
    printf("Username: %s\n", get_username());
    printf("Groupname: %s\n", get_groupname());
#endif
}

int main(void) {
#define N 10
    Thread threads[N] = {0};
    int nums[N] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (int i = 0; i < N; ++i) {
        thread_create(&threads[i], thrd_func, &nums[i]);
    }

    for (int i = 0; i < N; i++) {
        thread_join(threads[i], NULL);
    }

    test_thread_with_attributes();

    test_system_thread_ident();
}