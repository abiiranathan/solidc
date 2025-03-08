#ifndef A5C26D1F_5884_4DD7_918D_080E3F1F1A0D
#define A5C26D1F_5884_4DD7_918D_080E3F1F1A0D

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLAG_TYPES                                                                                 \
    X(BOOL, bool)                                                                                  \
    X(INT, int)                                                                                    \
    X(SIZE_T, size_t)                                                                              \
    X(INT8, int8_t)                                                                                \
    X(INT16, int16_t)                                                                              \
    X(INT32, int32_t)                                                                              \
    X(INT64, int64_t)                                                                              \
    X(UINT, unsigned int)                                                                          \
    X(UINT8, uint8_t)                                                                              \
    X(UINT16, uint16_t)                                                                            \
    X(UINT32, uint32_t)                                                                            \
    X(UINT64, uint64_t)                                                                            \
    X(FLOAT, float)                                                                                \
    X(DOUBLE, double)                                                                              \
    X(STRING, char*)

typedef enum {
#define X(name, type) FLAG_##name,
    FLAG_TYPES
#undef X
} FlagType;

typedef struct Subcommand Command;

// A flag struct. Each flag must have a corresponding value. e.g --verbose 1.
// We do not support switches like --verbose --silent
typedef struct Flag Flag;

// FlagValidator is a function pointer that receives the parsed flag value,
// buffer_size that is the capacity of the error_buffer.
// Returns false if the validation fails or true otherwise.
typedef bool (*FlagValidator)(void* value, size_t buffer_size, char* error_buffer);

// Register a new subcommand.
Command* AddCommand(const char* name, const char* description, void (*handler)(Command*));

// Register a new flag for a subcommand.
Flag* AddFlagCmd(Command* subcommand, FlagType type, const char* name, char short_name,
                 const char* description, void* value, bool required);

//  Register a new global flag.
Flag* AddFlag(FlagType type, const char* name, char short_name, const char* description,
              void* value, bool required);

// Print the usage information for the program.
void FlagUsage(const char* program_name);

// Set flag one or more flag validators.
// count is the number of validators passed as arguments.
void SetValidators(Flag* flag, size_t count, FlagValidator validators, ...);

// Parse the command line arguments and execute the matched subcommand.
// A function pointer can be passed to be excuted before the matched subcommand handler,
// passing in the user-data argument. This can NULL, in which case it is ignored.
void FlagParse(int argc, char** argv, void (*pre_exec)(void* user_data), void* user_data);

// Get the parsed flag value for a subcommand by name.
// Returns NULL if the flag is not found.
void* FlagValue(Command* subcommand, const char* name);

// Returns the global flag value by name or NULL if the flag is not found.
void* FlagValueG(const char* name);

// Generate helper functions for adding flags with concrete types
#define X(name, type)                                                                              \
    static inline Flag* AddFlag_##name(char* flag_name, char short_name, char* description,        \
                                       type* value, bool required) {                               \
        return AddFlag(FLAG_##name, flag_name, short_name, description, value, required);          \
    }                                                                                              \
    static inline Flag* AddFlagCmd_##name(Command* subcommand, char* flag_name, char short_name,   \
                                          char* description, type* value, bool required) {         \
        return AddFlagCmd(subcommand, FLAG_##name, flag_name, short_name, description, value,      \
                          required);                                                               \
    }
FLAG_TYPES
#undef X

#ifdef __cplusplus
}
#endif
#endif /* A5C26D1F_5884_4DD7_918D_080E3F1F1A0D */
