# AGENTS.md

This file is for contributors and for automated agents working in this repository. It describes what solidc is, how it is organized, and how to work with it safely.

## What solidc is

Solidc is a **cross-platform C11 library** — a collection of small, focused modules (memory, containers, I/O, concurrency, networking, math) shipped as a single static library `libsolidc.a` (`solidc::solidc` CMake target). The only external dependency is `pcre2-8`, vendored under `deps/pcre2-10.47` and `deps/xxhash`.

Design goals: one public API per feature that compiles unchanged on Linux (x86_64, aarch64), Windows (MSVC and MinGW), macOS (x64, arm64), and BSD.

## Repository layout

```
include/          public headers (installed as <solidc/*.h> via find_package,
                  or as include/*.h in-tree)
src/              one .c (or more) per module; see platform split below
  src/filepath/   filepath helpers + _posix.c / _win32.c backends
  src/process/    process/pipe logic + _posix.c / _win32.c backends
src/arena.c, src/cstr.c, src/map_swiss.c, ...  single-file modules
deps/             vendored pcre2-10.47, xxhash (built as OBJECT / STATIC)
tests/            one CTest executable per module (ctest --output-on-failure)
benchmarks/       micro-benchmarks (native Linux x64 only)
examples/         standalone CMake project; also built in-tree with -DBUILD_EXAMPLES=ON
build/            out-of-tree CMake build dir (ignored by .git)
.githooks/        pre-commit hook (clang-format)
```

## Cross-platform architecture

Most modules are single-file. Two modules that touch OS APIs are split:

```
src/filepath/filepath.c          # platform-neutral logic
src/filepath/filepath_string.c   # pure string helpers
src/filepath/filepath_posix.c    # POSIX backend
src/filepath/filepath_win32.c    # Win32 backend
src/filepath/filepath_internal.h # shared contract (PipeHandle layout, etc.)

src/process/process.c            # portable logic (error strings, accessors)
src/process/process_posix.c      # fork/exec, select()-based pipes
src/process/process_win32.c      # CreateProcess + overlapped I/O
src/process/process_internal.h   # shared contract
```

`CMakeLists.txt` selects the backend at configure time:

```cmake
if(WIN32 OR MINGW OR CMAKE_C_COMPILER MATCHES "mingw")
  list(APPEND SOURCES src/filepath/filepath_win32.c src/process/process_win32.c)
else()
  list(APPEND SOURCES src/filepath/filepath_posix.c src/process/process_posix.c)
endif()
```

Other platform shims live in `include/platform.h`, `include/wintypes.h`, and `include/win32_dirent.h`. The `examples/` and any new code should prefer the library's own helpers — e.g. `xtime.h` (`xtime_now` / `xtime_diff_nanos`) over raw `clock_gettime`, which does not exist on Windows.

### SIMD

`include/simd.h` selects SSE4.1 on x86_64, NEON on aarch64, and a scalar fallback otherwise. The ASCII case-conversion helpers in `simd.h` expose the pattern: keep per-architecture blocks small and testable.

## Building and checking

```bash
make                    # configure (build/) + build, native Release
make test               # ctest, 35 suites, single-threaded
make bench              # micro-benchmarks (native only)

# Cross-platform surface — the CI matrix in one command.
# Each cross-check is skipped gracefully when its toolchain/SDK is absent
# (plain Linux dev box) and exhaustive on CI (every runner is native).
make check-platforms    # linux x64 + cross aarch64/Windows + native macOS/BSD

# Individual cross checks
make check-linux
make check-linux-aarch64   # needs aarch64-linux-gnu-gcc
make check-windows         # needs x86_64-w64-mingw32-gcc  (sudo pacman -S mingw-w64-gcc)
make check-macos           # Darwin only
make check-bsd             # FreeBSD/OpenBSD only

cmake -S . -B build -DBUILD_EXAMPLES=ON   # also build examples/
cmake -S . -B build -DBUILD_TESTS=OFF     # library only
```

CMake options: `BUILD_TESTS` (ON), `BUILD_BENCHMARKS` (ON), `BUILD_EXAMPLES` (OFF), `BUILD_SHARED_LIBS` (OFF).

Standalone examples build against an *installed* copy:

```bash
cmake -S examples -B /tmp/ex && cmake --build /tmp/ex
```

In-tree builds (via `BUILD_EXAMPLES`) link against the just-built `solidc` target instead — use them when validating library changes.

## Conventions

- **Language:** `C11`, `-std=gnu11` (or `gnu11` via CMake). MSVC builds add `/experimental:c11atomics` and `/W4`.
- **Warnings:** `-Wall -Wextra -Werror` on solidc targets. Keep the tree warning-free on every platform.
- **Formatting:** `clang-format` (LLVM style, no repo-owned `.clang-format`). Enable the hook once per clone:

  ```bash
  git config core.hooksPath .githooks
  ```

  It formats staged `.c`/`.h` and re-stages the result; it is a no-op without `clang-format`.

- **Documentation:** every function in `include/` and `src/` carries a `/** ... */` doxygen comment (`@brief`, `@param`, `@return`, `@note`/`@warning` where useful). CI runs `doxygen Doxyfile` — malformed comments break it.
- **String handling:** prefer `memcpy` + explicit NUL and the `_s` variants on Windows over `strcat`/`strcpy`/`strncpy`/`sprintf`.
- **Commits:** keep platform backends, docs, and behavior changes in separate commits where practical.

## Adding a new module

1. Add `include/<name>.h` (public API, `#ifndef <NAME>_H`, `extern "C"` guard, doxygen).
2. Add `src/<name>.c` (or `src/<name>/<name>.c` + `_<platform>.c` if it needs OS backends).
3. Register sources in the top-level `CMakeLists.txt` `SOURCES` list (and the `WIN32 OR MINGW` block if split).
4. Add `tests/<name>_test.c` and a `add_test` entry.

## Gotchas for agents

- Do not use `strcat`/`strcpy`/`sprintf`/`strncpy`/`gets` — the tree is clean of them by policy.
- Do not use `__builtin_shufflevector` on GCC/ARM (clang-only); use `__builtin_shuffle` or the NEON fallback in `simd.h`.
- Do not assume dirent/POSIX clock APIs exist on Windows; go through `filepath.h`/`process.h`/`xtime.h`.
- `max_align_t` is unavailable in MSVC C mode — use a local alignment struct instead.
- `fread_unlocked`/`fwrite_unlocked` exist on glibc/BSD but **not** macOS (only the char-level `getc_unlocked` does) — see `src/stdstreams.c`.
- `pthread_condattr_setclock` does not exist on macOS — see `src/lock.c` for the `COND_CLOCK` pattern.
- Touching `.githooks/` or `Makefile` check targets affects every platform — verify with `make check-platforms`.
