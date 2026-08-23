#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>

#include "../include/flags.h"
#include "../include/thread.h"

// =============================================================================
// 1. APPLICATION CONTEXT & LOGIC (From your example)
// =============================================================================

typedef struct OpsConfig {
    bool verbose;
    bool dry_run;
    char* config_path;
    char* host;
    uint16_t port;
    size_t cache_size;
    int max_retries;
    char* db_name;
    int64_t migration_id;
    double timeout_sec;
    char mode_char;
    int8_t priority;
    float threshold;

    // Test helper to verify handler execution
    bool handler_was_called;
} OpsConfig;

// Initialize defaults
void init_config(OpsConfig* c) {
    memset(c, 0, sizeof(OpsConfig));
    c->config_path = "/default.conf";
    c->host = "localhost";
    c->port = 8080;
    c->cache_size = 1024;
    c->max_retries = 3;
    c->db_name = "prod";
    c->timeout_sec = 30.0;
    c->mode_char = 'N';
    c->priority = 10;
    c->threshold = 0.5f;
    c->handler_was_called = false;

    ASSERT(c->config_path);
    ASSERT(c->host);
    ASSERT(c->db_name);
}

// Handlers (Modified slightly to mark execution flag)
void hook_load_env(void* user_data) { (void)user_data; }

void handle_server_start(void* user_data) { ((OpsConfig*)user_data)->handler_was_called = true; }

void handle_db_migrate(void* user_data) { ((OpsConfig*)user_data)->handler_was_called = true; }

void handle_system_check(void* user_data) { ((OpsConfig*)user_data)->handler_was_called = true; }

FlagParser* build_ops_parser(OpsConfig* config) {
    FlagParser* root = flag_parser_new("ops", "Test Suite");

    flag_bool(root, "verbose", 'v', "Verbose", &config->verbose);
    flag_bool(root, "dry-run", 'd', "Dry Run", &config->dry_run);
    flag_string(root, "config", 'c', "Config", &config->config_path);
    flag_set_pre_invoke(root, hook_load_env);

    // Server
    FlagParser* cmd_server = flag_add_subcommand(root, "server", "Server", NULL);
    FlagParser* cmd_start = flag_add_subcommand(cmd_server, "start", "Start", handle_server_start);
    flag_req_string(cmd_start, "host", 'h', "Host", &config->host);
    flag_uint16(cmd_start, "port", 'p', "Port", &config->port);
    flag_size_t(cmd_start, "cache", 'z', "Cache", &config->cache_size);

    // Database
    FlagParser* cmd_db = flag_add_subcommand(root, "database", "DB", NULL);
    FlagParser* cmd_migrate = flag_add_subcommand(cmd_db, "migrate", "Migrate", handle_db_migrate);
    flag_string(cmd_migrate, "name", 'n', "DB Name", &config->db_name);
    flag_req_int64(cmd_migrate, "id", 'i', "ID", &config->migration_id);
    flag_double(cmd_migrate, "timeout", 't', "Timeout", &config->timeout_sec);

    // Check
    FlagParser* cmd_check = flag_add_subcommand(root, "check", "Check", handle_system_check);
    flag_char(cmd_check, "mode", 'm', "Mode", &config->mode_char);
    flag_int8(cmd_check, "priority", 'p', "Priority", &config->priority);
    flag_float(cmd_check, "thresh", 't', "Threshold", &config->threshold);

    return root;
}

// =============================================================================
// 2. TEST INFRASTRUCTURE
// =============================================================================

// Assertion macro that doesn't kill the whole suite, just the thread
#define TEST_ASSERT(cond, msg)                                                                              \
    do {                                                                                                    \
        if (!(cond)) {                                                                                      \
            fprintf(stderr, "\033[1;31m[FAIL] Thread %lu: %s\033[0m\n", (unsigned long)thread_self(), msg); \
            return (void*)1;                                                                                \
        }                                                                                                   \
    } while (0)

typedef struct TestPayload {
    int argc;
    char** argv;
    const char* test_name;
} TestPayload;

// =============================================================================
// 3. INDIVIDUAL TEST CASES
// =============================================================================

// Test 1: Successful Server Start
void* test_server_success(void* arg) {
    (void)arg;
    printf("Running: Server Success Test\n");

    OpsConfig c;
    init_config(&c);
    FlagParser* fp = build_ops_parser(&c);

    char* argv[] = {"ops", "server", "start", "--host=192.168.1.1", "-p", "9000", "--cache=5000"};
    int argc = 7;

    FlagStatus status = flag_parse_and_invoke(fp, argc, argv, &c);

    TEST_ASSERT(status == FLAG_OK, "Parser returned error");
    TEST_ASSERT(strcmp(c.host, "192.168.1.1") == 0, "Host mismatch");
    TEST_ASSERT(c.port == 9000, "Port mismatch");
    TEST_ASSERT(c.cache_size == 5000, "Cache size mismatch");
    TEST_ASSERT(c.handler_was_called == true, "Handler was not invoked");

    flag_parser_free(fp);
    return NULL;  // Success
}

// Test 2: Database Migration with Global Flags
void* test_db_migration(void* arg) {
    (void)arg;
    printf("Running: DB Migration Test\n");

    OpsConfig c;
    init_config(&c);
    FlagParser* fp = build_ops_parser(&c);

    // Testing global flags (-v, -d) mixed with subcommand flags
    char* argv[] = {"ops", "-v", "--dry-run", "database", "migrate", "--id", "999999999", "--timeout=0.5"};
    int argc = 8;

    FlagStatus status = flag_parse_and_invoke(fp, argc, argv, &c);

    TEST_ASSERT(status == FLAG_OK, "Parser returned error");
    TEST_ASSERT(c.verbose == true, "Verbose flag not set");
    TEST_ASSERT(c.dry_run == true, "Dry-run flag not set");
    TEST_ASSERT(c.migration_id == 999999999, "Migration ID mismatch");
    TEST_ASSERT(c.timeout_sec == 0.5, "Timeout mismatch");
    TEST_ASSERT(c.handler_was_called == true, "Handler was not invoked");

    flag_parser_free(fp);
    return NULL;
}

// Test 3: Type Edge Cases (Signed/Char/Float)
void* test_types(void* arg) {
    (void)arg;
    printf("Running: Type Checks Test\n");

    OpsConfig c;
    init_config(&c);
    FlagParser* fp = build_ops_parser(&c);

    char* argv[] = {"ops", "check", "-m", "Z", "--priority=-50", "--thresh=0.123"};
    int argc = 6;

    FlagStatus status = flag_parse_and_invoke(fp, argc, argv, &c);

    TEST_ASSERT(status == FLAG_OK, "Parser returned error");
    TEST_ASSERT(c.mode_char == 'Z', "Char parse failed");
    TEST_ASSERT(c.priority == -50, "Int8 negative parse failed");
    TEST_ASSERT(c.threshold > 0.1229 && c.threshold < 0.124, "Float parse failed");
    TEST_ASSERT(c.handler_was_called == true, "Handler was not invoked");

    flag_parser_free(fp);
    return NULL;
}

// Test 4: Missing Required Flag (Negative Test)
void* test_missing_required(void* arg) {
    (void)arg;
    printf("Running: Missing Required Flag Test\n");

    OpsConfig c;
    init_config(&c);
    FlagParser* fp = build_ops_parser(&c);

    // "host" is required for server start
    char* argv[] = {"ops", "server", "start", "-p", "80"};
    int argc = 5;

    // Redirect stderr to null to avoid spamming test output
    // (Optional, omitted here for simplicity)

    FlagStatus status = flag_parse_and_invoke(fp, argc, argv, &c);

    TEST_ASSERT(status == FLAG_ERROR_REQUIRED_MISSING, "Should have failed with REQUIRED_MISSING");
    TEST_ASSERT(c.handler_was_called == false, "Handler should NOT run on error");

    flag_parser_free(fp);
    return NULL;
}

// Test 5: Type Overflow (Negative Test)
void* test_overflow(void* arg) {
    (void)arg;
    printf("Running: Overflow Test\n");

    OpsConfig c;
    init_config(&c);
    FlagParser* fp = build_ops_parser(&c);

    // Port is uint16 (max 65535). 70000 should fail.
    char* argv[] = {"ops", "server", "start", "--host=loc", "--port=70000"};
    int argc = 5;

    FlagStatus status = flag_parse_and_invoke(fp, argc, argv, &c);

    TEST_ASSERT(status == FLAG_ERROR_INVALID_NUMBER, "Should have failed with INVALID_NUMBER");

    flag_parser_free(fp);
    return NULL;
}

// Test 6: Unknown Subcommand (Negative Test)
void* test_unknown_subcmd(void* arg) {
    (void)arg;
    printf("Running: Unknown Subcommand Test\n");

    OpsConfig c;
    init_config(&c);
    FlagParser* fp = build_ops_parser(&c);

    char* argv[] = {"ops", "database", "drop"};  // 'drop' doesn't exist
    int argc = 3;

    flag_parse_and_invoke(fp, argc, argv, &c);

    // Let's assert that the handler wasn't called.
    TEST_ASSERT(c.handler_was_called == false, "Handler should not run for unknown command");

    flag_parser_free(fp);
    return NULL;
}

// =============================================================================
// 4. TEST RUNNER
// =============================================================================

#define NUM_TESTS 6

// =============================================================================
// REGRESSION: strict bool values, negative-number values, combo flags
// =============================================================================

static void test_flag_regression_battery(void) {
    // 1. --bool=garbage must be rejected, not silently true
    {
        FlagParser* fp = flag_parser_new("t1", "");
        ASSERT(fp);
        static bool b = false;
        flag_add(fp, TYPE_BOOL, "verbose", 'v', "", &b, false);
        char* argv[] = {"t1", "--verbose=banana"};
        FlagStatus st = flag_parse(fp, 2, argv);
        assert(st != FLAG_OK && "garbage bool value must be rejected");
        flag_parser_free(fp);
    }

    // 2. Negative numbers consumable as separate value tokens (long form)
    {
        FlagParser* fp = flag_parser_new("t2", "");
        ASSERT(fp);
        static int offset = 0;
        flag_add(fp, TYPE_INT32, "offset", 'o', "", &offset, false);
        char* argv[] = {"t2", "--offset", "-5"};
        FlagStatus st = flag_parse(fp, 3, argv);
        assert(st == FLAG_OK && offset == -5);
        flag_parser_free(fp);
    }

    // 3. Same for short flags and doubles
    {
        FlagParser* fp = flag_parser_new("t3", "");
        ASSERT(fp);
        static double amt = 0;
        flag_add(fp, TYPE_DOUBLE, "amt", 'a', "", &amt, false);
        char* argv[] = {"t3", "-a", "-2.5"};
        FlagStatus st = flag_parse(fp, 3, argv);
        assert(st == FLAG_OK && amt == -2.5);
        flag_parser_free(fp);
    }

    // 4. Strict bool accepted spellings still work
    {
        FlagParser* fp = flag_parser_new("t4", "");
        ASSERT(fp);
        static bool a = false, b = true;
        flag_add(fp, TYPE_BOOL, "a", 0, "", &a, false);
        flag_add(fp, TYPE_BOOL, "b", 0, "", &b, false);
        char* argv[] = {"t4", "--a=true", "--b=0"};
        FlagStatus st = flag_parse(fp, 3, argv);
        assert(st == FLAG_OK && a == true && b == false);
        flag_parser_free(fp);
    }
}

int main(void) {
    test_flag_regression_battery();
    Thread threads[NUM_TESTS];
    void* (*tests[NUM_TESTS])(void*) = {
        test_server_success, test_db_migration, test_types, test_missing_required, test_overflow, test_unknown_subcmd,
    };

    printf("==========================================\n");
    printf("STARTING MULTI-THREADED TEST SUITE\n");
    printf("==========================================\n");

    // Launch threads
    for (int i = 0; i < NUM_TESTS; i++) {
        if (thread_create(&threads[i], tests[i], NULL) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }

    // Join threads and check results
    int failures = 0;
    for (int i = 0; i < NUM_TESTS; i++) {
        void* retval;
        thread_join(threads[i], &retval);
        if (retval != NULL) {
            failures++;
        }
    }

    printf("==========================================\n");
    if (failures == 0) {
        printf("\033[1;32mALL TESTS PASSED\033[0m\n");
        return 0;
    } else {
        printf("\033[1;31m%d TEST(S) FAILED\033[0m\n", failures);
        return 1;
    }
}
