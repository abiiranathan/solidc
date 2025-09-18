#include <stdarg.h>

#include "../include/flag.h"
#include "../include/str_to_num.h"

typedef struct Flag {
    bool required;              // Whether this flag must be passed
    bool is_provided;           // Internal flag to keep track of provided flag
    char short_name;            // short letter
    FlagType type;              // Type of the flag
    char* name;                 // flag identifier
    char* description;          // Description of the flag
    void* value;                // Pointer to the flag value
    size_t num_validators;      // Number of registered validators
    FlagValidator* validators;  // Registered array of flag validation callbacks
} Flag;

typedef struct Command {
    char* name;                        // Name of the subcommand
    char* description;                 // Description
    void (*handler)(struct Command*);  // The handler function
    size_t num_flags;                  // Subcommand flags
    Flag flags[MAX_SUBCOMMAND_FLAGS];  // Array of flags registered for this subcommand
} Command;

// Global flag context.
typedef struct FlagCtx {
    size_t num_flags;                   // Number of global flags.
    Flag flags[MAX_GLOBAL_FLAGS];       // Array of global flags.
    size_t num_cmd;                     // Number of subcommands on registered.
    Command commands[MAX_SUBCOMMANDS];  // Array of subcommands registered
} FlagCtx;

static FlagCtx* ctx       = &(FlagCtx){0};  // Initialize global context pointer
static char* PROGRAM_NAME = NULL;           // Program name, argv[0]

// Comparison for the (cstr) vs (const char*)
#define STREQ(s1, s2)      (strcmp(s1, s2) == 0)
#define SAFE_STR(s)        ((s) ? (s) : "")
#define IS_VALID_STRING(s) ((s) != NULL && (s)[0] != '\0')

static inline bool is_valid_flag_type(FlagType type) {
    return type >= FLAG_BOOL && type <= FLAG_STRING;
}

static inline void destroy_flags(const Flag* flags, size_t n) {
    for (size_t i = 0; i < n; i++) {
        free(flags[i].name);
        free(flags[i].description);

        // free flag validators if any
        if (flags[i].validators && flags[i].num_validators > 0) {
            free(flags[i].validators);
        }
    }
}

static inline void destroy_subcommands(const Command* cmd, size_t n_subcmds) {
    for (size_t i = 0; i < n_subcmds; i++) {
        destroy_flags(cmd[i].flags, cmd[i].num_flags);
        free(ctx->commands[i].name);
        free(ctx->commands[i].description);
    }
}

// Free memory used by the global flag context.
__attribute__((destructor())) static void flag_destroy(void) {
    // free flags in global ctx
    destroy_flags(ctx->flags, ctx->num_flags);

    // free subcommand flags, name and description.
    destroy_subcommands(ctx->commands, ctx->num_cmd);
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
    FLAG_ASSERT(handler != NULL, "handler must be a non-NULL function pointer");
    FLAG_ASSERT(ctx->num_cmd < MAX_SUBCOMMANDS, "You have run out subcommands");
    FLAG_ASSERT(IS_VALID_STRING(name), "subcommand name cannot be NULL or empty");
    FLAG_ASSERT(IS_VALID_STRING(desc), "subcommand description cannot be NULL or empty");

    Command* cmd     = &ctx->commands[ctx->num_cmd++];
    cmd->name        = strdup(name);
    cmd->description = strdup(desc);
    cmd->handler     = handler;
    cmd->num_flags   = 0;
    memset(cmd->flags, 0, sizeof(cmd->flags));

    FLAG_ASSERT(cmd->name, "failed to alloc subcommand name");
    FLAG_ASSERT(cmd->description, "failed to alloc subcommand description");
    return cmd;
}

Flag* AddFlagCmd(Command* cmd, FlagType type, const char* name, char short_name, const char* desc,
                 void* value, bool required) {

    FLAG_ASSERT(cmd->num_flags < MAX_SUBCOMMAND_FLAGS, "You have run out of subcommand flags");
    FLAG_ASSERT(is_valid_flag_type(type), "invalid flag type");
    FLAG_ASSERT(IS_VALID_STRING(name), "flag name cannot be NULL or empty");
    FLAG_ASSERT(IS_VALID_STRING(desc), "flag description cannot be NULL or empty");
    FLAG_ASSERT(value != NULL, "flag value pointer cannot be NULL");

    Flag* flag           = &cmd->flags[cmd->num_flags++];
    flag->type           = type;
    flag->short_name     = short_name;
    flag->name           = strdup(name);
    flag->description    = strdup(desc);
    flag->value          = value;
    flag->required       = required;
    flag->is_provided    = false;
    flag->num_validators = 0;
    flag->validators     = NULL;

    FLAG_ASSERT(flag->name, "failed to alloc flag name");
    FLAG_ASSERT(flag->description, "failed to alloc flag description");
    return flag;
}

Flag* AddFlag(FlagType type, const char* name, char short_name, const char* description, void* value,
              bool required) {
    FLAG_ASSERT(ctx->num_flags < MAX_GLOBAL_FLAGS, "You have run out of global flags");
    FLAG_ASSERT(is_valid_flag_type(type), "invalid flag type");
    FLAG_ASSERT(IS_VALID_STRING(name), "flag name cannot be NULL or empty");
    FLAG_ASSERT(IS_VALID_STRING(description), "flag description cannot be NULL or empty");
    FLAG_ASSERT(value != NULL, "flag value pointer cannot be NULL");

    Flag* flag           = &ctx->flags[ctx->num_flags++];
    flag->type           = type;
    flag->short_name     = short_name;
    flag->name           = strdup(name);
    flag->description    = strdup(description);
    flag->value          = value;
    flag->required       = required;
    flag->is_provided    = false;
    flag->num_validators = 0;
    flag->validators     = NULL;

    FLAG_ASSERT(flag->name, "failed to alloc flag name");
    FLAG_ASSERT(flag->description, "failed to alloc flag description");

    return flag;
}

static inline void format_flag_str(char* buf, size_t buf_size, const char* name, char short_name,
                                   size_t max_name_len) {
    if (buf_size < 1) return;
    int written = snprintf(buf, buf_size, "--%-*s | -%c", (int)max_name_len, SAFE_STR(name), short_name);
    if (written < 0 || (size_t)written >= buf_size) {
        buf[buf_size - 1] = '\0';  // Ensure null-termination
    }
}

void FlagUsage(const char* program_name) {
    FLAG_ASSERT(program_name != NULL, "program name not initialized");

    // Buffer sizes
    enum { LINE_BUF_SIZE = 256 };

    // Print usage line
    printf("Usage: %s [global flags] <subcommand> [subcommand flags]\n", program_name);

    // First pass: find maximum long flag name length across ALL flags
    size_t max_long_flag_name_len = strlen("help");  // Start with help flag

    // Check global flags
    for (size_t i = 0; i < ctx->num_flags; i++) {
        const Flag* flag = &ctx->flags[i];
        if (!flag->name) continue;
        size_t len = strlen(flag->name);
        if (len > max_long_flag_name_len) {
            max_long_flag_name_len = len;
        }
    }

    // Check subcommand flags
    for (size_t i = 0; i < ctx->num_cmd; i++) {
        const Command* subcmd = &ctx->commands[i];
        for (size_t j = 0; j < subcmd->num_flags; j++) {
            const Flag* flag = &subcmd->flags[j];
            size_t len       = strlen(flag->name);
            if (len > max_long_flag_name_len) {
                max_long_flag_name_len = len;
            }
        }
    }

    // Calculate maximum length for formatted flag strings
    size_t max_global_flag_len = 0;
    char test_buf[LINE_BUF_SIZE];

    // Calculate length for help flag
    format_flag_str(test_buf, sizeof(test_buf), "help", 'h', max_long_flag_name_len);
    max_global_flag_len = strlen(test_buf);

    // Calculate lengths for other global flags
    for (size_t i = 0; i < ctx->num_flags; i++) {
        const Flag* flag = &ctx->flags[i];
        if (!flag->name) continue;

        format_flag_str(test_buf, sizeof(test_buf), flag->name, flag->short_name, max_long_flag_name_len);

        size_t len = strlen(test_buf);
        if (len > max_global_flag_len) {
            max_global_flag_len = len;
        }
    }

    // Print global flags header
    printf("\nGlobal Flags:\n");

    // Print help flag
    char help_str[LINE_BUF_SIZE];
    format_flag_str(help_str, sizeof(help_str), "help", 'h', max_long_flag_name_len);
    printf("  %-*s: %s\n", (int)max_global_flag_len, help_str, "Print help text and exit");

    // Print each global flag
    for (size_t i = 0; i < ctx->num_flags; i++) {
        const Flag* flag = &ctx->flags[i];
        if (!flag->name || !flag->description) continue;

        char flag_str[LINE_BUF_SIZE];
        format_flag_str(flag_str, sizeof(flag_str), flag->name, flag->short_name, max_long_flag_name_len);

        printf("  %-*s: %s\n", (int)max_global_flag_len, flag_str, flag->description);
    }

    // Calculate maximum subcommand name length
    size_t max_subcmd_len = 0;
    for (size_t i = 0; i < ctx->num_cmd; i++) {
        const char* name = ctx->commands[i].name;
        size_t len       = name ? strlen(name) : 0;
        if (len > max_subcmd_len) {
            max_subcmd_len = len;
        }
    }

    // Calculate maximum flag length across ALL subcommands
    size_t max_subcmd_flag_len = 0;
    for (size_t i = 0; i < ctx->num_cmd; i++) {
        const Command* subcmd = &ctx->commands[i];
        for (size_t j = 0; j < subcmd->num_flags; j++) {
            const Flag* flag = &subcmd->flags[j];
            if (!flag->name) continue;

            format_flag_str(test_buf, sizeof(test_buf), flag->name, flag->short_name, max_long_flag_name_len);
            size_t len = strlen(test_buf);
            if (len > max_subcmd_flag_len) {
                max_subcmd_flag_len = len;
            }
        }
    }

    // Print subcommands header
    printf("\nSubcommands:\n");

    // Print each subcommand
    for (size_t i = 0; i < ctx->num_cmd; i++) {
        const Command* subcmd = &ctx->commands[i];
        if (!subcmd->name || !subcmd->description) continue;

        printf("  %-*s: %s\n", (int)max_subcmd_len, subcmd->name, subcmd->description);

        if (subcmd->num_flags > 0) {
            // Print each flag for this subcommand using the global max flag
            // length
            for (size_t j = 0; j < subcmd->num_flags; j++) {
                const Flag* flag = &subcmd->flags[j];
                if (!flag->name || !flag->description) continue;

                char flag_str[LINE_BUF_SIZE];
                format_flag_str(flag_str, sizeof(flag_str), flag->name, flag->short_name,
                                max_long_flag_name_len);

                printf("    %-*s: %s\n", (int)max_subcmd_flag_len, flag_str, flag->description);
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
static inline void validate_flag(Flag* flag) {
    if (flag->num_validators == 0 || flag->validators == NULL) {
        return;
    }

    bool is_valid;

    for (size_t i = 0; i < flag->num_validators; i++) {
        // Invoke validation callback with parsed value
        is_valid = flag->validators[i](flag->value);

        if (!is_valid) {
            FATAL_ERROR("Validation failed for flag: --%s\n", flag->name);
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
    FLAG_ASSERT(flag != NULL, "flag cannot be NULL");
    FLAG_ASSERT(flag->value != NULL, "flag value pointer cannot be NULL");
    FLAG_ASSERT(value != NULL, "value argument cannot be NULL");

    StoError code = STO_SUCCESS;
    switch (flag->type) {
        case FLAG_STRING:
            *(char**)flag->value = (char*)value;
            break;
        case FLAG_BOOL:
            if (value == NULL || strlen(value) == 1 || strncmp(value, "-", 1) == 0) {
                *(bool*)flag->value = true;
            } else {
                code = str_to_bool(value, (bool*)flag->value);
            }
            break;
        case FLAG_FLOAT:
            code = str_to_float(value, (float*)flag->value);
            break;
        case FLAG_DOUBLE:
            code = str_to_double(value, (double*)flag->value);
            break;
        case FLAG_INT:
            code = str_to_int(value, (int*)flag->value);
            break;
        case FLAG_INT8:
            code = str_to_i8(value, (int8_t*)flag->value);
            break;
        case FLAG_INT16:
            code = str_to_i16(value, (int16_t*)flag->value);
            break;
        case FLAG_INT32:
            code = str_to_i32(value, (int32_t*)flag->value);
            break;
        case FLAG_INT64:
            code = str_to_i64(value, (int64_t*)flag->value);
            break;
        case FLAG_UINT:
            code = str_to_uint(value, (unsigned int*)flag->value);
            break;
        case FLAG_UINT8:
            code = str_to_u8(value, (uint8_t*)flag->value);
            break;
        case FLAG_UINT16:
            code = str_to_u16(value, (uint16_t*)flag->value);
            break;
        case FLAG_UINT32:
            code = str_to_u32(value, (uint32_t*)flag->value);
            break;
        case FLAG_UINT64:
            code = str_to_u64(value, (uint64_t*)flag->value);
            break;

        default:
            FATAL_ERROR("Unknown flag type\n");
    }

    if (code != STO_SUCCESS) {
        FATAL_ERROR("conversion error for flag: --%s: %s(%s)\n", flag->name, sto_error_string(code), value);
    }
}

static void process_flag_name(char* name, bool is_long_flag, Command* cmd, size_t* i, char** argv,
                              size_t argc) {
    if (name == NULL || *name == '\0') {
        FATAL_ERROR("Invalid flag name: %s\n", name);
    }

    // Handle help command.
    if (strcmp(name, "help") == 0 || name[0] == 'h') {
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
            FATAL_ERROR("Flag --%s requires a value\n", flag->name);
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
    if (argc <= 0 || !argv || !argv[0]) {
        FATAL_ERROR("Invalid arguments to FlagParse\n");
    }

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
                for (size_t j = 0; j < ctx->num_cmd; j++) {
                    if (STREQ(ctx->commands[j].name, arg)) {
                        subcmd = &ctx->commands[j];
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
            FATAL_ERROR("Missing required global flag: --%s\n", ctx->flags[i].name);
        }
    }

    if (subcmd != NULL) {
        for (size_t i = 0; i < subcmd->num_flags; i++) {
            if (subcmd->flags[i].required && !subcmd->flags[i].is_provided) {
                FATAL_ERROR("Missing required flag for subcommand %s: --%s\n", subcmd->name,
                            subcmd->flags[i].name);
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
    if (!subcommand || !subcommand->handler) return;
    subcommand->handler(subcommand);
}

void SetValidators(Flag* flag, size_t count, FlagValidator validator, ...) {
    if (!flag || count == 0 || !validator) {
        return;
    }

    flag->num_validators = count;
    flag->validators     = malloc(count * sizeof(FlagValidator));
    FLAG_ASSERT(flag->validators != NULL, "unable to allocate flag validators");

    va_list args;
    va_start(args, validator);
    for (size_t i = 0; i < count; i++) {
        flag->validators[i] = validator;
        validator           = va_arg(args, FlagValidator);
    }
    va_end(args);
}
