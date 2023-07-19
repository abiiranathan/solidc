#include "input.h"

bool read_int(int* value, const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (scanf("%d", value) != 1) {
        fflush(stdin);
        printf("Invalid input. Please enter an integer.\n");
        return false;
    }
    fgetc(stdin);  // clears the stdin input buffer
    return true;
}

bool read_float(float* value, const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (scanf("%f", value) != 1) {
        fflush(stdin);
        printf("Invalid input. Please enter a floating-point number.\n");
        return false;
    }
    fgetc(stdin);  // clears the stdin input buffer
    return true;
}

bool read_double(double* value, const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (scanf("%lf", value) != 1) {
        fflush(stdin);
        printf(
            "Invalid input. Please enter a double-precision floating-point "
            "number.\n");
        return false;
    }
    fgetc(stdin);  // clears the stdin input buffer
    return true;
}

/*
Read string into the character buffer up to size.
If a prompt is provided, uses printf to print it to stdout and flush stdout.
*/
bool read_string(char* buffer, size_t size, const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    // Terminates on \n or EOF
    if (fgets(buffer, size, stdin) == NULL) {
        return false;
    }

    // Remove \n from message
    buffer[strlen(buffer) - 1] = 0;
    return true;
}
