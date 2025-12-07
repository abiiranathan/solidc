#include "../include/csvparser.h"
#include "../include/arena.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CsvReader {
    FILE* stream;      // file_t pointer corresponding to the file stream.
    Row** rows;        // Array of row pointers
    size_t num_rows;   // Number of rows in csv, excluding empty lines
    char delim;        // Delimiter character
    char quote;        // Quote character
    char comment;      // Comment character
    bool has_header;   // Whether the CSV file has a header
    bool skip_header;  // Whether to skip the header when parsing
    Arena* arena;      // single-threaded arena for memory allocation
} CsvReader;

typedef struct csv_line_params {
    Arena* arena;
    const char* line;
    size_t num_fields;
    size_t rowIndex;
    Row* row;
    char delim;
    char quote;
} csv_line_params;

static size_t line_count(CsvReader* reader);
static size_t get_num_fields(const char* line, char delim, char quote);
static bool parse_csv_line(csv_line_params* args);

CsvReader* csv_reader_new(const char* filename, size_t arena_memory) {
    CsvReader* reader = malloc(sizeof(CsvReader));
    if (!reader) {
        fprintf(stderr, "error allocating memory for CsvReader\n");
        return NULL;
    }

    FILE* stream = fopen(filename, "r");
    if (!stream) {
        fprintf(stderr, "error opening file %s\n", filename);
        free(reader);
        return NULL;
    }

    // Use passed in argument if provided or use default value.
    Arena* arena = arena_create((arena_memory ? arena_memory : CSV_ARENA_BLOCK_SIZE));
    if (!arena) {
        fprintf(stderr, "error creating memory arena\n");
        fclose(stream);
        free(reader);
        return NULL;
    }

    reader->arena    = arena;
    reader->num_rows = 0;
    reader->stream   = stream;

    // Set default configuration
    CsvReaderConfigure(reader, .delim = ',');
    reader->rows = NULL;
    return reader;
}

// Allocate memory for rows and set num_rows.
static Row** csv_allocate_rows(Arena* arena, size_t num_rows) {
    if (num_rows == 0) {
        fprintf(stderr, "No rows to allocate. Perhaps the CSV file is empty\n");
        return NULL;
    }

    Row** rows = arena_alloc_array(arena, sizeof(Row*), num_rows);
    if (!rows) {
        fprintf(stderr, "csv_allocate_rows(): error allocating memory for %lu rows in arena\n", num_rows);
        return NULL;
    }

    for (size_t i = 0; i < num_rows; i++) {
        rows[i] = arena_alloc(arena, sizeof(Row));
        if (!rows[i]) {
            fprintf(stderr,
                    "csv_allocate_rows(): error allocating memory for CsvRow: "
                    "%zu\n",
                    i);
            return NULL;
        }
    }
    return rows;
}

Row** csv_reader_parse(CsvReader* reader) {
    // read num_rows and allocate them on heap.
    reader->num_rows = line_count(reader);
    reader->rows     = csv_allocate_rows(reader->arena, reader->num_rows);
    if (!reader->rows) {
        return NULL;
    }

    char line[MAX_FIELD_SIZE] = {0};
    size_t rowIndex           = 0;
    bool headerSkipped        = false;

    // Read one line to determine the number of fields in the CSV file
    if (!fgets(line, MAX_FIELD_SIZE, reader->stream)) {
        fclose(reader->stream);
        return NULL;
    }

    // Reset the file pointer to the beginning of the file
    fseek(reader->stream, 0, SEEK_SET);

    // Get the number of fields in the CSV file
    size_t num_fields = get_num_fields(line, reader->delim, reader->quote);
    if (num_fields == 0) {
        fprintf(stderr, "Error: no fields found in CSV file\n");
        fclose(reader->stream);
        return NULL;
    }

    bool parse_success = true;
    while (fgets(line, MAX_FIELD_SIZE, reader->stream) && rowIndex < reader->num_rows) {
        // trim white space from end of line and skip empty lines
        char* end = line + strlen(line) - 1;
        while (end > line && isspace(*end)) {
            end--;
        }

        // If the line is empty, skip it
        if (end == line) {
            continue;
        }

        // Terminate the line with a null character
        end[1] = '\0';

        // skip comment lines
        if (line[0] == reader->comment) {
            continue;
        }

        if (reader->has_header && reader->skip_header && rowIndex == 0 && !headerSkipped) {
            headerSkipped = true;
            continue;
        }

        csv_line_params args = {
            .arena      = reader->arena,
            .line       = line,
            .rowIndex   = rowIndex,
            .row        = reader->rows[rowIndex],
            .delim      = reader->delim,
            .quote      = reader->quote,
            .num_fields = num_fields,
        };

        parse_success = parse_csv_line(&args);
        if (!parse_success) {
            break;
        }
        rowIndex++;
    }

    fclose(reader->stream);

    if (!parse_success) {
        fprintf(stderr, "csv_reader_parse() failed\n");
        fprintf(stderr, "Line: %s\n", line);
        return NULL;
    }

    return reader->rows;
}

void csv_reader_parse_async(CsvReader* reader, CsvRowCallback callback, size_t maxrows) {
    reader->num_rows = line_count(reader);

    size_t rowIndex           = 0;
    bool headerSkipped        = false;
    char line[MAX_FIELD_SIZE] = {0};

    // Limit the number of rows to parse if maxrows is set
    reader->num_rows = (maxrows > 0 && maxrows < reader->num_rows) ? maxrows : reader->num_rows;
    reader->rows     = csv_allocate_rows(reader->arena, reader->num_rows);
    if (!reader->rows) {
        return;
    }

    // Read one line to determine the number of fields in the CSV file
    if (!fgets(line, MAX_FIELD_SIZE, reader->stream)) {
        fclose(reader->stream);
        return;
    }

    // Reset the file pointer to the beginning of the file
    fseek(reader->stream, 0, SEEK_SET);

    // Get the number of fields in the CSV file
    size_t num_fields = get_num_fields(line, reader->delim, reader->quote);
    if (num_fields == 0) {
        fprintf(stderr, "Error: no fields found in CSV file\n");
        fclose(reader->stream);
        return;
    }

    while (fgets(line, MAX_FIELD_SIZE, reader->stream) && rowIndex < reader->num_rows) {
        // trim white space from end of line and skip empty lines
        char* end = line + strlen(line) - 1;
        while (end > line && isspace(*end)) {
            end--;
        }

        // If the line is empty, skip it
        if (end == line) {
            continue;
        }

        // Terminate the line with a null character
        end[1] = '\0';

        // skip comment lines
        if (line[0] == reader->comment) {
            continue;
        }

        if (reader->has_header && reader->skip_header && rowIndex == 0 && !headerSkipped) {
            headerSkipped = true;
            continue;
        }

        csv_line_params args = {
            .arena      = reader->arena,
            .line       = line,
            .rowIndex   = rowIndex,
            .row        = reader->rows[rowIndex],
            .delim      = reader->delim,
            .quote      = reader->quote,
            .num_fields = num_fields,
        };

        if (!parse_csv_line(&args)) {
            fprintf(stderr, "csv_reader_parse_async() failed\n");
            break;
        }

        // Pass the processed row to the caller.
        callback(rowIndex, reader->rows[rowIndex]);
        rowIndex++;
    }

    fclose(reader->stream);
}

size_t csv_reader_numrows(const CsvReader* reader) {
    return reader->num_rows;
}

void csv_reader_free(CsvReader* reader) {
    if (!reader) return;

    // The row are allocated in the arena, so we only need to free the arena.
    arena_destroy(reader->arena);

    free(reader);
    reader = NULL;
}

void csv_reader_setconfig(CsvReader* reader, CsvReaderConfig config) {
    if (config.delim != '\0') {
        reader->delim = config.delim;
    }

    if (config.quote != '\0') {
        reader->quote = config.quote;
    }

    if (config.comment != '\0') {
        reader->comment = config.comment;
    }

    reader->has_header  = config.has_header;
    reader->skip_header = config.skip_header;
}

// Function to count the number of fields in a CSV line
static size_t get_num_fields(const char* line, char delim, char quote) {
    size_t numFields = 0;
    int insideQuotes = 0;

    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == quote) {
            insideQuotes = !insideQuotes;  // Toggle insideQuotes flag
        } else if (line[i] == delim && !insideQuotes) {
            numFields++;
        }
    }

    // Add the last field if it is not empty
    if (line[0] != '\0') {
        numFields++;
    }
    return numFields;
}

// Function to parse a CSV line and split it into fields
static bool parse_csv_line(csv_line_params* args) {
    char field[MAX_FIELD_SIZE] = {0};
    int insideQuotes           = 0;

    Row* row    = args->row;
    row->fields = arena_alloc(args->arena, args->num_fields * sizeof(char*));
    if (!row->fields) {
        fprintf(stderr, "ERROR: unable to allocate memory for fields\n");
        return false;
    }

    char** fields     = row->fields;
    size_t fieldIndex = 0;
    row->count        = 0;

    for (size_t i = 0; args->line[i] != '\0'; i++) {
        if (args->line[i] == args->quote) {
            insideQuotes = !insideQuotes;
        } else if (args->line[i] == args->delim && !insideQuotes) {
            field[fieldIndex]  = '\0';
            fields[row->count] = arena_strdupn(args->arena, field, fieldIndex);
            if (!fields[row->count]) {
                fprintf(stderr, "ERROR: unable to allocate memory for fields[%zu]\n", row->count);
                return false;
            }
            row->count++;
            fieldIndex = 0;
        } else {
            field[fieldIndex++] = args->line[i];
        }
    }

    // If inside quotes at the end of the line, the line is not terminated
    if (insideQuotes) {
        fprintf(stderr, "ERROR: unterminated quoted field:%s in line %zu\n", args->line, args->rowIndex);
        return false;
    }

    // Add the last field.
    field[fieldIndex]  = '\0';
    fields[row->count] = arena_strdupn(args->arena, field, fieldIndex);
    if (!fields[row->count]) {
        fprintf(stderr, "ERROR: unable to allocate memory for fields[%zu]\n", row->count);
        return false;
    }
    row->count++;

    // validate the number of fields
    if (row->count != args->num_fields) {
        fprintf(stderr, "ERROR: invalid number of fields in line %zu\n", args->rowIndex);
        return false;
    }

    return true;
}

// count the number of lines in a csv file.
// ignore comments. Optionally skip header.
static size_t line_count(CsvReader* reader) {
    size_t lines       = 0;
    char prevChar      = '\n';
    bool headerSkipped = false;

    while (!feof(reader->stream)) {
        char c = (char)fgetc(reader->stream);
        if (c == EOF) {
            break;
        }

        // Ignore comment lines
        if (c == reader->comment) {
            while ((c = (char)fgetc(reader->stream)) != EOF && c != '\n')
                ;

            // read the new line character at end of comment line
            c = (char)fgetc(reader->stream);
            continue;
        }

        // Skip the header line if it exists and hasn't been skipped yet
        if (reader->has_header && reader->skip_header && !headerSkipped && lines == 0) {
            while ((c = (char)fgetc(reader->stream)) != EOF && c != '\n')
                ;
            headerSkipped = true;
            continue;
        }

        if (c == '\n' && !isspace(prevChar)) {
            char nextChar = (char)fgetc(reader->stream);
            if (nextChar == EOF) {
                break;
            }

            if (isspace(nextChar)) {
                continue;
            }
            lines++;

            // Put back the character if it is not EOF
            ungetc(nextChar, reader->stream);
        } else if (!isspace(c)) {
            prevChar = c;
        }
    }

    // Count last line if it doesn't end with a newline character and is not empty
    if (prevChar != '\n' && !isspace(prevChar)) {
        lines++;
    }

    fseek(reader->stream, 0, SEEK_SET);
    return lines;
}

typedef struct CsvWriter {
    FILE* stream;    // file_t pointer corresponding to the file stream.
    char delim;      // Delimiter character
    char quote;      // Quote character
    char newline;    // Newline character
    bool quote_all;  // Quote all fields
    bool flush;      // Flush the stream after writing each row
} CsvWriter;

CsvWriter* csvwriter_new(const char* filename) {
    CsvWriter* writer = malloc(sizeof(CsvWriter));
    if (!writer) {
        fprintf(stderr, "error allocating memory for CsvWriter\n");
        return NULL;
    }

    writer->stream = fopen(filename, "w");
    if (!writer->stream) {
        fprintf(stderr, "error opening file %s\n", filename);
        free(writer);
        return NULL;
    }

    writer->delim     = ',';
    writer->quote     = '"';
    writer->newline   = '\n';
    writer->quote_all = false;
    writer->flush     = false;
    return writer;
}

/**
 * Checks if a field needs quoting based on CSV rules.
 * @param field The field content to check.
 * @param delim The delimiter character.
 * @param quote The quote character.
 * @param newline The newline character.
 * @return true if field needs quoting, false otherwise.
 */
static inline bool field_needs_quoting(const char* field, char delim, char quote, char newline) {
    // Check for delimiter, quote character, or newline in the field
    return (strchr(field, delim) != nullptr || strchr(field, quote) != nullptr || strchr(field, newline) != nullptr);
}

/**
 * Writes a quoted field to the file stream, properly escaping quote characters.
 * @param fp File stream to write to.
 * @param field Field content to write.
 * @param quote Quote character to use and escape.
 * @return true on success, false on I/O error.
 */
static bool write_quoted_field(FILE* fp, const char* field, char quote) {
    // Write opening quote
    if (fputc(quote, fp) == EOF) {
        return false;
    }

    // Write field content, escaping quotes by doubling them (CSV standard)
    for (const char* ptr = field; *ptr != '\0'; ptr++) {
        if (*ptr == quote) {
            // Escape quote by writing it twice
            if (fputc(quote, fp) == EOF || fputc(quote, fp) == EOF) {
                return false;
            }
        } else {
            if (fputc(*ptr, fp) == EOF) {
                return false;
            }
        }
    }

    // Write closing quote
    if (fputc(quote, fp) == EOF) {
        return false;
    }

    return true;
}

/**
 * Writes a single field to the CSV file with proper quoting rules.
 * @param fp File stream to write to.
 * @param field Field content to write.
 * @param quote_all Whether to quote all fields regardless of content.
 * @param delim The delimiter character.
 * @param quote The quote character.
 * @param newline The newline character.
 * @return true on success, false on I/O error.
 */
static bool write_single_field(FILE* fp, const char* field, bool quote_all, char delim, char quote, char newline) {
    if (field == nullptr) {
        // Handle null field as empty string
        field = "";
    }

    if (quote_all || field_needs_quoting(field, delim, quote, newline)) {
        return write_quoted_field(fp, field, quote);
    } else {
        // Simple case - no quoting needed, use fputs for efficiency
        return fputs(field, fp) != EOF;
    }
}

/**
 * Optimized CSV writer function with comprehensive error handling.
 * @param writer Pointer to CsvWriter instance.
 * @param fields Array of field strings to write.
 * @param numfields Number of fields in the array.
 * @return true on success, false on error (check errno for details).
 */
bool csvwriter_write_row(CsvWriter* writer, const char** fields, size_t numfields) {
    // Input validation
    if (writer == nullptr) {
        errno = EINVAL;
        return false;
    }

    if (fields == nullptr && numfields > 0) {
        errno = EINVAL;
        return false;
    }

    FILE* fp = NULL;

    if (numfields == 0) {
        // Writing empty row - just write newline
        if (fputc(writer->newline, writer->stream) == EOF) {
            return false;
        }
        goto flush_and_exit;
    }

    fp = writer->stream;

    // Check if stream is valid before proceeding
    if (ferror(fp)) {
        return false;
    }

    // Write all fields with delimiters
    for (size_t i = 0; i < numfields; i++) {
        // Write delimiter before all fields except the first
        if (i > 0) {
            if (fputc(writer->delim, fp) == EOF) {
                return false;
            }
        }

        // Write the field content
        if (!write_single_field(fp, fields[i], writer->quote_all, writer->delim, writer->quote, writer->newline)) {
            return false;
        }
    }

    // Write row terminator
    if (fputc(writer->newline, fp) == EOF) {
        return false;
    }

flush_and_exit:
    // Flush if requested
    if (writer->flush && fp) {
        if (fflush(fp) != 0) {
            return false;
        }
    }

    // Final error check
    return !ferror(fp);
}

void csvwriter_free(CsvWriter* writer) {
    if (!writer) return;
    if (writer->stream) fclose(writer->stream);
    free(writer);
}

// configure the csv writer
void csvwriter_setconfig(CsvWriter* writer, CsvWriterConfig config) {
    if (config.delim != '\0') {
        writer->delim = config.delim;
    }

    if (config.quote != '\0') {
        writer->quote = config.quote;
    }

    writer->quote_all = config.quote_all;
    writer->flush     = config.flush;
}
