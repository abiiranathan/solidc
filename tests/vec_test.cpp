#define VEC_IMPL
#include "../vec.h"

#include <gtest/gtest.h>

class VecTest : public ::testing::Test {
   protected:
    virtual void SetUp() { v = vec_new(10, true, int_cmp); }
    virtual void TearDown() { vec_free(v); }

    vec_t* v;
};

// vec_push
TEST_F(VecTest, Push) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    ASSERT_EQ(vec_size(v), 4U);
    ASSERT_EQ(*(int*)vec_get(v, 0), 10);
    ASSERT_EQ(*(int*)vec_get(v, 1), 20);
    ASSERT_EQ(*(int*)vec_get(v, 2), 30);
    ASSERT_EQ(*(int*)vec_get(v, 3), 40);
}

// vec_pop
TEST_F(VecTest, Pop) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    vec_pop(v);

    ASSERT_EQ(vec_size(v), 3U);

    vec_pop(v);
    vec_pop(v);

    ASSERT_EQ(vec_size(v), 1U);

    vec_pop(v);

    ASSERT_EQ(vec_size(v), 0U);
}

// vec_get
TEST_F(VecTest, Get) {
    int *a, *b;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;

    vec_push(v, a);
    vec_push(v, b);

    ASSERT_EQ(*(int*)vec_get(v, 0), 10);
    ASSERT_EQ(*(int*)vec_get(v, 1), 20);
}

// vec_set
TEST_F(VecTest, Set) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);

    vec_set(v, 0, c);
    vec_set(v, 1, d);

    ASSERT_EQ(*(int*)vec_get(v, 0), 30);
    ASSERT_EQ(*(int*)vec_get(v, 1), 40);
}

// vec_reserve
TEST_F(VecTest, Reserve) {
    vec_reserve(v, 100);
    ASSERT_EQ(vec_capacity(v), 100U);
}

// vec_shrink
TEST_F(VecTest, Shrink) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    vec_shrink(v);

    ASSERT_EQ(vec_capacity(v), 4U);
}

// vec_clear
TEST_F(VecTest, Clear) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    vec_clear(v);

    ASSERT_EQ(vec_size(v), 0U);
}

// vec_swap
TEST_F(VecTest, Swap) {
    vec_t* v2 = vec_new(10, true, int_cmp);

    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 50;
    *b = 60;
    *c = 70;
    *d = 80;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);

    vec_push(v2, c);
    vec_push(v2, d);

    vec_swap(&v, &v2);

    ASSERT_EQ(vec_size(v), 2U);
    ASSERT_EQ(vec_size(v2), 3U);

    ASSERT_EQ(*(int*)vec_get(v, 0), 70);
    ASSERT_EQ(*(int*)vec_get(v, 1), 80);

    ASSERT_EQ(*(int*)vec_get(v2, 0), 50);
    ASSERT_EQ(*(int*)vec_get(v2, 1), 60);
    ASSERT_EQ(*(int*)vec_get(v2, 2), 70);

    // since we swapped v with v2, we need to free v
    // vec_free(v);
}

// vec_copy
TEST_F(VecTest, Copy) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    vec_t* v2 = vec_copy(v);

    ASSERT_EQ(vec_size(v), 4U);
    ASSERT_EQ(vec_size(v2), 4U);

    ASSERT_EQ(*(int*)vec_get(v, 0), 10);
    ASSERT_EQ(*(int*)vec_get(v, 1), 20);
    ASSERT_EQ(*(int*)vec_get(v, 2), 30);
    ASSERT_EQ(*(int*)vec_get(v, 3), 40);

    ASSERT_EQ(*(int*)vec_get(v2, 0), 10);
    ASSERT_EQ(*(int*)vec_get(v2, 1), 20);
    ASSERT_EQ(*(int*)vec_get(v2, 2), 30);
    ASSERT_EQ(*(int*)vec_get(v2, 3), 40);

    // vec_free(v2);
}

// vec_contains
TEST_F(VecTest, Contains) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    int e = 40;

    ASSERT_TRUE(vec_contains(v, a));
    ASSERT_TRUE(vec_contains(v, b));
    ASSERT_TRUE(vec_contains(v, c));
    ASSERT_TRUE(vec_contains(v, d));

    // Contains uses the cmp function to compare elements
    ASSERT_TRUE(vec_contains(v, &e));
}

// vec_find_index
TEST_F(VecTest, FindIndex) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);

    ASSERT_EQ(vec_find_index(v, a), 0);
    ASSERT_EQ(vec_find_index(v, b), 1);
    ASSERT_EQ(vec_find_index(v, c), 2);
    ASSERT_EQ(vec_find_index(v, d), -1);
}

// vec_foreach
TEST_F(VecTest, Foreach) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    int sum = 0;
    vec_foreach(v, i) {
        sum += *(int*)vec_get(v, i);
    }

    ASSERT_EQ(sum, 100);
}

// vec_foreach_ptr
TEST_F(VecTest, ForeachPtr) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    int sum = 0;
    vec_foreach_ptr(v, i, elem) {
        sum += *(int*)elem;
    }
    ASSERT_EQ(sum, 100);
}

// vec_reverse
TEST_F(VecTest, Reverse) {
    int *a, *b, *c, *d;

    a = (int*)malloc(sizeof(int));
    b = (int*)malloc(sizeof(int));
    c = (int*)malloc(sizeof(int));
    d = (int*)malloc(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;
    *d = 40;

    vec_push(v, a);
    vec_push(v, b);
    vec_push(v, c);
    vec_push(v, d);

    vec_reverse(v);

    ASSERT_EQ(*(int*)vec_get(v, 0), 40);
    ASSERT_EQ(*(int*)vec_get(v, 1), 30);
    ASSERT_EQ(*(int*)vec_get(v, 2), 20);
    ASSERT_EQ(*(int*)vec_get(v, 3), 10);
}

// Test vec_new with heap_allocated = true
TEST_F(VecTest, NewHeapAllocated) {
    vec_t* v = vec_new(10, true, str_cmp);

    ASSERT_EQ(vec_capacity(v), 10U);
    ASSERT_EQ(vec_size(v), 0U);

    vec_push(v, strdup("hello"));
    vec_push(v, strdup("world"));
    vec_push(v, strdup("foo"));
    vec_push(v, strdup("bar"));
    vec_push(v, strdup("baz"));

    ASSERT_EQ(vec_size(v), 5U);
    ASSERT_EQ(vec_capacity(v), 10U);

    vec_free(v);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}