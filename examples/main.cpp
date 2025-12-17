#include <solidc/arena.h>
#include <iostream>
#include <string>

int main() {
    Arena* arena     = arena_create(0);
    char* first_name = arena_strdup(arena, "Abiira");
    char* last_name  = arena_strdup(arena, "Nathan");

    std::string name = std::string(first_name) + " " + std::string(last_name);
    std::cout << "Name: " << name << "\n";
    arena_destroy(arena);
}
