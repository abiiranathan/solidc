#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>   // for errno constants
#include <stdio.h>   // for fprintf, stderr
#include <stdlib.h>  // for NULL
#include <string.h>  // for strerror
#include <time.h>    // for clock_gettime, timespec

#include "../include/lock.h"

#ifdef _WIN32

int lock_init(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }
    InitializeCriticalSection(lock);
    return 0;
}

int lock_acquire(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }

    EnterCriticalSection(lock);
    return 0;
}

int lock_release(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }

    LeaveCriticalSection(lock);
    return 0;
}

int lock_free(Lock* lock) {
    if (lock == NULL) {
        return 0;
    }

    DeleteCriticalSection(lock);
    return 0;
}

int lock_try_acquire(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }

    if (TryEnterCriticalSection(lock)) {
        return 0;
    }
    return -1;
}

int lock_wait(Lock* lock, Condition* condition, int timeout_ms) {
    if (lock == NULL || condition == NULL) {
        return -1;
    }

    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    if (SleepConditionVariableCS(condition, lock, timeout)) {
        return 0;
    }
    return -1;
}

int cond_init(Condition* condition) {
    if (condition == NULL) {
        return -1;
    }

    InitializeConditionVariable(condition);
    return 0;
}

int cond_signal(Condition* condition) {
    if (condition == NULL) {
        return -1;
    }

    WakeConditionVariable(condition);
    return 0;
}

int cond_broadcast(Condition* condition) {
    if (condition == NULL) {
        return -1;
    }

    WakeAllConditionVariable(condition);
    return 0;
}

int cond_wait(Condition* condition, Lock* lock) {
    if (condition == NULL || lock == NULL) {
        return -1;
    }

    if (SleepConditionVariableCS(condition, lock, INFINITE)) {
        return 0;
    }
    return -1;
}

int cond_wait_timeout(Condition* condition, Lock* lock, int timeout_ms) {
    if (condition == NULL || lock == NULL) {
        return -1;
    }

    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    if (SleepConditionVariableCS(condition, lock, timeout)) {
        return 0;
    }

    return -1;
}

int cond_free(Condition* condition) {
    // Windows condition variables don't require explicit cleanup
    (void)condition;
    return 0;
}

#else  // POSIX implementation

int lock_init(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }

    int ret = pthread_mutex_init(lock, NULL);
    if (ret != 0) {
        fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(ret));
        return -1;
    }
    return 0;
}

int lock_acquire(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }

    int ret = pthread_mutex_lock(lock);
    if (ret != 0) {
        switch (ret) {
            case EDEADLK:
                fprintf(stderr, "Deadlock detected in pthread_mutex_lock\n");
                return -1;
            case EINVAL:
                fprintf(stderr, "Invalid mutex in pthread_mutex_lock\n");
                return -1;
            default:
                fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(ret));
                return -1;
        }
    }

    return 0;
}

int lock_release(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }

    int ret = pthread_mutex_unlock(lock);
    if (ret != 0) {
        fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(ret));
        return -1;
    }

    return 0;
}

int lock_free(Lock* lock) {
    if (lock == NULL) {
        return 0;  // NULL is valid for cleanup functions
    }

    int ret = pthread_mutex_destroy(lock);
    if (ret != 0) {
        fprintf(stderr, "pthread_mutex_destroy failed: %s\n", strerror(ret));
        return -1;
    }

    return 0;
}

int lock_try_acquire(Lock* lock) {
    if (lock == NULL) {
        return -1;
    }

    int ret = pthread_mutex_trylock(lock);
    switch (ret) {
        case 0:
            return 0;
        case EBUSY:
            return -1;
        case EINVAL:
            fprintf(stderr, "Invalid mutex in pthread_mutex_trylock\n");
            return -1;
        default:
            fprintf(stderr, "pthread_mutex_trylock failed: %s\n", strerror(ret));
            return -1;
    }
}

int lock_wait(Lock* lock, Condition* condition, int timeout_ms) {
    return cond_wait_timeout(condition, lock, timeout_ms);
}

int cond_init(Condition* condition) {
    if (condition == NULL) {
        return -1;
    }

    int ret = pthread_cond_init(condition, NULL);
    if (ret != 0) {
        fprintf(stderr, "pthread_cond_init failed: %s\n", strerror(ret));
        return -1;
    }

    return 0;
}

int cond_signal(Condition* condition) {
    if (condition == NULL) {
        return -1;
    }

    int ret = pthread_cond_signal(condition);
    if (ret != 0) {
        fprintf(stderr, "pthread_cond_signal failed: %s\n", strerror(ret));
        return -1;
    }

    return 0;
}

int cond_broadcast(Condition* condition) {
    if (condition == NULL) {
        return -1;
    }

    int ret = pthread_cond_broadcast(condition);
    if (ret != 0) {
        fprintf(stderr, "pthread_cond_broadcast failed: %s\n", strerror(ret));
        return -1;
    }

    return 0;
}

int cond_wait(Condition* condition, Lock* lock) {
    if (condition == NULL || lock == NULL) {
        return -1;
    }

    int ret = pthread_cond_wait(condition, lock);
    if (ret != 0) {
        fprintf(stderr, "pthread_cond_wait failed: %s\n", strerror(ret));
        return -1;
    }
    return 0;
}

int cond_wait_timeout(Condition* condition, Lock* lock, int timeout_ms) {
    if (condition == NULL || lock == NULL) {
        return -1;
    }

    if (timeout_ms < 0) {
        // Negative timeout means wait indefinitely
        return cond_wait(condition, lock);
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        fprintf(stderr, "clock_gettime failed: %s\n", strerror(errno));
        return -1;
    }

    // Add timeout to current time
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;

    // Handle nanosecond overflow
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    int ret = pthread_cond_timedwait(condition, lock, &ts);
    switch (ret) {
        case 0:
            return 0;
        case ETIMEDOUT:
            return -1;
        case EINVAL:
            fprintf(stderr, "Invalid parameters in pthread_cond_timedwait\n");
            return -1;
        default:
            fprintf(stderr, "pthread_cond_timedwait failed: %s\n", strerror(ret));
            return -1;
    }
}

int cond_free(Condition* condition) {
    if (condition == NULL) {
        return 0;  // NULL is valid for cleanup functions
    }

    int ret = pthread_cond_destroy(condition);
    if (ret != 0) {
        fprintf(stderr, "pthread_cond_destroy failed: %s\n", strerror(ret));
        return -1;
    }

    return 0;
}

#endif
