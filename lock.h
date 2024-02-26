#ifndef AB1DC3C5_00AA_4460_BD6A_65D8301B4779
#define AB1DC3C5_00AA_4460_BD6A_65D8301B4779
// Cross-platform function wrapper for syncronization primitives

#ifdef _WIN32
#include <windows.h>

// Lock is a CRITICAL_SECTION
typedef CRITICAL_SECTION Lock;

// Condition is a CONDITION_VARIABLE
typedef CONDITION_VARIABLE Condition;

#else
#include <pthread.h>

// Lock is a pthread_mutex_t
typedef pthread_mutex_t Lock;

// Condition is a pthread_cond_t
typedef pthread_cond_t Condition;
#endif

// Initializes a lock
void lock_init(Lock* lock);

// Acquires a lock
void lock_acquire(Lock* lock);

// Releases a lock
void lock_release(Lock* lock);

// Frees a lock
void lock_free(Lock* lock);

// Tries to acquire a lock.
// Returns 0 if the lock was acquired, 1 if it was not.
int lock_try_acquire(Lock* lock);

// Waits for a condition to be signaled or a timeout to occur.
// timeout is in milliseconds
void lock_wait(Lock* lock, Condition* condition, int timeout);

// Initializes a condition variable
void cond_init(Condition* condition);

// Signals a condition variable
void cond_signal(Condition* condition);

// Broadcasts a condition variable
void cond_broadcast(Condition* condition);

// Waits for a condition to be signaled indefinitely
void cond_wait(Condition* condition, Lock* lock);

// Waits for a condition to be signaled or a timeout to occur.
void cond_wait_timeout(Condition* condition, Lock* lock, int timeout);

// Frees a condition variable
void cond_free(Condition* condition);

#ifdef OS_IMPL
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
}

#else
void lock_init(Lock* lock) {
    pthread_mutex_init(lock, NULL);
}

void lock_acquire(Lock* lock) {
    pthread_mutex_lock(lock);
}

void lock_release(Lock* lock) {
    pthread_mutex_unlock(lock);
}

void lock_free(Lock* lock) {
    pthread_mutex_destroy(lock);
}

int lock_try_acquire(Lock* lock) {
    return pthread_mutex_trylock(lock);
}

void lock_wait(Lock* lock, Condition* condition, int timeout) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (timeout % 1000) * 1000000;
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
    ts.tv_nsec += (timeout % 1000) * 1000000;
    pthread_cond_timedwait(condition, lock, &ts);
}

void cond_free(Condition* condition) {
    pthread_cond_destroy(condition);
}

#endif  // _WIN32

#endif  // OS_IMPL

#endif /* AB1DC3C5_00AA_4460_BD6A_65D8301B4779 */
