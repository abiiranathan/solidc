#include "../set.h"

#include <gtest/gtest.h>

SET_DEFINE(int)

class SetTest : public ::testing::Test {
   protected:
    virtual void SetUp() { set = set_create_int(10); }

    virtual void TearDown() { set_destroy_int(set); }
    Set_int* set;
};

TEST_F(SetTest, Create) {
    ASSERT_TRUE(set != nullptr);
    EXPECT_EQ(set_size_int(set), 0U);
    EXPECT_EQ(set_capacity_int(set), 10U);
    ASSERT_TRUE(set_isempty_int(set));
}

TEST_F(SetTest, Add) {
    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);
    EXPECT_EQ(set_size_int(set), 10);
    EXPECT_EQ(set_contains_int(set, 1), true);
    EXPECT_EQ(set_contains_int(set, 2), true);
    EXPECT_EQ(set_contains_int(set, 3), true);
    EXPECT_EQ(set_contains_int(set, 4), true);
    EXPECT_EQ(set_contains_int(set, 5), true);
    EXPECT_EQ(set_contains_int(set, 6), true);
    EXPECT_EQ(set_contains_int(set, 7), true);
    EXPECT_EQ(set_contains_int(set, 8), true);
    EXPECT_EQ(set_contains_int(set, 9), true);
    EXPECT_EQ(set_contains_int(set, 10), true);
}

TEST_F(SetTest, Remove) {
    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);
    set_remove_int(set, 1);
    set_remove_int(set, 2);
    set_remove_int(set, 3);
    set_remove_int(set, 4);
    set_remove_int(set, 5);
    set_remove_int(set, 6);
    set_remove_int(set, 7);
    set_remove_int(set, 8);
    set_remove_int(set, 9);
    set_remove_int(set, 10);
    EXPECT_EQ(set_size_int(set), 0);
    EXPECT_EQ(set_contains_int(set, 1), false);
    EXPECT_EQ(set_contains_int(set, 2), false);
    EXPECT_EQ(set_contains_int(set, 3), false);
    EXPECT_EQ(set_contains_int(set, 4), false);
    EXPECT_EQ(set_contains_int(set, 5), false);
    EXPECT_EQ(set_contains_int(set, 6), false);
    EXPECT_EQ(set_contains_int(set, 7), false);
    EXPECT_EQ(set_contains_int(set, 8), false);
    EXPECT_EQ(set_contains_int(set, 9), false);
    EXPECT_EQ(set_contains_int(set, 10), false);
}

TEST_F(SetTest, Clear) {
    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);
    set_clear_int(set);
    EXPECT_EQ(set_size_int(set), 0);
    EXPECT_EQ(set_contains_int(set, 1), false);
    EXPECT_EQ(set_contains_int(set, 2), false);
    EXPECT_EQ(set_contains_int(set, 3), false);
    EXPECT_EQ(set_contains_int(set, 4), false);
    EXPECT_EQ(set_contains_int(set, 5), false);
    EXPECT_EQ(set_contains_int(set, 6), false);
    EXPECT_EQ(set_contains_int(set, 7), false);
    EXPECT_EQ(set_contains_int(set, 8), false);
    EXPECT_EQ(set_contains_int(set, 9), false);
    EXPECT_EQ(set_contains_int(set, 10), false);
}

// set_intersection
TEST_F(SetTest, Intersection) {
    Set_int* set2 = set_create_int(10);
    ASSERT_TRUE(set2 != NULL);

    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);

    set_add_int(set2, 1);
    set_add_int(set2, 2);
    set_add_int(set2, 3);
    set_add_int(set2, 4);
    set_add_int(set2, 5);
    set_add_int(set2, 6);
    set_add_int(set2, 7);
    set_add_int(set2, 8);
    set_add_int(set2, 9);
    set_add_int(set2, 10);

    Set_int* set3 = set_intersection_int(set, set2);

    EXPECT_EQ(set_size_int(set3), 10);

    EXPECT_EQ(set_contains_int(set3, 1), true);
    EXPECT_EQ(set_contains_int(set3, 2), true);
    EXPECT_EQ(set_contains_int(set3, 3), true);
    EXPECT_EQ(set_contains_int(set3, 4), true);
    EXPECT_EQ(set_contains_int(set3, 5), true);
    EXPECT_EQ(set_contains_int(set3, 6), true);
    EXPECT_EQ(set_contains_int(set3, 7), true);
    EXPECT_EQ(set_contains_int(set3, 8), true);
    EXPECT_EQ(set_contains_int(set3, 9), true);
    EXPECT_EQ(set_contains_int(set3, 10), true);

    set_destroy_int(set2);
    set_destroy_int(set3);
}

// set_union
TEST_F(SetTest, Union) {
    Set_int* set2 = set_create_int(10);
    ASSERT_TRUE(set2 != NULL);

    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);

    set_add_int(set2, 11);
    set_add_int(set2, 12);
    set_add_int(set2, 13);
    set_add_int(set2, 14);
    set_add_int(set2, 15);
    set_add_int(set2, 16);
    set_add_int(set2, 17);
    set_add_int(set2, 18);
    set_add_int(set2, 19);
    set_add_int(set2, 20);

    Set_int* set3 = set_union_int(set, set2);

    EXPECT_EQ(set_size_int(set3), 20);

    EXPECT_EQ(set_contains_int(set3, 1), true);
    EXPECT_EQ(set_contains_int(set3, 2), true);
    EXPECT_EQ(set_contains_int(set3, 3), true);
    EXPECT_EQ(set_contains_int(set3, 4), true);
    EXPECT_EQ(set_contains_int(set3, 5), true);
    EXPECT_EQ(set_contains_int(set3, 6), true);
    EXPECT_EQ(set_contains_int(set3, 7), true);
    EXPECT_EQ(set_contains_int(set3, 8), true);
    EXPECT_EQ(set_contains_int(set3, 9), true);
    EXPECT_EQ(set_contains_int(set3, 10), true);
    EXPECT_EQ(set_contains_int(set3, 11), true);
    EXPECT_EQ(set_contains_int(set3, 12), true);
    EXPECT_EQ(set_contains_int(set3, 13), true);
    EXPECT_EQ(set_contains_int(set3, 14), true);
    EXPECT_EQ(set_contains_int(set3, 15), true);
    EXPECT_EQ(set_contains_int(set3, 16), true);
    EXPECT_EQ(set_contains_int(set3, 17), true);
    EXPECT_EQ(set_contains_int(set3, 18), true);
    EXPECT_EQ(set_contains_int(set3, 19), true);
    EXPECT_EQ(set_contains_int(set3, 20), true);

    set_destroy_int(set2);
    set_destroy_int(set3);
}

// set_difference
TEST_F(SetTest, Difference) {
    Set_int* set2 = set_create_int(10);
    ASSERT_TRUE(set2 != NULL);

    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);

    set_add_int(set2, 1);
    set_add_int(set2, 3);
    set_add_int(set2, 5);
    set_add_int(set2, 7);
    set_add_int(set2, 9);

    Set_int* set3 = set_difference_int(set, set2);

    EXPECT_EQ(set_size_int(set3), 5);

    EXPECT_EQ(set_contains_int(set3, 2), true);
    EXPECT_EQ(set_contains_int(set3, 4), true);
    EXPECT_EQ(set_contains_int(set3, 6), true);
    EXPECT_EQ(set_contains_int(set3, 8), true);
    EXPECT_EQ(set_contains_int(set3, 10), true);

    set_destroy_int(set2);
    set_destroy_int(set3);
}

// set_is_subset
TEST_F(SetTest, IsSubset) {
    Set_int* set2 = set_create_int(10);
    ASSERT_TRUE(set2 != NULL);

    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);

    set_add_int(set2, 1);
    set_add_int(set2, 2);
    set_add_int(set2, 3);
    set_add_int(set2, 4);
    set_add_int(set2, 5);
    set_add_int(set2, 6);
    set_add_int(set2, 7);
    set_add_int(set2, 8);
    set_add_int(set2, 9);
    set_add_int(set2, 10);

    EXPECT_EQ(set_isSubset_int(set, set2), true);

    set_destroy_int(set2);
}

/* set_symmetric_difference
Computes the symmetric difference of two sets A and B,
and returns a new set containing elements that are in A or B but not in both.
*/

TEST_F(SetTest, SymmetricDifference) {
    Set_int* set2 = set_create_int(10);
    ASSERT_TRUE(set2 != NULL);

    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);
    set_add_int(set, 4);
    set_add_int(set, 5);
    set_add_int(set, 6);
    set_add_int(set, 7);
    set_add_int(set, 8);
    set_add_int(set, 9);
    set_add_int(set, 10);

    set_add_int(set2, 1);
    set_add_int(set2, 3);
    set_add_int(set2, 5);
    set_add_int(set2, 7);
    set_add_int(set2, 9);
    set_add_int(set2, 11);
    set_add_int(set2, 13);
    set_add_int(set2, 15);
    set_add_int(set2, 17);
    set_add_int(set2, 19);

    Set_int* set3 = set_symmetric_difference_int(set, set2);

    // 5 even numbers in A plus
    // 5 odd numbers in B above 10
    EXPECT_EQ(set_size_int(set3), 10);

    EXPECT_EQ(set_contains_int(set3, 2), true);
    EXPECT_EQ(set_contains_int(set3, 4), true);
    EXPECT_EQ(set_contains_int(set3, 6), true);
    EXPECT_EQ(set_contains_int(set3, 8), true);
    EXPECT_EQ(set_contains_int(set3, 10), true);
    EXPECT_EQ(set_contains_int(set3, 11), true);
    EXPECT_EQ(set_contains_int(set3, 13), true);
    EXPECT_EQ(set_contains_int(set3, 15), true);
    EXPECT_EQ(set_contains_int(set3, 17), true);
    EXPECT_EQ(set_contains_int(set3, 19), true);

    set_destroy_int(set2);
    set_destroy_int(set3);
}

/* Determines if setA is a proper subset of setB, and
returns true if all elements in setA are
 also in setB but setA and setB are not equal. */
TEST_F(SetTest, IsProperSubset) {
    Set_int* set2 = set_create_int(10);
    ASSERT_TRUE(set2 != NULL);

    set_add_int(set, 1);
    set_add_int(set, 2);
    set_add_int(set, 3);

    set_add_int(set2, 1);
    set_add_int(set2, 2);
    set_add_int(set2, 3);

    EXPECT_EQ(set_isProperSubset_int(set, set2), false);  // because setA and setB are equal

    set_add_int(set2, 4);

    EXPECT_EQ(set_isProperSubset_int(set, set2), true);

    set_destroy_int(set2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}