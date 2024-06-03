#include "../include/optional.h"
#include <assert.h>
#include <string.h>

ResultFloat divide(float a, float b) {
    if (b == 0) {
        return ERR_FLOAT("Division by zero");
    }
    return OK_FLOAT(a / b);
}

ResultInt chk_add(int a, int b) {
    int res = a + b;
    // int overflow
    if (res < a || res < b) {
        return ERR_INT("Integer overflow");
    }

    return OK_INT(res);
}

void unwrap_callback(int v) {
    assert(v == 3);
}

int main() {
    ResultFloat res = divide(10, 0);
    assert(!res.is_ok);

    res = divide(10, 2);

    assert(res.is_ok);
    assert(res.value == 5.0f);

    ResultInt res_int = chk_add(2147483647, 1);
    assert(!res_int.is_ok);

    const char* err = UNWRAP_ERR(res_int);
    assert(strcmp(err, "Integer overflow") == 0);

    res_int = chk_add(1, 2);
    assert(res_int.is_ok);
    assert(res_int.value == 3);

    int val = UNWRAP(res_int);
    assert(val == 3);
    UNWRAP_OK(res_int, unwrap_callback);

    printf("[optional.h]: All tests passed\n");

    return 0;
}