#define ORDERED_MAP_IMPL
#include "../ordered_map.h"

#include <gtest/gtest.h>

TEST(OrderedMapTest, TestCreateInsertGetOrder) {
    ordered_map_t* map;
    size_t keySize  = 25;
    size_t capacity = 10;
    map             = ordered_map_new(keySize, capacity);

    char key1[] = "key1", key2[] = "key2", key3[] = "key3";
    char value1[] = "value1", value2[] = "value2", value3[] = "value3";

    ordered_map_insert(map, key1, value1);
    ordered_map_insert(map, key2, value2);
    ordered_map_insert(map, key3, value3);

    ASSERT_EQ(ordered_map_size(map), 3);
    ASSERT_STREQ((char*)ordered_map_get(map, key1), (char*)value1);
    ASSERT_STREQ((char*)ordered_map_get(map, key2), (char*)value2);
    ASSERT_STREQ((char*)ordered_map_get(map, key3), (char*)value3);

    // Test that the keys are in the correct order
    char* keys[3];
    ordered_map_foreach(map, key, value) {
        (void)value;
        if (strcmp((char*)key, key1) == 0) {
            keys[0] = (char*)key;
        } else if (strcmp((char*)key, key2) == 0) {
            keys[1] = (char*)key;
        } else if (strcmp((char*)key, key3) == 0) {
            keys[2] = (char*)key;
        }
    };

    ASSERT_STREQ(keys[0], key1);
    ASSERT_STREQ(keys[1], key2);
    ASSERT_STREQ(keys[2], key3);

    ordered_map_free(map);
}

TEST(OrderedMapTest, TestRemove) {
    ordered_map_t* map;
    size_t keySize  = 25;
    size_t capacity = 10;
    map             = ordered_map_new(keySize, capacity);

    char key1[] = "key1", key2[] = "key2", key3[] = "key3";
    char value1[] = "value1", value2[] = "value2", value3[] = "value3";

    ordered_map_insert(map, key1, value1);
    ordered_map_insert(map, key2, value2);
    ordered_map_insert(map, key3, value3);

    ordered_map_remove(map, key2);

    ASSERT_EQ(ordered_map_size(map), 2);
    ASSERT_STREQ((char*)ordered_map_get(map, key1), (char*)value1);
    ASSERT_EQ(ordered_map_get(map, key2), nullptr);
    ASSERT_STREQ((char*)ordered_map_get(map, key3), (char*)value3);

    ordered_map_free(map);
}

TEST(OrderedMapTest, TestForeach) {
    ordered_map_t* map;
    size_t keySize  = 25;
    size_t capacity = 10;
    map             = ordered_map_new(keySize, capacity);

    char key1[] = "key1", key2[] = "key2", key3[] = "key3";
    char value1[] = "value1", value2[] = "value2", value3[] = "value3";

    ordered_map_insert(map, key1, value1);
    ordered_map_insert(map, key2, value2);
    ordered_map_insert(map, key3, value3);

    char* keys[3];
    ordered_map_foreach(map, key, value) {
        (void)value;
        if (strcmp((char*)key, key1) == 0) {
            keys[0] = (char*)key;
        } else if (strcmp((char*)key, key2) == 0) {
            keys[1] = (char*)key;
        } else if (strcmp((char*)key, key3) == 0) {
            keys[2] = (char*)key;
        }
    };

    ASSERT_STREQ(keys[0], key1);
    ASSERT_STREQ(keys[1], key2);
    ASSERT_STREQ(keys[2], key3);

    ordered_map_free(map);
}

TEST(OrderedMapTest, TestUpdate) {
    ordered_map_t* map;
    size_t keySize  = 25;
    size_t capacity = 10;

    map = ordered_map_new(keySize, capacity);

    char key1[] = "key1", key2[] = "key2", key3[] = "key3";
    char value1[] = "value1", value2[] = "value2", value3[] = "value3";

    ordered_map_insert(map, key1, value1);
    ordered_map_insert(map, key2, value2);
    ordered_map_insert(map, key3, value3);

    char newValue[] = "newValue";
    ordered_map_insert(map, key2, newValue);

    ASSERT_EQ(ordered_map_size(map), 3);
    ASSERT_STREQ((const char*)ordered_map_get(map, key1), (char*)value1);
    ASSERT_STREQ((const char*)ordered_map_get(map, key2), (char*)newValue);
    ASSERT_STREQ((const char*)ordered_map_get(map, key3), (char*)value3);

    ordered_map_free(map);
}

TEST(OrderedMapTest, TestInsertUpdate) {
    ordered_map_t* map;
    size_t keySize  = 25;
    size_t capacity = 10;
    map             = ordered_map_new(keySize, capacity);

    // We are using std::string to avoid using stack allocated strings
    // that would be deallocated after the function returns
    std::string key1 = "key1", key2 = "key2", key3 = "key3";
    std::string value1 = "value1", value2 = "value2", value3 = "value3";

    ordered_map_insert(map, key1.data(), value1.data());
    ordered_map_insert(map, key2.data(), value2.data());
    ordered_map_insert(map, key3.data(), value3.data());

    std::string newValue = "newValue";
    ordered_map_insert(map, key2.data(), newValue.data());

    ASSERT_EQ(ordered_map_size(map), 3);

    ASSERT_EQ(memcmp(ordered_map_get(map, key1.data()), value1.data(), value1.size()), 0);
    ASSERT_EQ(memcmp(ordered_map_get(map, key2.data()), newValue.data(), newValue.size()), 0);
    ASSERT_EQ(memcmp(ordered_map_get(map, key3.data()), value3.data(), value3.size()), 0);

    std::string key4   = "key4";
    std::string value4 = "value4";
    ordered_map_insert(map, key4.data(), value4.data());

    ASSERT_EQ(ordered_map_size(map), 4);

    printf("key1: %s\n", (char*)ordered_map_get(map, key1.data()));
    printf("key2: %s\n", (char*)ordered_map_get(map, key2.data()));
    printf("key3: %s\n", (char*)ordered_map_get(map, key3.data()));
    printf("key4: %s\n", (char*)ordered_map_get(map, key4.data()));

    // casting to const char* alters the value of the pointer
    ASSERT_EQ(memcmp(ordered_map_get(map, key1.data()), value1.data(), value1.size()), 0);
    ASSERT_EQ(memcmp(ordered_map_get(map, key2.data()), newValue.data(), newValue.size()), 0);
    ASSERT_EQ(memcmp(ordered_map_get(map, key3.data()), value3.data(), value3.size()), 0);
    ASSERT_EQ(memcmp(ordered_map_get(map, key4.data()), value4.data(), value4.size()), 0);

    ordered_map_free(map);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}