#include "../include/csvparser.h"
#include "../include/file.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CsvParser {
    file_t* stream;    // file_t pointer corresponding to the file stream.
    CsvRow** rows;     // Array of row pointers
    size_t num_rows;   // Number of rows in csv, excluding empty lines
    char delim;        // Delimiter character
    char quote;        // Quote character
    char comment;      // Comment character
    bool has_header;   // Whether the CSV file has a header
    bool skip_header;  // Whether to skip the header when parsing
    Arena* arena;      // Arena for memory allocation
} CsvParser;

typedef struct LineArgs {
    Arena* arena;
    const char* line;
    size_t num_fields;
    size_t rowIndex;
    CsvRow* row;
    char delim;
    char quote;
} LineArgs;

static size_t line_count(CsvParser* self);
static size_t get_num_fields(const char* line, char delim, char quote);
static bool parse_csv_line(LineArgs* args);

CsvParser* csvparser_new(const char* filename) {
    CsvParser* parser = malloc(sizeof(CsvParser));
    if (!parser) {
        fprintf(stderr, "error allocating memory for CsvParser\n");
        return NULL;
    }

    file_t* f = file_open(filename, "r");
    if (!f) {
        fprintf(stderr, "error opening file %s\n", filename);
        free(parser);
        return NULL;
    }

    Arena* arena = arena_create(CSV_ARENA_BLOCK_SIZE);
    if (!arena) {
        fprintf(stderr, "error creating memory arena\n");
        file_close(f);
        free(parser);
        return NULL;
    }

    parser->arena    = arena;
    parser->num_rows = 0;
    parser->stream   = f;

    // Set default configuration
    CsvParserConfigure(parser, .delim = ',');
    parser->rows = NULL;
    return parser;
}

// Allocate memory for rows and set num_rows.
static CsvRow** csv_allocate_rows(Arena* arena, size_t num_rows) {
    CsvRow** rows = arena_alloc(arena, num_rows * sizeof(CsvRow*));
    if (!rows) {
        fprintf(stderr, "csv_allocate_rows(): error allocating memory for CsvRow*\n");
        return NULL;
    }

    for (size_t i = 0; i < num_rows; i++) {
        rows[i] = arena_alloc(arena, sizeof(CsvRow));
        if (!rows[i]) {
            fprintf(stderr, "csv_allocate_rows(): error allocating memory for CsvRow: %zu\n", i);
            return NULL;
        }
    }
    return rows;
}

CsvRow** csvparser_parse(CsvParser* self) {
    // read num_rows and allocate them on heap.
    self->num_rows = line_count(self);
    self->rows     = csv_allocate_rows(self->arena, self->num_rows);
    if (!self->rows) {
        return NULL;
    }

    char line[MAX_FIELD_SIZE] = {0};
    size_t rowIndex           = 0;
    bool headerSkipped        = false;
    FILE* fp                  = file_fp(self->stream);

    // Read one line to determine the number of fields in the CSV file
    if (!fgets(line, MAX_FIELD_SIZE, fp)) {
        file_close(self->stream);
        return NULL;
    }

    // Reset the file pointer to the beginning of the file
    file_seek(self->stream, 0, SEEK_SET);

    // Get the number of fields in the CSV file
    size_t num_fields = get_num_fields(line, self->delim, self->quote);
    if (num_fields == 0) {
        fprintf(stderr, "Error: no fields found in CSV file\n");
        file_close(self->stream);
        return NULL;
    }

    while (fgets(line, MAX_FIELD_SIZE, fp) && rowIndex < self->num_rows) {
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
        if (line[0] == self->comment) {
            continue;
        }

        if (self->has_header && self->skip_header && rowIndex == 0 && !headerSkipped) {
            headerSkipped = true;
            continue;
        }

        LineArgs args = {
            .arena      = self->arena,
            .line       = line,
            .rowIndex   = rowIndex,
            .row        = self->rows[rowIndex],
            .delim      = self->delim,
            .quote      = self->quote,
            .num_fields = num_fields,
        };

        if (!parse_csv_line(&args)) {
            break;
        }

        rowIndex++;
    }

    file_close(self->stream);
    return self->rows;
}

void csvparser_parse_async(CsvParser* self, RowCallback callback, size_t maxrows) {
    self->num_rows = line_count(self);

    size_t rowIndex           = 0;
    bool headerSkipped        = false;
    char line[MAX_FIELD_SIZE] = {0};

    // Limit the number of rows to parse if maxrows is set
    self->num_rows = (maxrows > 0 && maxrows < self->num_rows) ? maxrows : self->num_rows;
    self->rows     = csv_allocate_rows(self->arena, self->num_rows);
    if (!self->rows) {
        return;
    }

    FILE* fp = file_fp(self->stream);
    // Read one line to determine the number of fields in the CSV file
    if (!fgets(line, MAX_FIELD_SIZE, fp)) {
        file_close(self->stream);
        return;
    }
    // Reset the file pointer to the beginning of the file
    file_seek(self->stream, 0, SEEK_SET);

    // Get the number of fields in the CSV file
    size_t num_fields = get_num_fields(line, self->delim, self->quote);
    if (num_fields == 0) {
        fprintf(stderr, "Error: no fields found in CSV file\n");
        file_close(self->stream);
        return;
    }

    while (fgets(line, MAX_FIELD_SIZE, fp) && rowIndex < self->num_rows) {
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
        if (line[0] == self->comment) {
            continue;
        }

        if (self->has_header && self->skip_header && rowIndex == 0 && !headerSkipped) {
            headerSkipped = true;
            continue;
        }

        LineArgs args = {
            .arena      = self->arena,
            .line       = line,
            .rowIndex   = rowIndex,
            .row        = self->rows[rowIndex],
            .delim      = self->delim,
            .quote      = self->quote,
            .num_fields = num_fields,
        };

        if (!parse_csv_line(&args)) {
            break;
        }

        // Pass the processed row to the caller.
        callback(rowIndex, self->rows[rowIndex]);
        rowIndex++;
    }

    file_close(self->stream);
}

size_t csvparser_numrows(const CsvParser* self) {
    return self->num_rows;
}

void csvparser_free(CsvParser* self) {
    if (!self)
        return;

    // The row are allocated in the arena, so we only need to free the arena.
    arena_destroy(self->arena);

    free(self);
    self = NULL;
}

void csvparser_setconfig(CsvParser* parser, CsvParserConfig config) {
    if (config.delim != '\0') {
        parser->delim = config.delim;
    }

    if (config.quote != '\0') {
        parser->quote = config.quote;
    }

    if (config.comment != '\0') {
        parser->comment = config.comment;
    }

    parser->has_header  = config.has_header;
    parser->skip_header = config.skip_header;
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
static bool parse_csv_line(LineArgs* args) {
    char field[MAX_FIELD_SIZE] = {0};
    int insideQuotes           = 0;

    args->row->fields = arena_alloc(args->arena, args->num_fields * sizeof(char*));
    if (!args->row->fields) {
        fprintf(stderr, "ERROR: unable to allocate memory for row->fields\n");
        return false;
    }

    int fieldIndex       = 0;
    args->row->numFields = 0;

    for (int i = 0; args->line[i] != '\0'; i++) {
        if (args->line[i] == args->quote) {
            insideQuotes = !insideQuotes;
        } else if (args->line[i] == args->delim && !insideQuotes) {
            field[fieldIndex]                       = '\0';
            args->row->fields[args->row->numFields] = arena_alloc_string(args->arena, field);
            if (!args->row->fields[args->row->numFields]) {
                fprintf(stderr, "ERROR: unable to allocate memory for args->row->fields[%zu]\n",
                        args->row->numFields);
                return false;
            }

            args->row->numFields++;
            fieldIndex = 0;
        } else {
            field[fieldIndex++] = args->line[i];
        }
    }

    // If inside quotes at the end of the line, the line is not terminated
    if (insideQuotes) {
        fprintf(stderr, "ERROR: unterminated quoted field:%s in line %zu\n", args->line,
                args->rowIndex);
        return false;
    }

    // Add the last field.
    field[fieldIndex]                       = '\0';
    args->row->fields[args->row->numFields] = arena_alloc_string(args->arena, field);
    if (!args->row->fields[args->row->numFields]) {
        fprintf(stderr, "ERROR: unable to allocate memory for args->row->fields[%zu]\n",
                args->row->numFields);
        return false;
    }
    args->row->numFields++;

    // validate the number of fields
    if (args->row->numFields != args->num_fields) {
        fprintf(stderr, "ERROR: invalid number of fields in line %zu\n", args->rowIndex);
        return false;
    }

    return true;
}

// count the number of lines in a csv file.
// ignore comments. Optionally skip header.
static size_t line_count(CsvParser* self) {
    size_t lines       = 0;
    char prevChar      = '\n';
    bool headerSkipped = false;

    FILE* fp = file_fp(self->stream);
    while (!feof(fp)) {
        char c = (char)fgetc(fp);
        if (c == EOF) {
            break;
        }

        // Ignore comment lines
        if (c == self->comment) {
            while ((c = (char)fgetc(fp)) != EOF && c != '\n')
                ;

            // read the new line character at end of comment line
            c = (char)fgetc(fp);
            continue;
        }

        // Skip the header line if it exists and hasn't been skipped yet
        if (self->has_header && self->skip_header && !headerSkipped && lines == 0) {
            while ((c = (char)fgetc(fp)) != EOF && c != '\n')
                ;
            headerSkipped = true;
            continue;
        }

        if (c == '\n' && !isspace(prevChar)) {
            char nextChar = (char)fgetc(fp);
            if (nextChar == EOF) {
                break;
            }

            if (isspace(nextChar)) {
                continue;
            }
            lines++;

            // Put back the character if it is not EOF
            ungetc(nextChar, fp);
        } else if (!isspace(c)) {
            prevChar = c;
        }
    }

    // Count last line if it doesn't end with a newline character and is not empty
    if (prevChar != '\n' && !isspace(prevChar)) {
        lines++;
    }

    file_seek(self->stream, 0, SEEK_SET);
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

void csvwriter_write_row(CsvWriter* self, const char** fields, size_t numfields) {
    if (!self || !fields) {
        return;
    }

    FILE* fp = self->stream;
    for (size_t i = 0; i < numfields; i++) {
        if (i > 0) {
            fputc(self->delim, fp);
        }

        if (self->quote_all || strchr(fields[i], self->delim) || strchr(fields[i], self->quote)) {
            fputc(self->quote, fp);
            fputs(fields[i], fp);
            fputc(self->quote, fp);
        } else {
            fputs(fields[i], fp);
        }
    }

    fputc(self->newline, fp);
    if (self->flush) {
        fflush(fp);
    }
}

void csvwriter_free(CsvWriter* self) {
    if (!self) {
        return;
    }

    if (self->stream) {
        fclose(self->stream);
    }

    free(self);
    self = NULL;
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
