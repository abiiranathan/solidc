#include "stack.h"

// Define the stack for integers with a size of 5
DEFINE_STACK(int, 5);

// Define the stack for characters with a size of 3
DEFINE_STACK(char, 3);

// Sample usage of the DEFINE_STACK macro
int main() {
    Stack_int intStack;
    stack_init_int(&intStack);
    stack_push_int(&intStack, 10);
    stack_push_int(&intStack, 20);
    stack_push_int(&intStack, 30);
    stack_push_int(&intStack, 40);
    printf("Popped Integer: %d\n", stack_pop_int(&intStack));
    printf("Top Integer: %d\n", stack_peek_int(&intStack));
    printf("Top Integer: %d\n", stack_pop_int(&intStack));
    printf("Top Integer: %d\n", stack_pop_int(&intStack));
    printf("Top Integer: %d\n", stack_pop_int(&intStack));
    printf("Top Integer: %d\n", stack_pop_int(&intStack));

    Stack_char charStack;
    stack_init_char(&charStack);
    stack_push_char(&charStack, 'A');
    stack_push_char(&charStack, 'B');
    stack_push_char(&charStack, 'C');
    printf("Popped Character: %c\n", stack_pop_char(&charStack));
    printf("Top Character: %c\n", stack_peek_char(&charStack));
    printf("Top Character: %c\n", stack_pop_char(&charStack));
    printf("Top Character: %c\n", stack_pop_char(&charStack));
    printf("Top Character: %c\n", stack_pop_char(&charStack));

    return 0;
}
