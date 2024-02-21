#ifndef C4CF7E62_E0E2_4AD1_8601_968CD959C1FA
#define C4CF7E62_E0E2_4AD1_8601_968CD959C1FA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Bit flags
#define LOG_TIMESTAMP 0x1  // include timestamp
#define LOG_LEVEL 0x2      // include log level
#define LOG_FILENAME 0x4   // include file name
#define LOG_FLAG_ALL (LOG_TIMESTAMP | LOG_LEVEL | LOG_FILENAME)

// Log levels
#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_ERROR 3

// String representation for the flags
#define LOG_DEBUG_STR "DEBUG"
#define LOG_INFO_STR "INFO"
#define LOG_WARN_STR "WARN"
#define LOG_ERROR_STR "ERROR"

#define FORMAT_SIZE 64

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
            if (flags & LOG_LEVEL) {                                                               \
                const char* level_str = LOG_DEBUG_STR;                                             \
                switch (level) {                                                                   \
                    case LOG_DEBUG:                                                                \
                        level_str = LOG_DEBUG_STR;                                                 \
                        break;                                                                     \
                    case LOG_INFO:                                                                 \
                        level_str = LOG_INFO_STR;                                                  \
                        break;                                                                     \
                    case LOG_WARN:                                                                 \
                        level_str = LOG_WARN_STR;                                                  \
                        break;                                                                     \
                    case LOG_ERROR:                                                                \
                        level_str = LOG_ERROR_STR;                                                 \
                        break;                                                                     \
                }                                                                                  \
                fprintf(log_file, "[%-5s] ", level_str);                                           \
            }                                                                                      \
            if (flags & LOG_FILENAME) {                                                            \
                fprintf(log_file, "%s:%d - ", __FILE__, __LINE__);                                 \
            }                                                                                      \
            fprintf(log_file, fmt, ##__VA_ARGS__);                                                 \
        }                                                                                          \
    } while (0)

// %F - %Y-%m-%d
// %r - %I:%M:%S %p
extern FILE* log_file;
extern char LOG_TIME_FORMAT[FORMAT_SIZE];

static int log_level = LOG_DEBUG;
static int log_flags = LOG_TIMESTAMP | LOG_LEVEL | LOG_FILENAME;

#define DEBUG(fmt, ...) LOG(LOG_DEBUG, log_flags, fmt, ##__VA_ARGS__)
#define INFO(fmt, ...) LOG(LOG_INFO, log_flags, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) LOG(LOG_WARN, log_flags, fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...) LOG(LOG_ERROR, log_flags, fmt, ##__VA_ARGS__)

#ifdef LOG_IMPLEMENTATION
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

void log_setfile(FILE* out) {
    if (out != NULL) {
        log_file = out;
    }
}
#endif /* LOG_IMPLEMENTATION */

#endif /* C4CF7E62_E0E2_4AD1_8601_968CD959C1FA */
