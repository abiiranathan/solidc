# Containers, hashing, and caches

Headers: `<solidc/swiss_map.h>`, `<solidc/map.h>`, `<solidc/hash.h>`,
`<solidc/hashset.h>`, `<solidc/list.h>`, `<solidc/slist.h>`,
`<solidc/trie.h>`, `<solidc/cache.h>`.

## Pick one

| Need | Use | Alloc per element |
|---|---|---|
| General map, fast | `SwissMap` (swiss-table, SIMD probe) | key/value stored by pointer |
| Map with custom hash | `HashMap` (Robin Hood) | key/value stored by pointer |
| Fixed-size key set | `hashset_t` | **none** (keys copied inline) |
| UTF-8 word index | `trie_t` (arena-backed) | arena nodes |
| Expiring key→value | `cache_t` (sharded seqlock) | slab for large values |
| Ordered, cheap insert | `list_t` / `slist` | one malloc per node |

## The ownership rule (both maps)

`map_set`/`swiss_set` store **the key pointer and value pointer**, not copies.
The key must stay alive and **immutable** for the entry's lifetime. If
`key_free`/`value_free` are set in the config, the map owns them; otherwise the
**caller owns everything** (default is `NULL`). On update of an existing key,
the old **value** is freed (if `value_free` set) but the old key is kept.

`key_compare` is **required** — `create` returns `NULL` without it. `key_len`
must be consistent for logically equal keys.

## SwissMap (`swiss_map.h`) — preferred

```c
typedef struct { size_t initial_capacity;
                 KeyCmpFunction key_compare;         /* required */
                 KeyFreeFunction key_free;           /* optional */
                 ValueFreeFunction value_free;       /* optional */
                 float max_load_factor;              /* (0.1, 0.875], default .875 */
                 HashFunction hash_func;             /* optional */
               } SwissConfig;
#define SwissConfigInt (&(SwissConfig){.key_compare = key_compare_int})
#define SwissConfigStr (&(SwissConfig){.key_compare = key_compare_char_ptr})
/* also Float, Double */

SwissMap* swiss_create(const SwissConfig* cfg);
void      swiss_destroy(SwissMap* m);
bool      swiss_set(SwissMap* m, void* key, size_t key_len, void* value);
void*     swiss_get(SwissMap* m, void* key, size_t key_len);   /* NULL on miss */
bool      swiss_remove(SwissMap* m, void* key, size_t key_len);

swiss_iterator swiss_iter(SwissMap* m);
bool swiss_next(swiss_iterator* it, void** key, void** value);
size_t swiss_length(SwissMap* m);
size_t swiss_capacity(SwissMap* m);
bool   swiss_set_safe(...);  void* swiss_get_safe(...);  bool swiss_remove_safe(...);
```

Keys stored by pointer. `swiss_length`/`swiss_capacity` are `NULL`-safe. The
`_safe` variants lock the whole operation (use when sharing across threads);
the plain ones are not thread-safe.

## HashMap (`map.h`) — Robin Hood

Same shape: `map_create`/`map_destroy`/`map_set`/`map_get`/`map_remove`/
`map_iter`/`map_next`/`map_length`/`map_capacity` and `*_safe` variants, plus
`key_compare_int/char_ptr/float/double`. Use when you need a custom
`HashFunction` or a non-default load factor (`(0.1, 0.95]`, default 0.75).

```c
MapConfig cfg = { .key_compare = key_compare_int };
HashMap* m = map_create(&cfg);
int k = 42, v = 7;
map_set(m, &k, sizeof k, &v);
int* got = map_get(m, &k, sizeof k);
map_destroy(m);
```

## Hashing (`hash.h`)

Non-cryptographic, `NULL`-string hashers return 0:
`solidc_djb2_hash`, `solidc_djb2a_hash`, `solidc_sdbm_hash`,
`solidc_fnv1a_hash` (u32), `solidc_fnv1a_hash64` (u64), `solidc_elf_hash` (all
NUL-terminated string input); `solidc_crc32_hash` / `solidc_crc32c_hash` (blob
+ len; **different polynomials**, not interchangeable; CRC-32C uses SSE4.2 when
present); `solidc_murmur_hash(const char*, uint32_t len, uint32_t seed)`;
`solidc_XXH32`. Blob hashers accept `NULL` only when `len == 0`.

## Hashset (`hashset.h`) — header-only, zero per-element allocation

```c
hashset_t* hashset_create(size_t key_size, size_t initial_capacity,
                          uint64_t (*hash_fn)(const void*, size_t),   /* NULL => FNV-1a 64 */
                          bool (*equals_fn)(const void*, const void*, size_t)); /* NULL => memcmp */
void  hashset_destroy(hashset_t* s);
bool  hashset_add(hashset_t* s, const void* key);      /* true even if already present */
bool  hashset_contains(const hashset_t* s, const void* key);
bool  hashset_remove(hashset_t* s, const void* key);
size_t hashset_size(const hashset_t* s);
size_t hashset_capacity(const hashset_t* s);
bool   hashset_isempty(const hashset_t* s);
void   hashset_clear(hashset_t* s);
hashset_t* hashset_union/intersection/difference/symmetric_difference(const hashset_t* A, const hashset_t* B);
bool       hashset_is_subset/is_proper_subset(const hashset_t* A, const hashset_t* B);
```

Keys copied inline; `key_size` required. Set-operation results are newly
allocated and **caller-owned**. Not thread-safe.

## Linked lists

`list.h` (doubly) — nodes store a copy of the element inline (one malloc/node):

```c
list_t* list_new(size_t elem_size);
void list_free(list_t* l); void list_clear(list_t* l); size_t list_size(const list_t* l);
void list_push_back/…_front(list_t* l, void* elem);
void list_pop_back/…_front(list_t* l);
void* list_get(const list_t* l, size_t i);
void list_insert(list_t* l, size_t i, void* elem);
void list_remove(list_t* l, void* elem);          /* first content match (memcmp) */
void list_insert_after/…_before(list_t* l, void* elem, void* ref);
#define LIST_FOR_EACH(list, node)         /* node->data is element */
#define LIST_FOR_EACH_REVERSE(list, node)
```

`list_index_of` compares **content** (`memcmp`), not pointer identity.
Push/insert are `void` and silently skip on malloc failure.

`slist.h` (singly) adds `slist_node_new/free`, `slist_insert/remove(index)`,
`slist_index_of`, `slist_print_asint/aschar`, `SLIST_FOR_EACH`. No back-pop.
Both are not thread-safe.

## Trie (`trie.h`) — arena-backed, **no include guard**

```c
trie_t* trie_create(void);
void    trie_destroy(trie_t* t);
bool    trie_insert(trie_t* t, const char* word);   /* re-insert bumps frequency */
bool    trie_search(const trie_t* t, const char* word);
bool    trie_starts_with(const trie_t* t, const char* prefix);
bool    trie_delete(trie_t* t, const char* word);   /* clears flag only, O(1) */
uint32_t trie_get_frequency(const trie_t* t, const char* word);
size_t  trie_get_word_count(const trie_t* t);
bool    trie_is_empty(const trie_t* t);
const char** trie_autocomplete(const trie_t* t, const char* prefix,
                               size_t max, size_t* out_count, Arena* arena);
```

All nodes live in one internal arena; `trie_destroy` is a single
`arena_destroy`. `trie_autocomplete` results live in the **caller's arena**
until `arena_destroy`. `NULL` and 0 results on bad args/not-found. Not
thread-safe. Because there is no include guard, include it once per TU.

## Cache (`cache.h`) — sharded, seqlock, TTL seconds

Constants: `CACHE_SHARD_COUNT 256`, `CACHE_DEFAULT_TTL 300`,
`CACHE_MAX_KEY_LEN 64`, `CACHE_INLINE_VAL_LEN 160`, `CACHE_MAX_VALUE_LEN 2048`.

```c
typedef struct cache_s cache_t;
typedef struct { size_t capacity_per_shard; size_t slab_blocks_per_shard;
                 uint32_t default_ttl_sec; } cache_config_t;
cache_t* cache_create(const cache_config_t* cfg);
void     cache_destroy(cache_t* c);
bool cache_put(cache_t* c, const void* key, size_t key_len,
               const void* value, size_t val_len, uint32_t ttl_sec); /* 0 => default */
bool cache_get(cache_t* c, const void* key, size_t key_len,
               void* val_out, size_t val_cap, size_t* val_len);
bool cache_delete(cache_t* c, const void* key, size_t key_len);
```

Keys/values are copied; caller buffers need not outlive the call. Values ≤ 160
bytes inline; larger consume a slab block (put fails when exhausted). Lock-free
for concurrent put/get/delete; `cache_destroy` must not race with operations.
TTL granularity is whole seconds; expired entries are misses.
