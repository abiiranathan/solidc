#include "../include/dotenv.h"
#include "../include/defer.h"
#include "../include/file.h"
#include "../include/filepath.h"
#include "../include/macros.h"

int main() {
    char* env = make_tempfile();
    ASSERT(env);

    defer({
        remove(env);
        free(env);
    });

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
    char* name        = secure_getenv("NAME");
    char* author      = secure_getenv("AUTHOR");
    char* name_author = secure_getenv("NAME_AUTHOR");

    ASSERT(name && author && name_author);
    ASSERT_STR_EQ(name, "SOLID C");
    ASSERT_STR_EQ(author, "Dr. Abiira");
    ASSERT_STR_EQ(name_author, "SOLID C Dr. Abiira");
}
