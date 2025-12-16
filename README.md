# **Solidc**

**Solidc** is a robust collection of general-purpose cross-platform C libraries and data structures designed for rapid and safe development in C.

---

## ✨ Features

- **Cross-platform**: Works on Linux(and FreeBSD), Windows, Mac(OSX and ARM64).
- **Lightweight & Efficient**: Minimal dependencies and fast execution.
- **Well-tested**: Every module is tested and follows strict C 11 standards.
- **Modular**: Modular design for ease of extension and maintenance.
- **Support for C11**: Compiles on any modern C11 compiler for maximum portability.

---

## ⚙️ Build System

Solidc uses `Cmake` as the build system. A Makefile only exists to drive the cmake build
pipeline.

---

## 🔨 Installation

### 1. Clone the Repository

```bash
git clone https://github.com/abiiranathan/solidc.git
cd solidc
```

### 2. Build with CMake (Driven by the Makefile)

```bash
make            # Default build: native Debug
make release    # Release build or debug
make install    # May require root if prefix is /usr/local
```

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
