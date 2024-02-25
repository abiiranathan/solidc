#define VEC_IMPL
#include "../vec.h"

#include <gtest/gtest.h>

class VecTest : public ::testing::Test {
   protected:
    virtual void SetUp() { v = vec_new(sizeof(int), 4); }

    virtual void TearDown() { vec_free(v); }

    vec_t* v;
};

// vec_push
TEST_F(VecTest, Push) {
    int a = 10;
    vec_push(v, &a);
    ASSERT_EQ(vec_size(v), 1U);
    ASSERT_EQ(*(int*)vec_get(v, 0), 10);
}

// vec_pop
TEST_F(VecTest, Pop) {
    int a = 10;
    vec_push(v, &a);
    vec_pop(v);
    ASSERT_EQ(vec_size(v), 0U);
}

// vec_get
TEST_F(VecTest, Get) {
    int a = 10;
    vec_push(v, &a);
    ASSERT_EQ(*(int*)vec_get(v, 0), 10);
}

// vec_set
TEST_F(VecTest, Set) {
    int a = 10;
    vec_push(v, &a);
    int b = 20;
    vec_set(v, 0, &b);
    ASSERT_EQ(*(int*)vec_get(v, 0), 20);
}

// vec_size
TEST_F(VecTest, Size) {
    int i;

    for (i = 0; i < 10; i++) {
        vec_push(v, &i);
    }

    ASSERT_EQ(vec_size(v), 10U);
}

// vec_capacity
TEST_F(VecTest, Capacity) {
    ASSERT_EQ(vec_capacity(v), 4U);
}

// vec_reserve
TEST_F(VecTest, Reserve) {
    vec_reserve(v, 100);
    ASSERT_EQ(vec_capacity(v), 100U);
}

// vec_shrink
TEST_F(VecTest, Shrink) {
    vec_reserve(v, 100);
    int i = 100;
    vec_push(v, &i);
    vec_shrink(v);
    ASSERT_EQ(vec_capacity(v), 1U);
    ASSERT_EQ(vec_size(v), 1U);
}

// vec_clear
TEST_F(VecTest, Clear) {
    int i;

    for (i = 0; i < 10; i++) {
        vec_push(v, &i);
    }

    vec_clear(v);
    ASSERT_EQ(vec_size(v), 0U);
}

// vec_swap
TEST_F(VecTest, Swap) {
    vec_t* v2 = vec_new(sizeof(int), 4);
    int i;

    for (i = 0; i < 10; i++) {
        vec_push(v, &i);
    }

    vec_swap(&v, &v2);

    ASSERT_EQ(vec_size(v), 0U);
    ASSERT_EQ(vec_size(v2), 10U);
    ASSERT_EQ(*(int*)vec_get(v2, 0), 0);
    ASSERT_EQ(*(int*)vec_get(v2, 9), 9);

    vec_free(v2);
}

// vec_copy
TEST_F(VecTest, Copy) {
    int a, b, c;
    a = 10;
    b = 20;
    c = 30;
    vec_push(v, &a);
    vec_push(v, &b);
    vec_push(v, &c);

    vec_t* v2 = vec_copy(v);

    ASSERT_EQ(vec_size(v2), 3U);
    ASSERT_EQ(*(int*)vec_get(v2, 0), a);
    ASSERT_EQ(*(int*)vec_get(v2, 1), b);
    ASSERT_EQ(*(int*)vec_get(v2, 2), c);

    ASSERT_EQ(vec_size(v2), 3U);
    ASSERT_EQ(vec_capacity(v2), 4U);

    vec_free(v2);
}

// vec_contains
TEST_F(VecTest, Contains) {
    int a, b, c, d;
    a = 10;
    b = 20;
    c = 30;
    d = 40;

    vec_push(v, &a);
    vec_push(v, &b);
    vec_push(v, &c);

    ASSERT_TRUE(vec_contains(v, &a));
    ASSERT_TRUE(vec_contains(v, &b));
    ASSERT_TRUE(vec_contains(v, &c));
    ASSERT_FALSE(vec_contains(v, &d));
}

// vec_find_index
TEST_F(VecTest, FindIndex) {
    int a, b, c, d;
    a = 10;
    b = 20;
    c = 30;
    d = 40;

    vec_push(v, &a);
    vec_push(v, &b);
    vec_push(v, &c);

    ASSERT_EQ(vec_find_index(v, &a), 0);
    ASSERT_EQ(vec_find_index(v, &b), 1);
    ASSERT_EQ(vec_find_index(v, &c), 2);
    ASSERT_EQ(vec_find_index(v, &d), -1);
}

// vec_foreach
TEST_F(VecTest, Foreach) {
    int i;

    for (i = 0; i < 10; i++) {
        vec_push(v, &i);
    }

    int sum = 0;
    vec_foreach(v, i) {
        sum += *(int*)vec_get(v, i);
    }
    ASSERT_EQ(sum, 45);
}

// vec_foreach_ptr
TEST_F(VecTest, ForeachPtr) {
    int i;

    for (i = 0; i < 10; i++) {
        vec_push(v, &i);
    }

    int sum = 0;
    vec_foreach_ptr(v, i, elem) {
        sum += *(int*)elem;
    }
    ASSERT_EQ(sum, 45);
}

// vec_reverse
TEST_F(VecTest, Reverse) {
    int i;

    for (i = 0; i < 10; i++) {
        vec_push(v, &i);
    }

    vec_reverse(v);

    for (int j = 0; j < 10; j++) {
        ASSERT_EQ(*(int*)vec_get(v, j), 9 - j);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}