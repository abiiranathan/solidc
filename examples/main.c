#include <solidc/cstr.h>
#include <solidc/array.h>

DARRAY_DEFINE(IntArray, int);

int main() {
    cstr* s = cstr_new("Hello World");
    printf("String: %s\n", cstr_data_const(s));
    cstr_free(s);
}

// cmake --build build && ./build/example