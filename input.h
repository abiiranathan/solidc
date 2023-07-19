#ifndef __INPUT_H__
#define __INPUT_H__

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool read_int(int* value, const char* prompt);
bool read_float(float* value, const char* prompt);
bool read_double(double* value, const char* prompt);
bool read_string(char* buffer, size_t size, const char* prompt);

#endif /* __INPUT_H__ */
