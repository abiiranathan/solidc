#ifndef D231241A_A151_496C_BD63_5DB930D6E76C
#define D231241A_A151_496C_BD63_5DB930D6E76C

#include <stddef.h>

#define ALPHABET_SIZE 26

#ifndef MAX_WORD_SIZE
#define MAX_WORD_SIZE 100
#endif

typedef struct TrieNode TrieNode;

// Create a new trie node.
TrieNode *trie_create();

// Inserts a word into the trie.
void insert_word(struct TrieNode *root, const char *word);

// Searches for a word in the trie. Returns 1 if the word is found, 0 otherwise.
int search_word(struct TrieNode *root, const char *word);

// Removes a word from the trie.
void remove_word(struct TrieNode *root, const char *word);

size_t trie_autocomplete(struct TrieNode *root, const char *prefix,
                         char (*results)[MAX_WORD_SIZE], size_t arr_size);

// Frees the memory used by the trie.
void trie_destroy(struct TrieNode *root);

#endif /* D231241A_A151_496C_BD63_5DB930D6E76C */
