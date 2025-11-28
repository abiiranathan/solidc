#include "../include/xtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// LOGGING UTILITIES
// ============================================================================

// ANSI Color codes
#define COLOR_RED    "\033[0;31m"
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_CYAN   "\033[0;36m"
#define COLOR_RESET  "\033[0m"

/**
 * @brief Logs an error message.
 * Usage: LOG_ERROR("Message"); or LOG_ERROR("Value: %d", val);
 */
#define LOG_ERROR(...)                                                                                                 \
    do {                                                                                                               \
        fprintf(stderr, COLOR_RED "[ERROR]: %s:%d:%s(): ", __FILE__, __LINE__, __func__);                              \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
        fprintf(stderr, COLOR_RESET "\n");                                                                             \
    } while (0)

/**
 * @brief Logs info message indented.
 */
#define LOG_INFO(...)                                                                                                  \
    do {                                                                                                               \
        printf("    ");                                                                                                \
        printf(__VA_ARGS__);                                                                                           \
        printf("\n");                                                                                                  \
    } while (0)

/**
 * @brief Asserts condition is true. If false, prints error and exits.
 * FIX: We pass #condition as a %s argument so that % characters inside the
 * code (like date formats) are not interpreted as printf specifiers.
 */
#define LOG_ASSERT(condition, ...)                                                                                     \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, COLOR_RED "[ERROR]: %s:%d:%s(): Assertion failed: (%s) ", __FILE__, __LINE__, __func__,    \
                    #condition);                                                                                       \
            fprintf(stderr, __VA_ARGS__);                                                                              \
            fprintf(stderr, COLOR_RESET "\n");                                                                         \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

#define LOG_SECTION(name) printf("\n" COLOR_CYAN "=== %s ===" COLOR_RESET "\n", name)

/**
 * @brief Macro to run a test function with automatic logging
 */
#define RUN_TEST(test_func)                                                                                            \
    do {                                                                                                               \
        printf("  Running %-45s ... ", #test_func);                                                                    \
        fflush(stdout);                                                                                                \
        test_func();                                                                                                   \
        printf(COLOR_GREEN "PASSED" COLOR_RESET "\n");                                                                 \
    } while (0)

// ============================================================================
// TEST CASES
// ============================================================================

void test_initialization(void) {
    xtime_t t;
    // Fill with garbage first to ensure init cleans it
    memset(&t, 0xFF, sizeof(xtime_t));

    xtime_error_t err = xtime_init(&t);
    LOG_ASSERT(err == XTIME_OK, "xtime_init returned error");
    LOG_ASSERT(t.seconds == 0, "Seconds should be 0");
    LOG_ASSERT(t.nanoseconds == 0, "Nanoseconds should be 0");
    LOG_ASSERT(t.tz_offset == 0, "Timezone offset should be 0");
    LOG_ASSERT(t.has_tz == false, "Should not have timezone info");

    // Test Null Pointer
    err = xtime_init(NULL);
    LOG_ASSERT(err == XTIME_ERR_INVALID_ARG, "Should return invalid arg for null pointer");
}

void test_unix_conversion(void) {
    xtime_t t;
    int64_t expected_ts = 1700000000;  // 2023-11-14 @ 10:13:20 UTC

    // 1. From Unix
    xtime_error_t err = xtime_from_unix(expected_ts, &t);
    LOG_ASSERT(err == XTIME_OK, "xtime_from_unix failed");
    LOG_ASSERT(t.seconds == expected_ts, "Timestamp mismatch");
    LOG_ASSERT(t.nanoseconds == 0, "Nanos should be 0 after unix import");

    // 2. To Unix
    int64_t result_ts = xtime_to_unix(&t);
    LOG_ASSERT(result_ts == expected_ts, "xtime_to_unix mismatch");

    // 3. Negative Unix Timestamp (Pre-1970)
    int64_t pre_epoch_ts = -1000;
    xtime_from_unix(pre_epoch_ts, &t);
    LOG_ASSERT(xtime_to_unix(&t) == pre_epoch_ts, "Negative timestamp roundtrip failed");

    // 4. Null checks
    LOG_ASSERT(xtime_to_unix(NULL) == -1, "Should return -1 for null input");
    LOG_ASSERT(xtime_from_unix(100, NULL) == XTIME_ERR_INVALID_ARG, "Should return error for null pointer");
}

void test_now(void) {
    xtime_t t1, t2;

    // 1. Basic Monotonicity check
    LOG_ASSERT(xtime_now(&t1) == XTIME_OK, "xtime_now failed");

    // Busy wait slightly to ensure clock ticks
    volatile int k = 0;
    for (int i = 0; i < 100000; i++)
        k++;

    LOG_ASSERT(xtime_now(&t2) == XTIME_OK, "xtime_now failed second call");

    int cmp = xtime_compare(&t2, &t1);
    LOG_ASSERT(cmp >= 0, "Time went backwards! t2 should be >= t1");

    // 2. Sanity check (Current year should be > 2020)
    LOG_ASSERT(t1.seconds > 1577836800, "System time seems implausibly old (pre-2020)");

    // 3. UTC check
    xtime_t t_utc;
    LOG_ASSERT(xtime_utc_now(&t_utc) == XTIME_OK, "xtime_now_utc failed");
    LOG_ASSERT(t_utc.has_tz == false, "UTC time should not have tz flag set (implicitly 0)");
    LOG_ASSERT(t_utc.tz_offset == 0, "UTC offset should be 0");
}

void test_parsing_valid(void) {
    xtime_t t;
    xtime_error_t err;

    // 1. Standard ISO8601
    const char* iso = "2023-12-25T15:30:00";
    err             = xtime_parse(iso, XTIME_FMT_ISO8601, &t);
    LOG_ASSERT(err == XTIME_OK, "Failed to parse valid ISO8601");

    char buf[64];
    xtime_format_utc(&t, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2023-12-25") == 0, "Date parsed incorrectly");

    // 2. Date with explicit Timezone (Note: implementation parses TZ from string tail)
    // The format string consumes the time, parse_tz_offset handles the rest
    const char* with_tz = "2023-12-25T15:30:00+02:00";
    err                 = xtime_parse(with_tz, XTIME_FMT_ISO8601, &t);
    LOG_ASSERT(err == XTIME_OK, "Failed to parse ISO8601 with TZ");
    LOG_ASSERT(t.has_tz == true, "Should detect timezone");
    LOG_ASSERT(t.tz_offset == 120, "Offset should be +120 minutes");

    // 3. HTTP Format
    const char* http = "Thu, 28 Nov 2024 14:30:00 GMT";
    err              = xtime_parse(http, XTIME_FMT_HTTP, &t);
    LOG_ASSERT(err == XTIME_OK, "Failed to parse HTTP format");
}

void test_parsing_invalid(void) {
    xtime_t t;

    // 1. Garbage string
    LOG_ASSERT(xtime_parse("not-a-date", XTIME_FMT_ISO8601, &t) == XTIME_ERR_PARSE_FAILED,
               "Garbage string should fail");

    // 2. Format mismatch
    LOG_ASSERT(xtime_parse("2023-01-01", "%H:%M:%S", &t) == XTIME_ERR_PARSE_FAILED, "Format mismatch should fail");

    // 3. Null args
    LOG_ASSERT(xtime_parse(NULL, "%s", &t) == XTIME_ERR_INVALID_ARG, "Null str");
    LOG_ASSERT(xtime_parse("2023", NULL, &t) == XTIME_ERR_INVALID_ARG, "Null fmt");
    LOG_ASSERT(xtime_parse("2023", "%Y", NULL) == XTIME_ERR_INVALID_ARG, "Null struct");
}

void test_formatting(void) {
    xtime_t t;
    char buf[256];
    xtime_init(&t);

    // Base timestamp: 2024-01-01 00:00:00 UTC (Monday)
    t.seconds     = 1704067200;
    t.nanoseconds = 123456789;

    printf("=== Testing Time Formatting ===\n");

    // ============================================================================
    // 1. BASIC DATE FORMATS (UTC)
    // ============================================================================
    printf("\n--- Basic Date Formats (UTC) ---\n");

    xtime_format_utc(&t, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-01") == 0, "%%Y-%%m-%%d failed. Got: %s", buf);

    xtime_format_utc(&t, "%Y/%m/%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024/01/01") == 0, "%%Y/%%m/%%d failed. Got: %s", buf);

    xtime_format_utc(&t, "%m/%d/%Y", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "01/01/2024") == 0, "%%m/%%d/%%Y (US format) failed. Got: %s", buf);

    xtime_format_utc(&t, "%d.%m.%Y", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "01.01.2024") == 0, "%%d.%%m.%%Y (EU format) failed. Got: %s", buf);

    xtime_format_utc(&t, "%F", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-01") == 0, "%%F (ISO 8601 date) failed. Got: %s", buf);

    // ============================================================================
    // 2. BASIC TIME FORMATS (UTC)
    // ============================================================================
    printf("\n--- Basic Time Formats (UTC) ---\n");

    xtime_format_utc(&t, "%H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "00:00:00") == 0, "%%H:%%M:%%S failed. Got: %s", buf);

    xtime_format_utc(&t, "%T", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "00:00:00") == 0, "%%T (time shorthand) failed. Got: %s", buf);

    // Test with different hour
    t.seconds = 1704067200 + (13 * 3600) + (45 * 60) + 30;  // 13:45:30 UTC
    xtime_format_utc(&t, "%H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "13:45:30") == 0, "%%H:%%M:%%S (afternoon) failed. Got: %s", buf);

    xtime_format_utc(&t, "%I:%M:%S %p", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "01:45:30 PM") == 0, "%%I:%%M:%%S %%p (12-hour) failed. Got: %s", buf);

    // Test midnight in 12-hour format
    t.seconds = 1704067200;  // 00:00:00 UTC
    xtime_format_utc(&t, "%I:%M %p", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "12:00 AM") == 0, "%%I:%%M %%p (midnight) failed. Got: %s", buf);

    // Test noon in 12-hour format
    t.seconds = 1704067200 + (12 * 3600);  // 12:00:00 UTC
    xtime_format_utc(&t, "%I:%M %p", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "12:00 PM") == 0, "%%I:%%M %%p (noon) failed. Got: %s", buf);

    // ============================================================================
    // 3. COMBINED DATE AND TIME FORMATS (UTC)
    // ============================================================================
    printf("\n--- Combined Date/Time Formats (UTC) ---\n");

    t.seconds = 1704067200;  // Reset to 2024-01-01 00:00:00

    xtime_format_utc(&t, "%Y-%m-%d %H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-01 00:00:00") == 0, "ISO datetime failed. Got: %s", buf);

    xtime_format_utc(&t, "%Y-%m-%dT%H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-01T00:00:00") == 0, "ISO 8601 format failed. Got: %s", buf);

    xtime_format_utc(&t, "%a, %d %b %Y %H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "Mon, 01 Jan 2024 00:00:00") == 0, "RFC 2822 style failed. Got: %s", buf);

    xtime_format_utc(&t, "%A, %B %d, %Y %I:%M %p", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "Monday, January 01, 2024 12:00 AM") == 0, "Long format failed. Got: %s", buf);

    // ============================================================================
    // 4. WEEKDAY FORMATS
    // ============================================================================
    printf("\n--- Weekday Formats ---\n");

    xtime_format_utc(&t, "%a", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "Mon") == 0, "%%a (abbreviated weekday) failed. Got: %s", buf);

    xtime_format_utc(&t, "%A", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "Monday") == 0, "%%A (full weekday) failed. Got: %s", buf);

    xtime_format_utc(&t, "%w", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "1") == 0, "%%w (weekday number) failed. Got: %s", buf);

    // Test Sunday (weekday 0)
    t.seconds = 1704067200 + (6 * 86400);  // 2024-01-07 (Sunday)
    xtime_format_utc(&t, "%w %A", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "0 Sunday") == 0, "%%w %%A (Sunday) failed. Got: %s", buf);

    // ============================================================================
    // 5. MONTH FORMATS
    // ============================================================================
    printf("\n--- Month Formats ---\n");

    t.seconds = 1704067200;  // Reset to January

    xtime_format_utc(&t, "%b", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "Jan") == 0, "%%b (abbreviated month) failed. Got: %s", buf);

    xtime_format_utc(&t, "%B", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "January") == 0, "%%B (full month) failed. Got: %s", buf);

    xtime_format_utc(&t, "%m", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "01") == 0, "%%m (month number) failed. Got: %s", buf);

    // Test December
    // January has 31 days, so Dec 1 = Jan 1 + (31-1 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30)
    // Or simply: 2024-12-01 is 335 days after 2024-01-01
    t.seconds = 1704067200 + (335 * 86400);  // 2024-12-01
    xtime_format_utc(&t, "%m %B", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "12 December") == 0, "%%m %%B (December) failed. Got: %s", buf);

    // ============================================================================
    // 6. YEAR FORMATS
    // ============================================================================
    printf("\n--- Year Formats ---\n");

    t.seconds = 1704067200;  // 2024-01-01

    xtime_format_utc(&t, "%Y", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024") == 0, "%%Y (4-digit year) failed. Got: %s", buf);

    xtime_format_utc(&t, "%y", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "24") == 0, "%%y (2-digit year) failed. Got: %s", buf);

    xtime_format_utc(&t, "%C", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "20") == 0, "%%C (century) failed. Got: %s", buf);

    // ============================================================================
    // 7. DAY OF YEAR
    // ============================================================================
    printf("\n--- Day of Year ---\n");

    t.seconds = 1704067200;  // 2024-01-01 (day 1)
    xtime_format_utc(&t, "%j", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "001") == 0, "%%j (day 1) failed. Got: %s", buf);

    t.seconds = 1704067200 + (31 * 86400);  // 2024-02-01 (day 32)
    xtime_format_utc(&t, "%j", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "032") == 0, "%%j (day 32) failed. Got: %s", buf);

    t.seconds = 1704067200 + (365 * 86400);  // 2024-12-31 (day 366, leap year)
    xtime_format_utc(&t, "%j", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "366") == 0, "%%j (day 366, leap year) failed. Got: %s", buf);

    // ============================================================================
    // 8. UNIX TIMESTAMP
    // ============================================================================
    printf("\n--- Unix Timestamp ---\n");

    t.seconds = 1704067200;
    xtime_format_utc(&t, "%s", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "1704067200") == 0, "%%s (unix timestamp) failed. Got: %s", buf);

    xtime_format_utc(&t, XTIME_FMT_UNIX, buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "1704067200") == 0, "XTIME_FMT_UNIX failed. Got: %s", buf);

    // ============================================================================
    // 9. TIMEZONE FORMATTING (WITH OFFSET)
    // ============================================================================
    printf("\n--- Timezone Formatting ---\n");

    t.seconds     = 1704067200;  // 2024-01-01 00:00:00 UTC
    t.nanoseconds = 0;
    t.has_tz      = true;
    t.tz_offset   = 330;  // +05:30 (IST)

    // Time should be adjusted: 00:00 + 5:30 = 05:30
    xtime_format(&t, "%H:%M", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "05:30") == 0, "Timezone adjustment failed. Got: %s", buf);

    // Full datetime with timezone
    xtime_format(&t, "%Y-%m-%d %H:%M:%S %z", buf, sizeof(buf));
    LOG_ASSERT(strstr(buf, "2024-01-01 05:30:00") != NULL, "Datetime with TZ adjustment failed. Got: %s", buf);
    LOG_ASSERT(strstr(buf, "+05:30") != NULL, "Timezone string not appended. Got: %s", buf);

    // Negative timezone offset
    t.tz_offset = -300;  // -05:00 (EST)
    xtime_format(&t, "%Y-%m-%d %H:%M %z", buf, sizeof(buf));
    LOG_ASSERT(strstr(buf, "2023-12-31 19:00") != NULL, "Negative TZ adjustment failed. Got: %s", buf);
    LOG_ASSERT(strstr(buf, "-05:00") != NULL, "Negative timezone string failed. Got: %s", buf);

    // UTC (zero offset)
    t.tz_offset = 0;
    xtime_format(&t, "%Y-%m-%d %H:%M:%S %z", buf, sizeof(buf));
    LOG_ASSERT(strstr(buf, "2024-01-01 00:00:00") != NULL, "UTC (zero offset) adjustment failed. Got: %s", buf);
    LOG_ASSERT(strstr(buf, "+00:00") != NULL, "UTC timezone string failed. Got: %s", buf);

    // ============================================================================
    // 10. SPECIAL CHARACTERS AND LITERALS
    // ============================================================================
    printf("\n--- Special Characters and Literals ---\n");

    t.seconds = 1704067200;
    t.has_tz  = false;

    xtime_format_utc(&t, "%%", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "%") == 0, "%%%% (literal percent) failed. Got: %s", buf);

    xtime_format_utc(&t, "%n", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "\n") == 0, "%%n (newline) failed. Got: %s", buf);

    xtime_format_utc(&t, "%t", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "\t") == 0, "%%t (tab) failed. Got: %s", buf);

    xtime_format_utc(&t, "Date: %Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "Date: 2024-01-01") == 0, "Format with literal text failed. Got: %s", buf);

    // ============================================================================
    // 11. EDGE CASES
    // ============================================================================
    printf("\n--- Edge Cases ---\n");

    // Leap year day (Feb 29, 2024)
    t.seconds = 1704067200 + (59 * 86400);  // 2024-02-29
    xtime_format_utc(&t, "%Y-%m-%d (%A)", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-02-29 (Thursday)") == 0, "Leap year date failed. Got: %s", buf);

    // Year boundary
    t.seconds = 1704067200 - 1;  // 2023-12-31 23:59:59
    xtime_format_utc(&t, "%Y-%m-%d %H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2023-12-31 23:59:59") == 0, "Year boundary failed. Got: %s", buf);

    // Empty format string
    xtime_format_utc(&t, "", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "") == 0, "Empty format failed. Got: %s", buf);

    // Very long format string
    t.seconds = 1704067200;
    xtime_format_utc(&t, "%Y-%m-%d %H:%M:%S %Y-%m-%d %H:%M:%S %Y-%m-%d %H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strlen(buf) > 0, "Long format string failed");

    // ============================================================================
    // 12. BUFFER SIZE EDGE CASES
    // ============================================================================
    printf("\n--- Buffer Size Edge Cases ---\n");

    char small_buf[11] = {0};
    xtime_error_t err;

    err = xtime_format_utc(&t, "%Y-%m-%d", small_buf, sizeof(small_buf));
    LOG_ASSERT(err == XTIME_OK, "Small buffer for short format should succeed");
    LOG_ASSERT(strcmp(small_buf, "2024-01-01") == 0, "Small buffer content wrong. Got: %s", small_buf);

    err = xtime_format_utc(&t, "%Y-%m-%d %H:%M:%S", small_buf, sizeof(small_buf));
    LOG_ASSERT(err == XTIME_ERR_BUFFER_TOO_SMALL, "Small buffer should fail for long format");

    // ============================================================================
    // 13. JSON FORMAT
    // ============================================================================
    printf("\n--- JSON/ISO 8601 Format ---\n");

    t.seconds     = 1704067200;
    t.nanoseconds = 123456789;
    t.has_tz      = false;

    xtime_to_json(&t, buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-01T00:00:00.123456789Z") == 0, "JSON UTC format failed. Got: %s", buf);

    t.has_tz    = true;
    t.tz_offset = 330;  // +05:30
    xtime_to_json(&t, buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-01T05:30:00.123456789+05:30") == 0, "JSON with timezone failed. Got: %s", buf);

    t.nanoseconds = 0;
    xtime_to_json(&t, buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-01T05:30:00+05:30") == 0, "JSON without nanos failed. Got: %s", buf);

    // ============================================================================
    // 14. WEEK NUMBER
    // ============================================================================
    printf("\n--- Week Number ---\n");

    t.seconds = 1704067200;  // 2024-01-01 (Monday, Week 1)
    t.has_tz  = false;

    xtime_format_utc(&t, "%U", buf, sizeof(buf));  // Week number (Sunday start)
    LOG_ASSERT(strcmp(buf, "00") == 0, "%%U (week, Sunday start) failed. Got: %s", buf);

    xtime_format_utc(&t, "%W", buf, sizeof(buf));  // Week number (Monday start)
    LOG_ASSERT(strcmp(buf, "01") == 0, "%%W (week, Monday start) failed. Got: %s", buf);

    printf("\n=== All Formatting Tests Passed ===\n");
}

void test_buffer_safety(void) {
    xtime_t t;
    xtime_now(&t);
    char small_buf[4];  // Too small for "YYYY"

    // 1. Buffer too small
    xtime_error_t err = xtime_format(&t, "%Y-%m-%d", small_buf, sizeof(small_buf));
    LOG_ASSERT(err == XTIME_ERR_BUFFER_TOO_SMALL, "Should detect small buffer");

    // 2. Exact fit (remember null terminator)
    char year_buf[5];  // "YYYY\0"
    err = xtime_format(&t, "%Y", year_buf, sizeof(year_buf));
    LOG_ASSERT(err == XTIME_OK, "Exact buffer fit should work");
}

void test_arithmetic(void) {
    xtime_t t;
    xtime_from_unix(1000, &t);

    // 1. Add positive
    xtime_add_seconds(&t, 60);
    LOG_ASSERT(t.seconds == 1060, "Addition failed");

    // 2. Add negative (Subtract)
    xtime_add_seconds(&t, -70);
    LOG_ASSERT(t.seconds == 990, "Subtraction failed");

    // 3. Null check
    LOG_ASSERT(xtime_add_seconds(NULL, 10) == XTIME_ERR_INVALID_ARG, "Null check failed");
}

void test_comparison(void) {
    xtime_t t1, t2;
    xtime_from_unix(1000, &t1);
    xtime_from_unix(1000, &t2);
    t1.nanoseconds = 500;
    t2.nanoseconds = 500;

    // 1. Equal
    LOG_ASSERT(xtime_compare(&t1, &t2) == 0, "Equality failed");

    // 2. Seconds differ
    t2.seconds = 1001;
    LOG_ASSERT(xtime_compare(&t1, &t2) == -1, "Less than failed (seconds)");
    LOG_ASSERT(xtime_compare(&t2, &t1) == 1, "Greater than failed (seconds)");

    // 3. Nanoseconds differ
    t2.seconds     = 1000;
    t2.nanoseconds = 600;
    LOG_ASSERT(xtime_compare(&t1, &t2) == -1, "Less than failed (nanos)");
    LOG_ASSERT(xtime_compare(&t2, &t1) == 1, "Greater than failed (nanos)");

    // 4. Null inputs
    LOG_ASSERT(xtime_compare(NULL, &t2) == -2, "Null comparison should return -2");
}

void test_diff(void) {
    xtime_t start, end;
    double diff;

    // 1. Simple Difference
    xtime_from_unix(1000, &start);
    start.nanoseconds = 0;

    xtime_from_unix(1002, &end);
    end.nanoseconds = 500000000;  // 0.5s

    // End - Start = 2.5 seconds
    xtime_diff(&end, &start, &diff);
    LOG_ASSERT(fabs(diff - 2.5) < 0.000001, "Difference calculation incorrect. Expected 2.5, got %f", diff);

    // 2. Negative Difference
    xtime_diff(&start, &end, &diff);
    LOG_ASSERT(fabs(diff - (-2.5)) < 0.000001, "Negative difference incorrect. Expected -2.5, got %f", diff);
}

void test_timezone_parsing_logic(void) {
    // This specifically targets the internal parsing of valid/invalid TZ strings
    xtime_t t;
    const char* base_fmt = "%Y-%m-%dT%H:%M:%S";

    // Case A: Valid Z
    LOG_ASSERT(xtime_parse("2024-01-01T12:00:00Z", base_fmt, &t) == XTIME_OK, "Z parsing");
    LOG_ASSERT(t.has_tz && t.tz_offset == 0, "Z should imply offset 0");

    // Case B: Valid +HH:MM
    LOG_ASSERT(xtime_parse("2024-01-01T12:00:00+05:30", base_fmt, &t) == XTIME_OK, "Pos Offset");
    LOG_ASSERT(t.tz_offset == 330, "Offset +05:30 should be 330 mins");

    // Case C: Valid -HH (no minutes)
    // The implementation helper logic: if (*str == ':') str++; parse minutes.
    // So "2024...-05" implies hours=5, minutes=0.
    LOG_ASSERT(xtime_parse("2024-01-01T12:00:00-05", base_fmt, &t) == XTIME_OK, "Short Offset");
    LOG_ASSERT(t.tz_offset == -300, "Offset -05 should be -300 mins");

    // Case D: Invalid Range (>23 hours)
    LOG_ASSERT(xtime_parse("2024-01-01T12:00:00+25:00", base_fmt, &t) == XTIME_OK,
               "Base parse succeeds, but TZ invalid");
    // If TZ parsing fails, has_tz should remain false (initialized to false by xtime_init inside parse)
    LOG_ASSERT(t.has_tz == false, "Invalid TZ range should be ignored/fail silently in parser");
}

void test_json_formatting(void) {
    xtime_t t;
    char buf[64];
    xtime_error_t err;

    // 1. Setup specific time: 2023-10-05 14:30:00 UTC, 123456789 ns
    xtime_init(&t);
    t.seconds     = 1696516200;
    t.nanoseconds = 123456789;

    // Test UTC Output
    err = xtime_to_json(&t, buf, sizeof(buf));
    LOG_ASSERT(err == XTIME_OK, "JSON format UTC failed");
    // Expected: 2023-10-05T14:30:00.123456789Z
    LOG_ASSERT(strcmp(buf, "2023-10-05T14:30:00.123456789Z") == 0, "JSON UTC mismatch. Got: %s", buf);

    // 2. Test Timezone Offset (+05:30)
    t.has_tz    = true;
    t.tz_offset = 330;

    err = xtime_to_json(&t, buf, sizeof(buf));
    LOG_ASSERT(err == XTIME_OK, "JSON format with offset failed");
    // Time shifts: 14:30 UTC + 5:30 = 20:00
    // Expected: 2023-10-05T20:00:00.123456789+05:30
    const char* expected_tz = "2023-10-05T20:00:00.123456789+05:30";
    LOG_ASSERT(strcmp(buf, expected_tz) == 0, "JSON Offset mismatch.\nExpected: %s\nGot:      %s", expected_tz, buf);

    // 3. Test Buffer Too Small
    char small_buf[10];
    err = xtime_to_json(&t, small_buf, sizeof(small_buf));
    LOG_ASSERT(err == XTIME_ERR_BUFFER_TOO_SMALL, "Should detect small buffer");

    // 4. Test Zero Nanoseconds (Optional clean output check)
    t.nanoseconds = 0;
    t.has_tz      = false;
    xtime_to_json(&t, buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2023-10-05T14:30:00Z") == 0, "JSON clean zero-nanos mismatch. Got: %s", buf);

    // 5. Test Africa/Kampala Offset (UTC+3)
    t.seconds     = 1696516200;  // Reset to 2023-10-05 14:30:00 UTC
    t.nanoseconds = 0;           // Clean seconds for this test
    t.has_tz      = true;
    t.tz_offset   = 180;  // +3 hours * 60 minutes = 180 (Kampala/EAT)

    err = xtime_to_json(&t, buf, sizeof(buf));
    LOG_ASSERT(err == XTIME_OK, "JSON format Kampala failed");

    // Calculation: 14:30 UTC + 3 hours = 17:30
    const char* expected_kampala = "2023-10-05T17:30:00+03:00";
    LOG_ASSERT(strcmp(buf, expected_kampala) == 0, "JSON Kampala mismatch.\nExpected: %s\nGot:      %s",
               expected_kampala, buf);
}

void test_frontend_inputs(void) {
    xtime_t t;

    // 1. JavaScript Date.toISOString() format
    // "2023-11-28T14:30:00.456Z"
    const char* js_date = "2023-11-28T14:30:00.456Z";

    xtime_error_t err = xtime_parse(js_date, XTIME_FMT_ISO8601, &t);
    LOG_ASSERT(err == XTIME_OK, "Failed to parse JS Date");

    // Check nanoseconds (456ms = 456,000,000ns)
    LOG_ASSERT(t.nanoseconds == 456000000, "Frontend Milliseconds failed. Expected 456000000, got %u", t.nanoseconds);

    // Check TZ
    LOG_ASSERT(t.has_tz == true && t.tz_offset == 0, "Failed to parse Z in JS Date");

    // 2. Python/Postgres with high precision and offset
    // "2023-11-28 14:30:00.123456+03:00"
    const char* db_date = "2023-11-28 14:30:00.123456+03:00";

    err = xtime_parse(db_date, XTIME_FMT_DATETIME, &t);  // Note space separator format
    LOG_ASSERT(err == XTIME_OK, "Failed to parse High-Precision DB Date");

    LOG_ASSERT(t.nanoseconds == 123456000, "DB Microseconds failed. Got %u", t.nanoseconds);

    LOG_ASSERT(t.tz_offset == 180, "DB Timezone failed");
}

// ============================================================================
// NEW TESTS FOR EXTENDED FUNCTIONALITY
// ============================================================================

void test_arithmetic_subsecond(void) {
    xtime_t t;
    xtime_from_unix(1000, &t);  // 1000 seconds, 0 ns

    // 1. Add Nanoseconds (with overflow to seconds)
    // Add 1.5 seconds worth of nanoseconds
    xtime_add_nanoseconds(&t, 1500000000LL);
    LOG_ASSERT(t.seconds == 1001, "Nano overflow seconds failed. Got %lld", (long long)t.seconds);
    LOG_ASSERT(t.nanoseconds == 500000000, "Nano remainder failed. Got %u", t.nanoseconds);

    // 2. Add Microseconds (negative / subtraction)
    // Subtract 0.5 seconds (500,000 micros) -> Should go back to 1001s, 0ns
    xtime_add_microseconds(&t, -500000LL);
    LOG_ASSERT(t.seconds == 1001, "Micro subtraction seconds failed");
    LOG_ASSERT(t.nanoseconds == 0, "Micro subtraction nanos failed");

    // 3. Add Milliseconds
    // Add 100ms
    xtime_add_milliseconds(&t, 100);
    LOG_ASSERT(t.nanoseconds == 100000000, "Milli addition failed");
}

void test_arithmetic_large_units(void) {
    xtime_t t;
    xtime_from_unix(1000, &t);

    // 1. Minutes
    xtime_add_minutes(&t, 60);  // +1 hour
    LOG_ASSERT(t.seconds == 4600, "Add minutes failed. Expected 4600, got %lld", (long long)t.seconds);

    // 2. Hours
    xtime_add_hours(&t, -1);  // Back to 1000
    LOG_ASSERT(t.seconds == 1000, "Add hours failed");

    // 3. Days
    xtime_add_days(&t, 1);
    LOG_ASSERT(t.seconds == 1000 + 86400, "Add days failed");
}

void test_calendar_math(void) {
    xtime_t t;
    char buf[64];

    // Setup: 2024-02-29 (Leap Year)
    // We use xtime_parse to easily set a specific date
    xtime_parse("2024-02-29T12:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);

    // 1. Add Year (Leap to Non-Leap adjustment)
    // 2024-02-29 + 1 year should be 2025-02-28 (not 29, not March 1)
    xtime_add_years(&t, 1);
    xtime_format_utc(&t, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2025-02-28") == 0, "Add year clamp failed. Got %s", buf);

    // 2. Add Months (Month boundary handling)
    // Setup: 2023-01-31
    xtime_parse("2023-01-31T12:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);

    // Add 1 month -> Should be Feb 28 (2023 is non-leap)
    xtime_add_months(&t, 1);
    xtime_format_utc(&t, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2023-02-28") == 0, "Add month clamp failed. Got %s", buf);

    // 3. Add Months (Year rollover)
    // Setup: 2023-11-15
    xtime_parse("2023-11-15T12:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);
    xtime_add_months(&t, 2);  // -> 2024-01-15
    xtime_format_utc(&t, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2024-01-15") == 0, "Month/Year rollover failed. Got %s", buf);
}

void test_differences(void) {
    xtime_t start, end;
    int64_t diff;

    xtime_parse("2023-01-01T10:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &start);
    xtime_parse("2023-01-01T10:00:01.500Z", "%Y-%m-%dT%H:%M:%S", &end);
    end.nanoseconds = 500000000;  // Parse doesn't auto-handle sub-seconds in format string usually

    // 1. Diff Nanos
    xtime_diff_nanos(&end, &start, &diff);
    LOG_ASSERT(diff == 1500000000LL, "Diff nanos failed. Got %lld", (long long)diff);

    // 2. Diff Millis
    xtime_diff_millis(&end, &start, &diff);
    LOG_ASSERT(diff == 1500, "Diff millis failed");

    // 3. Diff Seconds
    xtime_diff_seconds(&end, &start, &diff);
    LOG_ASSERT(diff == 1, "Diff seconds failed");  // Integer truncation expected

    // 4. Diff Days
    xtime_add_days(&end, 5);  // Move end 5 days forward
    xtime_diff_days(&end, &start, &diff);
    LOG_ASSERT(diff == 5, "Diff days failed");
}

void test_truncation(void) {
    xtime_t t;
    char buf[64];
    // Setup: 2023-10-15 14:35:45.123456789
    xtime_parse("2023-10-15T14:35:45Z", "%Y-%m-%dT%H:%M:%SZ", &t);
    t.nanoseconds = 123456789;

    // 1. Truncate to Minute (Seconds/Nanos -> 0)
    xtime_t t_min = t;
    xtime_truncate_to_minute(&t_min);
    xtime_format_utc(&t_min, "%H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "14:35:00") == 0, "Truncate to minute failed time. Got %s", buf);
    LOG_ASSERT(t_min.nanoseconds == 0, "Truncate to minute failed nanos");

    // 2. Truncate to Day (Time -> 00:00:00)
    xtime_t t_day = t;
    xtime_truncate_to_day(&t_day);
    xtime_format_utc(&t_day, "%Y-%m-%d %H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2023-10-15 00:00:00") == 0, "Truncate to day failed. Got %s", buf);
}

void test_boundaries(void) {
    xtime_t t, res;
    char buf[64];

    // Friday, Nov 28, 2025
    xtime_parse("2025-11-28T15:30:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);

    // 1. Start of Week (Assuming Monday start)
    // Should be Monday, Nov 24
    xtime_start_of_week(&t, &res);
    xtime_format_utc(&res, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2025-11-24") == 0, "Start of week failed. Got %s", buf);

    // 2. Start of Month
    // Should be Nov 01
    xtime_start_of_month(&t, &res);
    xtime_format_utc(&res, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2025-11-01") == 0, "Start of month failed");

    // 3. End of Month
    // Should be Nov 30, 23:59:59...
    xtime_end_of_month(&t, &res);
    xtime_format_utc(&res, "%Y-%m-%d %H:%M:%S", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2025-11-30 23:59:59") == 0, "End of month failed time. Got %s", buf);
    LOG_ASSERT(res.nanoseconds == 999999999, "End of month failed nanos");

    // 4. End of Year
    // Should be Dec 31
    xtime_end_of_year(&t, &res);
    xtime_format_utc(&res, "%Y-%m-%d", buf, sizeof(buf));
    LOG_ASSERT(strcmp(buf, "2025-12-31") == 0, "End of year failed");
}

void test_leap_year_logic(void) {
    xtime_t t;

    // 2024 (Leap)
    xtime_parse("2024-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);
    LOG_ASSERT(xtime_is_leap_year(&t) == true, "2024 should be leap year");

    // 2023 (Common)
    xtime_parse("2023-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);
    LOG_ASSERT(xtime_is_leap_year(&t) == false, "2023 should not be leap year");

    // 2000 (Leap - Divisible by 400)
    xtime_parse("2000-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);
    LOG_ASSERT(xtime_is_leap_year(&t) == true, "2000 should be leap year");

    // 1900 (Common - Divisible by 100 but not 400)
    xtime_parse("1900-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &t);
    LOG_ASSERT(xtime_is_leap_year(&t) == false, "1900 should not be leap year");
}

int main(void) {
    LOG_SECTION("XTIME LIBRARY COMPREHENSIVE TEST SUITE");

    RUN_TEST(test_initialization);
    RUN_TEST(test_unix_conversion);
    RUN_TEST(test_now);
    RUN_TEST(test_formatting);
    RUN_TEST(test_buffer_safety);
    RUN_TEST(test_parsing_valid);
    RUN_TEST(test_parsing_invalid);
    RUN_TEST(test_timezone_parsing_logic);
    RUN_TEST(test_arithmetic);
    RUN_TEST(test_comparison);
    RUN_TEST(test_diff);
    RUN_TEST(test_json_formatting);
    RUN_TEST(test_frontend_inputs);

    LOG_SECTION("Extended Functionality");
    RUN_TEST(test_arithmetic_subsecond);
    RUN_TEST(test_arithmetic_large_units);
    RUN_TEST(test_calendar_math);
    RUN_TEST(test_differences);
    RUN_TEST(test_truncation);
    RUN_TEST(test_boundaries);
    RUN_TEST(test_leap_year_logic);

    printf("\n" COLOR_GREEN "=== ALL TESTS PASSED SUCCESSFULLY ===" COLOR_RESET "\n");
    return EXIT_SUCCESS;
}
