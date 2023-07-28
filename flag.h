#ifndef __FLAG_H__
#define __FLAG_H__

#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLAG_CAPACITY
#define FLAG_CAPACITY 50
#endif

long int* flag_int(char* name, long int def, char* desc);
char** flag_str(char* name, const char* def, char* desc);
double* flag_double(char* name, double def, char* desc);
float* flag_float(char* name, float def, char* desc);
bool* flag_bool(char* name, bool def, char* desc);
size_t* flag_size(const char* name, size_t def, const char* desc);
uint64_t* flag_uint64(const char* name, uint64_t def, const char* desc);

// Parses the command-line arguments and updates the flags.
// Prints errors and usage if an error occurs and exits with a status code of 1.
void flag_parse(int argc, char** argv);

// Prints the usage string to stream. executable is argv[0]
void flag_usage(FILE* stream, const char* executable);

#endif /* __FLAG_H__ */
