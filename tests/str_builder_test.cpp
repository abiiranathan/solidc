#define STR_BUILDER_IMPL
#include "../str_builder.h"

#include <gtest/gtest.h>

TEST(StrBuilderTest, Append) {
    string_builder* sb;
    sb = sb_alloc(1024);
    ASSERT_NE(nullptr, sb);

    // Append a long lorem ipsum string
    ASSERT_TRUE(sb_append(sb, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "));
    ASSERT_TRUE(
        sb_append(sb, "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "));
    ASSERT_TRUE(sb_append(sb,
                          "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi "
                          "ut aliquip ex ea commodo consequat. "));
    ASSERT_TRUE(sb_append(sb, "Duis aute irure dolor in reprehenderit in voluptate velit esse. "));

    // Append a single character
    ASSERT_TRUE(sb_append_char(sb, 'A'));

    // Append an integer
    ASSERT_TRUE(sb_append_int(sb, 123));

    // Append a float
    ASSERT_TRUE(sb_append_float(sb, 123.456f));

    // Append a double
    ASSERT_TRUE(sb_append_double(sb, 123.456));

    sb_free(sb);
}

TEST(StrBuilderTest, Clear) {
    string_builder* sb;
    sb = sb_alloc(144);
    ASSERT_NE(nullptr, sb);

    // Append a long lorem ipsum string
    ASSERT_TRUE(sb_append(sb, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "));

    // Clear the string
    sb_clear(sb);
    ASSERT_EQ(0, sb_len(sb));

    sb_free(sb);
}

// test that sb_grow works when called repeatedly
TEST(StrBuilderTest, Grow) {
    string_builder* sb;
    sb = sb_alloc(1);
    ASSERT_NE(nullptr, sb);

    // Append a long lorem ipsum string
    ASSERT_TRUE(sb_append(sb, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "));

    // Append a single character
    ASSERT_TRUE(sb_append_char(sb, 'A'));

    // Append an integer
    ASSERT_TRUE(sb_append_int(sb, 123));

    // Append a float
    ASSERT_TRUE(sb_append_float(sb, 123.456f));

    // Append a double
    ASSERT_TRUE(sb_append_double(sb, 123.456));

    // Test str equals
    ASSERT_STREQ(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. A123123.456001123.456000",
        sb_cstr(sb));

    sb_free(sb);
}

TEST(StrBuilderTest, GrowSmall) {
    string_builder* sb;
    sb = sb_alloc(2048);
    ASSERT_NE(nullptr, sb);

    // Append shakespear sonnets
    ASSERT_TRUE(sb_append(sb, "Shall I compare thee to a summer's day? "));
    ASSERT_TRUE(sb_append(sb, "Thou art more lovely and more temperate: "));
    ASSERT_TRUE(sb_append(sb, "Rough winds do shake the darling buds of May, "));
    ASSERT_TRUE(sb_append(sb, "And summer's lease hath all too short a date: "));
    ASSERT_TRUE(sb_append(sb, "Sometime too hot the eye of heaven shines, "));
    ASSERT_TRUE(sb_append(sb, "And often is his gold complexion dimmed; "));
    ASSERT_TRUE(sb_append(sb, "And every fair from fair sometime declines, "));
    ASSERT_TRUE(sb_append(sb, "By chance or nature's changing course untrimmed; "));
    ASSERT_TRUE(sb_append(sb, "But thy eternal summer shall not fade, "));
    ASSERT_TRUE(sb_append(sb, "Nor lose possession of that fair thou owest; "));
    ASSERT_TRUE(sb_append(sb, "Nor shall Death brag thou wanderest in his shade, "));
    ASSERT_TRUE(sb_append(sb, "When in eternal lines to time thou growest: "));

    // Test str equals
    ASSERT_STREQ(
        "Shall I compare thee to a summer's day? Thou art more lovely and more temperate: Rough "
        "winds do shake the darling buds of May, And summer's lease hath all too short a date: "
        "Sometime too hot the eye of heaven shines, And often is his gold complexion dimmed; And "
        "every fair from fair sometime declines, By chance or nature's changing course untrimmed; "
        "But thy eternal summer shall not fade, Nor lose possession of that fair thou owest; Nor "
        "shall Death brag thou wanderest in his shade, When in eternal lines to time thou "
        "growest: ",
        sb_cstr(sb));
    sb_free(sb);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}