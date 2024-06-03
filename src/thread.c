#include "../include/thread.h"
#include <time.h>

#ifdef _WIN32
typedef struct {
    ThreadStartRoutine start_routine;
    void* data;
} ThreadParams;

DWORD WINAPI thread_start_wrapper(LPVOID lpParameter) {
    ThreadParams* params = (ThreadParams*)lpParameter;
    void* result = params->start_routine(params->data);

    free(params);  // Free the parameter
    // Use uintptr_t to ensure proper casting
    return (DWORD)(uintptr_t)result;
}
#endif

int thread_create(Thread* thread, ThreadStartRoutine start_routine, void* data) {
    int ret = -1;
#ifdef _WIN32
    ThreadParams* params = (ThreadParams*)malloc(sizeof(ThreadParams));
    if (!params) {
        perror("malloc");
        return -1;
    }
    params->start_routine = start_routine;
    params->data = data;
    HANDLE t = CreateThread(NULL, 0, thread_start_wrapper, params, 0, NULL);
    if (t) {
        *thread = t;
        ret = 0;
    }
#else
    ret = pthread_create(thread, NULL, start_routine, data);
#endif
    return ret;
}

// Init thread attributes
int thread_attr_init(ThreadAttr* attr) {
    int ret = -1;
#ifdef _WIN32
    attr->stackSize = 0;
    attr->sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    attr->sa.lpSecurityDescriptor = NULL;
    attr->sa.bInheritHandle = TRUE;
    ret = 0;
#else
    ret = pthread_attr_init(attr);
#endif
    return ret;
}

// Destroy thread attributes
int thread_attr_destroy(ThreadAttr* attr) {
    int ret = -1;
#ifdef _WIN32
    (void)attr;
    ret = 0;
#else
    ret = pthread_attr_destroy(attr);
#endif
    return ret;
}

// Create a new thread with attributes
int thread_create_attr(Thread* thread, ThreadAttr* attr, ThreadStartRoutine start_routine,
                       void* data) {
    int ret = -1;

#ifdef _WIN32
    ThreadParams* params = (ThreadParams*)malloc(sizeof(ThreadParams));
    if (!params) {
        perror("malloc");
        return -1;
    }
    params->start_routine = start_routine;
    params->data = data;
    HANDLE t = CreateThread(&attr->sa, attr->stackSize, thread_start_wrapper, params, 0, NULL);
    if (t) {
        *thread = t;
        ret = 0;
    }
#else
    ret = pthread_create(thread, attr, start_routine, data);
#endif
    return ret;
}

// Wait for this thread to terminate.
int thread_join(Thread tid, void** retval) {
    int ret = -1;
#ifdef _WIN32
    if (WaitForSingleObject(tid, INFINITE) == WAIT_OBJECT_0) {
        if (retval) {
            GetExitCodeThread(tid, (DWORD*)retval);
        }
        CloseHandle(tid);
        ret = 0;
    }
#else
    // Unix
    if (pthread_join(tid, retval) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Get the current thread id
#ifdef _WIN32
DWORD thread_self(void) {
    return GetCurrentThreadId();
}
#else
// Get the current thread id
pthread_t thread_self(void) {
    return pthread_self();
}
#endif

// Detach a thread after creation.
// Detach a thread. Returns 0 if successful, -1 otherwise
// This does nothing on Windows.
int thread_detach(Thread tid) {
#ifdef _WIN32
    // Windows does not have detached threads.
    (void)tid;
    return 0;
#else
    return pthread_detach(tid);
#endif
}

// Sleep for a number of milliseconds
void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

// Get the current process id
int get_pid(void) {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

// Get the current thread id
int get_tid(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

// Get the number of CPU cores
int get_ncpus(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

#ifndef _WIN32
// Get the parent process id
int get_ppid(void) {
    return getppid();
}

// Get the current user id
int get_uid(void) {
    return getuid();
}

// Get the current group id
int get_gid(void) {
    return getgid();
}

// Get the current user name
char* get_username(void) {
    return getpwuid(getuid())->pw_name;
}

// Get the current group name
char* get_groupname(void) {
    return getgrgid(getgid())->gr_name;
}
#endif
