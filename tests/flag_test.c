#include "../include/flag.h"
#include "../include/cmp.h"

static int integer_flag    = 0;
static float float32_flag  = 0.0f;
static double float64_flag = 0.0;
static char* string_flag   = "";
int count                  = 0;
bool verbose               = false;
bool prompt                = false;
static char* greeting      = "";

// Validation callback
bool validate_int(void* value) {
    if (!value) {
        return false;
    }

    int num = *(int*)value;
    if (num < 0 || num > 10) {
        printf("integer must be between 1 and 10");
        return false;
    }
    return true;
}

void printHandler(Command* cmd) {
    int count_local      = FlagValue_INT(cmd, "count", 0);
    bool* verbose_local  = FlagValue(cmd, "verbose");
    bool prompt_local    = FlagValue_BOOL(cmd, "prompt", false);
    char* greeting_local = FlagValue_STRING(cmd, "greeting", "");

    FLAG_ASSERT(count_local, "count should not be NULL");
    FLAG_ASSERT(verbose_local, "verbose should not be NULL");
    FLAG_ASSERT(prompt_local, "prompt should not be NULL");

    FLAG_ASSERT(count_local == 5, "count should be 5");
    FLAG_ASSERT(*verbose_local, "verbose should be true");
    FLAG_ASSERT(prompt_local, "prompt should be true");

    FLAG_ASSERT(greeting_local, "greeting must not be NULL");
    FLAG_ASSERT(strcmp(greeting_local, "Hello World!") == 0, "greetings do not match");

    double float64 = *(double*)FlagValueG("float64");
    FLAG_ASSERT(float64 == 100.5, "float64 should be 100.5");
}

static void run_assertions(void) {
    FLAG_ASSERT(integer_flag == 10, "int should be 10");
    FLAG_ASSERT((cmp_float(float32_flag, 3.14f, (cmp_config_t){.epsilon = 1e2})), "float32 should be 3.14");
    FLAG_ASSERT(float64_flag == 100.5, "float64 should be 100.5");
    FLAG_ASSERT(verbose, "verbose should be true");
    FLAG_ASSERT(strcmp(string_flag, "hello") == 0, "string should be hello");
}

static char* argv[] = {"flag_test", "--int",    "10",    "--float32",  "3.14",        "--float64",
                       "100.5",     "--string", "hello", "print",      "--count",     "5",
                       "--verbose", "--prompt", "1",     "--greeting", "Hello World!"};

#define argc (sizeof(argv) / sizeof(argv[0]))

int main(void) {
    AddFlag_INT("int", 'i', "Integer flag", &integer_flag, true);
    AddFlag_FLOAT("float32", 'f', "Float32 flag", &float32_flag, true);
    AddFlag_STRING("string", 's', "String flag", &string_flag, true);
    AddFlag(FLAG_DOUBLE, "float64", 'd', "Float64 flag", &float64_flag, true);

    Command* printCmd;
    Flag* countFlag;

    printCmd  = AddCommand("print", "Prints a message", printHandler);
    countFlag = AddFlagCmd_INT(printCmd, "count", 'c', "No times to print", &count, true);

    AddFlagCmd(printCmd, FLAG_BOOL, "verbose", 'v', "Verbose output", &verbose, true);
    AddFlagCmd(printCmd, FLAG_BOOL, "prompt", 'p', "Prompt for input", &prompt, false);
    AddFlagCmd(printCmd, FLAG_STRING, "greeting", 'g', "Pass a greeting", &greeting, false);

    SetValidators(countFlag, 1, validate_int);

    FlagParse(argc, argv, NULL, NULL);
    run_assertions();
    return 0;
}
