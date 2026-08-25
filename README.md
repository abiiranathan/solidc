# Solidc

Solidc is a collection of cross-platform C11 libraries for common systems tasks — memory management, containers, I/O, concurrency, networking, and math — shipped as a single static library (`libsolidc.a`) with minimal dependencies.

Supported toolchains: GCC, Clang, MSVC. Tested on Linux x86_64 & aarch64, Windows x64 (MSVC + MinGW), and macOS x64/arm64 (Clang). The only external dependency (`pcre2-8`) is vendored under `deps/`.

## Features

- **Cross-platform by design.** Every module compiles on Linux, Windows, macOS, and BSD from the same source. Platform differences live in `src/<module>/process_posix.c` / `process_win32.c` style backends behind a single public header (`file:12`).
- **Small surface, fast paths.** Arena allocator with per-thread recycle bins, Swiss-table maps, SIMD Vec2/Vec3/Vec4. Fast paths are opt-in, not mandatory.
- **Strict and tested.** `C11`, `-Wall -Wextra -Werror`, 35 CTest suites, ASan/UBSan-clean.

## Build

```bash
git clone https://github.com/abiiranathan/solidc.git
cd solidc

make              # native Release, build/ + ninja
make debug        # Debug
make test         # ctest --output-on-failure
make install      # CMAKE_INSTALL_PREFIX=/usr/local by default
```

Optional:

```bash
git config core.hooksPath .githooks   # clang-format staged .c/.h before commit
cmake -S . -B build -DBUILD_EXAMPLES=ON  # also build examples/
make check-platforms  # cross-check Linux aarch64 + Windows on a Linux host (needs aarch64-linux-gnu-gcc, mingw-w64-gcc)
```

CMake directly:

```cmake
find_package(solidc CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE solidc::solidc)
```

`pkg-config` file is installed when not building with MSVC.

## Usage

Headers live under `include/` and are installed as `<solidc/*.h>` when using `find_package`, or as `include/*.h` when building in-tree. The snippets below compile against an installed copy (`-lsolidc -lpcre2-8` for `regex.h` callers; other modules need only `-lsolidc`).

### 1. Walk a directory tree and skip hidden directories

```c
#include <solidc/filepath.h>
#include <stdio.h>

static WalkDirOption on_entry(const FileAttributes *a, const char *path,
                              const char *name, void *ud) {
    (void)a; (void)ud;
    if (fattr_is_dir(a)) {
        if (name[0] == '.') return DirSkip;
        printf("%s/\n", path);
    } else {
        printf("%s\n", path);
    }
    return DirContinue;
}

int main(void) {
    const char *home = user_home_dir();
    if (!home) return 1;
    if (dir_walk(home, on_entry, NULL) != 0)
        perror("dir_walk");
}
```

```bash
gcc walk.c -lsolidc -o walk && ./walk
```

Path helpers (`filepath_join`, `filepath_basename`, `filepath_expanduser`, …) accept both `/` and `\` on every platform.

### 2. Spawn a process and capture its output

```c
#include <solidc/process.h>
#include <stdio.h>

int main(void) {
    PipeHandle *out = NULL;
    pipe_create(&out);

    ProcessOptions opts = {0};
    opts.io.stdout_pipe = out;

    ProcessHandle *proc = NULL;
    const char *argv[] = {"echo", "hello from solidc", NULL};

    if (process_create(&proc, "echo", argv, &opts) != PROCESS_SUCCESS)
        return 1;

    ProcessResult r;
    process_wait(proc, &r, -1);
    process_free(proc);

    char buf[256];
    size_t n = 0;
    if (pipe_read(out, buf, sizeof(buf) - 1, &n, 1000) == PROCESS_SUCCESS) {
        buf[n] = '\0';
        printf("child said: %s r.exit_code=%d\n", buf, r.exit_code);
    }
    pipe_close(out);
}
```

`process.h`, `pipe_*`, and `filepath.h` share the same `src/<module>/<module>_posix.c` / `<module>_win32.c` split — callers see one API.

### 3. Parallel work with the thread pool

```c
#include <solidc/threadpool.h>
#include <stdio.h>

static void work(void *arg) {
    int *v = arg;
    *v *= 2;
}

int main(void) {
    Threadpool *tp = threadpool_create(4);
    int xs[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (int i = 0; i < 8; i++) threadpool_submit(tp, work, &xs[i]);
    threadpool_wait(tp);
    threadpool_destroy(tp);
    for (int i = 0; i < 8; i++) printf("%d ", xs[i]);
}
```

### 4. Vectors, matrices, and one batched transform

```c
#include <solidc/vec.h>
#include <solidc/matrix.h>
#include <solidc/linear_alg.h>

int main(void) {
    Vec3 eye = {0, 3, -5}, tgt = {0, 0, 0}, up = {0, 1, 0};
    Mat4 view = mat4_look_at(vec3_load(eye), vec3_load(tgt), vec3_load(up));
    Mat4 proj = mat4_perspective(1.0f, 16.0f/9.0f, 0.1f, 50.0f);
    Mat4 mvp  = mat4_mul(proj, view);

    FMat pts = fmat_create(2, 4);
    fmat_set(&pts, 0, 0, 0); fmat_set(&pts, 0, 3, 1);
    fmat_set(&pts, 1, 0, 1); fmat_set(&pts, 1, 3, 1);

    FMat clip = fmat_batch_transform(&mvp, &pts); /* N×4 in one GEMM */
    /* clip rows are now in clip space; divide xyz by w for NDC */
    fmat_destroy(&pts);
    fmat_destroy(&clip);
}
```

See `examples/graphics.c`, `examples/ml_prototype.c`, `examples/iris_classifier.c`, and `examples/sw_renderer.c` for larger programs.

## Modules

| Header | Purpose |
|---|---|
| `aligned_alloc.h`, `align.h` | Aligned allocation, alignment helpers |
| `arena.h` | Bump arena with per-thread recycle bins |
| `cache.h` | Tagged, sharded cache |
| `ckdint.h` | Checked integer arithmetic |
| `cmp.h` | Floating-point comparison |
| `cstr.h`, `str.h`, `str_slice.h` | Dynamic strings, slices, utilities |
| `csvparser.h` | CSV reader/writer |
| `dotenv.h` | `.env` loader |
| `dynarray.h` | Type-safe dynamic array (macro) |
| `env.h`, `platform.h` | Env and OS compat shims |
| `file.h`, `filepath.h` | Files, directories, paths |
| `flags.h` | CLI flag parser |
| `hash.h`, `hashset.h`, `map.h`, `swiss_map.h` | Hashing, sets, maps |
| `list.h`, `slist.h`, `trie.h` | Linked lists, trie |
| `lock.h`, `rwlock.h`, `spinlock.h` | Mutex, RW-lock, spinlock |
| `thread.h`, `threadpool.h` | Threads, work-stealing pool |
| `process.h`, `pipeline.h` | Process spawning, pipes, pipelines |
| `socket.h`, `epoll.h` | TCP sockets, epoll wrapper |
| `stdstreams.h` | `FILE*` / stream helpers |
| `unicode.h` | UTF-8 helpers |
| `vec.h`, `matrix.h`, `linear_alg.h`, `simd.h` | SIMD vectors/matrices, FMat, SVD, PCA, ML ops |
| `sort.h` | Introsort + radix specializations |
| `regex.h` | PCRE2 wrapper |
| `xtime.h` | Calendar / monotonic time |
| `prettytable.h`, `strsim.h`, `str_to_num.h` | Tables, string similarity, parsing |

## Testing, benchmarks, and docs

```bash
make test                  # CTest, 35 suites
make bench                 # micro-benchmarks (native only)
make check-platforms       # Linux x64 + cross aarch64/Windows; macOS/BSD skipped gracefully when SDK absent
doxygen Doxyfile           # HTML docs (requires doxygen)
```

All public and internal functions carry `/** ... */` doxygen comments.

## Cross-platform notes

- `filepath.h` and `process.h` expose one API; internals live in `src/filepath/` and `src/process/` with `*_posix.c` / `*_win32.c` backends selected by CMake.
- `win32_dirent.h` / `wintypes.h` provide `dirent` and POSIX-type shims on Windows.
- Clocks: `lock.c` and the examples use `xtime.h` (`xtime_now` / `xtime_diff_nanos`) instead of raw `clock_gettime` so they compile on Windows.
- `simd.h` selects SSE4.1 on x86_64, NEON on aarch64, scalar fallback otherwise.

## License

MIT — see [LICENSE](LICENSE).
