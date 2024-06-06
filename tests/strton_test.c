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
void test_sto_ulong();
void test_sto_long();
void test_sto_double();
void test_sto_int();
void test_sto_ulong_b();
void test_sto_long_b();
void test_sto_int_b();
void test_sto_bool();

int main() {
    test_sto_ulong();
    test_sto_long();
    test_sto_double();
    test_sto_int();
    test_sto_ulong_b();
    test_sto_long_b();
    test_sto_int_b();
    test_sto_bool();

    printf("All tests completed.\n");
    return 0;
}

void test_sto_ulong() {
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

void test_sto_long() {
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

void test_sto_double() {
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

void test_sto_int() {
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

void test_sto_ulong_b() {
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

void test_sto_long_b() {
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

void test_sto_int_b() {
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

void test_sto_bool() {
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
