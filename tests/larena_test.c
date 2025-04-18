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
}
