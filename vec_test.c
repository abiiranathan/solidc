#include <stdio.h>
#define VEC_IMPLEMENTATION
#include "vec.h"

int main(void) {
    vec_t* vec = vec_new(100, 25);

    vec_push(vec, "Hello");
    vec_push(vec, "World");

    vec_push(vec, "!");
    vec_push(vec, "This");
    vec_push(vec, "is");
    vec_push(vec, "a");
    vec_push(vec, "test");
    vec_push(vec, "of");
    vec_push(vec, "the");
    vec_push(vec, "vector");
    vec_push(vec, "implementation");
    vec_push(vec, "in");
    vec_push(vec, "C");
    vec_push(vec, "!");

    for (size_t i = 0; i < vec_size(vec); i++) {
        printf("%s ", (const char*)vec_get(vec, i));
    }
    printf("\n");

    vec_free(vec);

    // Int vector
    vec_t* int_vec = vec_new(sizeof(int), 5);
    vec_push(int_vec, &(int){1});
    vec_push(int_vec, &(int){2});
    vec_push(int_vec, &(int){3});
    vec_push(int_vec, &(int){4});
    vec_push(int_vec, &(int){5});

    for (size_t i = 0; i < vec_size(int_vec); i++) {
        printf("%d ", *(int*)vec_get(int_vec, i));
    }
    printf("\n");
}