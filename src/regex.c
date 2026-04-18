#include "../include/regex.h"

#include <assert.h>   /* for assert */
#include <stdatomic.h>/* for atomic_fetch_add, atomic_fetch_sub */
#include <stdio.h>    /* for snprintf */
#include <stdlib.h>   /* for malloc, free */
#include <string.h>   /* for strlen, strncpy */

/**
 * Internal representation of a compiled regular expression.
 * The public API exposes only the opaque regex_t alias.
 */
struct regex_s {
    pcre2_code* code;     /**< PCRE2 compiled code object.             */
    char* pattern;        /**< Copy of the original pattern string.    */
    uint32_t group_count; /**< Number of capture groups (excl. g0).    */
    atomic_int refcount;  /**< Reference count; freed when it hits 0.  */
};

/**
 * Per-thread execution context.  Wraps a PCRE2 match-data block that is
 * pre-allocated to REGEX_MAX_GROUPS pairs so we avoid per-call allocation.
 */
struct regex_ctx_s {
    pcre2_match_data* match_data; /**< Pre-allocated PCRE2 match-data block. */
};

/**
 * State for the non-overlapping-match iterator.
 */
struct regex_iter_s {
    regex_t* re;         /**< Retained reference to the compiled pattern.   */
    regex_ctx_t* ctx;    /**< Borrowed reference; caller must keep alive.   */
    const char* subject; /**< Borrowed pointer; caller must keep alive.     */
    size_t len;          /**< Byte length of subject.                       */
    size_t offset;       /**< Current byte offset into subject.             */
};

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/**
 * Copies a PCRE2 error message for the given error code into buf.
 *
 * @param errcode   A PCRE2 error code (negative integer).
 * @param buf       Destination buffer.
 * @param buf_len   Capacity of buf.
 */
static void pcre2_err_message(int errcode, char* buf, size_t buf_len) {
    PCRE2_UCHAR8 tmp[256];
    if (pcre2_get_error_message(errcode, tmp, sizeof(tmp)) < 0) {
        snprintf(buf, buf_len, "PCRE2 error %d (no message available)", errcode);
    } else {
        snprintf(buf, buf_len, "%s", (const char*)tmp);
    }
}

/**
 * Populates a regex_match_t from the PCRE2 ovector and match-data block.
 *
 * @param re         Compiled regex whose group count is authoritative.
 * @param md         PCRE2 match-data block returned from pcre2_match.
 * @param rc         The positive return value from pcre2_match (number of
 *                   captured pairs filled in the ovector).
 * @param match      Output structure to populate.
 */
static void fill_match(const regex_t* re, pcre2_match_data* md, int rc, regex_match_t* match) {
    const PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);

    /* The match count from pcre2_match is the number of *filled* pairs;
     * groups beyond that still exist but were not captured this run.       */
    uint32_t total = re->group_count + 1; /* include g0 */
    if (total > REGEX_MAX_GROUPS) {
        total = REGEX_MAX_GROUPS;
    }
    match->count = total;

    for (uint32_t i = 0; i < total; i++) {
        match->group[i].start = ov[2 * i];
        match->group[i].end = ov[2 * i + 1];
    }

    /* Zero out any trailing slots not covered by this match. */
    for (uint32_t i = total; i < REGEX_MAX_GROUPS; i++) {
        match->group[i].start = 0;
        match->group[i].end = 0;
    }

    (void)rc; /* rc is used implicitly via the ovector; suppress unused warning */
}

regex_status_t regex_compile(const char* pattern, regex_flags_t flags, regex_t** out, char* errbuf, size_t errbuf_len) {
    if (pattern == NULL || out == NULL) {
        return REGEX_ERROR_ARGS;
    }
    *out = NULL;

    /* Compile via PCRE2. */
    int errcode = 0;
    PCRE2_SIZE erroffset = 0;

    pcre2_code* code = pcre2_compile((PCRE2_SPTR8)pattern, PCRE2_ZERO_TERMINATED, (uint32_t)flags, &errcode, &erroffset,
                                     NULL /* use default compile context */);

    if (code == NULL) {
        if (errbuf != NULL && errbuf_len > 0) {
            PCRE2_UCHAR8 tmp[256];
            pcre2_get_error_message(errcode, tmp, sizeof(tmp));
            snprintf(errbuf, errbuf_len, "pattern error at offset %zu: %s", (size_t)erroffset, (const char*)tmp);
        }
        return REGEX_ERROR;
    }

    /* Study / JIT-compile for speed if JIT is available. */
    pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);

    /* Query capture group count before committing. */
    uint32_t group_count = 0;
    pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &group_count);

    if (group_count + 1 > REGEX_MAX_GROUPS) {
        /* The total slots needed (groups + g0) exceed our cap. */
        pcre2_code_free(code);
        if (errbuf != NULL && errbuf_len > 0) {
            snprintf(errbuf, errbuf_len, "pattern has %u capture groups; limit is %d", group_count,
                     REGEX_MAX_GROUPS - 1);
        }
        return REGEX_ERROR_LIMIT;
    }

    /* Duplicate the pattern string for later introspection. */
    char* pat_dup = strdup(pattern);
    if (pat_dup == NULL) {
        pcre2_code_free(code);
        return REGEX_ERROR_NOMEM;
    }

    /* Allocate and initialise the regex_t wrapper. */
    regex_t* re = malloc(sizeof(*re));
    if (re == NULL) {
        free(pat_dup);
        pcre2_code_free(code);
        return REGEX_ERROR_NOMEM;
    }

    *re = (regex_t){
        .code = code,
        .pattern = pat_dup,
        .group_count = group_count,
        /* refcount initialised below via atomic store */
    };
    atomic_store(&re->refcount, 1);

    *out = re;
    return REGEX_OK;
}

regex_t* regex_retain(regex_t* re) {
    if (re != NULL) {
        atomic_fetch_add(&re->refcount, 1);
    }
    return re;
}

void regex_free(regex_t* re) {
    if (re == NULL) {
        return;
    }

    if (atomic_fetch_sub(&re->refcount, 1) == 1) {
        /* Last reference: tear down. */
        pcre2_code_free(re->code);
        free(re->pattern);
        free(re);
    }
}

/* ---------------------------------------------------------------------------
 * Execution context
 * ------------------------------------------------------------------------- */

regex_status_t regex_ctx_create(regex_ctx_t** out) {
    if (out == NULL) {
        return REGEX_ERROR_ARGS;
    }
    *out = NULL;

    regex_ctx_t* ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return REGEX_ERROR_NOMEM;
    }

    /* Allocate the match-data block for up to REGEX_MAX_GROUPS pairs.
     * We use the "from pattern" variant with a NULL code pointer so we can
     * pass an explicit pair count.  That variant is pcre2_match_data_create,
     * which takes an ovector pair count directly. */
    ctx->match_data = pcre2_match_data_create(REGEX_MAX_GROUPS, NULL);
    if (ctx->match_data == NULL) {
        free(ctx);
        return REGEX_ERROR_NOMEM;
    }

    *out = ctx;
    return REGEX_OK;
}

void regex_ctx_free(regex_ctx_t* ctx) {
    if (ctx == NULL) {
        return;
    }
    pcre2_match_data_free(ctx->match_data);
    free(ctx);
}

/* ---------------------------------------------------------------------------
 * Matching
 * ------------------------------------------------------------------------- */

regex_status_t regex_exec(const regex_t* re, regex_ctx_t* ctx, const char* subject, size_t len, size_t offset,
                          regex_match_t* match) {
    if (re == NULL || ctx == NULL || subject == NULL || match == NULL) {
        return REGEX_ERROR_ARGS;
    }
    if (offset > len) {
        return REGEX_ERROR_ARGS;
    }

    int rc = pcre2_match(re->code, (PCRE2_SPTR8)subject, (PCRE2_SIZE)len, (PCRE2_SIZE)offset, 0 /* no extra flags */,
                         ctx->match_data, NULL /* use default match context */);

    if (rc == PCRE2_ERROR_NOMATCH) {
        return REGEX_NO_MATCH;
    }
    if (rc < 0) {
        return REGEX_ERROR;
    }

    fill_match(re, ctx->match_data, rc, match);
    return REGEX_OK;
}

regex_status_t regex_match(const regex_t* re, regex_ctx_t* ctx, const char* subject, regex_match_t* match) {
    if (subject == NULL) {
        return REGEX_ERROR_ARGS;
    }
    return regex_exec(re, ctx, subject, strlen(subject), 0, match);
}

bool regex_is_match(const regex_t* re, regex_ctx_t* ctx, const char* subject, size_t len) {
    if (re == NULL || ctx == NULL || subject == NULL) {
        return false;
    }

    int rc = pcre2_match(re->code, (PCRE2_SPTR8)subject, (PCRE2_SIZE)len, 0, 0, ctx->match_data, NULL);

    return rc > 0;
}

/* ---------------------------------------------------------------------------
 * Iterator
 * ------------------------------------------------------------------------- */

regex_status_t regex_iter_init(regex_t* re, regex_ctx_t* ctx, const char* subject, size_t len, regex_iter_t** out) {
    if (re == NULL || ctx == NULL || subject == NULL || out == NULL) {
        return REGEX_ERROR_ARGS;
    }
    *out = NULL;

    regex_iter_t* iter = malloc(sizeof(*iter));
    if (iter == NULL) {
        return REGEX_ERROR_NOMEM;
    }

    *iter = (regex_iter_t){
        .re = regex_retain(re),
        .ctx = ctx,
        .subject = subject,
        .len = len,
        .offset = 0,
    };

    *out = iter;
    return REGEX_OK;
}

regex_status_t regex_iter_next(regex_iter_t* iter, regex_match_t* match) {
    if (iter == NULL || match == NULL) {
        return REGEX_ERROR_ARGS;
    }
    if (iter->offset > iter->len) {
        return REGEX_NO_MATCH;
    }

    int rc = pcre2_match(iter->re->code, (PCRE2_SPTR8)iter->subject, (PCRE2_SIZE)iter->len, (PCRE2_SIZE)iter->offset, 0,
                         iter->ctx->match_data, NULL);

    if (rc == PCRE2_ERROR_NOMATCH) {
        return REGEX_NO_MATCH;
    }
    if (rc < 0) {
        return REGEX_ERROR;
    }

    fill_match(iter->re, iter->ctx->match_data, rc, match);

    /* Advance the offset past this match to avoid re-matching.
     * If the match is zero-length we must advance by at least one byte to
     * prevent an infinite loop.  This mirrors the behaviour of most regex
     * engines (Perl, Python, Go) for zero-width matches. */
    size_t end = match->group[0].end;
    if (end == iter->offset) {
        iter->offset = end + 1;
    } else {
        iter->offset = end;
    }

    return REGEX_OK;
}

void regex_iter_free(regex_iter_t* iter) {
    if (iter == NULL) {
        return;
    }
    regex_free(iter->re);
    free(iter);
}

/* ---------------------------------------------------------------------------
 * Substitution (shared implementation)
 * ------------------------------------------------------------------------- */

/**
 * Shared body for regex_sub and regex_gsub; differs only in the PCRE2
 * substitute flags.
 */
static regex_status_t sub_impl(const regex_t* re, regex_ctx_t* ctx, const char* subject, size_t subject_len,
                               const char* replacement, char* out_buf, size_t* out_len, uint32_t pcre2_flags) {
    if (re == NULL || ctx == NULL || subject == NULL || replacement == NULL || out_buf == NULL || out_len == NULL) {
        return REGEX_ERROR_ARGS;
    }

    PCRE2_SIZE result_len = (PCRE2_SIZE)*out_len;

    int rc = pcre2_substitute(re->code, (PCRE2_SPTR8)subject, (PCRE2_SIZE)subject_len, 0 /* start offset */,
                              pcre2_flags | PCRE2_SUBSTITUTE_EXTENDED, ctx->match_data, NULL /* match context */,
                              (PCRE2_SPTR8)replacement, PCRE2_ZERO_TERMINATED, (PCRE2_UCHAR8*)out_buf, &result_len);

    if (rc == PCRE2_ERROR_NOMATCH || rc == 0) {
        return REGEX_NO_MATCH;
    }
    if (rc == PCRE2_ERROR_NOMEMORY) {
        /* result_len now holds the required size including NUL. */
        *out_len = (size_t)result_len;
        return REGEX_ERROR;
    }
    if (rc < 0) {
        return REGEX_ERROR;
    }

    /* result_len is the number of code units written, excluding NUL. */
    *out_len = (size_t)result_len;
    return REGEX_OK;
}

regex_status_t regex_sub(const regex_t* re, regex_ctx_t* ctx, const char* subject, size_t subject_len,
                         const char* replacement, char* out_buf, size_t* out_len) {
    return sub_impl(re, ctx, subject, subject_len, replacement, out_buf, out_len, 0 /* replace first match only */);
}

regex_status_t regex_gsub(const regex_t* re, regex_ctx_t* ctx, const char* subject, size_t subject_len,
                          const char* replacement, char* out_buf, size_t* out_len) {
    return sub_impl(re, ctx, subject, subject_len, replacement, out_buf, out_len, PCRE2_SUBSTITUTE_GLOBAL);
}

/* ---------------------------------------------------------------------------
 * Introspection
 * ------------------------------------------------------------------------- */

uint32_t regex_group_count(const regex_t* re) {
    if (re == NULL) {
        return 0;
    }
    return re->group_count;
}

const char* regex_pattern(const regex_t* re) {
    if (re == NULL) {
        return "";
    }
    return re->pattern;
}

void regex_strerror(regex_status_t status, char* buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) {
        return;
    }
    const char* msg;
    switch (status) {
        case REGEX_OK:
            msg = "success";
            break;
        case REGEX_NO_MATCH:
            msg = "no match";
            break;
        case REGEX_ERROR:
            msg = "general error";
            break;
        case REGEX_ERROR_NOMEM:
            msg = "memory allocation failed";
            break;
        case REGEX_ERROR_ARGS:
            msg = "invalid arguments";
            break;
        case REGEX_ERROR_LIMIT:
            msg = "too many capture groups";
            break;
        default:
            msg = "unknown status code";
            break;
    }
    snprintf(buf, buf_len, "%s", msg);
}