# falcon

Falcon is a robust collection of general purpose header-only C libraries and data structures designed for rapid development.

## Features

Falcon is designed to be a lightweight, easy-to-use, and efficient library for C developers. It includes a variety of features:

- **Header-only**: No need to compile or link against any library.
- **Cross-platform**: Works on Windows, Linux, and macOS.
- **Modern C**: Written in C11.
- **Data structures**: Includes a variety of data structures:
  - **Trie**: A trie data structure.
- **String manipulation**: Includes a variety of string manipulation functions:

## Usage

Falcon is a header-only library, so you can include the header files directly in your project.

Clone the repository with the following command:

```bash
git clone https://github.com/abiiranathan/falcon.git
```

Include the header files in your project or install system-wide with
cmake.

```bash
mkdir build && cd build
cmake ..
sudo cmake --install . --prefix=/usr/include
```

```c
#include <falcon/trie.h>
#include <falcon/string.h>
```

Build your project and link with the math library(-lm).

## Testing

All functions are tested with google test.

Run the tests with the following command:

```bash
make
```

## License

Falcon is licensed under the MIT license. See [LICENSE](LICENSE) for more information.
