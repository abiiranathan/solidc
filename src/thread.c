#include "../include/thread.h"
#include <stddef.h>  // for NULL
#include <stdio.h>   // for perror, fprintf
#include <stdlib.h>  // for malloc, free
#include <string.h>  // for strerror
#include <time.h>    // for nanosleep

#ifdef _WIN32
#include <process.h>  // for Windows threading
#else
#include <sys/syscall.h>  // for syscall numbers
#endif

/* Windows-specific thread parameter wrapper */
#ifdef _WIN32
/** Internal structure to pass parameters to Windows thread start wrapper. */
typedef struct {
    ThreadStartRoutine start_routine; /**< User's thread start function. */
    void* data;                       /**< User's thread argument. */
} ThreadParams;

/**
 * Internal Windows thread start wrapper.
 * Converts between Windows WINAPI calling convention and our generic interface.
 * @param lpParameter Pointer to ThreadParams structure.
 * @return Thread exit code as DWORD.
 */
DWORD WINAPI thread_start_wrapper(LPVOID lpParameter) {
    if (lpParameter == NULL) {
        return (DWORD)-1;  // Invalid parameter
    }

    ThreadParams* params             = (ThreadParams*)lpParameter;
    ThreadStartRoutine start_routine = params->start_routine;
    void* data                       = params->data;

    free(params);  // Free the parameter structure immediately

    if (start_routine == NULL) {
        return (DWORD)-1;  // Invalid start routine
    }

    void* result = start_routine(data);

    // Convert pointer to DWORD safely
    return (DWORD)(uintptr_t)result;
}
#endif

int thread_create(Thread* thread, ThreadStartRoutine start_routine, void* data) {
    if (thread == NULL || start_routine == NULL) {
        return EINVAL;  // Invalid parameter
    }

#ifdef _WIN32
    ThreadParams* params = (ThreadParams*)malloc(sizeof(ThreadParams));
    if (params == NULL) {
        return ENOMEM;  // Out of memory
    }

    params->start_routine = start_routine;
    params->data          = data;

    HANDLE handle = CreateThread(NULL,                  // Security attributes (default)
                                 0,                     // Stack size (default)
                                 thread_start_wrapper,  // Thread start function
                                 params,                // Thread parameter
                                 0,                     // Creation flags (run immediately)
                                 NULL                   // Thread ID (not needed)
    );

    if (handle == NULL) {
        free(params);
        DWORD error = GetLastError();
        fprintf(stderr, "CreateThread failed with error: %lu\n", error);
        return (int)error;
    }

    *thread = handle;
    return 0;

#else  // POSIX
    int ret = pthread_create(thread, NULL, start_routine, data);
    if (ret != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
    }
    return ret;
#endif
}

int thread_create_attr(Thread* thread, ThreadAttr* attr, ThreadStartRoutine start_routine,
                       void* data) {
    if (thread == NULL || attr == NULL || start_routine == NULL) {
        return EINVAL;  // Invalid parameter
    }

#ifdef _WIN32
    ThreadParams* params = (ThreadParams*)malloc(sizeof(ThreadParams));
    if (params == NULL) {
        return ENOMEM;  // Out of memory
    }

    params->start_routine = start_routine;
    params->data          = data;

    HANDLE handle = CreateThread(&attr->sa,             // Security attributes
                                 attr->stackSize,       // Stack size
                                 thread_start_wrapper,  // Thread start function
                                 params,                // Thread parameter
                                 0,                     // Creation flags (run immediately)
                                 NULL                   // Thread ID (not needed)
    );

    if (handle == NULL) {
        free(params);
        DWORD error = GetLastError();
        fprintf(stderr, "CreateThread failed with error: %lu\n", error);
        return (int)error;
    }

    *thread = handle;
    return 0;

#else  // POSIX
    int ret = pthread_create(thread, attr, start_routine, data);
    if (ret != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
    }
    return ret;
#endif
}

int thread_join(Thread tid, void** retval) {
#ifdef _WIN32
    DWORD wait_result = WaitForSingleObject(tid, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        DWORD error = GetLastError();
        fprintf(stderr, "WaitForSingleObject failed with error: %lu\n", error);
        return (int)error;
    }

    if (retval != NULL) {
        DWORD exit_code;
        if (GetExitCodeThread(tid, &exit_code)) {
            *retval = (void*)(uintptr_t)exit_code;
        } else {
            *retval     = NULL;
            DWORD error = GetLastError();
            fprintf(stderr, "GetExitCodeThread failed with error: %lu\n", error);
            // Continue anyway - we successfully waited
        }
    }

    CloseHandle(tid);  // Clean up the handle
    return 0;

#else  // POSIX
    int ret = pthread_join(tid, retval);
    if (ret != 0) {
        fprintf(stderr, "pthread_join failed: %s\n", strerror(ret));
    }
    return ret;
#endif
}

int thread_detach(Thread tid) {
#ifdef _WIN32
    // Windows threads are always "detached" in the POSIX sense
    // Just close the handle to release resources
    if (CloseHandle(tid)) {
        return 0;
    } else {
        DWORD error = GetLastError();
        fprintf(stderr, "CloseHandle failed with error: %lu\n", error);
        return (int)error;
    }

#else  // POSIX
    int ret = pthread_detach(tid);
    if (ret != 0) {
        fprintf(stderr, "pthread_detach failed: %s\n", strerror(ret));
    }
    return ret;
#endif
}

#ifdef _WIN32
DWORD thread_self(void) {
    return GetCurrentThreadId();
}
#else
pthread_t thread_self(void) {
    return pthread_self();
}
#endif

/* Thread Attribute Management */

int thread_attr_init(ThreadAttr* attr) {
    if (attr == NULL) {
        return EINVAL;  // Invalid parameter
    }

#ifdef _WIN32
    // Initialize to safe defaults
    attr->stackSize               = 0;  // Use system default stack size
    attr->sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    attr->sa.lpSecurityDescriptor = NULL;   // Default security
    attr->sa.bInheritHandle       = FALSE;  // Don't inherit handles by default
    return 0;

#else  // POSIX
    int ret = pthread_attr_init(attr);
    if (ret != 0) {
        fprintf(stderr, "pthread_attr_init failed: %s\n", strerror(ret));
    }
    return ret;
#endif
}

int thread_attr_destroy(ThreadAttr* attr) {
    if (attr == NULL) {
        return EINVAL;  // Invalid parameter
    }

#ifdef _WIN32
    // No-op on Windows, but still validate parameter
    (void)attr;  // Suppress unused parameter warning
    return 0;

#else  // POSIX
    int ret = pthread_attr_destroy(attr);
    if (ret != 0) {
        fprintf(stderr, "pthread_attr_destroy failed: %s\n", strerror(ret));
    }
    return ret;
#endif
}

/* Utility Functions */

void sleep_ms(int ms) {
    if (ms <= 0) {
        return;  // Invalid or zero sleep time
    }

#ifdef _WIN32
    Sleep((DWORD)ms);

#else  // POSIX
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;  // Convert remaining ms to nanoseconds

    // Handle EINTR by continuing to sleep for remaining time
    struct timespec remaining;
    while (nanosleep(&ts, &remaining) == -1) {
        if (errno != EINTR) {
            break;  // Some other error occurred
        }
        ts = remaining;  // Sleep for remaining time
    }
#endif
}

int get_pid(void) {
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else  // POSIX
    return (int)getpid();
#endif
}

unsigned long get_tid(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentThreadId();
#else  // POSIX
    return pthread_self();
#endif
}

long get_ncpus(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (long)sysinfo.dwNumberOfProcessors;

#else  // POSIX
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpus == -1) {
        perror("sysconf(_SC_NPROCESSORS_ONLN)");
    }
    return ncpus;
#endif
}

#ifndef _WIN32
/* POSIX-only System Information Functions */

int get_ppid(void) {
    return (int)getppid();
}

unsigned int get_uid(void) {
    return (unsigned int)getuid();
}

unsigned int get_gid(void) {
    return (unsigned int)getgid();
}

char* get_username(void) {
    struct passwd* pw = getpwuid(getuid());
    if (pw == NULL) {
        perror("getpwuid");
        return NULL;
    }
    return pw->pw_name;
}

char* get_groupname(void) {
    struct group* gr = getgrgid(getgid());
    if (gr == NULL) {
        perror("getgrgid");
        return NULL;
    }
    return gr->gr_name;
}

#endif  // _WIN32
