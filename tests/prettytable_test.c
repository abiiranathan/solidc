/**
 * @file example.c
 * @brief Example usage of prettytable library
 */

#include "../include/prettytable.h"

#include <stdio.h>
#include <string.h>

// Example 1: Simple 2D string array
typedef struct {
    const char*** data;  // 2D array of strings
    const char** headers;
} SimpleTable;

static const char* simple_get_header(void* user_data, int col) {
    SimpleTable* table = (SimpleTable*)user_data;
    return table->headers[col];
}

static const char* simple_get_cell(void* user_data, int row, int col) {
    SimpleTable* table = (SimpleTable*)user_data;
    return table->data[row][col];
}

void example_simple_table(void) {
    printf("Example 1: Simple Table\n\n");

    const char* headers[] = {"ID", "Name", "Age", "City"};
    const char* row0[] = {"1", "Alice", "30", "New York"};
    const char* row1[] = {"2", "Bob", "25", "Los Angeles"};
    const char* row2[] = {"3", "Charlie", "35", "Chicago"};
    const char** data[] = {row0, row1, row2};

    SimpleTable table = {.data = data, .headers = headers};

    prettytable_config cfg;
    prettytable_config_init(&cfg);
    cfg.num_rows = 3;
    cfg.num_cols = 4;
    cfg.get_header = simple_get_header;
    cfg.get_cell = simple_get_cell;
    cfg.user_data = &table;

    prettytable_print(&cfg);
}

// Example 2: Struct array with dynamic formatting
typedef struct {
    int id;
    const char* name;
    double salary;
} Employee;

typedef struct {
    Employee* employees;
    int count;
    char buffers[3][32];  // Separate buffer for each formatted column
} EmployeeTable;

static const char* employee_get_header(void* user_data, int col) {
    (void)user_data;
    const char* headers[] = {"ID", "Name", "Salary"};
    return headers[col];
}

static const char* employee_get_cell(void* user_data, int row, int col) {
    EmployeeTable* table = (EmployeeTable*)user_data;
    Employee* emp = &table->employees[row];

    switch (col) {
        case 0:
            snprintf(table->buffers[0], sizeof(table->buffers[0]), "%d", emp->id);
            return table->buffers[0];
        case 1:
            return emp->name;
        case 2:
            snprintf(table->buffers[2], sizeof(table->buffers[2]), "$%.2f", emp->salary);
            return table->buffers[2];
        default:
            return "";
    }
}

void example_struct_table(void) {
    printf("\n\nExample 2: Struct Table with Different Styles\n\n");

    Employee employees[] = {
      {101, "Alice Johnson", 75000.50}, {102, "Bob Smith", 82000.00}, {103, "Charlie Brown", 68000.75}};

    EmployeeTable table = {.employees = employees, .count = 3};

    prettytable_config cfg;
    prettytable_config_init(&cfg);
    cfg.num_rows = 3;
    cfg.num_cols = 3;
    cfg.get_header = employee_get_header;
    cfg.get_cell = employee_get_cell;
    cfg.user_data = &table;

    printf("Box style (default):\n");
    prettytable_print(&cfg);

    printf("\n\nASCII style:\n");
    cfg.style = &PRETTYTABLE_STYLE_ASCII;
    prettytable_print(&cfg);

    printf("\n\nDouble-line style:\n");
    cfg.style = &PRETTYTABLE_STYLE_DOUBLE;
    prettytable_print(&cfg);

    printf("\n\nMinimal style:\n");
    cfg.style = &PRETTYTABLE_STYLE_MINIMAL;
    prettytable_print(&cfg);
}

// Example 3: CSV-like data
static const char* csv_get_header(void* user_data, int col) {
    (void)user_data;
    const char* headers[] = {"Product", "Price", "Stock", "Category"};
    return headers[col];
}

static const char* csv_get_cell(void* user_data, int row, int col) {
    (void)user_data;
    // Simulating CSV data
    static const char* csv_data[][4] = {{"Laptop", "$999.99", "15", "Electronics"},
                                        {"Mouse", "$24.99", "150", "Accessories"},
                                        {"Keyboard", "$79.99", "85", "Accessories"},
                                        {"Monitor", "$299.99", "42", "Electronics"},
                                        {"USB Cable", "$9.99", "200", "Accessories"}};
    return csv_data[row][col];
}

void example_csv_table(void) {
    printf("\n\nExample 3: CSV-like Data\n\n");

    prettytable_config cfg;
    prettytable_config_init(&cfg);
    cfg.num_rows = 5;
    cfg.num_cols = 4;
    cfg.get_header = csv_get_header;
    cfg.get_cell = csv_get_cell;

    prettytable_print(&cfg);
}

// Example 4: No headers
static const char* no_header_get_cell(void* user_data, int row, int col) {
    (void)user_data;
    static const char* data[][3] = {
      {"Apple", "Red", "Sweet"}, {"Banana", "Yellow", "Sweet"}, {"Lemon", "Yellow", "Sour"}};
    return data[row][col];
}

void example_no_header_table(void) {
    printf("\n\nExample 4: Table Without Headers\n\n");

    prettytable_config cfg;
    prettytable_config_init(&cfg);
    cfg.num_rows = 3;
    cfg.num_cols = 3;
    cfg.get_cell = no_header_get_cell;
    cfg.show_header = false;

    prettytable_print(&cfg);
}

int main(void) {
    example_simple_table();
    example_struct_table();
    example_csv_table();
    example_no_header_table();

    return 0;
}
