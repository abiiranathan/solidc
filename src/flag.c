#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/flag.h"
#include "../include/strton.h"

typedef struct Flag {
    char* name;         // flag identifier
    char short_name;    // short letter
    char* description;  // Description of the flag
    FlagType type;      // Type of the flag
    void* value;        // Pointer to the flag value
    bool required;      // Whether this flag must be passed

    bool provided;              //Internal flag to keep track of provided flag
    size_t num_validators;      // Number of registered validators
    FlagValidator* validators;  // Registered array of flag validation callbacks
} Flag;

typedef struct Subcommand {
    char* name;                           // Name of the subcommand
    char* description;                    // Description
    void (*handler)(struct Subcommand*);  // The handler function
    size_t num_flags;                     // Subcommand flags
    Flag* flags;                          // Array of flags registered for this subcommand
} Subcommand;

// Global flag context.
typedef struct FlagCtx {
    size_t num_subcommands;   // Number of subcommands on registered.
    Subcommand* subcommands;  // Array of subcommands registered

    size_t num_flags;  // Number of global flags.
    Flag* flags;       // Array of global flags.
} FlagCtx;

// Global context
static FlagCtx* ctx = NULL;
char* program_name = NULL;

// Initialize the global flag context.
void flag_init(void) {
    ctx = malloc(sizeof(FlagCtx));
    if (ctx == NULL) {
        perror("malloc");
        fprintf(stderr, "unable to initialize global flag context\n");
        exit(EXIT_FAILURE);
    }

    ctx->num_subcommands = 0;
    ctx->subcommands = NULL;
    ctx->num_flags = 0;
    ctx->flags = NULL;
}

// Free memory used by the global flag context.
void flag_destroy(void) {
    if (ctx == NULL) {
        return;
    }

    for (size_t i = 0; i < ctx->num_subcommands; i++) {
        Subcommand* subcommand = &ctx->subcommands[i];

        // Free subcommand flag validators if any
        for (size_t j = 0; j < subcommand->num_flags; j++) {
            Flag* flag = &subcommand->flags[j];
            if (flag->num_validators > 0 && flag->validators != NULL) {
                free(flag->validators);
            }
        }
        free(subcommand->flags);
    }
    free(ctx->subcommands);

    // Free global flag validators if any
    for (size_t i = 0; i < ctx->num_flags; i++) {
        Flag* flag = &ctx->flags[i];
        if (flag->num_validators > 0 && flag->validators != NULL) {
            free(flag->validators);
        }
    }

    free(ctx->flags);
    free(ctx);
    ctx = NULL;
}

// Returns the flag value from the subcommand or NULL if None exists.
void* flag_value(Subcommand* subcommand, const char* name) {
    for (size_t i = 0; i < subcommand->num_flags; i++) {
        if (strcmp(subcommand->flags[i].name, name) == 0) {
            return subcommand->flags[i].value;
        }
    }
    return NULL;
}

// Returns the flag value for a global flag.
void* flag_global_value(const char* name) {
    for (size_t i = 0; i < ctx->num_flags; i++) {
        if (strcmp(ctx->flags[i].name, name) == 0) {
            return ctx->flags[i].value;
        }
    }
    return NULL;
}

// Register a new subcommand.
Subcommand* flag_add_subcommand(char* name, char* description, void (*handler)(Subcommand*)) {
    ++ctx->num_subcommands;
    ctx->subcommands = realloc(ctx->subcommands, ctx->num_subcommands * sizeof(Subcommand));
    if (ctx->subcommands == NULL) {
        perror("realloc");
        fprintf(stderr, "Unable to realloc ctx->subcommands array\n");
        exit(EXIT_FAILURE);
    }

    if (handler == NULL) {
        fprintf(stderr, "subcommand handler must not be NULL\n");
        exit(EXIT_FAILURE);
    }

    Subcommand* subcommand = &ctx->subcommands[ctx->num_subcommands - 1];
    subcommand->name = name;
    subcommand->description = description;
    subcommand->handler = handler;
    subcommand->num_flags = 0;
    subcommand->flags = NULL;
    return subcommand;
}

Flag* subcommand_add_flag(Subcommand* subcommand, FlagType type, char* name, char short_name,
                          char* description, void* value, bool required) {
    ++subcommand->num_flags;
    subcommand->flags = realloc(subcommand->flags, subcommand->num_flags * sizeof(Flag));
    if (subcommand->flags == NULL) {
        perror("realloc");
        fprintf(stderr, "Unable to realloc subcommand->flags array\n");
        exit(EXIT_FAILURE);
    }

    Flag* flag = &subcommand->flags[subcommand->num_flags - 1];
    flag->type = type;
    flag->name = name;
    flag->short_name = short_name;
    flag->description = description;
    flag->value = value;
    flag->required = required;
    flag->provided = false;
    flag->num_validators = 0;
    flag->validators = NULL;
    return flag;
}

Flag* global_add_flag(FlagType type, char* name, char short_name, char* description, void* value,
                      bool required) {
    ctx->num_flags++;
    ctx->flags = realloc(ctx->flags, ctx->num_flags * sizeof(Flag));
    if (ctx->flags == NULL) {
        perror("realloc");
        fprintf(stderr, "Unable to realloc ctx->flags array\n");
        exit(EXIT_FAILURE);
    }

    Flag* flag = &ctx->flags[ctx->num_flags - 1];
    flag->type = type;
    flag->name = name;
    flag->short_name = short_name;
    flag->description = description;
    flag->value = value;
    flag->required = required;
    flag->provided = false;
    flag->num_validators = 0;
    flag->validators = NULL;
    return flag;
}

void flag_print_usage(const char* program_name) {
    printf("Usage: %s [global flags] <subcommand> [flags]\n", program_name);
    if (ctx->num_flags > 0) {
        printf("Global Flags:\n");
    }

    printf("  --%s | -%c: %s\n", "help", 'h', "Print help text and exit");
    for (size_t i = 0; i < ctx->num_flags; i++) {
        Flag* flag = &ctx->flags[i];
        printf("  --%s | -%c: %s\n", flag->name, flag->short_name, flag->description);
    }

    printf("\nSubcommands:");
    for (size_t i = 0; i < ctx->num_subcommands; i++) {
        Subcommand* subcommand = &ctx->subcommands[i];
        printf("\n  %s: %s\n", subcommand->name, subcommand->description);

        for (size_t j = 0; j < subcommand->num_flags; j++) {
            Flag* flag = &subcommand->flags[j];
            printf("    --%s | -%c: %s\n", flag->name, flag->short_name, flag->description);
        }
    }
}

__attribute__((format(printf, 1, 2))) void FATAL_ERROR(char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

// validated a flag using the registered flag validators,
// calling each callback in the order they were registered.
static void validate_flag(Flag* flag) {
    if (!flag)
        return;

    if (flag->num_validators == 0 || flag->validators == NULL) {
        return;
    }

    for (size_t i = 0; i < flag->num_validators; i++) {
        char error[512] = {0};
        if (!flag->validators[i](flag->value, 512, error)) {
            fprintf(stderr, "Validation failed for flag: --%s\n", flag->name);
            FATAL_ERROR("%s\n", error);
        }
    }
}

static Flag* find_flag(Flag* flags, size_t num_flags, const char* name, bool is_long_flag) {
    for (size_t i = 0; i < num_flags; i++) {
        if (is_long_flag) {
            if (strcmp(flags[i].name, name) == 0) {
                return &flags[i];
            }
        } else {
            if (flags[i].short_name == name[0]) {
                return &flags[i];
            }
        }
    }
    return NULL;
}

static void parse_flag_value(Flag* flag, const char* value) {
    StoError code = STO_SUCCESS;
    switch (flag->type) {
        case FLAG_STRING:
            *(char**)flag->value = (char*)value;
            break;
        case FLAG_BOOL:  // we don't expect bool but non-the less we leave it here
            *(bool*)flag->value = true;
            break;
        case FLAG_FLOAT:
            code = sto_float(value, (float*)flag->value);
            break;
        case FLAG_DOUBLE:
            code = sto_double(value, (double*)flag->value);
            break;
        case FLAG_INT:
            code = sto_int(value, (int*)flag->value);
            break;
        case FLAG_INT8:
            code = sto_int8(value, (int8_t*)flag->value);
            break;
        case FLAG_INT16:
            code = sto_int16(value, (int16_t*)flag->value);
            break;
        case FLAG_INT32:
            code = sto_int32(value, (int32_t*)flag->value);
            break;
        case FLAG_INT64:
            code = sto_int64(value, (int64_t*)flag->value);
            break;
        case FLAG_UINT:
            code = sto_uint(value, (unsigned int*)flag->value);
            break;
        case FLAG_UINT8:
            code = sto_uint8(value, (uint8_t*)flag->value);
            break;
        case FLAG_UINT16:
            code = sto_int16(value, (int16_t*)flag->value);
            break;
        case FLAG_UINT32:
            code = sto_uint32(value, (uint32_t*)flag->value);
            break;
        case FLAG_UINT64:
            code = sto_uint64(value, (uint64_t*)flag->value);
            break;

        default:
            FATAL_ERROR("Unknown flag type\n");
    }

    if (code != STO_SUCCESS) {
        FATAL_ERROR("conversion error for flag: --%s: %s", flag->name, sto_error(code));
    }
}

void process_flag(Flag* flag, const char* name, size_t i, size_t nargs, char** argv) {
    if (flag == NULL) {
        FATAL_ERROR("Unknown flag: --%s\n", name);
    }

    flag->provided = true;
    if (flag->type == FLAG_BOOL) {
        *(bool*)flag->value = true;
    } else {
        if (i + 1 >= nargs) {
            FATAL_ERROR("Missing value for flag: --%s\n", name);
        }
        parse_flag_value(flag, argv[i + 1]);
    }
}

static void process_flag_name(char* name, bool is_long_flag, Subcommand* cmd, size_t i,
                              size_t nargs, char** argv) {
    if (strlen(name) == 0) {
        FATAL_ERROR("Invalid flag name: %s\n", name);
    }

    if (strcmp(name, "help") == 0 || name[0] == 'h') {
        flag_print_usage(program_name);
        flag_destroy();
        exit(EXIT_SUCCESS);
    }

    Flag* flag = NULL;

    // We have to check if the flag is a subcommand
    if (cmd == NULL) {
        flag = find_flag(ctx->flags, ctx->num_flags, name, is_long_flag);
    } else {
        flag = find_flag(cmd->flags, cmd->num_flags, name, is_long_flag);
    }

    if (flag == NULL) {
        FATAL_ERROR("Unknown flag: --%s\n", name);
    }

    flag->provided = true;
    if (flag->type == FLAG_BOOL) {
        *(bool*)flag->value = true;
    } else {
        // check if the next argument is a value and it does not start with -/--
        // since this is not a bool flag
        if (i + 1 >= nargs) {
            FATAL_ERROR("Missing value for flag: --%s\n", name);
        }

        // Look a head to make sure the next argument is not a flag but a value
        // otherwise the current flag would have been passed as a boolean when it's not.
        {
            char* name = "";
            bool is_long_flag = strncmp(argv[i], "--", 2) == 0;
            bool is_short_flag = strncmp(argv[i], "-", 1) == 0 && !is_long_flag;

            if (is_long_flag) {
                name = argv[i] + 2;
            } else if (is_short_flag) {
                name = argv[i] + 1;
            }

            Flag* nextFlag = NULL;
            if (cmd == NULL) {
                nextFlag = find_flag(ctx->flags, ctx->num_flags, name, is_long_flag);
            } else {
                nextFlag = find_flag(cmd->flags, cmd->num_flags, name, is_long_flag);
            }

            if (nextFlag) {
                FATAL_ERROR("Missing value for non-boolean flag: --%s", flag->name);
            }
        }

        parse_flag_value(flag, argv[i]);
    }
}

typedef enum {
    START,
    FLAG_NAME,
    FLAG_SUBCOMMAND,
} ParseState;

Subcommand* flag_parse(int argc, char** argv) {
    if (argc < 2) {
        FATAL_ERROR("Missing subcommand\n");
    }

    program_name = argv[0];
    ParseState state = START;
    char* name = NULL;
    bool is_long_flag = false;
    Subcommand* current_subcommand = NULL;
    size_t i = 1;
    size_t nargs = (size_t)argc;

    while (i < nargs) {
        char* arg = argv[i];
        switch (state) {
            case START: {
                is_long_flag = strncmp(arg, "--", 2) == 0;
                bool is_short_flag = strncmp(arg, "-", 1) == 0 && !is_long_flag;

                if (is_long_flag) {
                    name = arg + 2;
                    state = FLAG_NAME;
                } else if (is_short_flag) {
                    name = arg + 1;
                    state = FLAG_NAME;
                } else {
                    if (current_subcommand != NULL) {
                        FATAL_ERROR("You can't have multiple subcommands\n");
                    }
                    state = FLAG_SUBCOMMAND;
                    i--;
                }

            } break;
            case FLAG_NAME: {
                process_flag_name(name, is_long_flag, current_subcommand, i, nargs, argv);
                state = START;
            } break;
            case FLAG_SUBCOMMAND: {
                current_subcommand = NULL;
                for (size_t j = 0; j < ctx->num_subcommands; j++) {
                    if (strcmp(ctx->subcommands[j].name, arg) == 0) {
                        current_subcommand = &ctx->subcommands[j];
                        break;
                    }
                }

                if (current_subcommand == NULL) {
                    FATAL_ERROR("Unknown subcommand: %s\n", arg);
                }
                state = START;
            } break;
            default:
                FATAL_ERROR("Invalid state\n");
        }

        // Process the last 2 arguments
        if (i + 2 == nargs) {
            for (size_t index = 0; index < 2; index++) {
                arg = argv[i + index];
                is_long_flag = strncmp(arg, "--", 2) == 0;
                bool is_short_flag = strncmp(arg, "-", 1) == 0 && !is_long_flag;

                if (is_long_flag || is_short_flag) {
                    name = arg + (is_long_flag ? 2 : 1);
                    process_flag_name(name, is_long_flag, current_subcommand, i + index, nargs,
                                      argv);
                } else {
                    // could be a subcommand with no flags
                    if (current_subcommand != NULL) {
                        FATAL_ERROR("You can't have multiple subcommands\n");
                    }

                    for (size_t j = 0; j < ctx->num_subcommands; j++) {
                        if (strcmp(ctx->subcommands[j].name, arg) == 0) {
                            current_subcommand = &ctx->subcommands[j];
                            break;
                        }
                    }
                    if (current_subcommand == NULL) {
                        FATAL_ERROR("Unknown subcommand: %s\n", arg);
                    }
                }
            }
        }

        // Move to the next argument
        i++;
    }

    // Check required flags
    for (size_t i = 0; i < ctx->num_flags; i++) {
        if (ctx->flags[i].required && !ctx->flags[i].provided) {
            FATAL_ERROR("Missing required global flag: --%s\n", ctx->flags[i].name);
        }
    }

    if (current_subcommand != NULL) {
        for (size_t i = 0; i < current_subcommand->num_flags; i++) {
            if (current_subcommand->flags[i].required && !current_subcommand->flags[i].provided) {
                FATAL_ERROR("Missing required flag for subcommand %s: --%s\n",
                            current_subcommand->name, current_subcommand->flags[i].name);
            }
        }
    }

    // Validate all flags
    for (size_t i = 0; i < ctx->num_flags; i++) {
        validate_flag(&ctx->flags[i]);
    }

    if (current_subcommand != NULL) {
        for (size_t i = 0; i < current_subcommand->num_flags; i++) {
            validate_flag(&current_subcommand->flags[i]);
        }
    }
    return current_subcommand;
}

void flag_invoke(Subcommand* subcommand) {
    subcommand->handler(subcommand);
}

void flag_set_validators(Flag* flag, size_t count, FlagValidator validator, ...) {
    if (count == 0) {
        return;
    }

    flag->num_validators = count;
    flag->validators = malloc(count * sizeof(FlagValidator));
    assert(flag->validators != NULL);

    va_list args;
    va_start(args, validator);
    for (size_t i = 0; i < count; i++) {
        flag->validators[i] = validator;
        validator = va_arg(args, FlagValidator);
    }
    va_end(args);
}
