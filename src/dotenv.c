#include "../include/dotenv.h"

#include <ctype.h>   // for isspace
#include <errno.h>   // for errno
#include <stdio.h>   // for fprintf, stderr, fopen, fgets, fclose
#include <string.h>  // for strlen, strchr, memset

/**
 * Removes leading and trailing whitespace from a string in-place.
 * @param str The string to trim. Must be non-NULL.
 * @return Pointer to the trimmed string (may be different from input).
 */
static char* trim_whitespace(char* str) {
    if (str == NULL) {
        return NULL;
    }

    // Remove leading whitespace
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    // Remove trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}

/**
 * Removes surrounding quotes from a string in-place.
 * @param str The string to unquote. Must be non-NULL.
 * @return Pointer to the unquoted string (may be different from input).
 */
static char* remove_quotes(char* str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    if (len < 2) {
        return str;
    }

    // Check for matching quotes
    if ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\'')) {
        str[len - 1] = '\0';
        return str + 1;
    }

    return str;
}

/**
 * Interpolates environment variables in the format ${VAR_NAME}.
 * @param value The string containing variable references. Must be non-NULL.
 * @param result Buffer to store the interpolated result. Must be non-NULL.
 * @param result_size Size of the result buffer.
 * @return true on success, false if buffer is too small or invalid input.
 */
static bool interpolate(const char* value, char* result, size_t result_size) {
    if (value == NULL || result == NULL || result_size == 0) {
        return false;
    }

    size_t result_len = 0;
    const char* ptr   = value;
    memset(result, 0, result_size);

    while (*ptr && result_len < result_size - 1) {
        // Look for variable interpolation pattern: ${VAR_NAME}
        if (*ptr == '$' && *(ptr + 1) == '{') {
            const char* start = ptr + 2;
            const char* end   = strchr(start, '}');

            if (end == NULL) {
                // Malformed interpolation - copy literal characters
                fprintf(stderr, "Warning: Unclosed variable reference starting at position %td\n", ptr - value);
                result[result_len++] = *ptr++;
                continue;
            }

            // Extract variable name
            size_t var_name_len = (size_t)(end - start);
            if (var_name_len == 0) {
                // Empty variable name: ${}
                fprintf(stderr, "Warning: Empty variable name\n");
                ptr = end + 1;
                continue;
            }

            if (var_name_len >= MAX_VAR_NAME_LEN) {
                fprintf(stderr, "Error: Variable name too long (max %d)\n", MAX_VAR_NAME_LEN - 1);
                return false;
            }

            char var_name[MAX_VAR_NAME_LEN] = {0};
            snprintf(var_name, sizeof(var_name), "%.*s", (int)var_name_len, start);

            // Get variable value from environment
            const char* var_value = GETENV(var_name);
            if (var_value != NULL) {
                size_t var_value_len = strlen(var_value);
                // Check if we have enough space (account for null terminator)
                if (result_len + var_value_len >= result_size) {
                    fprintf(stderr, "Error: Result buffer too small for interpolation\n");
                    return false;
                }

                memcpy(result + result_len, var_value, var_value_len);
                result_len += var_value_len;
            } else {
                fprintf(stderr, "Warning: Environment variable '%s' not found, skipping\n", var_name);
            }

            ptr = end + 1;
        } else {
            // Regular character - copy as-is
            result[result_len++] = *ptr++;
        }
    }

    // Check if we ran out of buffer space
    if (*ptr != '\0') {
        fprintf(stderr, "Error: Result buffer too small\n");
        return false;
    }

    result[result_len] = '\0';
    return true;
}

/**
 * Processes a single key-value line and sets the environment variable.
 * @param key The environment variable name. Must be non-NULL.
 * @param value The value to set. Must be non-NULL.
 * @return true on success, false on error.
 */
static bool process_env_pair(char* key, char* value) {
    if (key == NULL || value == NULL) {
        return false;
    }

    // Trim key
    key = trim_whitespace(key);
    if (*key == '\0') {
        fprintf(stderr, "Error: Empty key\n");
        return false;
    }

    // Trim and unquote value
    value = trim_whitespace(value);
    value = remove_quotes(value);

    // Check if interpolation is needed
    if (strchr(value, '$') != NULL && strchr(value, '{') != NULL && strchr(value, '}') != NULL) {
        char interpolated_value[MAX_LINE_LENGTH] = {0};
        if (!interpolate(value, interpolated_value, sizeof(interpolated_value))) {
            fprintf(stderr, "Error: Failed to interpolate value for key '%s'\n", key);
            return false;
        }

        if (SETENV(key, interpolated_value, 1) != 0) {
            fprintf(stderr, "Error: Failed to set environment variable '%s': %s\n", key, strerror(errno));
            return false;
        }
    } else {
        if (SETENV(key, value, 1) != 0) {
            fprintf(stderr, "Error: Failed to set environment variable '%s': %s\n", key, strerror(errno));
            return false;
        }
    }

    return true;
}

bool load_dotenv(const char* path) {
    if (path == NULL) {
        fprintf(stderr, "Error: NULL path provided\n");
        return false;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", path, strerror(errno));
        return false;
    }

    char line[MAX_LINE_LENGTH] = {0};
    size_t line_number         = 0;
    bool had_errors            = false;

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        char* trimmed = trim_whitespace(line);

        // Skip empty lines and comments
        if (*trimmed == '\0' || *trimmed == '#') {
            continue;
        }

        // Find the '=' separator
        char* equals = strchr(trimmed, '=');
        if (equals == NULL) {
            fprintf(stderr, "Warning: Invalid line %zu (no '=' found): %s\n", line_number, trimmed);
            had_errors = true;
            continue;
        }

        // Split into key and value
        *equals     = '\0';
        char* key   = trimmed;
        char* value = equals + 1;

        if (!process_env_pair(key, value)) {
            fprintf(stderr, "Warning: Failed to process line %zu\n", line_number);
            had_errors = true;
        }
    }

    if (ferror(file)) {
        fprintf(stderr, "Error: Failed to read from file '%s'\n", path);
        fclose(file);
        return false;
    }

    fclose(file);
    return !had_errors;
}
