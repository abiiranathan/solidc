#include "../include/lock.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
void lock_init(Lock* lock) {
    InitializeCriticalSection(lock);
}

void lock_acquire(Lock* lock) {
    EnterCriticalSection(lock);
}

void lock_release(Lock* lock) {
    LeaveCriticalSection(lock);
}

void lock_free(Lock* lock) {
    DeleteCriticalSection(lock);
}

int lock_try_acquire(Lock* lock) {
    return TryEnterCriticalSection(lock);
}

void lock_wait(Lock* lock, Condition* condition, int timeout) {
    SleepConditionVariableCS(condition, lock, timeout);
}

void cond_init(Condition* condition) {
    InitializeConditionVariable(condition);
}

void cond_signal(Condition* condition) {
    WakeConditionVariable(condition);
}

void cond_broadcast(Condition* condition) {
    WakeAllConditionVariable(condition);
}

void cond_wait(Condition* condition, Lock* lock) {
    SleepConditionVariableCS(condition, lock, INFINITE);
}

void cond_wait_timeout(Condition* condition, Lock* lock, int timeout) {
    SleepConditionVariableCS(condition, lock, timeout);
}

void cond_free(Condition* condition) {
    // No-op
    (void)condition;
}
#else
void lock_init(Lock* lock) {
    int ret = pthread_mutex_init(lock, NULL);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize mutex: %d\n", ret);
        exit(1);
    }
}

void lock_acquire(Lock* lock) {
    int ret = pthread_mutex_lock(lock);
    if (ret != 0) {
        switch (ret) {
            case EDEADLK:
                fprintf(stderr,
                        "A deadlock condition was detected or the current thread already owns the "
                        "mutex.\n");
                break;
            case EINVAL:
                fprintf(stderr,
                        "The value specified by mutex does not refer to an initialized mutex "
                        "object.\n");
                break;
            default:
                fprintf(stderr, "Failed to lock mutex: %d\n", ret);
        }
        exit(1);
    }
}

void lock_release(Lock* lock) {
    int ret = pthread_mutex_unlock(lock);
    if (ret != 0) {
        fprintf(stderr, "Failed to unlock mutex: %d\n", ret);
        exit(1);
    }
}

void lock_free(Lock* lock) {
    if (!lock) {
        return;
    }

    int ret = pthread_mutex_destroy(lock);
    if (ret != 0) {
        fprintf(stderr, "Failed to destroy mutex: %d\n", ret);
        exit(1);
    }
}

int lock_try_acquire(Lock* lock) {
    return pthread_mutex_trylock(lock);
}

void lock_wait(Lock* lock, Condition* condition, int timeout) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (__syscall_slong_t)((timeout % 1000) * 1000000);
    pthread_cond_timedwait(condition, lock, &ts);
}

void cond_init(Condition* condition) {
    pthread_cond_init(condition, NULL);
}

void cond_signal(Condition* condition) {
    pthread_cond_signal(condition);
}

void cond_broadcast(Condition* condition) {
    pthread_cond_broadcast(condition);
}

void cond_wait(Condition* condition, Lock* lock) {
    pthread_cond_wait(condition, lock);
}

void cond_wait_timeout(Condition* condition, Lock* lock, int timeout) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (__syscall_slong_t)((timeout % 1000) * 1000000);
    pthread_cond_timedwait(condition, lock, &ts);
}

void cond_free(Condition* condition) {
    pthread_cond_destroy(condition);
}

#endif  // _WIN32
