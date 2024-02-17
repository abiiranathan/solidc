#include "trie.h"
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Create a new trie
TEST(TrieTest, Create) {
  TrieNode *root = trie_create();
  EXPECT_TRUE(root != NULL);
  trie_destroy(root);
}

// Insert a word into the trie
TEST(TrieTest, Insert) {
  TrieNode *root = trie_create();
  insert_word(root, "hello");
  EXPECT_TRUE(search_word(root, "hello"));
  trie_destroy(root);
}

// Search for a word in the trie
TEST(TrieTest, Search) {
  TrieNode *root = trie_create();
  insert_word(root, "hello");
  EXPECT_TRUE(search_word(root, "hello"));
  EXPECT_FALSE(search_word(root, "world"));
  trie_destroy(root);
}

// Remove a word from the trie
TEST(TrieTest, Remove) {
  TrieNode *root = trie_create();
  insert_word(root, "hello");
  remove_word(root, "hello");
  EXPECT_FALSE(search_word(root, "hello"));
  trie_destroy(root);
}

// Insert and remove multiple words
TEST(TrieTest, Multiple) {
  TrieNode *root = trie_create();
  insert_word(root, "hello");
  insert_word(root, "world");
  insert_word(root, "foo");
  insert_word(root, "bar");
  remove_word(root, "hello");
  remove_word(root, "world");
  remove_word(root, "foo");
  remove_word(root, "bar");
  EXPECT_FALSE(search_word(root, "hello"));
  EXPECT_FALSE(search_word(root, "world"));
  EXPECT_FALSE(search_word(root, "foo"));
  EXPECT_FALSE(search_word(root, "bar"));
  trie_destroy(root);
}

// Autocomplete
TEST(TrieTest, Autocomplete) {
  TrieNode *root = trie_create();
  insert_word(root, "hello");
  insert_word(root, "world");
  insert_word(root, "foo");
  insert_word(root, "bar");

  char results[10][MAX_WORD_SIZE] = {0};
  size_t arr_size = sizeof(results) / sizeof(results[0]);
  size_t match_count = trie_autocomplete(root, "hel", results, arr_size);

  EXPECT_EQ(match_count, 1);
  EXPECT_STREQ(results[0], "hello");

  match_count = trie_autocomplete(root, "w", results, arr_size);
  EXPECT_EQ(match_count, 1);
  EXPECT_STREQ(results[0], "world");

  match_count = trie_autocomplete(root, "f", results, arr_size);
  EXPECT_EQ(match_count, 1);
  trie_destroy(root);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}