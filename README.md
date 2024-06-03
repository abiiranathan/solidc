# solidc

Solidc is a robust collection of general purpose cross-platform C libraries and data structures designed for rapid C development.

## Features

Solidc is designed to be a lightweight, easy-to-use, and efficient library for C developers.

All the functions are implemented in a way that they are safe and easy to use
and are fully tested.

Available Headers:
1. [arena.h](./arena.h) - Arena allocator for fast memory allocation.
1. [cstr.h](./cstr.h) -  A simple string library with a lot of utility functions.
2. [file.h](./file.h) - Cross-platform file handling library for synchronous and asynchronous file operations.
3. [filepath.h](./filepath.h) - File path utility functions.
4. [list.h](./list.h) - Doubly linked list implementation.
5. [lock.h](./lock.h) - Cross-platform mutex abstraction.
6. [map.h](./map.h) - Generic hash map implementation.
7. [optional.h](./optional.h) - A simple optional type implementation.
8. [process.h](./process.h) - Cross-platform process creation and management.
9. [safe_map.h](./safe_map.h) -  A type-safe and thread-safe hash map implementation implemented with a macro.
10. [set.h](./set.h) - A generic set implementation.
11. [slist.h](./slist.h) - Singly linked list implementation.
12. [socket.h](./socket.h) -  Cross-platform socket library for TCP communication.
13. [solidc.h](./solidc.h) - A single header file that includes all the headers.
14. [thread.h](./thread.h) - Cross-platform thread library for creating and managing threads.
15. [threadpool.h](./threadpool.h) - A simple but roubust thread pool implementation.
16. [unicode.h](./unicode.h) - UTF-8 string utility functions.
17. [vec.h](./vec.h) -  A simple dynamic array implementation.

## Installation

```bash
git clone https://github.com/abiiranathan/solidc.git
cd solidc
mkdir -p build
cd build
cmake ..
make
sudo cmake --install .
```


## Usage

```c
#include <solidc/filepath.h>

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
    dir_walk(dirname, callback, NULL);
}

// compile: gcc -Wall -Wextra -pedantic -o walkhome solidc.c -lsolidc
// run: ./walkhome
```

## Linking with cmake

```cmake
find_package(solidc REQUIRED)
target_link_libraries(your_target PRIVATE solidc::solidc)
```

## License

This project is licensed under the MIT License.

See the [LICENSE](LICENSE) file for details.

## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request
