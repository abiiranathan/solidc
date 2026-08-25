/**
 * @file chan_patterns_test.c
 * @brief Tests for pub/sub, worker pool, and select/merge abstractions.
 */
#include "../include/chan_patterns.h"
#include "../include/process.h"
#include "../include/thread.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ANSI_RED    "\x1b[31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_RESET  "\x1b[0m"

static int g_passed = 0;
static int g_failed = 0;

static void check(const char* name, bool ok) {
    if (ok) {
        printf(ANSI_GREEN "[PASS] %s\n" ANSI_RESET, name);
        g_passed++;
    } else {
        printf(ANSI_RED "[FAIL] %s\n" ANSI_RESET, name);
        g_failed++;
    }
}

/* ====================================================================== */
/* Pub/Sub                                                                */
/* ====================================================================== */

static void test_pubsub_basic(void) {
    printf(ANSI_YELLOW "\n=== pub/sub basic ===\n" ANSI_RESET);

    chan_pubsub_t* ps = chan_pubsub_new(sizeof(int), 4, 16);
    check("pubsub_new", ps != NULL);

    size_t s0 = chan_pubsub_subscribe(ps);
    size_t s1 = chan_pubsub_subscribe(ps);
    check("two subscribers", s0 != CHAN_PUBSUB_NO_SUB && s1 != CHAN_PUBSUB_NO_SUB);
    check("subscriber count 2", chan_pubsub_subscriber_count(ps) == 2);

    int v = 7;
    check("publish ok", chan_pubsub_publish(ps, &v) == (int)CHAN_OK);

    int out = 0;
    check("sub0 got value", chan_recv(chan_pubsub_queue(ps, s0), &out) && out == 7);
    check("sub1 got value", chan_recv(chan_pubsub_queue(ps, s1), &out) && out == 7);
    check("queues empty after drain", chan_try_recv(chan_pubsub_queue(ps, s0), &out) == (int)CHAN_EMPTY);

    /* Unsubscribe: s1 no longer receives */
    check("unsubscribe", chan_pubsub_unsubscribe(ps, s1) == (int)CHAN_OK);
    v = 8;
    chan_pubsub_publish(ps, &v);
    check("sub0 got post-unsub value", chan_recv(chan_pubsub_queue(ps, s0), &out) && out == 8);
    check("subscriber count 1", chan_pubsub_subscriber_count(ps) == 1);

    check("unsubscribe twice is CHAN_INVALID", chan_pubsub_unsubscribe(ps, s1) == (int)CHAN_INVALID);
    check("queue of unsubscribed is NULL", chan_pubsub_queue(ps, s1) == NULL);

    chan_pubsub_free(ps);
}

static void test_pubsub_close_and_full(void) {
    printf(ANSI_YELLOW "\n=== pub/sub close + full queue ===\n" ANSI_RESET);

    chan_pubsub_t* ps = chan_pubsub_new(sizeof(int), 2, 2); /* tiny depth 2 */
    size_t s0 = chan_pubsub_subscribe(ps);
    size_t s1 = chan_pubsub_subscribe(ps);

    /* Fill s0's queue but drain s1 as we go: publish 2 (fills both), then
     * drain s1 only, then publish 2 more -> s0 full, s1 fine. */
    int v = 0;
    chan_pubsub_publish(ps, &v);
    chan_pubsub_publish(ps, &v);
    int out;
    check("s1 drain", chan_recv(chan_pubsub_queue(ps, s1), &out));
    check("s1 drain", chan_recv(chan_pubsub_queue(ps, s1), &out));

    check("publish to full sub reports CHAN_FULL", chan_pubsub_publish(ps, &v) == (int)CHAN_FULL);

    /* s0 has exactly 3 pending (2 + 1), s1 has 1 */
    int pending0 = 0;
    while (chan_try_recv(chan_pubsub_queue(ps, s0), &out) == (int)CHAN_OK) pending0++;
    check("s0 buffered 2 (full queue preserved)", pending0 == 2);
    int pending1 = 0;
    while (chan_try_recv(chan_pubsub_queue(ps, s1), &out) == (int)CHAN_OK) pending1++;
    check("s1 buffered 1", pending1 == 1);

    /* Close: queues close after drain */
    check("close ok", chan_pubsub_close(ps) == (int)CHAN_OK);
    check("publish after close is CHAN_CLOSED", chan_pubsub_publish(ps, &v) == (int)CHAN_CLOSED);
    check("subscribe after close fails", chan_pubsub_subscribe(ps) == CHAN_PUBSUB_NO_SUB);
    check("double close ok", chan_pubsub_close(ps) == (int)CHAN_OK);

    chan_pubsub_free(ps);
    check("pubsub_free NULL safe", true);
    chan_pubsub_free(NULL);
}

/* ====================================================================== */
/* Worker pool                                                            */
/* ====================================================================== */

typedef struct {
    atomic_llong sum;
    atomic_llong processed;
} tally;

static void sum_job(void* value, void* user) {
    tally* t = (tally*)user;
    atomic_fetch_add(&t->sum, *(int64_t*)value);
    atomic_fetch_add(&t->processed, 1);
}

static void test_workers_basic(void) {
    printf(ANSI_YELLOW "\n=== worker pool basic ===\n" ANSI_RESET);

    tally t = {0};
    atomic_store(&t.sum, 0);
    atomic_store(&t.processed, 0);

    chan_workers_t* w = chan_workers_new(4, sizeof(int64_t), 0, sum_job, &t);
    check("workers_new", w != NULL);

    enum { N = 10000 };
    int64_t expected = 0;
    for (int64_t i = 1; i <= N; i++) {
        int64_t v = i;
        expected += v;
        if (chan_workers_submit(w, &v) != (int)CHAN_OK) {
            check("submit ok", false);
            break;
        }
    }
    check("all submitted", chan_workers_pending(w) <= (size_t)N);

    check("shutdown drains everything", chan_workers_shutdown(w) == (int)CHAN_OK);
    check("all items processed", atomic_load(&t.processed) == N);
    check("checksum matches", atomic_load(&t.sum) == expected);
    check("pending zero after shutdown", chan_workers_pending(w) == 0);

    /* Submit after shutdown is rejected */
    int64_t v = 1;
    check("submit after shutdown is CHAN_CLOSED", chan_workers_submit(w, &v) == (int)CHAN_CLOSED);
    check("double shutdown ok", chan_workers_shutdown(w) == (int)CHAN_OK);

    chan_workers_free(w);
    check("workers_free NULL safe", true);
    chan_workers_free(NULL);
}

typedef struct {
    char buf[64];
} big_item;

static void big_job(void* value, void* user) {
    big_item* bi = (big_item*)value;
    tally* t = (tally*)user;
    if (strcmp(bi->buf, "payload") == 0) atomic_fetch_add(&t->processed, 1);
}

static void test_workers_large_items(void) {
    printf(ANSI_YELLOW "\n=== worker pool large items ===\n" ANSI_RESET);

    tally t = {0};
    atomic_store(&t.processed, 0);

    chan_workers_t* w = chan_workers_new(2, sizeof(big_item), 0, big_job, &t);
    check("large workers_new", w != NULL);

    big_item bi;
    memset(&bi, 0, sizeof(bi));
    strcpy(bi.buf, "payload");
    for (int i = 0; i < 100; i++) chan_workers_submit(w, &bi);
    chan_workers_shutdown(w);

    check("all large items processed", atomic_load(&t.processed) == 100);
    chan_workers_free(w);
}

/* ====================================================================== */
/* Select                                                                 */
/* ====================================================================== */

static void test_select_basic(void) {
    printf(ANSI_YELLOW "\n=== select basic ===\n" ANSI_RESET);

    Channel* a = chan_new(sizeof(int), 0);
    Channel* b = chan_new(sizeof(int), 0);

    /* Nothing ready: timeout */
    int out = 0;
    Channel* chans[2] = {a, b};
    void* outs[2] = {&out, &out};
    check("select timeout on idle channels", chan_select(chans, outs, 2, 30) == CHAN_SELECT_TIMEOUT);

    /* Value in b: select picks index 1 */
    int vb = 21;
    chan_send(b, &vb);
    int idx = chan_select(chans, outs, 2, 100);
    check("select picks b (index 1)", idx == 1);
    check("select received value", out == 21);

    /* Value in a: select picks index 0 (priority) */
    int va = 42;
    chan_send(a, &va);
    idx = chan_select(chans, outs, 2, 100);
    check("select picks a (index 0)", idx == 0);
    check("select received value a", out == 42);

    /* NULL outs discards the value */
    int vc = 5;
    chan_send(a, &vc);
    idx = chan_select(chans, NULL, 2, 100);
    check("select with NULL outs", idx == 0);
    check("value was consumed", chan_try_recv(a, &out) == (int)CHAN_EMPTY);

    /* Bad args */
    check("select count 0 is timeout", chan_select(chans, outs, 0, 10) == CHAN_SELECT_TIMEOUT);
    check("select over max is timeout", chan_select(chans, outs, CHAN_SELECT_MAX + 1, 10) == CHAN_SELECT_TIMEOUT);

    chan_free(a);
    chan_free(b);
}

static void test_select_skips_closed(void) {
    printf(ANSI_YELLOW "\n=== select skips closed channels ===\n" ANSI_RESET);

    Channel* closed = chan_new(sizeof(int), 0);
    Channel* live = chan_new(sizeof(int), 0);

    chan_close(closed);
    int v = 99;
    chan_send(live, &v);

    int out = 0;
    Channel* chans[2] = {closed, live};
    void* outs[2] = {&out, &out};
    int idx = chan_select(chans, outs, 2, 100);
    check("select skips closed, picks live", idx == 1 && out == 99);

    /* All closed: timeout */
    chan_close(live);
    check("select over all-closed is timeout", chan_select(chans, outs, 2, 30) == CHAN_SELECT_TIMEOUT);

    chan_free(closed);
    chan_free(live);
}

/* ====================================================================== */
/* Merge                                                                  */
/* ====================================================================== */

static void test_merge(void) {
    printf(ANSI_YELLOW "\n=== merge ===\n" ANSI_RESET);

    Channel* dest = chan_new(sizeof(int), 0);
    Channel* s1 = chan_new(sizeof(int), 0);
    Channel* s2 = chan_new(sizeof(int), 0);

    check("merge ok", chan_merge(dest, (Channel*[]){s1, s2}, 2) == (int)CHAN_OK);

    int a = 10, b = 20;
    chan_send(s1, &a);
    chan_send(s2, &b);
    chan_close(s1);
    chan_close(s2);

    /* Dest receives both values (order between sources unspecified), then
     * observes close once both forwarders finish. */
    int got10 = 0, got20 = 0;
    int v;
    while (chan_recv(dest, &v)) {
        if (v == 10) got10 = 1;
        if (v == 20) got20 = 1;
    }
    check("merge delivered s1 value", got10);
    check("merge delivered s2 value", got20);
    check("dest closed after sources", chan_try_recv(dest, &v) == (int)CHAN_CLOSED);

    /* Bad args */
    check("merge rejects NULL source", chan_merge(dest, (Channel*[]){NULL}, 1) == (int)CHAN_INVALID);
    Channel* mismatched = chan_new(sizeof(double), 0);
    check("merge rejects size mismatch", chan_merge(dest, (Channel*[]){mismatched}, 1) == (int)CHAN_INVALID);
    chan_free(mismatched);

    chan_free(dest);
    chan_free(s1);
    chan_free(s2);
}

/* ====================================================================== */
/* Integration: workers fed by a merged channel                           */
/* ====================================================================== */

static atomic_llong merged_count;

static void merged_job(void* value, void* user) {
    (void)value;
    tally* t = (tally*)user;
    atomic_fetch_add(&merged_count, 1);
    (void)t;
}

static void* merge_feeder(void* p) {
    Channel* src = (Channel*)p;
    for (int i = 0; i < 500; i++) {
        int v = i;
        chan_send(src, &v);
    }
    chan_close(src);
    return NULL;
}

static void test_integration_merge_into_workers(void) {
    printf(ANSI_YELLOW "\n=== integration: 2 sources -> merge -> 4 workers ===\n" ANSI_RESET);

    Channel* dest = chan_new(sizeof(int), 0);
    Channel* s1 = chan_new(sizeof(int), 0);
    Channel* s2 = chan_new(sizeof(int), 0);

    check("merge for integration", chan_merge(dest, (Channel*[]){s1, s2}, 2) == (int)CHAN_OK);

    tally t = {0};
    chan_workers_t* w = chan_workers_new(4, sizeof(int), 0, merged_job, &t);
    check("workers for integration", w != NULL);

    /* Feed both sources from their own threads */
    Thread t1, t2;
    check("feeders created", thread_create(&t1, merge_feeder, s1) == 0 && thread_create(&t2, merge_feeder, s2) == 0);

    /* Forwarder threads move s1/s2 into dest; main thread relays dest into
     * the worker pool until the merge closes dest. */
    int v;
    while (chan_recv(dest, &v)) {
        chan_workers_submit(w, &v);
    }
    chan_workers_shutdown(w);

    void* ret;
    thread_join(t1, &ret);
    thread_join(t2, &ret);

    check("all 1000 merged items processed", atomic_load(&merged_count) == 1000);

    chan_workers_free(w);
    chan_free(dest);
    chan_free(s1);
    chan_free(s2);
}

int main(void) {
    test_pubsub_basic();
    test_pubsub_close_and_full();
    test_workers_basic();
    test_workers_large_items();
    test_select_basic();
    test_select_skips_closed();
    test_merge();
    test_integration_merge_into_workers();

    printf("\n=== Summary ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n", g_passed + g_failed, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
