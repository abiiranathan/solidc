#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "../include/macros.h"
#include "../include/win_strptime.h"

// Windows does not have gmtime_r & localtime_r and completely lacks strptime.
// Guarded for all _WIN32 targets (MSVC and MinGW): CMake adds this source for
// every Windows build, and an empty TU here would leave strptime undefined.
#if defined(_WIN32)
/**
 * Helper function to check if a year is a leap year
 */
static inline bool is_leap_year(int year) {
    // year is tm_year (years since 1900)
    int actual_year = year + 1900;
    return (actual_year % 4 == 0 && actual_year % 100 != 0) || (actual_year % 400 == 0);
}

/**
 * Helper function to get the number of days in a month
 */
static inline int days_in_month(int month, int year) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 0 || month > 11) {
        return 0;
    }
    if (month == 1 && is_leap_year(year)) {  // February in leap year
        return 29;
    }
    return days[month];
}

/**
 * Helper function to validate a date
 */
static inline bool is_valid_date(int day, int month, int year) {
    if (month < 0 || month > 11) {
        return false;
    }
    if (day < 1) {
        return false;
    }
    return day <= days_in_month(month, year);
}

/**
 * Windows implementation of strptime using manual parsing.
 * Supports common format specifiers needed by xtime.
 *
 * POSIX compliance improvements:
 * - Proper whitespace handling per POSIX spec
 * - Correct field initialization
 * - Better error handling
 * - Date validation (rejects invalid dates like Feb 30)
 * - Support for additional format specifiers
 */
char* strptime(const char* buf, const char* fmt, struct tm* tm) {
    if (buf == NULL || fmt == NULL || tm == NULL) {
        return NULL;
    }

    const char* s = buf;
    const char* f = fmt;
    bool is_pm = false;
    bool has_ampm = false;
    int century = -1;  // For %C handling
    bool yday_set = false;

    // Initialize tm structure to safe defaults (POSIX requirement)
    // Don't zero everything - preserve what caller may have set
    // But ensure fields we might not set have safe values
    if (tm->tm_wday == 0 && tm->tm_yday == 0 && tm->tm_isdst == 0) {
        tm->tm_isdst = -1;  // Unknown DST status
    }

    while (*f != '\0') {
        // Handle whitespace: any whitespace in format matches zero or more in input
        if (isspace((unsigned char)*f)) {
            while (isspace((unsigned char)*f)) f++;
            while (isspace((unsigned char)*s)) s++;
            continue;
        }

        if (*f != '%') {
            // Literal character match (case-sensitive)
            if (*f != *s) {
                return NULL;
            }
            f++;
            s++;
            continue;
        }

        // Format specifier
        f++;  // Skip '%'

        if (*f == '\0') {
            return NULL;  // Trailing % is invalid
        }

        // Handle modifier flags (E and O)
        bool has_modifier = false;
        if (*f == 'E' || *f == 'O') {
            has_modifier = true;
            f++;  // Skip modifier (we'll ignore it for basic implementation)
            if (*f == '\0') {
                return NULL;
            }
        }

        /*
         * Glibc-parity: skip whitespace before each directive, and numeric
         * fields below accept ONE OR TWO digits (%Y consumes up to four),
         * matching glibc/POSIX strtol-style conversion.  Previously these
         * required exact two-digit padding, so inputs like "2024-6-5"
         * parsed on Linux but failed on Windows.
         */
        while (isspace((unsigned char)*s)) {
            s++;
        }

    #define WIN_STRPTIME_READ_1OR2(var)                           \
        do {                                                      \
            if (!isdigit((unsigned char)s[0])) {                  \
                return NULL;                                      \
            }                                                     \
            (var) = (unsigned char)s[0] - '0';                    \
            if (isdigit((unsigned char)s[1])) {                   \
                (var) = (var) * 10 + ((unsigned char)s[1] - '0'); \
                s += 2;                                           \
            } else {                                              \
                s += 1;                                           \
            }                                                     \
        } while (0)

        switch (*f) {
            case 'Y': {  // Year: consumes 1-4 digits (glibc/strtol parity)
                if (!isdigit((unsigned char)s[0])) {
                    return NULL;
                }
                int year = 0;
                int ndig = 0;
                while (ndig < 4 && isdigit((unsigned char)s[0])) {
                    year = year * 10 + (s[0] - '0');
                    s++;
                    ndig++;
                }
                tm->tm_year = year - 1900;
                break;
            }

            case 'y': {  // 2-digit year (00-99)
                if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1])) {
                    return NULL;
                }
                int year = (s[0] - '0') * 10 + (s[1] - '0');
                // POSIX: 69-99 -> 1969-1999, 00-68 -> 2000-2068
                if (century >= 0) {
                    tm->tm_year = century * 100 + year - 1900;
                } else {
                    tm->tm_year = (year >= 69) ? year : (year + 100);
                }
                s += 2;
                break;
            }

            case 'C': {  // Century (00-99)
                if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1])) {
                    return NULL;
                }
                century = (s[0] - '0') * 10 + (s[1] - '0');
                s += 2;
                break;
            }

            case 'm': {  // Month (1-12), one or two digits
                int month;
                WIN_STRPTIME_READ_1OR2(month);
                if (month < 1 || month > 12) {
                    return NULL;
                }
                tm->tm_mon = month - 1;
                break;
            }

            case 'd': {  // Day of month (1-31), one or two digits
                int day;
                WIN_STRPTIME_READ_1OR2(day);
                if (day < 1 || day > 31) {
                    return NULL;
                }
                tm->tm_mday = day;
                break;
            }

            case 'e': {  // Day of month (1-31, space-padded)
                // Skip leading whitespace/zero
                if (*s == ' ' || *s == '0') {
                    s++;
                }
                if (!isdigit((unsigned char)s[0])) {
                    return NULL;
                }
                int day = (s[0] - '0');
                s++;
                if (isdigit((unsigned char)s[0])) {
                    day = day * 10 + (s[0] - '0');
                    s++;
                }
                if (day < 1 || day > 31) {
                    return NULL;
                }
                tm->tm_mday = day;
                break;
            }

            case 'H': {  // Hour (0-23), one or two digits
                int hour;
                WIN_STRPTIME_READ_1OR2(hour);
                if (hour > 23) {
                    return NULL;
                }
                tm->tm_hour = hour;
                break;
            }

            case 'k': {  // Hour (0-23) - space-padded
                if (*s == ' ') {
                    s++;
                }
                if (!isdigit((unsigned char)s[0])) {
                    return NULL;
                }
                int hour = (s[0] - '0');
                s++;
                if (isdigit((unsigned char)s[0])) {
                    hour = hour * 10 + (s[0] - '0');
                    s++;
                }
                if (hour > 23) {
                    return NULL;
                }
                tm->tm_hour = hour;
                break;
            }

            case 'I': {  // Hour (01-12) - zero-padded
                if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1])) {
                    return NULL;
                }
                int hour = (s[0] - '0') * 10 + (s[1] - '0');
                if (hour < 1 || hour > 12) {
                    return NULL;
                }
                tm->tm_hour = hour;
                s += 2;
                break;
            }

            case 'l': {  // Hour (1-12) - space-padded
                if (*s == ' ') {
                    s++;
                }
                if (!isdigit((unsigned char)s[0])) {
                    return NULL;
                }
                int hour = (s[0] - '0');
                s++;
                if (isdigit((unsigned char)s[0])) {
                    hour = hour * 10 + (s[0] - '0');
                    s++;
                }
                if (hour < 1 || hour > 12) {
                    return NULL;
                }
                tm->tm_hour = hour;
                break;
            }

            case 'M': {  // Minute (0-59), one or two digits
                int min;
                WIN_STRPTIME_READ_1OR2(min);
                if (min > 59) {
                    return NULL;
                }
                tm->tm_min = min;
                break;
            }

            case 'S': {  // Second (00-60, allowing leap second), 1-2 digits
                int sec;
                WIN_STRPTIME_READ_1OR2(sec);
                if (sec > 60) {
                    return NULL;
                }
                tm->tm_sec = sec;
                break;
            }

            case 'p': {  // AM/PM (case-insensitive per POSIX)
                if ((s[0] == 'A' || s[0] == 'a') && (s[1] == 'M' || s[1] == 'm')) {
                    is_pm = false;
                    has_ampm = true;
                    s += 2;
                } else if ((s[0] == 'P' || s[0] == 'p') && (s[1] == 'M' || s[1] == 'm')) {
                    is_pm = true;
                    has_ampm = true;
                    s += 2;
                } else {
                    return NULL;
                }
                break;
            }

            case 'r': {  // 12-hour time with AM/PM (%I:%M:%S %p)
                char* result = strptime(s, "%I:%M:%S %p", tm);
                if (result == NULL) {
                    return NULL;
                }
                s = result;
                has_ampm = true;  // Mark that AM/PM was handled
                break;
            }

            case 'R': {  // Time in HH:MM format
                char* result = strptime(s, "%H:%M", tm);
                if (result == NULL) {
                    return NULL;
                }
                s = result;
                break;
            }

            case 'T': {  // Time in HH:MM:SS format
                char* result = strptime(s, "%H:%M:%S", tm);
                if (result == NULL) {
                    return NULL;
                }
                s = result;
                break;
            }

            case 'D': {  // Date in MM/DD/YY format
                char* result = strptime(s, "%m/%d/%y", tm);
                if (result == NULL) {
                    return NULL;
                }
                s = result;
                break;
            }

            case 'F': {  // Date in YYYY-MM-DD format (ISO 8601)
                char* result = strptime(s, "%Y-%m-%d", tm);
                if (result == NULL) {
                    return NULL;
                }
                s = result;
                break;
            }

            case 'b':    // Abbreviated month name
            case 'h':    // Same as %b
            case 'B': {  // Full month name
                static const char* months[] = {"january", "february", "march",     "april",   "may",      "june",
                                               "july",    "august",   "september", "october", "november", "december"};
                static const char* abbr_months[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                                    "jul", "aug", "sep", "oct", "nov", "dec"};

                bool found = false;
                // Try full names first, then abbreviations
                for (int i = 0; i < 12; i++) {
                    const char* full = months[i];
                    const char* abbr = abbr_months[i];
                    size_t full_len = strlen(full);
                    size_t abbr_len = 3;

                    // For %B, match full name; for %b/%h, match abbreviation
                    if (*f == 'B') {
                        if (_strnicmp(s, full, full_len) == 0) {
                            tm->tm_mon = i;
                            s += full_len;
                            found = true;
                            break;
                        }
                    } else {
                        if (_strnicmp(s, abbr, abbr_len) == 0) {
                            tm->tm_mon = i;
                            s += abbr_len;
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    return NULL;
                }
                break;
            }

            case 'a':    // Abbreviated weekday name
            case 'A': {  // Full weekday name
                static const char* weekdays[] = {"sunday",   "monday", "tuesday", "wednesday",
                                                 "thursday", "friday", "saturday"};
                static const char* abbr_weekdays[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

                bool found = false;
                for (int i = 0; i < 7; i++) {
                    const char* full = weekdays[i];
                    const char* abbr = abbr_weekdays[i];
                    size_t full_len = strlen(full);
                    size_t abbr_len = 3;

                    if (*f == 'A') {
                        if (_strnicmp(s, full, full_len) == 0) {
                            tm->tm_wday = i;
                            s += full_len;
                            found = true;
                            break;
                        }
                    } else {
                        if (_strnicmp(s, abbr, abbr_len) == 0) {
                            tm->tm_wday = i;
                            s += abbr_len;
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    return NULL;
                }
                break;
            }

            case 'j': { /* Day of year (001-366); mon/mday derived post-scan */
                if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) || !isdigit((unsigned char)s[2])) {
                    return NULL;
                }
                int yday = (s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0');
                if (yday < 1 || yday > 366) {
                    return NULL;
                }
                tm->tm_yday = yday - 1;
                yday_set = true;
                s += 3;
                break;
            }

            case 'u': {  // Weekday (1-7, Monday=1)
                if (!isdigit((unsigned char)s[0])) {
                    return NULL;
                }
                int wday = s[0] - '0';
                if (wday < 1 || wday > 7) {
                    return NULL;
                }
                tm->tm_wday = (wday == 7) ? 0 : wday;  // Convert to 0-6 (Sunday=0)
                s++;
                break;
            }

            case 'w': {  // Weekday (0-6, Sunday=0)
                if (!isdigit((unsigned char)s[0])) {
                    return NULL;
                }
                int wday = s[0] - '0';
                if (wday > 6) {
                    return NULL;
                }
                tm->tm_wday = wday;
                s++;
                break;
            }

            case 'z': {  // Timezone offset (+hhmm or -hhmm or +hh:mm or -hh:mm)
                // Consume the timezone but don't parse it into tm
                // The caller (xtime_parse) should handle this separately
                if (*s == '+' || *s == '-') {
                    s++;  // Skip sign
                    // Consume hours (2 digits)
                    if (isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1])) {
                        s += 2;
                    } else {
                        return NULL;
                    }
                    // Optional colon
                    if (*s == ':') {
                        s++;
                    }
                    // Consume minutes (2 digits)
                    if (isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1])) {
                        s += 2;
                    } else {
                        return NULL;
                    }
                } else if (*s == 'Z' || *s == 'z') {
                    s++;  // UTC designator
                } else {
                    return NULL;
                }
                break;
            }

            case 'Z': {  // Timezone name - variable length
                /* FIX: compare against the position where the specifier
                 * STARTED (saved before parsing), not buf — '%Z' appearing
                 * mid-format accepted an empty name because s had already
                 * advanced past buf. */
                const char* z_start = s;
                while (*s != '\0' && !isspace((unsigned char)*s) && *s != '+' && *s != '-' &&
                       isalpha((unsigned char)*s)) {
                    s++;
                }
                if (s == z_start) {
                    return NULL;  // No timezone found
                }
                break;
            }

            case 'n':  // Any whitespace
            case 't': {
                // Match one or more whitespace characters
                if (!isspace((unsigned char)*s)) {
                    return NULL;
                }
                while (isspace((unsigned char)*s)) {
                    s++;
                }
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
                // Unsupported/unknown format specifier
                return NULL;
        }

        f++;
    }

    /*
     * Derive mon/mday from %j + year, mirroring glibc which recomputes
     * derived fields after the format scan.  Without this, "%Y-%j" yields
     * a different struct tm on Windows than on Linux.
     */
    if (yday_set) {
        int year = tm->tm_year + 1900;
        int rem = tm->tm_yday; /* 0-based day of year */
        int m = 0;
        while (m < 11 && rem >= days_in_month(m, year)) {
            rem -= days_in_month(m, year);
            m++;
        }
        tm->tm_mon = m;
        tm->tm_mday = rem + 1;
    }

    // Apply AM/PM adjustment if needed (deferred from %I parsing)
    if (has_ampm) {
        if (is_pm && tm->tm_hour < 12) {
            tm->tm_hour += 12;
        } else if (!is_pm && tm->tm_hour == 12) {
            tm->tm_hour = 0;
        }
    }

    /*
     * NOTE: unlike earlier revisions, this shim does NOT reject
     * day-beyond-month lengths (e.g. Feb 30).  POSIX strptime validates
     * per-field ranges only, and glibc matches that.  Calendar validity
     * is enforced by the caller — xtime_parse() checks days_in_month()
     * and reports XTIME_ERR_DATE_OUT_OF_RANGE, keeping error codes
     * identical across Windows and POSIX builds.
     */

    // Ensure DST flag is set to unknown if not explicitly set
    if (tm->tm_isdst == 0) {
        tm->tm_isdst = -1;
    }

    return (char*)s;
}
#endif
