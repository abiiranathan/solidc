
#include "../include/str_slice.h"

#include <math.h>  // fabs
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>  // EXIT_FAILURE

// ─── Minimal test harness ────────────────────────────────────────────────────

static int _pass = 0;
static int _fail = 0;
static const char* _suite = "";

static void suite(const char* name) {
    _suite = name;
    printf("\n  %s\n", name);
}

static void _check(bool ok, const char* expr, const char* file, int line) {
    if (ok) {
        ++_pass;
        printf("    \033[32m✓\033[0m  %s\n", expr);
    } else {
        ++_fail;
        printf("    \033[31m✗\033[0m  %s  (%s:%d)\n", expr, file, line);
    }
}

#define CHECK(expr)           _check((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b)        _check((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NEQ(a, b)       _check((a) != (b), #a " != " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) _check(fabs((double)(a) - (double)(b)) <= (eps), #a " ≈ " #b, __FILE__, __LINE__)

// ─── Construction ─────────────────────────────────────────────────────────────

static void test_construction(void) {
    suite("construction");

    // SS_LIT — no runtime strlen, works with embedded NUL
    StrSlice lit = SS_LIT("hello");
    CHECK_EQ(lit.len, 5u);
    CHECK(lit.data != NULL);

    // embedded NUL: sizeof("a\0b") - 1 == 3
    StrSlice nul = SS_LIT("a\0b");
    CHECK_EQ(nul.len, 3u);

    // ss_from — explicit length
    const char buf[] = {'x', 'y', 'z'};
    StrSlice from = ss_from(buf, 3);
    CHECK_EQ(from.len, 3u);
    CHECK_EQ(from.data[1], 'y');

    // ss_from_cstr — measures with strlen
    StrSlice cstr = ss_from_cstr("world");
    CHECK_EQ(cstr.len, 5u);

    // ss_from_cstr with NULL — returns empty, does not crash
    StrSlice null_s = ss_from_cstr(NULL);
    CHECK(ss_is_empty(null_s));

    // ss_empty
    StrSlice empty = ss_empty();
    CHECK(ss_is_empty(empty));
    CHECK_EQ(empty.data, NULL);
}

// ─── Validity ─────────────────────────────────────────────────────────────────

static void test_validity(void) {
    suite("validity");

    CHECK(ss_is_valid(SS_LIT("ok")));
    CHECK(ss_is_valid(ss_empty()));  // len=0, NULL data is valid
    CHECK(ss_is_empty(ss_empty()));
    CHECK(!ss_is_empty(SS_LIT("x")));

    // non-null data with len=0 is also valid (e.g. end-of-buffer pointer)
    static const char buf[] = "abc";
    StrSlice zero_len = ss_from(buf, 0);
    CHECK(ss_is_valid(zero_len));
    CHECK(ss_is_empty(zero_len));

    // non-null pointer with positive length — valid
    StrSlice ok = ss_from(buf, 3);
    CHECK(ss_is_valid(ok));
}

// ─── Sub-slicing ──────────────────────────────────────────────────────────────

static void test_subslicing(void) {
    suite("sub-slicing");

    StrSlice s = SS_LIT("hello world");

    // ss_slice — happy path
    StrSliceErr err = SS_OK;
    StrSlice mid = ss_slice(s, 6, 5, &err);
    CHECK_EQ(err, SS_OK);
    CHECK(ss_equal(mid, SS_LIT("world")));

    // ss_slice — start at 0, length == total
    StrSlice all = ss_slice(s, 0, s.len, &err);
    CHECK_EQ(err, SS_OK);
    CHECK(ss_equal(all, s));

    // ss_slice — zero-length sub-slice (valid)
    StrSlice none = ss_slice(s, 3, 0, &err);
    CHECK_EQ(err, SS_OK);
    CHECK(ss_is_empty(none));

    // ss_slice — out of bounds
    StrSlice oob = ss_slice(s, 8, 10, &err);
    CHECK_EQ(err, SS_BOUNDS);
    CHECK(ss_is_empty(oob));

    // ss_skip
    CHECK(ss_equal(ss_skip(s, 6), SS_LIT("world")));
    CHECK(ss_is_empty(ss_skip(s, s.len)));  // skip exactly all
    CHECK(ss_is_empty(ss_skip(s, 999)));    // skip past end

    // ss_take
    CHECK(ss_equal(ss_take(s, 5), SS_LIT("hello")));
    CHECK(ss_equal(ss_take(s, 0), ss_empty()));
    CHECK(ss_equal(ss_take(s, 999), s));  // clamp to len
}

// ─── Comparison ───────────────────────────────────────────────────────────────

static void test_comparison(void) {
    suite("comparison");

    CHECK(ss_equal(SS_LIT("abc"), SS_LIT("abc")));
    CHECK(!ss_equal(SS_LIT("abc"), SS_LIT("ABC")));
    CHECK(!ss_equal(SS_LIT("abc"), SS_LIT("ab")));
    CHECK(ss_equal(ss_empty(), ss_empty()));

    // same pointer, different length — not equal
    StrSlice a = SS_LIT("abcdef");
    CHECK(!ss_equal(ss_take(a, 3), ss_take(a, 4)));

    // case-insensitive
    CHECK(ss_equal_nocase(SS_LIT("Hello"), SS_LIT("hElLO")));
    CHECK(!ss_equal_nocase(SS_LIT("hi"), SS_LIT("bye")));
    CHECK(ss_equal_nocase(SS_LIT("ABC123"), SS_LIT("abc123")));

    // starts_with / ends_with
    StrSlice url = SS_LIT("https://example.com");
    CHECK(ss_starts_with(url, SS_LIT("https")));
    CHECK(!ss_starts_with(url, SS_LIT("http:")));
    CHECK(ss_ends_with(url, SS_LIT(".com")));
    CHECK(!ss_ends_with(url, SS_LIT(".org")));

    // prefix longer than string — never matches
    CHECK(!ss_starts_with(SS_LIT("hi"), SS_LIT("hello")));
    CHECK(!ss_ends_with(SS_LIT("hi"), SS_LIT("hello")));
}

// ─── Search ───────────────────────────────────────────────────────────────────

static void test_search(void) {
    suite("search");

    StrSlice s = SS_LIT("the cat sat on the mat");

    CHECK_EQ(ss_find(s, SS_LIT("cat")), 4u);
    CHECK_EQ(ss_find(s, SS_LIT("the")), 0u);  // first occurrence
    CHECK_EQ(ss_find(s, SS_LIT("mat")), 19u);
    CHECK_EQ(ss_find(s, SS_LIT("dog")), (size_t)-1);

    // empty needle — always found at position 0
    CHECK_EQ(ss_find(s, SS_LIT("")), 0u);

    // needle longer than haystack
    CHECK_EQ(ss_find(SS_LIT("hi"), SS_LIT("hello world")), (size_t)-1);

    CHECK(ss_contains(s, SS_LIT("sat")));
    CHECK(!ss_contains(s, SS_LIT("bat")));

    // ss_split_on — happy path
    StrSlice key, val;
    StrSliceErr err = ss_split_on(SS_LIT("Content-Type: text/html"), SS_LIT(": "), &key, &val);
    CHECK_EQ(err, SS_OK);
    CHECK(ss_equal(key, SS_LIT("Content-Type")));
    CHECK(ss_equal(val, SS_LIT("text/html")));

    // separator at the very start → empty head
    err = ss_split_on(SS_LIT(":rest"), SS_LIT(":"), &key, &val);
    CHECK_EQ(err, SS_OK);
    CHECK(ss_is_empty(key));
    CHECK(ss_equal(val, SS_LIT("rest")));

    // separator at the very end → empty tail
    err = ss_split_on(SS_LIT("key:"), SS_LIT(":"), &key, &val);
    CHECK_EQ(err, SS_OK);
    CHECK(ss_equal(key, SS_LIT("key")));
    CHECK(ss_is_empty(val));

    // separator absent
    err = ss_split_on(SS_LIT("nodivider"), SS_LIT(":"), &key, &val);
    CHECK_EQ(err, SS_NOT_FOUND);
}

// ─── Trimming ─────────────────────────────────────────────────────────────────

static void test_trimming(void) {
    suite("trimming");

    CHECK(ss_equal(ss_trim(SS_LIT("  hello  ")), SS_LIT("hello")));
    CHECK(ss_equal(ss_trim(SS_LIT("\t\n hi \r")), SS_LIT("hi")));
    CHECK(ss_equal(ss_trim(SS_LIT("no-spaces")), SS_LIT("no-spaces")));

    // all whitespace → empty
    CHECK(ss_is_empty(ss_trim(SS_LIT("   \t\n"))));

    // already empty → still empty
    CHECK(ss_is_empty(ss_trim(ss_empty())));
}

// ─── Safe byte access ─────────────────────────────────────────────────────────

static void test_access(void) {
    suite("safe byte access");

    StrSlice s = SS_LIT("abc");
    char c = 0;

    CHECK(ss_get(s, 0, &c) && c == 'a');
    CHECK(ss_get(s, 2, &c) && c == 'c');
    CHECK(!ss_get(s, 3, &c));  // out of bounds
    CHECK(!ss_get(s, 99, &c));
}

// ─── ss_to_int ────────────────────────────────────────────────────────────────

static void test_to_int(void) {
    suite("ss_to_int");

    int v = 0;

    // basic positive
    CHECK_EQ(ss_to_int(SS_LIT("42"), &v), SS_OK);
    CHECK_EQ(v, 42);
    CHECK_EQ(ss_to_int(SS_LIT("+7"), &v), SS_OK);
    CHECK_EQ(v, 7);

    // negative
    CHECK_EQ(ss_to_int(SS_LIT("-1"), &v), SS_OK);
    CHECK_EQ(v, -1);
    CHECK_EQ(ss_to_int(SS_LIT("-999"), &v), SS_OK);
    CHECK_EQ(v, -999);

    // zero
    CHECK_EQ(ss_to_int(SS_LIT("0"), &v), SS_OK);
    CHECK_EQ(v, 0);
    CHECK_EQ(ss_to_int(SS_LIT("-0"), &v), SS_OK);
    CHECK_EQ(v, 0);

    // stops at first non-digit
    CHECK_EQ(ss_to_int(SS_LIT("12px"), &v), SS_OK);
    CHECK_EQ(v, 12);

    // INT_MAX and INT_MIN (2's complement, 32-bit assumed via limits.h)
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", INT_MAX);
    CHECK_EQ(ss_to_int(ss_from_cstr(buf), &v), SS_OK);
    CHECK_EQ(v, INT_MAX);

    snprintf(buf, sizeof(buf), "%d", INT_MIN);
    CHECK_EQ(ss_to_int(ss_from_cstr(buf), &v), SS_OK);
    CHECK_EQ(v, INT_MIN);

    // overflow
    CHECK_EQ(ss_to_int(SS_LIT("99999999999999999999"), &v), SS_OVERFLOW);

    // no digits
    CHECK_EQ(ss_to_int(SS_LIT(""), &v), SS_NOT_FOUND);
    CHECK_EQ(ss_to_int(SS_LIT("-"), &v), SS_NOT_FOUND);
    CHECK_EQ(ss_to_int(SS_LIT("abc"), &v), SS_NOT_FOUND);

    // NULL out-pointer
    CHECK_EQ(ss_to_int(SS_LIT("1"), NULL), SS_NULL);
}

// ─── ss_to_double ─────────────────────────────────────────────────────────────

static void test_to_double(void) {
    suite("ss_to_double");

    double v = 0.0;

    // integers
    CHECK_EQ(ss_to_double(SS_LIT("0"), &v), SS_OK);
    CHECK_NEAR(v, 0.0, 1e-12);
    CHECK_EQ(ss_to_double(SS_LIT("1"), &v), SS_OK);
    CHECK_NEAR(v, 1.0, 1e-12);
    CHECK_EQ(ss_to_double(SS_LIT("100"), &v), SS_OK);
    CHECK_NEAR(v, 100.0, 1e-12);

    // sign
    CHECK_EQ(ss_to_double(SS_LIT("-3.5"), &v), SS_OK);
    CHECK_NEAR(v, -3.5, 1e-12);
    CHECK_EQ(ss_to_double(SS_LIT("+2.0"), &v), SS_OK);
    CHECK_NEAR(v, 2.0, 1e-12);

    // fractional
    CHECK_EQ(ss_to_double(SS_LIT("3.14"), &v), SS_OK);
    CHECK_NEAR(v, 3.14, 1e-10);
    CHECK_EQ(ss_to_double(SS_LIT("0.1"), &v), SS_OK);
    CHECK_NEAR(v, 0.1, 1e-12);
    CHECK_EQ(ss_to_double(SS_LIT(".5"), &v), SS_OK);
    CHECK_NEAR(v, 0.5, 1e-12);

    // exponent forms
    CHECK_EQ(ss_to_double(SS_LIT("1e3"), &v), SS_OK);
    CHECK_NEAR(v, 1000.0, 1e-9);
    CHECK_EQ(ss_to_double(SS_LIT("1.5e2"), &v), SS_OK);
    CHECK_NEAR(v, 150.0, 1e-9);
    CHECK_EQ(ss_to_double(SS_LIT("2E-3"), &v), SS_OK);
    CHECK_NEAR(v, 0.002, 1e-12);
    CHECK_EQ(ss_to_double(SS_LIT("1e+2"), &v), SS_OK);
    CHECK_NEAR(v, 100.0, 1e-9);
    CHECK_EQ(ss_to_double(SS_LIT("-4e2"), &v), SS_OK);
    CHECK_NEAR(v, -400.0, 1e-9);

    // stops at non-numeric character
    CHECK_EQ(ss_to_double(SS_LIT("9.81m/s"), &v), SS_OK);
    CHECK_NEAR(v, 9.81, 1e-10);

    // malformed exponent — bare 'e' with no digits
    CHECK_EQ(ss_to_double(SS_LIT("1e"), &v), SS_INVALID);
    CHECK_EQ(ss_to_double(SS_LIT("1e+"), &v), SS_INVALID);

    // nothing parseable
    CHECK_EQ(ss_to_double(SS_LIT(""), &v), SS_NOT_FOUND);
    CHECK_EQ(ss_to_double(SS_LIT("abc"), &v), SS_NOT_FOUND);
    CHECK_EQ(ss_to_double(SS_LIT("-"), &v), SS_NOT_FOUND);

    // NULL out-pointer
    CHECK_EQ(ss_to_double(SS_LIT("1.0"), NULL), SS_NULL);
}

// ─── ss_to_bool ───────────────────────────────────────────────────────────────

static void test_to_bool(void) {
    suite("ss_to_bool");

    bool b = false;

    // true forms
    CHECK_EQ(ss_to_bool(SS_LIT("true"), &b), SS_OK);
    CHECK(b == true);
    CHECK_EQ(ss_to_bool(SS_LIT("TRUE"), &b), SS_OK);
    CHECK(b == true);
    CHECK_EQ(ss_to_bool(SS_LIT("True"), &b), SS_OK);
    CHECK(b == true);
    CHECK_EQ(ss_to_bool(SS_LIT("yes"), &b), SS_OK);
    CHECK(b == true);
    CHECK_EQ(ss_to_bool(SS_LIT("YES"), &b), SS_OK);
    CHECK(b == true);
    CHECK_EQ(ss_to_bool(SS_LIT("on"), &b), SS_OK);
    CHECK(b == true);
    CHECK_EQ(ss_to_bool(SS_LIT("ON"), &b), SS_OK);
    CHECK(b == true);
    CHECK_EQ(ss_to_bool(SS_LIT("1"), &b), SS_OK);
    CHECK(b == true);

    // false forms
    CHECK_EQ(ss_to_bool(SS_LIT("false"), &b), SS_OK);
    CHECK(b == false);
    CHECK_EQ(ss_to_bool(SS_LIT("FALSE"), &b), SS_OK);
    CHECK(b == false);
    CHECK_EQ(ss_to_bool(SS_LIT("no"), &b), SS_OK);
    CHECK(b == false);
    CHECK_EQ(ss_to_bool(SS_LIT("NO"), &b), SS_OK);
    CHECK(b == false);
    CHECK_EQ(ss_to_bool(SS_LIT("off"), &b), SS_OK);
    CHECK(b == false);
    CHECK_EQ(ss_to_bool(SS_LIT("OFF"), &b), SS_OK);
    CHECK(b == false);
    CHECK_EQ(ss_to_bool(SS_LIT("0"), &b), SS_OK);
    CHECK(b == false);

    // invalid — not silently swallowed
    CHECK_EQ(ss_to_bool(SS_LIT(""), &b), SS_INVALID);
    CHECK_EQ(ss_to_bool(SS_LIT("maybe"), &b), SS_INVALID);
    CHECK_EQ(ss_to_bool(SS_LIT("2"), &b), SS_INVALID);
    CHECK_EQ(ss_to_bool(SS_LIT("tru"), &b), SS_INVALID);

    // NULL out-pointer
    CHECK_EQ(ss_to_bool(SS_LIT("true"), NULL), SS_NULL);
}

// ─── Integration — realistic parse scenario ───────────────────────────────────

static void test_integration(void) {
    suite("integration — HTTP-style header parsing");

    // Parse a multiline header block by splitting on "\r\n" repeatedly.
    StrSlice headers = SS_LIT(
        "Content-Length: 348\r\n"
        "X-Retry: true\r\n"
        "X-Rate: 1.5\r\n");

    int content_length = -1;
    bool retry = false;
    double rate = 0.0;

    StrSlice line, rest = headers;
    while (ss_split_on(rest, SS_LIT("\r\n"), &line, &rest) == SS_OK) {
        if (ss_is_empty(line)) continue;

        StrSlice k, v;
        if (ss_split_on(line, SS_LIT(": "), &k, &v) != SS_OK) continue;

        if (ss_equal(k, SS_LIT("Content-Length")))
            ss_to_int(v, &content_length);
        else if (ss_equal(k, SS_LIT("X-Retry")))
            ss_to_bool(v, &retry);
        else if (ss_equal(k, SS_LIT("X-Rate")))
            ss_to_double(v, &rate);
    }

    CHECK_EQ(content_length, 348);
    CHECK(retry == true);
    CHECK_NEAR(rate, 1.5, 1e-12);
}

void test_to_owned_cstr(void) {
    suite("ss_to_owned_cstr");

    char* s = ss_to_owned_cstr(SS_LIT("hello"));
    CHECK(s != NULL);
    CHECK(strcmp(s, "hello") == 0);
    free(s);

    // embedded NUL
    s = ss_to_owned_cstr(SS_LIT("a\0b"));
    CHECK(s != NULL);
    CHECK(strcmp(s, "a\0b") == 0);
    free(s);

    // empty slice with NULL data — should return empty string, not NULL
    s = ss_to_owned_cstr(ss_empty());
    CHECK(s != NULL);
    CHECK(strcmp(s, "") == 0);
    free(s);

    // invalid slice — non-null pointer with positive length
    StrSlice invalid = {.data = (char*)0x1234, .len = 5};
    s = ss_to_owned_cstr(invalid);
    CHECK(s == NULL);
}

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(void) {
    printf("str_slice test suite\n");
    printf("════════════════════\n");

    test_construction();
    test_validity();
    test_subslicing();
    test_comparison();
    test_search();
    test_trimming();
    test_access();
    test_to_int();
    test_to_double();
    test_to_bool();
    test_integration();

    printf("\n════════════════════\n");
    printf("  \033[32m%d passed\033[0m", _pass);
    if (_fail > 0) printf("  \033[31m%d FAILED\033[0m", _fail);
    printf("\n\n");

    return _fail > 0 ? EXIT_FAILURE : 0;
}
