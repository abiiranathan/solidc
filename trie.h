#ifndef D231241A_A151_496C_BD63_5DB930D6E76C
#define D231241A_A151_496C_BD63_5DB930D6E76C

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALPHABET_SIZE 26

#ifndef MAX_WORD_SIZE
#define MAX_WORD_SIZE 100
#endif

typedef struct TrieNode TrieNode;

// Create a new trie node.
TrieNode* trie_create();

// Inserts a word into the trie.
void insert_word(struct TrieNode* root, const char* word);

// Searches for a word in the trie. Returns 1 if the word is found, 0 otherwise.
int search_word(struct TrieNode* root, const char* word);

// Removes a word from the trie.
void remove_word(struct TrieNode* root, const char* word);

size_t trie_autocomplete(struct TrieNode* root, const char* prefix, char (*results)[MAX_WORD_SIZE],
                         size_t arr_size);

// Frees the memory used by the trie.
void trie_destroy(struct TrieNode* root);

#if defined(__cplusplus)
}
#endif

#ifdef TRIE_IMPL

struct TrieNode {
    struct TrieNode* children[ALPHABET_SIZE];
    int isEndOfWord;
};

static struct TrieNode* create_node() {
    struct TrieNode* node = (struct TrieNode*)malloc(sizeof(struct TrieNode));
    node->isEndOfWord     = 0;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        node->children[i] = NULL;
    }
    return node;
}

// Create a new trie
TrieNode* trie_create() {
    return create_node();
}

void insert_word(struct TrieNode* root, const char* word) {
    struct TrieNode* current = root;
    for (size_t i = 0; i < strlen(word); i++) {
        int index = word[i] - 'a';
        if (!current->children[index]) {
            current->children[index] = create_node();
        }
        current = current->children[index];
    }
    current->isEndOfWord = 1;
}

void trie_destroy(struct TrieNode* root) {
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i]) {
            trie_destroy(root->children[i]);
        }
    }
    free(root);
}

// remove a word from the trie
void remove_word(struct TrieNode* root, const char* word) {
    struct TrieNode* current = root;
    for (size_t i = 0; i < strlen(word); i++) {
        int index = word[i] - 'a';
        if (!current->children[index]) {
            return;
        }
        current = current->children[index];
    }
    current->isEndOfWord = 0;
}

int search_word(struct TrieNode* root, const char* word) {
    struct TrieNode* current = root;
    for (size_t i = 0; i < strlen(word); i++) {
        int index = word[i] - 'a';
        if (!current->children[index]) {
            return 0;
        }
        current = current->children[index];
    }
    return (current != NULL && current->isEndOfWord);
}

void autoCompleteUtil(struct TrieNode* root, char* prefix, size_t* match_count, char* buffer,
                      int depth, char (*results)[MAX_WORD_SIZE], size_t arr_size) {

    // If we have found enough words, return
    if (*match_count >= arr_size) {
        return;
    }

    if (root->isEndOfWord) {
        buffer[depth] = '\0';
        // Copy the word "prefix + buffer" into the results array
        snprintf(results[*match_count], MAX_WORD_SIZE, "%s%s", prefix, buffer);
        (*match_count)++;
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i]) {
            buffer[depth] = 'a' + i;
            autoCompleteUtil(root->children[i], prefix, match_count, buffer, depth + 1, results,
                             arr_size);
        }
    }
}

size_t trie_autocomplete(struct TrieNode* root, const char* prefix, char (*results)[MAX_WORD_SIZE],
                         size_t arr_size) {
    struct TrieNode* current = root;
    for (size_t i = 0; i < strlen(prefix); i++) {
        int index = prefix[i] - 'a';
        if (!current->children[index]) {
            return 0;
        }
        current = current->children[index];
    }

    char buffer[100];
    size_t match_count = 0;
    autoCompleteUtil(current, (char*)prefix, &match_count, buffer, 0, results, arr_size);
    return match_count;
}

#endif /* TRIE_IMPL */

#endif /* D231241A_A151_496C_BD63_5DB930D6E76C */
