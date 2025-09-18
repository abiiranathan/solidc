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
 * @param head Pointer to the first CommandNode in the pipeline.
 * @param output_fd Optional file descriptor to capture the output of the last
 * command.
 */
void execute_pipeline(CommandNode* head, int output_fd) {
    int pipefd[2];
    int prev_pipe_read_end = -1;
    CommandNode* current   = head;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    HANDLE* proc_handles = NULL;
    int proc_count       = 0;
    int command_count    = 0;

    // Count commands first to allocate handles array
    CommandNode* count_node = head;
    while (count_node != NULL) {
        command_count++;
        count_node = count_node->next;
    }

    proc_handles = (HANDLE*)malloc(command_count * sizeof(HANDLE));
    if (!proc_handles) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    while (current != NULL) {
        // Create pipe if there's a next command
        if (current->next != NULL) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        // Prepare the command string
        char command[1024] = "";
        int i              = 0;
        while (current->args[i] != NULL) {
            // Add quotes if the argument contains spaces
            if (strchr(current->args[i], ' ') != NULL) {
                strcat(command, "\"");
                strcat(command, current->args[i]);
                strcat(command, "\" ");
            } else {
                strcat(command, current->args[i]);
                strcat(command, " ");
            }
            i++;
        }

        // Initialize STARTUPINFO
        ZeroMemory(&si, sizeof(si));
        si.cb      = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;

        // Set standard handles
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

        // Redirect input from previous pipe if any
        if (prev_pipe_read_end != -1) {
            si.hStdInput = (HANDLE)_get_osfhandle(prev_pipe_read_end);
        }

        // Redirect output to next pipe or specified file descriptor
        if (current->next != NULL) {
            si.hStdOutput = (HANDLE)_get_osfhandle(pipefd[1]);
        } else if (output_fd != -1) {
            si.hStdOutput = (HANDLE)_get_osfhandle(output_fd);
        }

        // Create the process
        ZeroMemory(&pi, sizeof(pi));
        if (!CreateProcess(NULL,     // No module name (use command line)
                           command,  // Command line
                           NULL,     // Process handle not inheritable
                           NULL,     // Thread handle not inheritable
                           TRUE,     // Set handle inheritance to TRUE
                           0,        // No creation flags
                           NULL,     // Use parent's environment block
                           NULL,     // Use parent's starting directory
                           &si,      // Pointer to STARTUPINFO structure
                           &pi       // Pointer to PROCESS_INFORMATION structure
                           )) {
            fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
            exit(EXIT_FAILURE);
        }

        // Store process handle
        proc_handles[proc_count++] = pi.hProcess;

        // Close thread handle (not needed)
        CloseHandle(pi.hThread);

        // Close write end of pipe
        if (current->next != NULL) {
            close(pipefd[1]);
        }

        // Close read end of previous pipe
        if (prev_pipe_read_end != -1) {
            close(prev_pipe_read_end);
        }

        // Save read end for next command
        prev_pipe_read_end = pipefd[0];

        current = current->next;
    }

    // Wait for all processes to finish
    WaitForMultipleObjects(proc_count, proc_handles, TRUE, INFINITE);

    // Close process handles
    for (int i = 0; i < proc_count; i++) {
        CloseHandle(proc_handles[i]);
    }

    free(proc_handles);
}
#else
/**
 * @brief Execute a pipeline of commands on UNIX.
 *
 * @param head Pointer to the first CommandNode in the pipeline.
 * @param output_fd Optional file descriptor to capture the output of the last
 * command. If -1, the output is not redirected.
 */
void execute_pipeline(CommandNode* head, int output_fd) {
    int pipefd[2];
    int prev_pipe_read_end = -1;
    CommandNode* current   = head;

    while (current != NULL) {
        // Create a pipe if there's a next command
        if (current->next != NULL) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        // Fork a child process
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {  // Child process
            // Redirect input from the previous pipe (if any)
            if (prev_pipe_read_end != -1) {
                dup2(prev_pipe_read_end, STDIN_FILENO);
                close(prev_pipe_read_end);
            }

            // Redirect output to the next pipe or the specified file descriptor
            if (current->next != NULL) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            } else if (output_fd != -1) {
                // Redirect output of the last command to the specified file descriptor
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            // Execute the command
            execvp(current->args[0], current->args);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else {  // Parent process
            // Close the write end of the current pipe (if any)
            if (current->next != NULL) {
                close(pipefd[1]);
            }

            // Close the read end of the previous pipe (if any)
            if (prev_pipe_read_end != -1) {
                close(prev_pipe_read_end);
            }

            // Save the read end of the current pipe for the next command
            prev_pipe_read_end = pipefd[0];
        }

        current = current->next;
    }

    // Wait for all child processes to finish
    while (wait(NULL) > 0)
        ;
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
