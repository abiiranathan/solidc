#include <ctype.h>
#include <stdlib.h>
#include "stack.h"

DEFINE_STACK(float, 100)
DEFINE_STACK(char, 100)

// Helper function to check if a character is an operator
bool is_operator(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/');
}

// Helper function to evaluate the result of an operation
float perform_operation(float left_operand, float right_operand, char operator) {
    switch (operator) {
        case '+':
            return left_operand + right_operand;
        case '-':
            return left_operand - right_operand;
        case '*':
            return left_operand * right_operand;
        case '/':
            if (right_operand == 0) {
                printf("Division by zero\n");
                return 0.0f;
            }
            return left_operand / right_operand;
        default:
            printf("Invalid operator: %c\n", operator);
            return 0.0f;
    }
}

// VM function to evaluate the expression
float evaluate_expression(const char* expression) {
    Stack_float operand_stack;
    Stack_char operator_stack;

    stack_init_float(&operand_stack);
    stack_init_char(&operator_stack);

    // Flag to expect an operand after an operator or opening parenthesis
    bool expect_operand = true;

    while (*expression != '\0') {
        if (isspace(*expression)) {
            expression++;  // Skip whitespace characters
        } else if (isdigit(*expression) || (*expression == '.' && isdigit(*(expression + 1)))) {
            char* endptr;
            float value = strtof(expression, &endptr);

            if (endptr != expression) {
                stack_push_float(&operand_stack, value);
                expression = endptr;     // Move the expression pointer to the next token
                expect_operand = false;  // We got an operand, now expect an operator or closing parenthesis
            } else {
                // Invalid operand
                printf("Invalid operand\n");
                return 0.0f;
            }
        } else if (expect_operand && (*expression == '-' && isdigit(*(expression + 1)))) {
            // Handle unary negation for negative numbers
            expression++;  // Move past the minus sign
            char* endptr;
            float value = strtof(expression, &endptr);

            if (endptr != expression) {
                stack_push_float(&operand_stack, -value);
                expression = endptr;     // Move the expression pointer to the next token
                expect_operand = false;  // We got an operand, now expect an operator or closing parenthesis
            } else {
                printf("Invalid operand\n");
                return 0.0f;
            }
        } else if (is_operator(*expression)) {
            while (!stack_isEmpty_char(&operator_stack) && is_operator(stack_peek_char(&operator_stack)) &&
                   stack_peek_char(&operator_stack) != '(') {
                char current_operator = stack_pop_char(&operator_stack);

                if (stack_isEmpty_float(&operand_stack)) {
                    printf("Stack underflow\n");
                    return 0.0f;
                }

                float right_operand = stack_pop_float(&operand_stack);

                if (stack_isEmpty_float(&operand_stack)) {
                    printf("Stack underflow\n");
                    return 0.0f;
                }

                float left_operand = stack_pop_float(&operand_stack);

                float result = perform_operation(left_operand, right_operand, current_operator);
                stack_push_float(&operand_stack, result);
            }

            stack_push_char(&operator_stack, *expression);
            expression++;
            expect_operand = true;  // After an operator, expect another operand
        } else if (*expression == '(') {
            stack_push_char(&operator_stack, *expression);
            expression++;
            expect_operand = true;  // After an opening parenthesis, expect an operand
        } else if (*expression == ')') {
            while (!stack_isEmpty_char(&operator_stack) && stack_peek_char(&operator_stack) != '(') {
                char current_operator = stack_pop_char(&operator_stack);

                if (stack_isEmpty_float(&operand_stack)) {
                    printf("Stack underflow\n");
                    return 0.0f;
                }

                float right_operand = stack_pop_float(&operand_stack);

                if (stack_isEmpty_float(&operand_stack)) {
                    printf("Stack underflow\n");
                    return 0.0f;
                }

                float left_operand = stack_pop_float(&operand_stack);

                float result = perform_operation(left_operand, right_operand, current_operator);
                stack_push_float(&operand_stack, result);
            }

            // Pop the opening parenthesis
            stack_pop_char(&operator_stack);
            expression++;
            // After closing parenthesis, expect an operator or closing parenthesis
            expect_operand = false;
        } else {
            // Invalid character in the expression
            printf("Invalid character: %c\n", *expression);
            return 0.0f;
        }
    }

    // Process any remaining operators
    while (!stack_isEmpty_char(&operator_stack)) {
        char current_operator = stack_pop_char(&operator_stack);

        if (stack_isEmpty_float(&operand_stack)) {
            printf("Stack underflow\n");
            return 0.0f;
        }

        float right_operand = stack_pop_float(&operand_stack);

        if (stack_isEmpty_float(&operand_stack)) {
            printf("Stack underflow\n");
            return 0.0f;
        }

        float left_operand = stack_pop_float(&operand_stack);

        float result = perform_operation(left_operand, right_operand, current_operator);
        stack_push_float(&operand_stack, result);
    }

    // Pop the final result from the stack
    float result = stack_pop_float(&operand_stack);
    if (!stack_isEmpty_float(&operand_stack)) {
        printf("Invalid expression\n");
        result = 0.0f;
    }
    return result;
}

int main() {
    const char* expression1 = "(3 + 4) * -5";                      // -35
    const char* expression2 = "-10 / -2";                          // 5
    const char* expression3 = "5*2.4 + 8.";                        // 20
    const char* expression4 = "((3.5 - 2) * 4.8) / (-2.5 + 1.5)";  // -7.2

    float result1 = evaluate_expression(expression1);
    float result2 = evaluate_expression(expression2);
    float result3 = evaluate_expression(expression3);
    float result4 = evaluate_expression(expression4);

    printf("Result 1: %f\n", result1);
    printf("Result 2: %f\n", result2);
    printf("Result 3: %f\n", result3);
    printf("Result 4: %f\n", result4);

    return 0;
}