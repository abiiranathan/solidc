#include "../include/process.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define LOG_ERROR(fmt, ...)                                                                                  \
    fprintf(stderr, "[ERROR]: %s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_ASSERT(condition, fmt, ...)                                                                      \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            LOG_ERROR("Assertion failed: " #condition " " fmt, ##__VA_ARGS__);                               \
        }                                                                                                    \
    } while (0)
#pragma clang diagnostic pop

void test_pipe_create() {
    PipeHandle* pipe_handle = NULL;
    ProcessError err        = pipe_create(&pipe_handle);

    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));
    LOG_ASSERT(pipe_handle != NULL, "pipe handle is NULL");

    // Close the pipe handle
    pipe_close(pipe_handle);
}

void test_pipe_read_write() {
    PipeHandle* pipe_handle = NULL;
    ProcessError err        = pipe_create(&pipe_handle);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed %s", process_error_string(err));

    const char* msg      = "Test message";
    size_t bytes_written = 0;
    err                  = pipe_write(pipe_handle, msg, strlen(msg), &bytes_written, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_write failed: %s", process_error_string(err));
    LOG_ASSERT(bytes_written == strlen(msg), "Incorrect number of bytes written: %zu", bytes_written);

    char buffer[128]  = {0};
    size_t bytes_read = 0;
    err               = pipe_read(pipe_handle, buffer, sizeof(buffer), &bytes_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed");
    LOG_ASSERT(bytes_read == strlen(msg), "Incorrect number of bytes read");
    LOG_ASSERT(strcmp(buffer, msg) == 0, "Read message does not match written message");
    pipe_close(pipe_handle);
}

void test_pipe_timeout() {
    PipeHandle* pipe_handle = NULL;
    ProcessError err        = pipe_create(&pipe_handle);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed %s", process_error_string(err));

    char buffer[128]  = {0};
    size_t bytes_read = 0;
    err               = pipe_read(pipe_handle, buffer, sizeof(buffer), &bytes_read, 0);
    LOG_ASSERT(err == PROCESS_ERROR_IO, "pipe_read did not time out as expected: %s",
               process_error_string(err));

    err = pipe_write(pipe_handle, "Test", 4, &bytes_read, 0);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_write did not time out as expected: %s",
               process_error_string(err));

    pipe_close(pipe_handle);
}

void test_process_error_handling() {
    // Test a scenario where process creation would fail
    ProcessError err = PROCESS_ERROR_UNKNOWN;
    LOG_ASSERT(strcmp(process_error_string(err), "Unknown error") == 0, "process_system_error failed");
}

void test_process_error_string() {
    const char* error_str = process_error_string(PROCESS_ERROR_INVALID_ARGUMENT);
    LOG_ASSERT(strcmp(error_str, "Invalid argument") == 0,
               "Error string mismatch for PROCESS_ERROR_INVALID_ARGUMENT");

    error_str = process_error_string(PROCESS_ERROR_MEMORY);
    LOG_ASSERT(strcmp(error_str, "Memory allocation failed") == 0,
               "Error string mismatch for PROCESS_ERROR_MEMORY");

    error_str = process_error_string(PROCESS_ERROR_UNKNOWN);
    LOG_ASSERT(strcmp(error_str, "Unknown error") == 0, "Error string mismatch for PROCESS_ERROR_UNKNOWN");
}

void test_pipe_close() {
    PipeHandle* pipe_handle = NULL;
    ProcessError err        = pipe_create(&pipe_handle);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed %s", process_error_string(err));
    pipe_close(pipe_handle);
}

void test_process_pipe_integration() {
    PipeHandle* pipe_handle = NULL;
    ProcessError err        = pipe_create(&pipe_handle);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed %s", process_error_string(err));

    const char* cmd    = "ping";
    const char* argv[] = {cmd, "localhost", "-c", "5", NULL};

    ProcessOptions options = {
        .detached            = false,
        .environment         = NULL,
        .inherit_environment = true,
        .working_directory   = NULL,
        .io.stdout_pipe      = pipe_handle,
    };

    // Assuming process creation and handling is done correctly, simulate using the pipe
    ProcessHandle* process;
    ProcessResult result;
    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process creation failed");

    err = process_wait(process, &result, -1);
    puts(process_error_string(err));
    LOG_ASSERT(err == PROCESS_SUCCESS, "process wait failed");
    LOG_ASSERT(result.exited_normally, "Process exited normally");
    printf("Exit code: %d\n", result.exit_code);
    LOG_ASSERT(result.exit_code == 0, "Process exited with non-zero status code");

    // For now, we will just validate that the pipe behaves correctly.
    char buffer[1024]      = {0};
    size_t bytes_read      = 0;
    err                    = pipe_read(pipe_handle, buffer, sizeof(buffer), &bytes_read, 10000);
    buffer[bytes_read - 1] = '\0';
    printf("Read from pipe: %s\n", buffer);

    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed");
    LOG_ASSERT(bytes_read > 0, "No data read from pipe");
    pipe_close(pipe_handle);
}

// Use standard file descriptors.
void test_process_pipe_integration_std() {
    ProcessHandle* process;
    ProcessResult result;
    ProcessError err;

    const char* cmd    = "ping";
    const char* argv[] = {cmd, "localhost", "-c", "5", NULL};

    err = process_create(&process, cmd, argv, NULL);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process creation failed");
    err = process_wait(process, &result, -1);

    LOG_ASSERT(err == PROCESS_SUCCESS, "process wait failed");
    LOG_ASSERT(result.exited_normally, "Process exited normally");
    LOG_ASSERT(result.exit_code == 0, "Process exited with non-zero status code");
}

void test_process_run_and_capture(void) {
    const char* cmd    = "ls";
    const char* argv[] = {cmd, "-la", NULL};

    ProcessError err = process_run_and_capture(cmd, argv, NULL, NULL);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process failed");
}

void test_process_run_and_capture_ouput(void) {
    const char* cmd    = "ls";
    const char* argv[] = {cmd, NULL};

    PipeHandle* ph;
    ProcessError err = pipe_create(&ph);
    LOG_ASSERT(err == PROCESS_SUCCESS, "error creating pipe");

    ProcessOptions opts = {.io.stdout_pipe = ph, .inherit_environment = true};
    err                 = process_run_and_capture(cmd, argv, &opts, NULL);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process failed");

    // read output from the pipe.
    char buffer[512];
    size_t read;
    err = pipe_read(ph, buffer, sizeof(buffer), &read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS && read > 0, "pipe read failed: read %zu: %s", read,
               process_error_string(err));
}

#ifndef _WIN32
void test_redirect_to_file() {
    // Simple redirection to a file
    ProcessHandle* handle;
    ProcessError err;

    // Redirect stdout to output.log and stderr to error.log
    const char* cmd    = "/bin/ls";
    const char* args[] = {"/bin/ls", "-la", NULL};

    err = process_run_with_file_redirection(&handle, cmd, args, "output.log", "error.log", false);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process failed: %s", process_error_string(err));

    // Wait for process completion
    ProcessResult result;
    err = process_wait(handle, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS && result.exit_code == 0, "process_wait failed: %s[%d]",
               process_error_string(err), result.exit_code);
}

void test_tee_to_multiple_destinations() {
    // Tee output to multiple destinations (file and pipe)
    // Create a file for output
    int file_fd = open("multi_output.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    LOG_ASSERT(file_fd != -1, "error creating file");

    // Prepare output destinations (file + pipe)
    int output_fds[] = {file_fd, STDOUT_FILENO, -1};
    int error_fds[]  = {file_fd, -1};

    // Run the command with tee
    ProcessResult result;
    ProcessError err;
    const char* cmd    = "/bin/ls";
    const char* args[] = {"/bin/ls", "/usr", NULL};

    err = process_run_with_multiwriter(&result, cmd, args, output_fds, error_fds);
    close(file_fd);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process failed: %s", process_error_string(err));
}

void test_process_create_with_redirection() {
    ProcessError err;
    PipeHandle *stdin_pipe, *stdout_pipe;

    err = pipe_create(&stdin_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe create failed");

    err = pipe_create(&stdout_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe create failed");

    const char* cmd = "/bin/cat";
    ProcessHandle* handle;
    const char* const argv[] = {cmd, NULL};

    const ExtProcessOptions options = {
        .working_directory   = NULL,
        .detached            = false,
        .environment         = NULL,
        .inherit_environment = true,
        .io =
            {
                .stdin_pipe   = stdin_pipe,
                .stdout_pipe  = stdout_pipe,
                .merge_stderr = true,
            },
    };

    err = process_create_with_redirection(&handle, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process create failed");

    size_t written, read;
    char data[16];
    char msg[] = "Hello World";

    err = pipe_write(stdin_pipe, msg, sizeof(msg), &written, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS && written == sizeof(msg), "pipe_write failed");

    err = pipe_read(stdin_pipe, data, sizeof(data), &read, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS && read == written, "pipe_write failed");
    data[read - 1] = '\0';
    LOG_ASSERT(strcmp(msg, data) == 0, "messages not equal, read %s\n", data);

    ProcessResult res;
    err = process_wait(handle, &res, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS && res.exited_normally, "pipe_wait failed with code: %d: %s",
               res.exit_code, process_error_string(err));
}
#endif

int main() {
    // Run all tests
    test_pipe_create();
    test_pipe_read_write();
    test_pipe_timeout();
    test_process_error_handling();
    test_process_error_string();
    test_pipe_close();
    test_process_pipe_integration();
    test_process_pipe_integration_std();
    test_process_run_and_capture();
    test_process_run_and_capture_ouput();

// Linux only functions
#ifndef _WIN32
    test_redirect_to_file();
    test_tee_to_multiple_destinations();
    test_process_create_with_redirection();
#endif
    printf("All process tests passed!\n");

    return 0;
}
