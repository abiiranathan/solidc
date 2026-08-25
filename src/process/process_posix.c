/**
 * @file process_posix.c
 * @brief POSIX (Linux/macOS/BSD) implementation of the process management
 *        backends: fork/exec process creation, select()-based pipes with
 *        timeouts, waitpid-based waiting, and file redirection.
 */

#include "process_internal.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#define ACCESS   access
#define PATH_SEP ":"
#define DIR_SEP  "/"

#ifndef X_OK
#define X_OK 1
#endif

/* Process handle: a POSIX child pid. */
struct ProcessHandle {
    pid_t pid;
    bool detached;
};

/* Opaque file redirection: an open descriptor plus lifetime policy. */
struct FileRedirection {
    int fd;
    bool close_on_exec;
};

ProcessError process_system_error(void) {
    switch (errno) {
        case EINVAL:
            return PROCESS_ERROR_INVALID_ARGUMENT;
        case ENOMEM:
            return PROCESS_ERROR_MEMORY;
        case EACCES:
        case EPERM:
            return PROCESS_ERROR_PERMISSION_DENIED;
        case EBADF:
        case EPIPE:
            return PROCESS_ERROR_IO;
        case ECHILD:
            return PROCESS_ERROR_WAIT_FAILED;
        default:
            return PROCESS_ERROR_UNKNOWN;
    }
}

// Helper function to search for command in PATH
static char* find_in_path(const char* command, const char* const* environment) {
    if (!command) return NULL;

    // Check for absolute or relative path
    if (command[0] == '/' || command[0] == '.') {
        return strdup(command);
    }

    // Find PATH in environment
    const char* path_env = NULL;
    for (int i = 0; environment && environment[i]; i++) {
        if (strncmp(environment[i], "PATH=", 5) == 0) {
            path_env = environment[i] + 5;
            break;
        }
    }

    if (!path_env) {
        return NULL;  // No PATH in environment
    }

    // Search each PATH component
    char* path_copy = strdup(path_env);
    if (!path_copy) return NULL;

    char* saveptr = NULL;
    char* dir = strtok_r(path_copy, PATH_SEP, &saveptr);
    char full_path[4096];

    while (dir) {
        snprintf(full_path, sizeof(full_path), "%s%s%s", dir, DIR_SEP, command);
        if (ACCESS(full_path, X_OK) == 0) {
            free(path_copy);
            return strdup(full_path);
        }

        dir = strtok_r(NULL, PATH_SEP, &saveptr);
    }

    free(path_copy);
    return NULL;
}

/* Implementation of the pipe API */
ProcessError pipe_create(PipeHandle** pipeHandle) {
    if (!pipeHandle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    *pipeHandle = (PipeHandle*)calloc(1, sizeof(PipeHandle));
    if (!*pipeHandle) {
        return PROCESS_ERROR_MEMORY;
    }

    int fds[2];
    if (pipe(fds) != 0) {
        free(*pipeHandle);
        *pipeHandle = NULL;
        return PROCESS_ERROR_PIPE_FAILED;
    }

    (*pipeHandle)->read_fd = fds[0];
    (*pipeHandle)->write_fd = fds[1];

    return PROCESS_SUCCESS;
}

ProcessError pipe_set_nonblocking(PipeHandle* pipe, bool nonblocking) {
    if (!pipe) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    int flags;

    // Set read end
    flags = fcntl(pipe->read_fd, F_GETFL);
    if (flags == -1) {
        return process_system_error();
    }

    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(pipe->read_fd, F_SETFL, flags) == -1) {
        return process_system_error();
    }

    // Set write end
    flags = fcntl(pipe->write_fd, F_GETFL);
    if (flags == -1) {
        return process_system_error();
    }

    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(pipe->write_fd, F_SETFL, flags) == -1) {
        return process_system_error();
    }

    return PROCESS_SUCCESS;
}

ProcessError pipe_read(PipeHandle* pipe, void* buffer, size_t size, size_t* bytes_read, int timeout_ms) {
    if (!pipe || !buffer || pipe->read_closed) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (bytes_read) {
        *bytes_read = 0;
    }

    if (timeout_ms >= 0) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(pipe->read_fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (long)((timeout_ms % 1000) * 1000);

        // select returns: -1 (error), 0 (timeout), >0 (ready)
        int select_result = select(pipe->read_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (select_result == -1) {
            if (errno == EINTR)
                return PROCESS_ERROR_WOULD_BLOCK;  // Retry logic usually handles this, but here we return
            return process_system_error();
        } else if (select_result == 0) {
            // If timeout was 0, this means "Would Block".
            // If timeout was > 0, this means "Timed Out".
            return (timeout_ms == 0) ? PROCESS_ERROR_WOULD_BLOCK : PROCESS_ERROR_TIMEOUT;
        }
        // If select_result > 0, data is ready, proceed to read()
    }

    ssize_t result = read(pipe->read_fd, buffer, size);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Non-blocking read with no data available
            return PROCESS_ERROR_WOULD_BLOCK;
        } else if (errno == EPIPE) {
            // Pipe was closed/broken
            return PROCESS_ERROR_PIPE_CLOSED;
        } else if (errno == EBADF) {
            // Invalid file descriptor
            return PROCESS_ERROR_PIPE_CLOSED;
        }
        return process_system_error();
    }

    if (result == 0) {
        // EOF - pipe was closed on the write end
        return PROCESS_ERROR_PIPE_CLOSED;
    }

    if (bytes_read) {
        *bytes_read = (size_t)result;
    }

    return PROCESS_SUCCESS;
}

ProcessError pipe_write(PipeHandle* pipe, const void* buffer, size_t size, size_t* bytes_written, int timeout_ms) {
    if (!pipe || !buffer || pipe->write_closed) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (bytes_written) {
        *bytes_written = 0;
    }

    if (timeout_ms >= 0) {
        // For non-zero timeout, we need to use select
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(pipe->write_fd, &write_fds);

        struct timeval timeout;
        struct timeval* timeout_ptr = NULL;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = ((long)timeout_ms % 1000) * 1000;
        timeout_ptr = &timeout;
        int select_result = select(pipe->write_fd + 1, NULL, &write_fds, NULL, timeout_ptr);
        if (select_result == -1) {
            if (errno == EINTR)
                return PROCESS_ERROR_WOULD_BLOCK;  // Retry logic usually handles this, but here we return
            return process_system_error();
        } else if (select_result == 0) {
            // If timeout was 0, this means "Would Block".
            // If timeout was > 0, this means "Timed Out".
            return (timeout_ms == 0) ? PROCESS_ERROR_WOULD_BLOCK : PROCESS_ERROR_TIMEOUT;
        }
        // If select_result > 0, data is ready, proceed to read()
    }

    ssize_t result = write(pipe->write_fd, buffer, size);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Non-blocking write - buffer full, no space available
            return PROCESS_ERROR_WOULD_BLOCK;
        } else if (errno == EPIPE) {
            // Broken pipe - read end was closed
            return PROCESS_ERROR_PIPE_CLOSED;
        } else if (errno == EBADF) {
            // Invalid file descriptor
            return PROCESS_ERROR_PIPE_CLOSED;
        }
        return process_system_error();
    }

    if (bytes_written) {
        *bytes_written = (size_t)result;
    }

    return PROCESS_SUCCESS;
}

void pipe_close(PipeHandle* pipe) {
    if (!pipe) {
        return;
    }

    if (pipe->read_fd != INVALID_NATIVE_HANDLE && !pipe->read_closed) {
        close(pipe->read_fd);
        pipe->read_closed = true;
        pipe->read_fd = INVALID_NATIVE_HANDLE;
    }
    if (pipe->write_fd != INVALID_NATIVE_HANDLE && !pipe->write_closed) {
        close(pipe->write_fd);
        pipe->write_closed = true;
        pipe->write_fd = INVALID_NATIVE_HANDLE;
    }

    free(pipe);
}

ProcessError pipe_close_read_end(PipeHandle* pipe) {
    if (!pipe) return PROCESS_ERROR_INVALID_ARGUMENT;
    if (pipe->read_fd != INVALID_NATIVE_HANDLE && !pipe->read_closed) {
        close(pipe->read_fd);
        pipe->read_closed = true;
        pipe->read_fd = INVALID_NATIVE_HANDLE;
    }
    return PROCESS_SUCCESS;
}

ProcessError pipe_close_write_end(PipeHandle* pipe) {
    if (!pipe) return PROCESS_ERROR_INVALID_ARGUMENT;
    if (pipe->write_fd != INVALID_NATIVE_HANDLE && !pipe->write_closed) {
        close(pipe->write_fd);
        pipe->write_closed = true;
        pipe->write_fd = INVALID_NATIVE_HANDLE;
    }
    return PROCESS_SUCCESS;
}

ProcessError unix_create_process(ProcessHandle** handle, const char* command, const char* const argv[],
                                 const ProcessOptions* options) {
    // Create pipes for redirection if needed
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    // Set up pipes for redirections
    if (options->io.stdin_pipe) {
        stdin_pipe[0] = options->io.stdin_pipe->read_fd;
        stdin_pipe[1] = options->io.stdin_pipe->write_fd;
    }

    if (options->io.stdout_pipe) {
        stdout_pipe[0] = options->io.stdout_pipe->read_fd;
        stdout_pipe[1] = options->io.stdout_pipe->write_fd;
    }

    if (options->io.stderr_pipe) {
        stderr_pipe[0] = options->io.stderr_pipe->read_fd;
        stderr_pipe[1] = options->io.stderr_pipe->write_fd;
    }

    /*
     * Resolve PATH before fork() when using a custom environment
     * (Bug #18, MT-safety): find_in_path() calls strdup/strtok_r/malloc,
     * none async-signal-safe.  In the child of a multithreaded parent
     * these can deadlock on a lock copied in the locked state.
     */
    char* resolved_path = NULL;
    if (!options->inherit_environment) {
        static const char* const empty_env[] = {NULL};
        const char* const* env = options->environment ? options->environment : empty_env;
        resolved_path = find_in_path(command, env);
    }
    const char* exec_target = resolved_path ? (const char*)resolved_path : command;
#ifdef UNIX_PROC_DEBUG
    fprintf(stderr, "[PROC] resolved=%p target=%s inherit=%d\n", (void*)resolved_path, exec_target,
            (int)options->inherit_environment);
#endif

    // Fork the process
    pid_t pid = fork();

    if (pid < 0) {
        free(resolved_path);
        return PROCESS_ERROR_FORK_FAILED;
    }

    if (pid == 0) {
        // Child process

        // Handle working directory
        if (options->working_directory) {
            if (chdir(options->working_directory) != 0) {
                _exit(127);
            }
        }

        // Handle standard input
        if (options->io.stdin_pipe) {
            /* BUG #19 class: a failed dup2 must abort the child, otherwise
             * exec runs with the wrong stdio attached. */
            if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
                _exit(127);
            }
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
        }

        // Handle standard output
        if (options->io.stdout_pipe) {
            if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
                _exit(127);
            }
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        }

        // Handle standard error
        if (options->io.stderr_pipe) {
            if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
                _exit(127);
            }
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        } else if (options->io.merge_stderr) {
            if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                _exit(127);
            }
        }

        // Detach from parent if requested
        if (options->detached) {
            if (setsid() < 0) {
                _exit(127);
            }
        }

        // Execute the command
        if (options->inherit_environment) {
            execvp(command, (char* const*)argv);
        } else {
            /* Custom or empty environment: PATH was resolved pre-fork
             * (async-signal-safe).  Fall back to the raw command text if
             * resolution failed but it is still an executable path. */
            const char* const* env = options->environment ? options->environment : (const char* const[]){NULL};
            execve(exec_target, (char* const*)argv, (char* const*)env);
        }

        // If we get here, exec failed
        perror("execve");
        _exit(127);
    }

    // Parent process
    free(resolved_path);

    *handle = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    if (!*handle) {
        /* The child already exists; returning an error here would leak it
         * as an unparented process.  Terminate and reap so the failure is
         * atomic from the caller's perspective. */
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return PROCESS_ERROR_MEMORY;
    }

    (*handle)->pid = pid;
    (*handle)->detached = options->detached;

    return PROCESS_SUCCESS;
}

static inline void set_process_result(int status, ProcessResult* result) {
    if (WIFEXITED(status)) {
        result->exit_code = WEXITSTATUS(status);
        result->exited_normally = true;
    } else if (WIFSIGNALED(status)) {
        result->exit_code = WTERMSIG(status);
        result->exited_normally = false;
        result->term_signal = WTERMSIG(status);  // only set term_signal here
    } else {
        result->exit_code = -1;  // Undefined exit reason
        result->exited_normally = false;
    }
}

// Cross-platform nanosleep function
void NANOSLEEP(long seconds, long nanoseconds) {
    // On Linux/Unix, we can use nanosleep directly
    struct timespec req;
    req.tv_sec = seconds;
    req.tv_nsec = nanoseconds;
    nanosleep(&req, NULL);
}

ProcessError process_wait(ProcessHandle* handle, ProcessResult* result, int timeout_ms) {
    if (!handle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (handle->detached) {
        // Cannot wait for detached processes
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    int status = 0;
    pid_t wait_result = 0;

    if (timeout_ms < 0) {
        // Wait indefinitely
        wait_result = waitpid(handle->pid, &status, 0);
    } else {
        /*
         * Bounded wait: poll WNOHANG in fixed slices instead of sleeping the
         * whole remaining timeout in one nanosleep (Perf #17).  The old code
         * slept the FULL remainder first and only re-checked afterwards, so
         * a process exiting at t=50ms still blocked for a 30s timeout.
         */
        const long SLICE_MS = 10;
        long slept_ms = 0;

        while ((wait_result = waitpid(handle->pid, &status, WNOHANG)) == 0) {
            if (slept_ms >= timeout_ms) {
                break;
            }

            long slice = (timeout_ms - slept_ms < SLICE_MS) ? (timeout_ms - slept_ms) : SLICE_MS;
            NANOSLEEP(slice / 1000, (slice % 1000) * 1000000L);
            slept_ms += slice;
        }
    }

    if (wait_result < 0) {
        return process_system_error();
    }

    if (wait_result == 0) {
        // Process is still running
        return PROCESS_ERROR_WAIT_FAILED;
    }

    if (result) {
        set_process_result(status, result);
    }

    return PROCESS_SUCCESS;
}

ProcessError process_terminate(ProcessHandle* handle, bool force) {
    if (!handle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (handle->pid <= 0) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (force) {
        // Force termination using SIGKILL (immediate termination)
        if (kill(handle->pid, SIGKILL) != 0) {
            return PROCESS_ERROR_KILL_FAILED;
        }
    } else {
        // Graceful termination using SIGTERM (request termination)
        if (kill(handle->pid, SIGTERM) != 0) {
            return PROCESS_ERROR_KILL_FAILED;
        }
    }
    return PROCESS_SUCCESS;
}

// ======== Redirection ==================
ProcessError process_redirect_to_file(FileRedirection** redirection, const char* filepath, int flags,
                                      unsigned int mode) {
    if (!redirection || !filepath) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    *redirection = (FileRedirection*)malloc(sizeof(FileRedirection));
    if (!*redirection) {
        return PROCESS_ERROR_MEMORY;
    }

    // Open the file with the specified flags and mode
    int fd = open(filepath, flags, mode);
    if (fd < 0) {
        free(*redirection);
        *redirection = NULL;
        return process_system_error();
    }

    (*redirection)->fd = fd;
    (*redirection)->close_on_exec = true;

    return PROCESS_SUCCESS;
}

ProcessError process_redirect_to_fd(FileRedirection** redirection, int fd, bool close_on_exec) {
    if (!redirection || fd < 0) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    *redirection = (FileRedirection*)malloc(sizeof(FileRedirection));
    if (!*redirection) {
        return PROCESS_ERROR_MEMORY;
    }

    (*redirection)->fd = fd;
    (*redirection)->close_on_exec = close_on_exec;

    return PROCESS_SUCCESS;
}

void process_close_redirection(FileRedirection* redirection) {
    if (!redirection) {
        return;
    }

    if (redirection->close_on_exec && redirection->fd >= 0) {
        close(redirection->fd);
    }

    free(redirection);
    redirection = NULL;
}

ProcessError process_create_with_redirection(ProcessHandle** handle, const char* command, const char* const argv[],
                                             const ExtProcessOptions* options) {
    if (!handle || !command || !argv || !argv[0]) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }
    /*
     * Resolve PATH before fork() when using a custom environment
     * (Bug #18, MT-safety): find_in_path() calls strdup/strtok_r/malloc,
     * none async-signal-safe.  In the child of a multithreaded parent
     * these can deadlock on a lock copied in the locked state.
     */
    char* resolved_path = NULL;
    if (!options->inherit_environment) {
        static const char* const empty_env[] = {NULL};
        const char* const* env = options->environment ? (const char* const*)options->environment : empty_env;
        resolved_path = find_in_path(command, env);
    }
    const char* exec_target = resolved_path ? (const char*)resolved_path : command;

    // Fork the process
    pid_t pid = fork();

    if (pid < 0) {
        free(resolved_path);
        return PROCESS_ERROR_FORK_FAILED;
    }

    if (pid == 0) {
        // Child process

        // Handle working directory
        if (options->working_directory) {
            if (chdir(options->working_directory) != 0) {
                _exit(127);
            }
        }

        // Handle standard input
        if (options->io.stdin_pipe) {
            if (dup2(options->io.stdin_pipe->read_fd, STDIN_FILENO) == -1) {
                perror("dup2");
                _exit(127); /* BUG #19: never return from a forked child */
            };

            // Close pipe handles that aren't needed in child
            close(options->io.stdin_pipe->read_fd);
            close(options->io.stdin_pipe->write_fd);
        }

        // Handle standard output
        if (options->io.stdout_pipe) {
            if (dup2(options->io.stdout_pipe->write_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(127); /* BUG #19: never return from a forked child */
            };
            close(options->io.stdout_pipe->read_fd);
            close(options->io.stdout_pipe->write_fd);
        } else if (options->io.stdout_file) {
            // Redirect stdout to file
            if (dup2(options->io.stdout_file->fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(127); /* BUG #19: never return from a forked child */
            };
            if (options->io.stdout_file->close_on_exec) {
                close(options->io.stdout_file->fd);
            }
        }

        // Handle standard error
        if (options->io.stderr_pipe) {
            if (dup2(options->io.stderr_pipe->write_fd, STDERR_FILENO) == -1) {
                perror("dup2");
                _exit(127); /* BUG #19: never return from a forked child */
            };
            close(options->io.stderr_pipe->read_fd);
            close(options->io.stderr_pipe->write_fd);
        } else if (options->io.stderr_file) {
            // Redirect stderr to file
            if (dup2(options->io.stderr_file->fd, STDERR_FILENO) == -1) {
                perror("dup2");
                _exit(127); /* BUG #19: never return from a forked child */
            };
            if (options->io.stderr_file->close_on_exec) {
                close(options->io.stderr_file->fd);
            }
        } else if (options->io.merge_stderr) {
            if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                perror("dup2");
                _exit(127); /* BUG #19: never return from a forked child */
            };
        }

        // Detach from parent if requested
        if (options->detached) {
            if (setsid() < 0) {
                _exit(127);
            }
        }

        // Execute the command.  exec_target is PATH-resolved pre-fork for
        // non-inherited environments (see note at function top).
        if (options->environment) {
            if (execve(exec_target, (char* const*)argv, (char* const*)options->environment) == -1) {
                perror("execve");
            };
        } else if (options->inherit_environment) {
            if (execvp(command, (char* const*)argv) == -1) {
                perror("execvp");
            };
        } else {
            char* empty_env[] = {NULL};
            /* Pre-fork resolution makes PATH lookup work even here, where
             * the previous implementation always failed for bare names. */
            if (execve(exec_target, (char* const*)argv, empty_env) == -1) {
                perror("execve");
            };
        }

        // If we get here, exec failed
        _exit(127);
    }

    // Parent process
    *handle = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    if (!*handle) {
        return PROCESS_ERROR_MEMORY;
    }

    (*handle)->pid = pid;
    (*handle)->detached = options->detached;

    return PROCESS_SUCCESS;
}

ProcessError process_run_with_multiwriter(ProcessResult* result, const char* cmd, const char* args[], int output_fds[],
                                          int error_fds[]) {
    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    // Create a pipe for stdout
    if (pipe(stdout_pipe) < 0) {
        perror("pipe (stdout)");
        return PROCESS_ERROR_PIPE_FAILED;
    }

    // Create a pipe for stderr
    if (pipe(stderr_pipe) < 0) {
        perror("pipe (stderr)");
        // Clean up stdout pipe if stderr pipe creation fails
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return PROCESS_ERROR_PIPE_FAILED;
    }

    // Fork the child process that will execute the command
    pid_t cmd_pid = fork();
    if (cmd_pid < 0) {
        perror("fork");
        // Clean up pipes if fork fails
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return PROCESS_ERROR_FORK_FAILED;
    }

    if (cmd_pid == 0) {  // Command process
        // Close read ends of the pipes (not needed in the command process)
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Redirect stdout to the write end of the stdout pipe
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            perror("dup2 (stdout)");
            _exit(127);
        }

        // Redirect stderr to the write end of the stderr pipe
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
            perror("dup2 (stderr)");
            _exit(127);
        }

        // Close the write ends of the pipes (they are now duplicated to
        // stdout/stderr)
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Execute the command
        execv(cmd, (char* const*)args);
        // If execv fails, print an error and exit
        perror("execv");
        _exit(127);
    }

    // Parent process
    // Close write ends of the pipes (not needed in the parent process)
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    int child_count = 1;  // Start with 1 for the command process

    // Fork a single tee process for stdout that writes to all output_fds
    pid_t stdout_tee_pid = fork();
    if (stdout_tee_pid < 0) {
        perror("fork tee (stdout)");
        // Clean up pipes and wait for the command process if fork fails
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        waitpid(cmd_pid, NULL, 0);  // Wait for the command process to avoid zombies
        return PROCESS_ERROR_FORK_FAILED;
    }

    if (stdout_tee_pid == 0) {  // Tee child process for stdout
        char buffer[4096];
        ssize_t n = 0;

        // Read from the stdout pipe and write to all output_fds
        while ((n = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
            for (int i = 0; output_fds[i] != -1; i++) {
                if (write(output_fds[i], buffer, (size_t)n) < 0) {
                    perror("write (stdout tee)");
                    _exit(127);
                }
            }
        }

        // Check for read errors
        if (n < 0) {
            perror("read (stdout tee)");
            _exit(127);
        }

        _exit(0);
    }

    child_count++;

    // Fork a single tee process for stderr that writes to all error_fds
    pid_t stderr_tee_pid = fork();
    if (stderr_tee_pid < 0) {
        perror("fork tee (stderr)");
        // Clean up pipes and wait for the command and stdout tee processes if fork
        // fails
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        waitpid(cmd_pid, NULL, 0);         // Wait for the command process
        waitpid(stdout_tee_pid, NULL, 0);  // Wait for the stdout tee process
        return PROCESS_ERROR_FORK_FAILED;
    }

    if (stderr_tee_pid == 0) {  // Tee child process for stderr
        char buffer[4096];
        ssize_t n = 0;

        // Read from the stderr pipe and write to all error_fds
        while ((n = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
            for (int i = 0; error_fds[i] != -1; i++) {
                if (write(error_fds[i], buffer, (size_t)n) < 0) {
                    perror("write (stderr tee)");
                    _exit(127);
                }
            }
        }

        // Check for read errors
        if (n < 0) {
            perror("read (stderr tee)");
            _exit(127);
        }

        _exit(0);
    }

    child_count++;

    // Close read ends of the pipes after all tee processes are started
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    // Wait for the command process specifically to get its status
    int cmd_status = 0;
    waitpid(cmd_pid, &cmd_status, 0);
    set_process_result(cmd_status, result);

    // Wait for all remaining child processes (stdout and stderr tee processes)
    while (child_count > 1) {
        wait(NULL);
        child_count--;
    }

    // Check if the command process exited successfully
    if (WIFEXITED(cmd_status)) {
        return PROCESS_SUCCESS;
    } else {
        return PROCESS_ERROR_EXEC_FAILED;
    }
}

ProcessError process_run_with_file_redirection(ProcessHandle** handle, const char* command, const char* const argv[],
                                               const char* stdout_file, const char* stderr_file, bool append) {
    ExtProcessOptions options;
    memset(&options, 0, sizeof(options));
    options.inherit_environment = true;

    FileRedirection* stdout_redir = NULL;
    FileRedirection* stderr_redir = NULL;
    ProcessError err = PROCESS_SUCCESS;

    if (stdout_file) {
        int flags = O_WRONLY | O_CREAT;
        flags |= (append ? O_APPEND : O_TRUNC);

        err = process_redirect_to_file(&stdout_redir, stdout_file, flags, 0644);
        if (err != PROCESS_SUCCESS) {
            return err;
        }
        options.io.stdout_file = stdout_redir;
    }

    if (stderr_file) {
        int flags = O_WRONLY | O_CREAT;
        flags |= (append ? O_APPEND : O_TRUNC);

        err = process_redirect_to_file(&stderr_redir, stderr_file, flags, 0644);
        if (err != PROCESS_SUCCESS) {
            if (stdout_redir) {
                process_close_redirection(stdout_redir);
            }
            return err;
        }
        options.io.stderr_file = stderr_redir;
    }

    // Create the process with redirection
    err = process_create_with_redirection(handle, command, argv, &options);

    // Clean up redirections
    if (stdout_redir) {
        process_close_redirection(stdout_redir);
    }

    if (stderr_redir) {
        process_close_redirection(stderr_redir);
    }

    return err;
}
