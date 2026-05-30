#include <assert.h>
#include <solidc/defer.h>
#include <solidc/regex.h>
#include <string.h>

int main(void)
{
    regex_t*     re  = regex_must_compile("(\\w+)@(\\w+\\.\\w+)", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    if (!re || !ctx) {
        return 1;
    }

    defer
    {
        regex_free(re);
        regex_ctx_free(ctx);
    };

    const char*   subj       = "user@example.com";
    regex_match_t match      = {0};
    char          buffer[64] = {0};

    regex_status_t st = regex_exec(re, ctx, subj, strlen(subj), 0, &match);
    assert(st == REGEX_OK);

    assert(match.count == 3u); /* g0 + two groups */

    /* g0: full match */
    assert(match.group[0].start == 0);
    assert(match.group[0].end == 16);
    regex_group_copy(subj, &match, 0, buffer, sizeof(buffer));
    assert(strcmp(buffer, "user@example.com") == 0);

    /* g1: "user" */
    assert(match.group[1].start == 0);
    assert(match.group[1].end == 4);
    regex_group_copy(subj, &match, 1, buffer, sizeof(buffer));
    assert(strcmp(buffer, "user") == 0);

    /* g2: "example.com" */
    assert(match.group[2].start == 5);
    assert(match.group[2].end == 16);
    regex_group_copy(subj, &match, 2, buffer, sizeof(buffer));
    assert(strcmp(buffer, "example.com") == 0);

    (void)st;
}