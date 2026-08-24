#include "../include/socket.h"
#include "../include/macros.h"
#include "../include/thread.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void init_afinet_addr(struct sockaddr_in* addr, uint16_t port) {
    addr->sin_family      = AF_INET;
    addr->sin_port        = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;  // inet_addr("127.0.0.1")
}

void handle_client(void* client_socket_ptr) {
    Socket* client_socket = (Socket*)client_socket_ptr;

    char buffer[1024];
    ssize_t bytes_read = socket_recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Received message: %s\n", buffer);
        ASSERT(strcmp(buffer, "Hello, world!") == 0);
    }

    socket_close(client_socket);
}

void* send_message_to_server(void* arg) {
    uint16_t port = *(uint16_t*)arg;
    sleep_ms(1000);  // wait for server to start

    Socket* s = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT(s != NULL);

    struct sockaddr_in addr;
    init_afinet_addr(&addr, port);

    ASSERT(socket_connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    const char* message = "Hello, world!";
    ASSERT(socket_send(s, message, strlen(message), 0) == (ssize_t)strlen(message));

    socket_close(s);
    return NULL;
}

/* =========================================================================
 * Regression tests for hardened error paths
 * ====================================================================== */

/* Invalid IPs must return NULL, not silently produce garbage addresses. */
void test_invalid_addresses_rejected(void) {
    ASSERT(socket_ipv4_address(NULL, 80) == NULL);
    ASSERT(socket_ipv4_address("not an ip", 80) == NULL);
    ASSERT(socket_ipv4_address("999.999.999.999", 80) == NULL);
    ASSERT(socket_ipv4_address("1.2.3", 80) == NULL);

    /* Valid parse must round-trip. */
    struct sockaddr_in* a = socket_ipv4_address("127.0.0.1", 8080);
    ASSERT(a != NULL);
    ASSERT(a->sin_family == AF_INET);
    ASSERT(ntohs(a->sin_port) == 8080);
    ASSERT(ntohl(a->sin_addr.s_addr) == 0x7F000001u);
    free(a);

    ASSERT(socket_ipv6_address(NULL, 80) == NULL);
    ASSERT(socket_ipv6_address("gggg::1", 80) == NULL);

    struct sockaddr_in6* b = socket_ipv6_address("::1", 9090);
    ASSERT(b != NULL);
    ASSERT(b->sin6_family == AF_INET6);
    ASSERT(ntohs(b->sin6_port) == 9090);
    unsigned char expected[16] = {0};
    expected[15] = 1;
    ASSERT(memcmp(&b->sin6_addr, expected, 16) == 0);
    free(b);

    printf("invalid-address rejection passed\n");
}

/* NULL-socket guards across the API. */
void test_null_guards(void) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    socklen_t slen = sizeof(sa);
    int val = 1;

    ASSERT_EQ(socket_bind(NULL, (struct sockaddr*)&sa, sizeof(sa)), -1);
    ASSERT_EQ(socket_listen(NULL, 5), -1);
    ASSERT_EQ(socket_listen((Socket*)0x1, -1), -1); /* negative backlog */
    ASSERT_EQ(socket_accept(NULL, NULL, NULL), NULL);
    ASSERT_EQ(socket_connect(NULL, (struct sockaddr*)&sa, sizeof(sa)), -1);
    ASSERT_EQ(socket_recv(NULL, &val, sizeof(val), 0), -1);
    ASSERT_EQ(socket_send(NULL, &val, sizeof(val), 0), -1);
    ASSERT_EQ(socket_get_option(NULL, SOL_SOCKET, SO_REUSEADDR, &val, &slen), -1);
    ASSERT_EQ(socket_set_option(NULL, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)), -1);
    ASSERT_EQ(socket_get_address(NULL, (struct sockaddr*)&sa, &slen), -1);
    ASSERT_EQ(socket_get_peer_address(NULL, (struct sockaddr*)&sa, &slen), -1);
    ASSERT_EQ(socket_type(NULL), -1);
    ASSERT_EQ(socket_family(NULL), -1);
    ASSERT_EQ(socket_set_non_blocking(NULL, 1), -1);
    ASSERT_EQ(socket_fd(NULL), -1);
    ASSERT_EQ(socket_close(NULL), -1);
    ASSERT_EQ(socket_reuse_port(NULL, 1), -1);

    printf("NULL guard test passed\n");
}

/* bind() failures must be reported, not swallowed (Bug: ret=0 after perror). */
void test_bind_failure_reported(void) {
    Socket* s = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT(s != NULL);

    /* Port 1 (tcpmux) requires root; binding there must fail and be
     * reported as failure — the old code returned 0 here. */
    struct sockaddr_in bad;
    memset(&bad, 0, sizeof(bad));
    bad.sin_family      = AF_INET;
    bad.sin_port        = htons(1);
    bad.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int rc              = socket_bind(s, (struct sockaddr*)&bad, sizeof(bad));
    if (geteuid() != 0) { ASSERT(rc != 0); }

    socket_close(s);
    printf("bind-failure reporting passed\n");
}

// add -lws2_32 to the linker flags on Windows
int main() {
    socket_initialize();

    test_invalid_addresses_rejected();
    test_null_guards();
    test_bind_failure_reported();

    Socket* s = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT(s != NULL);

    uint16_t port = 9999;
    struct sockaddr_in addr;
    init_afinet_addr(&addr, port);

    int enable = 1;
    socket_reuse_port(s, enable);

    ASSERT(socket_bind(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    ASSERT(socket_listen(s, 10) == 0);

    struct sockaddr_in addr2;
    socklen_t len = sizeof addr2;
    ASSERT(socket_get_address(s, (struct sockaddr*)&addr2, &len) == 0);
    ASSERT(addr.sin_family == addr2.sin_family);
    ASSERT(addr.sin_port == addr2.sin_port);
    ASSERT(addr.sin_addr.s_addr == addr2.sin_addr.s_addr);

    // start a thread that will send message to server after 5 seconds
    Thread t = {0};
    thread_create(&t, send_message_to_server, &port);

    // wait for a client to connect
    Socket* client_socket = socket_accept(s, (struct sockaddr*)&addr2, &len);
    ASSERT(client_socket != NULL);

    // handle the client in a separate thread
    handle_client(client_socket);

    thread_join(t, NULL);

    socket_close(s);
    socket_cleanup();

    printf("Socket test passed\n");
    return 0;
}
