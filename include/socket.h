#ifndef SOCKET_H
#define SOCKET_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

// for SO_REUSEPORT
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _WIN32
#include <mswsock.h>
#include <windows.h>   // Include after winsock2.h
#include <winsock2.h>  // Must be first on Windows
#include <ws2tcpip.h>
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
// On windows, you need to call initialize_winsock() before using any socket
// functions and cleanup_winsock() after you are done using the socket
// functions.
typedef struct {
#ifdef _WIN32
    SOCKET handle;
#else
    int handle;
#endif
} Socket;

// Initialize winsock on Windows. Does nothing on Unix.
void socket_initialize(void);

// Cleanup winsock on Windows. Does nothing on Unix.
void socket_cleanup(void);

// Create a socket, returns NULL on error.
Socket* socket_create(int domain, int type, int protocol) __attribute__((warn_unused_result));

// Close a socket and free the memory.
int socket_close(Socket* sock);

// Bind a socket to an address.
// Returns 0 on success, -1 on error.
int socket_bind(Socket* sock, const struct sockaddr* addr, socklen_t addrlen);

// Listen for incoming connections. The backlog parameter is the maximum length
// of the queue of pending connections. If the queue is full, the client will
// receive an ECONNREFUSED error. Returns 0 on success, -1 on error.
int socket_listen(Socket* sock, int backlog);

// Accept an incoming connection
Socket* socket_accept(Socket* sock, struct sockaddr* addr, socklen_t* addrlen) __attribute__((warn_unused_result));

// Connect to a remote socket
int socket_connect(Socket* sock, const struct sockaddr* addr, socklen_t addrlen);

// Read from a socket
ssize_t socket_recv(Socket* sock, void* buffer, size_t size, int flags);

// Write to a socket
ssize_t socket_send(Socket* sock, const void* buffer, size_t size, int flags);

// Get the socket file descriptor
int socket_fd(Socket* sock);

// Get the socket error. Returns the last error code.
// On Windows, it returns the code from WSAGetLastError().
// On Unix, it returns the value of errno.
int socket_error(void);

// Get the socket error message for the given error code
// as returned by socket_error().
void socket_strerror(int err, char* buffer, size_t size);

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

// Get the socket family
int socket_family(Socket* sock);

// Get the socket type
int socket_type(Socket* sock);

// Check if the system is little endian.
bool is_little_endian(void);

// Check if the system is big endian.
bool is_big_endian(void);

#if defined(__cplusplus)
}
#endif

#endif  // SOCKET_H
