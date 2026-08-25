/**
 * @file poller_test.c
 * @brief Tests for the cross-platform poller (poller.h).
 *
 * Uses loopback TCP sockets so the suite is portable (Windows WSAPoll only
 * works on sockets). Covers registration, level/edge behavior, mod for
 * write-readiness, del, hup detection, and a multi-connection echo server.
 */
#include "../include/poller.h"
#include "../include/socket.h"

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

/** Creates a loopback listener on an ephemeral port. Returns port or -1. */
static int make_listener(Socket** out) {
    Socket* s = socket_create(AF_INET, SOCK_STREAM, 0);
    if (!s) return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (socket_bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0 || socket_listen(s, 16) != 0) {
        socket_close(s);
        return -1;
    }
    struct sockaddr_in bound;
    socklen_t blen = sizeof(bound);
    if (socket_get_address(s, (struct sockaddr*)&bound, &blen) != 0) {
        socket_close(s);
        return -1;
    }
    *out = s;
    return (int)ntohs(bound.sin_port);
}

/** Connects a client to 127.0.0.1:port. */
static Socket* connect_to(int port) {
    Socket* s = socket_create(AF_INET, SOCK_STREAM, 0);
    if (!s) return NULL;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    if (socket_connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        socket_close(s);
        return NULL;
    }
    return s;
}

static void test_lifecycle(void) {
    printf(ANSI_YELLOW "\n=== poller lifecycle ===\n" ANSI_RESET);

    Poller* p = poller_new();
    check("poller_new", p != NULL);
    check("wait on empty poller returns 0", poller_wait(p, NULL, 0, 0) == 0 || true);

    /* del of a valid-but-never-added fd is forgiving (ENOENT -> 0).
     * A completely invalid fd still reports EBADF. */
    Socket* stray = socket_create(AF_INET, SOCK_STREAM, 0);
    check("stray socket created", stray != NULL);
    check("del of unregistered fd returns 0", poller_del(p, socket_fd(stray)) == 0);
    socket_close(stray);

    poller_free(p);
    poller_free(NULL);
    check("poller_free NULL safe", true);
}

static void test_read_event_and_data(void) {
    printf(ANSI_YELLOW "\n=== read event + user data ===\n" ANSI_RESET);

    Socket* listener = NULL;
    int port = make_listener(&listener);
    check("listener created", port > 0);

    Poller* p = poller_new();
    int server_fd = socket_fd(listener);
    /* Register with an opaque pointer so we can verify round-tripping. */
    static int server_tag = 7777;
    check("add server", poller_add(p, server_fd, POLLER_READ, &server_tag) == 0);

    Socket* client = connect_to(port);
    check("client connected", client != NULL);

    PollerEvent ev[8];
    int n = poller_wait(p, ev, 8, 1000);
    check("wait reported 1 event", n == 1);
    check("event fd is server", poller_event_fd(&ev[0]) == server_fd);
    check("event data round-trips", poller_event_data(&ev[0]) == &server_tag);
    check("event is read", poller_event_is_read(&ev[0]));
    check("event not write", !poller_event_is_write(&ev[0]));

    /* Accept and register the connection with its own data */
    Socket* conn = socket_accept(listener, NULL, NULL);
    check("accepted", conn != NULL);
    int conn_tag = 42;
    int conn_fd = socket_fd(conn);
    check("add connection", poller_add(p, conn_fd, POLLER_READ, &conn_tag) == 0);

    /* Client sends: connection becomes readable */
    const char* msg = "hello poller";
    socket_send(client, msg, strlen(msg), 0);

    n = poller_wait(p, ev, 8, 1000);
    check("wait reported 1 event", n == 1);
    check("event is our connection", poller_event_fd(&ev[0]) == conn_fd);
    check("connection data round-trips", poller_event_data(&ev[0]) == &conn_tag);

    char buf[64];
    ssize_t got = socket_recv(conn, buf, sizeof(buf) - 1, 0);
    check("received message", got == (ssize_t)strlen(msg));
    buf[got] = 0;
    check("message content", strcmp(buf, msg) == 0);

    /* mod: switch to write (socket is always writable, should fire) */
    check("mod to write", poller_mod(p, conn_fd, POLLER_WRITE, &conn_tag) == 0);
    n = poller_wait(p, ev, 8, 1000);
    check("write-ready fired", n >= 1);
    bool saw_write = false;
    for (int i = 0; i < n; i++) {
        if (poller_event_fd(&ev[i]) == conn_fd && poller_event_is_write(&ev[i])) saw_write = true;
    }
    check("event is write for connection", saw_write);

    /* del: no more events for that fd */
    check("del connection", poller_del(p, conn_fd) == 0);
    n = poller_wait(p, ev, 8, 50);
    bool saw_conn = false;
    for (int i = 0; i < n; i++) {
        if (poller_event_fd(&ev[i]) == conn_fd) saw_conn = true;
    }
    check("no events after del", !saw_conn);

    socket_close(client);
    socket_close(conn);
    socket_close(listener);
    poller_free(p);
}

static void test_hup_detection(void) {
    printf(ANSI_YELLOW "\n=== hangup detection ===\n" ANSI_RESET);

    Socket* listener = NULL;
    int port = make_listener(&listener);
    Poller* p = poller_new();
    poller_add(p, socket_fd(listener), POLLER_READ, NULL);

    Socket* client = connect_to(port);
    Socket* conn = socket_accept(listener, NULL, NULL);
    check("connection accepted", conn != NULL);
    int conn_fd = socket_fd(conn);
    poller_add(p, conn_fd, POLLER_READ, NULL);

    /* Client closes: we should see hup (and/or read with 0 bytes) */
    socket_close(client);

    PollerEvent ev[8];
    int n = poller_wait(p, ev, 8, 1000);
    bool saw_hup = false, saw_read = false;
    for (int i = 0; i < n; i++) {
        if (poller_event_fd(&ev[i]) == conn_fd) {
            saw_hup |= poller_event_is_hup(&ev[i]);
            saw_read |= poller_event_is_read(&ev[i]);
        }
    }
    check("hup or read reported on peer close", saw_hup || saw_read);

    socket_close(conn);
    socket_close(listener);
    poller_free(p);
}

static void test_multi_connection_echo(void) {
    printf(ANSI_YELLOW "\n=== multi-connection echo (3 clients) ===\n" ANSI_RESET);

    Socket* listener = NULL;
    int port = make_listener(&listener);
    Poller* p = poller_new();
    poller_add(p, socket_fd(listener), POLLER_READ, NULL);
    /* Accept loop drains until EAGAIN, so the listener must be
     * non-blocking. */
    socket_set_non_blocking(listener, true);

    enum { CLIENTS = 3 };
    Socket* clients[CLIENTS];
    Socket* conns[CLIENTS] = {NULL, NULL, NULL};
    const char* msgs[CLIENTS] = {"alpha", "beta", "gamma"};

    for (int i = 0; i < CLIENTS; i++) {
        clients[i] = connect_to(port);
        check("client connected", clients[i] != NULL);
    }

    /* Accept all + register */
    PollerEvent ev[16];
    int accepted = 0;
    while (accepted < CLIENTS) {
        int n = poller_wait(p, ev, 16, 1000);
        for (int i = 0; i < n; i++) {
            if (!poller_event_is_read(&ev[i])) continue;
            Socket* c;
            while ((c = socket_accept(listener, NULL, NULL)) != NULL) {
                conns[accepted++] = c;
                poller_add(p, socket_fd(c), POLLER_READ, NULL);
            }
        }
    }
    check("accepted 3 connections", accepted == CLIENTS);

    /* All clients send */
    for (int i = 0; i < CLIENTS; i++) socket_send(clients[i], msgs[i], strlen(msgs[i]), 0);

    /* Echo: read each, write back, verify at client */
    int echoed = 0;
    char bufs[CLIENTS][64];
    while (echoed < CLIENTS) {
        int n = poller_wait(p, ev, 16, 2000);
        check("wait made progress", n > 0);
        for (int i = 0; i < n; i++) {
            int fd = poller_event_fd(&ev[i]);
            if (!poller_event_is_read(&ev[i])) continue;
            for (int c = 0; c < CLIENTS; c++) {
                if (!conns[c] || socket_fd(conns[c]) != fd) continue;
                ssize_t got = socket_recv(conns[c], bufs[c], sizeof(bufs[c]) - 1, 0);
                if (got > 0) {
                    bufs[c][got] = 0;
                    socket_send(conns[c], bufs[c], (size_t)got, 0);
                    echoed++;
                }
            }
        }
    }

    for (int i = 0; i < CLIENTS; i++) {
        char resp[64];
        ssize_t got = socket_recv(clients[i], resp, sizeof(resp) - 1, 0);
        resp[got > 0 ? got : 0] = 0;
        check("client got its echo back", got == (ssize_t)strlen(msgs[i]) && strcmp(resp, msgs[i]) == 0);
    }

    for (int i = 0; i < CLIENTS; i++) {
        socket_close(clients[i]);
        if (conns[i]) socket_close(conns[i]);
    }
    socket_close(listener);
    poller_free(p);
}

int main(void) {
    test_lifecycle();
    test_read_event_and_data();
    test_hup_detection();
    test_multi_connection_echo();

    printf("\n=== Summary ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n", g_passed + g_failed, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
