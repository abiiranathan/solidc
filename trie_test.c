#include "trie.h"
#include <stdio.h>
#include <string.h>

// Helper function to calculate the minimum of three integers
static int min(int a, int b, int c) {
    int min_value = a;
    if (b < min_value)
        min_value = b;
    if (c < min_value)
        min_value = c;
    return min_value;
}

// Helper function to perform recursive fuzzy search
static void trie_fuzzy_search_helper(TrieNode* node, const char* word, int len, int max_distance,
                                     char* current_word, int current_len) {
    if (node == NULL) {
        return;
    }

    if (current_len > 0 && current_len <= len &&
        current_word[current_len - 1] == word[current_len - 1]) {
        if (node->is_end_of_word && abs(len - current_len) <= max_distance) {
            current_word[current_len] = '\0';
            printf("Fuzzy Match: %s\n", current_word);
        }
    }

    for (int i = 0; i < 26; i++) {
        if (node->children[i] != NULL) {
            current_word[current_len] = 'a' + i;
            trie_fuzzy_search_helper(node->children[i], word, len, max_distance, current_word,
                                     current_len + 1);
            current_word[current_len] = '\0';

            if (current_len < len && current_len + max_distance >= len) {
                trie_fuzzy_search_helper(node->children[i], word, len, max_distance, current_word,
                                         current_len);
            }
        }
    }
}

// Fuzzy search for words in the TRIE with a given maximum edit distance
void trie_fuzzy_search(Trie* trie, const char* word, int max_distance) {
    if (trie == NULL || word == NULL) {
        return;
    }

    int len = strlen(word);
    if (len == 0 || max_distance < 0) {
        return;
    }

    char* current_word = (char*)malloc(len + 1);
    if (current_word == NULL) {
        return;
    }

    trie_fuzzy_search_helper(trie->root, word, len, max_distance, current_word, 0);
    free(current_word);
}

int main() {
    Trie* trie = trie_init();

    // Insert words into the TRIE
    trie_insert(trie, "apple");
    trie_insert(trie, "app");
    trie_insert(trie, "banana");
    trie_insert(trie, "orange");
    trie_insert(trie, "grape");
    trie_insert(trie, "kiwi");

    // Search for words in the TRIE
    printf("Searching for 'apple': %s\n", trie_search(trie, "apple") ? "Found" : "Not found");
    printf("Searching for 'grape': %s\n", trie_search(trie, "grape") ? "Found" : "Not found");
    printf("Searching for 'watermelon': %s\n",
           trie_search(trie, "watermelon") ? "Found" : "Not found");

    // Perform fuzzy search with max edit distance of 1
    printf("Fuzzy search with max distance 1:\n");
    trie_fuzzy_search(trie, "appel", 1);
    trie_fuzzy_search(trie, "oranje", 1);
    trie_fuzzy_search(trie, "kiwii", 1);

    // Autocomplete search with prefix "ap"
    AutocompleteResult result = trie_autocomplete(trie, "ap");
    for (size_t i = 0; i < result.size; i++) {
        printf("Autocomplete suggestion: %s\n", result.words[i]);
    }

    // Free the TRIE
    trie_free(&(trie->root));
    free(trie);

    // Free the words inside AutocompleteResult
    autocomplete_result_free(&result);

    return 0;
}
