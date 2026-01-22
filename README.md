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

| Header            | Description                                                                 |
| ----------------- | --------------------------------------------------------------------------- |
| `aligned_alloc.h` | Aligned memory allocation functions                                         |
| `align.h`         | Memory alignment utilities and macros                                       |
| `arena.h`         | Fast memory arena allocator                                                 |
| `cache.h`         | Cache management utilities                                                  |
| `ckdint.h`        | Checked integer arithmetic to prevent overflow                              |
| `cmp.h`           | Floating-point comparison library                                           |
| `cstr.h`          | String utilities                                                            |
| `csvparser.h`     | CSV Parser Library                                                          |
| `defer.h`         | Cross-platform defer implementation                                         |
| `dotenv.h`        | Environment variable loading from .env files                                |
| `dynarray.h`      | Dynamic array implementation                                                |
| `env.h`           | Environment variable utilities                                              |
| `epoll.h`         | Cross-platform Epoll abstraction                                            |
| `file.h`          | Cross-platform file handling (sync & async)                                 |
| `filepath.h`      | File path manipulation utilities                                            |
| `flags.h`         | Robust Command-Line parsing library                                         |
| `global.h`        | Global variable management and DLL export definitions                       |
| `hash.h`          | Fast, non-cryptographic hash functions                                      |
| `hashset.h`       | Generic hash set                                                            |
| `linear_alg.h`    | Linear algebra operations                                                   |
| `list.h`          | Doubly linked list                                                          |
| `lock.h`          | Mutex abstraction                                                           |
| `macros.h`        | Utility macros for assertions, memory, and debugging                        |
| `map.h`           | Generic hash map                                                            |
| `matrix.h`        | Fast Mat3 and Mat4 matrix ops with SIMD                                     |
| `pipeline.h`      | Data pipeline utilities for stream processing                               |
| `platform.h`      | Cross-platform compatibility definitions                                    |
| `prettytable.h`   | Generic pretty table printer library                                        |
| `process.h`       | Process management utilities                                                |
| `rwlock.h`        | Read-write lock implementation                                              |
| `simd.h`          | Portable SIMD Intrinsics Wrapper                                            |
| `slist.h`         | Singly linked list                                                          |
| `socket.h`        | Cross-platform TCP socket library                                           |
| `solidc.h`        | Master header including all modules                                         |
| `spinlock.h`      | High-performance reader-writer spinlock                                     |
| `stdstreams.h`    | Read/write helpers for stdout/stderr/FILE*                                  |
| `str_to_num.h`    | String to number conversion utilities                                       |
| `str_utils.h`     | String utility functions                                                    |
| `thread.h`        | Thread creation & management                                                |
| `threadpool.h`    | Simple thread pool implementation                                           |
| `trie.h`          | Trie data structure implementation                                          |
| `unicode.h`       | UTF-8 string helpers                                                        |
| `vec.h`           | Fast Vec2 and Vec3 vector ops with SIMD                                     |
| `win32_dirent.h`  | dirent.h implementation for Windows                                         |
| `wintypes.h`      | Windows-specific type definitions                                           |
| `xtime.h`         | Extended time utilities and high-precision timing                           |

---

## 📚 Documentation

Detailed documentation for each module category:

- [**Arena Allocator**](docs/arena.md) - High-performance memory arena.
- [**Containers**](docs/containers.md) - List, Map, Set, Array, Trie.
- [**String Manipulation**](docs/strings.md) - Dynamic strings, Unicode, Utilities.
- [**I/O & System**](docs/io.md) - Files, Paths, CLI Flags, CSV, DotEnv.
- [**Concurrency**](docs/concurrency.md) - Threads, ThreadPool, Locks.
- [**Network**](docs/network.md) - Sockets, Epoll.
- [**Math & Numerics**](docs/math.md) - Vectors, Matrices, Checked Math.
- [**System Utilities**](docs/system.md) - Process, Time, Defer, Macros.

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
