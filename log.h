#ifndef C4CF7E62_E0E2_4AD1_8601_968CD959C1FA
#define C4CF7E62_E0E2_4AD1_8601_968CD959C1FA

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Bit flags
#define LOG_TIMESTAMP 0x1  // include timestamp
#define LOG_LEVEL 0x2      // include log level
#define LOG_FILENAME 0x4   // include file name
#define LOG_FLAG_ALL (LOG_TIMESTAMP | LOG_LEVEL | LOG_FILENAME)

// Format size for the timestamp string buffer (64 bytes)
#define FORMAT_SIZE 64

// Use X macro to define the log levels enum and the string representation
#define LOG_LEVELS(X)                                                                              \
    X(LOG_DEBUG)                                                                                   \
    X(LOG_INFO)                                                                                    \
    X(LOG_WARN)                                                                                    \
    X(LOG_ERROR)

#define X(a) a,
enum LogLevel { LOG_LEVELS(X) };
#undef X

// Create an array of string representations of the log levels
// where the index of the array is the log level
#define X(a) #a,
const char* log_level_str[] = {LOG_LEVELS(X)};
#undef X

// %F - %Y-%m-%d
// %r - %I:%M:%S %p
extern FILE* log_file;
extern char LOG_TIME_FORMAT[FORMAT_SIZE];

// Global log level and flags
int log_level = LOG_DEBUG;
int log_flags = LOG_FLAG_ALL;

#define LOG(level, flags, fmt, ...)                                                                \
    do {                                                                                           \
        if (log_file == NULL) {                                                                    \
            log_file = stdout;                                                                     \
        }                                                                                          \
        if (level >= log_level) {                                                                  \
            if (flags & LOG_TIMESTAMP) {                                                           \
                time_t t           = time(NULL);                                                   \
                struct tm* tm_info = localtime(&t);                                                \
                char buf[FORMAT_SIZE];                                                             \
                strftime(buf, FORMAT_SIZE, LOG_TIME_FORMAT, tm_info);                              \
                buf[sizeof(buf) - 1] = '\0';                                                       \
                fprintf(log_file, "[%s] ", buf);                                                   \
            }                                                                                      \
                                                                                                   \
            if ((flags & LOG_LEVEL && level <= LOG_ERROR)) {                                       \
                fprintf(log_file, "[%-5s] ", log_level_str[level]);                                \
            }                                                                                      \
            if (flags & LOG_FILENAME) {                                                            \
                fprintf(log_file, "%s:%d - ", __FILE__, __LINE__);                                 \
            }                                                                                      \
            fprintf(log_file, fmt, ##__VA_ARGS__);                                                 \
        }                                                                                          \
    } while (0)

#define DEBUG(fmt, ...) LOG(LOG_DEBUG, log_flags, fmt, ##__VA_ARGS__)
#define INFO(fmt, ...) LOG(LOG_INFO, log_flags, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) LOG(LOG_WARN, log_flags, fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...) LOG(LOG_ERROR, log_flags, fmt, ##__VA_ARGS__)

#if defined(__cplusplus)
}
#endif

#ifdef LOG_IMPL
FILE* log_file;
char LOG_TIME_FORMAT[FORMAT_SIZE] = "%F %r";

void log_setflags(int flags) {
    log_flags = flags;
}

void log_settimeformat(const char* format) {
    size_t fmt_len = strlen(format);
    strncpy(LOG_TIME_FORMAT, format, FORMAT_SIZE);
    if (fmt_len < FORMAT_SIZE - 1) {
        LOG_TIME_FORMAT[fmt_len] = '\0';
    } else {
        LOG_TIME_FORMAT[FORMAT_SIZE - 1] = '\0';
    }
}
void log_setlevel(enum LogLevel level) {
    log_level = level;
}

void log_setfile(FILE* out) {
    if (out != NULL) {
        log_file = out;
    }
}
#endif /* LOG_IMPL */

#endif /* C4CF7E62_E0E2_4AD1_8601_968CD959C1FA */
