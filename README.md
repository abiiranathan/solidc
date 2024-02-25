# solidc

Solidc is a robust collection of general purpose header-only C libraries and data structures designed for rapid C development.

## Features

Solidc is designed to be a lightweight, easy-to-use, and efficient library for C developers. It includes a variety of features:

All the functions are implemented in a way that they are safe and easy to use
and are fully tested with google test.

Available Headers:

- [solidc.h](solidc.h) - The main header file that includes all the other headers.
- [os.h](os.h) - File I/O operations, threads, threadpool, pipes, filepath, and directory operations.

- [list.h](list.h) - A doubly linked list implementation.
- [slist.h](slist.h) - A singly linked list implementation.
- [log.h](log.h) - A simple logging library.
- [map.h](map.h) - A hash map implementation.
- [ordered_map.h](ordered_map.h) - An ordered map implementation.
- [socket.h](socket.h) - A simple socket library.
- [str.h](str.h) - A string library with a variety of string manipulation functions(al most all functions are implemented in a way that they are safe to use).
- [trie.h](trie.h) - A trie implementation for strings.
- [vec.h](vec.h) - A dynamic array(vector) implementation.
- [set.h](set.h) - Defines `SET_DEFINE` macro to define a type-safe set.

If you want to include all the headers, you can include the main header file:

```c
#define SOLIDC_IMPL
#include <solidc/solidc.h>
```

Otherwise, you can include the individual headers as needed.

```c
#define TRIE_IMPL
#include <solidc/trie.h>

#define STR_IMPL
#include <solidc/str.h>

#define VEC_IMPL
#include <solidc/vec.h>

// No need to define SET_IMPL for the set headers
// since it's macro-based.
#include <solidc/set.h>

#define MAP_IMPL
#include <solidc/map.h>

#define ORDERED_MAP_IMPL
#include <solidc/ordered_map.h>

#define LIST_IMPL
#include <solidc/list.h>

#define SLIST_IMPL
#include <solidc/slist.h>

#define LOG_IMPL
#include <solidc/log.h>

#define OS_IMPL
#include <solidc/os.h>

#define SOCKET_IMPL
#include <solidc/socket.h>
```

## Usage

Solidc is a header-only library, so you can include the header files directly in your project.

Clone the repository with the following command:

```bash
git clone https://github.com/abiiranathan/solidc.git
```

Include the header files in your project or install system-wide with
cmake.

```bash
mkdir build && cd build
cmake ..
sudo cmake --install . --prefix=/usr/include
```

```c
#include <solidc/trie.h>
#include <solidc/string.h>
```

Build your project and link with the math library(-lm).

## Testing

All functions are tested with google test.
Install google test with the following command:

Ubuntu:

```bash
sudo apt-get install libgtest-dev
```

Arch Linux:

```bash
sudo pacman -S gtest
```

Run the tests with the following command:

```bash
make test
```

## Docs

The quick and probably most easy way to get started
is to read the test files at [tests](./tests/); since all the functions are tested.

I will be providing docs in the due course.

## License

Solidc is licensed under the MIT license. See [LICENSE](LICENSE) for more information.
