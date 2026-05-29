/**
 * @file test_strsim.c
 * @brief Unit tests for all strsim functions.
 *
 * Build and run:
 *   cc -std=c11 -Wall -Wextra -Werror strsim.c test_strsim.c -o test_strsim -lm
 *   ./test_strsim
 */

#include "../include/strsim.h"

#include <math.h>   /* for fabs */
#include <stdio.h>  /* for fprintf, printf */
#include <stdlib.h> /* for EXIT_FAILURE, EXIT_SUCCESS */

/* -------------------------------------------------------------------------
 * Minimal test runner
 * ------------------------------------------------------------------------- */

static int g_tests_run    = 0;
static int g_tests_failed = 0;

#define EXPECT_INT_EQ(a, b, msg)                                               \
    do {                                                                       \
        ++g_tests_run;                                                         \
        if ((int)(a) != (int)(b)) {                                            \
            fprintf(stderr, "FAIL [%s:%d] %s: got %d, want %d\n",            \
                    __FILE__, __LINE__, (msg), (int)(a), (int)(b));            \
            ++g_tests_failed;                                                  \
        }                                                                      \
    } while (0)

#define EXPECT_SIZE_EQ(a, b, msg)                                              \
    do {                                                                       \
        ++g_tests_run;                                                         \
        if ((size_t)(a) != (size_t)(b)) {                                      \
            fprintf(stderr, "FAIL [%s:%d] %s: got %zu, want %zu\n",          \
                    __FILE__, __LINE__, (msg), (size_t)(a), (size_t)(b));      \
            ++g_tests_failed;                                                  \
        }                                                                      \
    } while (0)

#define DBL_TOL 1e-6

#define EXPECT_DBL_NEAR(a, b, msg)                                             \
    do {                                                                       \
        ++g_tests_run;                                                         \
        if (fabs((double)(a) - (double)(b)) > DBL_TOL) {                      \
            fprintf(stderr, "FAIL [%s:%d] %s: got %.10f, want %.10f\n",      \
                    __FILE__, __LINE__, (msg), (double)(a), (double)(b));      \
            ++g_tests_failed;                                                  \
        }                                                                      \
    } while (0)

/* -------------------------------------------------------------------------
 * Cosine similarity
 * ------------------------------------------------------------------------- */

static void test_cosine(void)
{
    double sim = 0.0;

    EXPECT_INT_EQ(strsim_cosine(NULL, "x", &sim),  -1, "cosine null a");
    EXPECT_INT_EQ(strsim_cosine("x", NULL, &sim),  -1, "cosine null b");
    EXPECT_INT_EQ(strsim_cosine("x", "y", NULL),   -1, "cosine null out");
    EXPECT_INT_EQ(strsim_cosine("",  "x", &sim),   -2, "cosine empty a");
    EXPECT_INT_EQ(strsim_cosine("x", "",  &sim),   -2, "cosine empty b");

    strsim_cosine("hello", "hello", &sim);
    EXPECT_DBL_NEAR(sim, 1.0, "cosine identical");

    strsim_cosine("aaa", "bbb", &sim);
    EXPECT_DBL_NEAR(sim, 0.0, "cosine disjoint");

    /* "ab" vs "ac": dot=1, norm_a=sqrt(2), norm_b=sqrt(2) -> 0.5 */
    strsim_cosine("ab", "ac", &sim);
    EXPECT_DBL_NEAR(sim, 0.5, "cosine known value");

    double sim_ab, sim_ba;
    strsim_cosine("kitten", "sitting", &sim_ab);
    strsim_cosine("sitting", "kitten", &sim_ba);
    EXPECT_DBL_NEAR(sim_ab, sim_ba, "cosine symmetric");
}

/* -------------------------------------------------------------------------
 * Levenshtein distance
 * ------------------------------------------------------------------------- */

static void test_levenshtein(void)
{
    size_t d = 0;

    EXPECT_INT_EQ(strsim_levenshtein(NULL, "x", &d), -1, "lev null a");
    EXPECT_INT_EQ(strsim_levenshtein("x", NULL, &d), -1, "lev null b");
    EXPECT_INT_EQ(strsim_levenshtein("x", "y", NULL), -1, "lev null out");

    strsim_levenshtein("", "", &d);
    EXPECT_SIZE_EQ(d, 0, "lev both empty");

    strsim_levenshtein("", "hello", &d);
    EXPECT_SIZE_EQ(d, 5, "lev empty a");

    strsim_levenshtein("hello", "", &d);
    EXPECT_SIZE_EQ(d, 5, "lev empty b");

    strsim_levenshtein("kitten", "sitting", &d);
    EXPECT_SIZE_EQ(d, 3, "lev kitten->sitting");

    strsim_levenshtein("saturday", "sunday", &d);
    EXPECT_SIZE_EQ(d, 3, "lev saturday->sunday");

    strsim_levenshtein("abc", "xyz", &d);
    EXPECT_SIZE_EQ(d, 3, "lev all substitutions");

    size_t d_ab, d_ba;
    strsim_levenshtein("kitten", "sitting", &d_ab);
    strsim_levenshtein("sitting", "kitten", &d_ba);
    EXPECT_SIZE_EQ(d_ab, d_ba, "lev symmetric");
}

/* -------------------------------------------------------------------------
 * Damerau-Levenshtein distance
 * ------------------------------------------------------------------------- */

static void test_damerau_levenshtein(void)
{
    size_t d = 0;

    EXPECT_INT_EQ(strsim_damerau_levenshtein(NULL, "x", &d), -1, "dl null a");
    EXPECT_INT_EQ(strsim_damerau_levenshtein("x", NULL, &d), -1, "dl null b");
    EXPECT_INT_EQ(strsim_damerau_levenshtein("x", "y", NULL), -1, "dl null out");

    strsim_damerau_levenshtein("", "", &d);
    EXPECT_SIZE_EQ(d, 0, "dl both empty");

    strsim_damerau_levenshtein("", "abc", &d);
    EXPECT_SIZE_EQ(d, 3, "dl empty a");

    strsim_damerau_levenshtein("abc", "", &d);
    EXPECT_SIZE_EQ(d, 3, "dl empty b");

    /* Core distinction from Levenshtein: transposition = 1 op. */
    strsim_damerau_levenshtein("teh", "the", &d);
    EXPECT_SIZE_EQ(d, 1, "dl transposition teh->the");

    strsim_damerau_levenshtein("ab", "ba", &d);
    EXPECT_SIZE_EQ(d, 1, "dl transposition ab->ba");

    /* Identical strings. */
    strsim_damerau_levenshtein("hello", "hello", &d);
    EXPECT_SIZE_EQ(d, 0, "dl identical");

    /* Classic cases shared with Levenshtein. */
    strsim_damerau_levenshtein("kitten", "sitting", &d);
    EXPECT_SIZE_EQ(d, 3, "dl kitten->sitting");

    strsim_damerau_levenshtein("saturday", "sunday", &d);
    EXPECT_SIZE_EQ(d, 3, "dl saturday->sunday");

    /* Symmetry. */
    size_t d_ab, d_ba;
    strsim_damerau_levenshtein("ca", "abc", &d_ab);
    strsim_damerau_levenshtein("abc", "ca", &d_ba);
    EXPECT_SIZE_EQ(d_ab, d_ba, "dl symmetric");
}

/* -------------------------------------------------------------------------
 * Jaro-Winkler similarity
 * ------------------------------------------------------------------------- */

static void test_jaro_winkler(void)
{
    double sim = 0.0;

    EXPECT_INT_EQ(strsim_jaro_winkler(NULL, "x", &sim), -1, "jw null a");
    EXPECT_INT_EQ(strsim_jaro_winkler("x", NULL, &sim), -1, "jw null b");
    EXPECT_INT_EQ(strsim_jaro_winkler("x", "y", NULL),  -1, "jw null out");

    strsim_jaro_winkler("", "", &sim);
    EXPECT_DBL_NEAR(sim, 1.0, "jw both empty");

    strsim_jaro_winkler("", "abc", &sim);
    EXPECT_DBL_NEAR(sim, 0.0, "jw empty a");

    strsim_jaro_winkler("abc", "", &sim);
    EXPECT_DBL_NEAR(sim, 0.0, "jw empty b");

    strsim_jaro_winkler("hello", "hello", &sim);
    EXPECT_DBL_NEAR(sim, 1.0, "jw identical");

    strsim_jaro_winkler("abc", "xyz", &sim);
    EXPECT_DBL_NEAR(sim, 0.0, "jw fully disjoint");

    /*
     * Canonical textbook example: "MARTHA" vs "MARHTA".
     * Jaro = 0.9444..., prefix = 3 ("MAR"), JW = 0.9611...
     */
    strsim_jaro_winkler("MARTHA", "MARHTA", &sim);
    EXPECT_DBL_NEAR(sim, 0.961111, "jw MARTHA/MARHTA");

    /* Symmetry. */
    double sim_ab, sim_ba;
    strsim_jaro_winkler("kitten", "sitting", &sim_ab);
    strsim_jaro_winkler("sitting", "kitten", &sim_ba);
    EXPECT_DBL_NEAR(sim_ab, sim_ba, "jw symmetric");

    /* Prefix bonus: "JEFFery" vs "JEFFrey" share a 4-char prefix. */
    double sim_prefix, sim_noprefix;
    strsim_jaro_winkler("JEFFery", "JEFFrey", &sim_prefix);
    strsim_jaro_winkler("eryJEFF", "reyJEFF", &sim_noprefix);
    /* The version with the shared prefix must score higher or equal. */
    ++g_tests_run;
    if (sim_prefix < sim_noprefix) {
        fprintf(stderr,
                "FAIL [%s:%d] jw prefix bonus: %.6f not >= %.6f\n",
                __FILE__, __LINE__, sim_prefix, sim_noprefix);
        ++g_tests_failed;
    }
}

/* -------------------------------------------------------------------------
 * Jaccard bigram similarity
 * ------------------------------------------------------------------------- */

static void test_jaccard_bigram(void)
{
    double sim = 0.0;

    EXPECT_INT_EQ(strsim_jaccard_bigram(NULL, "x", &sim), -1, "jac null a");
    EXPECT_INT_EQ(strsim_jaccard_bigram("x", NULL, &sim), -1, "jac null b");
    EXPECT_INT_EQ(strsim_jaccard_bigram("x", "y", NULL),  -1, "jac null out");

    /* Single-char strings have no bigrams. */
    strsim_jaccard_bigram("a", "a", &sim);
    EXPECT_DBL_NEAR(sim, 1.0, "jac both single char equal");

    strsim_jaccard_bigram("a", "b", &sim);
    EXPECT_DBL_NEAR(sim, 0.0, "jac both single char different");

    strsim_jaccard_bigram("a", "ab", &sim);
    EXPECT_DBL_NEAR(sim, 0.0, "jac one single char");

    strsim_jaccard_bigram("hello", "hello", &sim);
    EXPECT_DBL_NEAR(sim, 1.0, "jac identical");

    strsim_jaccard_bigram("abc", "xyz", &sim);
    EXPECT_DBL_NEAR(sim, 0.0, "jac fully disjoint");

    /*
     * "night" bigrams: {ni, ig, gh, ht} (4 unique)
     * "nightly" bigrams: {ni, ig, gh, ht, tl, ly} (6 unique)
     * intersection = 4, union = 6 -> 4/6 = 0.6667
     */
    strsim_jaccard_bigram("night", "nightly", &sim);
    EXPECT_DBL_NEAR(sim, 4.0 / 6.0, "jac night/nightly");

    /* Symmetry. */
    double sim_ab, sim_ba;
    strsim_jaccard_bigram("kitten", "sitting", &sim_ab);
    strsim_jaccard_bigram("sitting", "kitten", &sim_ba);
    EXPECT_DBL_NEAR(sim_ab, sim_ba, "jac symmetric");
}

/* -------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------- */

int main(void)
{
    printf("=== strsim test suite ===\n\n");

    printf("-- cosine similarity --\n");
    test_cosine();

    printf("-- levenshtein distance --\n");
    test_levenshtein();

    printf("-- damerau-levenshtein distance --\n");
    test_damerau_levenshtein();

    printf("-- jaro-winkler similarity --\n");
    test_jaro_winkler();

    printf("-- jaccard bigram similarity --\n");
    test_jaccard_bigram();

    printf("\n%d test(s) run, %d failed.\n", g_tests_run, g_tests_failed);
    return (g_tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
