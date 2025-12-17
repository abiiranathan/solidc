#include <solidc/array.h>
#include <solidc/cstr.h>

int main() {
    cstr* s = cstr_new("Hello World");
    printf("String: %s\n", cstr_data_const(s));
    cstr_free(s);
}

// cmake --build build && ./build/example