#ifndef A8BA3E5B_9753_47B2_8F62_85E8A97F146F
#define A8BA3E5B_9753_47B2_8F62_85E8A97F146F

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // for nanosleep
#endif

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32
// Alias to win32 HANDLE
typedef HANDLE Thread;
typedef struct ThreadAttr {
    SECURITY_ATTRIBUTES sa;
    DWORD stackSize;
} ThreadAttr;
#else
// Alias to posix pthread
typedef pthread_t Thread;
typedef pthread_attr_t ThreadAttr;
#endif

typedef void* (*ThreadStartRoutine)(void*);

// Create a new thread
int thread_create(Thread* thread, ThreadStartRoutine start_routine, void* data);

// Initialize thread attributes
int thread_attr_init(ThreadAttr* attr);

// Destroy thread attributes
int thread_attr_destroy(ThreadAttr* attr);

// Create a new thread with attributes
int thread_create_attr(Thread* thread, ThreadAttr* attr, ThreadStartRoutine start_routine,
                       void* data);

// Join a thread
int thread_join(Thread tid, void** retval);

#ifdef _WIN32
DWORD thread_self(void);
#else
// Get the current thread id
pthread_t thread_self(void);
#endif

// Detach a thread after creation.
int thread_detach(Thread tid);

// Sleep for a number of milliseconds
void sleep_ms(int ms);

// Get the current process id
int get_pid(void);

// Get the current thread id
int get_tid(void);

// Get the parent process id
int get_ppid(void);

// Get the number of CPU cores
unsigned long get_ncpus(void);

#ifndef _WIN32
// Get the current user id
unsigned int get_uid(void);

// Get the current group id
unsigned int get_gid(void);

// Get the current user name
char* get_username(void);

// Get the current group name
char* get_groupname(void);
#endif

#endif /* A8BA3E5B_9753_47B2_8F62_85E8A97F146F */
