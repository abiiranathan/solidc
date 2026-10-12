# Concurrency and parallel pipelines

Headers: `<solidc/thread.h>`, `<solidc/threadpool.h>`, `<solidc/lock.h>`,
`<solidc/rwlock.h>`, `<solidc/spinlock.h>`, `<solidc/channels.h>`,
`<solidc/chan_patterns.h>`, `<solidc/pipeline.h>`.

## Threads (`thread.h`)

```c
#ifdef _WIN32
typedef HANDLE Thread;  typedef struct ThreadAttr { SECURITY_ATTRIBUTES sa; DWORD stackSize; } ThreadAttr;
#else
typedef pthread_t Thread;  typedef pthread_attr_t ThreadAttr;
#endif
typedef void* (*ThreadStartRoutine)(void*);

int  thread_create(Thread* t, ThreadStartRoutine fn, void* data);       /* 0 = ok */
int  thread_create_attr(Thread* t, ThreadAttr* a, ThreadStartRoutine fn, void* data);
int  thread_join(Thread t, void** retval);
int  thread_detach(Thread t);
void thread_exit(void* retval) NORETURN;
int  thread_attr_init/destroy(ThreadAttr*);
void sleep_ms(int ms);

int get_pid(void);  unsigned long get_tid(void);  long get_ncpus(void);  int get_ppid(void);
unsigned int get_uid/gid(void);  char* get_username/groupname(void);
```

Threads are joinable by default; join or detach exactly once. Return values on
Windows are truncated to 32 bits — portable code should return integer codes.
`get_username`/`get_groupname` return static buffers (copy to keep/share).
POSIX requires `-lpthread` (linked by the target).

## Thread pool (`threadpool.h`) — work-stealing, no per-task allocation

```c
Threadpool* threadpool_create(size_t num_threads);          /* 0 clamps to 1 */
bool   threadpool_submit(Threadpool* p, void (*fn)(void*), void* arg);
size_t threadpool_submit_batch(Threadpool* p, void (**fns)(void*), void** args, size_t count);
void   threadpool_wait(Threadpool* p);
void   threadpool_destroy(Threadpool* p, int timeout_ms);   /* -1 = wait forever */
```

- All functions are safe to call concurrently, **including from inside a task**.
- Use `threadpool_submit_batch` for bulk work (one global mutex acquisition; no
  per-task heap allocation). `NULL` function entries are skipped; `args == NULL`
  gives every task a `NULL` arg; returns the number enqueued. Treat a short
  return as fatal.
- `threadpool_wait` returns when all tasks (including follow-ons) complete; the
  pool remains usable.
- **Never call `threadpool_wait` or `threadpool_destroy` from inside a task** —
  deadlock (the calling task holds an active slot). Never submit after destroy.
- `arg` ownership stays with the caller; the function pointer and arg are copied
  into the queue, so they need not outlive the call.

## Lock + condition (`lock.h`)

```c
#ifdef _WIN32
typedef CRITICAL_SECTION Lock;  typedef CONDITION_VARIABLE Condition;
#else
typedef pthread_mutex_t Lock;   typedef pthread_cond_t Condition;
#endif

int lock_init/free(Lock*);   int lock_acquire/release/try_acquire(Lock*);
int cond_init/free(Condition*);
int cond_signal/broadcast(Condition*);
int cond_wait(Condition*, Lock*);
int cond_wait_timeout(Condition*, Lock*, int timeout_ms);   /* <0 = infinite */
int lock_wait(Lock*, Condition*, int timeout_ms);           /* arg order differs! */
```

`0` success, `-1` failure. Init/destroy not thread-safe; acquire/release/
signal/wait are. Always re-check the predicate in a `while` loop around
`cond_wait`. Create timed conditions only via `cond_init` (it uses
`CLOCK_MONOTONIC`, except macOS where it uses realtime) — do not mix clocks.
`lock_free(NULL)`/`cond_free(NULL)` are safe no-ops.

## RW lock (`rwlock.h`) / spinlock (`spinlock.h`)

`rwlock_t` (`SRWLOCK` / `pthread_rwlock_t`): `rwlock_init` (bool), `rwlock_rdlock`,
`rwlock_wrlock`, `rwlock_unlock_rd`, `rwlock_unlock_wr`, `rwlock_destroy`.
No allocation. Destroy only when no thread holds it.

`fast_rwlock_t` is a busy-wait atomic RW spinlock: `fast_rwlock_init`,
`fast_rwlock_rdlock/wrlock/unlock_rd/unlock_wr`, `cpu_relax`. Use only for very
short critical sections (< ~100 µs); writer starvation is possible. **In C11,
include `<stdbool.h>` before `<solidc/spinlock.h>`** (the header uses `true`/
`false` without including it).

## Channels (`channels.h`) — Go-style, internally synchronized

```c
typedef enum { CHAN_OK, CHAN_CLOSED, CHAN_FULL, CHAN_EMPTY, CHAN_TIMEOUT,
               CHAN_INVALID, CHAN_NOMEM, CHAN_WOULD_BLOCK } ChanStatus;

Channel*   chan_new(size_t value_size, size_t capacity_hint);  /* 0 => 16, rounded to pow2 */
ChanStatus chan_send(Channel*, const void* value);             /* grows; may CHAN_NOMEM */
bool       chan_recv(Channel*, void* out);                     /* false when closed+drained */
ChanStatus chan_try_send(Channel*, const void* value);         /* CHAN_FULL, never grows */
ChanStatus chan_try_recv(Channel*, void* out);
ChanStatus chan_recv_timeout(Channel*, void* out, int timeout_ms);
ChanStatus chan_close(Channel*);                               /* idempotent */
size_t     chan_len(Channel*);  size_t chan_cap(Channel*);
bool       chan_is_closed(Channel*);  size_t chan_value_size(Channel*);
void       chan_free(Channel*);
```

Values are copied by value (`value_size` bytes). FIFO. Safe for any number of
producers/consumers. After close, receives drain buffered values then return
false; double close returns `CHAN_CLOSED` (the header comment claiming
`CHAN_OK` is wrong). Free only after all users have stopped.

## Channel patterns (`chan_patterns.h`)

- Pub/sub: `chan_pubsub_new(value_size, max_subs (1..1024), queue_depth)`,
  `chan_pubsub_subscribe` (id or `CHAN_PUBSUB_NO_SUB`), `chan_pubsub_publish`
  (`CHAN_FULL` if a subscriber queue is full), `chan_pubsub_queue`
  (**hub-owned** `Channel*`, never `chan_free` it), `chan_pubsub_unsubscribe`,
  `chan_pubsub_close` (idempotent), `chan_pubsub_free`.
- Worker pool: `chan_workers_new(worker_count, value_size, queue_depth,
  chan_work_fn fn, void* user)`, `chan_workers_submit` (grows, must be called
  before shutdown), `chan_workers_pending`, `chan_workers_shutdown` (drains and
  joins; call once), `chan_workers_free`.
- `int chan_select(Channel** chans, void** outs, size_t count, int timeout_ms)`
  — up to `CHAN_SELECT_MAX` (32); returns winning index or `CHAN_SELECT_TIMEOUT`.
  `outs == NULL` allocates a scratch buffer internally; pass `outs` to stay
  allocation-free. Bad args also return `CHAN_SELECT_TIMEOUT`.
- `ChanStatus chan_merge(Channel* dest, Channel** sources, size_t count)` —
  detached forwarder per source, closes `dest` when all sources close. Keep
  `dest` alive until then.

## Process pipeline (`pipeline.h`)

```c
typedef struct CommandNode { char** args; struct CommandNode* next; } CommandNode;
CommandNode* create_command_node(char** args);   /* does NOT copy args; may exit() on OOM */
void         build_pipeline(CommandNode** commands);   /* NULL-terminated array; links ->next */
void         execute_pipeline(CommandNode* head, int output_fd);  /* blocks; may exit() on error */
void         free_pipeline(CommandNode* head);   /* frees nodes only, not args */
```

POSIX uses `posix_spawnp`, Windows `CreateProcess` (≤ `MAXIMUM_WAIT_OBJECTS`
stages). No error return channel — internal failures call `exit`. Do not use in
reusable library code; prefer `process.h` directly when you need error handling.
