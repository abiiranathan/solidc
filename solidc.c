#include "include/filepath.h"

WalkDirOption callback(const char* path, const char* name, void* data) {
    (void)name;
    (void)data;

    if (is_dir(path)) {
        // if its a hidden directory, skip it
        if (name[0] == '.') {
            return DirSkip;
        }

        printf("%s/\n", path);
        return DirContinue;
    } else {
        printf("%s/\n", path);
        return DirContinue;
    }
}

int main(void) {
    const char* dirname = user_home_dir();

    // walk through the directory
    dir_walk(dirname, callback, NULL);
}

// compile: gcc -Wall -Wextra -pedantic -o walkhome solidc.c -lsolidc
// run: ./walkhome
