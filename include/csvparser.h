/**
 * @file csv_parser.h
 * @brief CSV Parser Library
 *
 * This library provides functions for parsing CSV (Comma-Separated Values) data.
 * It allows you to read and manipulate CSV files with ease.
 */
#ifndef __CSV_PARSER_H__
#define __CSV_PARSER_H__

// If clang, ignore the initializer-overrides warning
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
#endif

// C++ compatibility
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "arena.h"

#include <stdbool.h>
#include <stddef.h>

#ifndef CSV_ARENA_BLOCK_SIZE
#define CSV_ARENA_BLOCK_SIZE 4096
#endif

#ifndef MAX_FIELD_SIZE
// Maximum size of the csv line.
#define MAX_FIELD_SIZE 1024
#endif

/**
 * @brief Opaque structure representing a CSV parser.
 * Create a new CSV parser with csvparser_new and free it with csvparser_free.
 * Use csvparser_parse to parse the CSV data and retrieve all the rows at once.
 * Use csvparser_parse_async to parse the CSV data and pass each processed row back in a callback.
 * Use csvparser_getnumrows to get the number of rows in the CSV data.
 * Use csvparser_setdelim to set the delimiter character for CSV fields.
 * 
 * You can redefine before including header the MAX_FIELD_SIZE macro to change the maximum size of the csv line
 * and the CSV_ARENA_BLOCK_SIZE macro to change the size of the arena block.
 */
typedef struct CsvParser CsvParser;

/**
 * @brief Structure representing a CSV row.
 */
typedef struct CsvRow {
    char** fields;     ///< Array of fields in each row.
    size_t numFields;  ///< Number of fields in each row.
} CsvRow;

// callback to process every row as its parsed.
typedef void (*RowCallback)(size_t rowIndex, CsvRow* row);

/**
 * @brief Create a new CSV parser associated with a filename.
 *
 * This function initializes a new CSV parser and associates it with the given filename.
 *
 * @param filename The filename of the CSV file to parse.
 * @return A pointer to the created CsvParser, or NULL on failure.
 */
CsvParser* csvparser_new(const char* filename);

/**
 * @brief Parse the CSV data and retrieve all the rows at once.
 *
 * This function parses the CSV data and returns all the rows as an array of 
 * CsvRow structure.
 *
 * The parser file descriptor and stream will automatically be closed.
 * Note that this function allocates an array of all items on the heap
 * that you must free with csv_parser_free.
 *
 * @param self A pointer to the CsvParser.
 * @return A pointer to the next CsvRow, or NULL if there are no more rows or an error occurs.
 */
CsvRow** csvparser_parse(CsvParser* self);

/**
 * @brief Parse the CSV data and pass each processed row back in a callback.
 * Return true from the callback to stop early.
 * The parser file descriptor and stream will automatically be closed.
 *
 * If alloc_max is 0, the parser will allocate all rows at once;
 * otherwise it will allocate alloc_max rows.
 * 
 * @param self A pointer to the CsvParser.
 * @param alloc_max The maximum number of rows to allocate at once.
 * @return void.
 */
void csvparser_parse_async(CsvParser* self, RowCallback callback, size_t alloc_max);

/**
 * @brief Get the number of rows in the CSV data.
 *
 * This function returns the total number of rows in the CSV data,
 * excluding empty lines and comments.
 *
 * @param self A pointer to the CsvParser.
 * @return The number of rows.
 */
size_t csvparser_numrows(const CsvParser* self);

/**
 * @brief Free memory used by the CsvParser and CsvRow structures.
 *
 * This function releases the memory used by the CsvParser and any CsvRow structures created with it.
 *
 * @param self A pointer to the CsvParser.
 */
void csvparser_free(CsvParser* self);

struct CsvParserConfig {
    char delim;
    char quote;
    char comment;
    bool has_header;
    bool skip_header;
};

struct CsvWriterConfig {
    char delim;      // Delimiter character
    char quote;      // Quote character
    char newline;    // Newline character
    bool quote_all;  // Quote all fields
    bool flush;      // Flush the stream after writing each row
    bool append;     // Append to the file if it exists, otherwise create a new file
    bool first_row;  // Whether the current row is the first row written to the file
};

typedef struct CsvParserConfig CsvParserConfig;
typedef struct CsvWriterConfig CsvWriterConfig;

void csvparser_setconfig(CsvParser* parser, CsvParserConfig config);

#define CsvParserConfigure(parser, ...)                                                            \
    csvparser_setconfig(parser, (CsvParserConfig){.delim       = ',',                              \
                                                  .quote       = '"',                              \
                                                  .comment     = '#',                              \
                                                  .has_header  = true,                             \
                                                  .skip_header = true,                             \
                                                  __VA_ARGS__})

/*
CSV Writer struct. Usage example:

CsvWriter* writer = csvwriter_new("test.csv");
if (!writer) {
    printf("Error creating CSV writer\n");
    return;
}

csvwriter_write_row(writer, (const char*[]){"name", "age"}, 2);
csvwriter_write_row(writer, (const char*[]){"Alice", "25"}, 2);
csvwriter_write_row(writer, (const char*[]){"Bob", "30"}, 2);
csvwriter_write_row(writer, (const char*[]){"Charlie", "35"}, 2);

csvwriter_free(writer);
*/
typedef struct CsvWriter CsvWriter;

#define CsvWriterConfigure(writer, ...)                                                            \
    csvwriter_setconfig(writer, (CsvWriterConfig){.delim     = ',',                                \
                                                  .quote     = '"',                                \
                                                  .newline   = '\n',                               \
                                                  .quote_all = false,                              \
                                                  .flush     = false,                              \
                                                  .first_row = false,                              \
                                                  __VA_ARGS__})

// Create a new CSV writer associated with a filename.
CsvWriter* csvwriter_new(const char* filename);

// Set the configuration for the CSV writer.
void csvwriter_write_row(CsvWriter* self, const char** fields, size_t numfields);

// Free memory used by the CsvWriter and close the file stream.
void csvwriter_free(CsvWriter* self);

#ifdef __cplusplus
}
#endif

#endif /* __CSV_PARSER_H__ */
