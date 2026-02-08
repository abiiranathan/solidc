#include "../include/socket.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static void printLastErrorMessage(const char* prefix) {
    LPSTR errorText = NULL;
    // create format flags
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

    FormatMessageA(flags, NULL, (DWORD)WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorText,
                   0, NULL);

    if (errorText != NULL) {
        fprintf(stderr, "%s failed with error %d: %s\n", prefix, WSAGetLastError(), errorText);
        LocalFree(errorText);
    } else {
        fprintf(stderr, "%s failed with unknown error %d\n", prefix, WSAGetLastError());
    }
}
#endif

#ifdef _WIN32
void socket_initialize() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void socket_cleanup() { WSACleanup(); }
#else
void socket_initialize(void) {
    // do nothing
}
void socket_cleanup(void) {
    // do nothing
}
#endif

// Create a socket
Socket* socket_create(int domain, int type, int protocol) {
    Socket* sock = (Socket*)malloc(sizeof(Socket));
    if (!sock) {
        perror("malloc");
        return NULL;
    }
#ifdef _WIN32
    sock->handle = socket(domain, type, protocol);
    if (sock->handle == INVALID_SOCKET) {
        free(sock);
        // print the error message
        printLastErrorMessage("socket");
        return NULL;
    }
#else
    sock->handle = socket(domain, type, protocol);
    if (sock->handle == -1) {
        free(sock);
        return NULL;
    }
#endif
    return sock;
}

// Close a socket and free the memory
int socket_close(Socket* sock) {
    int ret = -1;
    if (sock) {
#ifdef _WIN32
        ret = closesocket(sock->handle);
#else
        ret = close(sock->handle);
#endif
        free(sock);
    }
    return ret;
}

// Bind a socket to an address
int socket_bind(Socket* sock, const struct sockaddr* addr, socklen_t addrlen) {
    int ret = -1;
#ifdef _WIN32
    if ((ret = bind(sock->handle, addr, addrlen)) != 0) {
        printLastErrorMessage("bind");
    }
#else
    ret = bind(sock->handle, addr, addrlen);
    if (ret != 0) {
        perror("bind");
        ret = 0;
    }
#endif
    return ret;
}

int socket_listen(Socket* sock, int backlog) { return listen(sock->handle, backlog); }

// Accept an incoming connection
Socket* socket_accept(Socket* sock, struct sockaddr* addr, socklen_t* addrlen) {
    Socket* client = (Socket*)malloc(sizeof(Socket));
    if (!client) {
        perror("malloc");
        return NULL;
    }
#ifdef _WIN32
    client->handle = accept(sock->handle, addr, addrlen);
    if (client->handle == INVALID_SOCKET) {
        free(client);
        printLastErrorMessage("accept");
        return NULL;
    }
#else
    client->handle = accept(sock->handle, addr, addrlen);
    if (client->handle == -1) {
        perror("accept");
        free(client);
        return NULL;
    }
#endif
    return client;
}

// Connect to a remote socket
int socket_connect(Socket* sock, const struct sockaddr* addr, socklen_t addrlen) {
    int ret = -1;
    if (connect(sock->handle, addr, addrlen) == 0) {
        ret = 0;
    } else {
        perror("connect");
    }
    return ret;
}

/*
Read from a socket. Returns the number of bytes read, or -1 on error.
buffer: Points to a buffer where the message should be stored.
size: Specifies the length in bytes of the buffer pointed to by the buffer
argument.

flags: Specifies the type of message reception.
See man 2 recv for more information.
 */
ssize_t socket_recv(Socket* sock, void* buffer, size_t size, int flags) {
    ssize_t bytes = 0;
    bytes = recv(sock->handle, buffer, size, flags);
    return bytes;
}

/* Write to a socket. Returns the number of bytes written, or -1 on error.

buffer: Points to the buffer containing the message to send.
length: Specifies the length of the message in bytes.
flags:  Specifies the type of message transmission
See man 2 send for more information.
*/
ssize_t socket_send(Socket* sock, const void* buffer, size_t size, int flags) {
    return send(sock->handle, buffer, size, flags);
}

// Get the socket file descriptor
int socket_fd(Socket* sock) { return sock->handle; }

int socket_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void socket_strerror(int err, char* buffer, size_t size) {
#ifdef _WIN32
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, (DWORD)err, 0, buffer, size, NULL);
#else
#if defined(__GLIBC__) || defined(__linux__)
    // GNU version returns char*
    char* msg = strerror_r(err, buffer, size);
    (void)msg;
#else
    // POSIX version (macOS, BSD) returns int
    strerror_r(err, buffer, size);
#endif
#endif
}

// Get the socket option
int socket_get_option(Socket* sock, int level, int optname, void* optval, socklen_t* optlen) {
    int ret = -1;
#ifdef _WIN32
    ret = getsockopt(sock->handle, level, optname, (char*)optval, optlen);
#else
    ret = getsockopt(sock->handle, level, optname, optval, optlen);
#endif
    return ret;
}

// Set the socket option
int socket_set_option(Socket* sock, int level, int optname, const void* optval, socklen_t optlen) {
    int ret = -1;

#ifdef _WIN32
    ret = setsockopt(sock->handle, level, optname, (const char*)optval, optlen);
#else
    ret = setsockopt(sock->handle, level, optname, optval, optlen);
#endif
    return ret;
}

int socket_reuse_port(Socket* sock, int enable) {
#ifdef _WIN32
    // Enable SO_REUSEADDR
    if (setsockopt(sock->handle, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(int)) == SOCKET_ERROR) {
        perror("setsockopt");
        fprintf(stderr, "setsockopt SO_REUSEADDR failed\n");
        return 1;
    }

    // Enable SO_EXCLUSIVEADDRUSE
    if (setsockopt(sock->handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&enable, sizeof(int)) == SOCKET_ERROR) {
        perror("setsockopt");
        fprintf(stderr, "setsockopt SO_EXCLUSIVEADDRUSE failed\n");
        return 1;
    }
    return 0;
#else
    int ret = 0;
    // Always set SO_REUSEADDR
    if (setsockopt(sock->handle, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) {
        ret = -1;
    }

// SO_REUSEPORT is available on Linux and modern BSD/macOS
#ifdef SO_REUSEPORT
    if (setsockopt(sock->handle, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) != 0) {
        ret = -1;
    }
#endif

    return ret;
#endif
}

// Get the socket address
int socket_get_address(Socket* sock, struct sockaddr* addr, socklen_t* addrlen) {
    int ret = -1;
#ifdef _WIN32
    ret = getsockname(sock->handle, addr, addrlen);
#else
    ret = getsockname(sock->handle, addr, addrlen);
#endif
    return ret;
}

// Get the socket peer address
int socket_get_peer_address(Socket* sock, struct sockaddr* addr, socklen_t* addrlen) {
    return getpeername(sock->handle, addr, addrlen);
}

int socket_type(Socket* sock) {
    int ret = -1;
    int type = 0;
    socklen_t len = sizeof(type);
    if (socket_get_option(sock, SOL_SOCKET, SO_TYPE, &type, &len) == 0) {
        ret = type;
    }
    return ret;
}

int socket_family(Socket* sock) {
    int domain = -1;
    socklen_t len = sizeof(domain);
#ifdef _WIN32
    domain = socket_get_option(sock, SOL_SOCKET, SO_TYPE, &domain, &len);
#elif defined(__APPLE__)
    (void)len;
    // macOS doesn't support SO_DOMAIN, use getsockname instead
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(sock->handle, (struct sockaddr*)&addr, &addr_len) == 0) {
        domain = addr.ss_family;
    }
#else
    socket_get_option(sock, SOL_SOCKET, SO_DOMAIN, &domain, &len);
#endif
    return domain;
}

// set non-blocking mode
int socket_set_non_blocking(Socket* sock, int enable) {
#ifdef _WIN32
    u_long mode = enable ? 1 : 0;
    return ioctlsocket(sock->handle, FIONBIO, &mode);
#else
    int flags = fcntl(sock->handle, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }
    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(sock->handle, F_SETFL, flags);
#endif
}

// Create an IPv4 address
// Allocates a new sockaddr_in and set the address and port.
struct sockaddr_in* socket_ipv4_address(const char* ip, uint16_t port) {
    struct sockaddr_in* addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    if (!addr) {
        perror("malloc");
        return NULL;
    }
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = inet_addr(ip);
    return addr;
}

// Create an IPv6 address
// Allocates a new sockaddr_in6 and set the address and port.
struct sockaddr_in6* socket_ipv6_address(const char* ip, uint16_t port) {
    struct sockaddr_in6* addr = (struct sockaddr_in6*)malloc(sizeof(struct sockaddr_in6));
    if (!addr) {
        perror("malloc");
        return NULL;
    }
    memset(addr, 0, sizeof(struct sockaddr_in6));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    inet_pton(AF_INET6, ip, &addr->sin6_addr);
    return addr;
}
