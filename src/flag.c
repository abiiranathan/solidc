#include "../include/flag.h"
#include "../include/arena.h"
#include "../include/cstr.h"
#include "../include/strton.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef ARENA_CHUNK_SIZE
#define ARENA_CHUNK_SIZE (4096)  // 4KB

// Flag validator contains a callback function to validate a flag
// and an error message to print if validation fails
typedef struct flag_validator {
    bool (*func)(const void* value);  // optional callback to validate flag
    char* error_message;              // error message to print if validation fails
} flag_validator;

// Create a struct to hold flag information
typedef struct flag {
    char name[MAX_NAME];                // flag name. Also used for flag lookup
    void* value;                        // Value stored in the flag.
    flag_type type;                     // Flag Type enum.
    char description[MAX_DESCRIPTION];  // Flag description.
    bool required;                      // This flag must be provided
    flag_validator flag_validator;      // Optional validator for this flag.
} flag_t;

// Subcommand struct.
typedef struct subcommand {
    char name[MAX_NAME];                // name of the subcommand.
    char description[MAX_DESCRIPTION];  // usage description.

    // optional callback. Called automatically with flags, num_flags and global flag context.
    // when done parsing it's flags.
    void (*callback)(FlagArgs args);
    struct flag* flags;    // flags for this subcommand.
    size_t num_flags;      // number of flags for this subcommand.
    size_t flag_capacity;  // Maximum flags
} subcommand;

// Create a flag context to store global flags
typedef struct flag_ctx {
    flag_t flags[MAX_GLOBAL_FLAGS];  // flags in global context.
    size_t num_flags;                // Number of global flags stored.

    subcommand* subcommands[MAX_SUBCOMMANDS];  // array of pointers to subcommands
    size_t num_subcommands;                    // number of subcommands
    Arena* arena;                              // Memory allocation arena for the context.

} flag_ctx;

// Helper function to return a string representation of a flag type
static const char* flagAsString(flag_type type);

// Initialize a flag context and add global help flag.
// The help flag is added automatically to the global context.
// You can pass arguments using 3 different methods:
// -flag "value" | --flag="value" | --flag=value | --verbose(for boolean flags)
// Or evern "--port 8080" or "--port = 8080" (if quoted)
flag_ctx* flag_init(void) {
    flag_ctx* ctx = (flag_ctx*)malloc(sizeof(flag_ctx));
    FLAG_ASSERT(ctx != NULL, "Unable to allocate memory for flag context\n");

    Arena* arena = arena_create(ARENA_CHUNK_SIZE, 0);
    FLAG_ASSERT(arena != NULL, "Unable to allocate memory for arena\n");

    ctx->arena = arena;

    ctx->num_flags = 0;
    ctx->num_subcommands = 0;

    // Zero the memory for subcommands and global flags.
    memset(ctx->flags, 0, sizeof(flag_t) * MAX_GLOBAL_FLAGS);
    memset(ctx->subcommands, 0, sizeof(subcommand*) * MAX_SUBCOMMANDS);

    // Auto-append the halp flag
    flag_add_bool(ctx, .name = "help", .desc = "Print help message");
    return ctx;
}

void flag_context_destroy(flag_ctx* ctx) {
    if (ctx) {
        // Free the memory allocated for the arena
        arena_destroy(ctx->arena);
        free(ctx);
        ctx = NULL;
    }
}

flag_t* subcommand_add_flag(subcommand* subcmd, flag_type type, const char* name, const char* desc,
                            void* value, bool required) {
    FLAG_ASSERT(subcmd->num_flags < subcmd->flag_capacity,
                "[ERROR]: Not enough capacity for new flags to subcommand: %s\n", subcmd->name);
    FLAG_ASSERT(type >= FLAG_BOOL && type <= FLAG_STRING, "Invalid flag type\n");

    flag_t flag = {
        .name = {0},
        .description = {0},
        .value = value,
        .type = type,
        .required = required,
        .flag_validator = (flag_validator){0},
    };

    strncpy(flag.name, name, MAX_NAME - 1);
    flag.name[MAX_NAME - 1] = '\0';

    strncpy(flag.description, desc, MAX_DESCRIPTION - 1);
    flag.description[MAX_DESCRIPTION - 1] = '\0';

    subcmd->flags[subcmd->num_flags++] = flag;
    return &subcmd->flags[subcmd->num_flags - 1];
}

// Add a flag to the flag context
flag_t* _flag_add(flag_ctx* ctx, flag_params* params) {
    FLAG_ASSERT(MAX_GLOBAL_FLAGS > ctx->num_flags,
                "[ERROR]: Not enough capacity in global flags to add flag: %s\n", params->name);

    strncpy(ctx->flags[ctx->num_flags].name, params->name, MAX_NAME - 1);
    ctx->flags[ctx->num_flags].name[MAX_NAME - 1] = '\0';

    ctx->flags[ctx->num_flags].value = params->value;
    ctx->flags[ctx->num_flags].type = params->type;

    strncpy(ctx->flags[ctx->num_flags].description, params->desc, MAX_DESCRIPTION - 1);
    ctx->flags[ctx->num_flags].description[MAX_DESCRIPTION - 1] = '\0';

    ctx->flags[ctx->num_flags].required = params->required;
    ctx->flags[ctx->num_flags].flag_validator = (flag_validator){0};
    return &ctx->flags[ctx->num_flags++];
}

void setflag_validator(flag_ctx* ctx, flag_t* flag, bool (*validator)(const void* value),
                       const char* err_msg) {
    char* msg = arena_alloc_string(ctx->arena, err_msg);
    FLAG_ASSERT(msg != NULL, "unable to allocate err_msg in arena\n");

    flag->flag_validator = (flag_validator){.func = validator, .error_message = msg};
}

subcommand* flag_add_subcommand(flag_ctx* ctx, const char* name, const char* desc,
                                flag_handler handler, size_t capacity) {
    FLAG_ASSERT(MAX_SUBCOMMANDS > ctx->num_subcommands,
                "[ERROR]: Not enough capacity to add subcommand: %s\n", name);
    FLAG_ASSERT(handler != NULL, "No handler provided for subcommand: %s\n", name);

    subcommand* subcmd = (subcommand*)arena_alloc(ctx->arena, sizeof(subcommand));
    FLAG_ASSERT(subcmd, "[ERROR]: Unable to allocate memory for subcommand in arena");

    // Copy name to buffer
    strncpy(subcmd->name, name, MAX_NAME - 1);
    subcmd->name[MAX_NAME - 1] = '\0';

    // copy description to the buffer
    strncpy(subcmd->description, desc, MAX_DESCRIPTION - 1);
    subcmd->description[MAX_DESCRIPTION - 1] = '\0';

    subcmd->callback = handler;

    // Allocate memory for all the flags.
    subcmd->flags = (flag_t*)arena_alloc(ctx->arena, capacity * sizeof(flag_t));
    FLAG_ASSERT(subcmd->flags, "[ERROR]: Unable to allocate memory for subcommand flags");

    subcmd->num_flags = 0;
    subcmd->flag_capacity = capacity;
    ctx->subcommands[ctx->num_subcommands] = subcmd;
    ctx->num_subcommands++;
    return subcmd;
}

// Print an error message and exit
void flag_fatalf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void realdbgprintf(const char* src_file, int line_no, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s(%d): ", src_file, line_no);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

// Function to find a command by name
static subcommand* find_subcommand(subcommand** subcmds, int num_commands, const char* name) {
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(subcmds[i]->name, name) == 0) {
            return subcmds[i];
        }
    }
    return NULL;
}

void* flag_value(FlagArgs* args, const char* flag_name) {
    for (int i = 0; i < args->num_flags; i++) {
        if (strcmp(args->flags[i].name, flag_name) == 0) {
            return args->flags[i].value;
        }
    }
    return NULL;
}

void* flag_value_ctx(flag_ctx* ctx, const char* name) {
    for (size_t i = 0; i < ctx->num_flags; i++) {
        if (strcmp(ctx->flags[i].name, name) == 0) {
            return ctx->flags[i].value;
        }
    }
    return NULL;
}

// Invoke the subcommand callback.
void flag_invoke_subcommand(subcommand* subcmd, flag_ctx* ctx) {
    if (subcmd == NULL)
        return;

    subcmd->callback((FlagArgs){
        .flags = subcmd->flags,
        .num_flags = subcmd->num_flags,
        .ctx = ctx,
    });
}

typedef StoError (*converterFunc)(const char* arg, void* value);

StoError sto_string(Arena* arena, const char* arg, void* value) {
    if (arg == NULL || value == NULL) {
        return STO_INVALID;
    }

    *((char**)value) = arena_alloc_string(arena, arg);
    if (*((char**)value) == NULL) {
        return STO_INVALID;
    }
    return STO_SUCCESS;
}

// Create an array to do dynamic dispatch on flag->type.
static converterFunc typeMap[] = {
    [FLAG_BOOL] = (converterFunc)sto_bool,       [FLAG_INT] = (converterFunc)sto_int,
    [FLAG_SIZE_T] = (converterFunc)sto_ulong,    [FLAG_INT8] = (converterFunc)sto_int8,
    [FLAG_INT16] = (converterFunc)sto_int16,     [FLAG_INT32] = (converterFunc)sto_int32,
    [FLAG_INT64] = (converterFunc)sto_int64,     [FLAG_UINT] = (converterFunc)sto_uint,
    [FLAG_UINT8] = (converterFunc)sto_uint8,     [FLAG_UINT16] = (converterFunc)sto_uint16,
    [FLAG_UINT32] = (converterFunc)sto_uint32,   [FLAG_UINT64] = (converterFunc)sto_uint64,
    [FLAG_UINTPTR] = (converterFunc)sto_uintptr, [FLAG_FLOAT] = (converterFunc)sto_float,
    [FLAG_DOUBLE] = (converterFunc)sto_double,
};

static void parse_flag_helper(flag_ctx* ctx, flag_t* flags, int num_flags, int* iptr, int argc,
                              char** argv) {
    int i = (*iptr);
    // Handle both single-dash and double-dash flag notation
    char* flag_name = (argv[i][1] == '-') ? &argv[i][2] : &argv[i][1];

    // Split the flag name and value if they are separated by a space around the equal sign.
    // !!This is evil but it works for now.
    const char* delimiter = strstr(flag_name, " = ")  ? " = "
                            : strstr(flag_name, "=")  ? "="
                            : strstr(flag_name, "= ") ? "= "
                            : strstr(flag_name, " =") ? " ="
                                                      : NULL;

    // Support for flags with values separated by equal sign
    if (delimiter) {
        cstr* name = cstr_from(ctx->arena, flag_name);
        FLAG_ASSERT(name != NULL, "Unable to allocate memory for flag name\n");

        // Format: name= "value" with a space after equal sign when unquoted.
        if (cstr_ends_with(name, delimiter)) {
            // peek at next argument, if it has no - or --, then it's the value
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                cstr_trim_chars(name, delimiter);  // remove the trailing =
                flag_name = cstr_data(name);
                // Skip that space and update argv
                argv[i] = argv[i + 1];
            } else {
                flag_name[cstr_len(name) - 1] = '\0';
                flag_context_destroy(ctx);
                flag_fatalf("Missing value for flag: %s\n", flag_name);
            }
        } else {
            size_t n = 0;
            cstr** parts = cstr_split_at(ctx->arena, name, delimiter, 2, &n);
            if (n != 2) {
                flag_context_destroy(ctx);
                print_help_text(ctx, argv);
                flag_fatalf("Error: Invalid flag format: %s\n", flag_name);
            }

            // Make sure the format is name=value with no spaces
            flag_name = cstr_data(parts[0]);
            cstr_trim(parts[1]);
            if (cstr_len(parts[1]) == 0) {
                flag_fatalf("No space between flag name and value: %s\n", flag_name);
            }

            // push the value into argv, so that it can be parsed as a flag value
            // in the next iteration.
            argv[i--] = cstr_data(parts[1]);
        }
    }

    for (int j = 0; j < num_flags; ++j) {
        const char* actualName = flags[j].name;

        if (strcmp(flag_name, actualName) == 0) {
            i++;

            // Bool without a value is assumed to be true.
            if (i >= argc && flags[j].type == FLAG_BOOL) {
                *((bool*)flags[j].value) = true;
                continue;
            }

            // The FLAG_STRING type is a special case that requires a different conversion function
            // since we need to allocate memory for the string in the arena.
            StoError err = STO_INVALID;
            if (flags[j].type == FLAG_STRING) {
                err = sto_string(ctx->arena, argv[i], flags[j].value);
            } else {
                // Dynamic dispatch using the typeMap array.
                err = typeMap[flags[j].type](argv[i], flags[j].value);
            }

            if (err != STO_SUCCESS) {
                flag_fatalf("Error: Invalid value '%s' for flag %s. Expected type %s\n", argv[i],
                            actualName, flagAsString(flags[j].type));
            }

            // Validate the flag value if a validator is provided
            flag_validator validator = flags[j].flag_validator;
            if (validator.func && !validator.func(flags[j].value)) {
                flag_fatalf("ValidationError: %s\n", validator.error_message);
            }
        }
    }
}

// Parse command line arguments and set flag values
subcommand* parse_flags(flag_ctx* ctx, int argc, char* argv[]) {
    if (argc < 2) {
        return NULL;  // no subcommands or arguments
    }

    int subcmdIndex = -1;
    subcommand* subcmd = NULL;

    for (int i = 1; i < argc; i++) {
        // Parse a flag
        if (argv[i][0] == '-') {
            const char* flag_name = (argv[i][1] == '-') ? &argv[i][2] : &argv[i][1];

            if (strcmp(flag_name, "help") == 0) {
                print_help_text(ctx, argv);
                flag_context_destroy(ctx);
                exit(EXIT_SUCCESS);
            }
            parse_flag_helper(ctx, ctx->flags, ctx->num_flags, &i, argc, argv);
        } else {
            // if we have no subcommands, continue to next argument.
            if (ctx->num_subcommands == 0) {
                continue;
            }

            // Find subcommand matching current argument.
            subcmd = find_subcommand(ctx->subcommands, ctx->num_subcommands, &argv[i][0]);

            // We found a subcommand, break out of loop.
            if (subcmd) {
                subcmdIndex = i;
                break;
            }
        }
    }

    // Handle subcommand if it was found.
    if (!subcmd) {
        return NULL;
    }

    // start after subcommand and continue up to (argc -1)
    ++subcmdIndex;

    for (int i = subcmdIndex; i < argc; i++) {
        parse_flag_helper(ctx, subcmd->flags, subcmd->num_flags, &i, argc, argv);
    }
    return subcmd;
}

// Find the maximum length of a flag name in an array of flags.
static void maxFlagLengths(flag_t* flags, size_t n, size_t* name_len, size_t* type_len) {
    *name_len = 0;
    *type_len = 0;

    for (size_t i = 0; i < n; i++) {
        size_t len = strlen(flags[i].name);
        if (len > *name_len) {
            *name_len = len;
        }

        size_t typeLen = strlen(flagAsString(flags[i].type));
        if (typeLen > *type_len) {
            *type_len = typeLen;
        }
    }
}

// Print help message for available flags
void print_help_text(flag_ctx* ctx, char** argv) {
    puts(argv[0]);
    puts("Global flags:");

    size_t maxNameLen = 0;
    size_t maxTypeLength = 0;
    maxFlagLengths(ctx->flags, ctx->num_flags, &maxNameLen, &maxTypeLength);

    for (size_t i = 0; i < ctx->num_flags; i++) {
        const char* flagTypeName = flagAsString(ctx->flags[i].type);
        const char* required = ctx->flags[i].required ? "Required" : "Optional";

        // Print help message for available flags with aligned text using
        // clever *s formatting trick of printf.
        printf("  -%-*s --%s(%s) <%s>: %s\n\n", (int)maxNameLen, ctx->flags[i].name,
               ctx->flags[i].name, required, flagTypeName, ctx->flags[i].description);
    }

    for (size_t i = 0; i < ctx->num_subcommands; i++) {
        size_t subcommandMaxNameLen = 0;
        size_t subcommandMaxTypeLen = 0;
        maxFlagLengths(ctx->subcommands[i]->flags, ctx->subcommands[i]->num_flags,
                       &subcommandMaxNameLen, &subcommandMaxTypeLen);

        // Re-use maxNameLen for subcommand name length
        if (subcommandMaxNameLen > maxNameLen) {
            maxNameLen = subcommandMaxNameLen;
        }

        if (subcommandMaxTypeLen > maxTypeLength) {
            maxTypeLength = subcommandMaxTypeLen;
        }
    }

    // Print the subcommands
    {
        printf("Subcommands:\n");
        for (size_t i = 0; i < ctx->num_subcommands; i++) {
            printf("  %s: %s\n", ctx->subcommands[i]->name, ctx->subcommands[i]->description);

            for (size_t j = 0; j < ctx->subcommands[i]->num_flags; j++) {
                const char* required =
                    ctx->subcommands[i]->flags[j].required ? "Required" : "Optional";
                const char* name = ctx->subcommands[i]->flags[j].name;
                const char* desc = ctx->subcommands[i]->flags[j].description;
                const char* subcmd = ctx->subcommands[i]->flags[j].name;

                printf("    -%-*s --%s(%s) <%s>: %s\n", (int)maxNameLen, name, subcmd, required,
                       flagAsString(ctx->subcommands[i]->flags[j].type), desc);
            }
            printf("\n");
        }
        printf("\n");
    }
}

static const char* flags_array[16] = {
    [FLAG_BOOL] = "bool",         [FLAG_INT] = "int",           [FLAG_SIZE_T] = "size_t",
    [FLAG_INT8] = "int8_t",       [FLAG_INT16] = "int16_t",     [FLAG_INT32] = "int32_t",
    [FLAG_INT64] = "int64_t",     [FLAG_UINT] = "unsigned int", [FLAG_UINT8] = "uint8_t",
    [FLAG_UINT16] = "uint16_t",   [FLAG_UINT32] = "uint32_t",   [FLAG_UINT64] = "uint64_t",
    [FLAG_UINTPTR] = "uintptr_t", [FLAG_FLOAT] = "float",       [FLAG_DOUBLE] = "double",
    [FLAG_STRING] = "char *",
};

// Convert flag type to a string for printing
const char* flagAsString(flag_type type) {
    if (type < FLAG_BOOL || type > FLAG_STRING) {
        return "Unknown";
    }
    return flags_array[type];
}
