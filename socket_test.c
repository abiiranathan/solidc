#define SOCKET_IMPLEMENTATION
#include "socket.h"

static void init_afinet_addr(struct sockaddr_in* addr, int port) {
    addr->sin_family      = AF_INET;
    addr->sin_port        = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;  // inet_addr("127.0.0.1")
}

// On windows link with:  -lws2_32
// x86_64-w64-mingw32-gcc socket_test.c -lws2_32
int main() {
#ifdef _WIN32
    initialize_winsock();
#endif
    Socket* s = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == NULL) {
        fprintf(stderr, "unable to create socket\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    init_afinet_addr(&addr, 8080);

    int enable = 1;
    socket_reuse_port(s, enable);

    int ret;
    ret = socket_bind(s, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
        fprintf(stderr, "unable to bind socket\n");
        exit(EXIT_FAILURE);
    }

    ret = socket_listen(s, 3);
    if (ret == -1) {
        fprintf(stderr, "unable to listen on socket\n");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connections on port: %d\n", ntohs(addr.sin_port));
    while (1) {
        socklen_t len         = sizeof addr;
        Socket* client_socket = socket_accept(s, (struct sockaddr*)&addr, &len);
        if (client_socket == NULL) {
            fprintf(stderr, "error accepting client socket\n");
            continue;
        }

        char buffer[1024] = {0};
        ssize_t valread   = socket_read(client_socket, buffer, 1024);
        if (valread == EOF) {
            printf("error reading\n");
            socket_close(client_socket);
            continue;
        }

        printf("%s\n", buffer);
        const char* hello = "Hello from server\n";
        socket_write(client_socket, hello, strlen(hello));
        socket_close(client_socket);
    }

    socket_close(s);
#ifdef _WIN32
    cleanup_winsock();
#endif
}
