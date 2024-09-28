#ifndef D098E3F2_2EEB_4E1A_892A_156C7D7533C3
#define D098E3F2_2EEB_4E1A_892A_156C7D7533C3

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// Define error codes
typedef enum { STO_SUCCESS, STO_OVERFLOW, STO_INVALID } StoError;

/**
 * @brief Converts a string to a uint8_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_uint8(const char* str, uint8_t* result);

/**
 * @brief Converts a string to an int8_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_int8(const char* str, int8_t* result);

/**
 * @brief Converts a string to a uint16_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_uint16(const char* str, uint16_t* result);

/**
 * @brief Converts a string to an int16_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_int16(const char* str, int16_t* result);

/**
 * @brief Converts a string to a uint32_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_uint32(const char* str, uint32_t* result);

/**
 * @brief Converts a string to an int32_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_int32(const char* str, int32_t* result);

/**
 * @brief Converts a string to a uint64_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_uint64(const char* str, uint64_t* result);

/**
 * @brief Converts a string to an int64_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_int64(const char* str, int64_t* result);

/**
 * @brief Converts a string to an unsigned long.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_ulong(const char* str, unsigned long* result);

/**
 * @brief Converts a string to a long.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_long(const char* str, long* result);

/**
 * @brief Converts a string to a double.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_double(const char* str, double* result);

/**
 * @brief Converts a string to a float.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_float(const char* str, float* result);

/**
 * @brief Converts a string to an unsigned int.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_uint(const char* str, unsigned int* result);

/** uintptr_t is an unsigned integer type that is capable of storing a data pointer. */
/**
 * @brief Converts a string to a uintptr_t.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_INVALID if invalid input.
 */
StoError sto_uintptr(const char* str, uintptr_t* result);

/**
 * @brief Converts a string to an int.
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_int(const char* str, int* result);

/**
 * @brief Converts a string to an unsigned long with a specified base.
 * 
 * @param str The input string.
 * @param base The base for conversion.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_ulong_b(const char* str, int base, unsigned long* result);

/**
 * @brief Converts a string to a long with a specified base.
 * 
 * @param str The input string.
 * @param base The base for conversion.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_long_b(const char* str, int base, long* result);

/**
 * @brief Converts a string to an int with a specified base.
 * 
 * @param str The input string.
 * @param base The base for conversion.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_OVERFLOW if out of range, STO_INVALID if invalid input.
 */
StoError sto_int_b(const char* str, int base, int* result);

/**
 * @brief Converts a string to a boolean.
 * Valid inputs are "true", "false", "yes", "no", "1", "0", "on", "off".
 * 
 * @param str The input string.
 * @param result Pointer to the result variable.
 * @return StoError STO_SUCCESS on success, STO_INVALID if invalid input.
 */
StoError sto_bool(const char* str, bool* result);

const char* sto_error(StoError code);

#ifdef __cplusplus
}
#endif

#endif /* D098E3F2_2EEB_4E1A_892A_156C7D7533C3 */
