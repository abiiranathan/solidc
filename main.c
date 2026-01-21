
// #define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

int main() {
    char *temp = mkdtemp("");
    printf("temp: %s\n", temp);
}
