// Testing the singly linked list implementation
#define SLIST_IMPL
#include "../slist.h"

#include <gtest/gtest.h>

// Test the singly linked list implementation
class SListTest : public ::testing::Test {
   protected:
    // Create a singly linked list
    virtual void SetUp() { list = slist_new(sizeof(int)); }

    // Free the singly linked list
    virtual void TearDown() { slist_free(list); }
    slist_t* list;
};

// slist_push
TEST_F(SListTest, Push) {
    int a = 10;
    slist_push(list, &a);
    ASSERT_EQ(slist_size(list), 1U);
    ASSERT_EQ(*(int*)slist_get(list, 0), a);
}

// slist_pop
TEST_F(SListTest, Pop) {
    int a = 10, b = 20;
    slist_push(list, &a);
    slist_push(list, &b);
    slist_pop(list);
    ASSERT_EQ(slist_size(list), 1U);
    ASSERT_EQ(*(int*)slist_get(list, 0), a);
}

// slist_get
TEST_F(SListTest, Get) {
    int a = 10;
    slist_push(list, &a);
    ASSERT_EQ(*(int*)slist_get(list, 0), a);
}

// slist_insert
TEST_F(SListTest, Insert) {
    int a = 10, b = 20, c = 30, d = 40, e = 50;
    slist_push(list, &a);
    slist_push(list, &b);
    slist_push(list, &c);

    slist_insert(list, 1, &d);
    slist_insert(list, 4, &d);
    slist_insert(list, 2, &e);

    ASSERT_EQ(slist_size(list), 6U);

    // Items are inserted in reverse order
    ASSERT_EQ(*(int*)slist_get(list, 0), c);
    ASSERT_EQ(*(int*)slist_get(list, 1), d);
    ASSERT_EQ(*(int*)slist_get(list, 2), e);
    ASSERT_EQ(*(int*)slist_get(list, 3), b);
    ASSERT_EQ(*(int*)slist_get(list, 4), a);
    ASSERT_EQ(*(int*)slist_get(list, 5), d);
}

// slist_insert_after
TEST_F(SListTest, InsertAfter) {
    int a = 10, b = 20, c = 30, d = 40;
    slist_push(list, &a);
    slist_push(list, &b);
    slist_push(list, &c);

    slist_insert_after(list, &d, &b);
    ASSERT_EQ(slist_size(list), 4U);

    ASSERT_EQ(*(int*)slist_get(list, 0), c);
    ASSERT_EQ(*(int*)slist_get(list, 1), b);
    ASSERT_EQ(*(int*)slist_get(list, 2), d);
    ASSERT_EQ(*(int*)slist_get(list, 3), a);
}

// slist_insert_before
TEST_F(SListTest, InsertBefore) {
    int a = 10, b = 20, c = 30, d = 40;
    slist_push(list, &a);
    slist_push(list, &b);
    slist_push(list, &c);
    slist_print_asint(list);

    slist_insert_before(list, &d, &b);
    ASSERT_EQ(slist_size(list), 4U);

    slist_print_asint(list);

    ASSERT_EQ(*(int*)slist_get(list, 0), d);
    ASSERT_EQ(*(int*)slist_get(list, 1), c);
    ASSERT_EQ(*(int*)slist_get(list, 2), b);
    ASSERT_EQ(*(int*)slist_get(list, 3), a);

    slist_insert_before(list, &d, &a);
    ASSERT_EQ(slist_size(list), 5U);

    slist_print_asint(list);

    ASSERT_EQ(*(int*)slist_get(list, 0), d);  // previous d is now at index 0
    ASSERT_EQ(*(int*)slist_get(list, 1), c);  // c is at index 1
    ASSERT_EQ(*(int*)slist_get(list, 2), d);  // new d is at index 2(3-1)
    ASSERT_EQ(*(int*)slist_get(list, 3), b);
    ASSERT_EQ(*(int*)slist_get(list, 4), a);
}

// slist_remove
TEST_F(SListTest, Remove) {
    int a = 10, b = 20, c = 30, d = 40;
    slist_push(list, &a);
    slist_push(list, &b);
    slist_push(list, &c);
    slist_push(list, &d);

    slist_remove(list, 1);
    slist_remove(list, 2);

    ASSERT_EQ(slist_size(list), 2U);
    ASSERT_EQ(*(int*)slist_get(list, 0), d);
    ASSERT_EQ(*(int*)slist_get(list, 1), b);
}

// slist_index_of
TEST_F(SListTest, IndexOf) {
    int a = 10, b = 20, c = 30, d = 40;
    slist_push(list, &a);
    slist_push(list, &b);
    slist_push(list, &c);
    slist_push(list, &d);

    ASSERT_EQ(slist_index_of(list, &a), 3U);
    ASSERT_EQ(slist_index_of(list, &b), 2U);
    ASSERT_EQ(slist_index_of(list, &c), 1U);
    ASSERT_EQ(slist_index_of(list, &d), 0U);
    int e = 50;
    ASSERT_EQ(slist_index_of(list, &e), -1);
}

// slist_clear
TEST_F(SListTest, Clear) {
    int a = 10;
    slist_push(list, &a);
    slist_clear(list);
    ASSERT_EQ(slist_size(list), 0U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}