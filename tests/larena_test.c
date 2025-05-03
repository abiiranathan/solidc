#include "../include/larena.h"
#include "../include/macros.h"

#define ARENA_SIZE 1024 * 1024
#define BLOCK_SIZE 128
#define COUNT (ARENA_SIZE / BLOCK_SIZE)

int main() {
    LArena* arena = larena_create(ARENA_SIZE);
    ASSERT(arena);

    for (size_t i = 0; i < COUNT; ++i) {
        void* ptr = larena_alloc(arena, BLOCK_SIZE);
        ASSERT(ptr);
    }

    // Expect out of memory
    void* ptr = larena_alloc(arena, BLOCK_SIZE);
    ASSERT(ptr == NULL);

    larena_destroy(arena);

    arena = larena_create(128);
    ASSERT(arena);

    const char* str = "Hello World from Arena";
    char* new_str   = (char*)larena_alloc_string(arena, str);
    ASSERT_STR_EQ(str, new_str);

    // Resize the arena
    ASSERT(larena_resize(arena, ARENA_SIZE * 2));
    // printf("Free memory: %ld\n", larena_getfree_memory(arena));
    ptr = larena_alloc(arena, 1024);
    ASSERT(ptr);

    larena_destroy(arena);
}
