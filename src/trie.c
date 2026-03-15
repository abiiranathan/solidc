#include "../include/trie.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Internal node definition
 * ========================================================================= */

typedef struct trie_node {
    uint8_t* chars;              /* sorted array of child byte values      */
    struct trie_node** children; /* parallel array of child node pointers  */
    uint8_t nchildren;           /* number of live children                */
    uint8_t nalloc;              /* allocated capacity of chars/children   */
    bool is_end_of_word;
    uint8_t _pad;
    uint32_t frequency;
} trie_node;

typedef struct _trie {
    trie_node* root;
    size_t word_count;
} trie_t;

/* =========================================================================
 * Node lifecycle
 * ========================================================================= */

static trie_node* node_create(void) {
    trie_node* n = (trie_node*)calloc(1, sizeof(trie_node));
    return n;
}

/* Recursively free a node subtree. */
static void node_destroy(trie_node* n) {
    if (!n) return;
    for (uint8_t i = 0; i < n->nchildren; i++) node_destroy(n->children[i]);
    free(n->chars);
    free(n->children);
    free(n);
}

/* =========================================================================
 * Child lookup (binary search on sorted chars[])
 * ========================================================================= */

/*
 * Returns the index of `c` in n->chars[], or -1 if not present.
 * Binary search over nchildren (usually ≤ 26 for A-Z text).
 */
static inline int child_index(const trie_node* n, uint8_t c) {
    int lo = 0, hi = (int)n->nchildren - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (n->chars[mid] == c)
            return mid;
        else if (n->chars[mid] < c)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

/*
 * Find or create a child for byte `c` under parent `n`.
 * On insert the arrays grow by doubling, using insertion sort to keep
 * them sorted (cheap because nchildren is tiny in practice).
 */
static trie_node* child_find_or_create(trie_node* n, uint8_t c) {
    int idx = child_index(n, c);
    if (idx >= 0) return n->children[idx];

    /* Need to insert a new child.  Grow arrays if necessary. */
    if (n->nchildren == n->nalloc) {
        uint8_t new_alloc = n->nalloc == 0 ? 2 : (uint8_t)(n->nalloc * 2);
        if (new_alloc < n->nalloc) new_alloc = 255; /* overflow guard */

        uint8_t* nc = (uint8_t*)realloc(n->chars, new_alloc * sizeof(uint8_t));
        trie_node** np = (trie_node**)realloc(n->children, new_alloc * sizeof(trie_node*));

        if (!nc || !np) {
            free(nc);
            free(np);
            return NULL;
        }

        n->chars = nc;
        n->children = np;
        n->nalloc = new_alloc;
    }

    /* Insertion sort: find where `c` belongs, shift right. */
    int8_t pos = (int8_t)n->nchildren;
    while (pos > 0 && n->chars[pos - 1] > c) {
        n->chars[pos] = n->chars[pos - 1];
        n->children[pos] = n->children[pos - 1];
        pos--;
    }

    trie_node* child = node_create();
    if (!child) return NULL;

    n->chars[pos] = c;
    n->children[pos] = child;
    n->nchildren++;
    return child;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

trie_t* trie_create(void) {
    trie_t* t = (trie_t*)malloc(sizeof(trie_t));
    if (!t) return NULL;
    t->root = node_create();
    if (!t->root) {
        free(t);
        return NULL;
    }
    t->word_count = 0;
    return t;
}

void trie_destroy(trie_t* t) {
    if (!t) return;
    node_destroy(t->root);
    free(t);
}

bool trie_insert(trie_t* t, const char* word) {
    if (!t || !word || !*word) return false;

    trie_node* cur = t->root;
    for (const uint8_t* p = (const uint8_t*)word; *p; p++) {
        cur = child_find_or_create(cur, *p);
        if (!cur) return false;
    }

    if (!cur->is_end_of_word) {
        cur->is_end_of_word = true;
        t->word_count++;
    }
    cur->frequency++;
    return true;
}

bool trie_search(const trie_t* t, const char* word) {
    if (!t || !word || !*word) return false;

    const trie_node* cur = t->root;
    for (const uint8_t* p = (const uint8_t*)word; *p; p++) {
        int idx = child_index(cur, *p);
        if (idx < 0) return false;
        cur = cur->children[idx];
    }
    return cur->is_end_of_word;
}

bool trie_starts_with(const trie_t* t, const char* prefix) {
    if (!t || !prefix || !*prefix) return false;

    const trie_node* cur = t->root;
    for (const uint8_t* p = (const uint8_t*)prefix; *p; p++) {
        int idx = child_index(cur, *p);
        if (idx < 0) return false;
        cur = cur->children[idx];
    }
    return true;
}

bool trie_delete(trie_t* t, const char* word) {
    if (!t || !word || !*word) return false;

    const trie_node* cur = t->root;
    for (const uint8_t* p = (const uint8_t*)word; *p; p++) {
        int idx = child_index(cur, *p);
        if (idx < 0) return false;
        cur = cur->children[idx];
    }

    if (!cur->is_end_of_word) return false;
    ((trie_node*)cur)->is_end_of_word = false;
    ((trie_node*)cur)->frequency = 0;
    t->word_count--;
    return true;
}

uint32_t trie_get_frequency(const trie_t* t, const char* word) {
    if (!t || !word || !*word) return 0;

    const trie_node* cur = t->root;
    for (const uint8_t* p = (const uint8_t*)word; *p; p++) {
        int idx = child_index(cur, *p);
        if (idx < 0) return 0;
        cur = cur->children[idx];
    }
    return cur->is_end_of_word ? cur->frequency : 0;
}

size_t trie_get_word_count(const trie_t* t) { return t ? t->word_count : 0; }
bool trie_is_empty(const trie_t* t) { return !t || t->word_count == 0; }

/* =========================================================================
 * Autocomplete (DFS with arena allocation — same API as original)
 * ========================================================================= */

typedef struct {
    char** suggestions;
    size_t count;
    size_t capacity;
    size_t limit;
} _collector;

static void _collect(const trie_node* node, char* buf, size_t depth, size_t buf_max, _collector* c, Arena* arena) {
    if (!node || c->count >= c->limit) return;

    if (node->is_end_of_word) {
        buf[depth] = '\0';
        char* dup = arena_strdup(arena, buf);
        if (dup) c->suggestions[c->count++] = dup;
    }

    for (uint8_t i = 0; i < node->nchildren && c->count < c->limit; i++) {
        if (depth + 1 >= buf_max) return;
        buf[depth] = (char)node->chars[i];
        _collect(node->children[i], buf, depth + 1, buf_max, c, arena);
    }
}

const char** trie_autocomplete(const trie_t* t, const char* prefix, size_t max_suggestions, size_t* out_count,
                               Arena* arena) {
    if (out_count) *out_count = 0;
    if (!t || !prefix || !out_count || max_suggestions == 0 || !arena) return NULL;

    /* Navigate to the prefix node. */
    const trie_node* cur = t->root;
    for (const uint8_t* p = (const uint8_t*)prefix; *p; p++) {
        int idx = child_index(cur, *p);
        if (idx < 0) return NULL;
        cur = cur->children[idx];
    }

    /* Allocate suggestion array and word buffer on the arena. */
    char** suggestions = ARENA_ALLOC_ARRAY(arena, char*, max_suggestions);
    if (!suggestions) return NULL;

    const size_t buf_max = 1024;
    char* buf = (char*)arena_alloc(arena, buf_max);
    if (!buf) return NULL;

    size_t prefix_len = strlen(prefix);
    if (prefix_len >= buf_max) return NULL;
    memcpy(buf, prefix, prefix_len);

    _collector c = {.suggestions = suggestions, .count = 0, .capacity = max_suggestions, .limit = max_suggestions};

    _collect(cur, buf, prefix_len, buf_max, &c, arena);

    if (c.count == 0) return NULL;

    *out_count = c.count;
    return (const char**)suggestions;
}
