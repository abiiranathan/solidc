#ifndef A1A6896E_581A_42E7_8391_B3EA30D28307
#define A1A6896E_581A_42E7_8391_B3EA30D28307

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L  // for kill in signal.h
#endif

// Include the necessary headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>       // for _pipe
#include <process.h>  // for _execvp
#include <windows.h>  // for CreateProcess, WaitForSingleObject, etc
#else
#include <sys/types.h>  // for pid_t
#include <sys/wait.h>   // for waitpid, WIFEXITED, WEXITSTATUS
#include <unistd.h>     // for pipe, fork, execvp etc
#endif

// Helper macros for creating NULL terminated arrays
#define ENV_ARRAY(...) ((const char* const[]){__VA_ARGS__, NULL})
static const char* const DEFAULT_ENVP[] = {NULL};

// Cross-platform struct to represent a process.
// On Unix, it is a simple integer representing the process id.
// On Windows, it contains the process id, startup info and process info.
typedef struct Process {
#ifdef _WIN32
    DWORD pid;               // Process id is similar to pi.dwProcessId
    STARTUPINFO si;          // Start up info
    PROCESS_INFORMATION pi;  // Process information
#else
    unsigned long pid;  // Process ID(long to match DWORD on Windows)
#endif
} Process;

// Pipe end to close
typedef enum PipeEnd { PIPE_END_READ = 1, PIPE_END_WRITE = 2, PIPE_END_BOTH = 3 } PipeEnd;

// Cross-platform struct to represent a pipe.
// On Unix, it is a simple array of file descriptors.
// On Windows, it contains the read and write handles.
typedef struct PIPE {
#ifdef _WIN32
    HANDLE hReadPipe;   // Read handle
    HANDLE hWritePipe;  // Write handle
#else
    int fd[2];
#endif
} PIPE;

// =================== Pipe prototypes ===================
// Open a pipe.
int pipe_open(PIPE* pPipe);

// Close a pipe, specifying which end to close.
void pipe_close(PIPE* pPipe, PipeEnd endToClose);

// Read from a pipe into a buffer.
ssize_t pipe_read(PIPE* pPipe, void* buffer, size_t size);

// Write to a pipe from a buffer.
ssize_t pipe_write(PIPE* pPipe, const void* buffer, size_t size);

// ====================== Process functions ======================

/** Create a new process.
* @param proc: Pointer to a Process struct
* @param command: The command to run. If it contains a space, it must be quoted.
* If envp[0] is NULL, the new process inherits the environment of the calling process.
* by calling execvp(command, argv) on Unix or CreateProcess(NULL, command, ...) on Windows.
* @param argv: Array of arguments to the command. The first argument should be the command itself.
* The last argument must be NULL.
* @param envp: Array of environment variables. The last argument must be NULL.
* The envp array elements should be of the form "VAR=VALUE".
* You can use helper macros ENV_ARRAY and DEFAULT_ENVP to create these arrays.
* @return 0 if successful, -1 otherwise
**/
int process_create(Process* proc, const char* command, const char* const argv[],
                   const char* const envp[]);

// Wait for a process to exit
int process_wait(Process* proc, int* status);

// Kill a process
int process_kill(Process* proc);

#ifdef __cplusplus
}
#endif

#endif /* A1A6896E_581A_42E7_8391_B3EA30D28307 */
