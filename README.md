# **Solidc**

**Solidc** is a robust collection of general-purpose cross-platform C libraries and data structures designed for rapid and safe development in C.

---

## ✨ Features

- **Cross-platform**: Works on Linux, Windows (via MinGW), and WebAssembly (via Emscripten).
- **Lightweight & Efficient**: Minimal dependencies and fast execution.
- **Well-tested**: Every module is tested and follows strict C standards.
- **Modular**: Choose and link only the modules you need.
- **Modern C (C23)**: Fully compatible with the latest C standards.

---

## ⚙️ Build System

Solidc uses a flexible `Makefile` and `CMake` setup with support for:

### 🔧 Configurable Makefile Variables

```make
# CROSS_COMPILER: Path to the cross-compiler for Windows builds (MinGW)
CROSS_COMPILER ?= /usr/bin/x86_64-w64-mingw32-cc

# SYSTEM_NAME: Target system for CMake.
# Allowed values: Linux, Windows, WebAssembly (WASI)
SYSTEM_NAME ?= Linux

# RELEASE: Build type.
# Allowed values: Debug, Release
RELEASE ?= Debug

# TARGET: Target environment.
# Allowed values:
#   native  - Build for the host system (e.g., Linux)
#   cross   - Cross-compile for Windows using MinGW
#   wasm    - Compile to WebAssembly using Emscripten
TARGET ?= native
```

---

## 🔨 Installation

### 1. Clone the Repository

```bash
git clone https://github.com/abiiranathan/solidc.git
cd solidc
```

### 2. Build with Make (Recommended)

```bash
make        # Default build: native Debug
make release    # Release build
make cross      # Cross-compile for Windows
make wasm       # Build for WebAssembly (requires emcc)
sudo make install
```

You can customize the build:

```bash
make TARGET=cross SYSTEM_NAME=Windows CROSS_COMPILER=/usr/bin/x86_64-w64-mingw32-cc
make TARGET=wasm SYSTEM_NAME=WebAssembly WASM_CC=emcc
```

---

## 🧱 Dependencies

| Platform                                     | Command                                                                              |
| -------------------------------------------- | ------------------------------------------------------------------------------------ |
| **Arch Linux**                               | `sudo pacman -S xxhash mimalloc`                                                     |
| **Manjaro / AUR**                            | `yay -S xxhash mimalloc` <br>or `paru -S xxhash mimalloc`                            |
| **Manjaro / MingW**                          | `paru -S mingw-w64-mimalloc mingw-w64-xxhash`                                        |
| **Debian / Ubuntu**                          | `sudo apt update && sudo apt install libxxhash-dev libmimalloc-dev`                  |
| **Linux (via package manager, alternative)** | `sudo apt install mimalloc` (Ubuntu 24.04+, Debian 13+) <br>or build from source     |
| **Fedora**                                   | `sudo dnf install xxhash-libs mimalloc mimalloc-devel`                               |
| **openSUSE**                                 | `sudo zypper install libxxhash-devel mimalloc-devel`                                 |
| **AlmaLinux / RHEL / Rocky**                 | `sudo dnf install epel-release && sudo dnf install xxhash mimalloc mimalloc-devel`   |
| **macOS (Homebrew)**                         | `brew install xxhash mimalloc`                                                       |
| **Windows (vcpkg)**                          | `vcpkg install xxhash mimalloc`                                                      |
| **Windows (MSYS2)**                          | `pacman -S mingw-w64-x86_64-xxhash mingw-w64-x86_64-mimalloc`                        |
| **Conan**                                    | `conan install xxhash/0.8.2@ conan install mimalloc/2.1.2@`                          |
| **Build from source**                        | See [(https://github.com/microsoft/mimalloc)](https://github.com/microsoft/mimalloc) |


---

## 📦 Library Modules

| Header         | Description                                   |
| -------------- | --------------------------------------------- |
| `arena.h`      | Fast memory arena allocator                   |
| `cstr.h`       | String utilities                              |
| `file.h`       | Cross-platform file handling (sync & async)   |
| `filepath.h`   | File path manipulation utilities              |
| `list.h`       | Doubly linked list                            |
| `lock.h`       | Mutex abstraction                             |
| `map.h`        | Generic hash map                              |
| `safe_map.h`   | Type-safe, thread-safe hash map using macros  |
| `hashset.h`    | Generic set                                   |
| `slist.h`      | Singly linked list                            |
| `thread.h`     | Thread creation & management                  |
| `threadpool.h` | Simple thread pool implementation             |
| `process.h`    | Process management utilities                  |
| `optional.h`   | Optional type implementation                  |
| `socket.h`     | Cross-platform TCP socket library             |
| `vec.h`        | Dynamic array                                 |
| `hash.h`       | Fast, non-cryptographic hash functions        |
| `strton.h`     | String to number converters                   |
| `stdstreams.h` | Read/write helpers for stdout/stderr/FILE\*   |
| `unicode.h`    | UTF-8 string helpers                          |
| `defer.h`      | Cross-platform defer implementation           |
| `solidc.h`     | Master header that includes all above modules |
| `vec.h`        | Fast Vec2 and Vec3 vector ops with SIMD       |
| `matrix.h`     | Fast Mat3 and Mat4 matrix ops with SIMD       |
| `flags.h`      | Robust Command-Line parsing library           |

This is not an exhaustive list. See [Includes](./include) for a full list.
---

## 🧪 Example: Directory Walk

```c
#include <solidc/filepath.h>

WalkDirOption callback(const char* path, const char* name, void* data) {
    (void)data;
    if (is_dir(path)) {
        if (name[0] == '.') return DirSkip; // skip hidden dirs
        printf("%s/\n", path);
        return DirContinue;
    } else {
        printf("%s\n", path);
        return DirContinue;
    }
}

int main(void) {
    dir_walk(user_home_dir(), callback, NULL);
}
```

### Compile & Run:

```bash
gcc -Wall -Wextra -pedantic -o walkhome walkhome.c -lsolidc
./walkhome
```

---

## 🛠️ Linking with CMake

```cmake
find_package(solidc REQUIRED)
target_link_libraries(your_target PRIVATE solidc::solidc)
```

---

## 🧪 Testing

```bash
make test
```

---

## 🧪 Benchmarking & Profiling

```bash
make bench  # Native only
make perf   # Requires Linux perf tool
```

---

## 📝 License

**MIT License** – See the [LICENSE](LICENSE) file for full terms.

---

## 🤝 Contributing

1. Fork the repo
2. Create your feature branch: `git checkout -b awesome-feature`
3. Commit your changes: `git commit -am 'Add feature'`
4. Push to the branch: `git push origin awesome-feature`
5. Open a pull request 🚀

---
