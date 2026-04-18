#include <solidc/regex.h>
#include <solidc/str_slice.h>

static StrSlice ss_from_span(const char* base, regex_span_t span) {
    return (StrSlice){.data = base + span.start, .len = span.end - span.start};
}

int main() {
    regex_t* re = regex_must_compile("hello (\\w+)", REGEX_FLAG_NONE);
    regex_ctx_t* ctx = regex_ctx_must_create();
    StrSlice ss;

    if (re && ctx) {
        const char* subj = "hello world";
        regex_match_t m = {0};
        regex_status_t st = regex_exec(re, ctx, subj, strlen(subj), 0, &m);
        if (st == REGEX_OK) {
            // The whole match is in m.group[0], and the first capture group is in m.group[1].
            regex_span_t group1 = m.group[1]; /* first capture group */
            ss = ss_from_span(subj, group1);
            printf("Matched group 1: '%.*s'\n", (int)ss.len, ss.data);
        } else {
            printf("No match\n");
        }
    }
}
