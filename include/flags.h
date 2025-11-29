/*
 * @file flags.h
 *
 * A robust, single-file C flag parsing library.
 *
 */
#ifndef FLAGS_H
#define FLAGS_H

#include <ctype.h>     // strcasecmp
#include <errno.h>     // errno
#include <inttypes.h>  // Printing int types
#include <limits.h>    // INT_MAX, INT8_MAX, etc.
#include <stdarg.h>    // va_list
#include <stdbool.h>   // bool
#include <stddef.h>    // size_t
#include <stdint.h>    // intN_t, uintN_t
#include <stdio.h>     // printf, snprintf
#include <stdlib.h>    // malloc, free, strtol
#include <string.h>    // strcmp, strlen

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// PUBLIC API
// =============================================================================

/** Status codes for parsing results. */
typedef enum {
    FLAG_OK = 0,
    FLAG_ERROR_ALLOCATION,          // Malloc failed
    FLAG_ERROR_UNKNOWN_FLAG,        // --unknown passed
    FLAG_ERROR_MISSING_VALUE,       // --flag [EOF]
    FLAG_ERROR_INVALID_NUMBER,      // Parsing non-number or overflow
    FLAG_ERROR_VALIDATION,          // Custom validator failed
    FLAG_ERROR_REQUIRED_MISSING,    // Mandatory flag omitted
    FLAG_ERROR_UNKNOWN_SUBCOMMAND,  // Unknown command
    FLAG_ERROR_INVALID_ARGUMENT     // Null pointers passed to API
} FlagStatus;

/** Supported data types. */
typedef enum {
    TYPE_BOOL,
    TYPE_CHAR,    // Single character
    TYPE_STRING,  // char*
    TYPE_INT8,
    TYPE_UINT8,
    TYPE_INT16,
    TYPE_UINT16,
    TYPE_INT32,
    TYPE_UINT32,
    TYPE_INT64,
    TYPE_UINT64,
    TYPE_SIZE_T,
    TYPE_FLOAT,
    TYPE_DOUBLE
} FlagDataType;

typedef struct Flag Flag;
typedef struct FlagParser FlagParser;

/**
 * Validator Callback.
 * @param value Pointer to the raw value (cast based on flag type).
 * @param error_out Set this to a static string if validation fails.
 * @return true if valid.
 */
typedef bool (*FlagValidator)(const void* value, const char** error_out);

/** Create a new parser instance. Must be freed. */
FlagParser* flag_parser_new(const char* name, const char* description);

/** Free parser and all associated resources/subcommands. */
void flag_parser_free(FlagParser* parser);

/** Set footer text displayed at bottom of help. */
void flag_parser_set_footer(FlagParser* parser, const char* footer);

/**
 * Invoke the active subcommand with optional pre-invocation setup.
 * @param parser The parser that was used in flag_parse().
 * @param pre_invoke Optional callback to run before subcommand handler. Can be NULL.
 * @param user_data User data pointer passed to both callbacks.
 * @return true if a subcommand was invoked, false if none was active.
 */
bool flag_invoke_subcommand(FlagParser* parser, void (*pre_invoke)(void* user_data), void* user_data);

/**
 * Set a pre-invocation callback that runs before any subcommand handler.
 * This is called automatically during flag_parse if a subcommand is present.
 * @param parser The main parser.
 * @param pre_invoke Callback to run before subcommand handlers.
 */
void flag_set_pre_invoke(FlagParser* parser, void (*pre_invoke)(void* user_data));

/**
 * Core function to register a flag.
 * Use the macros (flag_int, flag_bool) instead of calling this directly.
 */
Flag* flag_add(FlagParser* parser, FlagDataType type, const char* name, char short_name, const char* desc,
               void* value_ptr, bool required);

// --- Type Macros (Optional Flags) ---
#define flag_bool(p, n, s, d, v)   flag_add(p, TYPE_BOOL, n, s, d, v, false)
#define flag_char(p, n, s, d, v)   flag_add(p, TYPE_CHAR, n, s, d, v, false)
#define flag_string(p, n, s, d, v) flag_add(p, TYPE_STRING, n, s, d, v, false)
#define flag_int(p, n, s, d, v)    flag_add(p, TYPE_INT32, n, s, d, v, false)  // Default int is 32
#define flag_int8(p, n, s, d, v)   flag_add(p, TYPE_INT8, n, s, d, v, false)
#define flag_int16(p, n, s, d, v)  flag_add(p, TYPE_INT16, n, s, d, v, false)
#define flag_int32(p, n, s, d, v)  flag_add(p, TYPE_INT32, n, s, d, v, false)
#define flag_int64(p, n, s, d, v)  flag_add(p, TYPE_INT64, n, s, d, v, false)
#define flag_uint8(p, n, s, d, v)  flag_add(p, TYPE_UINT8, n, s, d, v, false)
#define flag_uint16(p, n, s, d, v) flag_add(p, TYPE_UINT16, n, s, d, v, false)
#define flag_uint32(p, n, s, d, v) flag_add(p, TYPE_UINT32, n, s, d, v, false)
#define flag_uint64(p, n, s, d, v) flag_add(p, TYPE_UINT64, n, s, d, v, false)
#define flag_size_t(p, n, s, d, v) flag_add(p, TYPE_SIZE_T, n, s, d, v, false)
#define flag_float(p, n, s, d, v)  flag_add(p, TYPE_FLOAT, n, s, d, v, false)
#define flag_double(p, n, s, d, v) flag_add(p, TYPE_DOUBLE, n, s, d, v, false)

// --- Type Macros (Required Flags) ---
// Boolean (Rarely required, but possible)
#define flag_req_bool(p, n, s, d, v) flag_add(p, TYPE_BOOL, n, s, d, v, true)

// Char & String
#define flag_req_char(p, n, s, d, v)   flag_add(p, TYPE_CHAR, n, s, d, v, true)
#define flag_req_string(p, n, s, d, v) flag_add(p, TYPE_STRING, n, s, d, v, true)

// Standard Aliases (Default to 32-bit)
#define flag_req_int(p, n, s, d, v)  flag_add(p, TYPE_INT32, n, s, d, v, true)
#define flag_req_uint(p, n, s, d, v) flag_add(p, TYPE_UINT32, n, s, d, v, true)

// Explicit Sized Integers
#define flag_req_int8(p, n, s, d, v)   flag_add(p, TYPE_INT8, n, s, d, v, true)
#define flag_req_uint8(p, n, s, d, v)  flag_add(p, TYPE_UINT8, n, s, d, v, true)
#define flag_req_int16(p, n, s, d, v)  flag_add(p, TYPE_INT16, n, s, d, v, true)
#define flag_req_uint16(p, n, s, d, v) flag_add(p, TYPE_UINT16, n, s, d, v, true)
#define flag_req_int32(p, n, s, d, v)  flag_add(p, TYPE_INT32, n, s, d, v, true)
#define flag_req_uint32(p, n, s, d, v) flag_add(p, TYPE_UINT32, n, s, d, v, true)
#define flag_req_int64(p, n, s, d, v)  flag_add(p, TYPE_INT64, n, s, d, v, true)
#define flag_req_uint64(p, n, s, d, v) flag_add(p, TYPE_UINT64, n, s, d, v, true)

// Memory & Floating Point
#define flag_req_size_t(p, n, s, d, v) flag_add(p, TYPE_SIZE_T, n, s, d, v, true)
#define flag_req_float(p, n, s, d, v)  flag_add(p, TYPE_FLOAT, n, s, d, v, true)
#define flag_req_double(p, n, s, d, v) flag_add(p, TYPE_DOUBLE, n, s, d, v, true)

/** Register a subcommand. Returns the child parser. */
FlagParser* flag_add_subcommand(FlagParser* parser, const char* name, const char* desc, void (*handler)(void* data));

/** Attach a custom validator to a specific flag. */
void flag_set_validator(Flag* flag, FlagValidator validator);

/**
 * Parse arguments.
 * Returns FLAG_OK (0) on success.
 * Use flag_get_error() for details on failure.
 */
FlagStatus flag_parse(FlagParser* parser, int argc, char** argv);

/**
 * Parse arguments and automatically invoke subcommand if present.
 * @param parser The parser instance.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param user_data Optional user data passed to pre-invoke and handler callbacks.
 * @return FLAG_OK on success.
 */
FlagStatus flag_parse_and_invoke(FlagParser* parser, int argc, char** argv, void* user_data);

/** Get readable string for the last error. */
const char* flag_get_error(FlagParser* parser);

/** Get number of positional arguments (args that aren't flags). */
int flag_positional_count(FlagParser* parser);

/** Get positional argument at index. */
const char* flag_positional_at(FlagParser* parser, int index);

/** Check if a flag was explicitly provided. */
bool flag_is_present(FlagParser* parser, const char* flag_name);

/** Print auto-generated help message. */
void flag_print_usage(FlagParser* parser);

/** Convert status enum to string. */
const char* flag_status_str(FlagStatus status);

#ifdef __cplusplus
}
#endif
#endif  // FLAGS_H
