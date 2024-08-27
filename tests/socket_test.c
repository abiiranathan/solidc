#include "../include/socket.h"
#include "../include/thread.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void init_afinet_addr(struct sockaddr_in* addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;  // inet_addr("127.0.0.1")
}

void handle_client(void* client_socket_ptr) {
    Socket* client_socket = (Socket*)client_socket_ptr;

    char buffer[1024];
    ssize_t bytes_read = socket_recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Received message: %s\n", buffer);
        assert(strcmp(buffer, "Hello, world!") == 0);
    }

    socket_close(client_socket);
}

void* send_message_to_server(void* arg) {
    int port = *(int*)arg;
    sleep_ms(1000);  // wait for server to start

    Socket* s = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(s != NULL);

    struct sockaddr_in addr;
    init_afinet_addr(&addr, port);

    assert(socket_connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    const char* message = "Hello, world!";
    assert(socket_send(s, message, strlen(message), 0) == (ssize_t)strlen(message));

    socket_close(s);
    return NULL;
}

// add -lws2_32 to the linker flags on Windows
int main() {
    socket_initialize();

    Socket* s = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(s != NULL);

    int port = 9999;
    struct sockaddr_in addr;
    init_afinet_addr(&addr, port);

    int enable = 1;
    socket_reuse_port(s, enable);

    assert(socket_bind(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    assert(socket_listen(s, 10) == 0);

    struct sockaddr_in addr2;
    socklen_t len = sizeof addr2;
    assert(socket_get_address(s, (struct sockaddr*)&addr2, &len) == 0);
    assert(addr.sin_family == addr2.sin_family);
    assert(addr.sin_port == addr2.sin_port);
    assert(addr.sin_addr.s_addr == addr2.sin_addr.s_addr);

    // start a thread that will send message to server after 5 seconds
    Thread t;
    thread_create(&t, send_message_to_server, &port);

    // wait for a client to connect
    Socket* client_socket = socket_accept(s, (struct sockaddr*)&addr2, &len);
    assert(client_socket != NULL);

    // handle the client in a separate thread
    handle_client(client_socket);

    thread_join(t, NULL);

    socket_close(s);
    socket_cleanup();

    printf("Socket test passed\n");
    return 0;
}