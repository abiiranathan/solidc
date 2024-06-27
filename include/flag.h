#ifndef A5C26D1F_5884_4DD7_918D_080E3F1F1A0D
#define A5C26D1F_5884_4DD7_918D_080E3F1F1A0D

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FLAG_BOOL,    // bool
    FLAG_INT,     // int
    FLAG_SIZE_T,  // size_t
    FLAG_INT8,    // int8_t
    FLAG_INT16,   // int16_t
    FLAG_INT32,   // int32_t
    FLAG_INT64,   // int64_t
    FLAG_UINT,    // unsigned int
    FLAG_UINT8,   // uint8_t
    FLAG_UINT16,  // uint16_t
    FLAG_UINT32,  // uint32_t
    FLAG_UINT64,  // uint64_t
    FLAG_FLOAT,   // float
    FLAG_DOUBLE,  // double
    FLAG_STRING,  // char *
} FlagType;

typedef struct Subcommand Subcommand;
typedef struct Flag Flag;

// FlagValidator is a function pointer that receives the parsed flag value, and a buffer
// error of 512 bytes to write the error message if validation fails. Returns false
// if the validation fails or true otherwise.
typedef bool (*FlagValidator)(void* value, size_t size, char error[static size]);

// Initialise global flag context.
void flag_init(void);

// Free resources allocated by the global flag context.
void flag_destroy(void);

// Get the parsed flag value for a subcommand by name.
// Returns NULL if the flag is not found.
void* flag_value(Subcommand* subcommand, const char* name);

// Returns the global flag value by name or NULL if the flag is not found.
void* flag_global_value(const char* name);

// Register a new subcommand with the global flag context.
Subcommand* flag_add_subcommand(char* name, char* description, void (*handler)(Subcommand*));

// Register a new flag for a subcommand.
// Returns the created flag or NULL if the subcommand if the flag can not be added.
Flag* subcommand_add_flag(Subcommand* subcommand, FlagType type, char* name, char short_name,
                          char* description, void* value, bool required);

Flag* global_add_flag(FlagType type, char* name, char short_name, char* description, void* value,
                      bool required);

// Print the usage information for the program.
void flag_print_usage(const char* program_name);

// Set flag one or more flag validators. count is the number of validators passed.
void flag_set_validators(Flag* flag, size_t count, FlagValidator validators, ...);

// Parse the command line arguments and return the matched subcommand if any.
Subcommand* flag_parse(int argc, char** argv);

// Invoke the handler for the matched subcommand.
void flag_invoke(Subcommand* subcommand);

#endif /* A5C26D1F_5884_4DD7_918D_080E3F1F1A0D */
