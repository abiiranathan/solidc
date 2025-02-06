# solidc

Solidc is a robust collection of general purpose cross-platform C libraries and data structures designed for rapid C development.

## Features

Solidc is designed to be a lightweight, easy-to-use, and efficient library for C developers.

All the functions are implemented in a way that they are safe and easy to use
and are fully tested.

Available Headers:

1. [arena.h](./include/arena.h) - Arena allocator for fast memory allocation.
1. [cstr.h](./include/cstr.h) - A simple string library with a lot of utility functions.
1. [file.h](./include/file.h) - Cross-platform file handling library for synchronous and asynchronous file operations.
1. [filepath.h](./include/filepath.h) - File path utility functions.
1. [list.h](./include/list.h) - Doubly linked list implementation.
1. [lock.h](./include/lock.h) - Cross-platform mutex abstraction.
1. [map.h](./include/map.h) - Generic hash map implementation.
1. [optional.h](./include/optional.h) - A simple optional type implementation.
1. [process.h](./include/process.h) - Cross-platform process creation and management.
1. [safe_map.h](./include/safe_map.h) - A type-safe and thread-safe hash map implementation implemented with a macro.
1. [set.h](./include/set.h) - A generic set implementation.
1. [slist.h](./include/slist.h) - Singly linked list implementation.
1. [socket.h](./include/socket.h) - Cross-platform socket library for TCP communication.
1. [solidc.h](./include/solidc.h) - A single header file that includes all the headers.
1. [thread.h](./include/thread.h) - Cross-platform thread library for creating and managing threads.
1. [threadpool.h](./include/threadpool.h) - A simple but roubust thread pool implementation.
1. [unicode.h](./include/unicode.h) - UTF-8 string utility functions.
1. [vec.h](./include/vec.h) - A simple dynamic array implementation.
1. [hash.h](./include/hash.h) - A collection of fast non-cryptographic hash functions.
1. [strton.h](./include/strton.h) - A collection of functions to convert strings to numbers.
1. [stdstreams.h](./include/stdstreams.h) - A collection of functions to read and write to standard streams, FILE\* streams, convert strings to streams, io_copy, io_copy_n between streams, etc.

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
