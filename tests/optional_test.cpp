#include "../optional.h"
#include <gtest/gtest.h>

ResultFloat divide(float a, float b) {
    if (b == 0) {
        return ERR_FLOAT("Division by zero");
    }
    return OK_FLOAT(a / b);
}

ResultInt chk_add(int a, int b) {
    int res = a + b;
    // int overflow
    if (res < a || res < b) {
        return ERR_INT("Integer overflow");
    }

    return OK_INT(res);
}

TEST(OptionalTest, TestError) {
    auto res = divide(10, 0);

    ASSERT_FALSE(res.is_ok);
}

TEST(OptionalTest, TestOk) {
    auto res = divide(10, 2);

    ASSERT_TRUE(res.is_ok);
    ASSERT_EQ(res.value, 5.0f);
}

TEST(OptionalTest, TestErrorInt) {
    auto res = chk_add(2147483647, 1);

    ASSERT_FALSE(res.is_ok);

    auto err = UNWRAP_ERR(res);
    ASSERT_EQ(err, "Integer overflow");
}

TEST(OptionalTest, TestOkInt) {
    auto res = chk_add(1, 2);

    ASSERT_TRUE(res.is_ok);
    ASSERT_EQ(res.value, 3);

    auto val = UNWRAP(res);
    ASSERT_EQ(val, 3);

    UNWRAP_OK(res, [](float v) { ASSERT_EQ(v, 3); });
};

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}