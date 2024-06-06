#include "../include/safe_map.h"
#include <assert.h>
#include <time.h>

// Utility macro for logging
#define LOG_TEST_RESULT(test_name, condition)                                                      \
    do {                                                                                           \
        if (condition) {                                                                           \
            printf("[PASS] %s\n", test_name);                                                      \
        } else {                                                                                   \
            printf("[FAIL] %s\n", test_name);                                                      \
        }                                                                                          \
    } while (0)

// Test function prototypes
void test_safe_map();
void test_int_map();
void test_string_map();
void test_intmap_under_load();

// Creates a type: int_map
DEFINE_MAP(int_map, int, int)
typedef char* string;

// Creates a type: string_map
DEFINE_MAP(string_map, string, string)

int main() {
    test_safe_map();

    printf("All tests completed.\n");
    return 0;
}

void test_safe_map() {
    test_int_map();
    test_string_map();
    test_intmap_under_load();
}

#define BUCKET_COUNT 10
void test_int_map() {
    int_map* map = int_map_create(BUCKET_COUNT);
    assert(map != NULL);

    int_map_insert(map, 1, 10);
    int_map_insert(map, 2, 20);
    int_map_insert(map, 3, 30);

    int* value;
    LOG_TEST_RESULT("int_map_get", (value = int_map_get(map, 1)) != NULL);
    LOG_TEST_RESULT("*value=10", *value == 10);

    LOG_TEST_RESULT("int_map_get", (value = int_map_get(map, 2)) != NULL);
    LOG_TEST_RESULT("*value=20", *value == 20);

    LOG_TEST_RESULT("int_map_get", (value = int_map_get(map, 3)) != NULL);
    LOG_TEST_RESULT("*value=30", *value == 30);

    int_map_remove(map, 1);

    LOG_TEST_RESULT("int_map_get", (value = int_map_get(map, 1)) == NULL);

    int_map_clear(map);

    LOG_TEST_RESULT("int_map_get", (value = int_map_get(map, 2)) == NULL);
    LOG_TEST_RESULT("int_map_get", (value = int_map_get(map, 3)) == NULL);

    int_map_destroy(map);
}

void test_string_map() {
    string_map* map = string_map_create(BUCKET_COUNT);
    assert(map != NULL);

    string_map_insert(map, "one", "1");
    string_map_insert(map, "two", "2");
    string_map_insert(map, "three", "3");

    string* value;
    LOG_TEST_RESULT("string_map_get", (value = string_map_get(map, "one")) != NULL);
    LOG_TEST_RESULT("strcmp(value, \"1\")", strcmp(*value, "1") == 0);

    LOG_TEST_RESULT("string_map_get", (value = string_map_get(map, "two")) != NULL);
    LOG_TEST_RESULT("strcmp(value, \"2\")", strcmp(*value, "2") == 0);

    LOG_TEST_RESULT("string_map_get", (value = string_map_get(map, "three")) != NULL);
    LOG_TEST_RESULT("strcmp(value, \"3\")", strcmp(*value, "3") == 0);

    string_map_remove(map, "one");

    LOG_TEST_RESULT("string_map_get", (value = string_map_get(map, "one")) == NULL);

    string_map_clear(map);

    LOG_TEST_RESULT("string_map_get", (value = string_map_get(map, "two")) == NULL);
    LOG_TEST_RESULT("string_map_get", (value = string_map_get(map, "three")) == NULL);

    string_map_destroy(map);
}

void test_intmap_under_load(void) {
    // Int map load test with 10 million items
    int item_count = 10000000;
    time_t start = time(NULL);
    int_map* map2 = int_map_create(5000);
    assert(map2 != NULL);

    for (int i = 0; i < item_count; i++) {
        int_map_insert(map2, i, i * 10);
    }

    int* value;
    for (int i = 0; i < item_count; i++) {
        assert((value = int_map_get(map2, i)) != NULL);
        assert(*value == i * 10);
    }

    int_map_destroy(map2);
    time_t end = time(NULL);
    printf("Int map load test with %d items took %ld seconds.\n", item_count, end - start);
}