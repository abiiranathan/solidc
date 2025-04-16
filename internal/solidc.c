#include "../include/filepath.h"

WalkDirOption walk_callback(const char* path, const char* name, void*) {
    if (is_dir(path)) {
        if (name[0] == '.') {
            return DirSkip;
        }
    }

    printf("%s/\n", path);
    return DirContinue;
}

int main(void) {
    const char* dirname = user_home_dir();
    return dir_walk(dirname, walk_callback, NULL);
}
