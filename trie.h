#ifndef __TRIE_H__
#define __TRIE_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TrieNode {
    bool is_end_of_word;
    // Assuming only lowercase English letters
    struct TrieNode* children[26];
} TrieNode;

typedef struct {
    TrieNode* root;
} Trie;

// Allocate a new Trie and initialize it.
Trie* trie_init() {
    Trie* trie = (Trie*)malloc(sizeof(Trie));
    if (trie == NULL) {
        return NULL;
    }

    trie->root = (TrieNode*)malloc(sizeof(TrieNode));
    if (trie->root == NULL) {
        free(trie);
        return NULL;
    }

    trie->root->is_end_of_word = false;
    for (int i = 0; i < 26; i++) {
        trie->root->children[i] = NULL;
    }
    return trie;
}

// Insert a word into the Trie.
void trie_insert(Trie* trie, const char* word) {
    if (trie == NULL || word == NULL) {
        return;
    }

    TrieNode* curr = trie->root;
    while (*word != '\0') {
        int index = *word - 'a';
        if (curr->children[index] == NULL) {
            curr->children[index] = (TrieNode*)malloc(sizeof(TrieNode));
            for (int i = 0; i < 26; i++) {
                curr->children[index]->children[i] = NULL;
            }
            curr->children[index]->is_end_of_word = false;
        }
        curr = curr->children[index];
        word++;
    }
    curr->is_end_of_word = true;
}

// Search for a word in the Trie. Returns true if the word is found, false otherwise.
bool trie_search(Trie* trie, const char* word) {
    if (trie == NULL || word == NULL) {
        return false;
    }

    TrieNode* curr = trie->root;
    while (*word != '\0') {
        int index = *word - 'a';
        if (curr->children[index] == NULL) {
            return false;
        }
        curr = curr->children[index];
        word++;
    }
    return curr->is_end_of_word;
}

// Deallocate the memory occupied by the Trie.
void trie_free(TrieNode** trie) {
    if (trie == NULL || *trie == NULL) {
        return;
    }
    TrieNode* root = *trie;
    if (root != NULL) {
        for (int i = 0; i < 26; i++) {
            trie_free(&root->children[i]);
        }
        free(root);
    }
    *trie = NULL;
}

typedef struct {
    char** words;
    size_t size;
    size_t capacity;
} AutocompleteResult;

// Helper function to add a word to the AutocompleteResult data structure
static void autocomplete_result_add(AutocompleteResult* result, const char* word) {
    if (result->size >= result->capacity) {
        size_t new_capacity = (result->capacity == 0) ? 1 : result->capacity * 2;
        char** new_words = (char**)realloc(result->words, new_capacity * sizeof(char*));
        if (new_words == NULL) {
            return;
        }
        result->words = new_words;
        result->capacity = new_capacity;
    }

    result->words[result->size] = strdup(word);
    result->size++;
}

// Helper function to free the memory occupied by AutocompleteResult data structure
static void autocomplete_result_free(AutocompleteResult* result) {
    for (size_t i = 0; i < result->size; i++) {
        free(result->words[i]);
    }
    free(result->words);
    result->words = NULL;
    result->size = 0;
    result->capacity = 0;
}

// Helper function to perform recursive autocomplete search
static void trie_autocomplete_helper(TrieNode* node, char* prefix, AutocompleteResult* result,
                                     char* current_word, int current_len) {
    if (node == NULL) {
        return;
    }

    if (node->is_end_of_word) {
        current_word[current_len] = '\0';
        char* full_word = (char*)malloc(strlen(prefix) + current_len + 1);
        strcpy(full_word, prefix);
        strcat(full_word, current_word);
        autocomplete_result_add(result, full_word);
        free(full_word);
    }

    for (int i = 0; i < 26; i++) {
        if (node->children[i] != NULL) {
            current_word[current_len] = 'a' + i;
            trie_autocomplete_helper(node->children[i], prefix, result, current_word,
                                     current_len + 1);
            current_word[current_len] = '\0';
        }
    }
}

// Autocomplete search for words in the TRIE with a given prefix
AutocompleteResult trie_autocomplete(Trie* trie, const char* prefix) {
    AutocompleteResult result = {0};

    if (trie == NULL || prefix == NULL) {
        return result;
    }

    int len = strlen(prefix);
    if (len == 0) {
        return result;
    }

    TrieNode* curr = trie->root;
    for (int i = 0; i < len; i++) {
        int index = prefix[i] - 'a';
        if (curr->children[index] == NULL) {
            return result;
        }
        curr = curr->children[index];
    }

    char* current_word = (char*)malloc(len + 1);
    if (current_word == NULL) {
        return result;
    }

    trie_autocomplete_helper(curr, (char*)prefix, &result, current_word, 0);
    free(current_word);
    return result;
}

#endif /* __TRIE_H__ */
