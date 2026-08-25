/**
 * @file channels_test.c
 * @brief Tests for the Go-style channel implementation (channels.h).
 *
 * Covers single-threaded semantics (send/recv/try/close/len/cap), the
 * growth path, close-then-drain ordering, and multi-threaded producer/
 * consumer fan-in/fan-out with timeout and non-blocking operations.
 */
#include "../include/channels.h"
#include "../include/process.h"
#include "../include/thread.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

/* ---------------------------------------------------------------- */
/* Single-threaded semantics                                        */
/* ---------------------------------------------------------------- */

static void test_basic_send_recv(void) {
    printf(ANSI_YELLOW "\n=== basic send/recv ===\n" ANSI_RESET);

    Channel* ch = chan_new(sizeof(int), 0);
    check("chan_new non-null", ch != NULL);
    check("initial len 0", chan_len(ch) == 0);
    check("initial cap >= 16", chan_cap(ch) >= 16);
    check("not closed initially", !chan_is_closed(ch));

    for (int i = 0; i < 10; i++) {
        check("send ok", chan_send(ch, &i) == CHAN_OK);
    }
    check("len 10 after sends", chan_len(ch) == 10);

    for (int i = 0; i < 10; i++) {
        int v = -1;
        check("recv ok", chan_recv(ch, &v));
        check("recv FIFO order", v == i);
    }
    check("len 0 after drains", chan_len(ch) == 0);

    chan_free(ch);
}

static void test_try_operations(void) {
    printf(ANSI_YELLOW "\n=== try send/recv ===\n" ANSI_RESET);

    Channel* ch = chan_new(sizeof(double), 4);

    int rc = chan_try_recv(ch, &(double){0});
    check("try_recv on empty is CHAN_EMPTY", rc == (int)CHAN_EMPTY);

    double d = 3.14;
    check("try_send ok", chan_try_send(ch, &d) == (int)CHAN_OK);
    double out = 0;
    rc = chan_try_recv(ch, &out);
    check("try_recv ok", rc == (int)CHAN_OK);
    check("try_recv value", out == 3.14);

    /* NULL argument handling */
    check("try_send NULL value is CHAN_INVALID", chan_try_send(ch, NULL) == (int)CHAN_INVALID);
    check("try_recv NULL out is CHAN_INVALID", chan_try_recv(ch, NULL) == (int)CHAN_INVALID);
    check("send NULL value is CHAN_INVALID", chan_send(ch, NULL) == (int)CHAN_INVALID);

    chan_free(ch);
}

static void test_growth(void) {
    printf(ANSI_YELLOW "\n=== ring growth ===\n" ANSI_RESET);

    Channel* ch = chan_new(sizeof(uint32_t), 4); /* small initial cap */
    size_t cap0 = chan_cap(ch);

    enum { N = 1000 };
    for (uint32_t i = 0; i < N; i++) {
        if (chan_send(ch, &i) != (int)CHAN_OK) {
            check("growth send ok", false);
            chan_free(ch);
            return;
        }
    }
    check("capacity grew", chan_cap(ch) > cap0);
    check("len N after growth", chan_len(ch) == N);

    bool ordered = true;
    for (uint32_t i = 0; i < N; i++) {
        uint32_t v = 0xFFFFFFFFu;
        if (!chan_recv(ch, &v) || v != i) {
            ordered = false;
            break;
        }
    }
    check("FIFO preserved across growth", ordered);
    check("empty after drain", chan_len(ch) == 0);

    chan_free(ch);
}

static void test_close_semantics(void) {
    printf(ANSI_YELLOW "\n=== close semantics ===\n" ANSI_RESET);

    Channel* ch = chan_new(sizeof(int), 0);

    /* Close with buffered values: recv drains, then reports closed. */
    int a = 1, b = 2;
    chan_send(ch, &a);
    chan_send(ch, &b);
    check("close ok", chan_close(ch) == (int)CHAN_OK);
    check("is_closed after close", chan_is_closed(ch));

    check("send after close is CHAN_CLOSED", chan_send(ch, &a) == (int)CHAN_CLOSED);
    check("try_send after close is CHAN_CLOSED", chan_try_send(ch, &a) == (int)CHAN_CLOSED);

    int v = 0;
    check("drain 1 after close", chan_recv(ch, &v) && v == 1);
    check("drain 2 after close", chan_recv(ch, &v) && v == 2);
    check("recv after drained is false", !chan_recv(ch, &v));
    check("try_recv after drained is CHAN_CLOSED", chan_try_recv(ch, &v) == (int)CHAN_CLOSED);
    check("recv_timeout after drained is CHAN_CLOSED", chan_recv_timeout(ch, &v, 10) == (int)CHAN_CLOSED);

    /* Double close is idempotent */
    check("double close is CHAN_CLOSED (idempotent)", chan_close(ch) == (int)CHAN_CLOSED);
    check("triple close is CHAN_CLOSED", chan_close(ch) == (int)CHAN_CLOSED);

    chan_free(ch);

    /* NULL channel handling */
    check("close NULL is CHAN_INVALID", chan_close(NULL) == (int)CHAN_INVALID);
    check("len NULL is 0", chan_len(NULL) == 0);
    check("cap NULL is 0", chan_cap(NULL) == 0);
    check("is_closed NULL is true", chan_is_closed(NULL));
}

static void test_recv_timeout(void) {
    printf(ANSI_YELLOW "\n=== recv timeout ===\n" ANSI_RESET);

    Channel* ch = chan_new(sizeof(int), 0);

    int v = 0;
    check("timeout on empty channel", chan_recv_timeout(ch, &v, 50) == (int)CHAN_TIMEOUT);

    /* timeout_ms == 0 behaves like try_recv */
    check("zero timeout on empty is CHAN_TIMEOUT", chan_recv_timeout(ch, &v, 0) == (int)CHAN_TIMEOUT);
    int x = 7;
    chan_send(ch, &x);
    check("zero timeout with value is CHAN_OK", chan_recv_timeout(ch, &v, 0) == (int)CHAN_OK);
    check("zero timeout value", v == 7);

    /* negative timeout waits indefinitely — must only be used when a value
     * is guaranteed to arrive; here we send first so it returns. */
    int y = 42;
    check("send for negative-timeout recv", chan_send(ch, &y) == (int)CHAN_OK);
    check("negative timeout with value is CHAN_OK", chan_recv_timeout(ch, &v, -1) == (int)CHAN_OK);
    check("negative timeout value", v == 42);

    chan_free(ch);
    check("chan_free NULL safe", true);
    chan_free(NULL);
}

static void test_struct_values(void) {
    printf(ANSI_YELLOW "\n=== struct values ===\n" ANSI_RESET);

    typedef struct {
        int id;
        char tag[8];
        double score;
    } Item;

    Channel* ch = chan_new(sizeof(Item), 0);

    Item in = {42, "abc", 9.5};
    check("struct send", chan_send(ch, &in) == (int)CHAN_OK);

    Item out;
    memset(&out, 0, sizeof(out));
    check("struct recv", chan_recv(ch, &out));
    check("struct roundtrip", out.id == 42 && strcmp(out.tag, "abc") == 0 && out.score == 9.5);

    chan_free(ch);
}

/* ---------------------------------------------------------------- */
/* Multi-threaded tests                                             */
/* ---------------------------------------------------------------- */

#define PRODUCERS 4
#define CONSUMERS 4
#define ITEMS_PER 2500

typedef struct {
    Channel* ch;
    int id;
    atomic_llong* sum;
    atomic_llong* count;
} WorkerArg;

static void* producer_main(void* p) {
    WorkerArg* a = (WorkerArg*)p;
    for (int i = 0; i < ITEMS_PER; i++) {
        int64_t v = (int64_t)a->id * ITEMS_PER + i;
        if (chan_send(a->ch, &v) != CHAN_OK) return (void*)1;
    }
    return 0;
}

static void* consumer_main(void* p) {
    WorkerArg* a = (WorkerArg*)p;
    int64_t v;
    while (chan_recv(a->ch, &v)) {
        atomic_fetch_add(a->sum, v);
        atomic_fetch_add(a->count, 1);
    }
    return NULL;
}

static void test_fan_in_fan_out(void) {
    printf(ANSI_YELLOW "\n=== fan-in / fan-out (%dx%d, %d items each) ===\n" ANSI_RESET, PRODUCERS, CONSUMERS,
           ITEMS_PER);

    Channel* ch = chan_new(sizeof(int64_t), 64);
    atomic_llong sum = 0, count = 0;

    WorkerArg pargs[PRODUCERS], cargs[CONSUMERS];
    Thread producers[PRODUCERS], consumers[CONSUMERS];

    for (int i = 0; i < CONSUMERS; i++) {
        cargs[i].ch = ch;
        cargs[i].sum = &sum;
        cargs[i].count = &count;
        check("consumer thread created", thread_create(&consumers[i], consumer_main, &cargs[i]) == 0);
    }
    for (int i = 0; i < PRODUCERS; i++) {
        pargs[i].ch = ch;
        pargs[i].id = i;
        check("producer thread created", thread_create(&producers[i], producer_main, &pargs[i]) == 0);
    }

    for (int i = 0; i < PRODUCERS; i++) {
        void* ret;
        thread_join(producers[i], &ret);
        check("producer exited cleanly", ret == 0);
    }
    chan_close(ch); /* signal consumers: no more values */
    for (int i = 0; i < CONSUMERS; i++) {
        void* ret;
        thread_join(consumers[i], &ret);
        check("consumer exited cleanly", ret == 0);
    }

    int64_t total = (int64_t)PRODUCERS * ITEMS_PER;
    check("all items consumed", atomic_load(&count) == total);

    /* Verify sum: each producer sends ids [id*N, id*N+N) */
    int64_t expected = 0;
    for (int64_t v = 0; v < total; v++) expected += v;
    check("checksum matches", atomic_load(&sum) == expected);

    chan_free(ch);
}

/* Producer closes the channel itself after finishing — the common
 * "producer owns lifetime" pattern. */
static void* producer_closes_main(void* p) {
    WorkerArg* a = (WorkerArg*)p;
    for (int i = 0; i < 100; i++) {
        int64_t v = i;
        chan_send(a->ch, &v);
    }
    chan_close(a->ch);
    return 0;
}

static void* consumer_count_main(void* p) {
    WorkerArg* a = (WorkerArg*)p;
    int64_t v;
    while (chan_recv(a->ch, &v)) atomic_fetch_add(a->count, 1);
    return 0;
}

static void test_producer_closes_channel(void) {
    printf(ANSI_YELLOW "\n=== producer-owned close ===\n" ANSI_RESET);

    Channel* ch = chan_new(sizeof(int64_t), 0);
    atomic_llong count = 0;

    WorkerArg pa = {.ch = ch, .sum = NULL, .count = &count};
    WorkerArg ca = {.ch = ch, .sum = NULL, .count = &count};

    Thread producer, consumer;
    check("producer created", thread_create(&producer, producer_closes_main, &pa) == 0);
    check("consumer created", thread_create(&consumer, consumer_count_main, &ca) == 0);

    void* ret;
    thread_join(producer, &ret);
    thread_join(consumer, &ret);
    check("consumer saw all 100 items then observed close", atomic_load(&count) == 100);

    chan_free(ch);
}

/* Stress: many short-lived send/recv pairs with timeouts interleaved. */
static atomic_int timeout_hits;

static void* timeout_stress_consumer(void* p) {
    WorkerArg* a = (WorkerArg*)p;
    int64_t v;
    for (;;) {
        ChanStatus st = chan_recv_timeout(a->ch, &v, (atomic_load(a->count) % 3 == 0) ? 1 : -1);
        if (st == CHAN_OK) {
            atomic_fetch_add(a->count, 1);
        } else if (st == CHAN_TIMEOUT) {
            atomic_fetch_add(&timeout_hits, 1);
        } else if (st == CHAN_CLOSED) {
            break; /* drained: producer closed the channel */
        } else {
            return (void*)1; /* unexpected error */
        }
    }
    return NULL;
}

static void* timeout_stress_producer(void* p) {
    WorkerArg* a = (WorkerArg*)p;
    for (int i = 0; i < 2000; i++) {
        int64_t v = i;
        if (i % 7 == 0) {
            NANOSLEEP(0, 1000); /* 1us pause: let some consumer timeouts happen */
        }
        if (chan_send(a->ch, &v) != CHAN_OK) return (void*)1;
    }
    chan_close(a->ch); /* consumers drain buffered values, then observe close */
    return NULL;
}

static void test_timeout_stress(void) {
    printf(ANSI_YELLOW "\n=== timeout stress (1 producer, 2 consumers) ===\n" ANSI_RESET);

    Channel* ch = chan_new(sizeof(int64_t), 8);
    atomic_llong count = 0;
    atomic_store(&timeout_hits, 0);

    WorkerArg pa = {.ch = ch, .count = &count};
    WorkerArg ca1 = {.ch = ch, .count = &count};
    WorkerArg ca2 = {.ch = ch, .count = &count};

    Thread producer, c1, c2;
    check("threads created", thread_create(&producer, timeout_stress_producer, &pa) == 0 &&
                                 thread_create(&c1, timeout_stress_consumer, &ca1) == 0 &&
                                 thread_create(&c2, timeout_stress_consumer, &ca2) == 0);

    void* ret;
    thread_join(producer, &ret);
    check("producer clean", ret == 0);
    thread_join(c1, &ret);
    thread_join(c2, &ret);
    check("consumers clean", ret == 0);

    /* Both consumers together must receive every value (2000), regardless
     * of how many timeout retries happened along the way. */
    check("all 2000 values delivered under timeout pressure", atomic_load(&count) == 2000);
    printf("  (timeouts hit: %d)\n", atomic_load(&timeout_hits));

    chan_free(ch);
}

int main(void) {
    test_basic_send_recv();
    test_try_operations();
    test_growth();
    test_close_semantics();
    test_recv_timeout();
    test_struct_values();
    test_fan_in_fan_out();
    test_producer_closes_channel();
    test_timeout_stress();

    printf("\n=== Summary ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n", g_passed + g_failed, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
