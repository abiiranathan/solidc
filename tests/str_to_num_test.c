#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "../include/cmp.h"
#include "../include/str_to_num.h"

// Utility macro for logging
#define LOG_TEST_RESULT(test_name, condition)                                                                \
    do {                                                                                                     \
        if (condition) {                                                                                     \
            printf("[PASS] %s\n", test_name);                                                                \
        } else {                                                                                             \
            printf("[FAIL] %s\n", test_name);                                                                \
        }                                                                                                    \
    } while (0)

// Test function prototypes
void test_str_to_ulong(void);
void test_str_to_long(void);
void test_str_to_double(void);
void test_str_to_int(void);
void test_str_to_ulong_b(void);
void test_str_to_long_b(void);
void test_str_to_int_base(void);
void test_str_to_bool(void);
void test_str_to_u8(void);
void test_str_to_i8(void);
void test_str_to_u16(void);
void test_str_to_i16(void);
void test_str_to_u32(void);
void test_str_to_i32(void);
void test_str_to_u64(void);
void test_str_to_i64(void);

int main(void) {
    test_str_to_ulong();
    test_str_to_long();
    test_str_to_double();
    test_str_to_int();
    test_str_to_ulong_b();
    test_str_to_long_b();
    test_str_to_int_base();
    test_str_to_bool();
    test_str_to_u8();
    test_str_to_i8();
    test_str_to_u16();
    test_str_to_i16();
    test_str_to_u32();
    test_str_to_i32();
    test_str_to_u64();
    test_str_to_i64();

    printf("All tests completed.\n");
    return 0;
}

void test_str_to_ulong(void) {
    unsigned long result;
    StoError err;

    // Valid input
    err = str_to_ulong("12345", &result);
    LOG_TEST_RESULT("str_to_ulong valid input", err == STO_SUCCESS && result == 12345UL);

    // Invalid input
    err = str_to_ulong("abc", &result);
    LOG_TEST_RESULT("str_to_ulong invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_ulong("18446744073709551616", &result);
    LOG_TEST_RESULT("str_to_ulong overflow input", err == STO_OVERFLOW);
}

void test_str_to_long(void) {
    long result;
    StoError err;

    // Valid input
    err = str_to_long("12345", &result);
    LOG_TEST_RESULT("str_to_long valid input", err == STO_SUCCESS && result == 12345L);

    // Invalid input
    err = str_to_long("abc", &result);
    LOG_TEST_RESULT("str_to_long invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_long("9223372036854775808", &result);
    LOG_TEST_RESULT("str_to_long overflow input", err == STO_OVERFLOW);
}

void test_str_to_double(void) {
    double result;
    StoError err;
    cmp_config_t c = {.epsilon = 1e2};

    // Valid input
    err = str_to_double("123.45", &result);
    LOG_TEST_RESULT("str_to_double valid input", err == STO_SUCCESS);

    LOG_TEST_RESULT("str_to_double result is invalid", cmp_float(result, 123.45, c));

    // Invalid input
    err = str_to_double("abc", &result);
    LOG_TEST_RESULT("str_to_double invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_double("1e309", &result);
    LOG_TEST_RESULT("str_to_double overflow input", err == STO_OVERFLOW);
}

void test_str_to_int(void) {
    int result;
    StoError err;

    // Valid input
    err = str_to_int("12345", &result);
    LOG_TEST_RESULT("str_to_i valid input", err == STO_SUCCESS && result == 12345);

    // Invalid input
    err = str_to_int("abc", &result);
    LOG_TEST_RESULT("str_to_i invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_int("2147483648", &result);
    LOG_TEST_RESULT("str_to_i overflow input", err == STO_OVERFLOW);
}

void test_str_to_ulong_b(void) {
    unsigned long result;
    StoError err;

    // Valid input
    err = str_to_ulong_base("1a", 16, &result);
    LOG_TEST_RESULT("str_to_ulong_b valid input", err == STO_SUCCESS && result == 26UL);

    // Invalid input
    err = str_to_ulong_base("g", 16, &result);
    LOG_TEST_RESULT("str_to_ulong_b invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_ulong_base("ffffffffffffffff", 16, &result);
    LOG_TEST_RESULT("str_to_ulong_b overflow input", err == STO_OVERFLOW);
}

void test_str_to_long_b(void) {
    long result;
    StoError err;

    // Valid input
    err = str_to_long_base("1a", 16, &result);
    LOG_TEST_RESULT("str_to_long_b valid input", err == STO_SUCCESS && result == 26L);

    // Invalid input
    err = str_to_long_base("g", 16, &result);
    LOG_TEST_RESULT("str_to_long_b invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_long_base("8000000000000000", 16, &result);
    LOG_TEST_RESULT("str_to_long_b overflow input", err == STO_OVERFLOW);
}

void test_str_to_int_base(void) {
    int result;
    StoError err;

    // Valid input
    err = str_to_int_base("1a", 16, &result);
    LOG_TEST_RESULT("str_to_int_base valid input", err == STO_SUCCESS && result == 26);

    // Test valid input for octal
    err = str_to_int_base("12", 8, &result);
    LOG_TEST_RESULT("str_to_int_base valid input for octal", err == STO_SUCCESS && result == 10);

    // Test valid input for binary
    err = str_to_int_base("1010", 2, &result);
    LOG_TEST_RESULT("str_to_int_base valid input for binary", err == STO_SUCCESS && result == 10);

    // Invalid input
    err = str_to_int_base("g", 16, &result);
    LOG_TEST_RESULT("str_to_int_base invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_int_base("80000000", 16, &result);
    LOG_TEST_RESULT("str_to_int_base overflow input", err == STO_OVERFLOW);
}

void test_str_to_bool(void) {
    bool result;
    StoError err;

    // Valid true inputs
    err = str_to_bool("true", &result);
    LOG_TEST_RESULT("str_to_bool 'true'", err == STO_SUCCESS && result == true);

    err = str_to_bool("yes", &result);
    LOG_TEST_RESULT("str_to_bool 'yes'", err == STO_SUCCESS && result == true);

    err = str_to_bool("on", &result);
    LOG_TEST_RESULT("str_to_bool 'on'", err == STO_SUCCESS && result == true);

    err = str_to_bool("1", &result);
    LOG_TEST_RESULT("str_to_bool '1'", err == STO_SUCCESS && result == true);

    // Valid false inputs
    err = str_to_bool("false", &result);
    LOG_TEST_RESULT("str_to_bool 'false'", err == STO_SUCCESS && result == false);

    err = str_to_bool("no", &result);
    LOG_TEST_RESULT("str_to_bool 'no'", err == STO_SUCCESS && result == false);

    err = str_to_bool("off", &result);
    LOG_TEST_RESULT("str_to_bool 'off'", err == STO_SUCCESS && result == false);

    err = str_to_bool("0", &result);
    LOG_TEST_RESULT("str_to_bool '0'", err == STO_SUCCESS && result == false);

    // Invalid input
    err = str_to_bool("maybe", &result);
    LOG_TEST_RESULT("str_to_bool invalid input", err == STO_INVALID);
}

void test_str_to_u8(void) {
    uint8_t result;
    StoError err;

    // Valid input
    err = str_to_u8("255", &result);
    LOG_TEST_RESULT("str_to_u8 valid input", err == STO_SUCCESS && result == 255);

    // Invalid input
    err = str_to_u8("abc", &result);
    LOG_TEST_RESULT("str_to_u8 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_u8("256", &result);
    LOG_TEST_RESULT("str_to_u8 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_u8("-1", &result);
    LOG_TEST_RESULT("str_to_u8 underflow input", err == STO_OVERFLOW);
}

void test_str_to_i8(void) {
    int8_t result;
    StoError err;

    // Valid input
    err = str_to_i8("127", &result);
    LOG_TEST_RESULT("str_to_i8 valid input", err == STO_SUCCESS && result == 127);

    // Invalid input
    err = str_to_i8("abc", &result);
    LOG_TEST_RESULT("str_to_i8 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_i8("128", &result);
    LOG_TEST_RESULT("str_to_i8 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_i8("-129", &result);
    LOG_TEST_RESULT("str_to_i8 underflow input", err == STO_OVERFLOW);
}

void test_str_to_u16(void) {
    uint16_t result;
    StoError err;

    // Valid input
    err = str_to_u16("65535", &result);
    LOG_TEST_RESULT("str_to_u16 valid input", err == STO_SUCCESS && result == 65535);

    // Invalid input
    err = str_to_u16("abc", &result);
    LOG_TEST_RESULT("str_to_u16 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_u16("65536", &result);
    LOG_TEST_RESULT("str_to_u16 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_u16("-1", &result);
    LOG_TEST_RESULT("str_to_u16 underflow input", err == STO_OVERFLOW);
}

void test_str_to_i16(void) {
    int16_t result;
    StoError err;

    // Valid input
    err = str_to_i16("32767", &result);
    LOG_TEST_RESULT("str_to_i16 valid input", err == STO_SUCCESS && result == 32767);

    // Invalid input
    err = str_to_i16("abc", &result);
    LOG_TEST_RESULT("str_to_i16 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_i16("32768", &result);
    LOG_TEST_RESULT("str_to_i16 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_i16("-32769", &result);
    LOG_TEST_RESULT("str_to_i16 underflow input", err == STO_OVERFLOW);
}

void test_str_to_u32(void) {
    uint32_t result;
    StoError err;

    // Valid input
    err = str_to_u32("4294967295", &result);
    LOG_TEST_RESULT("str_to_u32 valid input", err == STO_SUCCESS && result == 4294967295);

    // Invalid input
    err = str_to_u32("abc", &result);
    LOG_TEST_RESULT("str_to_u32 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_u32("4294967296", &result);
    LOG_TEST_RESULT("str_to_u32 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_u32("-1", &result);
    LOG_TEST_RESULT("str_to_u32 underflow input", err == STO_OVERFLOW);
}

void test_str_to_i32(void) {
    int32_t result;
    StoError err;

    // Valid input
    err = str_to_i32("2147483647", &result);
    LOG_TEST_RESULT("str_to_i32 valid input", err == STO_SUCCESS && result == 2147483647);

    // Invalid input
    err = str_to_i32("abc", &result);
    LOG_TEST_RESULT("str_to_i32 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_i32("2147483648", &result);
    LOG_TEST_RESULT("str_to_i32 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_i32("-2147483649", &result);
    LOG_TEST_RESULT("str_to_i32 underflow input", err == STO_OVERFLOW);
}

void test_str_to_u64(void) {
    uint64_t result;
    StoError err;

    // Valid input
    err = str_to_u64("18446744073709551615", &result);
    LOG_TEST_RESULT("str_to_u64 valid input", err == STO_SUCCESS && result == 18446744073709551615ULL);

    // Invalid input
    err = str_to_u64("abc", &result);
    LOG_TEST_RESULT("str_to_u64 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_u64("18446744073709551616", &result);
    LOG_TEST_RESULT("str_to_u64 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_u64("-1", &result);
    LOG_TEST_RESULT("str_to_u64 underflow input", err == STO_OVERFLOW);
}

void test_str_to_i64(void) {
    int64_t result;
    StoError err;

    // Valid input
    err = str_to_i64("9223372036854775807", &result);
    LOG_TEST_RESULT("str_to_i64 valid input", err == STO_SUCCESS && result == 9223372036854775807LL);

    // Invalid input
    err = str_to_i64("abc", &result);
    LOG_TEST_RESULT("str_to_i64 invalid input", err == STO_INVALID);

    // Overflow input
    err = str_to_i64("9223372036854775808", &result);
    LOG_TEST_RESULT("str_to_i64 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = str_to_i64("-9223372036854775809", &result);
    LOG_TEST_RESULT("str_to_i64 underflow input", err == STO_OVERFLOW);
}
