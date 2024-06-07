#include "../include/strton.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// Utility macro for logging
#define LOG_TEST_RESULT(test_name, condition)                                                      \
    do {                                                                                           \
        if (condition) {                                                                           \
            printf("[PASS] %s\n", test_name);                                                      \
        } else {                                                                                   \
            printf("[FAIL] %s\n", test_name);                                                      \
        }                                                                                          \
    } while (0)

// Test function prototypes
void test_sto_ulong(void);
void test_sto_long(void);
void test_sto_double(void);
void test_sto_int(void);
void test_sto_ulong_b(void);
void test_sto_long_b(void);
void test_sto_int_b(void);
void test_sto_bool(void);
void test_sto_uint8(void);
void test_sto_int8(void);
void test_sto_uint16(void);
void test_sto_int16(void);
void test_sto_uint32(void);
void test_sto_int32(void);
void test_sto_uint64(void);
void test_sto_int64(void);

int main(void) {
    test_sto_ulong();
    test_sto_long();
    test_sto_double();
    test_sto_int();
    test_sto_ulong_b();
    test_sto_long_b();
    test_sto_int_b();
    test_sto_bool();
    test_sto_uint8();
    test_sto_int8();
    test_sto_uint16();
    test_sto_int16();
    test_sto_uint32();
    test_sto_int32();
    test_sto_uint64();
    test_sto_int64();

    printf("All tests completed.\n");
    return 0;
}

void test_sto_ulong(void) {
    unsigned long result;
    StoError err;

    // Valid input
    err = sto_ulong("12345", &result);
    LOG_TEST_RESULT("sto_ulong valid input", err == STO_SUCCESS && result == 12345UL);

    // Invalid input
    err = sto_ulong("abc", &result);
    LOG_TEST_RESULT("sto_ulong invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_ulong("18446744073709551616", &result);
    LOG_TEST_RESULT("sto_ulong overflow input", err == STO_OVERFLOW);
}

void test_sto_long(void) {
    long result;
    StoError err;

    // Valid input
    err = sto_long("12345", &result);
    LOG_TEST_RESULT("sto_long valid input", err == STO_SUCCESS && result == 12345L);

    // Invalid input
    err = sto_long("abc", &result);
    LOG_TEST_RESULT("sto_long invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_long("9223372036854775808", &result);
    LOG_TEST_RESULT("sto_long overflow input", err == STO_OVERFLOW);
}

void test_sto_double(void) {
    double result;
    StoError err;

    // Valid input
    err = sto_double("123.45", &result);
    LOG_TEST_RESULT("sto_double valid input", err == STO_SUCCESS && result == 123.45);

    // Invalid input
    err = sto_double("abc", &result);
    LOG_TEST_RESULT("sto_double invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_double("1e309", &result);
    LOG_TEST_RESULT("sto_double overflow input", err == STO_OVERFLOW);
}

void test_sto_int(void) {
    int result;
    StoError err;

    // Valid input
    err = sto_int("12345", &result);
    LOG_TEST_RESULT("sto_int valid input", err == STO_SUCCESS && result == 12345);

    // Invalid input
    err = sto_int("abc", &result);
    LOG_TEST_RESULT("sto_int invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_int("2147483648", &result);
    LOG_TEST_RESULT("sto_int overflow input", err == STO_OVERFLOW);
}

void test_sto_ulong_b(void) {
    unsigned long result;
    StoError err;

    // Valid input
    err = sto_ulong_b("1a", 16, &result);
    LOG_TEST_RESULT("sto_ulong_b valid input", err == STO_SUCCESS && result == 26UL);

    // Invalid input
    err = sto_ulong_b("g", 16, &result);
    LOG_TEST_RESULT("sto_ulong_b invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_ulong_b("ffffffffffffffff", 16, &result);
    LOG_TEST_RESULT("sto_ulong_b overflow input", err == STO_OVERFLOW);
}

void test_sto_long_b(void) {
    long result;
    StoError err;

    // Valid input
    err = sto_long_b("1a", 16, &result);
    LOG_TEST_RESULT("sto_long_b valid input", err == STO_SUCCESS && result == 26L);

    // Invalid input
    err = sto_long_b("g", 16, &result);
    LOG_TEST_RESULT("sto_long_b invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_long_b("8000000000000000", 16, &result);
    LOG_TEST_RESULT("sto_long_b overflow input", err == STO_OVERFLOW);
}

void test_sto_int_b(void) {
    int result;
    StoError err;

    // Valid input
    err = sto_int_b("1a", 16, &result);
    LOG_TEST_RESULT("sto_int_b valid input", err == STO_SUCCESS && result == 26);

    // Test valid input for octal
    err = sto_int_b("12", 8, &result);
    LOG_TEST_RESULT("sto_int_b valid input for octal", err == STO_SUCCESS && result == 10);

    // Test valid input for binary
    err = sto_int_b("1010", 2, &result);
    LOG_TEST_RESULT("sto_int_b valid input for binary", err == STO_SUCCESS && result == 10);

    // Invalid input
    err = sto_int_b("g", 16, &result);
    LOG_TEST_RESULT("sto_int_b invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_int_b("80000000", 16, &result);
    LOG_TEST_RESULT("sto_int_b overflow input", err == STO_OVERFLOW);
}

void test_sto_bool(void) {
    bool result;
    StoError err;

    // Valid true inputs
    err = sto_bool("true", &result);
    LOG_TEST_RESULT("sto_bool 'true'", err == STO_SUCCESS && result == true);

    err = sto_bool("yes", &result);
    LOG_TEST_RESULT("sto_bool 'yes'", err == STO_SUCCESS && result == true);

    err = sto_bool("on", &result);
    LOG_TEST_RESULT("sto_bool 'on'", err == STO_SUCCESS && result == true);

    err = sto_bool("1", &result);
    LOG_TEST_RESULT("sto_bool '1'", err == STO_SUCCESS && result == true);

    // Valid false inputs
    err = sto_bool("false", &result);
    LOG_TEST_RESULT("sto_bool 'false'", err == STO_SUCCESS && result == false);

    err = sto_bool("no", &result);
    LOG_TEST_RESULT("sto_bool 'no'", err == STO_SUCCESS && result == false);

    err = sto_bool("off", &result);
    LOG_TEST_RESULT("sto_bool 'off'", err == STO_SUCCESS && result == false);

    err = sto_bool("0", &result);
    LOG_TEST_RESULT("sto_bool '0'", err == STO_SUCCESS && result == false);

    // Invalid input
    err = sto_bool("maybe", &result);
    LOG_TEST_RESULT("sto_bool invalid input", err == STO_INVALID);
}

void test_sto_uint8(void) {
    uint8_t result;
    StoError err;

    // Valid input
    err = sto_uint8("255", &result);
    LOG_TEST_RESULT("sto_uint8 valid input", err == STO_SUCCESS && result == 255);

    // Invalid input
    err = sto_uint8("abc", &result);
    LOG_TEST_RESULT("sto_uint8 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_uint8("256", &result);
    LOG_TEST_RESULT("sto_uint8 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_uint8("-1", &result);
    LOG_TEST_RESULT("sto_uint8 underflow input", err == STO_OVERFLOW);
}

void test_sto_int8(void) {
    int8_t result;
    StoError err;

    // Valid input
    err = sto_int8("127", &result);
    LOG_TEST_RESULT("sto_int8 valid input", err == STO_SUCCESS && result == 127);

    // Invalid input
    err = sto_int8("abc", &result);
    LOG_TEST_RESULT("sto_int8 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_int8("128", &result);
    LOG_TEST_RESULT("sto_int8 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_int8("-129", &result);
    LOG_TEST_RESULT("sto_int8 underflow input", err == STO_OVERFLOW);
}

void test_sto_uint16(void) {
    uint16_t result;
    StoError err;

    // Valid input
    err = sto_uint16("65535", &result);
    LOG_TEST_RESULT("sto_uint16 valid input", err == STO_SUCCESS && result == 65535);

    // Invalid input
    err = sto_uint16("abc", &result);
    LOG_TEST_RESULT("sto_uint16 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_uint16("65536", &result);
    LOG_TEST_RESULT("sto_uint16 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_uint16("-1", &result);
    LOG_TEST_RESULT("sto_uint16 underflow input", err == STO_OVERFLOW);
}

void test_sto_int16(void) {
    int16_t result;
    StoError err;

    // Valid input
    err = sto_int16("32767", &result);
    LOG_TEST_RESULT("sto_int16 valid input", err == STO_SUCCESS && result == 32767);

    // Invalid input
    err = sto_int16("abc", &result);
    LOG_TEST_RESULT("sto_int16 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_int16("32768", &result);
    LOG_TEST_RESULT("sto_int16 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_int16("-32769", &result);
    LOG_TEST_RESULT("sto_int16 underflow input", err == STO_OVERFLOW);
}

void test_sto_uint32(void) {
    uint32_t result;
    StoError err;

    // Valid input
    err = sto_uint32("4294967295", &result);
    LOG_TEST_RESULT("sto_uint32 valid input", err == STO_SUCCESS && result == 4294967295);

    // Invalid input
    err = sto_uint32("abc", &result);
    LOG_TEST_RESULT("sto_uint32 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_uint32("4294967296", &result);
    LOG_TEST_RESULT("sto_uint32 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_uint32("-1", &result);
    LOG_TEST_RESULT("sto_uint32 underflow input", err == STO_OVERFLOW);
}

void test_sto_int32(void) {
    int32_t result;
    StoError err;

    // Valid input
    err = sto_int32("2147483647", &result);
    LOG_TEST_RESULT("sto_int32 valid input", err == STO_SUCCESS && result == 2147483647);

    // Invalid input
    err = sto_int32("abc", &result);
    LOG_TEST_RESULT("sto_int32 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_int32("2147483648", &result);
    LOG_TEST_RESULT("sto_int32 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_int32("-2147483649", &result);
    LOG_TEST_RESULT("sto_int32 underflow input", err == STO_OVERFLOW);
}

void test_sto_uint64(void) {
    uint64_t result;
    StoError err;

    // Valid input
    err = sto_uint64("18446744073709551615", &result);
    LOG_TEST_RESULT("sto_uint64 valid input",
                    err == STO_SUCCESS && result == 18446744073709551615ULL);

    // Invalid input
    err = sto_uint64("abc", &result);
    LOG_TEST_RESULT("sto_uint64 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_uint64("18446744073709551616", &result);
    LOG_TEST_RESULT("sto_uint64 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_uint64("-1", &result);
    LOG_TEST_RESULT("sto_uint64 underflow input", err == STO_OVERFLOW);
}

void test_sto_int64(void) {
    int64_t result;
    StoError err;

    // Valid input
    err = sto_int64("9223372036854775807", &result);
    LOG_TEST_RESULT("sto_int64 valid input", err == STO_SUCCESS && result == 9223372036854775807LL);

    // Invalid input
    err = sto_int64("abc", &result);
    LOG_TEST_RESULT("sto_int64 invalid input", err == STO_INVALID);

    // Overflow input
    err = sto_int64("9223372036854775808", &result);
    LOG_TEST_RESULT("sto_int64 overflow input", err == STO_OVERFLOW);

    // Underflow input
    err = sto_int64("-9223372036854775809", &result);
    LOG_TEST_RESULT("sto_int64 underflow input", err == STO_OVERFLOW);
}