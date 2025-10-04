#include "../include/filepath.h"
#include <stdlib.h>
#include <string.h>
#include "../include/macros.h"

WalkDirOption walk(const char* path, const char* name, void* data) {
    printf("%s\n", path);
    (void)name;
    (void)data;
    return DirContinue;
}

int main(void) {
    // test makedirs
    const char* dirname = "./temp/tests/arena";
    bool ok             = filepath_makedirs(dirname);
    ASSERT(ok);

    // test is_dir
    ASSERT(is_dir(dirname));

    // test dir_open
    Directory* dir = dir_open(dirname);
    ASSERT(dir != NULL);

    // test dir_next
    char* entry = dir_next(dir);
    ASSERT(entry != NULL);
    dir_close(dir);

    // test_get_cwd
    const char* cwd = get_cwd();
    ASSERT(cwd != NULL);

    // dir_list
    size_t count = 0;
    char** files = dir_list(cwd, &count);
    ASSERT(files != NULL);

    for (size_t i = 0; i < count; i++) {
        printf("%s\n", files[i]);
        free(files[i]);
    }

    free(files);

    // test path related functions
    const char* path = "./temp/tests/arena/test.txt";
    filepath_remove(path);
    ASSERT(!path_exists(path));

    // path join
    const char* joined = filepath_join(dirname, "test.txt");
    ASSERT(joined != NULL);
    // test path_exists
    printf("joined: %s\n", joined);
    ASSERT(!path_exists(joined));

    // basename
    char basename[FILENAME_MAX];
    filepath_basename(joined, basename, FILENAME_MAX);
    ASSERT(strcmp(basename, "test.txt") == 0);

    // filepath_dirname
    char dirname2[FILENAME_MAX];
    filepath_dirname(joined, dirname2, FILENAME_MAX);
    ASSERT(strcmp(dirname2, dirname) == 0);

    // filepath_extension
    char ext[FILENAME_MAX];
    filepath_extension(joined, ext, FILENAME_MAX);
    ASSERT(strcmp(ext, ".txt") == 0);

    // test filepath_nameonly
    char name[FILENAME_MAX];
    filepath_nameonly(joined, name, FILENAME_MAX);
    ASSERT(strcmp(name, "test") == 0);

    // test filepath_absolute
    // first create the file
    FILE* file = fopen(joined, "w");
    ASSERT(file != NULL);
    fclose(file);
    char* abs = filepath_absolute(joined);
    ASSERT(abs != NULL);
    printf("abs: %s\n", abs);
    free(abs);

    // test filepath_rename
    const char* newpath = "./temp/tests/arena/test2.txt";
    ASSERT(filepath_rename(joined, newpath) == 0);

    // rename back
    ASSERT(filepath_rename(newpath, joined) == 0);

    // test filepath_expanduser
    char* home = filepath_expanduser("~");
    ASSERT(home != NULL);
    free(home);

    // test filepath_expanduser_buf
    char expanded[FILENAME_MAX];
    ASSERT(filepath_expanduser_buf("~", expanded, FILENAME_MAX));
    printf("expanded home: %s\n", expanded);

    // test filepath_join_buf
    char abspath[FILENAME_MAX];
    ASSERT(filepath_join_buf(dirname, "test.txt", abspath, FILENAME_MAX));
    printf("filepath_join_buf: %s\n", abspath);

    // test filepath_split
    char dirbuf[FILENAME_MAX];
    char namebuf[FILENAME_MAX];
    filepath_split(joined, dirbuf, namebuf, FILENAME_MAX, FILENAME_MAX);

    ASSERT(strcmp(dirbuf, dirname) == 0);
    ASSERT(strcmp(namebuf, "test.txt") == 0);

    // test dir_size
    // Write some data to the file
    file = fopen(joined, "w");
    ASSERT(file != NULL);
    fprintf(file, "Hello, World!");
    fclose(file);

    ssize_t size = dir_size(dirname);
    ASSERT(size == 13);

    // test dir_walk
    ASSERT(dir_walk(dirname, walk, NULL) == 0);

    // Delete the directory
    dir_remove("./temp", true);

    return 0;
}
