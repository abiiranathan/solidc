# Memory, types, and utilities

Headers: `<solidc/arena.h>`, `<solidc/aligned_alloc.h>`, `<solidc/align.h>`,
`<solidc/defer.h>`, `<solidc/dynarray.h>`, `<solidc/types.h>`,
`<solidc/macros.h>`, `<solidc/ckdint.h>`.

## Arena (`arena.h`) — the primary allocation-free tool

Header-only fast path plus `src/arena.c`. Constants: `ARENA_DEFAULT_ALIGN 16`,
`ARENA_MIN_BLOCK_SIZE 64 KiB`, `ARENA_MAX_BLOCK_SIZE 8 MiB`.

```c
void  arena_init(Arena* a, void* buf, size_t size);      /* caller-owned backing */
Arena* arena_create(size_t reserve_size);                 /* 0 => default */
void  arena_reset(Arena* a);                              /* O(1), keeps pages */
void  arena_destroy(Arena* a);                            /* frees heap struct/blocks */

ArenaCheckpoint arena_save(const Arena* a);
void arena_restore(Arena* a, ArenaCheckpoint cp);         /* O(1) scoped rewind */

void* arena_alloc_align(Arena* a, size_t size, size_t alignment);
void* arena_alloc(Arena* a, size_t size);                 /* ARENA_DEFAULT_ALIGN */
void* arena_alloc_unaligned(Arena* a, size_t size);
void* arena_alloc_zero(Arena* a, size_t size);
void* arena_alloc_array_zero(Arena* a, size_t elem_size, size_t alignment, size_t count);
bool  arena_alloc_batch(Arena* a, const size_t* sizes, size_t count, void** out_ptrs);
char* arena_strdup(Arena* a, const char* str);
char* arena_strdupn(Arena* a, const char* str, size_t length);
void* arena_realloc(Arena* a, void* old_ptr, size_t old_size, size_t new_size, size_t alignment);
size_t arena_committed_size(const Arena* a);
size_t arena_used_size(const Arena* a);
```

Rules:
- All pointers valid until the next `arena_reset`/`arena_destroy`.
- `size == 0` returns `NULL` (unless `ARENA_ABORT_ON_OOM`, built in). Overflow in
  `arena_alloc_array_zero` → `NULL` without invoking the OOM handler.
- `arena_alloc_batch` is all-or-nothing and never aborts; sub-blocks are 16-byte
  aligned (use individual calls if you need stricter alignment).
- `arena_strdup*` return `NULL` on `NULL`/OOM and do **not** invoke the handler.
- Never `free()` an arena pointer. `arena_destroy` is one call for the whole
  arena; prefer it over many small frees.

Idiom — per-request scratch with rollback:

```c
Arena* a = arena_create(64 * 1024);
ArenaCheckpoint cp = arena_save(a);
if (!parse(a, input)) arena_restore(a, cp);   /* discard partial work */
arena_destroy(a);
```

## Aligned allocation (`aligned_alloc.h`)

```c
void* aligned_alloc_xp(size_t alignment, size_t size);   /* alignment: pow2 >= ptr size */
void  aligned_free_xp(void* ptr);
#define ALIGNED_ALLOC(alignment, size) aligned_alloc_xp(alignment, size)
#define ALIGNED_FREE(ptr)              aligned_free_xp(ptr)
```

C11 only. On Windows you **must** free with `ALIGNED_FREE`, not `free`.
Declaration attribute: `ALIGN(x)` from `align.h` (`__declspec(align)` /
`__attribute__((aligned))` / `_Alignas`).

## `defer` (`defer.h`) — cleanup on scope exit

Portable syntax differs by backend, so use only the provided wrappers:

```c
#define defer_free(ptr)      /* free + NULL */
#define defer_fclose(f)      /* fclose + NULL */
#define defer_call(fn)       /* fn() */
#define defer_call1(fn,arg)  /* fn(arg) */
#define defer_restore(var, saved_val)
/* POSIX: defer_close(fd), defer_munmap(ptr,len), defer_mutex_unlock(mu),
          defer_rwlock_unlock(rw), defer_closedir(d)
   Win32: defer_CloseHandle(h), defer_LocalFree(p), defer_VirtualFree(p),
          defer_UnmapViewOfFile(view) */
```

- GCC C uses `defer { ... }` (no trailing `;`); Clang C, MSVC, and C++ require
  `defer { ... };`. **Do not hand-write `defer { ... }` calls if portability
  matters — use the wrapper macros.**
- Never `return`/`break`/`continue`/`goto` out of a deferred block (UB).
- Captured variables must outlive the block.

## Dynamic array (`dynarray.h`)

`dynarray_t { void* data; size_t size, capacity, element_size; }`. Init first;
a zero struct is rejected. Growth is 1.5×, shrink below `capacity/4`.

```c
bool   dynarray_init(dynarray_t* a, size_t elem_size, size_t initial_capacity); /* 0 => 8 */
void   dynarray_free(dynarray_t* a);
bool   dynarray_push(dynarray_t* a, const void* e);
bool   dynarray_push_n(dynarray_t* a, const void* elems, size_t count);
bool   dynarray_pop(dynarray_t* a, void* out);
void*  dynarray_get(const dynarray_t* a, size_t i);          /* bounds-checked, NULL OOB */
bool   dynarray_set(dynarray_t* a, size_t i, const void* e);
bool   dynarray_reserve(dynarray_t* a, size_t cap);
bool   dynarray_shrink_to_fit(dynarray_t* a);
bool   dynarray_insert(dynarray_t* a, size_t i, const void* e);
bool   dynarray_remove(dynarray_t* a, size_t i);             /* order-preserving */
bool   dynarray_swap_remove(dynarray_t* a, size_t i, void* out);
void*  dynarray_detach(dynarray_t* a, size_t* out_size, size_t* out_capacity); /* malloc'd; free() */
#define DYNARRAY_GET_AS(type, arr, index) (((type*)(arr)->data)[index])
```

Not thread-safe. `get`/`get_const` pointers are invalidated by growth/removal.
`DYNARRAY_GET_AS` is unchecked. `detach` returns a `malloc` buffer the caller
owns (`free`), resets the array, `NULL` on empty/failure.

## Fixed-width types (`types.h`)

`i8/i16/i32/i64`, `u8/u16/u32/u64`, `f32` (`float`), `f64` (`double`),
`usize` (`size_t`), `isize` (`ptrdiff_t`), `iptr/uptr`, `byte` (`uint8_t`,
buffers — use `u8` for 8-bit math). Atomics: `atomic_i32/u32/i64/u64`, guarded
by `SOLIDC_HAS_ATOMICS`; absent when `SOLIDC_NO_ATOMICS` or pre-C11 (no
fallback). On MSVC atomics need `/experimental:c11atomics` (CMake adds it).

## Utility macros (`macros.h`)

- Hints/attrs: `LIKELY/UNLIKELY`, `WARN_UNUSED_RESULT`, `PRINTF_FORMAT`,
  `NORETURN`, `UNUSED`, `THREAD_LOCAL`.
- Asserts (no-ops under `NDEBUG`): `ASSERT`, `ASSERT_TRUE`, `ASSERT_NULL`,
  `ASSERT_NOT_NULL`, `ASSERT_STR_EQ`, `ASSERT_FLOAT_EQ`, `ASSERT_EQ/NE/RANGE`.
  Under MSVC `ASSERT_EQ/NE/RANGE` cast through `intptr_t` — use
  `ASSERT_FLOAT_EQ` for floats.
- Memory/array: `ARRAY_SIZE`, `ZERO_MEMORY`, `ZERO_STRUCT`, `ZERO_ARRAY`.
- Math/bits/align: `MIN`, `MAX`, `ABS`, `CLAMP`, `SET_BIT`, `CLEAR_BIT`,
  `TOGGLE_BIT`, `CHECK_BIT`, `ALIGN_UP`, `ALIGN_DOWN`, `IS_ALIGNED`,
  `round_up_pow2_u32/u64` (returns 1 for 0).
- Strings: `STREQ`, `STRNEQ`, `STR_EMPTY`, `STR_NOT_EMPTY`.
- Logging: `LOG_ERROR/WARN/INFO/FATAL` (always active; `FATAL` exits),
  `DEBUG_PRINT/DEBUG_VAR/DEBUG_STR/LOG_DEBUG` (compiled out under `NDEBUG`).
- Iteration: `FOR_EACH_ARRAY(item, array)` (item is a pointer),
  `FOR_EACH_RANGE(var, start, end)`.
- Timing: prefer `get_time_ns()` / `get_time_ms()` (uniform signature).
  `TIME_DIFF`/`TIME_DIFF_MS` have **different arities on Windows vs POSIX**.
- Endianness: `bswap16/32/64`, `TO_LE*/TO_BE*/FROM_LE*/FROM_BE*`,
  `IS_BIG_ENDIAN`/`IS_LITTLE_ENDIAN`.
- CPU: compile-time `HAS_SSE2/AVX/AVX2/AVX512F/NEON`; runtime
  `cpu_has_sse2/avx/avx2/avx512f/neon`. **Always gate on runtime checks before
  using compile-time-detected instructions, or you get SIGILL.**
- Secure: `solidc_secure_zero(void*, size_t)` / `SOLIDC_SECURE_ZERO(p, n)`.
- Safe strings: `SOLIDC_SNPRINTF`, `SOLIDC_STRCPY_S`, `SOLIDC_STRCAT_S`
  (fallback returns `-1` and empties dest on overflow).
- Misc: `STATIC_ASSERT`, `SOLIDC_CTZ/CLZ`, `SOLIDC_PREFETCH`, `SOLIDC_PAUSE`.

## Checked integer arithmetic (`ckdint.h`)

```c
bool ckd_add(r, a, b);   /* *r = a+b; true on overflow */
bool ckd_sub(r, a, b);
bool ckd_mul(r, a, b);
```

Generic over integer types on GCC/Clang (`__builtin_*_overflow`). MSVC
fallback supports **`size_t` only**. Use before any size computation that feeds
an allocation or index. Return convention: `true == overflow`.
