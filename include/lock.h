#ifndef AB1DC3C5_00AA_4460_BD6A_65D8301B4779
#define AB1DC3C5_00AA_4460_BD6A_65D8301B4779
// Cross-platform function wrapper for syncronization primitives

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

/// Initializes a lock
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

#ifdef __cplusplus
}
#endif

#endif /* AB1DC3C5_00AA_4460_BD6A_65D8301B4779 */
