/**
 * @file regex_test.c
 * @brief Test harness for the regex API (regex.h / regex.c).
 *
 * Design:
 *   - Zero external dependencies; the harness is self-contained.
 *   - Each TEST() block is an independent unit; failures are accumulated and
 *     reported at the end so the entire suite always runs.
 *   - Helpers EXPECT_EQ / EXPECT_STR / EXPECT_TRUE / EXPECT_FALSE print the
 *     source location on failure and set a per-test failure flag.
 *   - A top-level summary line gives a pass/fail count.
 */

#include "regex.h"

#include <stdarg.h> /* for va_list, va_start, va_end */
#include <stdio.h>  /* for printf, fprintf, stderr   */
#include <stdlib.h> /* for EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h> /* for strcmp, strlen             */

/* ---------------------------------------------------------------------------
 * Minimal test framework
 * ------------------------------------------------------------------------- */

static int g_total = 0;  /* tests registered */
static int g_passed = 0; /* tests that passed */
static int g_failed = 0; /* tests that failed */

/* Per-test failure flag; reset at the start of each TEST block. */
static int g_test_failed = 0;

/* Name of the currently-running test (for failure messages). */
static const char* g_test_name = NULL;

/** Marks the current test as failed and prints a formatted diagnostic. */
static void fail_at(const char* file, int line, const char* fmt, ...) {
    if (!g_test_failed) {
        /* Print the test name only on first failure within a test. */
        fprintf(stderr, "\n  FAIL  %s\n", g_test_name);
    }
    g_test_failed = 1;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "    %s:%d: ", file, line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

#define EXPECT_EQ(a, b)                                                                          \
    do {                                                                                         \
        long long _a = (long long)(a), _b = (long long)(b);                                      \
        if (_a != _b) fail_at(__FILE__, __LINE__, "%s == %s  =>  %lld != %lld", #a, #b, _a, _b); \
    } while (0)

#define EXPECT_NE(a, b)                                                                       \
    do {                                                                                      \
        long long _a = (long long)(a), _b = (long long)(b);                                   \
        if (_a == _b) fail_at(__FILE__, __LINE__, "%s != %s  =>  both are %lld", #a, #b, _a); \
    } while (0)

#define EXPECT_TRUE(expr)                                                     \
    do {                                                                      \
        if (!(expr)) fail_at(__FILE__, __LINE__, "expected true: %s", #expr); \
    } while (0)

#define EXPECT_FALSE(expr)                                                    \
    do {                                                                      \
        if ((expr)) fail_at(__FILE__, __LINE__, "expected false: %s", #expr); \
    } while (0)

#define EXPECT_STR(a, b)                                                                              \
    do {                                                                                              \
        const char *_a = (a), *_b = (b);                                                              \
        if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0)                                          \
            fail_at(__FILE__, __LINE__, "%s == %s  =>  \"%s\" != \"%s\"", #a, #b, _a ? _a : "(null)", \
                    _b ? _b : "(null)");                                                              \
    } while (0)

/**
 * Verifies that the span described by match->group[g] equals the substring
 * of subject from byte start to byte end (exclusive).
 */
#define EXPECT_SPAN(match_, g_, subject_, byte_start_, byte_end_)                                     \
    do {                                                                                              \
        size_t _s = (size_t)(byte_start_);                                                            \
        size_t _e = (size_t)(byte_end_);                                                              \
        EXPECT_EQ((match_).group[(g_)].start, _s);                                                    \
        EXPECT_EQ((match_).group[(g_)].end, _e);                                                      \
        size_t _len = _e - _s;                                                                        \
        if ((match_).group[(g_)].start != PCRE2_UNSET && _len > 0) {                                  \
            int _ok = (strncmp((subject_) + _s, (subject_) + (match_).group[(g_)].start, _len) == 0); \
            if (!_ok) fail_at(__FILE__, __LINE__, "group[%d] text mismatch", (int)(g_));              \
        }                                                                                             \
    } while (0)

/** Begins a named test.  Must be paired with END_TEST. */
#define BEGIN_TEST(name)           \
    do {                           \
        g_test_name = (name);      \
        g_test_failed = 0;         \
        g_total++;                 \
        printf("  %-65s", (name)); \
        fflush(stdout);

/** Closes a BEGIN_TEST block and records the result. */
#define END_TEST         \
    if (g_test_failed) { \
        g_failed++;      \
    } else {             \
        g_passed++;      \
        printf("ok\n");  \
    }                    \
    }                    \
    while (0)

/* ---------------------------------------------------------------------------
 * Test suite
 * ------------------------------------------------------------------------- */

/* --- compile ------------------------------------------------------------ */

static void test_compile_basic(void) {
    BEGIN_TEST("compile: valid pattern succeeds")
    regex_t* re = regex_must_compile("hello", REGEX_FLAG_NONE);
    EXPECT_TRUE(re != NULL);
    regex_free(re);
    END_TEST;
}

static void test_compile_bad_pattern(void) {
    BEGIN_TEST("compile: invalid pattern returns REGEX_ERROR")
    char errbuf[256] = {0};
    regex_t* re = NULL;
    regex_status_t st = regex_compile("([unclosed", REGEX_FLAG_NONE, &re, errbuf, sizeof(errbuf));
    EXPECT_EQ(st, REGEX_ERROR);
    EXPECT_TRUE(re == NULL);
    EXPECT_TRUE(errbuf[0] != '\0'); /* error message populated */
    END_TEST;
}

static void test_compile_null_args(void) {
    BEGIN_TEST("compile: NULL pattern/out returns REGEX_ERROR_ARGS")
    regex_t* re = NULL;
    EXPECT_EQ(regex_compile(NULL, REGEX_FLAG_NONE, &re, NULL, 0), REGEX_ERROR_ARGS);
    EXPECT_EQ(regex_compile("x", REGEX_FLAG_NONE, NULL, NULL, 0), REGEX_ERROR_ARGS);
    END_TEST;
}

static void test_compile_pattern_introspection(void) {
    BEGIN_TEST("compile: regex_pattern() returns original pattern")
    regex_t* re = regex_must_compile("foo(bar)", REGEX_FLAG_NONE);
    if (re) {
        EXPECT_STR(regex_pattern(re), "foo(bar)");
        EXPECT_EQ(regex_group_count(re), 1u);
        regex_free(re);
    }
    END_TEST;
}

static void test_compile_group_count(void) {
    BEGIN_TEST("compile: group count reflects capture groups")
    regex_t* re = regex_must_compile("(a)(b(c))", REGEX_FLAG_NONE);
    if (re) {
        /* Three capturing groups: (a), (b(c)), (c) */
        EXPECT_EQ(regex_group_count(re), 3u);
        regex_free(re);
    }
    END_TEST;
}

static void test_compile_flags_caseless(void) {
    BEGIN_TEST("compile: REGEX_FLAG_CASELESS compiles without error")
    regex_t* re = regex_must_compile("ABC", REGEX_FLAG_CASELESS);
    EXPECT_TRUE(re != NULL);
    regex_free(re);
    END_TEST;
}

/* --- ref-counting ------------------------------------------------------- */

static void test_retain_free(void) {
    BEGIN_TEST("retain/free: double-retain allows two free calls")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    if (re) {
        regex_t* alias = regex_retain(re);
        EXPECT_TRUE(alias == re);
        regex_free(alias); /* drops to refcount 1 */
        regex_free(re);    /* drops to 0; frees memory */
        /* If refcounting is wrong this will crash under ASan/Valgrind. */
    }
    END_TEST;
}

static void test_free_null(void) {
    BEGIN_TEST("free: NULL is a no-op")
    regex_free(NULL); /* must not crash */
    END_TEST;
}

/* --- context ------------------------------------------------------------ */

static void test_ctx_create_free(void) {
    BEGIN_TEST("ctx: create and free succeed")
    regex_ctx_t* ctx = regex_ctx_must_create();
    EXPECT_TRUE(ctx != NULL);
    regex_ctx_free(ctx);
    END_TEST;
}

static void test_ctx_free_null(void) {
    BEGIN_TEST("ctx: free(NULL) is a no-op")
    regex_ctx_free(NULL);
    END_TEST;
}

static void test_ctx_null_arg(void) {
    BEGIN_TEST("ctx: create with NULL out returns REGEX_ERROR_ARGS")
    EXPECT_EQ(regex_ctx_create(NULL), REGEX_ERROR_ARGS);
    END_TEST;
}

/* --- regex_exec --------------------------------------------------------- */

static void test_exec_basic_match(void) {
    BEGIN_TEST("exec: simple literal match")
    regex_t* re = regex_must_compile("world", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "hello world";
        regex_match_t match = {0};
        regex_status_t st = regex_exec(re, ctx, subj, strlen(subj), 0, &match);
        EXPECT_EQ(st, REGEX_OK);
        EXPECT_SPAN(match, 0, subj, 6, 11);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_no_match(void) {
    BEGIN_TEST("exec: REGEX_NO_MATCH when pattern absent")
    regex_t* re = regex_must_compile("xyz", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "hello world";
        regex_match_t match = {0};
        regex_status_t st = regex_exec(re, ctx, subj, strlen(subj), 0, &match);
        EXPECT_EQ(st, REGEX_NO_MATCH);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_offset(void) {
    BEGIN_TEST("exec: start offset skips earlier occurrence")
    regex_t* re = regex_must_compile("cat", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "cat and cat";
        regex_match_t match = {0};
        /* Skip past the first "cat" at offset 0 by starting at 4. */
        regex_status_t st = regex_exec(re, ctx, subj, strlen(subj), 4, &match);
        EXPECT_EQ(st, REGEX_OK);
        EXPECT_SPAN(match, 0, subj, 8, 11);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_capture_groups(void) {
    BEGIN_TEST("exec: capture groups populated correctly")
    regex_t* re = regex_must_compile("(\\w+)@(\\w+\\.\\w+)", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "user@example.com";
        regex_match_t match = {0};
        regex_status_t st = regex_exec(re, ctx, subj, strlen(subj), 0, &match);

        EXPECT_EQ(st, REGEX_OK);
        EXPECT_EQ(match.count, 3u); /* g0 + two groups */
        /* g0: full match */
        EXPECT_SPAN(match, 0, subj, 0, 16);
        /* g1: "user" */
        EXPECT_SPAN(match, 1, subj, 0, 4);
        /* g2: "example.com" */
        EXPECT_SPAN(match, 2, subj, 5, 16);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_null_args(void) {
    BEGIN_TEST("exec: NULL arguments return REGEX_ERROR_ARGS")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    regex_match_t m = {0};
    if (re && ctx) {
        EXPECT_EQ(regex_exec(NULL, ctx, "x", 1, 0, &m), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_exec(re, NULL, "x", 1, 0, &m), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_exec(re, ctx, NULL, 1, 0, &m), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_exec(re, ctx, "x", 1, 0, NULL), REGEX_ERROR_ARGS);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_offset_beyond_len(void) {
    BEGIN_TEST("exec: offset > len returns REGEX_ERROR_ARGS")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        regex_match_t m = {0};
        EXPECT_EQ(regex_exec(re, ctx, "abc", 3, 99, &m), REGEX_ERROR_ARGS);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_empty_subject(void) {
    BEGIN_TEST("exec: empty subject with non-matching pattern")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        regex_match_t m = {0};
        EXPECT_EQ(regex_exec(re, ctx, "", 0, 0, &m), REGEX_NO_MATCH);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_binary_subject(void) {
    BEGIN_TEST("exec: binary (non-NUL-terminated) subject works")
    regex_t* re = regex_must_compile("\\x02\\x03", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char subj[] = {0x01, 0x02, 0x03, 0x04};
        regex_match_t m = {0};
        regex_status_t st = regex_exec(re, ctx, subj, sizeof(subj), 0, &m);
        EXPECT_EQ(st, REGEX_OK);
        EXPECT_SPAN(m, 0, subj, 1, 3);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

/* --- regex_match -------------------------------------------------------- */

static void test_match_convenience(void) {
    BEGIN_TEST("match: NUL-terminated convenience wrapper")
    regex_t* re = regex_must_compile("\\d+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "abc 42 def";
        regex_match_t m = {0};
        EXPECT_EQ(regex_match(re, ctx, subj, &m), REGEX_OK);
        EXPECT_SPAN(m, 0, subj, 4, 6);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_match_null_subject(void) {
    BEGIN_TEST("match: NULL subject returns REGEX_ERROR_ARGS")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        regex_match_t m = {0};
        EXPECT_EQ(regex_match(re, ctx, NULL, &m), REGEX_ERROR_ARGS);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

/* --- regex_is_match ----------------------------------------------------- */

static void test_is_match_true(void) {
    BEGIN_TEST("is_match: returns true on match")
    regex_t* re = regex_must_compile("cat", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* s = "the cat sat";
        EXPECT_TRUE(regex_is_match(re, ctx, s, strlen(s)));
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_is_match_false(void) {
    BEGIN_TEST("is_match: returns false on no match")
    regex_t* re = regex_must_compile("dog", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* s = "the cat sat";
        EXPECT_FALSE(regex_is_match(re, ctx, s, strlen(s)));
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_is_match_null_args(void) {
    BEGIN_TEST("is_match: NULL arguments return false")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        EXPECT_FALSE(regex_is_match(NULL, ctx, "x", 1));
        EXPECT_FALSE(regex_is_match(re, NULL, "x", 1));
        EXPECT_FALSE(regex_is_match(re, ctx, NULL, 1));
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

/* --- regex_iter --------------------------------------------------------- */

static void test_iter_all_matches(void) {
    BEGIN_TEST("iter: finds all non-overlapping matches")
    regex_t* re = regex_must_compile("\\d+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "one 1 two 22 three 333";
        regex_iter_t* iter = NULL;
        EXPECT_EQ(regex_iter_init(re, ctx, subj, strlen(subj), &iter), REGEX_OK);

        regex_match_t m = {0};

        /* First match: "1" at [4,5) */
        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_OK);
        EXPECT_SPAN(m, 0, subj, 4, 5);

        /* Second match: "22" at [10,12) */
        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_OK);
        EXPECT_SPAN(m, 0, subj, 10, 12);

        /* Third match: "333" at [19,22) */
        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_OK);
        EXPECT_SPAN(m, 0, subj, 19, 22);

        /* Exhausted */
        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_NO_MATCH);

        regex_iter_free(iter);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_iter_no_matches(void) {
    BEGIN_TEST("iter: REGEX_NO_MATCH immediately when pattern absent")
    regex_t* re = regex_must_compile("\\d+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "no digits here";
        regex_iter_t* iter = NULL;
        EXPECT_EQ(regex_iter_init(re, ctx, subj, strlen(subj), &iter), REGEX_OK);
        regex_match_t m = {0};
        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_NO_MATCH);
        regex_iter_free(iter);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_iter_zero_width_match(void) {
    BEGIN_TEST("iter: zero-width match advances without infinite loop")
    /* a* matches the empty string between every character. */
    regex_t* re = regex_must_compile("a*", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "b";
        regex_iter_t* iter = NULL;
        EXPECT_EQ(regex_iter_init(re, ctx, subj, strlen(subj), &iter), REGEX_OK);

        regex_match_t m = {0};
        int n = 0;
        regex_status_t st;
        /* Count matches; must terminate in a finite number of steps. */
        while ((st = regex_iter_next(iter, &m)) == REGEX_OK) {
            n++;
            if (n > 10) {
                fail_at(__FILE__, __LINE__, "zero-width iterator appears infinite");
                break;
            }
        }
        EXPECT_EQ(st, REGEX_NO_MATCH);
        regex_iter_free(iter);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_iter_null_args(void) {
    BEGIN_TEST("iter: NULL arguments return REGEX_ERROR_ARGS")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        regex_iter_t* iter = NULL;
        EXPECT_EQ(regex_iter_init(NULL, ctx, "x", 1, &iter), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_iter_init(re, NULL, "x", 1, &iter), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_iter_init(re, ctx, NULL, 1, &iter), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_iter_init(re, ctx, "x", 1, NULL), REGEX_ERROR_ARGS);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_iter_free_null(void) {
    BEGIN_TEST("iter: free(NULL) is a no-op")
    regex_iter_free(NULL);
    END_TEST;
}

static void test_iter_count_words(void) {
    BEGIN_TEST("iter: count words via \\w+ iteration")
    regex_t* re = regex_must_compile("\\w+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "  the quick  brown fox  ";
        regex_iter_t* iter = NULL;
        EXPECT_EQ(regex_iter_init(re, ctx, subj, strlen(subj), &iter), REGEX_OK);
        regex_match_t m = {0};
        int count = 0;
        while (regex_iter_next(iter, &m) == REGEX_OK) {
            count++;
        }
        EXPECT_EQ(count, 4);
        regex_iter_free(iter);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

/* --- regex_sub ---------------------------------------------------------- */

static void test_sub_basic(void) {
    BEGIN_TEST("sub: replaces first match only")
    regex_t* re = regex_must_compile("cat", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "cat and cat";
        char buf[64];
        size_t len = sizeof(buf);
        regex_status_t st = regex_sub(re, ctx, subj, strlen(subj), "dog", buf, &len);
        EXPECT_EQ(st, REGEX_OK);
        EXPECT_STR(buf, "dog and cat");
        EXPECT_EQ(len, strlen("dog and cat"));
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_sub_with_backref(void) {
    BEGIN_TEST("sub: back-reference $1 in replacement")
    regex_t* re = regex_must_compile("(\\w+)=(\\w+)", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "key=value";
        char buf[64];
        size_t len = sizeof(buf);
        /* Swap key and value. */
        regex_status_t st = regex_sub(re, ctx, subj, strlen(subj), "$2=$1", buf, &len);
        EXPECT_EQ(st, REGEX_OK);
        EXPECT_STR(buf, "value=key");
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_sub_no_match(void) {
    BEGIN_TEST("sub: REGEX_NO_MATCH when pattern absent")
    regex_t* re = regex_must_compile("xyz", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "hello world";
        char buf[64];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_sub(re, ctx, subj, strlen(subj), "ZZZ", buf, &len), REGEX_NO_MATCH);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_sub_buffer_too_small(void) {
    BEGIN_TEST("sub: REGEX_ERROR when output buffer too small, out_len updated")
    regex_t* re = regex_must_compile("a", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "abc";
        char buf[2]; /* deliberately tiny */
        size_t len = sizeof(buf);
        regex_status_t st = regex_sub(re, ctx, subj, strlen(subj), "ZZZ", buf, &len);
        EXPECT_EQ(st, REGEX_ERROR);
        /* out_len must now hold the required capacity. */
        EXPECT_TRUE(len > sizeof(buf));
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_sub_null_args(void) {
    BEGIN_TEST("sub: NULL arguments return REGEX_ERROR_ARGS")
    regex_t* re = regex_must_compile("x", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        char buf[8];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_sub(NULL, ctx, "x", 1, "y", buf, &len), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_sub(re, NULL, "x", 1, "y", buf, &len), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_sub(re, ctx, NULL, 1, "y", buf, &len), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_sub(re, ctx, "x", 1, NULL, buf, &len), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_sub(re, ctx, "x", 1, "y", NULL, &len), REGEX_ERROR_ARGS);
        EXPECT_EQ(regex_sub(re, ctx, "x", 1, "y", buf, NULL), REGEX_ERROR_ARGS);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

/* --- regex_gsub --------------------------------------------------------- */

static void test_gsub_basic(void) {
    BEGIN_TEST("gsub: replaces all non-overlapping matches")
    regex_t* re = regex_must_compile("cat", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "cat and cat and cat";
        char buf[64];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_gsub(re, ctx, subj, strlen(subj), "dog", buf, &len), REGEX_OK);
        EXPECT_STR(buf, "dog and dog and dog");
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_gsub_digit_redaction(void) {
    BEGIN_TEST("gsub: redact all digit sequences")
    regex_t* re = regex_must_compile("\\d+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "order 42 shipped on day 7";
        char buf[64];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_gsub(re, ctx, subj, strlen(subj), "***", buf, &len), REGEX_OK);
        EXPECT_STR(buf, "order *** shipped on day ***");
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_gsub_no_match(void) {
    BEGIN_TEST("gsub: REGEX_NO_MATCH when pattern absent")
    regex_t* re = regex_must_compile("xyz", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "hello";
        char buf[32];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_gsub(re, ctx, subj, strlen(subj), "ZZZ", buf, &len), REGEX_NO_MATCH);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_gsub_named_backref(void) {
    BEGIN_TEST("gsub: named back-reference ${name} in replacement")
    regex_t* re = regex_must_compile("(?P<word>[A-Z][a-z]+)", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "Hello World";
        char buf[64];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_gsub(re, ctx, subj, strlen(subj), "[${word}]", buf, &len), REGEX_OK);
        EXPECT_STR(buf, "[Hello] [World]");
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

/* --- flags -------------------------------------------------------------- */

static void test_flag_caseless(void) {
    BEGIN_TEST("flag: REGEX_FLAG_CASELESS matches regardless of case")
    regex_t* re = regex_must_compile("hello", REGEX_FLAG_CASELESS);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        regex_match_t m = {0};
        EXPECT_EQ(regex_match(re, ctx, "HELLO", &m), REGEX_OK);
        EXPECT_EQ(regex_match(re, ctx, "HeLLo", &m), REGEX_OK);
        EXPECT_EQ(regex_match(re, ctx, "world", &m), REGEX_NO_MATCH);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_flag_multiline(void) {
    BEGIN_TEST("flag: REGEX_FLAG_MULTILINE anchors ^ to each line")
    regex_t* re = regex_must_compile("^line", REGEX_FLAG_MULTILINE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "first\nline two\nline three";
        regex_match_t m = {0};
        /* Anchored to start of second line. */
        regex_status_t st = regex_exec(re, ctx, subj, strlen(subj), 6, &m);
        EXPECT_EQ(st, REGEX_OK);
        EXPECT_SPAN(m, 0, subj, 6, 10);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_flag_dotall(void) {
    BEGIN_TEST("flag: REGEX_FLAG_DOTALL lets . match newline")
    regex_t* re_off = regex_must_compile("a.b", REGEX_FLAG_NONE);
    regex_t* re_on = regex_must_compile("a.b", REGEX_FLAG_DOTALL);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re_off && re_on && ctx) {
        const char* subj = "a\nb";
        regex_match_t m = {0};
        EXPECT_EQ(regex_match(re_off, ctx, subj, &m), REGEX_NO_MATCH);
        EXPECT_EQ(regex_match(re_on, ctx, subj, &m), REGEX_OK);
    }
    regex_ctx_free(ctx);
    regex_free(re_off);
    regex_free(re_on);
    END_TEST;
}

static void test_flag_utf(void) {
    BEGIN_TEST("flag: REGEX_FLAG_UTF matches Unicode code point class")
    /* \w with UCP should match the accented character é (U+00E9). */
    regex_t* re = regex_must_compile("\\w+", REGEX_FLAG_UTF | REGEX_FLAG_UCP);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "caf\xc3\xa9"; /* "café" in UTF-8 */
        regex_match_t m = {0};
        EXPECT_EQ(regex_match(re, ctx, subj, &m), REGEX_OK);
        /* The entire word should be captured. */
        EXPECT_SPAN(m, 0, subj, 0, strlen(subj));
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

/* --- introspection ------------------------------------------------------ */

static void test_group_count_no_groups(void) {
    BEGIN_TEST("introspection: group count 0 for pattern with no groups")
    regex_t* re = regex_must_compile("abc", REGEX_FLAG_NONE);
    if (re) {
        EXPECT_EQ(regex_group_count(re), 0u);
        regex_free(re);
    }
    END_TEST;
}

static void test_group_count_null(void) {
    BEGIN_TEST("introspection: group_count(NULL) returns 0")
    EXPECT_EQ(regex_group_count(NULL), 0u);
    END_TEST;
}

static void test_pattern_null(void) {
    BEGIN_TEST("introspection: pattern(NULL) returns empty string")
    const char* p = regex_pattern(NULL);
    EXPECT_TRUE(p != NULL && p[0] == '\0');
    END_TEST;
}

static void test_strerror(void) {
    BEGIN_TEST("introspection: regex_strerror covers all status codes")
    char buf[64];
    /* Verify each code produces a non-empty, non-null string. */
    regex_status_t codes[] = {
        REGEX_OK, REGEX_NO_MATCH, REGEX_ERROR, REGEX_ERROR_NOMEM, REGEX_ERROR_ARGS, REGEX_ERROR_LIMIT,
    };
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        buf[0] = '\0';
        regex_strerror(codes[i], buf, sizeof(buf));
        EXPECT_TRUE(buf[0] != '\0');
    }
    /* NULL buf must not crash. */
    regex_strerror(REGEX_OK, NULL, 0);
    END_TEST;
}

/* --- edge / regression -------------------------------------------------- */

static void test_exec_reuse_ctx(void) {
    BEGIN_TEST("edge: same ctx reused across different patterns")
    regex_t* re1 = regex_must_compile("\\d+", REGEX_FLAG_NONE);
    regex_t* re2 = regex_must_compile("[A-Z]+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re1 && re2 && ctx) {
        regex_match_t m = {0};
        EXPECT_EQ(regex_match(re1, ctx, "abc 99 XYZ", &m), REGEX_OK);
        EXPECT_EQ(regex_match(re2, ctx, "abc 99 XYZ", &m), REGEX_OK);
    }
    regex_ctx_free(ctx);
    regex_free(re1);
    regex_free(re2);
    END_TEST;
}

static void test_iter_captures_within_iter(void) {
    BEGIN_TEST("edge: capture groups accessible inside iter results")
    regex_t* re = regex_must_compile("(\\d+)-(\\d+)", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "10-20 and 30-40";
        regex_iter_t* iter = NULL;
        EXPECT_EQ(regex_iter_init(re, ctx, subj, strlen(subj), &iter), REGEX_OK);

        regex_match_t m = {0};

        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_OK);
        EXPECT_SPAN(m, 1, subj, 0, 2); /* "10" */
        EXPECT_SPAN(m, 2, subj, 3, 5); /* "20" */

        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_OK);
        EXPECT_SPAN(m, 1, subj, 10, 12); /* "30" */
        EXPECT_SPAN(m, 2, subj, 13, 15); /* "40" */

        EXPECT_EQ(regex_iter_next(iter, &m), REGEX_NO_MATCH);
        regex_iter_free(iter);
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_gsub_empty_replacement(void) {
    BEGIN_TEST("edge: gsub with empty replacement deletes all matches")
    regex_t* re = regex_must_compile("\\s+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "a b c d";
        char buf[16];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_gsub(re, ctx, subj, strlen(subj), "", buf, &len), REGEX_OK);
        EXPECT_STR(buf, "abcd");
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_sub_anchored_pattern(void) {
    BEGIN_TEST("edge: sub with anchored pattern matches only at start")
    regex_t* re = regex_must_compile("^\\w+", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (re && ctx) {
        const char* subj = "hello world";
        char buf[32];
        size_t len = sizeof(buf);
        EXPECT_EQ(regex_sub(re, ctx, subj, strlen(subj), "goodbye", buf, &len), REGEX_OK);
        EXPECT_STR(buf, "goodbye world");
    }
    regex_ctx_free(ctx);
    regex_free(re);
    END_TEST;
}

static void test_exec_http_headers(void) {
    BEGIN_TEST("exec: parse HTTP headers with named capture groups")
    /*
     * Two-pass strategy:
     *   Pass 1 - iterate over each header line using the line pattern.
     *   Pass 2 - apply the field pattern to each line to extract
     *            the field name and value.
     *
     * The subject is a raw HTTP/1.1 response header block as it arrives
     * off the wire, with CRLF line endings.
     */
    const char* headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: 1024\r\n"
        "X-Request-Id: abc-123\r\n"
        "Cache-Control: no-cache, no-store\r\n"
        "\r\n";

    /*
     * Pass 1: match each non-empty line (stops before the blank CRLF
     * that terminates the header block).
     */
    regex_t* line_re = regex_must_compile("[^\\r\\n]+", REGEX_FLAG_NONE);

    /*
     * Pass 2: split a single header line into name and value.
     *   (?P<name>[^:]+)   - field name: anything before the first colon
     *   :\\s*             - colon plus optional whitespace
     *   (?P<value>.+)     - field value: rest of line (trimmed by the
     *                       line_re above which excludes \r\n)
     */
    regex_t* field_re = regex_must_compile("(?P<name>[^:]+):\\s*(?P<value>.+)", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();

    if (!line_re || !field_re || !ctx) {
        goto cleanup;
    }

    /* Expected results in order of appearance. */
    static const struct {
        const char* name;
        const char* value;
    } expected[] = {
        {NULL, "HTTP/1.1 200 OK"},
        {"Content-Type", "application/json; charset=utf-8"},
        {"Content-Length", "1024"},
        {"X-Request-Id", "abc-123"},
        {"Cache-Control", "no-cache, no-store"},
    };
    const size_t expected_count = sizeof(expected) / sizeof(expected[0]);

    regex_iter_t* iter = NULL;
    EXPECT_EQ(regex_iter_init(line_re, ctx, headers, strlen(headers), &iter), REGEX_OK);

    regex_match_t line_m = {0};
    size_t i = 0;

    while (regex_iter_next(iter, &line_m) == REGEX_OK) {
        /* Extract the line as a bounded substring (not NUL-terminated). */
        size_t line_start = line_m.group[0].start;
        size_t line_len = line_m.group[0].end - line_start;
        const char* line = headers + line_start;

        if (i >= expected_count) {
            fail_at(__FILE__, __LINE__, "more lines matched than expected (i=%zu)", i);
            break;
        }

        if (expected[i].name == NULL) {
            /* Status line — just verify the full text. */
            EXPECT_EQ(line_len, strlen(expected[i].value));
            EXPECT_TRUE(strncmp(line, expected[i].value, line_len) == 0);
        } else {
            /* Header line — apply the field pattern. */
            regex_match_t field_m = {0};
            regex_status_t st = regex_exec(field_re, ctx, line, line_len, 0, &field_m);
            EXPECT_EQ(st, REGEX_OK);

            if (st == REGEX_OK) {
                /* group[1] = name, group[2] = value */
                size_t name_start = field_m.group[1].start;
                size_t name_len = field_m.group[1].end - name_start;
                size_t val_start = field_m.group[2].start;
                size_t val_len = field_m.group[2].end - val_start;

                EXPECT_EQ(name_len, strlen(expected[i].name));
                EXPECT_TRUE(strncmp(line + name_start, expected[i].name, name_len) == 0);

                EXPECT_EQ(val_len, strlen(expected[i].value));
                EXPECT_TRUE(strncmp(line + val_start, expected[i].value, val_len) == 0);
            }
        }
        i++;
    }

    EXPECT_EQ(i, expected_count);
    regex_iter_free(iter);

cleanup:
    regex_ctx_free(ctx);
    regex_free(field_re);
    regex_free(line_re);
    END_TEST;
}

/* ---------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------- */

int main(void) {
    printf("regex API test suite\n");
    printf("%-57s%s\n", "test", "result");
    printf("%.57s%.6s\n", "--------------------------------------------------------", "------");

    /* compile */
    test_compile_basic();
    test_compile_bad_pattern();
    test_compile_null_args();
    test_compile_pattern_introspection();
    test_compile_group_count();
    test_compile_flags_caseless();

    /* ref-counting */
    test_retain_free();
    test_free_null();

    /* context */
    test_ctx_create_free();
    test_ctx_free_null();
    test_ctx_null_arg();

    /* exec */
    test_exec_basic_match();
    test_exec_no_match();
    test_exec_offset();
    test_exec_capture_groups();
    test_exec_null_args();
    test_exec_offset_beyond_len();
    test_exec_empty_subject();
    test_exec_binary_subject();

    /* match */
    test_match_convenience();
    test_match_null_subject();

    /* is_match */
    test_is_match_true();
    test_is_match_false();
    test_is_match_null_args();

    /* iter */
    test_iter_all_matches();
    test_iter_no_matches();
    test_iter_zero_width_match();
    test_iter_null_args();
    test_iter_free_null();
    test_iter_count_words();

    /* sub */
    test_sub_basic();
    test_sub_with_backref();
    test_sub_no_match();
    test_sub_buffer_too_small();
    test_sub_null_args();

    /* gsub */
    test_gsub_basic();
    test_gsub_digit_redaction();
    test_gsub_no_match();
    test_gsub_named_backref();

    /* flags */
    test_flag_caseless();
    test_flag_multiline();
    test_flag_dotall();
    test_flag_utf();

    /* introspection */
    test_group_count_no_groups();
    test_group_count_null();
    test_pattern_null();
    test_strerror();

    /* edge / regression */
    test_exec_reuse_ctx();
    test_iter_captures_within_iter();
    test_gsub_empty_replacement();
    test_sub_anchored_pattern();

    /* Complex real-world example: parsing HTTP headers with named capture groups. */
    test_exec_http_headers();

    /* Summary */
    printf("%.63s\n", "----------------------------------------------------------------");
    printf("results: %d passed, %d failed, %d total\n", g_passed, g_failed, g_total);

    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
