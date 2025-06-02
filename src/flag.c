#include "../include/flag.h"
#include <stdarg.h>
#include "../include/cstr.h"
#include "../include/strton.h"

#define LOG_ASSERT(cond, msg)                                                                                          \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, msg);                                                    \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

typedef struct Flag {
    bool required;              // Whether this flag must be passed
    bool is_provided;           // Internal flag to keep track of provided flag
    char short_name;            // short letter
    FlagType type;              // Type of the flag
    cstr* name;                 // flag identifier
    cstr* description;          // Description of the flag
    void* value;                // Pointer to the flag value
    size_t num_validators;      // Number of registered validators
    FlagValidator* validators;  // Registered array of flag validation callbacks
} Flag;

typedef struct Subcommand {
    cstr* name;                           // Name of the subcommand
    cstr* description;                    // Description
    void (*handler)(struct Subcommand*);  // The handler function
    size_t num_flags;                     // Subcommand flags
    Flag flags[MAX_SUBCOMMAND_FLAGS];     // Array of flags registered for this subcommand
} Command;

// Global flag context.
typedef struct FlagCtx {
    size_t num_subcommands;                // Number of subcommands on registered.
    Command subcommands[MAX_SUBCOMMANDS];  // Array of subcommands registered
    size_t num_flags;                      // Number of global flags.
    Flag flags[MAX_GLOBAL_FLAGS];          // Array of global flags.
} FlagCtx;

static FlagCtx* ctx = &(FlagCtx){0};  // Initialize global context

static char* PROGRAM_NAME = NULL;  // Program name, argv[0]

#define STREQ(s1, s2) (strcmp(cstr_data_const(s1), s2) == 0)

static inline void destroy_flags(const Flag* flags, size_t n) {
    for (size_t i = 0; i < n; i++) {
        cstr_free(flags[i].name);
        cstr_free(flags[i].description);

        // free flag validators if any
        if (flags[i].validators && flags[i].num_validators > 0) {
            free(flags[i].validators);
        }
    }
}

static inline void destroy_subcommands(const Command* cmd, size_t n_subcmds) {
    for (size_t i = 0; i < n_subcmds; i++) {
        destroy_flags(cmd[i].flags, cmd[i].num_flags);
        cstr_free(ctx->subcommands[i].name);
        cstr_free(ctx->subcommands[i].description);
    }
}

// Free memory used by the global flag context.
__attribute__((destructor())) static void flag_destroy(void) {
    // free flags in global ctx
    destroy_flags(ctx->flags, ctx->num_flags);

    // free subcommand flags
    destroy_subcommands(ctx->subcommands, ctx->num_subcommands);
}

// Returns the flag value from the subcommand or NULL if None exists.
void* FlagValue(Command* subcommand, const char* name) {
    for (size_t i = 0; i < subcommand->num_flags; i++) {
        if (STREQ(subcommand->flags[i].name, name)) {
            return subcommand->flags[i].value;
        }
    }
    return NULL;
}

// Returns the flag value for a global flag.
void* FlagValueG(const char* name) {
    for (size_t i = 0; i < ctx->num_flags; i++) {
        if (STREQ(ctx->flags[i].name, name)) {
            return ctx->flags[i].value;
        }
    }
    return NULL;
}

// Register a new subcommand.
Command* AddCommand(const char* name, const char* desc, void (*handler)(Command*)) {
    LOG_ASSERT(handler, "handler must a valid function pointer");
    LOG_ASSERT(ctx->num_subcommands < MAX_SUBCOMMANDS, "You have run out subcommands");

    Command* cmd     = &ctx->subcommands[ctx->num_subcommands++];
    cmd->name        = cstr_new(name);
    cmd->description = cstr_new(desc);
    cmd->handler     = handler;
    cmd->num_flags   = 0;
    memset(cmd->flags, 0, sizeof(cmd->flags));

    LOG_ASSERT(cmd->name, "failed to alloc subcommand name");
    LOG_ASSERT(cmd->description, "failed to alloc subcommand description");
    return cmd;
}

Flag* AddFlagCmd(Command* cmd, FlagType type, const char* name, char short_name, const char* desc, void* value,
                 bool required) {

    LOG_ASSERT(cmd->num_flags < MAX_SUBCOMMAND_FLAGS, "You have run out of subcommand flags");
    Flag* flag           = &cmd->flags[cmd->num_flags++];
    flag->type           = type;
    flag->short_name     = short_name;
    flag->name           = cstr_new(name);
    flag->description    = cstr_new(desc);
    flag->value          = value;
    flag->required       = required;
    flag->is_provided    = false;
    flag->num_validators = 0;
    flag->validators     = NULL;

    LOG_ASSERT(flag->name, "failed to alloc flag name");
    LOG_ASSERT(flag->description, "failed to alloc flag description");
    return flag;
}

Flag* AddFlag(FlagType type, const char* name, char short_name, const char* description, void* value, bool required) {
    LOG_ASSERT(ctx->num_flags < MAX_GLOBAL_FLAGS, "You have run out of global flags");
    Flag* flag           = &ctx->flags[ctx->num_flags++];
    flag->type           = type;
    flag->short_name     = short_name;
    flag->name           = cstr_new(name);
    flag->description    = cstr_new(description);
    flag->value          = value;
    flag->required       = required;
    flag->is_provided    = false;
    flag->num_validators = 0;
    flag->validators     = NULL;

    LOG_ASSERT(flag->name, "failed to alloc flag name");
    LOG_ASSERT(flag->description, "failed to alloc flag description");

    return flag;
}

void FlagUsage(const char* program_name) {
    printf("Usage: %s [global flags] <subcommand> [flags]\n", program_name);

    // Calculate maximum length for global flags, including "--help | -h"
    size_t max_global_flag_len = 7 + strlen("help");  // "--help | -h"
    for (size_t i = 0; i < ctx->num_flags; i++) {
        size_t len = 7 + cstr_len(ctx->flags[i].name);  // "--%s | -%c"
        if (len > max_global_flag_len) {
            max_global_flag_len = len;
        }
    }

    // Print global flags, including the help flag
    printf("Global Flags:\n");
    char help_str[256];
    snprintf(help_str, sizeof(help_str), "--%s | -%c", "help", 'h');
    printf("  %-*s: %s\n", (int)max_global_flag_len, help_str, "Print help text and exit");
    for (size_t i = 0; i < ctx->num_flags; i++) {
        Flag* flag = &ctx->flags[i];
        char flag_str[256];
        snprintf(flag_str, sizeof(flag_str), "--%s | -%c", cstr_data_const(flag->name), flag->short_name);
        printf("  %-*s: %s\n", (int)max_global_flag_len, flag_str, cstr_data_const(flag->description));
    }

    // Calculate maximum subcommand name length
    size_t max_subcmd_len = 0;
    for (size_t i = 0; i < ctx->num_subcommands; i++) {
        size_t len = cstr_len(ctx->subcommands[i].name);
        if (len > max_subcmd_len) {
            max_subcmd_len = len;
        }
    }

    // Print subcommands
    printf("\nSubcommands:\n");
    for (size_t i = 0; i < ctx->num_subcommands; i++) {
        Command* subcmd = &ctx->subcommands[i];
        printf(
            "  %-*s: %s\n", (int)max_subcmd_len, cstr_data_const(subcmd->name), cstr_data_const(subcmd->description));

        if (subcmd->num_flags > 0) {
            // Calculate maximum flag length for this subcommand
            size_t max_flag_len = 0;
            for (size_t j = 0; j < subcmd->num_flags; j++) {
                size_t len = 7 + cstr_len(subcmd->flags[j].name);  // "--%s | -%c"
                if (len > max_flag_len) {
                    max_flag_len = len;
                }
            }

            // Print flags for this subcommand
            for (size_t j = 0; j < subcmd->num_flags; j++) {
                Flag* flag = &subcmd->flags[j];
                char flag_str[256];
                snprintf(flag_str, sizeof(flag_str), "--%s | -%c", cstr_data_const(flag->name), flag->short_name);
                printf("    %-*s: %s\n", (int)max_flag_len, flag_str, cstr_data_const(flag->description));
            }
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
    if (!flag || flag->num_validators == 0 || flag->validators == NULL) {
        return;
    }

    for (size_t i = 0; i < flag->num_validators; i++) {
        char error[512] = {0};
        if (!flag->validators[i](flag->value, sizeof(error) - 1, error)) {
            fprintf(stderr, "Validation failed for flag: --%s\n", cstr_data_const(flag->name));
            FATAL_ERROR("%s\n", error);
        }
    }
}

static Flag* find_flag(Flag* flags, size_t num_flags, const char* name, bool is_long_flag) {
    for (size_t i = 0; i < num_flags; i++) {
        if (is_long_flag) {
            if (STREQ(flags[i].name, name)) {
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
    if (!flag || !flag->value || !value) {
        FATAL_ERROR("Invalid flag or value\n");
    }

    StoError code = STO_SUCCESS;
    switch (flag->type) {
        case FLAG_STRING:
            *(char**)flag->value = (char*)value;
            break;
        case FLAG_BOOL:
            if (value == NULL || strlen(value) == 1 || strncmp(value, "-", 1) == 0) {
                *(bool*)flag->value = true;
            } else {
                code = sto_bool(value, (bool*)flag->value);
            }
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
        FATAL_ERROR("conversion error for flag: --%s: %s(%s)\n", cstr_data_const(flag->name), sto_error(code), value);
    }
}

void process_flag(Flag* flag, const char* name, size_t i, char** argv) {
    if (flag == NULL) {
        FATAL_ERROR("Unknown flag: --%s\n", name);
    }

    parse_flag_value(flag, argv[i + 1]);
    flag->is_provided = true;
}

static void process_flag_name(char* name, bool is_long_flag, Command* cmd, size_t* i, char** argv, size_t argc) {
    if (name == NULL || strlen(name) == 0) {
        FATAL_ERROR("Invalid flag name: %s\n", name);
    }

    // Handle help command.
    if (strcmp(name, "help") == 0 || name[0] == 'h') {
        LOG_ASSERT(PROGRAM_NAME != NULL, "program name not initialized");
        FlagUsage(PROGRAM_NAME);
        exit(EXIT_SUCCESS);
    }

    Flag* flag = NULL;
    if (cmd == NULL) {
        flag = find_flag(ctx->flags, ctx->num_flags, name, is_long_flag);
    } else {
        flag = find_flag(cmd->flags, cmd->num_flags, name, is_long_flag);
    }

    if (flag == NULL) {
        FATAL_ERROR("Unknown flag: --%s\n", name);
    }

    // Check if there's a next argument
    if (*i < argc && strncmp(argv[*i], "-", 1) != 0) {
        // Next argument exists and doesn’t start with "-", so it’s the value
        parse_flag_value(flag, argv[*i]);
        (*i)++;  // Consume the value
    } else {
        // No value provided
        if (flag->type == FLAG_BOOL) {
            *(bool*)flag->value = true;  // Set boolean to true if no value
        } else {
            FATAL_ERROR("Flag --%s requires a value\n", cstr_data_const(flag->name));
        }
    }

    flag->is_provided = true;
}

typedef enum {
    START,
    FLAG_NAME,
    FLAG_SUBCOMMAND,
} ParseState;

void FlagParse(int argc, char** argv, void (*pre_exec)(void* user_data), void* user_data) {
    PROGRAM_NAME      = argv[0];
    ParseState state  = START;
    char* name        = NULL;
    bool is_long_flag = false;
    Command* subcmd   = NULL;

    if (argc < 2) {
        goto validation;
    }

    for (size_t i = 1; i < (size_t)argc;) {
        char* arg = argv[i];

        switch (state) {
            case START: {
                is_long_flag       = strncmp(arg, "--", 2) == 0;
                bool is_short_flag = strncmp(arg, "-", 1) == 0 && !is_long_flag;
                if (is_long_flag) {
                    name  = arg + 2;
                    state = FLAG_NAME;
                    i++;
                } else if (is_short_flag) {
                    name  = arg + 1;
                    state = FLAG_NAME;
                    i++;
                } else {
                    if (subcmd != NULL) {
                        FATAL_ERROR("You can't have multiple subcommands\n");
                    }
                    state = FLAG_SUBCOMMAND;
                    break;
                }

                // Process flag immediately without changing state, otherwise
                // for loop will exit be4 last argument is processed!
                if (state == FLAG_NAME) {
                    process_flag_name(name, is_long_flag, subcmd, &i, argv, (size_t)argc);
                    state = START;
                }
            } break;
            case FLAG_SUBCOMMAND: {
                for (size_t j = 0; j < ctx->num_subcommands; j++) {
                    if (STREQ(ctx->subcommands[j].name, arg)) {
                        subcmd = &ctx->subcommands[j];
                        break;
                    }
                }

                if (subcmd == NULL) {
                    FATAL_ERROR("Unknown subcommand: %s\n", arg);
                }
                state = START;
                i++;
            } break;
            default:
                FATAL_ERROR("Invalid state\n");
        }
    }

validation:
    // Check required flags
    for (size_t i = 0; i < ctx->num_flags; i++) {
        if (ctx->flags[i].required && !ctx->flags[i].is_provided) {
            FATAL_ERROR("Missing required global flag: --%s\n", cstr_data_const(ctx->flags[i].name));
        }
    }

    if (subcmd != NULL) {
        for (size_t i = 0; i < subcmd->num_flags; i++) {
            if (subcmd->flags[i].required && !subcmd->flags[i].is_provided) {
                FATAL_ERROR("Missing required flag for subcommand %s: --%s\n",
                            cstr_data_const(subcmd->name),
                            cstr_data_const(subcmd->flags[i].name));
            }
        }
    }

    // Validate all flags
    for (size_t i = 0; i < ctx->num_flags; i++) {
        validate_flag(&ctx->flags[i]);
    }

    if (subcmd != NULL) {
        for (size_t i = 0; i < subcmd->num_flags; i++) {
            validate_flag(&subcmd->flags[i]);
        }
    }

    if (pre_exec != NULL) {
        pre_exec(user_data);
    }

    if (subcmd != NULL) {
        subcmd->handler(subcmd);
    }
}

void FlagInvoke(Command* subcommand) {
    subcommand->handler(subcommand);
}

void SetValidators(Flag* flag, size_t count, FlagValidator validator, ...) {
    if (count == 0) {
        return;
    }

    flag->num_validators = count;
    flag->validators     = malloc(count * sizeof(FlagValidator));
    LOG_ASSERT(flag->validators != NULL, "unable to allocate flag validators");

    va_list args;
    va_start(args, validator);
    for (size_t i = 0; i < count; i++) {
        flag->validators[i] = validator;
        validator           = va_arg(args, FlagValidator);
    }
    va_end(args);
}
