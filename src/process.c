/**
 * @file process.c
 * @brief Implementation of the process management API
 */

#include "../include/process.h"
#include <errno.h>

/* Platform-specific implementations of process and pipe handles */
#ifdef _WIN32
struct ProcessHandle {
    PROCESS_INFORMATION process_info;
    bool detached;
};

#else
struct ProcessHandle {
    pid_t pid;
    bool detached;
};
#endif

struct PipeHandle {
    PipeFd read_fd;
    PipeFd write_fd;
    bool read_closed;
    bool write_closed;
};

// New structure to represent file redirection
struct FileRedirection {
    int fd;              // File descriptor
    bool close_on_exec;  // Whether to close on exec
};

/* Default options for process creation */
static const ProcessOptions DEFAULT_OPTIONS = {
    .working_directory   = NULL,
    .inherit_environment = true,
    .environment         = NULL,
    .detached            = false,
    .io =
        {
            .stdin_pipe   = NULL,
            .stdout_pipe  = NULL,
            .stderr_pipe  = NULL,
            .merge_stderr = false,
        },
};

/* Error handling helper functions */
ProcessError process_system_error(void) {
#ifdef _WIN32
    DWORD error = GetLastError();
    switch (error) {
        case ERROR_INVALID_PARAMETER:
            return PROCESS_ERROR_INVALID_ARGUMENT;
        case ERROR_NOT_ENOUGH_MEMORY:
            return PROCESS_ERROR_MEMORY;
        case ERROR_ACCESS_DENIED:
            return PROCESS_ERROR_PERMISSION_DENIED;
        case ERROR_BROKEN_PIPE:
        case ERROR_PIPE_BUSY:
        case ERROR_PIPE_NOT_CONNECTED:
            return PROCESS_ERROR_IO;
        default:
            return PROCESS_ERROR_UNKNOWN;
    }
#else
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
#endif
}

/**
Returns True if pipe read closed.
*/
bool pipe_read_closed(PipeHandle* handle) {
    return handle->read_closed;
}

/**
Returns True if pipe write closed.
*/
bool pipe_write_closed(PipeHandle* handle) {
    return handle->write_closed;
}

/**
Returns the pipe write read descriptor.
*/
PipeFd pipe_read_fd(PipeHandle* handle) {
    return handle->read_fd;
}

/**
Returns the pipe write file descriptor.
*/
PipeFd pipe_write_fd(PipeHandle* handle) {
    return handle->write_fd;
}

/* String descriptions for error codes */
const char* process_error_string(ProcessError error) {
    switch (error) {
        case PROCESS_SUCCESS:
            return "Success";
        case PROCESS_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";
        case PROCESS_ERROR_FORK_FAILED:
            return "Fork failed";
        case PROCESS_ERROR_EXEC_FAILED:
            return "Exec failed";
        case PROCESS_ERROR_PIPE_FAILED:
            return "Pipe creation failed";
        case PROCESS_ERROR_MEMORY:
            return "Memory allocation failed";
        case PROCESS_ERROR_WAIT_FAILED:
            return "Wait for process failed";
        case PROCESS_ERROR_KILL_FAILED:
            return "Failed to terminate process";
        case PROCESS_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case PROCESS_ERROR_IO:
            return "I/O error";
        case PROCESS_ERROR_UNKNOWN:
            return "Unknown error";
        default:
            return "Invalid error code";
    }
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

#ifdef _WIN32
    SECURITY_ATTRIBUTES security_attrs;
    memset(&security_attrs, 0, sizeof(security_attrs));
    security_attrs.nLength        = sizeof(security_attrs);
    security_attrs.bInheritHandle = TRUE;

    if (!CreatePipe(&(*pipeHandle)->read_fd, &(*pipeHandle)->write_fd, &security_attrs, 0)) {
        free(*pipeHandle);
        *pipeHandle = NULL;
        return PROCESS_ERROR_PIPE_FAILED;
    }
#else
    int fds[2];
    if (pipe(fds) != 0) {
        free(*pipeHandle);
        *pipeHandle = NULL;
        return PROCESS_ERROR_PIPE_FAILED;
    }

    (*pipeHandle)->read_fd  = fds[0];
    (*pipeHandle)->write_fd = fds[1];

    // Set both ends to non-blocking mode
    int flags;
    flags = fcntl((*pipeHandle)->read_fd, F_GETFL);
    fcntl((*pipeHandle)->read_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl((*pipeHandle)->write_fd, F_GETFL);
    fcntl((*pipeHandle)->write_fd, F_SETFL, flags | O_NONBLOCK);
#endif

    return PROCESS_SUCCESS;
}

ProcessError pipe_read(PipeHandle* pipe, void* buffer, size_t size, size_t* bytes_read, int timeout_ms) {
    if (!pipe || !buffer || pipe->read_closed) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (bytes_read) {
        *bytes_read = 0;
    }

#ifdef _WIN32
    DWORD bytes_read_win = 0;
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!overlapped.hEvent) {
        return process_system_error();
    }

    if (!ReadFile(pipe->read_fd, buffer, (DWORD)size, NULL, &overlapped)) {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            CloseHandle(overlapped.hEvent);
            return process_system_error();
        }
    }

    DWORD wait_result = WaitForSingleObject(overlapped.hEvent, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);

    if (wait_result == WAIT_OBJECT_0) {
        if (!GetOverlappedResult(pipe->read_fd, &overlapped, &bytes_read_win, FALSE)) {
            CloseHandle(overlapped.hEvent);
            return process_system_error();
        }
        if (bytes_read) {
            *bytes_read = bytes_read_win;
        }
    } else if (wait_result == WAIT_TIMEOUT) {
        CancelIo(pipe->read_fd);
        CloseHandle(overlapped.hEvent);
        return PROCESS_ERROR_IO;  // Timeout
    } else {
        CloseHandle(overlapped.hEvent);
        return process_system_error();
    }

    CloseHandle(overlapped.hEvent);
#else
    if (timeout_ms != 0) {
        // For non-zero timeout, we need to use select
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(pipe->read_fd, &read_fds);

        struct timeval timeout;
        struct timeval* timeout_ptr = NULL;

        if (timeout_ms > 0) {
            timeout.tv_sec  = timeout_ms / 1000;
            timeout.tv_usec = (timeout_ms % 1000) * 1000;
            timeout_ptr     = &timeout;
        }

        int select_result = select(pipe->read_fd + 1, &read_fds, NULL, NULL, timeout_ptr);
        if (select_result == -1) {
            return process_system_error();
        } else if (select_result == 0) {
            return PROCESS_ERROR_IO;  // Timeout
        }
    }

    ssize_t result = read(pipe->read_fd, buffer, size);

    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PROCESS_ERROR_IO;  // Would block
        }
        return process_system_error();
    }

    if (bytes_read) {
        *bytes_read = (size_t)result;
    }
#endif

    return PROCESS_SUCCESS;
}

ProcessError pipe_write(PipeHandle* pipe, const void* buffer, size_t size, size_t* bytes_written,
                        int timeout_ms) {
    if (!pipe || !buffer || pipe->write_closed) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (bytes_written) {
        *bytes_written = 0;
    }

#ifdef _WIN32
    DWORD bytes_written_win = 0;
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!overlapped.hEvent) {
        return process_system_error();
    }

    if (!WriteFile(pipe->write_fd, buffer, (DWORD)size, NULL, &overlapped)) {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            CloseHandle(overlapped.hEvent);
            return process_system_error();
        }
    }

    DWORD wait_result = WaitForSingleObject(overlapped.hEvent, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);

    if (wait_result == WAIT_OBJECT_0) {
        if (!GetOverlappedResult(pipe->write_fd, &overlapped, &bytes_written_win, FALSE)) {
            CloseHandle(overlapped.hEvent);
            return process_system_error();
        }
        if (bytes_written) {
            *bytes_written = bytes_written_win;
        }
    } else if (wait_result == WAIT_TIMEOUT) {
        CancelIo(pipe->write_fd);
        CloseHandle(overlapped.hEvent);
        return PROCESS_ERROR_IO;  // Timeout
    } else {
        CloseHandle(overlapped.hEvent);
        return process_system_error();
    }

    CloseHandle(overlapped.hEvent);
#else
    if (timeout_ms != 0) {
        // For non-zero timeout, we need to use select
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(pipe->write_fd, &write_fds);

        struct timeval timeout;
        struct timeval* timeout_ptr = NULL;

        if (timeout_ms > 0) {
            timeout.tv_sec  = timeout_ms / 1000;
            timeout.tv_usec = (timeout_ms % 1000) * 1000;
            timeout_ptr     = &timeout;
        }

        int select_result = select(pipe->write_fd + 1, NULL, &write_fds, NULL, timeout_ptr);

        if (select_result == -1) {
            return process_system_error();
        } else if (select_result == 0) {
            return PROCESS_ERROR_IO;  // Timeout
        }
    }

    ssize_t result = write(pipe->write_fd, buffer, size);

    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PROCESS_ERROR_IO;  // Would block
        }
        return process_system_error();
    }

    if (bytes_written) {
        *bytes_written = (size_t)result;
    }
#endif

    return PROCESS_SUCCESS;
}

void pipe_close(PipeHandle* pipe) {
    if (!pipe) {
        return;
    }

#ifdef _WIN32
    if (!pipe->read_closed) {
        CloseHandle(pipe->read_fd);
        pipe->read_closed = true;
    }
    if (!pipe->write_closed) {
        CloseHandle(pipe->write_fd);
        pipe->write_closed = true;
    }
#else
    if (!pipe->read_closed) {
        close(pipe->read_fd);
        pipe->read_closed = true;
    }
    if (!pipe->write_closed) {
        close(pipe->write_fd);
        pipe->write_closed = true;
    }
#endif

    free(pipe);
}

/* Implementation of the process API */
#ifdef _WIN32
static ProcessError win32_create_process(ProcessHandle** handle, const char* command,
                                         const char* const argv[], const ProcessOptions* options) {
    // Build command line string (Windows style with quotes)
    size_t cmdline_len = 0;
    int arg_count      = 0;

    while (argv[arg_count] != NULL) {
        cmdline_len += strlen(argv[arg_count]) + 3;  // Extra space for quotes and space
        arg_count++;
    }

    char* cmdline = (char*)malloc(cmdline_len + 1);
    if (!cmdline) {
        return PROCESS_ERROR_MEMORY;
    }

    cmdline[0] = '\0';
    for (int i = 0; i < arg_count; i++) {
        if (i > 0) {
            strcat(cmdline, " ");
        }

        // Quote the argument if it contains spaces
        bool needs_quotes = (strchr(argv[i], ' ') != NULL);

        if (needs_quotes) {
            strcat(cmdline, "\"");
        }

        strcat(cmdline, argv[i]);

        if (needs_quotes) {
            strcat(cmdline, "\"");
        }
    }

    // Prepare startup info with redirections
    STARTUPINFOA startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb      = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;

    // Set up standard handles (inherit by default)
    startup_info.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    // Apply redirections if specified
    if (options->io.stdin_pipe) {
        startup_info.hStdInput = options->io.stdin_pipe->read_fd;
    }

    if (options->io.stdout_pipe) {
        startup_info.hStdOutput = options->io.stdout_pipe->write_fd;
    }

    if (options->io.stderr_pipe) {
        startup_info.hStdError = options->io.stderr_pipe->write_fd;
    } else if (options->io.merge_stderr) {
        startup_info.hStdError = startup_info.hStdOutput;
    }

    // Create the process
    DWORD creation_flags = 0;
    if (options->detached) {
        creation_flags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
    }

    PROCESS_INFORMATION process_info;
    BOOL success = CreateProcessA(command,                         // command to execute
                                  cmdline,                         // Command line
                                  NULL,                            // Process handle not inheritable
                                  NULL,                            // Thread handle not inheritable
                                  TRUE,                            // Inherit handles
                                  creation_flags,                  // Creation flags
                                  (LPVOID)(options->environment),  // Environment block
                                  options->working_directory,      // Working directory
                                  &startup_info,                   // Startup info
                                  &process_info                    // Process info
    );

    free(cmdline);

    if (!success) {
        return process_system_error();
    }

    // Store the process handle
    *handle = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    if (!*handle) {
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        return PROCESS_ERROR_MEMORY;
    }

    (*handle)->process_info = process_info;
    (*handle)->detached     = options->detached;

    return PROCESS_SUCCESS;
}
#else
static ProcessError unix_create_process(ProcessHandle** handle, const char* command, const char* const argv[],
                                        const ProcessOptions* options) {
    // Create pipes for redirection if needed
    int stdin_pipe[2]  = {-1, -1};
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

    // Fork the process
    pid_t pid = fork();

    if (pid < 0) {
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
            dup2(stdin_pipe[0], STDIN_FILENO);
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
        }

        // Handle standard output
        if (options->io.stdout_pipe) {
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        }

        // Handle standard error
        if (options->io.stderr_pipe) {
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        } else if (options->io.merge_stderr) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        }

        // Detach from parent if requested
        if (options->detached) {
            if (setsid() < 0) {
                _exit(127);
            }
        }

        // Execute the command
        if (options->environment) {
            execve(command, (char* const*)argv, (char* const*)options->environment);
        } else if (options->inherit_environment) {
            execvp(command, (char* const*)argv);
        } else {
            char* empty_env[] = {NULL};
            execve(command, (char* const*)argv, empty_env);
        }

        // If we get here, exec failed
        perror("execve");
        _exit(127);
    }

    // Parent process
    *handle = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    if (!*handle) {
        return PROCESS_ERROR_MEMORY;
    }

    (*handle)->pid      = pid;
    (*handle)->detached = options->detached;

    return PROCESS_SUCCESS;
}
#endif

ProcessError process_create(ProcessHandle** handle, const char* command, const char* const argv[],
                            const ProcessOptions* options) {
    if (!handle || !command || !argv || !argv[0]) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    // Use default options if not provided
    ProcessOptions effective_options;
    if (options) {
        effective_options = *options;
    } else {
        effective_options = DEFAULT_OPTIONS;
    }

#ifdef _WIN32
    return win32_create_process(handle, command, argv, &effective_options);
#else
    return unix_create_process(handle, command, argv, &effective_options);
#endif
}

#ifndef _WIN32
static inline void set_process_result(int status, ProcessResult* result) {
    if (WIFEXITED(status)) {
        result->exit_code       = WEXITSTATUS(status);
        result->exited_normally = true;
    } else if (WIFSIGNALED(status)) {
        result->exit_code       = WTERMSIG(status);
        result->exited_normally = false;
        result->term_signal     = WTERMSIG(status);  // only set term_signal here
    } else {
        result->exit_code       = -1;  // Undefined exit reason
        result->exited_normally = false;
    }
}
#endif

// Cross-platform nanosleep function
void NANOSLEEP(long seconds, long nanoseconds) {
#ifdef _WIN32
    // On Windows, Sleep works in milliseconds,
    // so we convert seconds and nanoseconds to milliseconds
    long total_milliseconds = seconds * 1000 + nanoseconds / 1000000;
    Sleep(total_milliseconds);
#else
    // On Linux/Unix, we can use nanosleep directly
    struct timespec req;
    req.tv_sec  = seconds;
    req.tv_nsec = nanoseconds;
    nanosleep(&req, NULL);
#endif
}

ProcessError process_wait(ProcessHandle* handle, ProcessResult* result, int timeout_ms) {
    if (!handle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (handle->detached) {
        // Cannot wait for detached processes
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    // Windows-specific code
    DWORD wait_result =
        WaitForSingleObject(handle->process_info.hProcess, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);

    if (wait_result == WAIT_TIMEOUT) {
        return PROCESS_ERROR_WAIT_FAILED;
    } else if (wait_result != WAIT_OBJECT_0) {
        return process_system_error();
    }

    if (result) {
        DWORD exit_code;
        if (!GetExitCodeProcess(handle->process_info.hProcess, &exit_code)) {
            return process_system_error();
        }

        result->exit_code       = (int)exit_code;
        result->exited_normally = true;
        result->term_signal     = 0;  // No signal on Windows
    }
#else
    // Linux/Unix-specific code
    int status;
    pid_t wait_result;

    if (timeout_ms < 0) {
        // Wait indefinitely
        wait_result = waitpid(handle->pid, &status, 0);
    } else {
        // Wait with timeout using WNOHANG and a busy-wait loop for the timeout
        int remaining_timeout_ms = timeout_ms;

        // Here, we wait in a loop until the process exits or the timeout is reached
        while ((wait_result = waitpid(handle->pid, &status, WNOHANG)) == 0 && remaining_timeout_ms > 0) {
            struct timespec ts;
            ts.tv_sec  = remaining_timeout_ms / 1000;
            ts.tv_nsec = (remaining_timeout_ms % 1000) * 1000000L;

            // Sleep for the remaining timeout duration
            NANOSLEEP(ts.tv_sec, ts.tv_nsec);

            // Adjust remaining timeout
            remaining_timeout_ms -= (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
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

#endif
    return PROCESS_SUCCESS;
}

/**
 * @brief Terminate a running process
 *
 * @param[in] handle Process handle
 * @param[in] force If true, force immediate termination
 * (SIGKILL/TerminateProcess)
 * @return PROCESS_SUCCESS on success, error code otherwise
 */
ProcessError process_terminate(ProcessHandle* handle, bool force) {
    if (!handle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    if (force) {
        // Force termination using TerminateProcess (like SIGKILL)
        if (!TerminateProcess(handle->process_info.hProcess, 1)) {
            return PROCESS_ERROR_TERMINATE_FAILED;
        }
    } else {
        // Graceful termination using GenerateConsoleCtrlEvent or other method
        // GenerateCtrlEvent is typically used for console processes
        if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0)) {
            return PROCESS_ERROR_TERMINATE_FAILED;
        }
    }
#else
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
#endif
    return PROCESS_SUCCESS;
}

ProcessError process_run_and_capture(const char* command, const char* const argv[], ProcessOptions* options,
                                     int* exit_code) {
    ProcessHandle* proc;
    ProcessError err;
    err = process_create(&proc, command, argv, options);
    if (err != PROCESS_SUCCESS) {
        return err;
    }

    ProcessResult res;
    err = process_wait(proc, &res, -1);

    if (exit_code) {
        *exit_code = res.exit_code;
    }

    if (err != PROCESS_SUCCESS) {
        return err;
    }

    if (res.exit_code != 0) {
        return PROCESS_ERROR_EXEC_FAILED;
    }
    return PROCESS_SUCCESS;
}

#ifndef _WIN32
// ======== Redirection ==================
/**
 * @brief Create a new file redirection for a process
 *
 * @param[out] redirection Pointer to store the created redirection
 * @param[in] filepath Path to the file
 * @param[in] flags File open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.)
 * @param[in] mode File mode for creation (if O_CREAT is used)
 * @return ProcessError
 */
ProcessError process_redirect_to_file(FileRedirection** redirection, const char* filepath, int flags,
                                      int mode) {
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

    (*redirection)->fd            = fd;
    (*redirection)->close_on_exec = true;

    return PROCESS_SUCCESS;
}

/**
 * @brief Create a file redirection from an existing file descriptor
 *
 * @param[out] redirection Pointer to store the created redirection
 * @param[in] fd Existing file descriptor
 * @param[in] close_on_exec Whether to close the FD when the process exits
 * @return ProcessError
 */
ProcessError process_redirect_to_fd(FileRedirection** redirection, int fd, bool close_on_exec) {
    if (!redirection || fd < 0) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    *redirection = (FileRedirection*)malloc(sizeof(FileRedirection));
    if (!*redirection) {
        return PROCESS_ERROR_MEMORY;
    }

    (*redirection)->fd            = fd;
    (*redirection)->close_on_exec = close_on_exec;

    return PROCESS_SUCCESS;
}

/**
 * @brief Close and free a file redirection
 *
 * @param redirection The redirection to close
 */
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

/**
 * @brief Create a process with extended redirection options
 *
 * @param[out] handle Pointer to store the process handle
 * @param[in] command Command to execute
 * @param[in] argv Arguments for the command (NULL-terminated)
 * @param[in] options Process options with extended IO
 * @return ProcessError
 */
ProcessError process_create_with_redirection(ProcessHandle** handle, const char* command,
                                             const char* const argv[], const ExtProcessOptions* options) {
    if (!handle || !command || !argv || !argv[0]) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }
    // Fork the process
    pid_t pid = fork();

    if (pid < 0) {
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
                return PROCESS_ERROR_IO;
            };

            // Close pipe handles that aren't needed in child
            close(options->io.stdin_pipe->read_fd);
            close(options->io.stdin_pipe->write_fd);
        }

        // Handle standard output
        if (options->io.stdout_pipe) {
            if (dup2(options->io.stdout_pipe->write_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                return PROCESS_ERROR_IO;
            };
            close(options->io.stdout_pipe->read_fd);
            close(options->io.stdout_pipe->write_fd);
        } else if (options->io.stdout_file) {
            // Redirect stdout to file
            if (dup2(options->io.stdout_file->fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                return PROCESS_ERROR_IO;
            };
            if (options->io.stdout_file->close_on_exec) {
                close(options->io.stdout_file->fd);
            }
        }

        // Handle standard error
        if (options->io.stderr_pipe) {
            if (dup2(options->io.stderr_pipe->write_fd, STDERR_FILENO) == -1) {
                perror("dup2");
                return PROCESS_ERROR_IO;
            };
            close(options->io.stderr_pipe->read_fd);
            close(options->io.stderr_pipe->write_fd);
        } else if (options->io.stderr_file) {
            // Redirect stderr to file
            if (dup2(options->io.stderr_file->fd, STDERR_FILENO) == -1) {
                perror("dup2");
                return PROCESS_ERROR_IO;
            };
            if (options->io.stderr_file->close_on_exec) {
                close(options->io.stderr_file->fd);
            }
        } else if (options->io.merge_stderr) {
            if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                perror("dup2");
                return PROCESS_ERROR_IO;
            };
        }

        // Detach from parent if requested
        if (options->detached) {
            if (setsid() < 0) {
                _exit(127);
            }
        }

        // Execute the command
        if (options->environment) {
            if (execve(command, (char* const*)argv, (char* const*)options->environment) == -1) {
                perror("execve");
            };
        } else if (options->inherit_environment) {
            if (execvp(command, (char* const*)argv) == -1) {
                perror("execvp");
            };
        } else {
            char* empty_env[] = {NULL};
            if (execve(command, (char* const*)argv, empty_env) == -1) {
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

    (*handle)->pid      = pid;
    (*handle)->detached = options->detached;

    return PROCESS_SUCCESS;
}

ProcessError process_run_with_multiwriter(ProcessResult* result, const char* cmd, const char* args[],
                                          int output_fds[], int error_fds[]) {
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
            exit(EXIT_FAILURE);
        }

        // Redirect stderr to the write end of the stderr pipe
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
            perror("dup2 (stderr)");
            exit(EXIT_FAILURE);
        }

        // Close the write ends of the pipes (they are now duplicated to
        // stdout/stderr)
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Execute the command
        execv(cmd, (char* const*)args);
        // If execv fails, print an error and exit
        perror("execv");
        exit(EXIT_FAILURE);
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
        ssize_t n;

        // Read from the stdout pipe and write to all output_fds
        while ((n = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
            for (int i = 0; output_fds[i] != -1; i++) {
                if (write(output_fds[i], buffer, (size_t)n) < 0) {
                    perror("write (stdout tee)");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Check for read errors
        if (n < 0) {
            perror("read (stdout tee)");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
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
        ssize_t n;

        // Read from the stderr pipe and write to all error_fds
        while ((n = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
            for (int i = 0; error_fds[i] != -1; i++) {
                if (write(error_fds[i], buffer, (size_t)n) < 0) {
                    perror("write (stderr tee)");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Check for read errors
        if (n < 0) {
            perror("read (stderr tee)");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }

    child_count++;

    // Close read ends of the pipes after all tee processes are started
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    // Wait for the command process specifically to get its status
    int cmd_status;
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

/**
 * @brief Helper function to set up redirection to a file
 *
 * @param[in] command Command to run
 * @param[in] argv Command arguments
 * @param[in] stdout_file File to redirect stdout to, or NULL
 * @param[in] stderr_file File to redirect stderr to, or NULL
 * @param[in] append Whether to append to files (true) or overwrite (false)
 * @return ProcessError
 */
ProcessError process_run_with_file_redirection(ProcessHandle** handle, const char* command,
                                               const char* const argv[], const char* stdout_file,
                                               const char* stderr_file, bool append) {
    ExtProcessOptions options;
    memset(&options, 0, sizeof(options));
    options.inherit_environment = true;

    FileRedirection* stdout_redir = NULL;
    FileRedirection* stderr_redir = NULL;
    ProcessError err              = PROCESS_SUCCESS;

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

#endif  // Linux only
