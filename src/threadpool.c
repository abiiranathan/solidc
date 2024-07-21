#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#ifdef THPOOL_DEBUG
#define THPOOL_DEBUG 1
#else
#define THPOOL_DEBUG 0
#endif

#if !defined(DISABLE_PRINT) || defined(THPOOL_DEBUG)
#define err(str) fprintf(stderr, str)
#else
#define err(str)
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif
#endif

/* Thread and synchronization primitives abstraction */
#ifdef _WIN32
#define THREAD HANDLE
#define THREAD_CREATE                                                                              \
    (tid, func, arg)(*(tid) = (HANDLE)_beginthreadex(NULL, 0, func, arg, CREATE_SUSPENDED, NULL))
#define THREAD_DETACH(tid) CloseHandle(tid)
#define THREAD_RESUME(tid) ResumeThread(tid)

#define MUTEX HANDLE
#define MUTEX_INIT(m) ((*(m) = CreateMutex(NULL, FALSE, NULL)) == NULL)
#define MUTEX_DESTROY(m) CloseHandle(m)
#define MUTEX_LOCK(m) WaitForSingleObject(*(m), INFINITE)
#define MUTEX_UNLOCK(m) ReleaseMutex(*(m))
#define SLEEP(sec) Sleep((sec) * 1000)

#define COND HANDLE
#define COND_INIT(c) ((*(c) = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
#define COND_DESTROY(c) CloseHandle(c)
#define COND_SIGNAL(c) SetEvent(*(c))
#define COND_BROADCAST(c) PulseEvent(*(c))
#define COND_WAIT(c, m)                                                                            \
    {                                                                                              \
        MUTEX_UNLOCK(m);                                                                           \
        WaitForSingleObject(*(c), INFINITE);                                                       \
        MUTEX_LOCK(m);                                                                             \
    }
#else
#define THREAD pthread_t
#define THREAD_CREATE(tid, func, arg) pthread_create(tid, NULL, func, arg)
#define THREAD_DETACH(tid) pthread_detach(tid)
#define THREAD_RESUME(tid)

#define MUTEX pthread_mutex_t
#define MUTEX_INIT(m) pthread_mutex_init(m, NULL)
#define MUTEX_DESTROY(m) pthread_mutex_destroy(m)
#define MUTEX_LOCK(m) pthread_mutex_lock(m)
#define MUTEX_UNLOCK(m) pthread_mutex_unlock(m)

#define COND pthread_cond_t
#define COND_INIT(c) pthread_cond_init(c, NULL)
#define COND_DESTROY(c) pthread_cond_destroy(c)
#define COND_SIGNAL(c) pthread_cond_signal(c)
#define COND_BROADCAST(c) pthread_cond_broadcast(c)
#define COND_WAIT(c, m) pthread_cond_wait(c, m)
#define SLEEP(sec) sleep(sec)
#endif

// Job represents a work item that should be executed by a thread in the thread pool
typedef struct job {
    struct job* next;             // Pointer to the next job
    void (*function)(void* arg);  // Function to be executed
    void* arg;                    // Argument to be passed to the function
} job;

// Job queue is a linked list of jobs that are waiting to be executed
// It's a linked list because we need to add jobs to the end of the queue
// and keep track of the front of the queue to pull jobs from it
typedef struct jobqueue {
    MUTEX rwmutex;  // Mutex for the job queue
    job* front;     // Pointer to the front of the queue
    job* rear;      // Pointer to the rear of the queue
    COND has_jobs;  // Semaphore to signal that there are jobs in the queue
    int len;        // Number of jobs in the queue
} jobqueue;

// thread represents a cross-platform wrapper around a thread
typedef struct thread {
    int id;                   // Thread ID
    struct threadpool* pool;  // Pointer to the thread pool
    THREAD pthread;           // Thread handle
} thread;

// threadpool represents a thread pool that manages a group of threads
typedef struct threadpool {
    thread** threads;                  // Array of threads
    volatile int num_threads_alive;    // Number of threads that are currently alive
    volatile int num_threads_working;  // Number of threads that are currently working
    MUTEX thcount_lock;                // Mutex  for num_threads_alive
    COND threads_all_idle;             // Condition variable for signaling idle threads
    jobqueue jobqueue;                 // Linked list for the queue of jobs
} threadpool;

static volatile int threads_keepalive;  // Flag to keep threads alive
static volatile int threads_on_hold;    // Flag to pause threads

// ================ static functions declarations ====================
static int thread_init(threadpool* pool, struct thread** thp, int id);

#ifdef _WIN32
static int threadWorker(struct thread* thp);
#else
static void* threadWorker(struct thread* thp);
#endif

static void thread_hold(int sig_id);
static void thread_destroy(struct thread* thp);

static int jobqueue_init(jobqueue* jobqueue_p);
static void jobqueue_clear(jobqueue* jobqueue_p);
static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob_p);
static struct job* jobqueue_pull(jobqueue* jobqueue_p);
static void jobqueue_destroy(jobqueue* jobqueue_p);

// ============== end ot prototypes ==========================

static int jobqueue_init(jobqueue* jobqueue_p) {
    jobqueue_p->len = 0;
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    MUTEX_INIT(&jobqueue_p->rwmutex);
    COND_INIT(&jobqueue_p->has_jobs);

    return 0;
}

static void jobqueue_clear(jobqueue* jobqueue_p) {
    while (jobqueue_p->len) {
        free(jobqueue_pull(jobqueue_p));
    }
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    jobqueue_p->len = 0;
}

static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob) {
    MUTEX_LOCK(&jobqueue_p->rwmutex);
    newjob->next = NULL;

    if (jobqueue_p->rear == NULL) {
        jobqueue_p->front = newjob;
        jobqueue_p->rear = newjob;
    } else {
        jobqueue_p->rear->next = newjob;
        jobqueue_p->rear = newjob;
    }

    jobqueue_p->len++;
    COND_SIGNAL(&jobqueue_p->has_jobs);
    MUTEX_UNLOCK(&jobqueue_p->rwmutex);
}

/** Get first job from queue(removes it from queue)
 * Notice: Caller MUST hold a mutex
 */
static struct job* jobqueue_pull(jobqueue* jobqueue_p) {
    MUTEX_LOCK(&jobqueue_p->rwmutex);

    // Wait until there are jobs in the queue
    while (jobqueue_p->len == 0 && threads_keepalive) {
        COND_WAIT(&jobqueue_p->has_jobs, &jobqueue_p->rwmutex);
    }

    job* job_p = NULL;  // Initialize job_p
    if (jobqueue_p->len > 0) {
        job_p = jobqueue_p->front;
        jobqueue_p->front = job_p->next;
        jobqueue_p->len--;
        if (jobqueue_p->len == 0) {
            jobqueue_p->rear = NULL;
        }
    }

    MUTEX_UNLOCK(&jobqueue_p->rwmutex);
    return job_p;
}

static void jobqueue_destroy(jobqueue* jobqueue_p) {
    jobqueue_clear(jobqueue_p);
    MUTEX_DESTROY(&jobqueue_p->rwmutex);
    COND_DESTROY(&jobqueue_p->has_jobs);
}

static void thread_hold(int sig_id) {
    (void)sig_id;
    threads_on_hold = 1;
    while (threads_on_hold) {
        SLEEP(1);
    }
}

static int thread_init(threadpool* pool, thread** thp, int id) {
    *thp = (thread*)malloc(sizeof(thread));
    if (*thp == NULL) {
        err("thread_init(): Could not allocate memory for thread\n");
        return -1;
    }

    (*thp)->pool = pool;
    (*thp)->id = id;

#ifdef _WIN32
    THREAD_CREATE(&(*thp)->pthread, (_beginthreadex_proc_type)threadWorker, (*thp));
#else
    THREAD_CREATE(&(*thp)->pthread, (void* (*)(void*))threadWorker, (*thp));
#endif
    return 0;
}

/* Frees a thread  */
static void thread_destroy(thread* thp) {
    if (thp == NULL) {
        return;
    }
#ifdef _WIN32
    WaitForSingleObject(thp->pthread, INFINITE);
    CloseHandle(thp->pthread);
#else
    pthread_join(thp->pthread, NULL);
#endif
    free(thp);
    thp = NULL;
}

#ifdef _WIN32
int threadWorker(thread* thp) {
#else
void* threadWorker(thread* thp) {
#endif
    char thread_name[128] = {0};

    snprintf(thread_name, 128, "thread-pool-%d", thp->id);

#if defined(__linux__)
    prctl(PR_SET_NAME, thread_name);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
    pthread_set_name_np(thp->pthread, thread_name);
#elif defined(__APPLE__)
pthread_setname_np(thread_name);
#endif

    threadpool* pool = thp->pool;

    /* Register signal handler */
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_ONSTACK;
    act.sa_handler = thread_hold;
    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        err("thread_do(): cannot handle SIGUSR1");
    }

    /* Mark thread as alive (initialized) */
    MUTEX_LOCK(&pool->thcount_lock);
    pool->num_threads_alive++;
    MUTEX_UNLOCK(&pool->thcount_lock);

#ifdef _WIN32
    THREAD_RESUME(thp->pthread);
#endif
    while (threads_keepalive) {
        job* job_p = jobqueue_pull(&pool->jobqueue);

        if (job_p) {
            MUTEX_LOCK(&pool->thcount_lock);
            pool->num_threads_working++;
            MUTEX_UNLOCK(&pool->thcount_lock);

            void (*func_buff)(void*);
            void* arg_buff;

            func_buff = job_p->function;
            arg_buff = job_p->arg;
            func_buff(arg_buff);
            free(job_p);

            MUTEX_LOCK(&pool->thcount_lock);
            pool->num_threads_working--;
            if (!pool->num_threads_working) {
                COND_SIGNAL(&pool->threads_all_idle);
            }
            MUTEX_UNLOCK(&pool->thcount_lock);
        }
    }

    // Mark the thread as finished(inactive)
    MUTEX_LOCK(&pool->thcount_lock);
    pool->num_threads_alive--;
    MUTEX_UNLOCK(&pool->thcount_lock);

    return 0;
}

threadpool* threadpool_create(int num_threads) {
    threads_on_hold = 0;
    threads_keepalive = 1;

    if (num_threads < 0) {
        num_threads = 0;
    }

    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (pool == NULL) {
        err("threadpool_create(): Could not allocate memory for thread pool\n");
        return NULL;
    }

    pool->num_threads_alive = 0;
    pool->num_threads_working = 0;

    if (jobqueue_init(&pool->jobqueue) == -1) {
        free(pool);
        return NULL;
    }

    pool->threads = (thread**)malloc(num_threads * sizeof(thread*));
    if (pool->threads == NULL) {
        err("threadpool_create(): Could not allocate memory for threads\n");
        free(pool);
        return NULL;
    }

    MUTEX_INIT(&pool->thcount_lock);
    COND_INIT(&pool->threads_all_idle);

    int n;
    for (n = 0; n < num_threads; n++) {
        thread_init(pool, &pool->threads[n], n);
    }

    for (n = 0; n < num_threads; n++) {
#ifdef _WIN32
        THREAD_RESUME(pool->threads[n]->pthread);
#endif
    }

    while (pool->num_threads_alive != num_threads) {
        SLEEP(1);
    }

    return pool;
}

void threadpool_wait(threadpool* pool) {
    MUTEX_LOCK(&pool->thcount_lock);
    while (pool->jobqueue.len || pool->num_threads_working) {
        COND_WAIT(&pool->threads_all_idle, &pool->thcount_lock);
    }
    MUTEX_UNLOCK(&pool->thcount_lock);
}

void threadpool_destroy(threadpool* pool) {
    /* No need to destroy if it's NULL */
    if (pool == NULL)
        return;

    volatile int threads_total = pool->num_threads_alive;

    /* End each thread 's infinite loop */
    threads_keepalive = 0;

    /* Give one second to kill idle threads */
    double TIMEOUT = 1.0;
    time_t start, end;
    double tpassed = 0.0;
    time(&start);
    while (tpassed < TIMEOUT && pool->num_threads_alive) {
        COND_BROADCAST(&pool->jobqueue.has_jobs);
        time(&end);
        tpassed = difftime(end, start);
    }

    /* Poll remaining threads */
    while (pool->num_threads_alive) {
        COND_BROADCAST(&pool->jobqueue.has_jobs);
        SLEEP(1);
    }

    /* Job queue cleanup */
    jobqueue_destroy(&pool->jobqueue);

    /* Deallocs */
    int n;
    for (n = 0; n < threads_total; n++) {
        thread_destroy(pool->threads[n]);
    }
    free(pool->threads);
    free(pool);

    pool = NULL;
}

int threadpool_add_task(threadpool* pool, void (*task)(void*), void* arg_p) {
    job* newjob = (job*)malloc(sizeof(job));
    if (newjob == NULL) {
        err("threadpool_add_work(): Could not allocate memory for new job\n");
        return -1;
    }

    newjob->function = task;
    newjob->arg = arg_p;

    jobqueue_push(&pool->jobqueue, newjob);
    return 0;
}

void threadpool_pause(threadpool* pool) {
    int n;
    for (n = 0; n < pool->num_threads_alive; n++) {
#ifdef _WIN32
        SuspendThread(pool->threads[n]->pthread);
#else
        pthread_kill(pool->threads[n]->pthread, SIGUSR1);
#endif
    }
}

void threadpool_resume(threadpool* pool) {
    (void)pool;
    threads_on_hold = 0;
}

int threadpool_num_threads_working(threadpool* pool) {
    return pool->num_threads_working;
}
