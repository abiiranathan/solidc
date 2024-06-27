#include "../include/flag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int integer_flag = 0;
static float float32_flag = 0.0;
static double float64_flag = 0.0;
static char* string_flag = "";

int count = 0;
bool verbose = true;
bool prompt = false;

#define FLAG_ASSERT(cond, msg)                                                                     \
    if (!(cond)) {                                                                                 \
        fprintf(stderr, "Assertion failed: %s\n", msg);                                            \
        exit(1);                                                                                   \
    }

bool validate_int(void* value, size_t size, char* error) {
    int num = *(int*)value;
    if (num < 0 || num > 10) {
        strncpy(error, "integer must be between 1 and 10", size);
        return false;
    }
    return true;
}

void printHandler(Subcommand* cmd) {
    int* count = flag_value(cmd, "count");
    bool* verbose = flag_value(cmd, "verbose");
    bool* prompt = flag_value(cmd, "prompt");

    FLAG_ASSERT(count, "count should not be NULL");
    FLAG_ASSERT(verbose, "verbose should not be NULL");
    FLAG_ASSERT(prompt, "prompt should not be NULL");

    FLAG_ASSERT(*count == 5, "count should be 5");
    FLAG_ASSERT(*verbose, "verbose should be true");
    FLAG_ASSERT(*prompt, "prompt should be true");

    // ctx will provide you with access to global flags
    double float64 = *(double*)flag_global_value("float64");
    FLAG_ASSERT(float64 == 100.5, "float64 should be 100.5");
}

static char* argv[] = {
    "flag_test", "--int", "10",    "--float32", "3.14", "--float64", "100.5",
    "--string",  "hello", "print", "--count",   "5",    "--verbose", "--prompt",

};

#define argc (sizeof(argv) / sizeof(argv[0]))

int main(void) {
    flag_init();

    // register global flags
    global_add_flag(FLAG_INT, "int", 'i', "Integer flag", &integer_flag, true);
    global_add_flag(FLAG_FLOAT, "float32", 'f', "Float32 flag", &float32_flag, true);
    global_add_flag(FLAG_STRING, "string", 's', "String flag", &string_flag, true);
    global_add_flag(FLAG_DOUBLE, "float64", 'd', "Float64 flag", &float64_flag, true);

    // register subcommands
    Subcommand* print_cmd = flag_add_subcommand("print", "Prints a message", printHandler);
    Flag* cflag = subcommand_add_flag(print_cmd, FLAG_INT, "count", 'c', "Number of times to print",
                                      &count, true);
    subcommand_add_flag(print_cmd, FLAG_BOOL, "verbose", 'v', "Verbose output", &verbose, true);
    subcommand_add_flag(print_cmd, FLAG_BOOL, "prompt", 'p', "Prompt for input", &prompt, true);

    // add flag validator
    flag_set_validators(cflag, 1, validate_int);

    // Parse flags
    Subcommand* matched_cmd = flag_parse(argc, argv);
    FLAG_ASSERT(matched_cmd, "expected a print subcommand to match, got NULL");

    FLAG_ASSERT(integer_flag == 10, "int should be 10");
    FLAG_ASSERT(float32_flag == 3.14f, "float32 should be 3.14");

    FLAG_ASSERT(float64_flag == 100.5, "float64 should be 100.5");
    FLAG_ASSERT(verbose, "verbose should be true");
    FLAG_ASSERT(strcmp(string_flag, "hello") == 0, "string should be hello");

    flag_invoke(matched_cmd);

    // Clean up allocated memory
    flag_destroy();

    return 0;
}
