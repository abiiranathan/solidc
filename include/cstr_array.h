#ifndef CSTR_ARRAY_H
#define CSTR_ARRAY_H

#include <stddef.h>
#include "cstr.h"

typedef struct {
    cstr** data;
    size_t len;
    size_t cap;
} cstr_array;

// Allocate a string array with initial capacity.
// If initial_c == 0, a default value is used.
cstr_array* cstr_array_new(size_t initial_cap);

// Frees all cstrs + array
void cstr_array_free(cstr_array* arr);

// Push s to array. Takes ownership of s.
// Returns 0 on success or -1 on failure.
int cstr_array_push(cstr_array* arr, cstr* s);

// Push str to array. Copies string str.
// Returns 0 on success or -1 on failure.
int cstr_array_push_str(cstr_array* arr, const char* str);

// Return cstr at index or NULL if index is OUB.
cstr* cstr_array_get(cstr_array* arr, size_t index);

// Remove cstr and index.
// Frees removed string
void cstr_array_remove(cstr_array* arr, size_t index);

#endif  // CSTR_ARRAY_H
