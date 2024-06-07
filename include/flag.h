/**
 * @file flag.h
 * @brief Header file containing declarations for a command-line flag parsing library.
 * 
 * This file contains declarations for a command-line flag parsing library. It defines the
 * structures and functions used to create and manage flags and subcommands, parse command-line
 * arguments, and perform validation on flag values.
 * 
 * The library supports a variety of flag types, including bool, int, size_t, int8_t, int16_t,
 * int32_t, int64_t, unsigned int, uint8_t, uint16_t, uint32_t, uint64_t, uintptr_t, float,
 * double, and char *. It also allows for the addition of custom validators to validate flag
 * values.
 * 
 * @author Dr. Abiira Nathan
 */

#ifndef __FLAG_H__
#define __FLAG_H__

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
#endif

// This macro ensures fdopen strdup are available
#undef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum length of flag or subcommand name
#ifndef MAX_NAME
#define MAX_NAME 64
#endif

// Maximum length of flag or subcommand description
#ifndef MAX_DESCRIPTION
#define MAX_DESCRIPTION 256
#endif

// Maximum number of global flags
#ifndef MAX_GLOBAL_FLAGS
#define MAX_GLOBAL_FLAGS 24
#endif

// Maximum number of subcommands
#ifndef MAX_SUBCOMMANDS
#define MAX_SUBCOMMANDS 8
#endif

extern void realdbgprintf(const char* src_file, int line_no, const char* fmt, ...);

// Public API for debug printing
#define dbgprintf(...)                                                                             \
    (sizeof("" #__VA_ARGS__) > 1 ? realdbgprintf(__FILE__, __LINE__, __VA_ARGS__)                  \
                                 : realdbgprintf(__FILE__, __LINE__, ""))

#ifdef NDEBUG
#define FLAG_ASSERT(expr, ...) ((void)0)
#else
#define FLAG_ASSERT(expr, ...)                                                                     \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            realdbgprintf(__FILE__, __LINE__, __VA_ARGS__);                                        \
            assert((expr));                                                                        \
        }                                                                                          \
    } while (0)
#endif

// Supported flag types
typedef enum {
    FLAG_BOOL,     // bool
    FLAG_INT,      // int
    FLAG_SIZE_T,   // size_t
    FLAG_INT8,     // int8_t
    FLAG_INT16,    // int16_t
    FLAG_INT32,    // int32_t
    FLAG_INT64,    // int64_t
    FLAG_UINT,     // unsigned int
    FLAG_UINT8,    // uint8_t
    FLAG_UINT16,   // uint16_t
    FLAG_UINT32,   // uint32_t
    FLAG_UINT64,   // uint64_t
    FLAG_UINTPTR,  // uintptr_t
    FLAG_FLOAT,    // float
    FLAG_DOUBLE,   // double
    FLAG_STRING,   // char *
} flag_type;

// subcommand handler arguments passed to the subcommand handler.
// Get the value of a flag by name using flag_value.
// Get the value of a global flag by name using flag_value_ctx.
typedef struct FlagArgs {
    struct flag* flags;    // subcommand flags
    int num_flags;         // number of flags for subcommand
    struct flag_ctx* ctx;  // global ctx(to access other global flags)
} FlagArgs;

typedef struct flag_validator flag_validator;
typedef struct flag flag_t;
typedef struct subcommand subcommand;
typedef struct flag_ctx flag_ctx;
typedef bool (*validator)(const void* value);
typedef void (*flag_handler)(FlagArgs args);

// Parameter struct for flag creation.
typedef struct flag_params {
    flag_type type;    // Type of flag value
    const char* name;  // Name of flag. Must be unique.
    const char* desc;  // Description of the flag. Used in help message.
    void* value;       // Pointer to value
    bool required;     // Required flag?
} flag_params;

// Create a global flag context. Must be freed with DestroyFlagContext.
flag_ctx* flag_init(void);

// Free memory used by flag_ctx flags.
void flag_context_destroy(flag_ctx* ctx);

// Internal function helpers for flag and subcommand creation.
extern flag_t* _flag_add(flag_ctx* ctx, flag_params* params);
subcommand* flag_add_subcommand(flag_ctx* ctx, const char* name, const char* desc,
                                flag_handler handler, size_t capacity);

flag_t* subcommand_add_flag(subcommand* subcmd, flag_type type, const char* name, const char* desc,
                            void* value, bool required);

// Set a custom validator for a flag.
void setflag_validator(flag_ctx* ctx, flag_t* flag, bool (*validator)(const void* value),
                       const char* err_msg);

// Parse command-line arguments and return the subcommand selected or NULL if no subcommand.
// The flag context is updated with the values of the flags.
subcommand* parse_flags(flag_ctx* ctx, int argc, char* argv[]);

// Extract value of the flag by name given an array of flags.
// Return a pointer to the value or NULL if not found.
void* flag_value(FlagArgs* args, const char* flag_name);

// Get value from global flag context.
void* flag_value_ctx(flag_ctx* ctx, const char* name);

// Invoke the subcommand callback.
void flag_invoke_subcommand(subcommand* subcmd, flag_ctx* ctx);

// Print help message for flags in flag context.
// Does not exit the program automatically.
void print_help_text(flag_ctx* ctx, char** argv);

// Flag helper macros
#define flag_add_int(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_INT, __VA_ARGS__})
#define flag_add_size_t(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_SIZE_T, __VA_ARGS__})
#define flag_add_int8(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_INT8, __VA_ARGS__})
#define flag_add_int16(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_INT16, __VA_ARGS__})
#define flag_add_int32(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_INT32, __VA_ARGS__})
#define flag_add_int64(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_INT64, __VA_ARGS__})
#define flag_add_uint(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_UINT, __VA_ARGS__})
#define flag_add_uint8(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_UINT8, __VA_ARGS__})
#define flag_add_uint16(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_UINT16, __VA_ARGS__})
#define flag_add_uint32(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_UINT32, __VA_ARGS__})
#define flag_add_uint64(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_UINT64, __VA_ARGS__})
#define flag_add_uintptr(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_UINTPTR, __VA_ARGS__})
#define flag_add_float(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_FLOAT, __VA_ARGS__})
#define flag_add_double(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_DOUBLE, __VA_ARGS__})
#define flag_add_string(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_STRING, __VA_ARGS__})
#define flag_add_bool(ctx, ...) _flag_add(ctx, &(flag_params){.type = FLAG_BOOL, __VA_ARGS__})

#endif /* __FLAG_H__ */
