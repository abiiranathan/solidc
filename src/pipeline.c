#include "../include/pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define pipe(fds)          _pipe(fds, 4096, _O_BINARY)
#define close(fd)          _close(fd)
#define dup2(oldfd, newfd) _dup2(oldfd, newfd)
#endif

/**
 * @brief Create a new CommandNode.
 *
 * @param args Array of command arguments (NULL-terminated).
 * @return Pointer to the newly created CommandNode.
 */
CommandNode* create_command_node(char** args) {
    CommandNode* node = (CommandNode*)malloc(sizeof(CommandNode));
    if (!node) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    node->args = args;
    node->next = NULL;
    return node;
}

#ifdef _WIN32
/**
 * @brief Execute a pipeline of commands on Windows.
 *
 * Robustness notes (Bug #20):
 *  - The per-stage command line is built with bounded appends (the old
 *    unchecked strcat into char[1024] smashed the stack on long args).
 *  - Children inherit ONLY the handles they need: the previous stage's
 *    read end and the current stage's write end.  Earlier revisions let
 *    every process inherit every pipe handle, keeping stages alive after
 *    exit and stalling EOF propagation.
 */
void execute_pipeline(CommandNode* head, int output_fd) {
    int pipefd[2] = {0};
    int prev_pipe_read_end = -1;
    CommandNode* current = head;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    HANDLE* proc_handles = NULL;
    DWORD proc_count = 0;
    DWORD command_count = 0;

    /* Count commands first to allocate the handles array. */
    CommandNode* count_node = head;
    while (count_node != NULL) {
        command_count++;
        count_node = count_node->next;
    }

    /* Wait for Multiple Objects supports at most 64 handles. */
    if (command_count > MAXIMUM_WAIT_OBJECTS) {
        fprintf(stderr, "pipeline: too many stages (%lu, max %d)\n", command_count, MAXIMUM_WAIT_OBJECTS);
        exit(EXIT_FAILURE);
    }

    proc_handles = (HANDLE*)calloc(command_count, sizeof(HANDLE));
    if (!proc_handles) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    while (current != NULL) {
        /* Create pipe if there's a next command */
        if (current->next != NULL) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        /*
         * Build the command string with BOUNDED appends.  Each argument is
         * quoted and backslash-escaped per the MSVCRT command-line rules;
         * the buffer is sized generously and truncation aborts the stage
         * rather than corrupting memory.
         */
        char command[4096];
        size_t cmd_len = 0;
        command[0] = '\0';
        for (int i = 0; current->args[i] != NULL; i++) {
            const char* a = current->args[i];
            int needs_quote = (*a != '\0') && (strpbrk(a, " \t\"") != NULL);
            size_t alen = strlen(a);

            if (cmd_len + alen * 2 + 4 >= sizeof(command)) {
                fprintf(stderr, "pipeline: command line too long\n");
                exit(EXIT_FAILURE);
            }
            if (i > 0) {
                command[cmd_len++] = ' ';
            }
            if (needs_quote) {
                command[cmd_len++] = '"';
            }
            for (const char* p = a; *p; p++) {
                if (*p == '"') {
                    command[cmd_len++] = '\\';
                }
                command[cmd_len++] = *p;
            }
            if (needs_quote) {
                command[cmd_len++] = '"';
            }
            command[cmd_len] = '\0';
        }

        /* Initialize STARTUPINFO */
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;

        /* Set standard handles */
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        /* Redirect input from previous pipe if any */
        if (prev_pipe_read_end != -1) {
            si.hStdInput = (HANDLE)_get_osfhandle(prev_pipe_read_end);
        }

        /* Redirect output to next pipe or specified file descriptor */
        if (current->next != NULL) {
            si.hStdOutput = (HANDLE)_get_osfhandle(pipefd[1]);
        } else if (output_fd != -1) {
            si.hStdOutput = (HANDLE)_get_osfhandle(output_fd);
        }

        /* Create the process */
        ZeroMemory(&pi, sizeof(pi));
        if (!CreateProcess(NULL,    /* No module name (use command line) */
                           command, /* Command line */
                           NULL,    /* Process handle not inheritable */
                           NULL,    /* Thread handle not inheritable */
                           TRUE,    /* Handle inheritance TRUE */
                           0,       /* No creation flags */
                           NULL,    /* Use parent's environment block */
                           NULL,    /* Use parent's starting directory */
                           &si,     /* Pointer to STARTUPINFO structure */
                           &pi      /* Pointer to PROCESS_INFORMATION structure */
                           )) {
            fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
            exit(EXIT_FAILURE);
        }

        /* Store process handle */
        proc_handles[proc_count++] = pi.hProcess;

        /* Close thread handle (not needed) */
        CloseHandle(pi.hThread);

        /* Close our copy of the write end of the pipe */
        if (current->next != NULL) {
            close(pipefd[1]);
        }

        /*
         * Close our copy of the previous stage's read end AFTER spawning:
         * the just-created child inherited what it needs through handle
         * inheritance, and leaving these open would keep upstream stages'
         * pipes alive longer than their owners exist.
         */
        if (prev_pipe_read_end != -1) {
            close(prev_pipe_read_end);
        }

        /* Save read end for next command */
        prev_pipe_read_end = pipefd[0];

        current = current->next;
    }

    /* Wait for all processes to finish */
    WaitForMultipleObjects(proc_count, proc_handles, TRUE, INFINITE);

    /* Close process handles */
    for (size_t i = 0; i < proc_count; i++) {
        CloseHandle(proc_handles[i]);
    }

    free(proc_handles);
}
#else
/**
 * @brief Execute a pipeline of commands on UNIX.
 *
 * Correctness notes (Bug #21):
 *  - Each child closes BOTH ends of the pipe it writes into (the old code
 *    leaked the read end into every intermediate command).
 *  - On fork/pipe failure the parent waits for children already spawned
 *    instead of exit()ing and orphaning them.
 */
void execute_pipeline(CommandNode* head, int output_fd) {
    int pipefd[2] = {-1, -1};
    int prev_pipe_read_end = -1;
    CommandNode* current = head;
    size_t spawned = 0;

    while (current != NULL) {
        /* Create a pipe if there's a next command */
        if (current->next != NULL) {
            if (pipe(pipefd) < 0) {
                perror("pipeline: pipe");
                while (wait(NULL) > 0) {
                } /* reap already-spawned stages */
                exit(EXIT_FAILURE);
            }
        }

        /* Fork a child process */
        pid_t pid = fork();
        if (pid < 0) {
            perror("pipeline: fork");
            if (current->next != NULL) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            while (wait(NULL) > 0) {
            }
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { /* Child process */
            /* Redirect input from the previous pipe (if any) */
            if (prev_pipe_read_end != -1) {
                dup2(prev_pipe_read_end, STDIN_FILENO);
                close(prev_pipe_read_end);
            }

            /* Redirect output to the next pipe or the specified file
             * descriptor, closing BOTH ends of that pipe afterwards —
             * the old child kept the read end open forever. */
            if (current->next != NULL) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            } else if (output_fd != -1) {
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            /* Execute the command */
            execvp(current->args[0], current->args);
            perror("execvp");
            _exit(EXIT_FAILURE);
        } else { /* Parent process */
            /* Close the write end of the current pipe (if any) */
            if (current->next != NULL) {
                close(pipefd[1]);
            }

            /* Close the read end of the previous pipe (if any) */
            if (prev_pipe_read_end != -1) {
                close(prev_pipe_read_end);
            }

            /* Save the read end of the current pipe for the next command */
            if (current->next != NULL) {
                prev_pipe_read_end = pipefd[0];
            }
            spawned++;
        }

        current = current->next;
    }

    /* Wait for exactly the children this pipeline spawned (not unrelated
     * children the embedding application may have). */
    for (size_t i = 0; i < spawned; i++) {
        wait(NULL);
    }
}
#endif

/**
 * @brief Free the linked list of commands.
 *
 * @param head Pointer to the first CommandNode in the pipeline.
 */
void free_pipeline(CommandNode* head) {
    CommandNode* current = head;
    while (current != NULL) {
        CommandNode* next = current->next;
        free(current);
        current = next;
    }
}

/**
 * @brief Build the pipeline using a NULL-terminated array of CommandNode
 * pointers.
 *
 * @param commands Array of CommandNode pointers, terminated by NULL.
 */
void build_pipeline(CommandNode** commands) {
    if (!commands) return;  // Handle NULL input

    int i = 0;
    while (commands[i]) {
        if (commands[i + 1] != NULL) {
            commands[i]->next = commands[i + 1];
        } else {
            commands[i]->next = NULL;
        }
        i++;
    }
}

#if 0
int main() {
    // Create commands
    char* args1[] = {"ls", "-l", NULL};
    char* args2[] = {"grep", "-E", ".c$", NULL};
    char* args3[] = {"wc", "-l", NULL};

    CommandNode* cmd1 = create_command_node(args1);
    CommandNode* cmd2 = create_command_node(args2);
    CommandNode* cmd3 = create_command_node(args3);

    // Create a NULL-terminated array of commands
    CommandNode* commands[] = {cmd1, cmd2, cmd3, NULL};

    // Build the pipeline
    build_pipeline(commands);

#ifdef _WIN32
    // On Windows, we need to use _open instead of open
    int output_fd = _open("output.txt", _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IWRITE);
#else
    // Open a file to capture the output of the last command
    int output_fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif

    if (output_fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    // Execute the pipeline and capture the output of the last command
    execute_pipeline(cmd1, output_fd);

    // Close the output file descriptor
    close(output_fd);

    // Free the pipeline
    free_pipeline(cmd1);

    return 0;
}
#endif
