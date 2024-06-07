#include "../include/flag.h"
#include <string.h>

static int integer_flag = 0;
static size_t size_t_flag = 0;
static int8_t int8_flag = 0;
static int16_t int16_flag = 0;
static int32_t int32_flag = 0;
static int64_t int64_flag = 0;
static unsigned int uint_flag = 0;
static uint8_t uint8_flag = 0;
static uint16_t uint16_flag = 0;
static uint32_t uint32_flag = 0;
static uint64_t uint64_flag = 0;
static uintptr_t uintptr_flag = 0;
static float float32_flag = 0.0;
static double float64_flag = 0.0;
static const char* string_flag = "";
int count = 0;
bool verbose = true;
bool prompt = false;
bool tls = true;

bool validate_int(const void* value) {
    int* int_value = (int*)value;
    return (*int_value >= 0 && *int_value <= 10);
}

void printHandler(FlagArgs args) {
    int* count = flag_value(&args, "count");
    bool* verbose = flag_value(&args, "verbose");
    bool* prompt = flag_value(&args, "prompt");
    bool* tls = flag_value(&args, "tls");

    FLAG_ASSERT(count, "count should not be NULL");
    FLAG_ASSERT(verbose, "verbose should not be NULL");
    FLAG_ASSERT(prompt, "prompt should not be NULL");
    FLAG_ASSERT(tls, "tls should not be NULL");

    FLAG_ASSERT(*count == 5, "count should be 5");
    FLAG_ASSERT(*verbose, "verbose should be true");
    FLAG_ASSERT(*prompt, "prompt should be true");
    FLAG_ASSERT(!(*tls), "tls should be false");

    // ctx will provide you with access to global flags
    double float64 = *(double*)flag_value_ctx(args.ctx, "float64");
    FLAG_ASSERT(float64 == 3.14, "float64 should be 3.14");
}

#define argc (40)
static char* argv[argc] = {
    "flag_test", "--int",     "10",        "--size_t",  "100",     "--int8",   "8",     "--int16",
    "16",        "--int32",   "32",        "--int64",   "64",      "--uint",   "10",    "--uint8",
    "8",         "--uint16",  "16",        "--uint32",  "32",      "--uint64", "64",    "--uintptr",
    "64",        "--float32", "3.14",      "--float64", "3.14",    "--string", "hello", "print",
    "--count",   "5",         "--verbose", "true",      "-prompt", "yes",      "-tls",  "no",
};

int main(void) {
    flag_ctx* ctx = flag_init();

    flag_add_int(ctx, .name = "int", .desc = "An integer flag", .value = &integer_flag, );
    flag_add_size_t(ctx, .name = "size_t", .desc = "A size_t flag", .value = &size_t_flag);
    flag_add_int8(ctx, .name = "int8", .value = &int8_flag, .desc = "An int8_t flag");
    flag_add_int16(ctx, .name = "int16", .value = &int16_flag, .desc = "An int16_t flag");
    flag_add_int32(ctx, .name = "int32", .value = &int32_flag, .desc = "An int32_t flag");
    flag_add_int64(ctx, .name = "int64", .value = &int64_flag, .desc = "An int64_t flag");
    flag_add_uint(ctx, .name = "uint", .value = &uint_flag, .desc = "An unsigned int flag");
    flag_add_uint8(ctx, .name = "uint8", .value = &uint8_flag, .desc = "A uint8_t flag");
    flag_add_uint16(ctx, .name = "uint16", .value = &uint16_flag, .desc = "A uint16_t flag");
    flag_add_uint32(ctx, .name = "uint32", .value = &uint32_flag, .desc = "A uint32_t flag");
    flag_add_uint64(ctx, .name = "uint64", .value = &uint64_flag, .desc = "A uint64_t flag");
    flag_add_uintptr(ctx, .name = "uintptr", .value = &uintptr_flag, .desc = "A uintptr_t flag");
    flag_add_float(ctx, .name = "float32", .value = &float32_flag, .desc = "A float32 flag");
    flag_add_double(ctx, .name = "float64", .value = &float64_flag, .desc = "A float64 flag");
    flag_add_string(ctx, .name = "string", .value = &string_flag, .desc = "A string flag");

    // register subcommands
    subcommand* cmd = flag_add_subcommand(ctx, "print", "print hello", printHandler, 4);
    subcommand_add_flag(cmd, FLAG_BOOL, "verbose", "Verbose mode", &verbose, false);

    flag_t* fcount;
    fcount = subcommand_add_flag(cmd, FLAG_INT, "count", "Number of times to print", &count, true);
    subcommand_add_flag(cmd, FLAG_BOOL, "prompt", "Prompt for input", &prompt, false);
    subcommand_add_flag(cmd, FLAG_BOOL, "tls", "Enable tls", &tls, true);

    // add flag validator
    setflag_validator(ctx, fcount, validate_int, "count must be between 0 and 10");

    // Parse flags
    subcommand* matched_cmd = parse_flags(ctx, argc, argv);
    FLAG_ASSERT(matched_cmd, "expected a print subcommand to match, got NULL");

    FLAG_ASSERT(integer_flag == 10, "int should be 10");
    FLAG_ASSERT(int8_flag == 8, "int8 should be 8");
    FLAG_ASSERT(int16_flag == 16, "int16 should be 16");
    FLAG_ASSERT(int32_flag == 32, "int32 should be 32");
    FLAG_ASSERT(int64_flag == 64, "int64 should be 64");
    FLAG_ASSERT(uint_flag == 10, "uint should be 10");
    FLAG_ASSERT(uint8_flag == 8, "uint8 should be 8");
    FLAG_ASSERT(uint16_flag == 16, "uint16 should be 16");
    FLAG_ASSERT(uint32_flag == 32, "uint32 should be 32");
    FLAG_ASSERT(uint64_flag == 64, "uint64 should be 64");
    FLAG_ASSERT(uintptr_flag == 64, "uintptr should be 64");
    FLAG_ASSERT(float32_flag == 3.14f, "float32 should be 3.14");
    FLAG_ASSERT(float64_flag == 3.14, "float64 should be 3.14");
    FLAG_ASSERT(strcmp(string_flag, "hello") == 0, "string should be hello");

    flag_invoke_subcommand(matched_cmd, ctx);

    // Clean up allocated memory
    flag_context_destroy(ctx);
    return 0;
}