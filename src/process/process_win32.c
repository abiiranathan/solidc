/**
 * @file process_win32.c
 * @brief Win32 implementation of the process management backends:
 *        CreateProcess-based spawning with MSDN-safe command-line escaping,
 *        overlapped-I/O pipes with timeouts, and handle-based waiting.
 *
 * NOTE: This file is only compiled for _WIN32 targets and cannot be built
 * on POSIX machines; keep it in sync with process_internal.h.
 */

#include "macros.h"
#include "process_internal.h"

#include <string.h>

#define ACCESS _access

#ifndef X_OK
    // Windows doesn't have X_OK, but MinGW does.
    #define X_OK 0
#endif

/* Process handle: primary process/thread handles from CreateProcess. */
struct ProcessHandle {
    PROCESS_INFORMATION process_info;
    bool detached;
};

/** @brief Maps GetLastError() onto ProcessError codes: invalid parameter, out of memory, access denied,
 * broken/busy/disconnected pipe; anything else maps to PROCESS_ERROR_UNKNOWN. @return Matching ProcessError value. */
ProcessError process_system_error(void) {
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
}

/**
 * @brief Creates a unidirectional anonymous pipe for IPC.
 *
 * @param[out] pipeHandle Receives the allocated pipe handle on success.
 * @return PROCESS_SUCCESS, PROCESS_ERROR_INVALID_ARGUMENT, or
 *         PROCESS_ERROR_PIPE_FAILED / PROCESS_ERROR_MEMORY on failure.
 */
ProcessError pipe_create(PipeHandle** pipeHandle) {
    if (!pipeHandle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    *pipeHandle = (PipeHandle*)calloc(1, sizeof(PipeHandle));
    if (!*pipeHandle) {
        return PROCESS_ERROR_MEMORY;
    }

    SECURITY_ATTRIBUTES security_attrs;
    memset(&security_attrs, 0, sizeof(security_attrs));
    security_attrs.nLength = sizeof(security_attrs);
    security_attrs.bInheritHandle = TRUE;

    if (!CreatePipe(&(*pipeHandle)->read_fd, &(*pipeHandle)->write_fd, &security_attrs, 0)) {
        free(*pipeHandle);
        *pipeHandle = NULL;
        return PROCESS_ERROR_PIPE_FAILED;
    }

    return PROCESS_SUCCESS;
}

/** @brief Switches both ends of the pipe between PIPE_NOWAIT and PIPE_WAIT via SetNamedPipeHandleState(). @return
 * PROCESS_SUCCESS, or the mapped system error if either end cannot be updated. */
ProcessError pipe_set_nonblocking(PipeHandle* pipe, bool nonblocking) {
    if (!pipe) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    DWORD mode = nonblocking ? PIPE_NOWAIT : PIPE_WAIT;
    if (!SetNamedPipeHandleState(pipe->read_fd, &mode, NULL, NULL)) {
        return process_system_error();
    }
    if (!SetNamedPipeHandleState(pipe->write_fd, &mode, NULL, NULL)) {
        return process_system_error();
    }

    return PROCESS_SUCCESS;
}

/** @brief Overlapped ReadFile() with a manual-reset completion event: waits up to timeout_ms (-1 = INFINITE) for
 * completion, CancelIo()s on timeout, and maps broken-pipe errors to PROCESS_ERROR_PIPE_CLOSED; a 0 ms timeout with no
 * data yields WOULD_BLOCK. @param[out] buffer Destination for the data. @param[out] bytes_read Optional receiver filled
 * from GetOverlappedResult(). @return PROCESS_SUCCESS on success, error code otherwise. */
ProcessError pipe_read(PipeHandle* pipe, void* buffer, size_t size, size_t* bytes_read, int timeout_ms) {
    if (!pipe || !buffer || pipe->read_closed) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (bytes_read) {
        *bytes_read = 0;
    }

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
            if (error == ERROR_BROKEN_PIPE) {
                return PROCESS_ERROR_PIPE_CLOSED;
            }
            return process_system_error();
        }
    }

    DWORD wait_result = WaitForSingleObject(overlapped.hEvent, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);

    if (wait_result == WAIT_OBJECT_0) {
        if (!GetOverlappedResult(pipe->read_fd, &overlapped, &bytes_read_win, FALSE)) {
            CloseHandle(overlapped.hEvent);
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                return PROCESS_ERROR_PIPE_CLOSED;
            }
            return process_system_error();
        }
        if (bytes_read) {
            *bytes_read = bytes_read_win;
        }
    } else if (wait_result == WAIT_TIMEOUT) {
        CancelIo(pipe->read_fd);
        CloseHandle(overlapped.hEvent);
        // Map 0ms timeout to WOULDBLOCK for consistency with non-blocking reads
        return (timeout_ms == 0) ? PROCESS_ERROR_WOULD_BLOCK : PROCESS_ERROR_TIMEOUT;
    } else {
        CloseHandle(overlapped.hEvent);
        return process_system_error();
    }

    CloseHandle(overlapped.hEvent);

    return PROCESS_SUCCESS;
}

/** @brief Overlapped WriteFile() mirroring pipe_read(): event-driven wait bounded by timeout_ms (-1 = INFINITE),
 * CancelIo() on timeout, and broken-pipe/no-data errors mapped to PROCESS_ERROR_PIPE_CLOSED. @param[in] buffer Data to
 * write. @param[out] bytes_written Optional receiver filled from GetOverlappedResult(). @return PROCESS_SUCCESS on
 * success, error code otherwise. */
ProcessError pipe_write(PipeHandle* pipe, const void* buffer, size_t size, size_t* bytes_written, int timeout_ms) {
    if (!pipe || !buffer || pipe->write_closed) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (bytes_written) {
        *bytes_written = 0;
    }

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
            if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                return PROCESS_ERROR_PIPE_CLOSED;
            }
            return process_system_error();
        }
    }

    DWORD wait_result = WaitForSingleObject(overlapped.hEvent, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);

    if (wait_result == WAIT_OBJECT_0) {
        if (!GetOverlappedResult(pipe->write_fd, &overlapped, &bytes_written_win, FALSE)) {
            CloseHandle(overlapped.hEvent);
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                return PROCESS_ERROR_PIPE_CLOSED;
            }
            return process_system_error();
        }
        if (bytes_written) {
            *bytes_written = bytes_written_win;
        }
    } else if (wait_result == WAIT_TIMEOUT) {
        CancelIo(pipe->write_fd);
        CloseHandle(overlapped.hEvent);
        return PROCESS_ERROR_TIMEOUT;
    } else {
        CloseHandle(overlapped.hEvent);
        return process_system_error();
    }

    CloseHandle(overlapped.hEvent);

    return PROCESS_SUCCESS;
}

/** @brief CloseHandle()s both pipe ends (marking them closed and resetting the handles) and frees the handle. Safe to
 * pass NULL. */
void pipe_close(PipeHandle* pipe) {
    if (!pipe) {
        return;
    }

    if (pipe->read_fd != INVALID_NATIVE_HANDLE && !pipe->read_closed) {
        CloseHandle(pipe->read_fd);
        pipe->read_closed = true;
        pipe->read_fd = INVALID_NATIVE_HANDLE;
    }
    if (pipe->write_fd != INVALID_NATIVE_HANDLE && !pipe->write_closed) {
        CloseHandle(pipe->write_fd);
        pipe->write_closed = true;
        pipe->write_fd = INVALID_NATIVE_HANDLE;
    }

    free(pipe);
}

/** @brief Closes only the read handle of the pipe; the write end stays usable. Idempotent. @return PROCESS_SUCCESS, or
 * PROCESS_ERROR_INVALID_ARGUMENT if pipe is NULL. */
ProcessError pipe_close_read_end(PipeHandle* pipe) {
    if (!pipe) return PROCESS_ERROR_INVALID_ARGUMENT;
    if (pipe->read_fd != INVALID_NATIVE_HANDLE && !pipe->read_closed) {
        CloseHandle(pipe->read_fd);
        pipe->read_closed = true;
        pipe->read_fd = INVALID_NATIVE_HANDLE;
    }
    return PROCESS_SUCCESS;
}

/** @brief Closes only the write handle of the pipe; the read end stays usable. Idempotent. @return PROCESS_SUCCESS, or
 * PROCESS_ERROR_INVALID_ARGUMENT if pipe is NULL. */
ProcessError pipe_close_write_end(PipeHandle* pipe) {
    if (!pipe) return PROCESS_ERROR_INVALID_ARGUMENT;
    if (pipe->write_fd != INVALID_NATIVE_HANDLE && !pipe->write_closed) {
        CloseHandle(pipe->write_fd);
        pipe->write_closed = true;
        pipe->write_fd = INVALID_NATIVE_HANDLE;
    }
    return PROCESS_SUCCESS;
}

/** @brief Appends one argv element to dest per the MSDN command-line parsing rules: arguments containing whitespace or
 * quotes are wrapped in double quotes, backslash runs before a quote (or the end of the argument) are doubled, and a
 * literal '"' becomes "\\". Uses a write cursor so the whole command line is built in O(n) — a single capacity check
 * per token guarantees no overflow (the caller pre-sizes the buffer to the worst case). @param dest Destination buffer.
 * @param dest_size Total capacity of dest in bytes. @param arg Argument to append. @param cursor In/out write position
 * (index of the NUL terminator). @return PROCESS_SUCCESS, or PROCESS_ERROR_UNKNOWN if dest_size would be exceeded. */
static ProcessError append_escaped_win32_arg(char* dest, size_t dest_size, const char* arg, size_t* cursor) {
    /* Fast path: nothing that needs quoting */
    if (*arg != '\0' && !strpbrk(arg, " \t\n\v\"")) {
        size_t len = strlen(arg);
        if (*cursor + len + 1 > dest_size) return PROCESS_ERROR_UNKNOWN;
        memcpy(dest + *cursor, arg, len + 1);
        *cursor += len;
        return PROCESS_SUCCESS;
    }

    size_t pos = *cursor;
    if (pos + 1 >= dest_size) return PROCESS_ERROR_UNKNOWN;
    dest[pos++] = '"';

    for (const char* p = arg; *p != '\0';) {
        /* Count consecutive backslashes */
        int num_bs = 0;
        while (*p == '\\') {
            num_bs++;
            p++;
        }

        /* Backslashes to emit for this run: doubled before a quote or the end */
        int emit_bs;
        if (*p == '\0') {
            emit_bs = num_bs * 2;
        } else if (*p == '"') {
            emit_bs = num_bs * 2 + 1;
        } else {
            emit_bs = num_bs;
        }

        if (pos + (size_t)emit_bs + 2 >= dest_size) return PROCESS_ERROR_UNKNOWN;
        for (int k = 0; k < emit_bs; k++) dest[pos++] = '\\';

        if (*p == '\0') break;
        if (*p == '"') {
            dest[pos++] = '"';
            p++;
        } else {
            dest[pos++] = *p;
            p++;
        }
    }

    if (pos + 2 > dest_size) return PROCESS_ERROR_UNKNOWN;
    dest[pos++] = '"';
    dest[pos] = '\0';
    *cursor = pos;
    return PROCESS_SUCCESS;
}

/** @brief CreateProcessA() backend for process_create(): sizes a worst-case command-line buffer (each char may double
 * plus quotes/space), escapes every argv element via append_escaped_win32_arg(), wires stdio handles from options (pipe
 * ends, or the parent console stdio; stderr merges into stdout when requested), applies
 * DETACHED_PROCESS|CREATE_NEW_PROCESS_GROUP when detached, and stores the returned process/thread handles. @return
 * PROCESS_SUCCESS; PROCESS_ERROR_INVALID_ARGUMENT (empty argv), PROCESS_ERROR_MEMORY, or a mapped CreateProcess error
 * otherwise. */
ProcessError win32_create_process(ProcessHandle** handle, const char* command, const char* const argv[],
                                  const ProcessOptions* options) {
    /* Calculate an upper-bound for the command-line buffer.
     * Each character can expand to at most 2 (backslash doubling) plus
     * 2 surrounding quotes + 1 space separator. */
    size_t cmdline_len = 0;
    int arg_count = 0;

    while (argv[arg_count] != NULL) {
        cmdline_len += strlen(argv[arg_count]) * 2 + 4; /* worst-case escaping */
        arg_count++;
    }
    if (arg_count == 0) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    char* cmdline = (char*)malloc(cmdline_len + 1);
    if (!cmdline) {
        return PROCESS_ERROR_MEMORY;
    }
    cmdline[0] = '\0';

    size_t cursor = 0;
    for (int i = 0; i < arg_count; i++) {
        if (i > 0) {
            if (cursor + 2 > cmdline_len + 1) {
                free(cmdline);
                return PROCESS_ERROR_UNKNOWN;
            }
            cmdline[cursor++] = ' ';
            cmdline[cursor] = '\0';
        }
        ProcessError err = append_escaped_win32_arg(cmdline, cmdline_len + 1, argv[i], &cursor);
        if (err != PROCESS_SUCCESS) {
            free(cmdline);
            return err;
        }
    }

    /* Prepare startup info with redirections */
    STARTUPINFOA startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    if (options->io.stdin_pipe) startup_info.hStdInput = options->io.stdin_pipe->read_fd;
    if (options->io.stdout_pipe) startup_info.hStdOutput = options->io.stdout_pipe->write_fd;

    if (options->io.stderr_pipe) {
        startup_info.hStdError = options->io.stderr_pipe->write_fd;
    } else if (options->io.merge_stderr) {
        startup_info.hStdError = startup_info.hStdOutput;
    }

    DWORD creation_flags = 0;
    if (options->detached) {
        creation_flags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
    }

    PROCESS_INFORMATION process_info;
    BOOL success = CreateProcessA(command, cmdline, NULL, NULL, TRUE, creation_flags, (LPVOID)(options->environment),
                                  options->working_directory, &startup_info, &process_info);

    free(cmdline);

    if (!success) {
        return process_system_error();
    }

    *handle = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    if (!*handle) {
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        return PROCESS_ERROR_MEMORY;
    }

    (*handle)->process_info = process_info;
    (*handle)->detached = options->detached;

    return PROCESS_SUCCESS;
}

// Cross-platform nanosleep function
/** @brief Win32 backend of the cross-platform sleep: converts to milliseconds for Sleep(), clamping negative inputs to
 * zero and capping the total at Sleep()'s documented maximum minus one tick. */
void NANOSLEEP(long seconds, long nanoseconds) {
    // On Windows, Sleep works in milliseconds,
    // so we convert seconds and nanoseconds to milliseconds.
    // Clamp to avoid signed overflow on extreme inputs.
    if (seconds < 0) seconds = 0;
    if (nanoseconds < 0) nanoseconds = 0;
    unsigned long total_milliseconds = (unsigned long)seconds * 1000ul + (unsigned long)nanoseconds / 1000000ul;
    if (total_milliseconds > 0xFFFFFFFEul) total_milliseconds = 0xFFFFFFFEul; /* Sleep's documented max - 1 */
    Sleep((DWORD)total_milliseconds);
}

/** @brief WaitForSingleObject() on the child process handle (timeout_ms < 0 = INFINITE), then GetExitCodeProcess() to
 * fill result; term_signal is always 0 on Windows. Detached handles cannot be waited on. @return PROCESS_SUCCESS when
 * the child exited and the exit code was fetched; PROCESS_ERROR_WAIT_FAILED on timeout (child still running);
 * PROCESS_ERROR_INVALID_ARGUMENT or a mapped system error otherwise. */
ProcessError process_wait(ProcessHandle* handle, ProcessResult* result, int timeout_ms) {
    if (!handle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

    if (handle->detached) {
        // Cannot wait for detached processes
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

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

        result->exit_code = (int)exit_code;
        result->exited_normally = true;
        result->term_signal = 0;  // No signal on Windows
    }

    return PROCESS_SUCCESS;
}

/** @brief Terminate the child: force=true calls TerminateProcess() (SIGKILL analogue); otherwise
 * GenerateConsoleCtrlEvent(CTRL_C_EVENT) is used for graceful console termination. @return PROCESS_SUCCESS;
 * PROCESS_ERROR_INVALID_ARGUMENT for NULL handles; PROCESS_ERROR_TERMINATE_FAILED if the Win32 call fails. */
ProcessError process_terminate(ProcessHandle* handle, bool force) {
    if (!handle) {
        return PROCESS_ERROR_INVALID_ARGUMENT;
    }

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
    return PROCESS_SUCCESS;
}
