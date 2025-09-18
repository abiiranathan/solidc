#include "../include/filepath.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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
    assert(ok);

    // test is_dir
    assert(is_dir(dirname));

    // test dir_open
    Directory* dir = dir_open(dirname);
    assert(dir != NULL);

    // test dir_next
    char* entry = dir_next(dir);
    assert(entry != NULL);
    dir_close(dir);

    // test_get_cwd
    const char* cwd = get_cwd();
    assert(cwd != NULL);

    // dir_list
    size_t count;
    char** files = dir_list(cwd, &count);
    assert(files != NULL);

    for (size_t i = 0; i < count; i++) {
        printf("%s\n", files[i]);
        free(files[i]);
    }

    free(files);

    // test path related functions
    const char* path = "./temp/tests/arena/test.txt";
    filepath_remove(path);
    assert(!path_exists(path));

    // path join
    const char* joined = filepath_join(dirname, "test.txt");
    assert(joined != NULL);
    // test path_exists
    printf("joined: %s\n", joined);
    assert(!path_exists(joined));

    // basename
    char basename[FILENAME_MAX];
    filepath_basename(joined, basename, FILENAME_MAX);
    assert(strcmp(basename, "test.txt") == 0);

    // filepath_dirname
    char dirname2[FILENAME_MAX];
    filepath_dirname(joined, dirname2, FILENAME_MAX);
    assert(strcmp(dirname2, dirname) == 0);

    // filepath_extension
    char ext[FILENAME_MAX];
    filepath_extension(joined, ext, FILENAME_MAX);
    assert(strcmp(ext, ".txt") == 0);

    // test filepath_nameonly
    char name[FILENAME_MAX];
    filepath_nameonly(joined, name, FILENAME_MAX);
    assert(strcmp(name, "test") == 0);

    // test filepath_absolute
    // first create the file
    FILE* file = fopen(joined, "w");
    assert(file != NULL);
    fclose(file);
    char* abs = filepath_absolute(joined);
    assert(abs != NULL);
    printf("abs: %s\n", abs);
    free(abs);

    // test filepath_rename
    const char* newpath = "./temp/tests/arena/test2.txt";
    assert(filepath_rename(joined, newpath) == 0);

    // rename back
    assert(filepath_rename(newpath, joined) == 0);

    // test filepath_expanduser
    char* home = filepath_expanduser("~");
    assert(home != NULL);
    free(home);

    // test filepath_expanduser_buf
    char expanded[FILENAME_MAX];
    assert(filepath_expanduser_buf("~", expanded, FILENAME_MAX));
    printf("expanded home: %s\n", expanded);

    // test filepath_join_buf
    char abspath[FILENAME_MAX];
    assert(filepath_join_buf(dirname, "test.txt", abspath, FILENAME_MAX));
    printf("filepath_join_buf: %s\n", abspath);

    // test filepath_split
    char dirbuf[FILENAME_MAX];
    char namebuf[FILENAME_MAX];
    filepath_split(joined, dirbuf, namebuf, FILENAME_MAX, FILENAME_MAX);

    assert(strcmp(dirbuf, dirname) == 0);
    assert(strcmp(namebuf, "test.txt") == 0);

    // test dir_size
    // Write some data to the file
    file = fopen(joined, "w");
    assert(file != NULL);
    fprintf(file, "Hello, World!");
    fclose(file);

    ssize_t size = dir_size(dirname);
    assert(size == 13);

    // test dir_walk
    assert(dir_walk(dirname, walk, NULL) == 0);

    // Delete the directory
    dir_remove("./temp", true);

    return 0;
}
