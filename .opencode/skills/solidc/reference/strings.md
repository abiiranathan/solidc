# Strings, parsing, and text

Headers: `<solidc/cstr.h>`, `<solidc/str.h>`, `<solidc/str_slice.h>`,
`<solidc/unicode.h>`, `<solidc/strsim.h>`, `<solidc/str_to_num.h>`,
`<solidc/regex.h>`, `<solidc/csvparser.h>`, `<solidc/dotenv.h>`,
`<solidc/flags.h>`, `<solidc/prettytable.h>`, `<solidc/env.h>`.

No module here exposes a custom allocator hook; allocating calls use libc and
are released with the module destructor or `free()`. Do **not** include
`unicode.h` and `regex.h` in the same TU (both declare `regex_match`, the
unicode one has no implementation).

## cstr (`cstr.h`) — dynamic string with SSO

32-byte struct, SSO up to 15 chars; `data` may point inside itself.

```c
typedef struct cstr { char* data; uint32_t length, capacity; char buf[16]; } cstr;
typedef struct { const char* data; uint32_t length; } cstr_view;   /* non-owning */

cstr*  cstr_new(const char* s);          cstr* cstr_new_len(const char* d, size_t n);
cstr*  cstr_new_view(cstr_view v);       cstr* cstr_init(size_t cap);
void   cstr_free(cstr* s);               /* struct + heap buffer; NULL-safe */
void   cstr_drop(cstr* s);               /* buffer only; keeps stack struct */
void   cstr_init_inplace(cstr* s);       /* SSO on a stack/embedded cstr */
void   cstr_wipe(cstr* s);               /* secure zero + release */

bool cstr_reserve(cstr*, size_t);  void cstr_shrink_to_fit(cstr*);  void cstr_clear(cstr*);
bool cstr_append(cstr*, const char*);        bool cstr_append_len(cstr*, const char*, size_t);
bool cstr_append_view(cstr*, cstr_view);     bool cstr_append_cstr(cstr*, const cstr*);
bool cstr_append_char(cstr*, char);          bool cstr_append_fmt(cstr*, const char*, ...);
cstr* cstr_format(const char* fmt, ...);     /* new cstr */
bool cstr_prepend(cstr*, const char*);       bool cstr_insert(cstr*, size_t, const char*);
bool cstr_remove(cstr*, size_t index, size_t count);  size_t cstr_remove_all(cstr*, const char*);

void cstr_lower/upper/trim/ltrim/rtrim(cstr*);
bool cstr_copy(cstr* dst, const cstr* src);  /* use INSTEAD of shallow copy */
int  cstr_cmp(const cstr*, const cstr*);     bool cstr_equals(const cstr*, const cstr*);
bool cstr_starts_with/ends_with/contains(const cstr*, const char*);
int  cstr_find(const cstr*, const char*);    /* CSTR_NPOS (-1) if absent */

cstr* cstr_substr(const cstr*, size_t, size_t);      /* caller frees */
cstr* cstr_replace_all(const cstr*, const char*, const char*);
cstr** cstr_split(const cstr*, const char* delim, size_t* count_out);  /* free each + free(array) */
cstr* cstr_join(const cstr** strings, size_t count, const char* delim);
```

**Never shallow-copy or `realloc` a `cstr` in SSO mode** — its `data`
self-pointer dangles. Use `cstr_copy`/`cstr_new_view`. `cstr_append_fast` /
`cstr_prepend_fast` skip the capacity check (caller guarantees space).
`cstr_split` returns a non-NUL-terminated array; free each element with
`cstr_free` and the array with `free`. `cstr_len(NULL) == 0`,
`cstr_empty(NULL) == true`.

## str (`str.h`) — header-only C-string helpers

Operates on NUL-terminated `char*`. Predicates: `str_is_empty/blank/alpha/
digit/alnum/numeric/float`, `str_equals/iequals/contains/starts_with/ends_with`,
`str_find/rfind/count_substr/word_count`. In-place: `str_lower/upper/
capitalize/camelcase/pascalcase/titlecase/ltrim/rtrim/trim/trim_chars/
truncate/reverse/remove_char/remove_all/remove_substr/wipe`. Allocating (free
with `free()`): `str_dup/ndup/substr/repeat/pad_left/pad_right/center/
to_snakecase/replace/replace_all/split/join/concat`.
`str_split` returns a **NULL-sentinel-terminated** array released with
`str_free_split(parts)`. `str_concat` **requires a trailing `NULL`** sentinel.
Number→string: `str_from_int/long/double(value, buf, buflen)` return `buf` or
`NULL` if it would truncate. `str_lower/upper` delegate to SIMD ASCII
conversion and are UTF-8-safe (bytes ≥ 0x80 untouched).

## StrSlice (`str_slice.h`) — zero-copy parsing (preferred for untrusted input)

```c
typedef struct { const char* data; size_t len; } StrSlice;
typedef enum { SS_OK, SS_NULL, SS_BOUNDS, SS_NOT_FOUND, SS_OVERFLOW, SS_INVALID } StrSliceErr;
#define SS_LIT("literal") ((StrSlice){.data=("literal"), .len=sizeof("literal")-1})

StrSlice ss_from(const char*, size_t);  StrSlice ss_from_cstr(const char*);  StrSlice ss_empty(void);
StrSlice ss_slice(StrSlice, size_t start, size_t len, StrSliceErr* err);
StrSlice ss_skip(StrSlice, size_t n);   StrSlice ss_take(StrSlice, size_t n);  StrSlice ss_trim(StrSlice);
bool ss_equal(StrSlice, StrSlice);      bool ss_equal_nocase(StrSlice, StrSlice);
bool ss_starts_with/ends_with/contains(StrSlice, StrSlice);
size_t ss_find(StrSlice hay, StrSlice needle);                 /* (size_t)-1 if absent */
StrSliceErr ss_split_on(StrSlice, StrSlice sep, StrSlice* head, StrSlice* tail);
bool ss_get(StrSlice, size_t i, char* out);
StrSliceErr ss_to_int(StrSlice, int*);  ss_to_double(StrSlice, double*);  ss_to_bool(StrSlice, bool*);
char* ss_to_owned_cstr(StrSlice);       /* malloc'd */
```

`ss_to_int` stops at the first non-digit and returns `SS_OK` (it does not
require a fully-numeric slice); use `ss_to_bool` for strict booleans. `SS_LIT`
requires a string literal (uses `sizeof`).

## Unicode (`unicode.h`) — single-allocation UTF-8 string

`utf8_string { char* data; size_t length, count, capacity; }` — struct and data
are **one allocation**, so growth functions take `utf8_string**` and you must
re-read `*s` after a successful call.

```c
utf8_string* utf8_new(const char*);     void utf8_free(utf8_string*);
utf8_string* utf8_new_with_capacity(size_t);  utf8_string* utf8_clone(const utf8_string*);
bool utf8_append(utf8_string**, const char*);       /* re-read *s */
bool utf8_insert(utf8_string**, size_t index, const char*);
bool utf8_replace(utf8_string**, const char*, const char*);
size_t utf8_replace_all(utf8_string**, const char*, const char*);
bool utf8_remove(utf8_string*, size_t index, size_t count);   /* count in codepoints */
utf8_string* utf8_reverse/concat(const utf8_string*, ...);    /* utf8_free */
char* utf8_substr(const utf8_string*, size_t, size_t);        /* free() */
utf8_string** utf8_split(const utf8_string*, const char* delim, size_t* n);
void utf8_split_free(utf8_string**, size_t);
uint32_t utf8_to_codepoint(const char*);  void ucp_to_utf8(uint32_t, char[5]);
size_t utf8_char_length(const char*);     bool is_valid_utf8(const char*);
bool is_utf8_alpha/digit/alnum/whitespace/punct(const char*);
void utf8_ltrim/rtrim/trim/trim_char/trim_chars/tolower/toupper(char*);
long utf8_writeto(const utf8_string*, const char* file);
utf8_string* utf8_readfrom(const char* file);
```

`utf8_split` frees with `utf8_split_free`; `utf8_copy`/`utf8_substr` with
`free`. Classification is locale-dependent.

## Number parsing (`str_to_num.h`) — strict, no allocation

```c
typedef enum { STO_SUCCESS, STO_OVERFLOW, STO_UNDERFLOW, STO_INVALID } StoError;
StoError str_to_u8/i8/u16/i16/u32/i32/u64/i64(const char*, ...);
StoError str_to_uint/long/ulong/uintptr/int/double/float(const char*, ...);
StoError str_to_int_base/long_base/ulong_base(const char*, int base, ...);
StoError str_to_bool(const char*, bool*);     /* true/false/yes/no/on/off/1/0 */
const char* sto_error_string(StoError);       /* never NULL */
```

`STO_INVALID` for NULL/empty/trailing garbage/bad base; `STO_OVERFLOW` out of
range; `STO_UNDERFLOW` for negative unsigned or subnormal floats. Output only
written on success.

## String similarity (`strsim.h`)

```c
int strsim_cosine(const char*, const char*, double*);
int strsim_levenshtein(const char*, const char*, size_t*);
int strsim_damerau_levenshtein(const char*, const char*, size_t*);
int strsim_jaro_winkler(const char*, const char*, double*);
int strsim_jaccard_bigram(const char*, const char*, double*);
```

Return `0` success, `-1` NULL, `-2` allocation failure (cosine: `-2` = empty
input). Similarities in `[0,1]`.

## Regex (`regex.h`) — PCRE2 wrapper

```c
regex_status_t regex_compile(const char* pattern, regex_flags_t flags,
                             regex_t** out, char* errbuf, size_t errbuf_len);
regex_t* regex_retain(regex_t*);   void regex_free(regex_t*);   /* ref-counted */
regex_status_t regex_ctx_create(regex_ctx_t**);  void regex_ctx_free(regex_ctx_t*);
regex_status_t regex_exec(const regex_t*, regex_ctx_t*, const char* subj,
                          size_t len, size_t offset, regex_match_t*);
regex_status_t regex_match(regex_t*, regex_ctx_t*, const char* subj, regex_match_t*);
bool regex_is_match(regex_t*, regex_ctx_t*, const char*, size_t);
regex_status_t regex_iter_init/next(regex_t*, regex_ctx_t*, const char*, size_t, regex_iter_t**);
void regex_iter_free(regex_iter_t*);
regex_status_t regex_sub/gsub(...);   /* two-pass sizing via *out_len */
char* regex_group_dup(const char* subj, const regex_match_t*, uint32_t group); /* free() */
```

Flags: `REGEX_FLAG_CASELESS/MULTILINE/DOTALL/EXTENDED/UTF/UCP/UNGREEDY/ANCHORED`.
`REGEX_MAX_GROUPS` is 64. `regex_t` is ref-counted and safe to share; a
`regex_ctx_t` is **per-thread**. Spans are byte offsets (`end` exclusive;
unparticipating group has `start == end == PCRE2_UNSET`). `regex_must_compile` /
`regex_ctx_must_create` call `exit(1)` — do not use them in library code.

## CSV (`csvparser.h`)

`CsvReader* csv_reader_new(const char* file, size_t arena_memory);`
`Row** csv_reader_parse(CsvReader*)`, `void csv_reader_parse_async(CsvReader*,
CsvRowCallback, size_t alloc_max)`, `size_t csv_reader_numrows(const CsvReader*)`,
`void csv_reader_free(CsvReader*)`; `csv_reader_setconfig`/`getconfig`.
Writer: `CsvWriter* csvwriter_new(const char*)`,
`bool csvwriter_write_row(CsvWriter*, const char** fields, size_t n)`,
`csvwriter_free`, and the `CsvWriterConfigure(writer, ...)` macro.
Rows/fields are owned by the reader's arena — free only via
`csv_reader_free(reader)`. `Row { char** fields; size_t count; }`.

## dotenv (`dotenv.h`)

`bool load_dotenv(const char* path);` writes into the process environment via
`env.h` (`SETENV`) and **overwrites** existing vars. Interpolation uses
`${VAR}` (the header comment's `$(name)` is wrong). Returns `true` only if no
line failed.

## Env (`env.h`)

```c
#define GETENV(name)   /* secure_getenv on Linux, getenv elsewhere */
#define SETENV(name, value)
```

## Flags (`flags.h`)

`FlagParser* flag_parser_new(const char* name, const char* desc)`,
`flag_add(parser, type, name, short, desc, value_ptr, required)`, the typed
macros `flag_bool/string/int/...` and `flag_req_*`, validators via
`flag_set_validator`, then `FlagStatus flag_parse(parser, argc, argv)` /
`flag_parse_and_invoke(...)`. Errors via `flag_get_error`, usage via
`flag_print_usage`, `flag_parser_free`. Parser memory is internal arena — never
`free` a `Flag*`. Status enum `FlagStatus` (`FLAG_OK`, `FLAG_ERROR_*`).

## Pretty table (`prettytable.h`)

Fill a `prettytable_config` (call `prettytable_config_init` first — a
zero-initialized config has `output == NULL` and headers disabled), set
`num_rows/num_cols/get_header/get_cell/user_data`, optionally
`get_length`/`style`, then `int prettytable_print(const prettytable_config*)`.
Styles: `PRETTYTABLE_STYLE_BOX/ASCII/MINIMAL/DOUBLE`.
