#include "../include/csvparser.h"
#include "../include/filepath.h"

#include <stdio.h>   // for printf, fprintf, stderr
#include <stdlib.h>  // for abort, exit
#include <string.h>  // for strcmp, strlen

/** Test framework macros for better assertion handling. */
#define ASSERT(condition, message, ...)                                                                      \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            fprintf(stderr, "ASSERTION FAILED at %s:%d in %s(): " message "\n", __FILE__, __LINE__,          \
                    __func__, ##__VA_ARGS__);                                                                \
            abort();                                                                                         \
        }                                                                                                    \
    } while (0)

#define ASSERT_EQ(expected, actual, message, ...)                                                            \
    do {                                                                                                     \
        if ((expected) != (actual)) {                                                                        \
            fprintf(stderr, "ASSERTION FAILED at %s:%d in %s(): Expected %ld, got %ld. " message "\n",       \
                    __FILE__, __LINE__, __func__, (long)(expected), (long)(actual), ##__VA_ARGS__);          \
            abort();                                                                                         \
        }                                                                                                    \
    } while (0)

#define ASSERT_STR_EQ(expected, actual, message, ...)                                                        \
    do {                                                                                                     \
        if (strcmp((expected), (actual)) != 0) {                                                             \
            fprintf(stderr, "ASSERTION FAILED at %s:%d in %s(): Expected \"%s\", got \"%s\". " message "\n", \
                    __FILE__, __LINE__, __func__, (expected), (actual), ##__VA_ARGS__);                      \
            abort();                                                                                         \
        }                                                                                                    \
    } while (0)

#define ASSERT_NOT_NULL(ptr, message, ...) ASSERT((ptr) != nullptr, message, ##__VA_ARGS__)

#define ASSERT_NULL(ptr, message, ...) ASSERT((ptr) == nullptr, message, ##__VA_ARGS__)

/** Test case counter for progress reporting. */
static size_t test_count  = 0;
static size_t test_passed = 0;

/** Marks the start of a test case. */
#define TEST_START(test_name)                                                                                \
    do {                                                                                                     \
        test_count++;                                                                                        \
        printf("Running test %zu: %s... ", test_count, (test_name));                                         \
        fflush(stdout);                                                                                      \
    } while (0)

/** Marks successful completion of a test case. */
#define TEST_PASS()                                                                                          \
    do {                                                                                                     \
        test_passed++;                                                                                       \
        printf("PASSED\n");                                                                                  \
    } while (0)

/**
 * Compares two CSV rows for equality.
 * @param expected Expected row data.
 * @param actual Actual row data from parser.
 * @param row_index Row index for error reporting.
 * @return true if rows match, false otherwise.
 */
static bool compare_csv_rows(const Row* expected, const Row* actual, size_t row_index) {
    ASSERT_NOT_NULL(expected, "Expected row cannot be null");
    ASSERT_NOT_NULL(actual, "Actual row cannot be null");

    ASSERT_EQ(expected->count, actual->count, "Field count mismatch in row %zu", row_index);

    for (size_t i = 0; i < expected->count; i++) {
        ASSERT_NOT_NULL(expected->fields[i], "Expected field %zu in row %zu is null", i, row_index);
        ASSERT_NOT_NULL(actual->fields[i], "Actual field %zu in row %zu is null", i, row_index);

        ASSERT_STR_EQ(expected->fields[i], actual->fields[i], "Field %zu mismatch in row %zu", i, row_index);
    }

    return true;
}

/**
 * Creates a temporary file with the given CSV content.
 * @param csv_data CSV content to write to file.
 * @return Path to temporary file. Caller must free and remove file.
 */
static char* create_temp_csv_file(const char* csv_data) {
    ASSERT_NOT_NULL(csv_data, "CSV data cannot be null");

    char* tmpfile = make_tempfile();
    ASSERT_NOT_NULL(tmpfile, "Failed to create temporary file path");

    FILE* file = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(file, "Failed to open temporary file for writing: %s", tmpfile);

    size_t data_len = strlen(csv_data);
    size_t written  = fwrite(csv_data, 1, data_len, file);
    ASSERT_EQ(data_len, written, "Failed to write complete CSV data to file");

    int close_result = fclose(file);
    ASSERT_EQ(0, close_result, "Failed to close temporary file");

    return tmpfile;
}

/**
 * Runs a comprehensive CSV parser test case.
 * @param test_name Descriptive name for the test.
 * @param csv_data Raw CSV data as string.
 * @param expected_rows Expected parsed row data.
 * @param num_expected_rows Number of expected rows.
 * @param skip_header Whether to skip header row during parsing.
 * @param has_header Whether the CSV data contains a header row.
 */
static void run_csv_reader_test(const char* test_name, const char* csv_data, Row* expected_rows,
                                size_t num_expected_rows, bool skip_header, bool has_header) {
    TEST_START(test_name);

    // Create temporary file with CSV data
    char* tmpfile = create_temp_csv_file(csv_data);

    // Create and configure CSV reader
    CsvReader* reader = csv_reader_new(tmpfile);

    ASSERT_NOT_NULL(reader, "Failed to create CSV reader");

    CsvReaderConfigure(reader, .skip_header = skip_header, .has_header = has_header);

    // Parse CSV data
    Row** rows = csv_reader_parse(reader);
    if (num_expected_rows > 0) {
        ASSERT_NOT_NULL(rows, "CSV parsing returned null");

        // Verify row count
        size_t actual_row_count = csv_reader_numrows(reader);
        ASSERT_EQ(num_expected_rows, actual_row_count, "Row count mismatch: expected %zu, got %zu",
                  num_expected_rows, actual_row_count);

        // Compare each row
        for (size_t i = 0; i < num_expected_rows; i++) {
            compare_csv_rows(&expected_rows[i], rows[i], i);
        }
    } else {
        ASSERT_NULL(rows, "rows should be NULL pointer");
    }

    // Cleanup
    csv_reader_free(reader);
    int remove_result = remove(tmpfile);
    ASSERT_EQ(0, remove_result, "Failed to remove temporary file: %s", tmpfile);

    free(tmpfile);  // Assuming make_tempfile() returns malloc'd memory

    TEST_PASS();
}

/**
 * Tests CSV writer functionality by writing data and then reading it back.
 */
static void test_csv_writer(void) {
    TEST_START("CSV Writer Round-trip");

    const char* test_filename = "test_writer_output.csv";

    // Create writer and write test data
    CsvWriter* writer = csvwriter_new(test_filename);
    ASSERT_NOT_NULL(writer, "Failed to create CSV writer");

    // Write header and data rows
    const char* header_fields[]  = {"name", "age"};
    const char* alice_fields[]   = {"Alice", "25"};
    const char* bob_fields[]     = {"Bob", "30"};
    const char* charlie_fields[] = {"Charlie", "35"};

    bool ok = csvwriter_write_row(writer, header_fields, 2);
    ASSERT(ok, "Failed to write header row");

    ok = csvwriter_write_row(writer, alice_fields, 2);
    ASSERT(ok, "Failed to write Alice row");

    ok = csvwriter_write_row(writer, bob_fields, 2);
    ASSERT(ok, "Failed to write Bob row");

    ok = csvwriter_write_row(writer, charlie_fields, 2);
    ASSERT(ok, "Failed to write Charlie row");

    csvwriter_free(writer);

    // Read back and verify the written data
    CsvReader* reader = csv_reader_new(test_filename);
    ASSERT_NOT_NULL(reader, "Failed to create CSV reader for written file");

    CsvReaderConfigure(reader, .skip_header = false, .has_header = true);

    Row** rows = csv_reader_parse(reader);
    ASSERT_NOT_NULL(rows, "Failed to parse written CSV file");

    // Define expected results (including header since skip_header=false)
    Row expected_rows[] = {
        {.fields = (char*[]){"name", "age"}, .count = 2},
        {.fields = (char*[]){"Alice", "25"}, .count = 2},
        {.fields = (char*[]){"Bob", "30"}, .count = 2},
        {.fields = (char*[]){"Charlie", "35"}, .count = 2},
    };

    size_t actual_row_count = csv_reader_numrows(reader);
    ASSERT_EQ(4, actual_row_count, "Written CSV should have 4 rows");

    // Verify each row
    for (size_t i = 0; i < 4; i++) {
        compare_csv_rows(&expected_rows[i], rows[i], i);
    }

    // Cleanup
    csv_reader_free(reader);
    int remove_result = remove(test_filename);
    ASSERT_EQ(0, remove_result, "Failed to remove test CSV file");

    TEST_PASS();
}

/**
 * Tests edge cases for CSV parsing.
 */
static void test_csv_edge_cases(void) {
    // Test empty CSV
    const char* empty_csv = "";
    Row* no_rows          = nullptr;

    run_csv_reader_test("Empty CSV", empty_csv, no_rows, 0, false, false);

    // Test CSV with only header
    const char* header_only_csv = "name,age\n";
    Row header_row[]            = {{.fields = (char*[]){"name", "age"}, .count = 2}};
    run_csv_reader_test("Header-only CSV (no skip)", header_only_csv, header_row, 1, false, true);

    // Test CSV with quoted fields containing commas
    const char* quoted_csv =
        "name,description\n"
        "Alice,\"Software Engineer, Senior\"\n"
        "Bob,\"Manager, Engineering\"\n";

    Row quoted_expected[] = {
        {.fields = (char*[]){"name", "description"}, .count = 2},
        {.fields = (char*[]){"Alice", "Software Engineer, Senior"}, .count = 2},
        {.fields = (char*[]){"Bob", "Manager, Engineering"}, .count = 2},
    };
    run_csv_reader_test("Quoted fields with commas", quoted_csv, quoted_expected, 3, false, true);

    // Test CSV with empty fields
    const char* empty_fields_csv =
        "name,age,city\n"
        "Alice,,Seattle\n"
        ",30,\n";

    Row empty_fields_expected[] = {
        {.fields = (char*[]){"name", "age", "city"}, .count = 3},
        {.fields = (char*[]){"Alice", "", "Seattle"}, .count = 3},
        {.fields = (char*[]){"", "30", ""}, .count = 3},
    };
    run_csv_reader_test("Empty fields", empty_fields_csv, empty_fields_expected, 3, false, true);
}

/**
 * Prints a summary of test results.
 */
static void print_test_summary(void) {
    printf("TEST SUMMARY\n");
    printf("Total tests: %zu\n", test_count);
    printf("Passed: %zu\n", test_passed);
    printf("Failed: %zu\n", test_count - test_passed);

    if (test_passed == test_count) {
        printf("✅ All tests PASSED!\n");
    } else {
        printf("❌ Some tests FAILED!\n");
    }
}

int main(void) {
    printf("Starting CSV Parser Test Suite\n");
    printf("==============================\n");

    // Basic CSV parsing tests
    const char* basic_csv =
        "name,age\n"
        "Alice,25\n"
        "Bob,30\n"
        "Charlie,35\n";

    // Test including header row
    Row expected_with_header[] = {
        {.fields = (char*[]){"name", "age"}, .count = 2},
        {.fields = (char*[]){"Alice", "25"}, .count = 2},
        {.fields = (char*[]){"Bob", "30"}, .count = 2},
        {.fields = (char*[]){"Charlie", "35"}, .count = 2},
    };
    run_csv_reader_test("Basic CSV with header", basic_csv, expected_with_header, 4, false, true);

    // Test skipping header row
    Row expected_without_header[] = {
        {.fields = (char*[]){"Alice", "25"}, .count = 2},
        {.fields = (char*[]){"Bob", "30"}, .count = 2},
        {.fields = (char*[]){"Charlie", "35"}, .count = 2},
    };
    run_csv_reader_test("Basic CSV skip header", basic_csv, expected_without_header, 3, true, true);

    // Test CSV writer functionality
    test_csv_writer();

    // Test edge cases
    test_csv_edge_cases();

    // Print final results
    print_test_summary();

    // Return appropriate exit code
    return (test_passed == test_count) ? 0 : 1;
}
