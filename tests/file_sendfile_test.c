/**
 * @file file_sendfile_test.c
 * @brief Tests for the cross-platform file_sendfile() wrapper.
 *
 * Writes a temp file, sends it over a loopback socket, verifies the bytes
 * received match. Also covers partial sends and offset tracking.
 */
#include "../include/file.h"
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

static int make_listener(Socket** out) {
    Socket* s = socket_create(AF_INET, SOCK_STREAM, 0);
    if (!s) return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (socket_bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0 || socket_listen(s, 4) != 0) {
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

/** Writes pattern bytes to a temp file; returns fd or -1. */
static int make_temp_file(const char* path, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    for (size_t i = 0; i < size; i++) {
        unsigned char byte = (unsigned char)(i & 0xFF);
        fwrite(&byte, 1, 1, f);
    }
    fclose(f);
    FILE* rf = fopen(path, "rb");
    if (!rf) return -1;
    int fd = fileno(rf);
    return fd; /* leak rf deliberately: fd must stay open for the test */
}

int main(void) {
    const char* path = "sendfile_test.tmp";
    const size_t FILE_SIZE = 512 * 1024; /* 512 KB: multiple kernel chunks */

    printf(ANSI_YELLOW "\n=== sendfile round-trip (%zu KB) ===\n" ANSI_RESET, FILE_SIZE / 1024);

    int file_fd = make_temp_file(path, FILE_SIZE);
    check("temp file created", file_fd >= 0);

    Socket* listener = NULL;
    int port = make_listener(&listener);
    check("listener created", port > 0);

    Socket* client = connect_to(port);
    Socket* conn = socket_accept(listener, NULL, NULL);
    check("connection established", client && conn);

    int sock_fd = socket_fd(conn);
    int64_t offset = 0;
    int64_t total_sent = 0;
    int rc;

    /* Loop until EOF: exercises partial sends naturally */
    while ((rc = (int)file_sendfile(sock_fd, file_fd, &offset, 256 * 1024)) > 0) {
        total_sent += rc;
    }
    check("sendfile loop ended with 0 (EOF)", rc == 0);
    check("all bytes sent", total_sent == (int64_t)FILE_SIZE);
    check("offset tracked to EOF", offset == (int64_t)FILE_SIZE);

    /* Verify at the client */
    char* recv_buf = (char*)malloc(FILE_SIZE + 1);
    size_t received = 0;
    while (received < FILE_SIZE) {
        ssize_t r = socket_recv(client, recv_buf + received, FILE_SIZE - received, 0);
        if (r <= 0) break;
        received += (size_t)r;
    }
    check("client received all bytes", received == FILE_SIZE);

    /* Verify content pattern */
    bool content_ok = true;
    for (size_t i = 0; i < received; i++) {
        if ((unsigned char)recv_buf[i] != (unsigned char)(i & 0xFF)) {
            content_ok = false;
            printf("  mismatch at byte %zu: got %d want %d\n", i, recv_buf[i], (int)(i & 0xFF));
            break;
        }
    }
    check("content pattern matches", content_ok);

    /* Partial send from a non-zero offset */
    int64_t off2 = FILE_SIZE / 2;
    int64_t sent2 = file_sendfile(sock_fd, file_fd, &off2, FILE_SIZE / 4);
    check("partial send from mid-file", sent2 == FILE_SIZE / 4);
    check("offset advanced correctly", off2 == FILE_SIZE / 2 + FILE_SIZE / 4);

    /* Bad args */
    int64_t bad_off = 0;
    check("count 0 is EINVAL", file_sendfile(sock_fd, file_fd, &bad_off, 0) == -1);
    check("NULL offset is EINVAL", file_sendfile(sock_fd, file_fd, NULL, 100) == -1);

    free(recv_buf);
    socket_close(client);
    socket_close(conn);
    socket_close(listener);
    close(file_fd);
    remove(path);

    printf("\n=== Summary ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n", g_passed + g_failed, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
