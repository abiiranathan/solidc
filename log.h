#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_FLAG_TIME 0x1                                                              // include timestamp
#define LOG_FLAG_LEVEL 0x2                                                             // include log level
#define LOG_FLAG_FILE 0x4                                                              // include file name
#define LOG_FLAG_LINE 0x8                                                              // include line number
#define LOG_FLAG_ALL (LOG_FLAG_TIME | LOG_FLAG_LEVEL | LOG_FLAG_FILE | LOG_FLAG_LINE)  // include everything

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3

#define LOG_LEVEL_DEBUG_STR "DEBUG"
#define LOG_LEVEL_INFO_STR "INFO"
#define LOG_LEVEL_WARN_STR "WARN"
#define LOG_LEVEL_ERROR_STR "ERROR"

extern char LOG_TIME_FORMAT[18];
extern FILE *log_file;

#define LOG(level, flags, fmt, ...)                                \
    do {                                                           \
        if (!log_file) {                                           \
            log_file = stdout;                                     \
        }                                                          \
        if (level >= log_level) {                                  \
            if (flags & LOG_FLAG_TIME) {                           \
                time_t t = time(NULL);                             \
                struct tm *tm_info = localtime(&t);                \
                char buf[20];                                      \
                strftime(buf, 20, LOG_TIME_FORMAT, tm_info);       \
                fprintf(log_file, "[%s] ", buf);                   \
            }                                                      \
            if (flags & LOG_FLAG_LEVEL) {                          \
                const char *level_str;                             \
                switch (level) {                                   \
                    case LOG_LEVEL_DEBUG:                          \
                        level_str = LOG_LEVEL_DEBUG_STR;           \
                        break;                                     \
                    case LOG_LEVEL_INFO:                           \
                        level_str = LOG_LEVEL_INFO_STR;            \
                        break;                                     \
                    case LOG_LEVEL_WARN:                           \
                        level_str = LOG_LEVEL_WARN_STR;            \
                        break;                                     \
                    case LOG_LEVEL_ERROR:                          \
                        level_str = LOG_LEVEL_ERROR_STR;           \
                        break;                                     \
                    default:                                       \
                        level_str = "";                            \
                        break;                                     \
                }                                                  \
                fprintf(log_file, "[%-5s] ", level_str);           \
            }                                                      \
            if (flags & LOG_FLAG_FILE) {                           \
                fprintf(log_file, "%s:%d - ", __FILE__, __LINE__); \
            }                                                      \
            fprintf(log_file, fmt, ##__VA_ARGS__);                 \
        }                                                          \
    } while (0)

static int log_level = LOG_LEVEL_DEBUG;
static int log_flags = LOG_FLAG_TIME | LOG_FLAG_LEVEL | LOG_FLAG_FILE;

#define log_debug(fmt, ...) LOG(LOG_LEVEL_DEBUG, log_flags, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) LOG(LOG_LEVEL_INFO, log_flags, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) LOG(LOG_LEVEL_WARN, log_flags, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) LOG(LOG_LEVEL_ERROR, log_flags, fmt, ##__VA_ARGS__)

void set_log_time_format(char *format);
void set_log_level(int level);
void set_log_flags(int flags);
void set_log_file(FILE *file);

#endif /* __LOG_H__ */
