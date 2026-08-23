#include "../include/dotenv.h"
#include "../include/file.h"
#include "../include/filepath.h"
#include "../include/macros.h"

// Adversarial semantics battery (bash / python-dotenv parity).
static void test_dotenv_semantics(void) {
    const char* path = make_tempfile();
    ASSERT(path);

    file_t fp;
    ASSERT(file_open(&fp, path, "w") == FILE_SUCCESS);

    const char* body =
        "H1=value#notcomment\n"        /* '#' without word boundary is data   */
        "H2=\"a#b\"\n"                /* quoted hash survives               */
        "H3=val # trailing comment\n"  /* spaced inline comment stripped     */
        "H4=\"after quote\" # gone\n"
        "ESC=\"a\\\"b\"\n"          /* escaped quote inside double quotes */
        "NL=\"line\\nbreak\"\n"      /* escape sequences in double quotes  */
        "SQ='literal\\nunchanged'\n"  /* single quotes are fully literal    */
        "EMPTY=\n";
    ASSERT(file_write_string(&fp, body) > 0);
    file_close((file_t*)&fp);

    ASSERT(load_dotenv(path));

    ASSERT_STR_EQ(GETENV("H1"), "value#notcomment");
    ASSERT_STR_EQ(GETENV("H2"), "a#b");
    ASSERT_STR_EQ(GETENV("H3"), "val");
    ASSERT_STR_EQ(GETENV("H4"), "after quote");
    ASSERT_STR_EQ(GETENV("ESC"), "a\"b");
    ASSERT_STR_EQ(GETENV("NL"), "line\nbreak");
    ASSERT_STR_EQ(GETENV("SQ"), "literal\\nunchanged");
    ASSERT_STR_EQ(GETENV("EMPTY"), "");

    remove(path);
    free((void*)path);
}

int main() {
    test_dotenv_semantics();
    char* env = make_tempfile();
    ASSERT(env);

    file_t fp;
    file_result_t res;

    res = file_open(&fp, env, "w");
    ASSERT(res == FILE_SUCCESS);

    const char* bytes = "NAME = SOLID C\nAUTHOR=\"Dr. Abiira\"\nNAME_AUTHOR=${NAME} ${AUTHOR}";
    ASSERT(file_write_string(&fp, bytes) > 0);

    // Close the file
    file_close((file_t*)&fp);

    ASSERT(load_dotenv(env));

    // Load the environement variables and verify them
    char* name        = GETENV("NAME");
    char* author      = GETENV("AUTHOR");
    char* name_author = GETENV("NAME_AUTHOR");

    ASSERT(name && author && name_author);
    ASSERT_STR_EQ(name, "SOLID C");
    ASSERT_STR_EQ(author, "Dr. Abiira");
    ASSERT_STR_EQ(name_author, "SOLID C Dr. Abiira");

    UNSETENV("NAME");
    UNSETENV("AUTHOR");
    UNSETENV("NAME_AUTHOR");

    name        = GETENV("NAME");
    author      = GETENV("AUTHOR");
    name_author = GETENV("NAME_AUTHOR");

    ASSERT_NULL(name);
    ASSERT_NULL(author);
    ASSERT_NULL(name_author);

    remove(env);
    free(env);
}
