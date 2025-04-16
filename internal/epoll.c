#include <time.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../include/epoll.h"

typedef struct epoll_connection {
    char* read_buffer;     // Dynamic buffer for reading
    size_t read_buf_size;  // Current buffer size
    size_t read_offset;    // How much has been read

    // char* write_buffer;         // Dynamic buffer for writing
    // size_t write_buf_length;    // Number of bytes written into buffer.
    // size_t write_buf_capacity;  // Total capacity of write buffer.

    int client_fd;        // client file descriptor.
    int epoll_fd;         // EPOLL file descriptor.
    int closed;           // flag to indicate if the connection is already closed.
    bool end_of_message;  // Reached E.O.M
} EpollConn;

// server running flag.
sig_atomic_t running = 1;

/**
 * @brief Shutsdown the server eventloop.
 */
void epoll_shutdown(void) {
    running = 0;
}

/**
 * @brief Sets a file descriptor to non-blocking mode.
 *
 * @param sock_fd The file descriptor of the socket.
 * @return true on success, false on failure.
 */
bool set_nonblocking(int sock_fd) {
    int flags, s;

    flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return false;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sock_fd, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl");
        return false;
    }

    return true;
}

/**
 * @brief Enables TCP keep-alive probes for a socket.
 *
 *        This function configures the socket to send keep-alive probes after a period of inactivity
 *        to detect dead peers.
 *
 * @param sock_fd The file descriptor of the socket.
 * @return true on success, false on failure.
 */
bool enable_keepalive(int sock_fd) {
    int keepalive = 1;   // Enable keepalive
    int keepidle  = 60;  // 60 seconds before sending keepalive probes
    int keepintvl = 5;   // 5 seconds interval between keepalive probes
    int keepcnt   = 3;   // 3 keepalive probes before closing the connection

    if (setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(int)) < 0) {
        perror("setsockopt (SO_KEEPALIVE)");
        return false;
    }

    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int)) < 0) {
        perror("setsockopt (TCP_KEEPIDLE)");
        return false;
    }

    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int)) < 0) {
        perror("setsockopt (TCP_KEEPINTVL)");
        return false;
    }

    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int)) < 0) {
        perror("setsockopt (TCP_KEEPCNT)");
        return false;
    }
    return true;
}

/**
 * @brief Adds a file descriptor to the epoll instance.
 *
 * @param epoll_fd The file descriptor of the epoll instance.
 * @param sock_fd  The file descriptor of the socket to add.
 * @param event    The epoll_event struct defining the events to monitor.
 * @return true on success, false on failure.
 */
bool epoll_ctl_add(int epoll_fd, int sock_fd, struct epoll_event* event) {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, event) != 0) {
        perror("epoll_ctl (ADD)");
        return false;
    }
    return true;
}

/**
 * @brief Modifies the events being monitored for a file descriptor in the epoll instance.
 *
 * @param epoll_fd The file descriptor of the epoll instance.
 * @param sock_fd  The file descriptor of the socket to modify.
 * @param event    The epoll_event struct defining the new events to monitor.
 * @param events   The new event flags (EPOLLIN, EPOLLOUT, etc.)
 * @return true on success, false on failure.
 */
bool epoll_ctl_mod(int epoll_fd, int sock_fd, struct epoll_event* event, uint32_t events) {
    event->events = events;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock_fd, event) == -1) {
        perror("epoll_ctl (MOD)");
        return false;
    }
    return true;
}

/**
 * @brief Deletes a file descriptor from the epoll instance.
 *
 * @param epoll_fd The file descriptor of the epoll instance.
 * @param sock_fd  The file descriptor of the socket to delete.
 * @return true on success, false on failure.
 */
bool epoll_ctl_del(int epoll_fd, int sock_fd) {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, NULL) != 0 && errno != ENOENT) {
        perror("epoll_ctl (DEL)");
        return false;
    }
    return true;
}

/**
 * @brief Removes a file descriptor from epoll and closes it.
 *
 * @param epoll_fd  The file descriptor of the epoll instance.
 * @param client_fd The file descriptor of the client socket to close.
 */
void epoll_close(int epoll_fd, int client_fd) {
    if (!epoll_ctl_del(epoll_fd, client_fd)) {
        fprintf(stderr, "Failed to remove fd %d from epoll.\n", client_fd);
    }

    if (close(client_fd) != 0) {
        perror("close");
        fprintf(stderr, "Failed to close fd %d.\n", client_fd);
    }
}

/**
 * @brief Retrieves the IP address of the peer connected to a socket.
 *
 * @param client_fd The file descriptor of the client socket.
 * @param ipstr     A buffer to store the IP address string (must be at least INET6_ADDRSTRLEN bytes).
 * @return true on success, false on failure.
 */
bool get_peer_address(int client_fd, char ipstr[INET6_ADDRSTRLEN]) {
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);

    if (client_fd < 0) {
        fprintf(stderr, "Invalid file descriptor\n");
        return false;
    }

    if (getpeername(client_fd, (struct sockaddr*)&addr, &len) == -1) {
        perror("getpeername failed");
        return false;
    }

    memset(ipstr, 0, INET6_ADDRSTRLEN);

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)&addr;
        if (inet_ntop(AF_INET, &s->sin_addr, ipstr, INET6_ADDRSTRLEN) == NULL) {
            perror("inet_ntop failed (IPv4)");
            return false;
        }
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
        if (inet_ntop(AF_INET6, &s->sin6_addr, ipstr, INET6_ADDRSTRLEN) == NULL) {
            perror("inet_ntop failed (IPv6)");
            return false;
        }
    } else {
        fprintf(stderr, "Unknown address family: %d\n", addr.ss_family);
        return false;
    }

    return true;
}

/**
 * @brief Creates a socket, binds it to the specified port, and sets it to listen for connections.
 *
 * @param port      The port number to bind the socket to.
 * @param reuseport A flag indicating whether to allow reuse of the port.
 * @return The file descriptor of the listening socket on success, -1 on failure.
 */
int epoll_create_and_bind_socket(const char* port, bool reuseport) {
    if (port == NULL) {
        fprintf(stderr, "%s\n", "Port must not be NULL");
        return -1;
    }

    struct addrinfo hints;
    struct addrinfo *result, *addr;
    int svc_fd, server_fd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC;   /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags    = AI_PASSIVE;  /* All interfaces */

    svc_fd = getaddrinfo(NULL, port, &hints, &result);
    if (svc_fd != 0) {
        fprintf(stderr, "%s\n", gai_strerror(svc_fd));
        return -1;
    }

    for (addr = result; addr != NULL; addr = addr->ai_next) {
        server_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (server_fd == -1)
            continue;

        if (reuseport) {
            // Allow reuse of the port.
            int enable = 1;
            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                perror("setsockopt (SO_REUSEPORT | SO_REUSEADDR)");
                close(server_fd);  // close the socket if setsockopt fails.
                freeaddrinfo(result);
                return -1;
            }
        }

        svc_fd = bind(server_fd, addr->ai_addr, addr->ai_addrlen);
        if (svc_fd == 0) {
            /* We managed to bind successfully! */
            break;
        }
        close(server_fd);
    }

    freeaddrinfo(result);

    return (addr == NULL) ? -1 : server_fd;
}

/**
 * @brief Sends all data in a buffer to a socket, handling partial sends and retries.
 *
 * @param sock_fd The file descriptor of the socket to send data to.
 * @param buf     A pointer to the data buffer.
 * @param size    The size of the data buffer in bytes.
 * @return The total number of bytes sent on success, -1 on failure.
 */
ssize_t epoll_sendall(int sock_fd, const void* buf, size_t size) {
    size_t total_sent  = 0;
    ssize_t bytes_sent = 0;
    size_t remaining   = size;
    const char* data   = (const char*)buf;

    while (remaining > 0) {
        size_t chunk_size = remaining < SEND_BUFFER_SIZE ? remaining : SEND_BUFFER_SIZE;
        bytes_sent        = send(sock_fd, data + total_sent, chunk_size, MSG_NOSIGNAL);
        if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100);  // Retry after a short delay
                continue;
            } else {
                perror("send");
                return -1;
            }
        }

        total_sent += (size_t)bytes_sent;
        remaining -= (size_t)bytes_sent;
    }
    return total_sent;
}

/**
 * @brief Peeks at the data available on a socket without consuming it.
 *
 * @param client_fd  The file descriptor of the socket to peek at.
 * @param buffer     A buffer to store the peeked data.
 * @param buffer_len The size of the buffer.
 * @return The number of bytes peeked on success, -1 on error.
 */
ssize_t epoll_peek(int client_fd, char* buffer, size_t buffer_len) {
    ssize_t bytes_received = recv(client_fd, buffer, buffer_len, MSG_PEEK);
    if (bytes_received == -1) {
        perror("recv (MSG_PEEK)");
    }
    return bytes_received;
}

/**
 * @brief Reads data from a socket.
 *
 * @param client_fd  The file descriptor of the socket to read from.
 * @param buffer     A buffer to store the received data.
 * @param buffer_len The maximum number of bytes to read.
 * @return The number of bytes read on success, 0 on connection close, -1 on error.
 */
ssize_t epoll_read(int client_fd, char* buffer, size_t buffer_len) {
    ssize_t bytes_received = recv(client_fd, buffer, buffer_len, 0);
    if (bytes_received == -1) {
        perror("recv");
    }
    return bytes_received;
}

/**
 * @brief Reads data from a socket, waiting until the requested number of bytes have been read.
 *
 * @param client_fd  The file descriptor of the socket to read from.
 * @param buffer     A buffer to store the received data.
 * @param buffer_len The number of bytes to read.
 * @return The number of bytes read on success, -1 on error.  Will only return less than buffer_len
 *         bytes if a signal interrupts the call or an error occurs.
 */
ssize_t epoll_read_all(int client_fd, char* buffer, size_t buffer_len) {
    ssize_t bytes_received = recv(client_fd, buffer, buffer_len, MSG_WAITALL);
    if (bytes_received == -1) {
        perror("recv (MSG_WAITALL)");
    }
    return bytes_received;
}

/**
 * @brief Sets a signal handler for a given signal.
 *
 *        This function registers a handler function to be called when the specified signal is received.
 *        It also ignores the SIGPIPE signal, which can occur when writing to a closed socket.
 *
 * @param handler A pointer to the signal handler function.
 * @return 0 on success, -1 on failure.
 */
int epoll_set_handler(void (*handler)(int sig)) {
    errno = 0;
    if (handler == NULL) {
        fprintf(stderr, "signal handler must not be NULL\n");
        return -1;
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // See man 2 sigaction for more information.
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    };

    // Ignore SIGPIPE signal when writing to a closed socket or pipe.
    // Potential causes:
    // https://stackoverflow.com/questions/108183/how-to-prevent-sigpipes-or-handle-them-properly
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal (SIGPIPE)");
        return -1;
    };
    return 0;
}

/* Generic TCP stream reader callback */
static ssize_t reader_callback(struct epoll_event* ev) {
    EpollConn* conn     = ev->data.ptr;
    const int client_fd = conn->client_fd;

    // Temporary read buffer
    char chunk[READ_BUFFER_SIZE];
    ssize_t bytes_read = epoll_read(client_fd, chunk, sizeof(chunk));

    if (bytes_read > 0) {
        const size_t new_data_size = conn->read_offset + bytes_read;

        // Safety check - prevent memory exhaustion attacks
        if (new_data_size > MAX_INBOUND_MSG) {
            fprintf(stderr, "Message too large from client %d\n", client_fd);
            return -1;
        }

        // Grow buffer if needed (geometric growth)
        if (new_data_size >= conn->read_buf_size) {
            size_t new_size = conn->read_buf_size ? conn->read_buf_size * 2 : READ_BUFFER_SIZE;
            while (new_size < new_data_size) {
                new_size *= 2;
            }

            char* new_buf = realloc(conn->read_buffer, new_size);
            if (!new_buf) {
                perror("Failed to expand read buffer");
                return -1;
            }
            conn->read_buffer   = new_buf;
            conn->read_buf_size = new_size;
        }

        // Append new data to buffer
        memcpy(conn->read_buffer + conn->read_offset, chunk, bytes_read);
        conn->read_offset += bytes_read;
        // Let the event loop detect end of message and call process message.
    } else if (bytes_read == 0) {
        // Clean connection close
        if (conn->read_offset > 0) {
            printf("Client %d disconnected during read\n", client_fd);
            conn->end_of_message = 1;  // No more data.
        }
        return 0;
    } else {
        // Read error
        perror("Socket read error");
        return -1;
    }

    return bytes_read;
}

/**
 * @brief Write message of length "length" to the connection buffer, growing it if necessary.
 * @return true on success, false otherwise.
 */
bool epoll_write(EpollConn* conn, const void* msg, size_t length) {
    return epoll_sendall(conn->client_fd, msg, length) > 0;
}

bool epoll_parse_request_line(char* req_data, char** method, char** uri, char** http_version, char** header_start) {
    *method = req_data;
    *uri    = strchr(req_data, ' ');
    if (!*uri) {
        return false;
    }

    **uri = '\0';
    (*uri)++;

    *http_version = strchr(*uri, ' ');
    if (!*http_version) {
        return false;
    }
    **http_version = '\0';
    (*http_version)++;

    *header_start = strstr(*http_version, "\r\n");
    if (!*header_start) {
        return false;
    }

    **header_start = '\0';
    *header_start += 2;
    return true;
}

// Default callback to close epoll connections.
static void epoll_closer_callback(struct epoll_event* event) {
    EpollConn* conn = event->data.ptr;
    if (conn) {
        conn->closed = true;
        // printf("Closing connection with fd: %d\n", conn->client_fd);
        epoll_close(conn->epoll_fd, conn->client_fd);
        free(conn);
        event->data.ptr = NULL;
    }
}

/**
 * @brief Runs the main epoll event loop.
 *
 *        This function listens for incoming connections on the specified server socket and processes events
 *        using the provided I/O callback functions.
 *
 * @param server_fd The file descriptor of the server socket.
 * @param io_cb     A pointer to the io_callbacks structure containing the I/O callback functions.
 * @return 0 on success, 1 on failure.
 */
int epoll_eventloop(int server_fd, io_callbacks* io_cb) {
    struct epoll_event event = {0}, events[MAXEVENTS] = {0};
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int epoll_fd;

    if (!io_cb || !io_cb->request_handler || !io_cb->end_of_message) {
        fprintf(stderr, "Error: All IO callbacks must be non-NULL.\n");
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        return -1;
    }

    // Create an epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        return -1;
    }

    event.events  = EPOLLIN;    // Monitor for incoming connections
    event.data.fd = server_fd;  // Attach server fd

    // Add server socket to epoll
    if (!epoll_ctl_add(epoll_fd, server_fd, &event)) {
        close(epoll_fd);
        return -1;
    }

    printf("Server waiting for connections...\n");
    int status = 0;
    while (running) {
        // Wait for events
        int nfds = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                // Interrupted by signal, continue to the next iteration
                continue;
            }
            perror("epoll_wait");
            status = 1;
            break;
        }

        // Process events
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // We have a new connection
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    if (errno == EINTR) {  // Interrupted by signal
                        running = 0;
                        status  = 1;
                        break;
                    }
                    perror("accept");
                    continue;  // we ignore the failed connection
                }

                // Set the client socket to non-blocking mode
                if (!set_nonblocking(client_fd)) {
                    close(client_fd);
                    fprintf(stderr, "Failed to set client socket to non-blocking mode.\n");
                    continue;
                }

                EpollConn* conn = (EpollConn*)malloc(sizeof(EpollConn));
                if (!conn) {
                    perror("malloc (connection_data)");
                    close(client_fd);  // Important: Clean up the socket if allocation fails
                    continue;
                }
                memset(conn, 0, sizeof(EpollConn));

                // Add the client socket to epoll
                conn->client_fd = client_fd;  // Store the client fd in connection.
                conn->epoll_fd  = epoll_fd;   // Store the epoll fd in connection.
                event.data.ptr  = conn;       // Store pointer to connection data
                event.events    = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLERR;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl (ADD)");
                    free(conn);
                    close(client_fd);
                    continue;
                }
            } else {
                // Handle client socket events
                EpollConn* conn_data = (EpollConn*)events[i].data.ptr;
                if (!conn_data) {
                    fprintf(stderr, "Error: NULL connection data.\n");
                    continue;
                }

                // Create a new event structure for modifications
                struct epoll_event mod_event;
                mod_event.data.ptr = conn_data;

                // Ready to read the data from client.
                if (events[i].events & EPOLLIN) {
                    ssize_t bytes_read = reader_callback(&events[i]);
                    if (bytes_read > 0) {
                        /* Protocol-Specific End of message (E.O.M) callback */
                        bool message_complete = conn_data->end_of_message ||
                                                io_cb->end_of_message(conn_data->read_buffer, conn_data->read_offset);
                        if (message_complete) {
                            conn_data->end_of_message = 1;
                            // Monitor for write readiness since we are done reading.
                            mod_event.events = EPOLLOUT | EPOLLET | EPOLLHUP | EPOLLERR;
                            if (!epoll_ctl_mod(epoll_fd, conn_data->client_fd, &mod_event, mod_event.events)) {
                                fprintf(stderr, "Failed to modify epoll event for fd %d\n", conn_data->client_fd);
                            }
                        }
                    } else if (bytes_read == 0) {
                        // Connection closed or E.O.F
                        if (!conn_data->closed) {
                            epoll_closer_callback(&events[i]);
                        }
                    } else {
                        if (!(errno == EAGAIN || errno == EWOULDBLOCK) && !conn_data->closed) {
                            printf("Closing conn after read error\n");
                            epoll_closer_callback(&events[i]);
                        }
                    }
                } else if (events[i].events & EPOLLOUT) {
                    // Call the request handler.
                    io_cb->request_handler(conn_data);
                    epoll_closer_callback(&events[i]);
                } else if ((events[i].events & EPOLLERR) || ((events[i].events & EPOLLHUP))) {
                    if (!conn_data->closed) {
                        epoll_closer_callback(&events[i]);
                    }
                }
            }
        }
    }

    close(epoll_fd);
    close(server_fd);
    return status;
}
