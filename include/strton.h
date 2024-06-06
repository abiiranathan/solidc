#ifndef D098E3F2_2EEB_4E1A_892A_156C7D7533C3
#define D098E3F2_2EEB_4E1A_892A_156C7D7533C3

#include <stdbool.h>

// Define error codes
typedef enum { STO_SUCCESS, STO_OVERFLOW, STO_UNDERFLOW, STO_INVALID, STO_UNKNOWN } StoError;

StoError sto_ulong(const char* str, unsigned long* result);
StoError sto_long(const char* str, long* result);
StoError sto_double(const char* str, double* result);
StoError sto_int(const char* str, int* result);
StoError sto_ulong_b(const char* str, int base, unsigned long* result);
StoError sto_long_b(const char* str, int base, long* result);
StoError sto_int_b(const char* str, int base, int* result);

// Boolean converter for true/false, yes/no, on/off, 1/0
StoError sto_bool(const char* str, bool* result);

#endif /* D098E3F2_2EEB_4E1A_892A_156C7D7533C3 */
