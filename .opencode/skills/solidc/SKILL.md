---
name: solidc
description: 'Write, review, or modify C11 code that uses the solidc cross-platform library (libsolidc.a / solidc::solidc). Use when adding solidc modules, using solidc headers (arena, cstr, vec, matrix, swiss_map, threadpool, process, filepath, socket, poller, channels, regex, sort, etc.), writing tests or examples in this repo, or when asked for fast, allocation-free, and secure C. Triggers on "solidc", "libsolidc", "solidc::solidc", "libsolidc.a", "<solidc/*.h>", include/ or src/ edits, and any task in /home/nabiizy/Code/C/solidc.'
license: MIT
---

# Writing solidc code

solidc is a cross-platform **C11** library of small modules (memory, containers,
I/O, concurrency, networking, math, strings) shipped as one static library
`libsolidc.a` (CMake target `solidc::solidc`). Only external dep is vendored
PCRE2 (`regex.h`). The `build`/`plan` agents should load the reference files
below before writing code.

## Golden rules

1. **C11 only.** `types.h` hard-errors below C11. No C++ features, no VLAs in
   library code, no `//`-only assumptions that break MSVC. GNU extensions are
   allowed only where the repo already uses them (`-std=gnu11`).
2. **Never use `strcat`/`strcpy`/`strncpy`/`sprintf`/`gets`.** Tree policy.
   Use `snprintf`/`SOLIDC_SNPRINTF`, `cstr_*`, `str_*`, or explicit `memcpy` +
   NUL. Prefer the `_s` wrappers on Windows via `SOLIDC_STRCPY_S`.
3. **Match the ownership contract exactly** (see reference files). Every
   allocator has a matching destructor: `arena_create`→`arena_destroy`,
   `fmat_create`→`fmat_destroy`, `cstr_new`→`cstr_free`, `map_create`→
   `map_destroy`, `threadpool_create`→`threadpool_destroy`, etc.
4. **No custom allocator hooks exist.** Only the arena and the modules built on
   it (trie, csvparser, flags) are arena-backed. Everything else uses libc
   `malloc`/`free`.
5. **Doxygen every public function** in `include/` and `src/` with
   `@brief`/`@param`/`@return` (`@note`/`@warning` where useful). CI runs
   `doxygen Doxyfile`.
6. **Zero warnings.** `-Wall -Wextra -Werror` on Unix; keep MSVC `/W4` clean.
   Run `make` (or `cmake --build`) before declaring done.

## Allocation-free by default

Order of preference for the fast path:

1. **Stack / caller-provided buffer** — value types (`Vec3`, `Mat4`, `xtime_t`,
   `regex_match_t`, `PipeRead`...). All vector/matrix/SIMD/cmp/checked-int
   operations are header-only, inline, and allocate nothing.
2. **`Arena`** — bump allocation, O(1) reset, per-thread recycle bins. Use for
   scratch/parse/request lifetimes, temporary strings, and returned arrays.
   ```c
   Arena* a = arena_create(0);                 /* 0 => default */
   char* s = arena_strdup(a, name);            /* no per-alloc free needed */
   int* xs = arena_alloc_array_zero(a, sizeof *xs, ARENA_DEFAULT_ALIGN, n);
   /* ... */
   arena_destroy(a);                           /* one shot */
   ```
   Stack form: `Arena a; arena_init(&a, buf, sizeof buf);` then `arena_destroy(&a)`
   (does **not** free `buf`). Use `arena_save`/`arena_restore` for O(1) scoped
   reuse, `arena_reset` to rewind.
   `ARENA_ABORT_ON_OOM` is defined for the library build, so `arena_alloc*`
   return `NULL` only for `size == 0` or arithmetic overflow — prefer
   `arena_alloc_zero`/`arena_alloc_array_zero`.
3. **Type-erased `dynarray_t`** — one growable buffer, reserve up front to avoid
   reallocs. `dynarray_detach` hands out a `malloc`'d exact-size buffer.
4. **Fixed-capacity, single-allocation containers** for the hot path:
   `hashset_t` (keys copied inline, no per-element allocation),
   `SwissMap`/`HashMap` with pre-sized capacity, `cache_t`.
5. **libc `malloc`** — only when no arena applies or ownership must cross an
   API boundary. Free immediately, prefer `defer_free`.

Concurrency fast paths: `threadpool_submit_batch` is the preferred bulk path
(it amortizes the global mutex and does no per-task allocation); never call
`threadpool_wait`/`destroy` from inside a task. `fast_rwlock_t` for critical
sections under ~100 µs.

Other performance levers: `sol_sort_i32/i64/f64` (radix for n ≥ 1024),
`simd_*`/`Vec2/3/4` typed vertical ops, `fmat_batch_transform` (one GEMM for
N points), `Cache`/`SwissMap`/`hashset` over ad-hoc linear scans, and checked
arithmetic (`ckd_add`/`ckd_mul`) to avoid branchy guards.

## Secure by default

- **Zero secrets** with `SOLIDC_SECURE_ZERO(p, n)` / `solidc_secure_zero`
  (not elidable), and `cstr_wipe`/`str_wipe` for wiped string lifetimes.
- **Validate untrusted input** with `str_to_num.h` (`STO_OVERFLOW`/`STO_INVALID`)
  or `str_slice.h` (`SS_OK`/`SS_OVERFLOW`) instead of `atoi`/`strtol` without
  end-pointer checks.
- **Bounds-check**: prefer `str_slice`, `cstr` (SSO, length-tracked),
  `dynarray_get` (OOB → `NULL`). `fmat_get`/`fmat_set` and `DYNARRAY_GET_AS` are
  **unchecked** — validate indices yourself.
- **TOCTOU**: `is_dir`/`is_file`/`path_exists`/`filepath_absolute` are stat-based
  and follow symlinks. Do not use them as an authorization gate on
  attacker-writable paths; use `is_symlink` (POSIX `lstat`) and open with
  `O_NOFOLLOW`/relative dirfds where it matters.
- **Temp files**: `make_tempfile()` uses `mkstemp` + random prefix, mode `0600`.
  Never build predictable temp names with `sprintf`.
- **No `exit()` in library code** (`regex_must_compile`, `pipeline_*` do — do
  not call these from reusable code). Prefer the status-returning variants.
- **Do not include both `regex.h` and `unicode.h`** in one TU: both declare
  `regex_match` with different signatures. `unicode.h`'s declaration has no
  implementation; use `regex.h`.
- Cap resource use: `REGEX_MAX_GROUPS`, `CACHE_MAX_VALUE_LEN`, channel
  capacities, `pipeline` stage limits.

## Error handling conventions

- POSIX-style: `0`/positive on success, `-1` on failure (check `errno`).
- Explicit enums: `ProcessError`, `FILE_*`, `ChanStatus`, `StoError`,
  `xtime_error_t`, `regex_status_t`, `FlagStatus`. Always map with the module's
  `*_error_string`/`*_strerror`.
- `bool` APIs return `false` on invalid args **and** allocation failure.
- `WARN_UNUSED_RESULT`-annotated calls (mostly `filepath.h`) must be checked.

## Include and build rules

- Installed/external code: `#include <solidc/arena.h>` and link `solidc::solidc`.
- In-tree (tests, examples, `src/`): include via the target's include dir
  (`#include <arena.h>`) or the repo-relative form used by tests/examples
  (`#include "../include/arena.h"`). There is no `include/solidc/` directory.
- `_GNU_SOURCE`/`_DARWIN_C_SOURCE`/`_WIN32_WINNT` and platform backends are
  handled by CMake; do not re-define them. On Windows pull Windows APIs through
  `<solidc/platform.h>` (keeps `winsock2.h` before `windows.h`).
- New module: add `include/<name>.h` + `src/<name>.c` (+ `_posix.c`/`_win32.c`
  if it touches the OS), register sources in the top-level `CMakeLists.txt`
  (`SOURCES` and the `WIN32 OR MINGW` block if split), add `tests/<name>_test.c`
  and a `TEST_MODULES` entry.

## Canonical example

Fast, secure, arena-first, no leaks:

```c
#include <solidc/arena.h>
#include <solidc/swiss_map.h>
#include <solidc/str_to_num.h>
#include <solidc/cmp.h>
#include <solidc/macros.h>
#include <stdio.h>

typedef struct { double score; } User;

int main(int argc, char** argv) {
    int n = 8;
    /* Parse untrusted argv strictly: no atoi, no unchecked strtol. */
    if (argc > 1 && str_to_int(argv[1], &n) != STO_SUCCESS) {
        fprintf(stderr, "bad count\n");
        return 1;
    }

    Arena* arena = arena_create(0);                 /* one owner for scratch */
    int*   ids   = arena_alloc_array_zero(arena, sizeof *ids,   ARENA_DEFAULT_ALIGN, (size_t)n);
    User*  users = arena_alloc_array_zero(arena, sizeof *users, ARENA_DEFAULT_ALIGN, (size_t)n);

    SwissMap* by_id = swiss_create(SwissConfigInt);
    for (int i = 0; i < n; i++) {
        ids[i] = i;
        users[i].score = (double)i / 3.0;
        swiss_set(by_id, &ids[i], sizeof ids[i], &users[i]);  /* ids outlive map */
    }

    if (n > 0) {
        User* u = swiss_get(by_id, &ids[0], sizeof ids[0]);
        if (u && CMP(u->score, 0.0)) puts("match");
    }

    SOLIDC_SECURE_ZERO(users, sizeof *users * (size_t)n);     /* scrub sensitive data */
    swiss_destroy(by_id);
    arena_destroy(arena);                            /* frees ids + users in one shot */
    return 0;
}
```

## Verify

```bash
make                              # configure + build (Release, native)
make test                         # ctest, all suites
make debug                        # Debug build
make check-platforms              # cross aarch64/Windows + native macOS/BSD (skips if absent)
doxygen Doxyfile                  # doc check
git config core.hooksPath .githooks   # clang-format staged files (once)
```

Add a test: create `tests/<module>_test.c` (asserts are live — tests compile
with `-UNDEBUG`) and add `<module>` to `TEST_MODULES` in `tests/CMakeLists.txt`.

## Reference index

Load only what the task needs:

| File | Covers |
|---|---|
| `reference/memory.md` | arena, aligned_alloc, align, defer, dynarray, types, macros, ckdint, secure zero |
| `reference/containers.md` | swiss_map, map, hash, hashset, list, slist, trie, cache |
| `reference/strings.md` | cstr, str, str_slice, unicode, strsim, str_to_num, regex, csvparser, dotenv, flags, prettytable, env |
| `reference/concurrency.md` | thread, threadpool, lock/cond, rwlock, spinlock, channels, chan_patterns, pipeline |
| `reference/io.md` | file, filepath, process, socket, epoll, poller, stdstreams, xtime |
| `reference/math.md` | vec, simd, matrix, linear_alg, cmp, sort |

## Choosing an API quickly

| Need | Use |
|---|---|
| Scratch allocation, short lifetime | `Arena` |
| Growable array of anything | `dynarray_t` |
| Int/pointer map (fast) | `SwissMap` (`swiss_*`) |
| Map needing per-key custom hash | `HashMap` (`map_*`) |
| Copyable fixed-size key set | `hashset_t` |
| Dynamic/UTF-8 string | `cstr`, `utf8_string` |
| Zero-copy parse of a buffer | `StrSlice` / `ss_*` |
| Strict number parsing | `str_to_num.h` |
| Thread pool | `threadpool.h` |
| Go-style queues | `channels.h` / `chan_patterns.h` |
| Parallel scalar work, no frees | `Arena` + `threadpool_submit_batch` |
| Directory walk | `dir_walk` / `dir_walkx` |
| Spawn a process | `process.h` + `pipe_*` |
| TCP server | `socket.h` + `poller.h` |
| Time/benchmarks | `xtime_now`/`xtime_diff_nanos`, `get_time_ns` |
| Sorting numbers | `sol_sort_i32/i64/f64` |
| Float comparison | `CMP()` / `cmp_*` |
| Graphics / linear algebra | `Vec*`, `Mat*`, `FMat` |
