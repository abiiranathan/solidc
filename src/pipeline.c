#include "../include/pipeline.h"

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

/**
 * @brief Execute a pipeline of commands.
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
    if (!commands)
        return;  // Handle NULL input

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
  char *args1[] = {"ls", "-l", NULL};
  char *args2[] = {"grep", "-E", ".c$", NULL};
  char *args3[] = {"wc", "-l", NULL};

  CommandNode *cmd1 = create_command_node(args1);
  CommandNode *cmd2 = create_command_node(args2);
  CommandNode *cmd3 = create_command_node(args3);

  // Create a NULL-terminated array of commands
  CommandNode *commands[] = {cmd1, cmd2, cmd3, NULL};

  // Build the pipeline
  build_pipeline(commands);

  // Open a file to capture the output of the last command
  int output_fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
