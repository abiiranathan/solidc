/**
 * @file test_process_extended.c
 * @brief Extended test suite for process management API with focus on timeouts,
 *        environment variables, and pipe capture
 */

#include "../include/process.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_ERROR(fmt, ...)                                                                                            \
    fprintf(stderr, "[ERROR]: %s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_ASSERT(condition, fmt, ...)                                                                                \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            LOG_ERROR("Assertion failed: " #condition " " fmt, ##__VA_ARGS__);                                         \
        }                                                                                                              \
    } while (0)

// ============================================================================
// TIMEOUT TESTS
// ============================================================================

/**
 * @brief Test pipe read with immediate timeout (0ms)
 */
void test_pipe_read_immediate_timeout(void) {
    PipeHandle* pipe = NULL;
    ProcessError err = pipe_create(&pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    char buffer[128]  = {0};
    size_t bytes_read = 0;

    // Read with 0ms timeout should return immediately (non-blocking)
    err = pipe_read(pipe, buffer, sizeof(buffer), &bytes_read, 0);
    LOG_ASSERT(err == PROCESS_ERROR_WOULD_BLOCK, "Expected would block error, got: %s", process_error_string(err));
    LOG_ASSERT(bytes_read == 0, "Expected no bytes read, got: %zu", bytes_read);

    pipe_close(pipe);
}

/**
 * @brief Test pipe write with immediate timeout (0ms)
 */
void test_pipe_write_immediate_timeout(void) {
    PipeHandle* pipe = NULL;
    ProcessError err = pipe_create(&pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    const char* msg      = "Test";
    size_t bytes_written = 0;

    // Write with 0ms timeout should succeed if buffer space is available
    err = pipe_write(pipe, msg, strlen(msg), &bytes_written, 0);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_write failed: %s", process_error_string(err));
    LOG_ASSERT(bytes_written == strlen(msg), "Expected %zu bytes written, got: %zu", strlen(msg), bytes_written);

    pipe_close(pipe);
}

/**
 * @brief Test pipe read with short timeout (100ms)
 */
void test_pipe_read_short_timeout(void) {
    PipeHandle* pipe = NULL;
    ProcessError err = pipe_create(&pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    char buffer[128]  = {0};
    size_t bytes_read = 0;

    // Read with 100ms timeout should timeout (no data available)
    err = pipe_read(pipe, buffer, sizeof(buffer), &bytes_read, 100);
    LOG_ASSERT(err == PROCESS_ERROR_TIMEOUT, "Expected timeout error, got: %s", process_error_string(err));

    pipe_close(pipe);
}

void* writer_fn(void* arg) {
    usleep(200000);  // Sleep 200ms
    const char* msg = "Delayed message";
    size_t written  = 0;
    pipe_write((PipeHandle*)arg, msg, strlen(msg), &written, -1);
    return NULL;
}

/**
 * @brief Test pipe read with infinite timeout (-1)
 */
void test_pipe_read_infinite_timeout(void) {
    PipeHandle* pipe = NULL;
    ProcessError err = pipe_create(&pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    // Write data in a separate thread after a delay
    pthread_t writer_thread;
    pthread_create(&writer_thread, NULL, writer_fn, pipe);

    char buffer[128]  = {0};
    size_t bytes_read = 0;

    // Read with infinite timeout should wait for data
    err = pipe_read(pipe, buffer, sizeof(buffer), &bytes_read, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed: %s", process_error_string(err));
    LOG_ASSERT(bytes_read > 0, "Expected data, got %zu bytes", bytes_read);
    LOG_ASSERT(strcmp(buffer, "Delayed message") == 0, "Read data mismatch: %s", buffer);

    pthread_join(writer_thread, NULL);
    pipe_close(pipe);
}

/**
 * @brief Test process_wait with timeout
 */
void test_process_wait_with_timeout(void) {
    ProcessHandle* process = NULL;

    // Use sleep command that takes longer than timeout
    const char* cmd    = "sleep";
    const char* argv[] = {cmd, "5", NULL};  // Sleep for 5 seconds

    ProcessError err = process_create(&process, cmd, argv, NULL);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};

    // Wait with 500ms timeout should timeout
    err = process_wait(process, &result, 500);
    LOG_ASSERT(err == PROCESS_ERROR_WAIT_FAILED, "Expected wait timeout, got: %s", process_error_string(err));

    // Clean up: terminate the process
    process_terminate(process, true);
    process_free(process);
}

/**
 * @brief Test process_wait without timeout (should complete)
 */
void test_process_wait_no_timeout(void) {
    ProcessHandle* process = NULL;

    const char* cmd    = "sleep";
    const char* argv[] = {cmd, "1", NULL};  // Sleep for 1 second

    ProcessError err = process_create(&process, cmd, argv, NULL);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};

    // Wait with infinite timeout should complete successfully
    err = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));
    LOG_ASSERT(result.exited_normally, "Process did not exit normally");
    LOG_ASSERT(result.exit_code == 0, "Expected exit code 0, got: %d", result.exit_code);

    process_free(process);
}

// ============================================================================
// ENVIRONMENT VARIABLE TESTS
// ============================================================================

/**
 * @brief Test process with inherited environment
 */
void test_process_inherit_environment(void) {
    ProcessHandle* process  = NULL;
    PipeHandle* stdout_pipe = NULL;

    ProcessError err = pipe_create(&stdout_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    // Set an environment variable in the current process
    setenv("TEST_INHERITED_VAR", "inherited_value", 1);

    const char* cmd    = "sh";
    const char* argv[] = {cmd, "-c", "echo $TEST_INHERITED_VAR", NULL};

    ProcessOptions options = {
        .inherit_environment = true,
        .io.stdout_pipe      = stdout_pipe,
    };

    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));

    // Read the output
    char buffer[128]  = {0};
    size_t bytes_read = 0;
    err               = pipe_read(stdout_pipe, buffer, sizeof(buffer) - 1, &bytes_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed: %s", process_error_string(err));

    // Verify the inherited variable was present
    buffer[bytes_read] = '\0';
    LOG_ASSERT(strstr(buffer, "inherited_value") != NULL, "Expected 'inherited_value', got: %s", buffer);

    process_free(process);
    pipe_close(stdout_pipe);
    unsetenv("TEST_INHERITED_VAR");
}

/**
 * @brief Test process with custom environment
 */
void test_process_custom_environment(void) {
    ProcessHandle* process  = NULL;
    PipeHandle* stdout_pipe = NULL;

    ProcessError err = pipe_create(&stdout_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    // Create custom environment
    const char* custom_env[] = {"CUSTOM_VAR1=value1", "CUSTOM_VAR2=value2", "PATH=/usr/bin:/bin", NULL};

    // Must be absolute when using custom environment
    const char* cmd    = "/usr/bin/sh";
    const char* argv[] = {cmd, "-c", "echo $CUSTOM_VAR1:$CUSTOM_VAR2", NULL};

    ProcessOptions options = {
        .inherit_environment = false,
        .environment         = custom_env,
        .io.stdout_pipe      = stdout_pipe,
    };

    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));

    // Read the output
    char buffer[128]  = {0};
    size_t bytes_read = 0;
    err               = pipe_read(stdout_pipe, buffer, sizeof(buffer) - 1, &bytes_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed: %s", process_error_string(err));

    // Verify the custom variables were set
    buffer[bytes_read] = '\0';
    LOG_ASSERT(strstr(buffer, "value1:value2") != NULL, "Expected 'value1:value2', got: %s", buffer);

    process_free(process);
    pipe_close(stdout_pipe);
}

/**
 * @brief Test process with empty environment
 */
void test_process_empty_environment(void) {
    ProcessHandle* process = NULL;

    const char* cmd    = "/usr/bin/sh";
    const char* argv[] = {cmd, "-c", "exit 42", NULL};

    const char* empty_env[] = {NULL};

    ProcessOptions options = {
        .inherit_environment = false,
        .environment         = empty_env,
    };

    ProcessError err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));
    LOG_ASSERT(result.exit_code == 42, "Expected exit code 42, got: %d", result.exit_code);

    process_free(process);
}

// ============================================================================
// PIPE OUTPUT CAPTURE TESTS
// ============================================================================

/**
 * @brief Test capturing stdout through pipe
 */
void test_capture_stdout_through_pipe(void) {
    ProcessHandle* process  = NULL;
    PipeHandle* stdout_pipe = NULL;

    ProcessError err = pipe_create(&stdout_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    const char* cmd    = "echo";
    const char* argv[] = {cmd, "Hello from stdout", NULL};

    ProcessOptions options = {
        .inherit_environment = true,
        .io.stdout_pipe      = stdout_pipe,
    };

    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));

    // Read captured output
    char buffer[256]  = {0};
    size_t bytes_read = 0;
    err               = pipe_read(stdout_pipe, buffer, sizeof(buffer) - 1, &bytes_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed: %s", process_error_string(err));
    LOG_ASSERT(bytes_read > 0, "No data read from pipe");

    buffer[bytes_read] = '\0';
    LOG_ASSERT(strstr(buffer, "Hello from stdout") != NULL, "Expected 'Hello from stdout', got: %s", buffer);

    process_free(process);
    pipe_close(stdout_pipe);
}

/**
 * @brief Test capturing stderr through pipe
 */
void test_capture_stderr_through_pipe(void) {
    ProcessHandle* process  = NULL;
    PipeHandle* stderr_pipe = NULL;

    ProcessError err = pipe_create(&stderr_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    const char* cmd    = "sh";
    const char* argv[] = {cmd, "-c", "echo 'Error message' >&2", NULL};

    ProcessOptions options = {
        .inherit_environment = true,
        .io.stderr_pipe      = stderr_pipe,
    };

    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));

    // Read captured error output
    char buffer[256]  = {0};
    size_t bytes_read = 0;
    err               = pipe_read(stderr_pipe, buffer, sizeof(buffer) - 1, &bytes_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed: %s", process_error_string(err));
    LOG_ASSERT(bytes_read > 0, "No data read from stderr pipe");

    buffer[bytes_read] = '\0';
    LOG_ASSERT(strstr(buffer, "Error message") != NULL, "Expected 'Error message', got: %s", buffer);

    process_free(process);
    pipe_close(stderr_pipe);
}

/**
 * @brief Test capturing both stdout and stderr through separate pipes
 */
void test_capture_stdout_and_stderr_separate(void) {
    ProcessHandle* process  = NULL;
    PipeHandle *stdout_pipe = NULL, *stderr_pipe = NULL;

    ProcessError err = pipe_create(&stdout_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "stdout pipe_create failed: %s", process_error_string(err));

    err = pipe_create(&stderr_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "stderr pipe_create failed: %s", process_error_string(err));

    const char* cmd    = "sh";
    const char* argv[] = {cmd, "-c", "echo 'Standard output' && echo 'Standard error' >&2", NULL};

    ProcessOptions options = {
        .inherit_environment = true,
        .io.stdout_pipe      = stdout_pipe,
        .io.stderr_pipe      = stderr_pipe,
    };

    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));

    // Read stdout
    char stdout_buffer[256] = {0};
    size_t stdout_read      = 0;
    err                     = pipe_read(stdout_pipe, stdout_buffer, sizeof(stdout_buffer) - 1, &stdout_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "stdout pipe_read failed: %s", process_error_string(err));
    stdout_buffer[stdout_read] = '\0';

    // Read stderr
    char stderr_buffer[256] = {0};
    size_t stderr_read      = 0;
    err                     = pipe_read(stderr_pipe, stderr_buffer, sizeof(stderr_buffer) - 1, &stderr_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "stderr pipe_read failed: %s", process_error_string(err));
    stderr_buffer[stderr_read] = '\0';

    // Verify outputs
    LOG_ASSERT(strstr(stdout_buffer, "Standard output") != NULL, "Expected 'Standard output' in stdout, got: %s",
               stdout_buffer);
    LOG_ASSERT(strstr(stderr_buffer, "Standard error") != NULL, "Expected 'Standard error' in stderr, got: %s",
               stderr_buffer);

    process_free(process);
    pipe_close(stdout_pipe);
    pipe_close(stderr_pipe);
}

/**
 * @brief Test capturing stderr merged with stdout
 */
void test_capture_merged_stderr_to_stdout(void) {
    ProcessHandle* process  = NULL;
    PipeHandle* stdout_pipe = NULL;

    ProcessError err = pipe_create(&stdout_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    const char* cmd    = "sh";
    const char* argv[] = {cmd, "-c", "echo 'Out' && echo 'Err' >&2", NULL};

    ProcessOptions options = {
        .inherit_environment = true,
        .io.stdout_pipe      = stdout_pipe,
        .io.merge_stderr     = true,
    };

    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));

    // Read merged output
    char buffer[512]  = {0};
    size_t bytes_read = 0;
    err               = pipe_read(stdout_pipe, buffer, sizeof(buffer) - 1, &bytes_read, 1000);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_read failed: %s", process_error_string(err));

    buffer[bytes_read] = '\0';

    // Both stdout and stderr should be in the same buffer
    LOG_ASSERT(strstr(buffer, "Out") != NULL, "Expected 'Out' in merged output, got: %s", buffer);
    LOG_ASSERT(strstr(buffer, "Err") != NULL, "Expected 'Err' in merged output, got: %s", buffer);

    process_free(process);
    pipe_close(stdout_pipe);
}

/**
 * @brief Test capturing large output through pipe
 */
void test_capture_large_output(void) {
    ProcessHandle* process  = NULL;
    PipeHandle* stdout_pipe = NULL;

    ProcessError err = pipe_create(&stdout_pipe);
    LOG_ASSERT(err == PROCESS_SUCCESS, "pipe_create failed: %s", process_error_string(err));

    // Generate ~1KB of output
    const char* cmd    = "sh";
    const char* argv[] = {cmd, "-c", "for i in $(seq 1 50); do echo 'Line $i with some text'; done", NULL};

    ProcessOptions options = {
        .inherit_environment = true,
        .io.stdout_pipe      = stdout_pipe,
    };

    err = process_create(&process, cmd, argv, &options);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_create failed: %s", process_error_string(err));

    ProcessResult result = {};
    err                  = process_wait(process, &result, -1);
    LOG_ASSERT(err == PROCESS_SUCCESS, "process_wait failed: %s", process_error_string(err));

    // Read output in chunks
    char buffer[2048] = {0};
    size_t total_read = 0;
    size_t bytes_read = 0;

    do {
        err = pipe_read(stdout_pipe, buffer + total_read, sizeof(buffer) - total_read - 1, &bytes_read, 1000);
        if (err == PROCESS_SUCCESS) {
            total_read += bytes_read;
        }
    } while (err == PROCESS_SUCCESS && bytes_read > 0 && total_read < sizeof(buffer) - 1);

    buffer[total_read] = '\0';
    LOG_ASSERT(total_read > 500, "Expected at least 500 bytes, got: %zu", total_read);

    process_free(process);
    pipe_close(stdout_pipe);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("=== Running Extended Process Tests ===\n\n");

    printf("--- Timeout Tests ---\n");
    test_pipe_read_immediate_timeout();
    test_pipe_write_immediate_timeout();
    test_pipe_read_short_timeout();
    test_pipe_read_infinite_timeout();
    test_process_wait_with_timeout();
    test_process_wait_no_timeout();

    printf("--- Environment Variable Tests ---\n");
    test_process_inherit_environment();
    test_process_custom_environment();
    test_process_empty_environment();

    printf("--- Pipe Output Capture Tests ---\n");
    test_capture_stdout_through_pipe();
    test_capture_stderr_through_pipe();
    test_capture_stdout_and_stderr_separate();
    test_capture_merged_stderr_to_stdout();
    test_capture_large_output();

    printf("\n=== All Extended Tests Passed! ===\n");
    return 0;
}
