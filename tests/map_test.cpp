#include <gtest/gtest.h>

#define MAP_IMPL
#include "../map.h"

class MapTest : public ::testing::Test {
   protected:
    virtual void SetUp() {
        m = map_create(3, [](const void* a, const void* b) -> bool {
            return strcmp((const char*)a, (const char*)b) == 0;
        });
    }

    virtual void TearDown() { map_destroy(m); }

    map* m;
};

TEST_F(MapTest, SetAndGet) {
    map_set(m, "name", "John");
    map_set(m, "age", "30");
    map_set(m, "city", "New York");

    ASSERT_STREQ((char*)map_get(m, "name"), "John");
    ASSERT_STREQ((char*)map_get(m, "age"), "30");
    ASSERT_STREQ((char*)map_get(m, "city"), "New York");
}

TEST_F(MapTest, Remove) {
    map_set(m, "name", "John");
    map_remove(m, "name");

    ASSERT_EQ(map_get(m, "name"), nullptr);
}

TEST_F(MapTest, Length) {
    map_set(m, "name", "John");
    map_set(m, "age", "30");
    map_set(m, "city", "New York");

    ASSERT_EQ(map_length(m), 3);
}

TEST_F(MapTest, GetKeys) {
    map_set(m, "name", "John");
    map_set(m, "age", "30");
    map_set(m, "city", "New York");

    size_t num_keys;
    char** keys = (char**)map_getkeys(m, &num_keys);

    ASSERT_EQ(num_keys, 3U);
    // Order is not guaranteed
    // So we need to check all the keys
    bool name = false, age = false, city = false;
    for (size_t i = 0; i < num_keys; i++) {
        if (strcmp(keys[i], "name") == 0) {
            name = true;
        } else if (strcmp(keys[i], "age") == 0) {
            age = true;
        } else if (strcmp(keys[i], "city") == 0) {
            city = true;
        } else {
            ASSERT_TRUE(false);
        }
    }

    ASSERT_TRUE(name);
    ASSERT_TRUE(age);
    ASSERT_TRUE(city);

    map_free_keys((void**)keys);
}

TEST_F(MapTest, Iterator) {
    map_set(m, "name", "John");
    map_set(m, "age", "30");
    map_set(m, "city", "New York");

    map_iterator* iter = map_iterator_create(m);
    // not null
    ASSERT_TRUE(iter != NULL);

    // Order is not guaranteed
    // So we need to check all the keys
    bool name = false, age = false, city = false;
    while (map_iterator_has_next(iter)) {
        entry e = map_iterator_next(iter);
        if (strcmp((char*)e.key, "name") == 0) {
            ASSERT_STREQ((char*)e.value, "John");
            name = true;
        } else if (strcmp((char*)e.key, "age") == 0) {
            ASSERT_STREQ((char*)e.value, "30");
            age = true;
        } else if (strcmp((char*)e.key, "city") == 0) {
            ASSERT_STREQ((char*)e.value, "New York");
            city = true;
        } else {
            ASSERT_TRUE(false);
        }
    }

    ASSERT_TRUE(name);
    ASSERT_TRUE(age);
    ASSERT_TRUE(city);

    map_iterator_destroy(iter);
}

// test map set_from_array
TEST_F(MapTest, SetFromArray) {
    const void* keys[]   = {"name", "age", "city"};
    const void* values[] = {"John", "30", "New York"};
    map_set_from_array(m, keys, values, 3);

    ASSERT_STREQ((char*)map_get(m, "name"), "John");
    ASSERT_STREQ((char*)map_get(m, "age"), "30");
    ASSERT_STREQ((char*)map_get(m, "city"), "New York");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
