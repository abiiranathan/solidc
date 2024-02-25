#define LIST_IMPL
#include "../list.h"

#include <gtest/gtest.h>

class ListTest : public ::testing::Test {
   protected:
    virtual void SetUp() { l = list_new(sizeof(int)); }

    virtual void TearDown() { list_free(l); }

    list_t* l;
};

// list_push_back
TEST_F(ListTest, Push) {
    int a = 10;
    list_push_back(l, &a);
    ASSERT_EQ(list_size(l), 1U);
    ASSERT_EQ(*(int*)list_get(l, 0), 10);
}

// list_push_front
TEST_F(ListTest, PushFront) {
    int a = 10;
    int b = 20;
    list_push_back(l, &a);
    list_push_front(l, &b);
    ASSERT_EQ(list_size(l), 2U);

    ASSERT_EQ(*(int*)list_get(l, 0), 20);
    ASSERT_EQ(*(int*)list_get(l, 1), 10);
}

// list_pop_back
TEST_F(ListTest, Pop) {
    int a = 10;
    list_push_back(l, &a);
    list_pop_back(l);
    ASSERT_EQ(list_size(l), 0U);
}

// list_pop_front
TEST_F(ListTest, PopFront) {
    int a = 10;
    int b = 20;
    list_push_back(l, &a);
    list_push_back(l, &b);
    list_pop_front(l);
    ASSERT_EQ(list_size(l), 1U);
    ASSERT_EQ(*(int*)list_get(l, 0), b);
}

// list_get
TEST_F(ListTest, Get) {
    int a = 10;
    list_push_back(l, &a);
    ASSERT_EQ(*(int*)list_get(l, 0), 10);

    // Test out of bounds
    ASSERT_EQ(list_get(l, 1), nullptr);
}

TEST_F(ListTest, IndexOf) {
    int a = 10;
    int b = 20;
    list_push_back(l, &a);
    list_push_back(l, &b);
    ASSERT_EQ(list_index_of(l, &a), 0U);
    ASSERT_EQ(list_index_of(l, &b), 1U);
    int c = 30;
    ASSERT_EQ(list_index_of(l, &c), -1);
}

TEST_F(ListTest, Insert) {
    int a = 10;
    int b = 20;
    list_push_back(l, &a);
    list_insert(l, 0, &b);
    ASSERT_EQ(list_size(l), 2U);
    ASSERT_EQ(*(int*)list_get(l, 0), b);
    ASSERT_EQ(*(int*)list_get(l, 1), a);
}

TEST_F(ListTest, InsertAfter) {
    int a = 10;
    int b = 20;
    int c = 30;
    list_push_back(l, &a);
    list_push_back(l, &b);
    list_insert_after(l, &c, &a);

    ASSERT_EQ(list_size(l), 3U);
    ASSERT_EQ(*(int*)list_get(l, 0), a);
    ASSERT_EQ(*(int*)list_get(l, 1), c);
    ASSERT_EQ(*(int*)list_get(l, 2), b);

    // Test inserting after the last element
    int d = 40;
    list_insert_after(l, &d, &b);

    ASSERT_EQ(list_size(l), 4U);
    ASSERT_EQ(*(int*)list_get(l, 0), a);
    ASSERT_EQ(*(int*)list_get(l, 1), c);
    ASSERT_EQ(*(int*)list_get(l, 2), d);
    ASSERT_EQ(*(int*)list_get(l, 3), b);

    // Test inserting after an element that doesn't exist
    int e = 50;
    int f = 60;

    list_insert_after(l, &e, &f);
    ASSERT_EQ(list_size(l), 4U);

    // Insert after the first element
    list_insert_after(l, &e, &a);

    ASSERT_EQ(list_size(l), 5U);
    ASSERT_EQ(*(int*)list_get(l, 0), a);
    ASSERT_EQ(*(int*)list_get(l, 1), e);
}

TEST_F(ListTest, InsertBefore) {
    int a = 10;
    int b = 20;
    list_push_back(l, &a);
    list_insert_before(l, &b, &a);
    ASSERT_EQ(list_size(l), 2U);
    ASSERT_EQ(*(int*)list_get(l, 0), b);
    ASSERT_EQ(*(int*)list_get(l, 1), a);
}

TEST_F(ListTest, Clear) {
    int a = 10;
    list_push_back(l, &a);
    list_clear(l);
    ASSERT_EQ(list_size(l), 0U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}