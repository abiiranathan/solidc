/**
 * @file thread.h
 * @brief Cross-platform thread management and system information utilities.
 *
 * Provides a unified interface for thread creation, synchronization, and system
 * information queries across Windows and POSIX systems. All thread functions
 * return 0 on success and non-zero on error for consistency with POSIX conventions.
 *
 * Thread safety is documented per function. Generally, thread creation and
 * attribute management functions are thread-safe, but individual thread
 * attribute objects should not be modified concurrently.
 *
 * @note This library requires linking with pthread on POSIX systems (-lpthread).
 */

#ifndef SOLIDC_THREADS_H
#define SOLIDC_THREADS_H

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // for nanosleep and extended POSIX features
#endif

#include <stdint.h>  // for uintptr_t

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <errno.h>     // for error constants
#include <grp.h>       // for getgrgid
#include <pthread.h>   // for pthread functions
#include <pwd.h>       // for getpwuid
#include <sys/stat.h>  // for file operations
#include <sys/wait.h>  // for process waiting
#include <unistd.h>    // for POSIX system calls
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Type Definitions */

#ifdef _WIN32
/** Platform-specific thread handle (Windows HANDLE). */
typedef HANDLE Thread;

/** Thread attribute configuration for Windows. */
typedef struct ThreadAttr {
    SECURITY_ATTRIBUTES sa; /**< Security attributes for thread creation. */
    DWORD stackSize;        /**< Stack size in bytes (0 = system default). */
} ThreadAttr;
#else
/** Platform-specific thread handle (POSIX pthread_t). */
typedef pthread_t Thread;

/** Thread attribute configuration (POSIX pthread_attr_t). */
typedef pthread_attr_t ThreadAttr;
#endif

/**
 * Thread start routine function pointer type.
 * @param arg User-provided argument passed to the thread.
 * @return Thread exit value (can be retrieved via thread_join).
 * @note The return value should be a pointer or integer cast to void*.
 */
typedef void* (*ThreadStartRoutine)(void* arg);

/* Thread Management Functions */

/**
 * Creates a new thread with default attributes.
 * @param thread Pointer to store the created thread handle. Must not be NULL.
 * @param start_routine Function to execute in the new thread. Must not be NULL.
 * @param data Argument to pass to the start routine. Can be NULL.
 * @return 0 on success, non-zero error code on failure.
 * @note Thread-safe: Yes.
 * @note Created thread is joinable by default. Call thread_detach() to make it detached.
 */
int thread_create(Thread* thread, ThreadStartRoutine start_routine, void* data);

/**
 * Creates a new thread with custom attributes.
 * @param thread Pointer to store the created thread handle. Must not be NULL.
 * @param attr Pointer to initialized thread attributes. Must not be NULL.
 * @param start_routine Function to execute in the new thread. Must not be NULL.
 * @param data Argument to pass to the start routine. Can be NULL.
 * @return 0 on success, non-zero error code on failure.
 * @note Thread-safe: Yes.
 * @note The attr object can be destroyed immediately after this call returns.
 */
int thread_create_attr(Thread* thread, ThreadAttr* attr, ThreadStartRoutine start_routine,
                       void* data);

/**
 * Waits for a thread to terminate and retrieves its exit value.
 * @param tid Thread handle to wait for.
 * @param retval Pointer to store thread's return value. Can be NULL if not needed.
 * @return 0 on success, non-zero error code on failure.
 * @note Thread-safe: Yes, but each thread can only be joined once.
 * @note On Windows, closes the thread handle automatically after join.
 * @note Joining a detached thread results in undefined behavior.
 */
int thread_join(Thread tid, void** retval);

/**
 * Detaches a thread, allowing system to reclaim resources when it terminates.
 * @param tid Thread handle to detach.
 * @return 0 on success, non-zero error code on failure.
 * @note Thread-safe: Yes.
 * @note On Windows, this is a no-op as threads are always "detached" by default.
 * @note After detaching, the thread cannot be joined.
 */
int thread_detach(Thread tid);

#ifdef _WIN32
/**
 * Returns the current thread's ID (Windows-specific).
 * @return Current thread ID as a DWORD.
 * @note Thread-safe: Yes.
 * @note This is the Windows thread ID, not the thread handle.
 */
DWORD thread_self(void);
#else
/**
 * Returns the current thread's handle (POSIX-specific).
 * @return Current thread handle as pthread_t.
 * @note Thread-safe: Yes.
 * @note This returns the pthread_t handle, not a numeric ID.
 */
pthread_t thread_self(void);
#endif

/* Thread Attribute Management */

/**
 * Initializes thread attributes to default values.
 * @param attr Pointer to thread attributes structure. Must not be NULL.
 * @return 0 on success, non-zero error code on failure.
 * @note Thread-safe: Yes (different attr objects), No (same attr object).
 * @note Must call thread_attr_destroy() to release resources.
 */
int thread_attr_init(ThreadAttr* attr);

/**
 * Destroys thread attributes and releases associated resources.
 * @param attr Pointer to initialized thread attributes. Must not be NULL.
 * @return 0 on success, non-zero error code on failure.
 * @note Thread-safe: Yes (different attr objects), No (same attr object).
 * @note On Windows, this is a no-op but should still be called for portability.
 */
int thread_attr_destroy(ThreadAttr* attr);

/* Utility Functions */

/**
 * Suspends execution of the calling thread for specified milliseconds.
 * @param ms Number of milliseconds to sleep. Must be non-negative.
 * @note Thread-safe: Yes.
 * @note Other threads continue to run during sleep.
 * @note Actual sleep time may be longer due to system scheduling.
 */
void sleep_ms(int ms);

/* System Information Functions */

/**
 * Returns the current process ID.
 * @return Process ID as an integer.
 * @note Thread-safe: Yes.
 * @note Process ID remains constant throughout the process lifetime.
 */
int get_pid(void);

/**
 * Returns the current thread ID as an unsigned long.
 * @return Thread ID as unsigned long for cross-platform compatibility.
 * @note Thread-safe: Yes.
 * @note On Windows, returns the thread ID. On POSIX, returns pthread_t cast to unsigned long.
 */
unsigned long get_tid(void);

/**
 * Returns the number of available CPU cores.
 * @return Number of CPU cores, or -1 on error.
 * @note Thread-safe: Yes.
 * @note Returns online processors, which may be less than total processors.
 */
long get_ncpus(void);

#ifndef _WIN32
/**
 * Returns the parent process ID (POSIX only).
 * @return Parent process ID as an integer.
 * @note Thread-safe: Yes.
 * @note Not available on Windows.
 */
int get_ppid(void);

/**
 * Returns the current user ID (POSIX only).
 * @return User ID as unsigned integer.
 * @note Thread-safe: Yes.
 * @note Not available on Windows.
 */
unsigned int get_uid(void);

/**
 * Returns the current group ID (POSIX only).
 * @return Group ID as unsigned integer.
 * @note Thread-safe: Yes.
 * @note Not available on Windows.
 */
unsigned int get_gid(void);

/**
 * Returns the current user name (POSIX only).
 * @return Pointer to static string containing username, or NULL on error.
 * @note Thread-safe: No (returns pointer to static data).
 * @note Caller should not modify or free the returned string.
 * @note Not available on Windows.
 */
char* get_username(void);

/**
 * Returns the current group name (POSIX only).
 * @return Pointer to static string containing group name, or NULL on error.
 * @note Thread-safe: No (returns pointer to static data).
 * @note Caller should not modify or free the returned string.
 * @note Not available on Windows.
 */
char* get_groupname(void);
#endif  // _WIN32

#ifdef __cplusplus
}
#endif

#endif /* SOLIDC_THREADS_H */
