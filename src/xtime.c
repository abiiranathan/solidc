#include "../include/xtime.h"
#include <ctype.h>   // for isdigit
#include <errno.h>   // for errno
#include <stdio.h>   // for snprintf
#include <stdlib.h>  // for strtol, abs
#include <string.h>  // for strncpy, memset, strlen

// Platform-specific includes for high-resolution time
#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <sys/time.h>  // for gettimeofday
#endif

// Windows does not have gmtime_r & localtime_r and completely lacks strptime.
#if defined(_WIN32) || defined(_WIN64)
// Windows compatibility wrappers
static inline struct tm* gmtime_r(const time_t* timer, struct tm* buf) {
    return (gmtime_s(buf, timer) == 0) ? buf : NULL;
}

static inline struct tm* localtime_r(const time_t* timer, struct tm* buf) {
    return (localtime_s(buf, timer) == 0) ? buf : NULL;
}

/**
 * Windows implementation of strptime using manual parsing.
 * Supports common format specifiers needed by xtime.
 */
static char* strptime(const char* buf, const char* fmt, struct tm* tm) {
    if (buf == NULL || fmt == NULL || tm == NULL) {
        return NULL;
    }

    const char* s = buf;
    const char* f = fmt;
    bool is_pm    = false;
    bool has_ampm = false;

    while (*f != '\0') {
        // Skip whitespace in both format and input
        while (*f == ' ' || *f == '\t')
            f++;
        while (*s == ' ' || *s == '\t')
            s++;

        if (*f != '%') {
            // Literal character match
            if (*f != *s) {
                return NULL;
            }
            f++;
            s++;
            continue;
        }

        // Format specifier
        f++;  // Skip '%'

        switch (*f) {
            case 'Y': {  // 4-digit year
                if (!isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[2]) || !isdigit(s[3])) {
                    return NULL;
                }
                int year    = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
                tm->tm_year = year - 1900;
                s += 4;
                break;
            }

            case 'y': {  // 2-digit year
                if (!isdigit(s[0]) || !isdigit(s[1])) {
                    return NULL;
                }
                int year = (s[0] - '0') * 10 + (s[1] - '0');
                // Assume 1969-2068 range (POSIX convention)
                tm->tm_year = (year >= 69) ? year : (year + 100);
                s += 2;
                break;
            }

            case 'm': {  // Month (01-12) - zero-padded
                if (!isdigit(s[0]) || !isdigit(s[1])) {
                    return NULL;
                }
                int month = (s[0] - '0') * 10 + (s[1] - '0');
                if (month < 1 || month > 12) {
                    return NULL;
                }
                tm->tm_mon = month - 1;
                s += 2;
                break;
            }

            case 'd': {  // Day of month (01-31) - zero-padded, MUST be 2 digits
                if (!isdigit(s[0]) || !isdigit(s[1])) {
                    return NULL;
                }
                int day = (s[0] - '0') * 10 + (s[1] - '0');
                if (day < 1 || day > 31) {
                    return NULL;
                }
                tm->tm_mday = day;
                s += 2;
                break;
            }

            case 'e': {  // Day of month (1-31, space or zero padded)
                if (*s == ' ') {
                    s++;  // Skip leading space
                    if (!isdigit(s[0])) {
                        return NULL;
                    }
                    int day = (s[0] - '0');
                    s++;
                    if (day < 1 || day > 9) {
                        return NULL;
                    }
                    tm->tm_mday = day;
                } else if (isdigit(s[0])) {
                    int day = (s[0] - '0');
                    s++;
                    if (isdigit(s[0])) {
                        day = day * 10 + (s[0] - '0');
                        s++;
                    }
                    if (day < 1 || day > 31) {
                        return NULL;
                    }
                    tm->tm_mday = day;
                } else {
                    return NULL;
                }
                break;
            }

            case 'H': {  // Hour (00-23) - zero-padded
                if (!isdigit(s[0]) || !isdigit(s[1])) {
                    return NULL;
                }
                int hour = (s[0] - '0') * 10 + (s[1] - '0');
                if (hour > 23) {
                    return NULL;
                }
                tm->tm_hour = hour;
                s += 2;
                break;
            }

            case 'I': {  // Hour (01-12) - zero-padded
                if (!isdigit(s[0]) || !isdigit(s[1])) {
                    return NULL;
                }
                int hour = (s[0] - '0') * 10 + (s[1] - '0');
                if (hour < 1 || hour > 12) {
                    return NULL;
                }
                tm->tm_hour = hour;
                // Apply AM/PM adjustment if already parsed
                if (has_ampm) {
                    if (is_pm && tm->tm_hour < 12) {
                        tm->tm_hour += 12;
                    } else if (!is_pm && tm->tm_hour == 12) {
                        tm->tm_hour = 0;
                    }
                }
                s += 2;
                break;
            }

            case 'M': {  // Minute (00-59) - zero-padded
                if (!isdigit(s[0]) || !isdigit(s[1])) {
                    return NULL;
                }
                int min = (s[0] - '0') * 10 + (s[1] - '0');
                if (min > 59) {
                    return NULL;
                }
                tm->tm_min = min;
                s += 2;
                break;
            }

            case 'S': {  // Second (00-60, allowing leap second) - zero-padded
                if (!isdigit(s[0]) || !isdigit(s[1])) {
                    return NULL;
                }
                int sec = (s[0] - '0') * 10 + (s[1] - '0');
                if (sec > 60) {
                    return NULL;
                }
                tm->tm_sec = sec;
                s += 2;
                break;
            }

            case 'p': {  // AM/PM
                if ((s[0] == 'A' || s[0] == 'a') && (s[1] == 'M' || s[1] == 'm')) {
                    is_pm    = false;
                    has_ampm = true;
                    // If hour already set (from %I), apply adjustment now
                    if (tm->tm_hour == 12) {
                        tm->tm_hour = 0;
                    }
                } else if ((s[0] == 'P' || s[0] == 'p') && (s[1] == 'M' || s[1] == 'm')) {
                    is_pm    = true;
                    has_ampm = true;
                    // If hour already set (from %I), apply adjustment now
                    if (tm->tm_hour < 12) {
                        tm->tm_hour += 12;
                    }
                } else {
                    return NULL;
                }
                s += 2;
                break;
            }

            case 'b':    // Abbreviated month name
            case 'B': {  // Full month name
                static const char* months[]      = {"january", "february", "march",     "april",   "may",      "june",
                                                    "july",    "august",   "september", "october", "november", "december"};
                static const char* abbr_months[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                                    "jul", "aug", "sep", "oct", "nov", "dec"};

                bool found = false;
                for (int i = 0; i < 12; i++) {
                    size_t len      = (*f == 'b') ? 3 : strlen(months[i]);
                    const char* cmp = (*f == 'b') ? abbr_months[i] : months[i];

                    if (_strnicmp(s, cmp, len) == 0) {
                        tm->tm_mon = i;
                        s += len;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return NULL;
                }
                break;
            }

            case 'a':    // Abbreviated weekday name
            case 'A': {  // Full weekday name
                static const char* weekdays[]      = {"sunday",   "monday", "tuesday", "wednesday",
                                                      "thursday", "friday", "saturday"};
                static const char* abbr_weekdays[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

                bool found = false;
                for (int i = 0; i < 7; i++) {
                    size_t len      = (*f == 'a') ? 3 : strlen(weekdays[i]);
                    const char* cmp = (*f == 'a') ? abbr_weekdays[i] : weekdays[i];

                    if (_strnicmp(s, cmp, len) == 0) {
                        tm->tm_wday = i;
                        s += len;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return NULL;
                }
                break;
            }

            case 'j': {  // Day of year (001-366) - zero-padded, 3 digits
                if (!isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[2])) {
                    return NULL;
                }
                int yday = (s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0');
                if (yday < 1 || yday > 366) {
                    return NULL;
                }
                tm->tm_yday = yday - 1;
                s += 3;
                break;
            }

            case 'z': {  // Timezone offset (+hhmm or -hhmm or +hh:mm or -hh:mm)
                // Consume the timezone but don't parse it into tm
                // The caller (xtime_parse) handles this separately
                if (*s == '+' || *s == '-') {
                    s++;  // Skip sign
                    // Consume hours (2 digits)
                    if (isdigit(s[0]) && isdigit(s[1])) {
                        s += 2;
                    } else {
                        return NULL;
                    }
                    // Optional colon
                    if (*s == ':') {
                        s++;
                    }
                    // Consume minutes (2 digits)
                    if (isdigit(s[0]) && isdigit(s[1])) {
                        s += 2;
                    }
                } else if (*s == 'Z' || *s == 'z') {
                    s++;  // UTC designator
                }
                break;
            }

            case 'Z': {  // Timezone name - variable length
                // Skip timezone name until whitespace or special char
                while (*s != '\0' && !isspace(*s) && *s != '+' && *s != '-') {
                    s++;
                }
                break;
            }

            case 'T': {  // Time in HH:MM:SS format
                // Recursively parse %H:%M:%S
                char* result = strptime(s, "%H:%M:%S", tm);
                if (result == NULL) {
                    return NULL;
                }
                s = result;
                break;
            }

            case 'F': {  // Date in YYYY-MM-DD format
                // Recursively parse %Y-%m-%d
                char* result = strptime(s, "%Y-%m-%d", tm);
                if (result == NULL) {
                    return NULL;
                }
                s = result;
                break;
            }

            case '%': {  // Literal '%'
                if (*s != '%') {
                    return NULL;
                }
                s++;
                break;
            }

            default:
                // Unsupported format specifier
                return NULL;
        }

        f++;
    }

    // Set DST flag to unknown
    tm->tm_isdst = -1;

    return (char*)s;
}

#endif

/** Nanoseconds per second */
static const int64_t NANOS_PER_SEC   = 1000000000LL;
static const int64_t NANOS_PER_MICRO = 1000LL;
static const int64_t NANOS_PER_MILLI = 1000000LL;

/** Maximum valid timezone offset in minutes */
static const int16_t MAX_TZ_OFFSET = 1439;  // ±23:59

/**
 * Helper: Checks if a year is a leap year.
 */
static bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * Helper: Returns the number of days in a given month.
 */
static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month < 0 || month > 11) {
        return 0;
    }

    if (month == 1 && is_leap_year(year)) {  // February in leap year
        return 29;
    }

    return days[month];
}

xtime_error_t xtime_init(xtime_t* t) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *t = (xtime_t){.seconds = 0, .nanoseconds = 0, .tz_offset = 0, .has_tz = false};

    return XTIME_OK;
}

/**
 * Helper: Gets local timezone offset in minutes from UTC.
 * Returns 0 on failure.
 */
static int16_t get_local_tz_offset(time_t timestamp) {
    struct tm utc_tm, local_tm;

    // Get both UTC and local time representations
    if (gmtime_r(&timestamp, &utc_tm) == NULL) {
        return 0;
    }
    if (localtime_r(&timestamp, &local_tm) == NULL) {
        return 0;
    }

    // Calculate difference in minutes
    // Account for day boundary crossings
    int day_diff  = local_tm.tm_mday - utc_tm.tm_mday;
    int hour_diff = local_tm.tm_hour - utc_tm.tm_hour;
    int min_diff  = local_tm.tm_min - utc_tm.tm_min;

    // Adjust for day boundaries
    if (day_diff > 1) {
        day_diff = -1;  // Wrapped backwards
    } else if (day_diff < -1) {
        day_diff = 1;  // Wrapped forwards
    }

    int total_minutes = (day_diff * 24 * 60) + (hour_diff * 60) + min_diff;

    // Sanity check
    if (abs(total_minutes) > MAX_TZ_OFFSET) {
        return 0;
    }

    return (int16_t)total_minutes;
}

xtime_error_t xtime_now(xtime_t* t) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

#if defined(_WIN32) || defined(_WIN64)
    // Windows: Use GetSystemTimePreciseAsFileTime for high precision
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);

    // Convert FILETIME (100-nanosecond intervals since 1601-01-01) to Unix time
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    // Windows epoch is 1601-01-01, Unix epoch is 1970-01-01
    // Difference: 116444736000000000 * 100ns = 11644473600 seconds
    const int64_t WINDOWS_TO_UNIX_EPOCH = 11644473600LL;
    int64_t total_100ns                 = (int64_t)(uli.QuadPart);

    t->seconds     = (total_100ns / 10000000LL) - WINDOWS_TO_UNIX_EPOCH;
    t->nanoseconds = (uint32_t)((total_100ns % 10000000LL) * 100);

#elif defined(__APPLE__)
    // macOS: Use mach_absolute_time
    static mach_timebase_info_data_t timebase_info = {0};
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t mach_time = mach_absolute_time();
    uint64_t nanos     = (mach_time * timebase_info.numer) / timebase_info.denom;

    // Get wall clock time for seconds component
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return XTIME_ERR_SYSTEM;
    }

    t->seconds     = (int64_t)tv.tv_sec;
    t->nanoseconds = (uint32_t)(tv.tv_usec * 1000);

#else
    // Linux/POSIX: Use clock_gettime with CLOCK_REALTIME
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return XTIME_ERR_SYSTEM;
    }

    t->seconds     = (int64_t)ts.tv_sec;
    t->nanoseconds = (uint32_t)ts.tv_nsec;
#endif

    // Capture local timezone offset
    t->tz_offset = get_local_tz_offset((time_t)t->seconds);
    t->has_tz    = true;

    return XTIME_OK;
}

xtime_error_t xtime_utc_now(xtime_t* t) {
    xtime_error_t err = xtime_now(t);
    if (err == XTIME_OK) {
        t->tz_offset = 0;
        t->has_tz    = false;
    }
    return err;
}

/**
 * Helper: Parses timezone offset from string like "+0530" or "-08:00"
 * Returns offset in minutes, or 0 if parsing fails.
 */
static bool parse_tz_offset(const char* str, int16_t* offset) {
    if (str == NULL || offset == NULL) {
        return false;
    }

    // Skip whitespace
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    // Check for sign
    int sign = 1;
    if (*str == '+') {
        sign = 1;
        str++;
    } else if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == 'Z' || *str == 'z') {
        *offset = 0;
        return true;
    } else {
        return false;
    }

    // Parse hours
    if (!isdigit(str[0]) || !isdigit(str[1])) {
        return false;
    }
    int hours = (str[0] - '0') * 10 + (str[1] - '0');
    str += 2;

    // Check for colon separator (optional)
    if (*str == ':') {
        str++;
    }

    // Parse minutes
    int minutes = 0;
    if (isdigit(str[0]) && isdigit(str[1])) {
        minutes = (str[0] - '0') * 10 + (str[1] - '0');
    }

    // Validate range
    if (hours > 23 || minutes > 59) {
        return false;
    }

    int total_minutes = (hours * 60 + minutes) * sign;
    if (abs(total_minutes) > MAX_TZ_OFFSET) {
        return false;
    }

    *offset = (int16_t)total_minutes;
    return true;
}

xtime_error_t xtime_parse(const char* str, const char* format, xtime_t* t) {
    if (str == NULL || format == NULL || t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    xtime_init(t);

    struct tm tm_result;
    memset(&tm_result, 0, sizeof(tm_result));

    // 1. Parse the main date/time parts
    char* remaining = strptime(str, format, &tm_result);
    if (remaining == NULL) {
        return XTIME_ERR_PARSE_FAILED;
    }

    // 2. Convert struct tm to Unix timestamp (UTC-based)
    // Manual calculation to avoid mktime/timegm portability issues
    int year   = tm_result.tm_year + 1900;
    int month  = tm_result.tm_mon;
    int day    = tm_result.tm_mday;
    int hour   = tm_result.tm_hour;
    int minute = tm_result.tm_min;
    int second = tm_result.tm_sec;

    // Calculate days since Unix epoch (1970-01-01)
    int64_t days = 0;

    // Add years
    for (int y = 1970; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    // Add months
    for (int m = 0; m < month; m++) {
        days += days_in_month(year, m);
    }

    // Add days (tm_mday is 1-based)
    days += day - 1;

    // Convert to seconds and add time components
    t->seconds = (days * 86400) + (hour * 3600) + (minute * 60) + second;

    // 3. Handle Fractional Seconds (The ".123" part from JS/JSON)
    if (*remaining == '.') {
        remaining++;  // Skip the dot

        int64_t multiplier = 100000000;  // Start at 100ms
        uint32_t nanos     = 0;

        // Read up to 9 digits (nanosecond precision)
        while (isdigit(*remaining)) {
            if (multiplier >= 1) {
                nanos += (*remaining - '0') * multiplier;
                multiplier /= 10;
            }
            remaining++;
        }
        t->nanoseconds = nanos;
    }

    // 4. Handle Timezone
    // Skip any leftover whitespace
    while (*remaining == ' ')
        remaining++;

    if (*remaining != '\0') {
        int16_t tz_offset = 0;
        if (parse_tz_offset(remaining, &tz_offset)) {
            t->tz_offset = tz_offset;
            t->has_tz    = true;
        }
    }

    return XTIME_OK;
}

xtime_error_t xtime_format(const xtime_t* t, const char* format, char* buf, size_t buflen) {
    if (t == NULL || format == NULL || buf == NULL || buflen == 0) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Handle special format: Unix timestamp
    if (strcmp(format, "%s") == 0 || strcmp(format, XTIME_FMT_UNIX) == 0) {
        int written = snprintf(buf, buflen, "%lld", (long long)t->seconds);
        if (written < 0 || (size_t)written >= buflen) {
            return XTIME_ERR_BUFFER_TOO_SMALL;
        }
        return XTIME_OK;
    }

    // Adjust timestamp for timezone if present
    time_t timestamp = (time_t)t->seconds;
    if (t->has_tz && t->tz_offset != 0) {
        // Add timezone offset to display time in local timezone
        timestamp += (time_t)(t->tz_offset * 60);
    }

    struct tm tm_result;

    // Use gmtime_r since we've already adjusted for timezone
    if (gmtime_r(&timestamp, &tm_result) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Format using strftime
    size_t result = strftime(buf, buflen, format, &tm_result);
    if (result == 0) {
        return XTIME_ERR_BUFFER_TOO_SMALL;
    }

    // Append timezone if format contains %z and we have timezone info
    if (t->has_tz && strstr(format, "%z") != NULL) {
        size_t len  = strlen(buf);
        int hours   = abs(t->tz_offset) / 60;
        int minutes = abs(t->tz_offset) % 60;
        char sign   = t->tz_offset >= 0 ? '+' : '-';

        int written = snprintf(buf + len, buflen - len, "%c%02d:%02d", sign, hours, minutes);
        if (written < 0 || len + (size_t)written >= buflen) {
            return XTIME_ERR_BUFFER_TOO_SMALL;
        }
    }

    return XTIME_OK;
}

xtime_error_t xtime_format_utc(const xtime_t* t, const char* format, char* buf, size_t buflen) {
    if (t == NULL || format == NULL || buf == NULL || buflen == 0) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Handle special format: Unix timestamp
    if (strcmp(format, "%s") == 0 || strcmp(format, XTIME_FMT_UNIX) == 0) {
        int written = snprintf(buf, buflen, "%lld", (long long)t->seconds);
        if (written < 0 || (size_t)written >= buflen) {
            return XTIME_ERR_BUFFER_TOO_SMALL;
        }
        return XTIME_OK;
    }

    // Convert to struct tm in UTC
    time_t timestamp = (time_t)t->seconds;
    struct tm tm_result;

    if (gmtime_r(&timestamp, &tm_result) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Format using strftime
    size_t result = strftime(buf, buflen, format, &tm_result);
    if (result == 0) {
        return XTIME_ERR_BUFFER_TOO_SMALL;
    }

    return XTIME_OK;
}

xtime_error_t xtime_to_json(const xtime_t* t, char* buf, size_t buflen) {
    if (t == NULL || buf == NULL || buflen == 0) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Adjust seconds for timezone if present for the breakdown
    time_t timestamp = (time_t)t->seconds;
    if (t->has_tz && t->tz_offset != 0) {
        timestamp += (time_t)(t->tz_offset * 60);
    }

    struct tm tm_val;
    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // 1. Format Date and Time: YYYY-MM-DDTHH:MM:SS
    int written = snprintf(buf, buflen, "%04d-%02d-%02dT%02d:%02d:%02d", tm_val.tm_year + 1900, tm_val.tm_mon + 1,
                           tm_val.tm_mday, tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);

    if (written < 0 || (size_t)written >= buflen) {
        return XTIME_ERR_BUFFER_TOO_SMALL;
    }
    size_t current_len = (size_t)written;

    // 2. Append Nanoseconds: .nnnnnnnnn
    // RFC 3339 allows fractional seconds. We print full precision.
    if (t->nanoseconds > 0) {
        written = snprintf(buf + current_len, buflen - current_len, ".%09u", t->nanoseconds);
        if (written < 0 || current_len + (size_t)written >= buflen) {
            return XTIME_ERR_BUFFER_TOO_SMALL;
        }
        current_len += (size_t)written;
    }

    // 3. Append Timezone: Z or ±HH:MM
    if (!t->has_tz || t->tz_offset == 0) {
        // UTC
        if (current_len + 1 >= buflen) return XTIME_ERR_BUFFER_TOO_SMALL;
        buf[current_len++] = 'Z';
        buf[current_len]   = '\0';
    } else {
        // Offset
        int hrs   = abs(t->tz_offset) / 60;
        int mins  = abs(t->tz_offset) % 60;
        char sign = (t->tz_offset >= 0) ? '+' : '-';

        written = snprintf(buf + current_len, buflen - current_len, "%c%02d:%02d", sign, hrs, mins);

        if (written < 0 || current_len + (size_t)written >= buflen) {
            return XTIME_ERR_BUFFER_TOO_SMALL;
        }
    }

    return XTIME_OK;
}

int64_t xtime_to_unix(const xtime_t* t) {
    if (t == NULL) {
        return -1;
    }
    return t->seconds;
}

xtime_error_t xtime_from_unix(int64_t timestamp, xtime_t* t) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    t->seconds     = timestamp;
    t->nanoseconds = 0;
    t->tz_offset   = 0;
    t->has_tz      = false;

    return XTIME_OK;
}

xtime_error_t xtime_add_seconds(xtime_t* t, int64_t seconds) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    t->seconds += seconds;
    return XTIME_OK;
}

int xtime_compare(const xtime_t* t1, const xtime_t* t2) {
    if (t1 == NULL || t2 == NULL) {
        return -2;
    }

    if (t1->seconds < t2->seconds) {
        return -1;
    }
    if (t1->seconds > t2->seconds) {
        return 1;
    }

    // Seconds are equal, compare nanoseconds
    if (t1->nanoseconds < t2->nanoseconds) {
        return -1;
    }
    if (t1->nanoseconds > t2->nanoseconds) {
        return 1;
    }

    return 0;
}

xtime_error_t xtime_diff(const xtime_t* t1, const xtime_t* t2, double* diff) {
    if (t1 == NULL || t2 == NULL || diff == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    int64_t sec_diff  = t1->seconds - t2->seconds;
    int64_t nano_diff = (int64_t)t1->nanoseconds - (int64_t)t2->nanoseconds;

    *diff = (double)sec_diff + ((double)nano_diff / (double)NANOS_PER_SEC);

    return XTIME_OK;
}

const char* xtime_strerror(xtime_error_t err) {
    switch (err) {
        case XTIME_OK:
            return "Success";
        case XTIME_ERR_INVALID_ARG:
            return "Invalid argument";
        case XTIME_ERR_PARSE_FAILED:
            return "Failed to parse time string";
        case XTIME_ERR_BUFFER_TOO_SMALL:
            return "Output buffer too small";
        case XTIME_ERR_INVALID_TIME:
            return "Invalid time value";
        case XTIME_ERR_SYSTEM:
            return "System error";
        default:
            return "Unknown error";
    }
}

// =============== Helpers ===========================

xtime_error_t xtime_add_nanoseconds(xtime_t* t, int64_t nanos) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    int64_t total_nanos = (int64_t)t->nanoseconds + nanos;

    // Handle overflow/underflow into seconds
    if (total_nanos >= NANOS_PER_SEC) {
        int64_t extra_secs = total_nanos / NANOS_PER_SEC;
        t->seconds += extra_secs;
        t->nanoseconds = (uint32_t)(total_nanos % NANOS_PER_SEC);
    } else if (total_nanos < 0) {
        int64_t borrow_secs = (-total_nanos + NANOS_PER_SEC - 1) / NANOS_PER_SEC;
        t->seconds -= borrow_secs;
        t->nanoseconds = (uint32_t)(total_nanos + (borrow_secs * NANOS_PER_SEC));
    } else {
        t->nanoseconds = (uint32_t)total_nanos;
    }

    return XTIME_OK;
}

xtime_error_t xtime_add_microseconds(xtime_t* t, int64_t micros) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    return xtime_add_nanoseconds(t, micros * NANOS_PER_MICRO);
}

xtime_error_t xtime_add_milliseconds(xtime_t* t, int64_t millis) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    return xtime_add_nanoseconds(t, millis * NANOS_PER_MILLI);
}

xtime_error_t xtime_add_minutes(xtime_t* t, int64_t minutes) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    return xtime_add_seconds(t, minutes * 60);
}

xtime_error_t xtime_add_hours(xtime_t* t, int64_t hours) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    return xtime_add_seconds(t, hours * 3600);
}

xtime_error_t xtime_add_days(xtime_t* t, int64_t days) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    return xtime_add_seconds(t, days * 86400);
}

xtime_error_t xtime_add_months(xtime_t* t, int months) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Store the time-of-day portion
    int64_t time_of_day_seconds = t->seconds % 86400;
    if (time_of_day_seconds < 0) {
        time_of_day_seconds += 86400;
    }

    // Get the date components
    time_t timestamp = (time_t)t->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Add months
    int total_months = tm_val.tm_mon + months;
    int year_adjust  = total_months / 12;
    int new_month    = total_months % 12;

    // Handle negative months
    if (new_month < 0) {
        new_month += 12;
        year_adjust -= 1;
    }

    int new_year = tm_val.tm_year + 1900 + year_adjust;

    // Adjust day if it exceeds the new month's length
    int max_day = days_in_month(new_year, new_month);
    int new_day = (tm_val.tm_mday > max_day) ? max_day : tm_val.tm_mday;

    // Calculate days since Unix epoch (1970-01-01)
    // This is more reliable than using mktime/timegm
    int64_t days = 0;

    // Add years
    for (int y = 1970; y < new_year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    // Add months
    for (int m = 0; m < new_month; m++) {
        days += days_in_month(new_year, m);
    }

    // Add days
    days += new_day - 1;  // -1 because day 1 is the first day

    // Reconstruct the timestamp
    t->seconds = (days * 86400) + time_of_day_seconds;

    return XTIME_OK;
}

xtime_error_t xtime_add_years(xtime_t* t, int years) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Store the time-of-day portion
    int64_t time_of_day_seconds = t->seconds % 86400;
    if (time_of_day_seconds < 0) {
        time_of_day_seconds += 86400;
    }

    // Get the date components
    time_t timestamp = (time_t)t->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    int new_year  = tm_val.tm_year + 1900 + years;
    int new_month = tm_val.tm_mon;
    int new_day   = tm_val.tm_mday;

    // Handle Feb 29 in non-leap years
    if (new_month == 1 && new_day == 29) {
        if (!is_leap_year(new_year)) {
            new_day = 28;
        }
    }

    // Calculate days since Unix epoch (1970-01-01)
    int64_t days = 0;

    // Add years
    for (int y = 1970; y < new_year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    // Add months
    for (int m = 0; m < new_month; m++) {
        days += days_in_month(new_year, m);
    }

    // Add days
    days += new_day - 1;  // -1 because day 1 is the first day

    // Reconstruct the timestamp
    t->seconds = (days * 86400) + time_of_day_seconds;

    return XTIME_OK;
}

xtime_error_t xtime_diff_nanos(const xtime_t* t1, const xtime_t* t2, int64_t* nanos) {
    if (t1 == NULL || t2 == NULL || nanos == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    int64_t sec_diff  = t1->seconds - t2->seconds;
    int64_t nano_diff = (int64_t)t1->nanoseconds - (int64_t)t2->nanoseconds;

    *nanos = (sec_diff * NANOS_PER_SEC) + nano_diff;

    return XTIME_OK;
}

xtime_error_t xtime_diff_micros(const xtime_t* t1, const xtime_t* t2, int64_t* micros) {
    if (t1 == NULL || t2 == NULL || micros == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    int64_t nanos;
    xtime_error_t err = xtime_diff_nanos(t1, t2, &nanos);
    if (err != XTIME_OK) {
        return err;
    }

    *micros = nanos / NANOS_PER_MICRO;

    return XTIME_OK;
}

xtime_error_t xtime_diff_millis(const xtime_t* t1, const xtime_t* t2, int64_t* millis) {
    if (t1 == NULL || t2 == NULL || millis == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    int64_t nanos;
    xtime_error_t err = xtime_diff_nanos(t1, t2, &nanos);
    if (err != XTIME_OK) {
        return err;
    }

    *millis = nanos / NANOS_PER_MILLI;

    return XTIME_OK;
}

xtime_error_t xtime_diff_seconds(const xtime_t* t1, const xtime_t* t2, int64_t* seconds) {
    if (t1 == NULL || t2 == NULL || seconds == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *seconds = t1->seconds - t2->seconds;

    return XTIME_OK;
}

xtime_error_t xtime_diff_minutes(const xtime_t* t1, const xtime_t* t2, int64_t* minutes) {
    if (t1 == NULL || t2 == NULL || minutes == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *minutes = (t1->seconds - t2->seconds) / 60;

    return XTIME_OK;
}

xtime_error_t xtime_diff_hours(const xtime_t* t1, const xtime_t* t2, int64_t* hours) {
    if (t1 == NULL || t2 == NULL || hours == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *hours = (t1->seconds - t2->seconds) / 3600;

    return XTIME_OK;
}

xtime_error_t xtime_diff_days(const xtime_t* t1, const xtime_t* t2, int64_t* days) {
    if (t1 == NULL || t2 == NULL || days == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *days = (t1->seconds - t2->seconds) / 86400;

    return XTIME_OK;
}

bool xtime_is_leap_year(const xtime_t* t) {
    if (t == NULL) {
        return false;
    }

    time_t timestamp = (time_t)t->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return false;
    }

    return is_leap_year(tm_val.tm_year + 1900);
}

xtime_error_t xtime_truncate_to_second(xtime_t* t) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    t->nanoseconds = 0;

    return XTIME_OK;
}

xtime_error_t xtime_truncate_to_minute(xtime_t* t) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Truncate to the start of the current minute
    // Remove seconds and nanoseconds
    t->seconds     = (t->seconds / 60) * 60;
    t->nanoseconds = 0;

    return XTIME_OK;
}

xtime_error_t xtime_truncate_to_hour(xtime_t* t) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Truncate to the start of the current hour
    // Remove minutes, seconds, and nanoseconds
    t->seconds     = (t->seconds / 3600) * 3600;
    t->nanoseconds = 0;

    return XTIME_OK;
}

xtime_error_t xtime_truncate_to_day(xtime_t* t) {
    if (t == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    // Truncate to the start of the current day (00:00:00 UTC)
    // Remove hours, minutes, seconds, and nanoseconds
    t->seconds     = (t->seconds / 86400) * 86400;
    t->nanoseconds = 0;

    return XTIME_OK;
}

xtime_error_t xtime_start_of_week(const xtime_t* t, xtime_t* result) {
    if (t == NULL || result == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *result = *t;

    // First truncate to start of day
    xtime_error_t err = xtime_truncate_to_day(result);
    if (err != XTIME_OK) {
        return err;
    }

    // Get the day of week
    time_t timestamp = (time_t)result->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Calculate days to subtract to get to Monday (tm_wday: 0=Sun, 1=Mon, ...)
    int days_to_monday = (tm_val.tm_wday == 0) ? 6 : (tm_val.tm_wday - 1);

    // Subtract days to get to Monday
    return xtime_add_days(result, -days_to_monday);
}

xtime_error_t xtime_start_of_month(const xtime_t* t, xtime_t* result) {
    if (t == NULL || result == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *result = *t;

    time_t timestamp = (time_t)result->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Calculate how many days to subtract to get to day 1
    int days_to_subtract = tm_val.tm_mday - 1;

    // First truncate to start of current day
    xtime_error_t err = xtime_truncate_to_day(result);
    if (err != XTIME_OK) {
        return err;
    }

    // Then subtract days to get to the 1st
    return xtime_add_days(result, -days_to_subtract);
}

xtime_error_t xtime_start_of_year(const xtime_t* t, xtime_t* result) {
    if (t == NULL || result == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *result = *t;

    time_t timestamp = (time_t)result->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Calculate days since January 1st (tm_yday is 0-based)
    int days_to_subtract = tm_val.tm_yday;

    // First truncate to start of current day
    xtime_error_t err = xtime_truncate_to_day(result);
    if (err != XTIME_OK) {
        return err;
    }

    // Then subtract days to get to Jan 1
    return xtime_add_days(result, -days_to_subtract);
}

xtime_error_t xtime_end_of_day(const xtime_t* t, xtime_t* result) {
    if (t == NULL || result == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *result = *t;

    // Truncate to start of day
    xtime_error_t err = xtime_truncate_to_day(result);
    if (err != XTIME_OK) {
        return err;
    }

    // Add 86399 seconds (23:59:59) and 999999999 nanoseconds
    result->seconds += 86399;
    result->nanoseconds = 999999999;

    return XTIME_OK;
}

xtime_error_t xtime_end_of_month(const xtime_t* t, xtime_t* result) {
    if (t == NULL || result == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *result = *t;

    time_t timestamp = (time_t)result->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Get last day of month
    int last_day = days_in_month(tm_val.tm_year + 1900, tm_val.tm_mon);

    // Calculate days to add to get to last day
    int days_to_add = last_day - tm_val.tm_mday;

    // Truncate to start of current day
    xtime_error_t err = xtime_truncate_to_day(result);
    if (err != XTIME_OK) {
        return err;
    }

    // Add days to get to last day
    err = xtime_add_days(result, days_to_add);
    if (err != XTIME_OK) {
        return err;
    }

    // Set to end of day (23:59:59.999999999)
    result->seconds += 86399;
    result->nanoseconds = 999999999;

    return XTIME_OK;
}

xtime_error_t xtime_end_of_year(const xtime_t* t, xtime_t* result) {
    if (t == NULL || result == NULL) {
        return XTIME_ERR_INVALID_ARG;
    }

    *result = *t;

    time_t timestamp = (time_t)result->seconds;
    struct tm tm_val;

    if (gmtime_r(&timestamp, &tm_val) == NULL) {
        return XTIME_ERR_INVALID_TIME;
    }

    // Calculate if leap year
    int year               = tm_val.tm_year + 1900;
    int total_days_in_year = is_leap_year(year) ? 366 : 365;

    // Calculate days to add to get to Dec 31
    int days_to_add = (total_days_in_year - 1) - tm_val.tm_yday;

    // Truncate to start of current day
    xtime_error_t err = xtime_truncate_to_day(result);
    if (err != XTIME_OK) {
        return err;
    }

    // Add days to get to Dec 31
    err = xtime_add_days(result, days_to_add);
    if (err != XTIME_OK) {
        return err;
    }

    // Set to end of day (23:59:59.999999999)
    result->seconds += 86399;
    result->nanoseconds = 999999999;

    return XTIME_OK;
}
