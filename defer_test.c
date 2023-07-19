#include "defer.h"
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

void defer_example() {
    Deferral;

    FILE* file = fopen("data.txt", "w+");
    if (file == NULL) {
        printf("Failed to open file.\n");
        Return;  // Return from the function
    }

    // Defer closing the file
    Defer({
        printf("Closing file.\n");
        fclose(file);
        remove("data.txt");
        printf("removing file.\n");
    });

    // Perform some operations with the file
    printf("writing file contents...\n");

    char* buffer = "Hello World!";
    fwrite(buffer, strlen(buffer), 1, file);

    Return;
}

int main() {
    defer_example();

    return 0;
}
