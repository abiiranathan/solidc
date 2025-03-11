#ifndef PIPELINE_H
#define PIPELINE_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__cplusplus__)
#extern "C" {
#endif

// CommandNode represents a command in the pipeline.
typedef struct CommandNode {
    char** args;               // Command arguments (NULL-terminated)
    struct CommandNode* next;  // Pointer to the next command in the pipeline
} CommandNode;

/**
 * @brief Create a new CommandNode.
 *
 * @param args Array of command arguments (NULL-terminated).
 * @return Pointer to the newly created CommandNode.
 */
CommandNode* create_command_node(char** args);

/**
 * @brief Execute a pipeline of commands.
 *
 * @param head Pointer to the first CommandNode in the pipeline.
 * @param output_fd File descriptor to capture the output of the last command.
 *                 If -1, the output is not redirected.
 */
void execute_pipeline(CommandNode* head, int output_fd);

/**
 * @brief Free the linked list of commands.
 *
 * @param head Pointer to the first CommandNode in the pipeline.
 */
void free_pipeline(CommandNode* head);

/**
 * @brief Build the pipeline using a NULL-terminated array of CommandNode
 * pointers.
 *
 * @param commands Array of CommandNode pointers, terminated by NULL.
 */
void build_pipeline(CommandNode** commands);

#if defined(__cplusplus__)
}
#endif

#endif  // PIPELINE_H
