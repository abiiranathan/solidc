# I/O, files, processes, and networking

Headers: `<solidc/file.h>`, `<solidc/filepath.h>`, `<solidc/process.h>`,
`<solidc/socket.h>`, `<solidc/epoll.h>`, `<solidc/poller.h>`,
`<solidc/stdstreams.h>`, `<solidc/xtime.h>`.

On Windows include `<solidc/platform.h>` (keeps `winsock2.h` before
`windows.h`, provides `ssize_t`). `filepath.h` and `process.h` have one public
API with POSIX/Win32 backends chosen by CMake.

## Files (`file.h`)

```c
typedef struct FileAttributes { uint32_t attrs; size_t size; time_t mtime; } FileAttributes;
/* fattr_has/is_file/is_dir/is_symlink/is_device */
typedef struct { FILE* stream; FileAttributes attr; native_handle_t native_handle; } file_t;
typedef enum { FILE_SUCCESS, FILE_ERROR_INVALID_ARGS, FILE_ERROR_OPEN_FAILED,
               FILE_ERROR_IO_FAILED, FILE_ERROR_LOCK_FAILED, FILE_ERROR_MEMORY_FAILED,
               FILE_ERROR_SYSTEM_ERROR } file_result_t;

int  populate_file_attrs(const char* path, FileAttributes* attr);
file_result_t file_open(file_t* f, const char* name, const char* mode);
void          file_close(file_t* f);
file_result_t file_truncate(file_t* f, int64_t length);
size_t file_read(const file_t*, void* buf, size_t size, size_t count);
size_t file_write(file_t*, const void* buf, size_t size, size_t count);
size_t file_write_string(file_t*, const char* s);
ssize_t file_pread(const file_t*, void* buf, size_t size, int64_t offset);
ssize_t file_pwrite(file_t*, const void* buf, size_t size, int64_t offset);
void*  file_readall(file_t* f, size_t* size_out);      /* malloc'd; free() */
file_result_t file_lock(const file_t*);   file_result_t file_unlock(const file_t*);
file_result_t file_copy(const file_t* src, file_t* dst);
void*  file_mmap(const file_t*, size_t length, bool read, bool write);
file_result_t file_munmap(void* addr, size_t length);
file_result_t file_flush(file_t*);  int64_t file_tell(const file_t*);
file_result_t file_seek(file_t*, int64_t offset, int whence);
int64_t file_sendfile(int out_fd, int in_fd, int64_t* offset, size_t count);
```

`file_read`/`file_write` follow `fread`/`fwrite` element semantics (use
`feof`/`ferror`). `file_readall` returns a `malloc` buffer (valid 1-byte buffer
for empty files), caller frees. `file_lock` is an exclusive non-blocking
advisory whole-file lock. **`file_sendfile` may send fewer bytes than requested
and must be looped** until it returns 0 or reaches EOF; on Windows `out_fd`
must be a `SOCKET` cast to `int`.

## File paths (`filepath.h`)

```c
Directory* dir_open(const char* path);   void dir_close(Directory*);
char* dir_next(Directory*);              /* internal, dot entries included */
int  dir_create(const char*);  int dir_remove(const char* path, bool recursive);
int  dir_rename(const char* oldp, const char* newp);  int dir_chdir(const char*);
char** dir_list(const char* path, size_t* count);      /* free each + array */
void   dir_list_with_callback(const char* path, void (*cb)(const char*));
bool is_dir/is_file/is_symlink(const char*);   bool filepath_makedirs(const char*);
char* get_tempdir/make_tempfile/make_tempdir(void);    /* malloc'd; free() */
int  dir_walk(const char* path, WalkDirCallback cb, void* data);
int  dir_walkx(const char* path, WalkDirCallbackX cb, void* data);
int  dir_walk_depth_first(const char* path, WalkDirCallback cb, void* data);
ssize_t dir_size(const char*);   bool path_exists(const char*);   char* get_cwd(void);
void filepath_basename/dirname/extension/nameonly(const char* path, char* out, size_t n);
char* filepath_absolute(const char*);  char* filepath_join(const char* a, const char* b);
void filepath_split(const char* path, char* dir, char* name, size_t dsz, size_t nsz);
const char* user_home_dir(void);        /* env pointer, DO NOT free */
char* filepath_expanduser(const char*); bool filepath_expanduser_buf(const char*, char*, size_t);
int filepath_remove(const char*);       int filepath_rename(const char*, const char*);
```

`WalkDirCallback` returns `DirContinue`/`DirStop`/`DirSkip`/`DirError`;
`WalkDirCallbackX` receives a `LazyFileAttributes*` valid only during the
callback (Linux `getdents64` fast path). All string-returning functions are
`WARN_UNUSED_RESULT` and `malloc`'d (except `user_home_dir`). `dir_walk` is
breadth-first, callback before recursion; `dir_walk_depth_first` after.
`dir_remove(path, true)` classifies entries with `lstat`, so symlinks are
removed as links. `filepath_basename/dirname/extension/nameonly/split/
makedirs/expanduser` accept **both `/` and `\`**.

Security: `is_dir`/`is_file`/`path_exists`/`filepath_absolute` follow symlinks
and are TOCTOU races — do not use as an authorization gate. `is_symlink` uses
`lstat` on POSIX but **always returns false on Windows**. `make_tempfile` uses
`mkstemp` (mode 0600). Recursive walks/deletes are not safe against concurrent
modification.

## Processes (`process.h`)

```c
typedef enum { PROCESS_SUCCESS = 0, PROCESS_ERROR_INVALID_ARGUMENT = -1,
               PROCESS_ERROR_FORK_FAILED, PROCESS_ERROR_EXEC_FAILED, PROCESS_ERROR_PIPE_FAILED,
               PROCESS_ERROR_MEMORY, PROCESS_ERROR_WAIT_FAILED, PROCESS_ERROR_KILL_FAILED,
               PROCESS_ERROR_PERMISSION_DENIED, PROCESS_ERROR_IO, PROCESS_ERROR_TIMEOUT,
               PROCESS_ERROR_WOULD_BLOCK, PROCESS_ERROR_PIPE_CLOSED,
               PROCESS_ERROR_TERMINATE_FAILED, PROCESS_ERROR_UNKNOWN } ProcessError;

typedef struct { const char* working_directory; bool inherit_environment;
                 const char* const* environment; bool detached;
                 struct { PipeHandle* stdin_pipe, *stdout_pipe, *stderr_pipe; bool merge_stderr; } io;
               } ProcessOptions;
typedef struct { int exit_code; bool exited_normally; int term_signal; } ProcessResult;

ProcessError process_create(ProcessHandle** h, const char* cmd,
                            const char* const argv[], const ProcessOptions* opts);
ProcessError process_wait(ProcessHandle* h, ProcessResult* r, int timeout_ms); /* -1 = block */
ProcessError process_terminate(ProcessHandle* h, bool force);
void         process_free(ProcessHandle* h);

ProcessError pipe_create(PipeHandle** p);
ProcessError pipe_read(PipeHandle*, void* buf, size_t size, size_t* n, int timeout_ms);
ProcessError pipe_write(PipeHandle*, const void* buf, size_t size, size_t* n, int timeout_ms);
PipeFd pipe_read_fd/pipe_write_fd(PipeHandle*);
bool   pipe_read_closed/pipe_write_closed(PipeHandle*);
ProcessError pipe_close_read_end/pipe_close_write_end(PipeHandle*);
void   pipe_close(PipeHandle*);
ProcessError pipe_set_nonblocking(PipeHandle*, bool);
const char* process_error_string(ProcessError);
ProcessError process_run_and_capture(const char* cmd, const char* const argv[],
                                     ProcessOptions* opts, int* exit_code);
void NANOSLEEP(long seconds, long nanoseconds);   /* function, not a macro */
```

`process_free` does **not** wait/terminate the child. A detached handle cannot
be waited on. With a custom `environment`, POSIX `execve` does not search
`PATH` (use an absolute/relative-slash command). `NANOSLEEP` does not retry on
signal. POSIX-only extras: `process_redirect_to_file`, `process_redirect_to_fd`,
`process_close_redirection`, `process_run_with_multiwriter`,
`process_create_with_redirection`, `process_run_with_file_redirection`.

## Sockets (`socket.h`)

```c
typedef struct { int handle; /* SOCKET on Windows */ } Socket;
void socket_initialize(void);  void socket_cleanup(void);   /* required on Windows */
Socket* socket_create(int domain, int type, int protocol);
int socket_close(Socket*);  int socket_bind(Socket*, const struct sockaddr*, socklen_t);
int socket_listen(Socket*, int backlog);
Socket* socket_accept(Socket*, struct sockaddr*, socklen_t*);
int socket_connect(Socket*, const struct sockaddr*, socklen_t);
ssize_t socket_recv/send(Socket*, void*, size_t, int flags);
int socket_fd(Socket*);  int socket_error(void);
void socket_strerror(int err, char* buf, size_t n);
int socket_get_option/set_option(Socket*, int level, int optname, void* val, socklen_t* len);
int socket_reuse_port(Socket*, int enable);
int socket_get_address/get_peer_address(Socket*, struct sockaddr*, socklen_t*);
int socket_family/type(Socket*);  int socket_set_non_blocking(Socket*, int enable);
struct sockaddr_in*  socket_ipv4_address(const char* ip, uint16_t port);  /* free() */
struct sockaddr_in6* socket_ipv6_address(const char* ip, uint16_t port);
```

Call `socket_initialize()`/`socket_cleanup()` on Windows only. `EINTR` is not
retried; partial sends are possible — loop. `socket_set_non_blocking(s, 1)`
enables. `socket_reuse_port` sets `SO_REUSEADDR` + `SO_REUSEPORT`
(`SO_EXCLUSIVEADDRUSE` on Windows).

## Poller (`poller.h`) — preferred readiness API

```c
typedef enum { POLLER_READ = 1<<0, POLLER_WRITE = 1<<1 } PollerEvents;
typedef enum { POLLER_EDGE = 1<<8, POLLER_EXCLUSIVE = 1<<9 } PollerFlags;
typedef struct { int fd; void* data; bool readable, writable, error, hup; } PollerEvent;

Poller* poller_new(void);  void poller_free(Poller*);
int poller_add/mod(Poller*, int fd, int events, void* data);
int poller_del(Poller*, int fd);
int poller_wait(Poller*, PollerEvent* evs, int max, int timeout_ms);  /* -1 blocks */
int   poller_event_fd(const PollerEvent*);  void* poller_event_data(const PollerEvent*);
bool  poller_event_is_read/is_write/is_error/is_hup(const PollerEvent*);
```

Backed by epoll (Linux), kqueue (macOS/BSD), `WSAPoll` (Windows). `poller_free`
does **not** close descriptors. `EINTR` returns `-1` (`errno == EINTR`), not
retried. With `POLLER_EDGE`, drain until `EAGAIN`. Edge/exclusive are ignored on
Windows (and `POLLER_EXCLUSIVE` is currently a no-op everywhere). Bad args to
`poller_wait`-support functions return `-1`. One poller per event loop.

## epoll (`epoll.h`) — Linux-native, POSIX-only

Linux uses `<sys/epoll.h>`; BSD/macOS get an inline kqueue polyfill. Also
provides `set_nonblocking(int fd)` and `create_listen_socket(uint16_t port)`.
The polyfill allocates a temp buffer per `epoll_wait` and collapses some flags.
Do not include the header on Windows.

## Streams (`stdstreams.h`)

```c
typedef struct stream* stream_t;
stream_t create_file_stream(FILE* fp);   /* does NOT own fp */
stream_t create_string_stream(size_t initial_capacity);
void     stream_destroy(stream_t);
int      stream_seek(stream_t, long offset, int whence);
size_t   file_stream_read(stream_t, void* ptr, size_t size, size_t count);
int      string_stream_write(stream_t, const char* s);
int      string_stream_write_len(stream_t, const char* s, size_t n);
const char* string_stream_data(stream_t);   /* valid until next mutation */
ssize_t  read_until(stream_t, int delim, char* buf, size_t buf_size);
unsigned long string_stream_copy_fast(stream_t dst, stream_t src);
unsigned long io_copy(stream_t writer, stream_t reader);
unsigned long io_copy_n(stream_t writer, stream_t reader, size_t n);
bool readline(const char* prompt, char* buf, size_t n);
int  getpassword(const char* prompt, char* buf, size_t n);
```

POSIX-style return (`>0` bytes, `0` EOF, `-1` error). String streams use SSO up
to `STRING_STREAM_SSO_CAP` (128). `read_until` consumes the delimiter but does
**not** copy it into the buffer. `readline` strips the newline.

## Time (`xtime.h`)

```c
typedef struct { int64_t seconds; uint32_t nanoseconds; int16_t tz_offset; bool has_tz; } xtime_t;
typedef enum { XTIME_OK, XTIME_ERR_INVALID_ARG, XTIME_ERR_PARSE_FAILED,
               XTIME_ERR_DATE_OUT_OF_RANGE, XTIME_ERR_BUFFER_TOO_SMALL,
               XTIME_ERR_INVALID_TIME, XTIME_ERR_SYSTEM } xtime_error_t;

xtime_error_t xtime_init(xtime_t*);  xtime_now(xtime_t*);  xtime_utc_now(xtime_t*);
xtime_error_t xtime_parse(const char* str, const char* fmt, xtime_t*);
xtime_error_t xtime_format(const xtime_t*, const char* fmt, char* buf, size_t n);
xtime_error_t xtime_format_utc(const xtime_t*, const char* fmt, char* buf, size_t n);
int64_t xtime_to_unix(const xtime_t*);  xtime_error_t xtime_from_unix(int64_t, xtime_t*);
int     xtime_compare(const xtime_t*, const xtime_t*);
xtime_error_t xtime_diff(const xtime_t*, const xtime_t*, double* seconds);
xtime_error_t xtime_diff_nanos/micros/millis/seconds/minutes/hours/days(const xtime_t*, const xtime_t*, int64_t*);
xtime_error_t xtime_add_seconds/.../years/months(xtime_t*, int64_t);
xtime_error_t xtime_start_of_week/month/year, xtime_end_of_day/month/year(...);
bool xtime_is_leap_year(const xtime_t*);  xtime_error_t xtime_truncate_to_*(xtime_t*);
const char* xtime_strerror(xtime_error_t);
```

Allocation-free, fixed-size structs; `XTIME_FMT_*` format macros provided
(ISO8601, RFC3339, RFC2822, HTTP, DATESTR). **Prefer `xtime_now`/
`xtime_diff_nanos` over `clock_gettime`, which does not exist on Windows.**
`nanoseconds` is `[0, 999999999]`; `tz_offset` is minutes.
