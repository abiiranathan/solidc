#ifndef __STACK_H__
#define __STACK_H__

#include <stdbool.h>
#include <stdio.h>

#define DEFINE_STACK(T, size)                                                                      \
    typedef struct Stack_##T {                                                                     \
        T data[size];                                                                              \
        int top;                                                                                   \
    } Stack_##T;                                                                                   \
                                                                                                   \
    void stack_init_##T(Stack_##T* stack) { stack->top = -1; }                                     \
                                                                                                   \
    bool stack_isEmpty_##T(Stack_##T* stack) { return stack->top == -1; }                          \
                                                                                                   \
    bool stack_isFull_##T(Stack_##T* stack) { return stack->top == size - 1; }                     \
                                                                                                   \
    void stack_push_##T(Stack_##T* stack, T data) {                                                \
        if (stack_isFull_##T(stack)) {                                                             \
            printf("Stack overflow\n");                                                            \
            return;                                                                                \
        }                                                                                          \
        stack->top++;                                                                              \
        stack->data[stack->top] = data;                                                            \
    }                                                                                              \
                                                                                                   \
    T stack_pop_##T(Stack_##T* stack) {                                                            \
        if (stack_isEmpty_##T(stack)) {                                                            \
            printf("Stack underflow\n");                                                           \
            return (T){0};                                                                         \
        }                                                                                          \
        T poppedData = stack->data[stack->top];                                                    \
        stack->top--;                                                                              \
        return poppedData;                                                                         \
    }                                                                                              \
                                                                                                   \
    T stack_peek_##T(Stack_##T* stack) {                                                           \
        if (stack_isEmpty_##T(stack)) {                                                            \
            printf("Stack is empty\n");                                                            \
            return (T){0};                                                                         \
        }                                                                                          \
        return stack->data[stack->top];                                                            \
    }
#endif /* __STACK_H__ */
