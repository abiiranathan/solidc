// Define this before including str.h to implement the functions.
// since str.h is a header only library.
#define STR_IMPLEMENTATION

// Adds support for advanced regex features using PCRE.
// Don't forget to link with -lpcre2-8
// #define USE_PCRE_REGEX
// Adds 2 functions:
// - regex_sub_match_pcre (for capturing a single group from a string using
// PCRE)
// - regex_capture (for capturing multiple groups from a string using PCRE)

#include "str.h"
#include <gtest/gtest.h>

TEST(StringTest, TestStringCopy) {
  char *s = string_copy("Hello, world!");
  ASSERT_STREQ(s, "Hello, world!");
  free(s);

  // test with null
  s = string_copy(NULL);
  ASSERT_STREQ(s, NULL);
}

TEST(StringTest, TestStringAppend) {
  char *s = string_append("Hello, ", "world!");
  ASSERT_STREQ(s, "Hello, world!");
  free(s);
}

TEST(StringTest, TestStringAppendChar) {
  char *s = string_append_char("Hello", '!');
  ASSERT_STREQ(s, "Hello!");
  free(s);
}

TEST(StringTest, TestStringInsert) {
  char *s = NULL;
  s = string_insert("Hello,", 6, " World!");
  ASSERT_STREQ(s, "Hello, World!");
  free(s);
}

TEST(StringTest, TestStringSplit) {
  size_t n;
  char **tokens = string_split("Hello, world!", " ", &n);
  ASSERT_EQ(n, 2);
  ASSERT_STREQ(tokens[0], "Hello,");
  ASSERT_STREQ(tokens[1], "world!");
  free(tokens[0]);
  free(tokens[1]);
  free(tokens);
}

TEST(StringTest, TestStringSubstr) {
  char *s = string_copy("Hello, world!");
  char *substr = string_substr(s, 7, 12);
  ASSERT_STREQ(substr, "world");
  free(substr);
  free(s);
}

TEST(StringTest, TestStringJoin) {
  const char *strings[] = {"Hello", "world", "!"};
  char *joined = string_join(strings, 3, " ");
  ASSERT_STREQ(joined, "Hello world !");
  free(joined);
}

// snakecase
TEST(StringTest, TestStringSnakecase) {
  char *s = string_snakecase("HelloWorld123My123NameIsJohn Doe.");
  std::cout << s << std::endl;
  ASSERT_STREQ(s, "hello_world_123_my_123_name_is_john_doe.");
  free(s);
}

// camelcase
TEST(StringTest, TestStringCamelcase) {
  char *s = string_copy("Hello World 123! My Name Is John Doe.");
  string_camelcase(s);
  ASSERT_STREQ(s, "helloWorld123!MyNameIsJohnDoe.");
  free(s);
}

// titlecase
TEST(StringTest, TestStringTitlecase) {
  char *s = string_copy("hello world 123! my name is john doe.");
  string_titlecase(s);
  ASSERT_STREQ(s, "Hello World 123! My Name Is John Doe.");
  free(s);
}

// pascalcase
TEST(StringTest, TestStringPascalcase) {
  char *s = string_copy("hello world 123! my name is john doe.");
  string_pascalcase(s);
  ASSERT_STREQ(s, "HelloWorld123!MyNameIsJohnDoe.");
  free(s);
}

// string_replace
TEST(StringTest, TestStringReplace) {
  char *s = string_replace("Hello, world world!", "world", "there");
  ASSERT_STREQ(s, "Hello, there world!");
  free(s);
}

// string_replace_all
TEST(StringTest, TestStringReplaceAll) {
  char *s = string_replace_all("Hello, world world!", "world", "there");
  ASSERT_STREQ(s, "Hello, there there!");
  free(s);
}

// string_trim
TEST(StringTest, TestStringTrim) {
  char s[] = "  Hello, world!  ";
  string_trim(s);
  ASSERT_STREQ(s, "Hello, world!");
}

// string_trim_chars
TEST(StringTest, TestStringTrimChars) {
  char s[] = "  Hello, world!  ";
  string_trim_chars(s, " !");
  ASSERT_STREQ(s, "Hello, world");
}

// string_trim_char
TEST(StringTest, TestStringTrimChar) {
  char s[] = "  Hello, world!  ";
  string_trim_char(s, ' ');
  ASSERT_STREQ(s, "Hello, world!");
}

// string_ltrim
TEST(StringTest, TestStringLtrim) {
  char s[] = "  Hello, world!  ";
  string_ltrim(s);
  ASSERT_STREQ(s, "Hello, world!  ");
}

// string_rtrim
TEST(StringTest, TestStringRtrim) {
  char s[] = "  Hello, world!  ";
  string_rtrim(s);
  ASSERT_STREQ(s, "  Hello, world!");
}

// string_reverse
TEST(StringTest, TestStringReverse) {
  char s[] = "Hello, world!";
  string_reverse(s);
  ASSERT_STREQ(s, "!dlrow ,olleH");
}

// string_count_substr
TEST(StringTest, TestStringCountSubstr) {
  char s[] = "Hello, world! Hello, world!";
  ASSERT_EQ(string_count_substr(s, "Hello"), 2);
}

// string_remove_char
TEST(StringTest, TestStringRemoveChar) {
  char s[] = "Hello, world!";
  string_remove_char(s, 'o');
  ASSERT_STREQ(s, "Hell, wrld!");
}

// string_remove_substr
TEST(StringTest, TestStringRemoveSubstr) {
  char s[] = "Hello, world!";
  string_remove_substr(s, 6, 7);
  ASSERT_STREQ(s, "Hello,");
}

// string_contains
TEST(StringTest, TestStringContains) {
  ASSERT_EQ(string_contains("Hello, world!", "world"), 1);
}

// string_starts_with
TEST(StringTest, TestStringStartsWith) {
  ASSERT_EQ(string_starts_with("Hello, world!", "Hello"), 1);
}

// string_ends_with
TEST(StringTest, TestStringEndsWith) {
  ASSERT_EQ(string_ends_with("Hello, world!", "world!"), 1);
}

// regex_match
TEST(StringTest, TestRegexMatch) {
  ASSERT_EQ(regex_match("Hello, world!", "Hello, world!"), 1);
}

// regex_replace
TEST(StringTest, TestRegexReplace) {
  char *s = regex_replace("Hello, world!", "world", "there");
  ASSERT_STREQ(s, "Hello, there!");
  free(s);
}

// regex_replace_all
TEST(StringTest, TestRegexReplaceAll) {
  char *s = regex_replace_all("Hello, world! Hello, world!", "world", "there");
  ASSERT_STREQ(s, "Hello, there! Hello, there!");
  free(s);
}

// regex_split
TEST(StringTest, TestRegexSplit) {
  size_t len;
  const char *tel = "123-456-7890";
  const char **result = regex_split(tel, "-", &len);

  ASSERT_EQ(len, 3);
  ASSERT_STREQ(result[0], "123");
  ASSERT_STREQ(result[1], "456");
  ASSERT_STREQ(result[2], "7890");
  free((void *)result[0]);
  free((void *)result[1]);
  free((void *)result[2]);
  free(result);
}

#ifdef USE_PCRE_REGEX

// regex_sub_match_pcre
TEST(StringTest, TestRegexSubMatchPcre) {
  char *s = regex_sub_match_pcre("Hello, world!", "Hello, ([a-z]+)!", 1);
  ASSERT_STREQ(s, "world");
  free(s);
}

// regex_capture
TEST(StringTest, TestRegexCapture) {
  const char *str = "Hello, World! How are you?";
  const char *regex = "([a-zA-Z]+), ([a-zA-Z]+)! (\\w+) (\\w+) ([a-zA-Z\?]+)";

  int num_capture_groups = 6;
  int num_matches = 0;

  char **sub_matches =
      regex_capture(str, regex, num_capture_groups, &num_matches);
  ASSERT_STREQ(sub_matches[0], "Hello, World! How are you?");
  ASSERT_STREQ(sub_matches[1], "Hello");
  ASSERT_STREQ(sub_matches[2], "World");
  ASSERT_STREQ(sub_matches[3], "How");
  ASSERT_STREQ(sub_matches[4], "are");
  ASSERT_STREQ(sub_matches[5], "you?");
  free(sub_matches[0]);
  free(sub_matches[1]);
  free(sub_matches[2]);
  free(sub_matches[3]);
  free(sub_matches[4]);
  free(sub_matches[5]);
  free(sub_matches);

  const char *str2 = "John Doe <johndoe@gmail.com> 123-456-7890";
  const char *regex2 =
      "([a-zA-Z]+ [a-zA-Z]+) <([a-zA-Z0-9]+@[a-zA-Z0-9]+\\.[a-zA-Z]+)> "
      "([0-9]+-[0-9]+-[0-9]+)";

  num_capture_groups = 4;
  num_matches = 0;

  char **sub_matches2 =
      regex_capture(str2, regex2, num_capture_groups, &num_matches);
  ASSERT_STREQ(sub_matches2[0], "John Doe <johndoe@gmail.com> 123-456-7890");
  ASSERT_STREQ(sub_matches2[1], "John Doe");
  ASSERT_STREQ(sub_matches2[2], "johndoe@gmail.com");
  ASSERT_STREQ(sub_matches2[3], "123-456-7890");

  for (int i = 0; i < num_matches; i++) {
    free(sub_matches2[i]);
  }
  free(sub_matches2);
}
#endif

// string_fmt
TEST(StringTest, TestStringFmt) {
  char *s = string_format("Hello, %s - %d!", "world", 123);
  ASSERT_STREQ(s, "Hello, world - 123!");
  free(s);
}

// string_prepend
TEST(StringTest, TestStringPrepend) {
  char *s = string_prepend("world!", "Hello, ");
  ASSERT_STREQ(s, "Hello, world!");
  free(s);
}

TEST(StringTest, TestStringToInt) {
  bool valid;
  int n = string_to_int("123", &valid);
  ASSERT_EQ(n, 123);
  ASSERT_TRUE(valid);

  n = string_to_int("123.45", &valid);
  ASSERT_EQ(n, 0);
  ASSERT_FALSE(valid);
}

TEST(StringTest, TestStringToLong) {
  bool valid;
  long n = string_to_long("123", &valid);
  ASSERT_EQ(n, 123);
  ASSERT_TRUE(valid);

  n = string_to_long("123.45", &valid);
  ASSERT_EQ(n, 0);
  ASSERT_FALSE(valid);
}

TEST(StringTest, TestStringToLongLong) {
  bool valid;
  long long n = string_to_longlong("123", &valid);
  ASSERT_EQ(n, 123);
  ASSERT_TRUE(valid);

  n = string_to_longlong("123.45", &valid);
  ASSERT_EQ(n, 0);
  ASSERT_FALSE(valid);
}

TEST(StringTest, TestStringToFloat) {
  bool valid;
  float n = string_to_float("123.45", &valid);
  ASSERT_FLOAT_EQ(n, 123.45);
  ASSERT_TRUE(valid);

  n = string_to_float("123.45.67", &valid);
  ASSERT_FLOAT_EQ(n, 0);
  ASSERT_FALSE(valid);
}

TEST(StringTest, TestStringToDouble) {
  bool valid;
  double n = string_to_double("123.45", &valid);
  ASSERT_DOUBLE_EQ(n, 123.45);
  ASSERT_TRUE(valid);

  n = string_to_double("123.45.67", &valid);
  ASSERT_DOUBLE_EQ(n, 0);
  ASSERT_FALSE(valid);
}

TEST(StringTest, TestStringToBool) {
  bool valid;
  bool n = string_to_bool("true", &valid);
  ASSERT_TRUE(n);
  ASSERT_TRUE(valid);

  n = string_to_bool("false", &valid);
  ASSERT_FALSE(n);
  ASSERT_TRUE(valid);

  n = string_to_bool("1", &valid);
  ASSERT_TRUE(n);
  ASSERT_TRUE(valid);

  n = string_to_bool("0", &valid);
  ASSERT_FALSE(n);
  ASSERT_TRUE(valid);

  n = string_to_bool("truee", &valid);
  ASSERT_FALSE(n);
  ASSERT_FALSE(valid);
}

TEST(StringTest, TestStringToIntBase) {
  bool valid;
  int n = string_to_int_base("123", 10, &valid);
  ASSERT_EQ(n, 123);
  ASSERT_TRUE(valid);

  n = string_to_int_base("123", 2, &valid);
  ASSERT_EQ(n, 0);
  ASSERT_FALSE(valid);

  // test with hex
  n = string_to_int_base("1A", 16, &valid);
  ASSERT_EQ(n, 26);

  // test with octal
  n = string_to_int_base("123", 8, &valid);
  ASSERT_EQ(n, 83);

  // test with binary
  n = string_to_int_base("1010", 2, &valid);
  ASSERT_EQ(n, 10);
}

TEST(StringTest, TestStringToLongBase) {
  bool valid;
  long n = string_to_long_base("123", 10, &valid);
  ASSERT_EQ(n, 123);
  ASSERT_TRUE(valid);

  n = string_to_long_base("123", 2, &valid);
  ASSERT_EQ(n, 0);
  ASSERT_FALSE(valid);
}

// test strings_equal
TEST(StringTest, TestStringsEqual) {
  ASSERT_TRUE(strings_equal("Hello, world!", "Hello, world!"));
  ASSERT_FALSE(strings_equal("Hello, world!", "Hello, World!"));
}

// test strings_equal_nocase
TEST(StringTest, TestStringsEqualNocase) {
  ASSERT_TRUE(strings_equal_nocase("Hello, world!", "Hello, world!"));
  ASSERT_TRUE(strings_equal_nocase("HELLO, WORLD!", "HELLO, WORLD!"));
}

// starts_with_nocase
TEST(StringTest, TestStartsWithNocase) {
  ASSERT_TRUE(string_starts_with_nocase("Hello, world!", "Hello"));
  ASSERT_TRUE(string_starts_with_nocase("HELLO, WORLD!", "hello"));
}

// ends_with_nocase
TEST(StringTest, TestEndsWithNocase) {
  ASSERT_TRUE(string_ends_with_nocase("Hello, world!", "world!"));
  ASSERT_TRUE(string_ends_with_nocase("HELLO, WORLD!", "WORLD!"));
}

// contains_nocase
TEST(StringTest, TestContainsNocase) {
  ASSERT_TRUE(string_contains_nocase("Hello, world!", "world"));
  ASSERT_TRUE(string_contains_nocase("HELLO, WORLD!", "world"));
}

// string_levenshtein_distance
TEST(StringTest, TestLevenshteinDistance) {
  ASSERT_EQ(string_levenshtein_distance("kitten", "sitting"), 3);
  ASSERT_EQ(string_levenshtein_distance("sitting", "kitten"), 3);
  ASSERT_EQ(string_levenshtein_distance("sitting", "sitting"), 0);
  ASSERT_EQ(string_levenshtein_distance("sitting", "sittin"), 1);
  ASSERT_EQ(string_levenshtein_distance("sittin", "sitting"), 1);
}

// string_hamming_distance
TEST(StringTest, TestHammingDistance) {
  ASSERT_EQ(string_hamming_distance("karolin", "kathrin"), 3);
  ASSERT_EQ(string_hamming_distance("karolin", "kerstin"), 3);
  ASSERT_EQ(string_hamming_distance("1011101", "1001001"), 2);
  ASSERT_EQ(string_hamming_distance("2173896", "2233796"), 3);
}

/*
https://rosettacode.org/wiki/Jaro_similarity#:~:text=The%20Jaro%20distance%20is%20a,1%20is%20an%20exact%20match.
*/
TEST(StringTest, TestJaroDistance) {
  EXPECT_NEAR(string_jaro_distance("MARTHA", "MARHTA"), 0.94444420, 1e-4);
  EXPECT_NEAR(string_jaro_distance("DIXON", "DICKSONX"), 0.76666665, 1e-4);
  EXPECT_NEAR(string_jaro_distance("JELLYFISH", "SMELLYFISH"), 0.89629632,
              1e-4);
}

// test string_lcs
TEST(StringTest, TestLcs) {
  char a[] = "thisisatest";
  char b[] = "testing123testing";
  char *s = NULL;
  int t = string_lcs(a, b, &s);
  printf("%.*s\n", t, s); // tsitest
  ASSERT_STREQ(s, "tsitest");
  ASSERT_EQ(t, 7);
  free(s);

  // // genes
  char a2[] = "AGGTAB";
  char b2[] = "GXTXAYB";
  char *s2 = NULL;
  int t2 = string_lcs(a2, b2, &s2);
  ASSERT_STREQ(s2, "GTAB");
  ASSERT_EQ(t2, 4);
  free(s2);
}

// test cosine_similarity
TEST(StringTest, TestCosineSimilarity) {
  char a[] = "this is a test";
  char b[] = "this is a test";
  double sim = string_cosine_similarity(a, b);
  ASSERT_DOUBLE_EQ(sim, 1.0);

  char a2[] = "ABCDEF";
  char b2[] = "ABCXYZ";
  double sim2 = string_cosine_similarity(a2, b2);
  ASSERT_DOUBLE_EQ(sim2, 0.5);
}

// test string_soundex
TEST(StringTest, TestSoundex) {
  char *s = string_soundex("Robert");
  ASSERT_STREQ(s, "R163");
  free(s);

  s = string_soundex("Rupert");
  ASSERT_STREQ(s, "R163");
  free(s);

  s = string_soundex("Rubin");
  ASSERT_STREQ(s, "R150");
  free(s);

  // Martha
  s = string_soundex("Martha");
  ASSERT_STREQ(s, "M630");
  free(s);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
