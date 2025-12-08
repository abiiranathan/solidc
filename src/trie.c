#include "../include/trie.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Maximum number of children per node (ASCII printable + extended). */
#define TRIE_CHARSET_SIZE 256

/** Represents a single node in the Trie. */
typedef struct trie_node {
    /** Array of child nodes indexed by character value. */
    struct trie_node* children[TRIE_CHARSET_SIZE];
    /** True if this node represents the end of a valid word. */
    bool is_end_of_word;
    /** Frequency count for this word (useful for autocomplete ranking). */
    uint32_t frequency;
} trie_node_t;

/** Root structure for the Trie data structure. */
typedef struct _trie {
    /** Root node of the Trie. */
    trie_node_t* root;
    /** Total number of unique words stored. */
    size_t word_count;
} trie_t;

/**
 * Creates a new Trie node with all fields initialized to zero.
 * @return Pointer to new node on success, NULL on allocation failure.
 */
static trie_node_t* trie_node_create(void) {
    trie_node_t* node = calloc(1, sizeof(*node));
    if (node == NULL) {
        fprintf(stderr, "Error: Failed to allocate Trie node\n");
        return NULL;
    }
    node->is_end_of_word = false;
    node->frequency      = 0;
    return node;
}

/**
 * Creates a new Trie data structure.
 * @return Pointer to new Trie on success, NULL on allocation failure.
 */
trie_t* trie_create(void) {
    trie_t* trie = malloc(sizeof(*trie));
    if (trie == NULL) {
        fprintf(stderr, "Error: Failed to allocate Trie structure\n");
        return NULL;
    }

    trie->root = trie_node_create();
    if (trie->root == NULL) {
        free(trie);
        return NULL;
    }

    trie->word_count = 0;
    return trie;
}

/**
 * Recursively destroys a Trie node and all its children.
 * @param node Node to destroy (can be NULL).
 */
static void trie_node_destroy(trie_node_t* node) {
    if (node == NULL) {
        return;
    }

    // Recursively free all children
    for (size_t i = 0; i < TRIE_CHARSET_SIZE; i++) {
        if (node->children[i] != NULL) {
            trie_node_destroy(node->children[i]);
        }
    }

    free(node);
}

/**
 * Destroys a Trie and frees all associated memory.
 * @param trie Trie to destroy (can be NULL).
 */
void trie_destroy(trie_t* trie) {
    if (trie == NULL) {
        return;
    }

    trie_node_destroy(trie->root);
    free(trie);
}

/**
 * Inserts a word into the Trie.
 * @param trie The Trie structure.
 * @param word The word to insert (null-terminated).
 * @return true on success, false on failure (invalid input or allocation error).
 */
bool trie_insert(trie_t* trie, const char* word) {
    if (trie == NULL || word == NULL || *word == '\0') {
        return false;
    }

    trie_node_t* current       = trie->root;
    const unsigned char* uword = (const unsigned char*)word;

    // Traverse/create path for each character
    while (*uword != '\0') {
        unsigned char index = *uword;

        if (current->children[index] == NULL) {
            current->children[index] = trie_node_create();
            if (current->children[index] == NULL) {
                return false;  // Allocation failure
            }
        }

        current = current->children[index];
        uword++;
    }

    // Mark end of word and increment frequency
    if (!current->is_end_of_word) {
        current->is_end_of_word = true;
        trie->word_count++;
    }
    current->frequency++;

    return true;
}

/**
 * Searches for an exact word in the Trie.
 * @param trie The Trie structure.
 * @param word The word to search for (null-terminated).
 * @return true if word exists in Trie, false otherwise.
 */
bool trie_search(const trie_t* trie, const char* word) {
    if (trie == NULL || word == NULL || *word == '\0') {
        return false;
    }

    const trie_node_t* current = trie->root;
    const unsigned char* uword = (const unsigned char*)word;

    while (*uword != '\0') {
        unsigned char index = *uword;

        if (current->children[index] == NULL) {
            return false;  // Path doesn't exist
        }

        current = current->children[index];
        uword++;
    }

    return current->is_end_of_word;
}

/**
 * Checks if any word in the Trie starts with the given prefix.
 * @param trie The Trie structure.
 * @param prefix The prefix to search for (null-terminated).
 * @return true if prefix exists, false otherwise.
 */
bool trie_starts_with(const trie_t* trie, const char* prefix) {
    if (trie == NULL || prefix == NULL || *prefix == '\0') {
        return false;
    }

    const trie_node_t* current   = trie->root;
    const unsigned char* uprefix = (const unsigned char*)prefix;

    while (*uprefix != '\0') {
        unsigned char index = *uprefix;

        if (current->children[index] == NULL) {
            return false;
        }

        current = current->children[index];
        uprefix++;
    }

    return true;  // Prefix path exists
}

/**
 * Deletes a word from the Trie.
 * @param trie The Trie structure.
 * @param word The word to delete (null-terminated).
 * @return true if word was deleted, false if word doesn't exist.
 */
bool trie_delete(trie_t* trie, const char* word) {
    if (trie == NULL || word == NULL || *word == '\0') {
        return false;
    }

    trie_node_t* current       = trie->root;
    const unsigned char* uword = (const unsigned char*)word;

    while (*uword != '\0') {
        unsigned char index = *uword;
        if (current->children[index] == NULL) {
            return false;
        }
        current = current->children[index];
        uword++;
    }

    if (!current->is_end_of_word) {
        return false;
    }

    current->is_end_of_word = false;
    current->frequency      = 0;
    trie->word_count--;

    return true;
}

/**
 * Gets the frequency count for a word.
 * @param trie The Trie structure.
 * @param word The word to query (null-terminated).
 * @return Frequency count, or 0 if word doesn't exist.
 */
uint32_t trie_get_frequency(const trie_t* trie, const char* word) {
    if (trie == NULL || word == NULL || *word == '\0') {
        return 0;
    }

    const trie_node_t* current = trie->root;
    const unsigned char* uword = (const unsigned char*)word;

    while (*uword != '\0') {
        unsigned char index = *uword;
        if (current->children[index] == NULL) {
            return 0;
        }
        current = current->children[index];
        uword++;
    }

    return current->is_end_of_word ? current->frequency : 0;
}

/**
 * Gets the total number of unique words in the Trie.
 * @param trie The Trie structure.
 * @return Number of unique words, or 0 if trie is NULL.
 */
size_t trie_get_word_count(const trie_t* trie) {
    return trie != NULL ? trie->word_count : 0;
}

/**
 * Checks if the Trie is empty.
 * @param trie The Trie structure.
 * @return true if empty or NULL, false otherwise.
 */
bool trie_is_empty(const trie_t* trie) {
    return trie == NULL || trie->word_count == 0;
}

/**
 * Recursively collects all words from a node with a given prefix using Arena allocation.
 * @param node Current node being traversed.
 * @param prefix Current prefix string buffer (managed by Arena).
 * @param max_len Size of the prefix buffer.
 * @param depth Current depth in recursion (index into prefix).
 * @param collector Collector for storing results.
 * @param arena The Arena for allocating suggestion strings.
 */
static void collect_words_recursive(const trie_node_t* node, char* prefix, size_t max_len, size_t depth,
                                    suggestions_collector_t* collector, Arena* arena) {
    if (node == NULL || collector->count >= collector->limit) {
        return;
    }

    // If this node marks end of word, add it to suggestions
    if (node->is_end_of_word) {
        prefix[depth] = '\0';

        // Duplicate string into the Arena
        char* dup = arena_strdup(arena, prefix);
        if (dup != NULL) {
            collector->suggestions[collector->count++] = dup;
        }
    }

    // Recursively traverse all children
    for (size_t i = 0; i < TRIE_CHARSET_SIZE; i++) {
        if (node->children[i] != NULL) {
            // Check buffer bounds
            if (depth + 1 >= max_len) {
                return;  // Prevent buffer overflow
            }

            prefix[depth] = (char)i;
            collect_words_recursive(node->children[i], prefix, max_len, depth + 1, collector, arena);

            if (collector->count >= collector->limit) {
                return;  // Reached limit, stop collecting
            }
        }
    }
}

/**
 * Autocomplete function that returns all words with a given prefix.
 * Stores the result array and suggestion strings in the provided Arena.
 *
 * @param trie The Trie structure.
 * @param prefix The prefix to search for (null-terminated).
 * @param max_suggestions Maximum number of suggestions to return.
 * @param out_count Output parameter for number of suggestions found.
 * @param arena The Arena allocator to use for the results.
 * @return Array of suggestion strings (allocated in arena), or NULL on error/not found.
 * @note The returned array and strings are owned by the Arena and freed when arena_destroy is called.
 */
const char** trie_autocomplete(const trie_t* trie, const char* prefix, size_t max_suggestions, size_t* out_count,
                               Arena* arena) {
    if (out_count != NULL) {
        *out_count = 0;
    }

    if (trie == NULL || prefix == NULL || out_count == NULL || max_suggestions == 0 || arena == NULL) {
        return NULL;
    }

    // Navigate to the prefix node
    const trie_node_t* current   = trie->root;
    const unsigned char* uprefix = (const unsigned char*)prefix;
    size_t prefix_len            = strlen(prefix);

    while (*uprefix != '\0') {
        unsigned char index = *uprefix;
        if (current->children[index] == NULL) {
            return NULL;  // Prefix doesn't exist
        }
        current = current->children[index];
        uprefix++;
    }

    // Initialize collector
    // We allocate the full limit upfront in the Arena to avoid resizing logic
    suggestions_collector_t collector;
    collector.limit       = max_suggestions;
    collector.count       = 0;
    collector.suggestions = (char**)arena_alloc_array(arena, sizeof(char*), max_suggestions);

    if (collector.suggestions == NULL) {
        return NULL;
    }

    // Allocate buffer for building words in the Arena
    const size_t max_word_len = 1024;
    char* word_buffer         = (char*)arena_alloc(arena, max_word_len);

    if (word_buffer == NULL) {
        return NULL;
    }

    // Copy prefix into buffer
    if (prefix_len >= max_word_len) {
        return NULL;
    }
    strncpy(word_buffer, prefix,
            prefix_len + 1);  // +1 to copy null terminator if space allows, mostly to silence warnings

    // Collect all words with this prefix
    collect_words_recursive(current, word_buffer, max_word_len, prefix_len, &collector, arena);

    // Return results
    if (collector.count == 0) {
        return NULL;
    }

    *out_count = collector.count;
    return (const char**)collector.suggestions;
}
