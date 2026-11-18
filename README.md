# Solidc

Solidc is a collection of cross-platform C11 libraries for common systems tasks — memory management, containers, I/O, concurrency, networking, and math — shipped as a single static library (`libsolidc.a`) with minimal dependencies.

Supported toolchains: GCC, Clang, MSVC. Tested on Linux x86_64 & aarch64, Windows x64 (MSVC + MinGW), and macOS x64/arm64 (Clang). The only external dependency (`pcre2-8`) is vendored under `deps/`.

## Features

- **Cross-platform by design.** Every module compiles on Linux, Windows, macOS, and BSD from the same source. Platform differences live in `src/<module>/process_posix.c` / `process_win32.c` style backends behind a single public header (`file:12`).
- **Small surface, fast paths.** Arena allocator with per-thread recycle bins, Swiss-table maps, SIMD Vec2/Vec3/Vec4. Fast paths are opt-in, not mandatory.
- **Strict and tested.** `C11`, `-Wall -Wextra -Werror`, 39 CTest suites, ASan/UBSan-clean.

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

Headers live under `include/` and are installed as `<solidc/*.h>` when using `find_package`, or as `include/*.h` when building in-tree. The snippets below compile against an installed copy (`-lsolidc -lpcre2-8` for `regex.h` callers; other modules need only `-lsolidc`). When compiling manually on Linux/glibc, also pass `-D_GNU_SOURCE`; the CMake target propagates it automatically.

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
gcc -D_GNU_SOURCE walk.c -lsolidc -o walk && ./walk
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
    threadpool_destroy(tp, -1);
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

### 5. Arena-backed records in a Swiss-map

```c
#include <solidc/arena.h>
#include <solidc/swiss_map.h>
#include <stdio.h>

typedef struct { double score; } User;

int main(void) {
    enum { N = 4 };
    Arena* a = arena_create(0); /* 0 => default block size */
    int*   ids   = arena_alloc_array_zero(a, sizeof *ids,   ARENA_DEFAULT_ALIGN, N);
    User*  users = arena_alloc_array_zero(a, sizeof *users, ARENA_DEFAULT_ALIGN, N);

    SwissMap* by_id = swiss_create(SwissConfigInt);
    for (int i = 0; i < N; i++) {
        ids[i] = i;
        users[i].score = i * 0.5;
        /* The map stores pointers: ids/users stay valid until arena_destroy(). */
        swiss_set(by_id, &ids[i], sizeof ids[i], &users[i]);
    }

    User* u = swiss_get(by_id, &ids[2], sizeof ids[2]);
    if (u) printf("user 2 score: %.1f\n", u->score);

    swiss_destroy(by_id); /* no free callbacks set: keys/values remain ours */
    arena_destroy(a);     /* ids + users released in one shot */
}
```

### 6. Build text with `cstr` and parse numbers strictly

```c
#include <solidc/cstr.h>
#include <solidc/str_to_num.h>
#include <stdio.h>

int main(int argc, char** argv) {
    int n = 0;
    if (argc < 2 || str_to_int(argv[1], &n) != STO_SUCCESS) {
        fprintf(stderr, "usage: %s <count>\n", argv[0]);
        return 1;
    }

    cstr* line = cstr_format("count=%d, squares:", n);
    if (!line) return 1;

    for (int i = 1; i <= n; i++) {
        char item[32];
        snprintf(item, sizeof item, " %lld", (long long)i * i);
        cstr_append(line, item);
    }
    puts(cstr_data(line));

    cstr_free(line);
}
```

`str_to_int` rejects trailing garbage and reports overflow via `STO_OVERFLOW`/`STO_INVALID` — map it with `sto_error_string()` when reporting errors.

### 7. Read and write a whole file

```c
#include <solidc/file.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    file_t f;

    if (file_open(&f, "notes.txt", "w") != FILE_SUCCESS) return 1;
    file_write_string(&f, "hello from solidc\n");
    file_close(&f);

    if (file_open(&f, "notes.txt", "r") != FILE_SUCCESS) return 1;
    size_t len = 0;
    char* data = file_readall(&f, &len); /* malloc'd; free() it */
    file_close(&f);

    if (data) {
        fwrite(data, 1, len, stdout);
        free(data);
    }
}
```

### 8. Hand values between threads with a channel

```c
#include <solidc/channels.h>
#include <solidc/thread.h>
#include <stdio.h>

static void* producer(void* arg) {
    Channel* ch = arg;
    for (int i = 0; i < 5; i++) chan_send(ch, &i);
    chan_close(ch); /* drain remaining values, then recv returns false */
    return NULL;
}

int main(void) {
    Channel* ch = chan_new(sizeof(int), 0); /* value size, capacity hint */
    Thread t;
    thread_create(&t, producer, ch);

    int v;
    while (chan_recv(ch, &v)) printf("%d\n", v);

    thread_join(t, NULL);
    chan_free(ch);
}
```

Channels copy values by size and are safe for any number of producers and consumers.

### 9. Match and extract regex groups

```c
#include <solidc/regex.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    regex_t* re = NULL;
    regex_ctx_t* ctx = NULL;
    char err[128];

    if (regex_compile("(\\w+)@(\\w+\\.\\w+)", REGEX_FLAG_CASELESS, &re, err, sizeof err) != REGEX_OK) {
        fprintf(stderr, "regex_compile: %s\n", err);
        return 1;
    }
    if (regex_ctx_create(&ctx) != REGEX_OK) {
        regex_free(re);
        return 1;
    }

    const char* subject = "Contact: Ada@Example.com";
    regex_match_t m;
    if (regex_match(re, ctx, subject, &m) == REGEX_OK) {
        char* user = regex_group_dup(subject, &m, 1);
        char* host = regex_group_dup(subject, &m, 2);
        printf("user=%s host=%s\n", user ? user : "?", host ? host : "?");
        free(user);
        free(host);
    }

    regex_ctx_free(ctx);
    regex_free(re);
}
```

`regex_t` is shareable across threads; `regex_ctx_t` is per-thread.

### 10. Round-trip a CSV file

```c
#include <solidc/csvparser.h>
#include <stdio.h>

int main(void) {
    CsvWriter* out = csvwriter_new("people.csv");
    if (!out) return 1;
    csvwriter_write_row(out, (const char*[]){"name", "age"}, 2);
    csvwriter_write_row(out, (const char*[]){"Ada", "36"}, 2);
    csvwriter_write_row(out, (const char*[]){"Grace", "85"}, 2);
    csvwriter_free(out);

    CsvReader* in = csv_reader_new("people.csv", 0);
    if (!in) return 1;

    Row** rows = csv_reader_parse(in); /* owned by the reader's arena */
    size_t n = csv_reader_numrows(in);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < rows[i]->count; j++)
            printf("%s%s", rows[i]->fields[j], j + 1 < rows[i]->count ? "," : "\n");
    }

    csv_reader_free(in); /* frees the rows and fields too */
}
```

More end-to-end programs live in `examples/`; the tests in `tests/` are also small, complete programs worth copying from.

## Modules

| Header                                        | Purpose                                       |
| --------------------------------------------- | --------------------------------------------- |
| `aligned_alloc.h`, `align.h`                  | Aligned allocation, alignment helpers         |
| `arena.h`                                     | Bump arena with per-thread recycle bins       |
| `cache.h`                                     | Tagged, sharded cache                         |
| `ckdint.h`                                    | Checked integer arithmetic                    |
| `cmp.h`                                       | Floating-point comparison                     |
| `cstr.h`, `str.h`, `str_slice.h`              | Dynamic strings, slices, utilities            |
| `csvparser.h`                                 | CSV reader/writer                             |
| `dotenv.h`                                    | `.env` loader                                 |
| `dynarray.h`                                  | Type-safe dynamic array (macro)               |
| `env.h`, `platform.h`                         | Env and OS compat shims                       |
| `file.h`, `filepath.h`                        | Files, directories, paths                     |
| `flags.h`                                     | CLI flag parser                               |
| `hash.h`, `hashset.h`, `map.h`, `swiss_map.h` | Hashing, sets, maps                           |
| `list.h`, `slist.h`, `trie.h`                 | Linked lists, trie                            |
| `lock.h`, `rwlock.h`, `spinlock.h`            | Mutex, RW-lock, spinlock                      |
| `thread.h`, `threadpool.h`                    | Threads, work-stealing pool                   |
| `process.h`, `pipeline.h`                     | Process spawning, pipes, pipelines            |
| `socket.h`, `epoll.h`                         | TCP sockets, epoll wrapper                    |
| `stdstreams.h`                                | `FILE*` / stream helpers                      |
| `unicode.h`                                   | UTF-8 helpers                                 |
| `vec.h`, `matrix.h`, `linear_alg.h`, `simd.h` | SIMD vectors/matrices, FMat, SVD, PCA, ML ops |
| `sort.h`                                      | Introsort + radix specializations             |
| `regex.h`                                     | PCRE2 wrapper                                 |
| `xtime.h`                                     | Calendar / monotonic time                     |
| `prettytable.h`, `strsim.h`, `str_to_num.h`   | Tables, string similarity, parsing            |

## Testing, benchmarks, and docs

```bash
make test                  # CTest, 39 suites
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
