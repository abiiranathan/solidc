#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENGLISH_UNICODE 127

typedef struct TrieNode {
    bool is_end_of_word;
    struct TrieNode* children[MAX_ENGLISH_UNICODE + 1];  // +1 for English characters
} TrieNode;

typedef struct {
    TrieNode* root;
} Trie;

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
    for (int i = 0; i <= MAX_ENGLISH_UNICODE; i++) {  // Initialize all children to NULL
        trie->root->children[i] = NULL;
    }

    return trie;
}

void trie_insert(Trie* trie, const char* word) {
    if (trie == NULL || word == NULL) {
        return;
    }

    TrieNode* curr = trie->root;
    size_t len = strlen(word);
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)word[i];
        if (ch > MAX_ENGLISH_UNICODE) {
            // Ignore non-English characters
            continue;
        }

        if (curr->children[ch] == NULL) {
            curr->children[ch] = (TrieNode*)malloc(sizeof(TrieNode));
            for (int j = 0; j <= MAX_ENGLISH_UNICODE; j++) {
                curr->children[ch]->children[j] = NULL;
            }
            curr->children[ch]->is_end_of_word = false;
        }
        curr = curr->children[ch];
    }
    curr->is_end_of_word = true;
}

bool trie_search(Trie* trie, const char* word) {
    if (trie == NULL || word == NULL) {
        return false;
    }

    TrieNode* curr = trie->root;
    size_t len = strlen(word);
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)word[i];
        if (ch > MAX_ENGLISH_UNICODE) {
            // Ignore non-English characters
            return false;
        }
        if (curr->children[ch] == NULL) {
            return false;
        }
        curr = curr->children[ch];
    }
    return curr->is_end_of_word;
}

void trie_free_node(TrieNode* node) {
    if (node == NULL) {
        return;
    }

    for (int i = 0; i <= MAX_ENGLISH_UNICODE; i++) {
        trie_free_node(node->children[i]);
    }
    free(node);
}

void trie_free(Trie** trie) {
    if (trie == NULL || *trie == NULL) {
        return;
    }

    trie_free_node((*trie)->root);
    free(*trie);
    *trie = NULL;
}

int main() {
    Trie* trie = trie_init();

    // Insert some words and numbers into the TRIE
    trie_insert(trie, "hello");
    trie_insert(trie, "world");
    trie_insert(trie, "trie");
    trie_insert(trie, "implementation");
    trie_insert(trie, "english");
    trie_insert(trie, "characters");
    trie_insert(trie, "123");
    trie_insert(trie, "456");
    trie_insert(trie, "789");
    trie_insert(trie, "word 123");

    // Search for words and numbers in the TRIE
    printf("Search results:\n");
    printf("hello: %s\n", trie_search(trie, "hello") ? "Found" : "Not Found");
    printf("world: %s\n", trie_search(trie, "world") ? "Found" : "Not Found");
    printf("trie: %s\n", trie_search(trie, "trie") ? "Found" : "Not Found");
    printf("english: %s\n", trie_search(trie, "english") ? "Found" : "Not Found");
    printf("nonenglish: %s\n", trie_search(trie, "nonenglish") ? "Found" : "Not Found");
    printf("123: %s\n", trie_search(trie, "123") ? "Found" : "Not Found");
    printf("456: %s\n", trie_search(trie, "456") ? "Found" : "Not Found");
    printf("word 123: %s\n", trie_search(trie, "word 123") ? "Found" : "Not Found");

    // Free the TRIE
    trie_free(&trie);

    return 0;
}
