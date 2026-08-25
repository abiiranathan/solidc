/**
 * @file process_win32.c
 * @brief Win32 implementation of the process management backends:
 *        CreateProcess-based spawning with MSDN-safe command-line escaping,
 *        overlapped-I/O pipes with timeouts, and handle-based waiting.
 *
 * NOTE: This file is only compiled for _WIN32 targets and cannot be built
 * on POSIX machines; keep it in sync with process_internal.h.
 */

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

/* Implementation of the pipe API */
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

ProcessError pipe_close_read_end(PipeHandle* pipe) {
    if (!pipe) return PROCESS_ERROR_INVALID_ARGUMENT;
    if (pipe->read_fd != INVALID_NATIVE_HANDLE && !pipe->read_closed) {
        CloseHandle(pipe->read_fd);
        pipe->read_closed = true;
        pipe->read_fd = INVALID_NATIVE_HANDLE;
    }
    return PROCESS_SUCCESS;
}

ProcessError pipe_close_write_end(PipeHandle* pipe) {
    if (!pipe) return PROCESS_ERROR_INVALID_ARGUMENT;
    if (pipe->write_fd != INVALID_NATIVE_HANDLE && !pipe->write_closed) {
        CloseHandle(pipe->write_fd);
        pipe->write_closed = true;
        pipe->write_fd = INVALID_NATIVE_HANDLE;
    }
    return PROCESS_SUCCESS;
}

/**
 * Append a single argv element to dest using proper Windows escaping rules.
 *
 * Rules (from MSDN "Parsing C++ Command-Line Arguments"):
 *  - Backslashes before a double-quote are doubled, then the quote is escaped.
 *  - Backslashes before the closing quote are doubled.
 *  - All other backslashes are literal.
 *  - Arguments with spaces/tabs/quotes are wrapped in double-quotes.
 *
 * Uses the bounds-checked _s string APIs; dest_size must cover the NUL.
 * Returns PROCESS_SUCCESS, or PROCESS_ERROR_UNKNOWN if the append would
 * overflow (cannot happen with the worst-case sized command-line buffer,
 * but never rely on that at the API boundary).
 */
static ProcessError append_escaped_win32_arg(char* dest, size_t dest_size, const char* arg) {
    /* Fast path: nothing that needs quoting */
    if (*arg != '\0' && !strpbrk(arg, " \t\n\v\"")) {
        return strcat_s(dest, dest_size, arg) == 0 ? PROCESS_SUCCESS : PROCESS_ERROR_UNKNOWN;
    }

    if (strcat_s(dest, dest_size, "\"") != 0) {
        return PROCESS_ERROR_UNKNOWN;
    }

    for (const char* p = arg; *p != '\0';) {
        /* Count consecutive backslashes */
        int num_bs = 0;
        while (*p == '\\') {
            num_bs++;
            p++;
        }

        if (*p == '\0') {
            /* Trailing backslashes: double them before the closing quote */
            for (int k = 0; k < num_bs * 2; k++) {
                if (strcat_s(dest, dest_size, "\\") != 0) return PROCESS_ERROR_UNKNOWN;
            }
            break;
        } else if (*p == '"') {
            /* Backslashes before a quote: double them, then escape the quote */
            for (int k = 0; k < num_bs * 2 + 1; k++) {
                if (strcat_s(dest, dest_size, "\\") != 0) return PROCESS_ERROR_UNKNOWN;
            }
            if (strcat_s(dest, dest_size, "\"") != 0) return PROCESS_ERROR_UNKNOWN;
            p++;
        } else {
            /* Literal backslashes followed by a normal char */
            for (int k = 0; k < num_bs; k++) {
                if (strcat_s(dest, dest_size, "\\") != 0) return PROCESS_ERROR_UNKNOWN;
            }
            const char ch[2] = {*p, '\0'};
            if (strcat_s(dest, dest_size, ch) != 0) return PROCESS_ERROR_UNKNOWN;
            p++;
        }
    }

    return strcat_s(dest, dest_size, "\"") == 0 ? PROCESS_SUCCESS : PROCESS_ERROR_UNKNOWN;
}

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

    for (int i = 0; i < arg_count; i++) {
        if (i > 0) {
            if (strcat_s(cmdline, cmdline_len + 1, " ") != 0) {
                free(cmdline);
                return PROCESS_ERROR_UNKNOWN;
            }
        }
        ProcessError err = append_escaped_win32_arg(cmdline, cmdline_len + 1, argv[i]);
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
