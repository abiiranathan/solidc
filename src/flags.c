#include "../include/flags.h"

#define INITIAL_CAPACITY  8
#define ERR_BUF_SIZE      128
#define MAX_FLAG_NAME_LEN 128

struct Flag {
    FlagDataType type;
    char* name;
    char short_name;
    char* description;
    void* value_ptr;
    bool required;
    bool is_present;
    FlagValidator validator;
};

struct FlagParser {
    char* name;
    char* description;
    char* footer;

    Flag* flags;
    size_t flag_count;
    size_t flag_capacity;

    FlagParser** subcommands;
    size_t cmd_count;
    size_t cmd_capacity;
    void (*handler)(void* user_data);
    void (*pre_invoke)(void* user_data);

    char** positional_args;
    size_t pos_count;
    size_t pos_capacity;

    char last_error[ERR_BUF_SIZE];
    FlagParser* active_subcommand;
};

// --- Memory Helpers ---

static void* xrealloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "Fatal: Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

static char* xstrdup(const char* s) {
    if (!s) return NULL;
    char* new_s = strdup(s);
    if (!new_s) {
        fprintf(stderr, "Fatal: Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return new_s;
}

// --- Lifecycle ---

FlagParser* flag_parser_new(const char* name, const char* description) {
    FlagParser* fp = (FlagParser*)calloc(1, sizeof(FlagParser));
    if (!fp) return NULL;
    fp->name        = xstrdup(name);
    fp->description = xstrdup(description);
    return fp;
}

void flag_parser_free(FlagParser* fp) {
    if (!fp) return;
    free(fp->name);
    free(fp->description);
    free(fp->footer);
    for (size_t i = 0; i < fp->flag_count; i++) {
        free(fp->flags[i].name);
        free(fp->flags[i].description);
    }
    free(fp->flags);
    for (size_t i = 0; i < fp->cmd_count; i++) {
        flag_parser_free(fp->subcommands[i]);
    }
    free(fp->subcommands);
    free(fp->positional_args);
    free(fp);
}

void flag_parser_set_footer(FlagParser* parser, const char* footer) {
    if (!parser) return;
    free(parser->footer);
    parser->footer = xstrdup(footer);
}

void flag_set_pre_invoke(FlagParser* fp, void (*pre_invoke)(void* user_data)) {
    if (fp) {
        fp->pre_invoke = pre_invoke;
    }
}

// --- Registration ---

Flag* flag_add(FlagParser* fp, FlagDataType type, const char* name, char short_name, const char* desc, void* value_ptr,
               bool required) {
    if (!fp || !name || !value_ptr) return NULL;

    if (fp->flag_count >= fp->flag_capacity) {
        size_t new_cap    = (fp->flag_capacity == 0) ? INITIAL_CAPACITY : fp->flag_capacity * 2;
        fp->flags         = (Flag*)xrealloc(fp->flags, new_cap * sizeof(Flag));
        fp->flag_capacity = new_cap;
    }

    Flag* f        = &fp->flags[fp->flag_count++];
    f->type        = type;
    f->name        = xstrdup(name);
    f->short_name  = short_name;
    f->description = xstrdup(desc);
    f->value_ptr   = value_ptr;
    f->required    = required;
    f->is_present  = false;
    f->validator   = NULL;
    return f;
}

FlagParser* flag_add_subcommand(FlagParser* fp, const char* name, const char* desc, void (*handler)(void* data)) {
    if (!fp || !name) return NULL;
    if (fp->cmd_count >= fp->cmd_capacity) {
        size_t new_cap   = (fp->cmd_capacity == 0) ? INITIAL_CAPACITY : fp->cmd_capacity * 2;
        fp->subcommands  = (FlagParser**)xrealloc(fp->subcommands, new_cap * sizeof(FlagParser*));
        fp->cmd_capacity = new_cap;
    }
    FlagParser* sub                  = flag_parser_new(name, desc);
    sub->handler                     = handler;
    sub->pre_invoke                  = NULL;
    fp->subcommands[fp->cmd_count++] = sub;
    return sub;
}

// --- Subcommand Invocation ---
bool flag_invoke_subcommand(FlagParser* fp, void (*pre_invoke)(void* user_data), void* user_data) {
    if (!fp || !fp->active_subcommand) {
        return false;
    }

    FlagParser* sub = fp->active_subcommand;

    // Run pre-invocation callback if provided
    if (pre_invoke) {
        pre_invoke(user_data);
    }

    // Invoke the subcommand handler if it exists
    if (sub->handler) {
        sub->handler(user_data);
    }

    return true;
}

void flag_set_validator(Flag* flag, FlagValidator validator) {
    if (flag) flag->validator = validator;
}

// --- Parsing Internals ---

static void set_error(FlagParser* fp, const char* fmt, ...) {
    if (!fp) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(fp->last_error, ERR_BUF_SIZE, fmt, args);
    va_end(args);
}

static Flag* find_flag_long(FlagParser* fp, const char* name) {
    if (!fp || !name) return NULL;
    for (size_t i = 0; i < fp->flag_count; i++) {
        if (strcmp(fp->flags[i].name, name) == 0) return &fp->flags[i];
    }
    return NULL;
}

static Flag* find_flag_short(FlagParser* fp, char c) {
    if (!fp || !c) return NULL;
    for (size_t i = 0; i < fp->flag_count; i++) {
        if (fp->flags[i].short_name == c) return &fp->flags[i];
    }
    return NULL;
}

// Helper to check range for signed integers
static bool check_range_int(long long val, long long min, long long max) {
    return (val >= min && val <= max);
}

// Helper to check range for unsigned integers
static bool check_range_uint(unsigned long long val, unsigned long long max) {
    return (val <= max);
}

static FlagStatus parse_value(Flag* flag, const char* str) {
    if (!str) return FLAG_ERROR_MISSING_VALUE;
    char* endptr = NULL;
    errno        = 0;

    switch (flag->type) {
        case TYPE_BOOL: {
            bool val = true;
            if (strcasecmp(str, "false") == 0 || strcmp(str, "0") == 0) val = false;
            *(bool*)flag->value_ptr = val;
            break;
        }
        case TYPE_CHAR:
            if (!str || strlen(str) == 0) return FLAG_ERROR_MISSING_VALUE;
            if (strlen(str) != 1) return FLAG_ERROR_INVALID_ARGUMENT;
            *(char*)flag->value_ptr = str[0];
            break;
        case TYPE_STRING:
            *(char**)flag->value_ptr = (char*)str;
            break;

        // Signed Integers
        case TYPE_INT8:
        case TYPE_INT16:
        case TYPE_INT32:
        case TYPE_INT64: {
            long long val = strtoll(str, &endptr, 10);
            if (endptr == str || *endptr != '\0' || errno == ERANGE) return FLAG_ERROR_INVALID_NUMBER;

            if (flag->type == TYPE_INT8 && !check_range_int(val, INT8_MIN, INT8_MAX)) return FLAG_ERROR_INVALID_NUMBER;
            if (flag->type == TYPE_INT16 && !check_range_int(val, INT16_MIN, INT16_MAX))
                return FLAG_ERROR_INVALID_NUMBER;
            if (flag->type == TYPE_INT32 && !check_range_int(val, INT32_MIN, INT32_MAX))
                return FLAG_ERROR_INVALID_NUMBER;

            // Assign based on type size
            if (flag->type == TYPE_INT8)
                *(int8_t*)flag->value_ptr = (int8_t)val;
            else if (flag->type == TYPE_INT16)
                *(int16_t*)flag->value_ptr = (int16_t)val;
            else if (flag->type == TYPE_INT32)
                *(int32_t*)flag->value_ptr = (int32_t)val;
            else
                *(int64_t*)flag->value_ptr = (int64_t)val;
            break;
        }

        // Unsigned Integers
        case TYPE_UINT8:
        case TYPE_UINT16:
        case TYPE_UINT32:
        case TYPE_UINT64:
        case TYPE_SIZE_T: {
            if (str[0] == '-') return FLAG_ERROR_INVALID_NUMBER;
            unsigned long long val = strtoull(str, &endptr, 10);
            if (endptr == str || *endptr != '\0' || errno == ERANGE) return FLAG_ERROR_INVALID_NUMBER;

            if (flag->type == TYPE_UINT8 && !check_range_uint(val, UINT8_MAX)) return FLAG_ERROR_INVALID_NUMBER;
            if (flag->type == TYPE_UINT16 && !check_range_uint(val, UINT16_MAX)) return FLAG_ERROR_INVALID_NUMBER;
            if (flag->type == TYPE_UINT32 && !check_range_uint(val, UINT32_MAX)) return FLAG_ERROR_INVALID_NUMBER;
            if (flag->type == TYPE_SIZE_T && !check_range_uint(val, SIZE_MAX)) return FLAG_ERROR_INVALID_NUMBER;

            if (flag->type == TYPE_UINT8)
                *(uint8_t*)flag->value_ptr = (uint8_t)val;
            else if (flag->type == TYPE_UINT16)
                *(uint16_t*)flag->value_ptr = (uint16_t)val;
            else if (flag->type == TYPE_UINT32)
                *(uint32_t*)flag->value_ptr = (uint32_t)val;
            else if (flag->type == TYPE_SIZE_T)
                *(size_t*)flag->value_ptr = (size_t)val;
            else
                *(uint64_t*)flag->value_ptr = (uint64_t)val;
            break;
        }

        case TYPE_FLOAT: {
            float val = strtof(str, &endptr);
            if (endptr == str || *endptr != '\0' || errno == ERANGE) return FLAG_ERROR_INVALID_NUMBER;
            *(float*)flag->value_ptr = val;
            break;
        }
        case TYPE_DOUBLE: {
            double val = strtod(str, &endptr);
            if (endptr == str || *endptr != '\0' || errno == ERANGE) return FLAG_ERROR_INVALID_NUMBER;
            *(double*)flag->value_ptr = val;
            break;
        }
        default:
            return FLAG_ERROR_INVALID_ARGUMENT;
    }
    return FLAG_OK;
}

// --- Main Parse Logic ---

FlagStatus flag_parse(FlagParser* fp, int argc, char** argv) {
    if (!fp || argc < 1) return FLAG_ERROR_INVALID_ARGUMENT;

    int i            = 1;
    bool end_of_opts = false;

    while (i < argc) {
        char* arg = argv[i];

        // 1. End of Options
        if (!end_of_opts && strcmp(arg, "--") == 0) {
            end_of_opts = true;
            i++;
            continue;
        }

        // 2. Positionals (or Subcommands)
        if (end_of_opts || arg[0] != '-') {
            // Check subcommands
            if (!end_of_opts && fp->cmd_count > 0) {
                for (size_t c = 0; c < fp->cmd_count; c++) {
                    if (strcmp(fp->subcommands[c]->name, arg) == 0) {
                        fp->active_subcommand = fp->subcommands[c];
                        // Recursive call for subcommand
                        return flag_parse(fp->active_subcommand, argc - i, argv + i);
                    }
                }
            }

            // Add positional
            if (fp->pos_count >= fp->pos_capacity) {
                size_t new_cap      = (fp->pos_capacity == 0) ? INITIAL_CAPACITY : fp->pos_capacity * 2;
                fp->positional_args = (char**)xrealloc(fp->positional_args, new_cap * sizeof(char*));
                fp->pos_capacity    = new_cap;
            }
            fp->positional_args[fp->pos_count++] = arg;
            i++;
            continue;
        }

        // 3. Long Flags
        if (strncmp(arg, "--", 2) == 0) {
            char* name_start = arg + 2;
            if (strcmp(name_start, "help") == 0) {
                flag_print_usage(fp);
                exit(0);
            }

            char* eq = strchr(name_start, '=');
            char name_buf[MAX_FLAG_NAME_LEN];
            const char* val_str     = NULL;
            const char* lookup_name = name_start;

            if (eq) {
                size_t len = (size_t)(eq - name_start);
                if (len >= MAX_FLAG_NAME_LEN) len = MAX_FLAG_NAME_LEN - 1;
                strncpy(name_buf, name_start, len);
                name_buf[len] = '\0';
                lookup_name   = name_buf;
                val_str       = eq + 1;
            }

            Flag* f = find_flag_long(fp, lookup_name);
            if (!f) {
                set_error(fp, "Unknown flag: --%s", lookup_name);
                return FLAG_ERROR_UNKNOWN_FLAG;
            }

            f->is_present = true;

            if (f->type == TYPE_BOOL) {
                bool b = true;
                if (val_str && (strcasecmp(val_str, "false") == 0 || strcmp(val_str, "0") == 0)) b = false;
                *(bool*)f->value_ptr = b;
            } else {
                if (!val_str) {
                    if (i + 1 < argc && argv[i + 1][0] != '-')
                        val_str = argv[++i];
                    else {
                        set_error(fp, "Flag --%s requires a value", f->name);
                        return FLAG_ERROR_MISSING_VALUE;
                    }
                }
                FlagStatus s = parse_value(f, val_str);
                if (s != FLAG_OK) {
                    set_error(fp, "Invalid value for --%s: '%s' (Type mismatch or overflow)", f->name, val_str);
                    return s;
                }
            }
            i++;
            continue;
        }

        // 4. Short Flags
        if (arg[0] == '-') {
            size_t len = strlen(arg);
            for (size_t k = 1; k < len; k++) {
                char c  = arg[k];
                Flag* f = find_flag_short(fp, c);
                if (!f) {
                    set_error(fp, "Unknown short flag: -%c", c);
                    return FLAG_ERROR_UNKNOWN_FLAG;
                }
                f->is_present = true;

                if (f->type == TYPE_BOOL) {
                    *(bool*)f->value_ptr = true;
                } else {
                    const char* val_str = NULL;
                    if (k + 1 < len) {
                        val_str = &arg[k + 1];
                        if (*val_str == '=') val_str++;
                        FlagStatus s = parse_value(f, val_str);
                        if (s != FLAG_OK) {
                            set_error(fp, "Invalid value for -%c (Type mismatch or overflow)", c);
                            return s;
                        }
                        break;  // consumed rest
                    } else {
                        if (i + 1 < argc && argv[i + 1][0] != '-') {
                            val_str      = argv[++i];
                            FlagStatus s = parse_value(f, val_str);
                            if (s != FLAG_OK) {
                                set_error(fp, "Invalid value for -%c (Type mismatch or overflow)", c);
                                return s;
                            }
                        } else {
                            set_error(fp, "Flag -%c requires a value", c);
                            return FLAG_ERROR_MISSING_VALUE;
                        }
                    }
                }
            }
            i++;
            continue;
        }
    }

    // Validation
    for (size_t k = 0; k < fp->flag_count; k++) {
        Flag* f = &fp->flags[k];
        if (f->required && !f->is_present) {
            set_error(fp, "Missing required flag: --%s", f->name);
            return FLAG_ERROR_REQUIRED_MISSING;
        }
        if (f->is_present && f->validator) {
            const char* err = NULL;
            if (!f->validator(f->value_ptr, &err)) {
                set_error(fp, "Validation failed for --%s: %s", f->name, err ? err : "invalid");
                return FLAG_ERROR_VALIDATION;
            }
        }
    }

    return FLAG_OK;
}

FlagStatus flag_parse_and_invoke(FlagParser* fp, int argc, char** argv, void* user_data) {
    if (!fp || argc < 1) {
        return FLAG_ERROR_INVALID_ARGUMENT;
    }

    // Parse all arguments
    FlagStatus status = flag_parse(fp, argc, argv);
    if (status != FLAG_OK) {
        return status;
    }

    // Run pre-invocation callback (Global setup)
    if (fp->pre_invoke) {
        fp->pre_invoke(user_data);
    }

    // Find the deepest active subcommand (The "Leaf" command)
    FlagParser* target = fp;
    while (target->active_subcommand) {
        target = target->active_subcommand;
    }

    // Run the handler for the target command
    if (target->handler) {
        target->handler(user_data);
    } else {
        // If the target has subcommands but no handler, show help
        if (target->cmd_count > 0) {
            flag_print_usage(target);
            return FLAG_ERROR_UNKNOWN_SUBCOMMAND;
        }
    }

    return FLAG_OK;
}

// --- Usage / Help ---

static const char* type_to_str(FlagDataType t) {
    switch (t) {
        case TYPE_BOOL:
            return "";
        case TYPE_CHAR:
            return "CHAR";
        case TYPE_STRING:
            return "STR";
        case TYPE_INT8:
            return "INT8";
        case TYPE_UINT8:
            return "UINT8";
        case TYPE_INT16:
            return "INT16";
        case TYPE_UINT16:
            return "UINT16";
        case TYPE_INT32:
            return "INT";
        case TYPE_UINT32:
            return "UINT";
        case TYPE_INT64:
            return "INT64";
        case TYPE_UINT64:
            return "UINT64";
        case TYPE_SIZE_T:
            return "SIZE";
        case TYPE_FLOAT:
            return "FLOAT";
        case TYPE_DOUBLE:
            return "DBL";
        default:
            return "VAL";
    }
}

static void print_usage_rec(FlagParser* fp, bool is_sub) {
    if (!fp) return;

    printf("Usage: %s%s [flags] %s\n", fp->name, is_sub ? " <subcmd>" : "", fp->cmd_count > 0 ? "<command>" : "[args]");

    if (fp->description) printf("\n%s\n", fp->description);

    // Calculate alignment
    size_t max_width = 0;
    for (size_t i = 0; i < fp->flag_count; i++) {
        size_t w = 6;                        // "  -x, "
        w += strlen(fp->flags[i].name) + 2;  // --name
        if (fp->flags[i].type != TYPE_BOOL) {
            w += strlen(type_to_str(fp->flags[i].type)) + 1;  // =TYPE
        }
        if (w > max_width) max_width = w;
    }

    if (fp->flag_count > 0) printf("\nFlags:\n");
    for (size_t i = 0; i < fp->flag_count; i++) {
        Flag* f = &fp->flags[i];

        // Build the left column: "  -s, --long=TYPE"
        char left[128];
        int pos = 0;
        pos += snprintf(left + pos, (size_t)(128 - pos), "  ");
        if (f->short_name)
            pos += snprintf(left + pos, (size_t)(128 - pos), "-%c, ", f->short_name);
        else
            pos += snprintf(left + pos, (size_t)(128 - pos), "    ");

        pos += snprintf(left + pos, (size_t)(128 - pos), "--%s", f->name);
        if (f->type != TYPE_BOOL) {
            pos += snprintf(left + pos, (size_t)(128 - pos), "=%s", type_to_str(f->type));
        }

        // Print row
        printf("%-*s  %s", (int)max_width, left, f->description ? f->description : "");
        if (f->required) printf(" (Required)");
        printf("\n");
    }

    if (fp->cmd_count > 0) {
        printf("\nCommands:\n");
        for (size_t i = 0; i < fp->cmd_count; i++) {
            printf("  %-*s  %s\n", (int)max_width, fp->subcommands[i]->name,
                   fp->subcommands[i]->description ? fp->subcommands[i]->description : "");
        }
    }

    if (fp->footer) printf("\n%s\n", fp->footer);
}

void flag_print_usage(FlagParser* fp) {
    if (!fp) return;
    if (fp->active_subcommand) {
        FlagParser* leaf = fp->active_subcommand;
        while (leaf->active_subcommand)
            leaf = leaf->active_subcommand;
        print_usage_rec(leaf, false);
    } else {
        print_usage_rec(fp, false);
    }
}

// Accessors
const char* flag_get_error(FlagParser* fp) {
    if (!fp) return "";

    FlagParser* root = fp;

    if (root->active_subcommand) {
        root = fp->active_subcommand;
        while (root->active_subcommand)
            root = root->active_subcommand;
    }
    return root->last_error;
}

int flag_positional_count(FlagParser* fp) {
    return fp ? (int)fp->pos_count : 0;
}

const char* flag_positional_at(FlagParser* fp, int i) {
    return (fp && i >= 0 && (size_t)i < fp->pos_count) ? fp->positional_args[i] : NULL;
}

bool flag_is_present(FlagParser* fp, const char* name) {
    Flag* f = find_flag_long(fp, name);
    return f ? f->is_present : false;
}

const char* flag_status_str(FlagStatus s) {
    switch (s) {
        case FLAG_OK:
            return "OK";
        case FLAG_ERROR_MISSING_VALUE:
            return "Missing Value";
        case FLAG_ERROR_UNKNOWN_FLAG:
            return "Unknown Flag";
        case FLAG_ERROR_INVALID_NUMBER:
            return "Invalid Number";
        case FLAG_ERROR_VALIDATION:
            return "Validation Failed";
        case FLAG_ERROR_REQUIRED_MISSING:
            return "Required Flag Missing";
        default:
            return "Error";
    }
}
