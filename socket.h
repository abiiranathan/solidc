#ifndef SOCKET_H
#define SOCKET_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <mswsock.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// Socket abstraction that works on both Windows and Unix
// On windows, you need to call initialize_winsock() before using any socket functions
// and cleanup_winsock() after you are done using the socket functions.
typedef struct {
#ifdef _WIN32
    SOCKET handle;
#else
    int handle;
#endif
} Socket;

#ifdef _WIN32
static void printLastErrorMessage(const char* prefix);
void initialize_winsock();
void cleanup_winsock();
#endif

// Create a socket
Socket* socket_create(int domain, int type, int protocol);
// Close a socket
int socket_close(Socket* sock);
// Bind a socket to an address
int socket_bind(Socket* sock, const struct sockaddr* addr, socklen_t addrlen);
// Listen for incoming connections
int socket_listen(Socket* sock, int backlog);
// Accept an incoming connection
Socket* socket_accept(Socket* sock, struct sockaddr* addr, socklen_t* addrlen);
// Connect to a remote socket
int socket_connect(Socket* sock, const struct sockaddr* addr, socklen_t addrlen);
// Read from a socket
ssize_t socket_read(Socket* sock, void* buffer, size_t size);
// Write to a socket
ssize_t socket_write(Socket* sock, const void* buffer, size_t size);
// Get the socket file descriptor
int socket_fd(Socket* sock);
// Get the socket error
int socket_error();
// Get the socket option
int socket_get_option(Socket* sock, int level, int optname, void* optval, socklen_t* optlen);
// Set the socket option (SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
int socket_reuse_port(Socket* sock, int enable);
// Set the socket option
int socket_set_option(Socket* sock, int level, int optname, const void* optval, socklen_t optlen);
// Get the socket address
int socket_get_address(Socket* sock, struct sockaddr* addr, socklen_t* addrlen);
// Get the socket peer address
int socket_get_peer_address(Socket* sock, struct sockaddr* addr, socklen_t* addrlen);
// Get the socket type
int socket_type(Socket* sock);
// Get the socket protocol
int socket_protocol(Socket* sock);
// Get the socket family
int socket_family(Socket* sock);
// Get the socket type
int socket_type(Socket* sock);

#if defined(__cplusplus)
}
#endif

#ifdef SOCKET_IMPL
#include <errno.h>

#ifdef _WIN32
static void printLastErrorMessage(const char* prefix) {
    LPSTR errorText = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorText, 0,
        NULL);
    if (errorText != NULL) {
        fprintf(stderr, "%s failed with error %d: %s\n", prefix, WSAGetLastError(), errorText);
        LocalFree(errorText);
    } else {
        fprintf(stderr, "%s failed with unknown error %d\n", prefix, WSAGetLastError());
    }
}
#endif

// Create a socket
Socket* socket_create(int domain, int type, int protocol) {
    Socket* sock = (Socket*)malloc(sizeof(Socket));
    if (!sock) {
        perror("malloc");
        return NULL;
    }
    sock->handle = -1;
#ifdef _WIN32
    sock->handle = socket(domain, type, protocol);
    if (sock->handle == INVALID_SOCKET) {
        free(sock);
        // print the error message
        printLastErrorMessage("socket");
        return NULL;
    }
#else
    // Unix
    sock->handle = socket(domain, type, protocol);
    if (sock->handle == -1) {
        free(sock);
        return NULL;
    }
#endif
    return sock;
}

// Close a socket
int socket_close(Socket* sock) {
    int ret = -1;
    if (sock) {
#ifdef _WIN32
        // Windows
        if (closesocket(sock->handle) == 0) {
            ret = 0;
        }
#else
        // Unix
        if (close(sock->handle) == 0) {
            ret = 0;
        }
#endif
        free(sock);
    }
    return ret;
}

// Bind a socket to an address
int socket_bind(Socket* sock, const struct sockaddr* addr, socklen_t addrlen) {
    int ret = -1;
#ifdef _WIN32
    // Windows
    if (bind(sock->handle, addr, addrlen) == 0) {
        ret = 0;
    } else {
        printLastErrorMessage("bind");
    }
#else
    // Unix
    if (bind(sock->handle, addr, addrlen) == 0) {
        ret = 0;
    } else {
        perror("bind");
    }
#endif
    return ret;
}

// Listen for incoming connections
int socket_listen(Socket* sock, int backlog) {
    int ret = -1;
    // Unix
    if (listen(sock->handle, backlog) == 0) {
        ret = 0;
    } else {
        perror("listen");
    }
    return ret;
}

// Accept an incoming connection
Socket* socket_accept(Socket* sock, struct sockaddr* addr, socklen_t* addrlen) {
    Socket* client = (Socket*)malloc(sizeof(Socket));
    if (!client) {
        perror("malloc");
        return NULL;
    }
#ifdef _WIN32
    // Windows
    client->handle = accept(sock->handle, addr, addrlen);
    if (client->handle == INVALID_SOCKET) {
        free(client);
        printLastErrorMessage("accept");
        return NULL;
    }
#else
    // Unix
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
size: Specifies the length in bytes of the buffer pointed to by the buffer argument.

flags:
Specifies  the  type of message reception. Values of this argument are formed by logically OR'ing
zero or more of the following values:

    MSG_PEEK - Peeks at an incoming message. The data is treated as unread and the
next recv() or similar function shall still return this data.

    MSG_OOB  - Requests out-of-band data. The significance and semantics of
out-of-band data  are  proto‐ col-specific.

    MSG_WAITALL On SOCK_STREAM sockets this requests that the function block until the
full amount of data can  be  returned.
The  function may return the smaller amount of data if the
socket is a message-based socket, if a signal is caught,
if the connection is terminated, if
MSG_PEEK was specified, or if an error is pending for the socket.
 */
ssize_t socket_read(Socket* sock, void* buffer, size_t size, int flags) {
    ssize_t bytes;
    bytes = recv(sock->handle, buffer, size, flags);
    return bytes;
}

/* Write to a socket. Returns the number of bytes written, or -1 on error.

buffer: Points to the buffer containing the message to send.
length: Specifies the length of the message in bytes.
flags:  Specifies the type of message transmission.
Values of this argument are formed by logically  OR'ing zero or more of the following flags:

MSG_EOR - Terminates a record (if supported by the protocol).
MSG_OOB - Sends out-of-band data on sockets that support out-of-band
communications. The signif‐ icance and semantics of out-of-band data are protocol-specific.
MSG_NOSIGNAL  Requests not to send the SIGPIPE signal if an attempt to send is
made on a stream-oriented socket that is no longer connected.
The [EPIPE] error shall still be returned.
*/
ssize_t socket_write(Socket* sock, const void* buffer, size_t size, int flags) {
    ssize_t bytes;
    bytes = send(sock->handle, buffer, size, flags);
    return bytes;
}

// Get the socket file descriptor
int socket_fd(Socket* sock) {
    return sock->handle;
}

// Get the socket error. Returns the last error code.
// On Windows, it returns the code from WSAGetLastError().
// On Unix, it returns the value of errno.
int socket_error() {
#ifdef _WIN32
    // Windows
    return WSAGetLastError();
#else
    // Unix
    return errno;
#endif
}

// Get the socket error message as a string.
const char* socket_strerror(int err) {
#ifdef _WIN32
    // Windows
    static char buffer[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, buffer, sizeof(buffer), NULL);
    return buffer;
#else
    // Unix
    return strerror(err);
#endif
}

// Get the socket option
int socket_get_option(Socket* sock, int level, int optname, void* optval, socklen_t* optlen) {
    int ret = -1;
#ifdef _WIN32
    // Windows
    if (getsockopt(sock->handle, level, optname, (char*)optval, optlen) == 0) {
        ret = 0;
    }
#else
    // Unix
    if (getsockopt(sock->handle, level, optname, optval, optlen) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Set the socket option
int socket_set_option(Socket* sock, int level, int optname, const void* optval, socklen_t optlen) {
    int ret = -1;

#ifdef _WIN32
    // Windows
    if (setsockopt(sock->handle, level, optname, (const char*)optval, optlen) == 0) {
        ret = 0;
    }
#else
    // Unix
    if (setsockopt(sock->handle, level, optname, optval, optlen) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Call this function be4 binding the socket.
int socket_reuse_port(Socket* sock, int enable) {
#ifdef _WIN32
    // Enable SO_REUSEADDR
    if (setsockopt(sock->handle, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(int)) ==
        SOCKET_ERROR) {
        fprintf(stderr, "setsockopt SO_REUSEADDR failed\n");
        return 1;
    }

    // Enable SO_EXCLUSIVEADDRUSE
    if (setsockopt(sock->handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&enable, sizeof(int)) ==
        SOCKET_ERROR) {
        fprintf(stderr, "setsockopt SO_EXCLUSIVEADDRUSE failed\n");
        return 1;
    }

#else
    return setsockopt(sock->handle, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &enable, sizeof(int));
#endif
}

// Get the socket address
int socket_get_address(Socket* sock, struct sockaddr* addr, socklen_t* addrlen) {
    int ret = -1;
#ifdef _WIN32
    // Windows
    if (getsockname(sock->handle, addr, addrlen) == 0) {
        ret = 0;
    }
#else
    // Unix
    if (getsockname(sock->handle, addr, addrlen) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Get the socket peer address
int socket_get_peer_address(Socket* sock, struct sockaddr* addr, socklen_t* addrlen) {
    int ret = -1;
    // Windows
    if (getpeername(sock->handle, addr, addrlen) == 0) {
        ret = 0;
    }
    return ret;
}

// Get the socket type
int socket_type(Socket* sock) {
    int ret = -1;
    int type;
    socklen_t len = sizeof(type);
    if (socket_get_option(sock, SOL_SOCKET, SO_TYPE, &type, &len) == 0) {
        ret = type;
    }
    return ret;
}

// Get the socket domain or type(address family, SOCK_STREAM or SOCK_DGRAM, for example).)
int socket_family(Socket* sock) {
    int domain    = 0;
    socklen_t len = sizeof(domain);
#ifdef _WIN32
    if (socket_get_option(sock, SOL_SOCKET, SO_TYPE, &domain, &len) == 0) {
        return domain;
    }
#else
    if (socket_get_option(sock, SOL_SOCKET, SO_DOMAIN, &domain, &len) == 0) {
        return domain;
    }
#endif
    return -1;  // Error
}

#ifdef _WIN32
void initialize_winsock() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void cleanup_winsock() {
    WSACleanup();
}
#endif

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

// epoll_create
#ifndef _WIN32  // epoll_create is not available on Windows
// Create an epoll instance. Returns the file descriptor of the new epoll instance, or -1 on error.
int socket_epoll_create() {
    return epoll_create(0);
}

/* socket_epoll_ctl is a wrapper around epoll_ctl.
 It adds a new file descriptor to the epoll instance.
 Returns 0 on success, or -1 on error.
 Usage:
 struct epoll_event event;
 socket_epoll_ctl(epoll_fd, sock_fd, &event, EPOLLIN);

A simple accept function using epoll:
 int Accept() {
  int client_fd;
  socklen_t client_len = sizeof(server_addr);
  client_fd = socket_accept(server_fd, (struct sockaddr *)&server_addr, &client_len);
  if (client_fd != -1) {
    nonblocking(client_fd);
    socket_epoll_ctl_add(epoll_fd, client_fd, &event, EPOLLIN | EPOLLET | EPOLLONESHOT);
  }
  return client_fd;
}
*/
int socket_epoll_ctl_add(int epoll_fd, int sock_fd, struct epoll_event* event, uint32_t events) {
    event->events  = events;
    event->data.fd = sock_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, event) == -1) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

// Modify an existing file descriptor in the epoll instance.
int socket_epoll_ctl_mod(int epoll_fd, int sock_fd, struct epoll_event* event, uint32_t events) {
    event->events  = events;
    event->data.fd = sock_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock_fd, event) == -1) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

// Remove a file descriptor from the epoll instance.
int socket_epoll_ctl_del(int epoll_fd, int sock_fd) {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, NULL) == -1) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

// Wait for events on an epoll instance.
// Returns the number of file descriptors ready for I/O, or -1
// on error. Timeout can be -1 to block indefinitely, or 0 to return immediately.
// Otherwise, it specifies the maximum number of milliseconds to wait.
int socket_epoll_wait(int epoll_fd, struct epoll_event* events, int maxevents, int timeout) {
    return epoll_wait(epoll_fd, events, maxevents, timeout);
}

#endif
// ================== END Socket related functions ==================

// ================== Socket address related functions ==================

// Create an IPv4 address
// Allocate a new sockaddr_in and set the address and port.
struct sockaddr_in* socket_ipv4_address(const char* ip, int port) {
    struct sockaddr_in* addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    if (!addr) {
        perror("malloc");
        return NULL;
    }
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family      = AF_INET;
    addr->sin_port        = htons(port);
    addr->sin_addr.s_addr = inet_addr(ip);
    return addr;
}

// Create an IPv6 address
// Allocate a new sockaddr_in6 and set the address and port.
struct sockaddr_in6* socket_ipv6_address(const char* ip, int port) {
    struct sockaddr_in6* addr = (struct sockaddr_in6*)malloc(sizeof(struct sockaddr_in6));
    if (!addr) {
        perror("malloc");
        return NULL;
    }
    memset(addr, 0, sizeof(struct sockaddr_in6));
    addr->sin6_family = AF_INET6;
    addr->sin6_port   = htons(port);
    inet_pton(AF_INET6, ip, &addr->sin6_addr);
    return addr;
}

// ================== END Socket address related functions ==================

#endif  // SOCKET_IMPL
#endif  // SOCKET_H
