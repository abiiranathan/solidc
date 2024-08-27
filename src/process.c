#include "../include/process.h"

#ifdef _WIN32
static int _win32_process_create(Process* proc, const char* const argv[],
                                 const char* const envp[]) {

    ZeroMemory(&proc->si, sizeof(proc->si));
    proc->si.cb = sizeof(proc->si);
    ZeroMemory(&proc->pi, sizeof(proc->pi));

    // Build the command line
    size_t cmdlen = 0;
    for (size_t i = 0; argv[i] != NULL; ++i) {
        cmdlen += strlen(argv[i]) + 1;
    }

    // Add a safe buffer of 16 bytes
    cmdlen += 16;

    char* cmdline = (char*)malloc(cmdlen);
    if (!cmdline) {
        perror("malloc");
        return -1;
    }
    cmdline[0] = '\0';

    for (size_t i = 0; argv[i] != NULL; ++i) {
        strcat(cmdline, argv[i]);  // Concatenate the argument(user should quote if needed)
        strcat(cmdline, " ");
    }

    BOOL success = CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, (LPVOID)envp, NULL, &proc->si,
                                 &(proc->pi));
    free(cmdline);
    if (!success) {
        fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
        return -1;
    }
    return 0;
}
#else
static int linux_process_create(Process* proc, const char* command, const char* const argv[],
                                const char* const envp[]) {

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Child process
        if (envp[0]) {
            execve(command, (char* const*)argv, (char* const*)envp);
        } else {
            execvp(command, (char* const*)argv);
        }
        perror("execve");
        _exit(127);
    } else {
        // Parent process
        proc->pid = pid;
    }
    return 0;
}
#endif

int process_create(Process* proc, const char* command, const char* const argv[],
                   const char* const envp[]) {
#ifdef _WIN32
    (void)command;  // we build the command line from argv
    return _win32_process_create(proc, argv, envp);
#else
    return linux_process_create(proc, command, argv, envp);
#endif
}

/*
Wait for a process to exit.

@param pid: The process id
@param status: A pointer to an integer to store the exit status

@return 0 if successful, -1 otherwise
*/
int process_wait(Process* proc, int* status) {
    int ret = -1;
#ifdef _WIN32
    DWORD dw = WaitForSingleObject(proc->pi.hProcess, INFINITE);
    if (dw == WAIT_OBJECT_0) {
        DWORD exit_code;
        if (GetExitCodeProcess(proc->pi.hProcess, &exit_code)) {
            if (status) {
                *status = exit_code;
            }
            ret = 0;
        }
    }
    CloseHandle(proc->pi.hProcess);
    CloseHandle(proc->pi.hThread);

#else
    // Unix
    int stat;
    if (waitpid((int)proc->pid, &stat, 0) != -1) {
        if (WIFEXITED(stat)) {
            if (status) {
                *status = WEXITSTATUS(stat);
            }
            ret = 0;
        }
    }
#endif
    return ret;
}

/*

Kill a process.
@param pid: The process id
@return 0 if successful, -1 otherwise
*/
int process_kill(Process* proc) {
    int ret = -1;
#ifdef _WIN32
    // Windows
    if (TerminateProcess(proc->pi.hProcess, 0)) {
        ret = 0;
    }
    CloseHandle(proc->pi.hProcess);
    CloseHandle(proc->pi.hThread);
#else
    // Unix
    if (kill((int)proc->pid, SIGKILL) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// ============== Pipe functions ==============
#ifdef _WIN32
int pipe_open(PIPE* pPipe) {
    return CreatePipe(&(pPipe->hReadPipe), &(pPipe->hWritePipe), NULL, 0) ? 0 : -1;
}

void pipe_close(PIPE* pPipe, PipeEnd endToClose) {
    if (endToClose == PIPE_END_READ || endToClose == PIPE_END_BOTH) {
        CloseHandle(pPipe->hReadPipe);
    }
    if (endToClose == PIPE_END_WRITE || endToClose == PIPE_END_BOTH) {
        CloseHandle(pPipe->hWritePipe);
    }
}

ssize_t pipe_read(PIPE* pPipe, void* buffer, size_t size) {
    DWORD bytesRead;
    if (ReadFile(pPipe->hReadPipe, buffer, size, &bytesRead, NULL)) {
        return (ssize_t)bytesRead;
    }
    return -1;
}

ssize_t pipe_write(PIPE* pPipe, const void* buffer, size_t size) {
    DWORD bytesWritten;
    if (WriteFile(pPipe->hWritePipe, buffer, size, &bytesWritten, NULL)) {
        FlushFileBuffers(pPipe->hWritePipe);
        return (ssize_t)bytesWritten;
    }
    return -1;
}
#else
int pipe_open(PIPE* pPipe) {
    return pipe(pPipe->fd);
}

void pipe_close(PIPE* pPipe, PipeEnd endToClose) {
    if (endToClose == PIPE_END_READ || endToClose == PIPE_END_BOTH) {
        close(pPipe->fd[0]);
    }
    if (endToClose == PIPE_END_WRITE || endToClose == PIPE_END_BOTH) {
        close(pPipe->fd[1]);
    }
}

ssize_t pipe_read(PIPE* pPipe, void* buffer, size_t size) {
    return read(pPipe->fd[0], buffer, size);
}

ssize_t pipe_write(PIPE* pPipe, const void* buffer, size_t size) {
    return write(pPipe->fd[1], buffer, size);
}
#endif
