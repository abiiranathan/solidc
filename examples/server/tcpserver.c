#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // for SO_REUSEPORT, accept4
#endif

#include "tcpserver.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <unistd.h>

// Epoll shim for cross-platform support (FreeBSD/macOS)
#ifdef __linux__
#include <sys/epoll.h>
#else
#include "epoll.h"  // Custom shim for non-Linux
#endif

// --- Internal Structures ---

/** Represents a single client connection. */
struct tcpserver_connection {
    int fd;                           /** Socket file descriptor */
    char* read_buffer;                /** Incoming data buffer */
    size_t read_buffer_size;          /** Total size of read buffer */
    size_t read_buffer_len;           /** Current bytes in read buffer */
    char* write_buffer;               /** Outgoing data buffer */
    size_t write_buffer_size;         /** Total size of write buffer */
    size_t write_buffer_len;          /** Current bytes queued for writing */
    size_t write_buffer_pos;          /** Current write position */
    bool close_after_write;           /** Close connection after write completes */
    void* userdata;                   /** User-defined per-connection data */
    struct sockaddr_in peer_addr;     /** Remote peer address */
    int epfd;                         /** Epoll file descriptor for this connection */
    const tcpserver_config_t* config; /** Server configuration reference */
};

/** Worker thread context. */
typedef struct {
    tcpserver_t* server;   /** Back-reference to parent server */
    pthread_t thread;      /** Thread handle */
    int thread_id;         /** Worker thread ID */
    int listen_fd;         /** This thread's listen socket */
    int epfd;              /** This thread's epoll instance */
    volatile bool running; /** Thread running flag */
} tcpserver_worker_t;

/** Main server structure. */
struct tcpserver {
    tcpserver_config_t config;        /** Server configuration */
    tcpserver_worker_t* workers;      /** Array of worker threads */
    int num_workers;                  /** Number of active workers */
    volatile bool shutdown_requested; /** Shutdown flag */
};

// --- Internal Helper Functions ---

/**
 * Sets a file descriptor to non-blocking mode.
 * @param fd File descriptor
 * @return 0 on success, -1 on failure
 */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Creates and binds a listening socket with SO_REUSEPORT.
 * @param port Port number to bind to
 * @param config Server configuration for socket options
 * @return Socket file descriptor on success, -1 on failure
 */
static int create_listen_socket(uint16_t port, const tcpserver_config_t* config) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return -1;
    }

    // Enable address reuse
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        close(listen_fd);
        return -1;
    }

    // Enable port reuse for multi-threaded load balancing
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        perror("setsockopt SO_REUSEPORT");
        close(listen_fd);
        return -1;
    }

    // Configure TCP_NODELAY if requested
    if (config->nodelay) {
        if (setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) {
            perror("setsockopt TCP_NODELAY");
        }
    }

    // Set receive buffer size if specified
    if (config->rcvbuf_size > 0) {
        if (setsockopt(listen_fd, SOL_SOCKET, SO_RCVBUF, &config->rcvbuf_size, sizeof(config->rcvbuf_size)) == -1) {
            perror("setsockopt SO_RCVBUF");
        }
    }

    // Set send buffer size if specified
    if (config->sndbuf_size > 0) {
        if (setsockopt(listen_fd, SOL_SOCKET, SO_SNDBUF, &config->sndbuf_size, sizeof(config->sndbuf_size)) == -1) {
            perror("setsockopt SO_SNDBUF");
        }
    }

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = INADDR_ANY};

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    if (set_nonblocking(listen_fd) == -1) {
        perror("set_nonblocking");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

/**
 * Updates epoll event registration for a connection.
 * @param conn Connection to update
 * @param want_write Whether to monitor for write readiness
 * @return 0 on success, -1 on failure
 */
static int update_epoll_events(tcpserver_connection_t* conn, bool want_write) {
    struct epoll_event ev = {.events = EPOLLIN | EPOLLET | EPOLLRDHUP, .data.ptr = conn};
    if (want_write) {
        ev.events |= EPOLLOUT;
    }
    return epoll_ctl(conn->epfd, EPOLL_CTL_MOD, conn->fd, &ev);
}

/**
 * Handles write operations for a connection.
 * Attempts to flush the write buffer to the socket.
 * @param conn Connection to write from
 */
static void handle_write(tcpserver_connection_t* conn) {
    // Nothing to write
    if (conn->write_buffer_pos == conn->write_buffer_len) {
        conn->write_buffer_pos = 0;
        conn->write_buffer_len = 0;

        if (conn->close_after_write) {
            epoll_ctl(conn->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
            if (conn->config->on_close) {
                conn->config->on_close(conn, conn->config->userdata);
            }

            close(conn->fd);
            free(conn->read_buffer);
            free(conn->write_buffer);
            free(conn);
            return;
        }

        // Notify write complete
        if (conn->config->on_write_complete) {
            conn->config->on_write_complete(conn, conn->config->userdata);
        }

        update_epoll_events(conn, false);
        return;
    }

    // Write as much as possible
    while (conn->write_buffer_pos < conn->write_buffer_len) {
        size_t remaining = conn->write_buffer_len - conn->write_buffer_pos;
        ssize_t written  = write(conn->fd, conn->write_buffer + conn->write_buffer_pos, remaining);

        if (written > 0) {
            conn->write_buffer_pos += (size_t)written;
        } else if (written == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket buffer full, wait for EPOLLOUT
                update_epoll_events(conn, true);
                return;
            } else {
                // Write error, close connection
                epoll_ctl(conn->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
                if (conn->config->on_close) {
                    conn->config->on_close(conn, conn->config->userdata);
                }
                close(conn->fd);
                free(conn->read_buffer);
                free(conn->write_buffer);
                free(conn);
                return;
            }
        }
    }

    // All data written
    conn->write_buffer_pos = 0;
    conn->write_buffer_len = 0;

    if (conn->close_after_write) {
        epoll_ctl(conn->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
        if (conn->config->on_close) {
            conn->config->on_close(conn, conn->config->userdata);
        }

        close(conn->fd);
        free(conn->read_buffer);
        free(conn->write_buffer);
        free(conn);
    } else {
        if (conn->config->on_write_complete) {
            conn->config->on_write_complete(conn, conn->config->userdata);
        }
        update_epoll_events(conn, false);
    }
}

/**
 * Handles read operations for a connection.
 * Reads available data and invokes the on_read callback.
 * @param conn Connection to read from
 */
static void handle_read(tcpserver_connection_t* conn) {
    while (true) {
        size_t remaining = conn->read_buffer_size - conn->read_buffer_len - 1;
        if (remaining == 0) {
            // Buffer full, close connection
            fprintf(stderr, "Read buffer overflow on fd %d\n", conn->fd);
            conn->close_after_write = true;
            handle_write(conn);
            return;
        }

        ssize_t nread = read(conn->fd, conn->read_buffer + conn->read_buffer_len, remaining);

        if (nread > 0) {
            conn->read_buffer_len += (size_t)nread;
            conn->read_buffer[conn->read_buffer_len] = '\0';

            // Invoke user callback
            if (conn->config->on_read) {
                size_t consumed =
                    conn->config->on_read(conn, conn->read_buffer, conn->read_buffer_len, conn->config->userdata);

                if (consumed > 0 && consumed <= conn->read_buffer_len) {
                    // Shift remaining data to beginning of buffer
                    size_t remaining_data = conn->read_buffer_len - consumed;
                    if (remaining_data > 0) {
                        memmove(conn->read_buffer, conn->read_buffer + consumed, remaining_data);
                    }
                    conn->read_buffer_len = remaining_data;
                }
            }
        } else if (nread == 0) {
            // Client closed connection
            if (conn->write_buffer_len > 0) {
                conn->close_after_write = true;
                handle_write(conn);
            } else {
                epoll_ctl(conn->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
                if (conn->config->on_close) {
                    conn->config->on_close(conn, conn->config->userdata);
                }

                close(conn->fd);
                free(conn->read_buffer);
                free(conn->write_buffer);
                free(conn);
            }
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data available
                break;
            }

            // Read error
            epoll_ctl(conn->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
            if (conn->config->on_close) {
                conn->config->on_close(conn, conn->config->userdata);
            }
            close(conn->fd);
            free(conn->read_buffer);
            free(conn->write_buffer);
            free(conn);
            return;
        }
    }
}

/**
 * Accepts new connections on a listening socket.
 * @param worker Worker thread context
 */
static void handle_accept(tcpserver_worker_t* worker) {
    while (true) {
        struct sockaddr_in client_addr = {0};
        socklen_t addr_len             = sizeof(client_addr);

#ifdef __linux__
        int client_fd = accept4(worker->listen_fd, (struct sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept4");
            break;
        }
#else
        int client_fd = accept(worker->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept");
            break;
        }
        if (set_nonblocking(client_fd) == -1) {
            close(client_fd);
            continue;
        }
#endif

        tcpserver_connection_t* conn = malloc(sizeof(*conn));
        if (conn == NULL) {
            close(client_fd);
            continue;
        }

        conn->read_buffer  = malloc(worker->server->config.read_buffer_size);
        conn->write_buffer = malloc(worker->server->config.write_buffer_size);

        if (conn->read_buffer == NULL || conn->write_buffer == NULL) {
            free(conn->read_buffer);
            free(conn->write_buffer);
            free(conn);
            close(client_fd);
            continue;
        }

        *conn = (tcpserver_connection_t){
            .fd                = client_fd,
            .read_buffer       = conn->read_buffer,
            .read_buffer_size  = worker->server->config.read_buffer_size,
            .read_buffer_len   = 0,
            .write_buffer      = conn->write_buffer,
            .write_buffer_size = worker->server->config.write_buffer_size,
            .write_buffer_len  = 0,
            .write_buffer_pos  = 0,
            .close_after_write = false,
            .userdata          = NULL,
            .peer_addr         = client_addr,
            .epfd              = worker->epfd,
            .config            = &worker->server->config,
        };

        struct epoll_event ev = {.events = EPOLLIN | EPOLLET | EPOLLRDHUP, .data.ptr = conn};

        if (epoll_ctl(worker->epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            perror("epoll_ctl add client");
            free(conn->read_buffer);
            free(conn->write_buffer);
            free(conn);
            close(client_fd);
            continue;
        }

        // Notify connection callback
        if (worker->server->config.on_connect) {
            worker->server->config.on_connect(conn, worker->server->config.userdata);
        }
    }
}

/**
 * Worker thread main loop.
 * Each worker has its own listen socket and epoll instance.
 * @param arg Pointer to tcpserver_worker_t
 * @return NULL
 */
static void* worker_routine(void* arg) {
    tcpserver_worker_t* worker = (tcpserver_worker_t*)arg;

    worker->listen_fd = create_listen_socket(worker->server->config.port, &worker->server->config);
    if (worker->listen_fd == -1) {
        fprintf(stderr, "Worker %d failed to create listen socket\n", worker->thread_id);
        return NULL;
    }

    worker->epfd = epoll_create1(0);
    if (worker->epfd == -1) {
        perror("epoll_create1");
        close(worker->listen_fd);
        return NULL;
    }

    // Register listen socket with NULL data to distinguish from connections
    struct epoll_event ev = {.events = EPOLLIN | EPOLLET, .data.ptr = NULL};

    if (epoll_ctl(worker->epfd, EPOLL_CTL_ADD, worker->listen_fd, &ev) == -1) {
        perror("epoll_ctl add listen");
        close(worker->listen_fd);
        close(worker->epfd);
        return NULL;
    }

    printf("Worker %d listening on port %d (fd: %d)\n", worker->thread_id, worker->server->config.port,
           worker->listen_fd);

    struct epoll_event events[TCPSERVER_MAX_EVENTS] = {0};
    worker->running                                 = true;

    // Event loop
    while (!worker->server->shutdown_requested) {
        int nfds = epoll_wait(worker->epfd, events, TCPSERVER_MAX_EVENTS, 100);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.ptr == NULL) {
                // Accept new connections
                handle_accept(worker);
            } else {
                // Handle existing connection
                tcpserver_connection_t* conn = (tcpserver_connection_t*)events[i].data.ptr;
                uint32_t e                   = events[i].events;

                if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    if (e & EPOLLRDHUP) {
                        conn->close_after_write = true;
                        handle_write(conn);
                    } else {
                        epoll_ctl(conn->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
                        if (conn->config->on_close) {
                            conn->config->on_close(conn, conn->config->userdata);
                        }
                        close(conn->fd);
                        free(conn->read_buffer);
                        free(conn->write_buffer);
                        free(conn);
                    }
                    continue;
                }

                if (e & EPOLLIN) handle_read(conn);
                if (e & EPOLLOUT) handle_write(conn);
            }
        }
    }

    worker->running = false;
    close(worker->listen_fd);
    close(worker->epfd);
    return NULL;
}

// --- Public API Implementation ---

void tcpserver_config_default(tcpserver_config_t* config) {
    if (config == NULL) return;

    *config = (tcpserver_config_t){
        .port              = 8080,
        .num_threads       = 0,
        .read_buffer_size  = TCPSERVER_READ_BUFFER_SIZE,
        .write_buffer_size = TCPSERVER_WRITE_BUFFER_SIZE,
        .nodelay           = false,
        .rcvbuf_size       = 0,
        .sndbuf_size       = 0,
        .on_connect        = NULL,
        .on_read           = NULL,
        .on_close          = NULL,
        .on_write_complete = NULL,
        .userdata          = NULL,
    };
}

tcpserver_t* tcpserver_create(const tcpserver_config_t* config) {
    if (config == NULL || config->on_read == NULL) {
        fprintf(stderr, "Invalid config: on_read callback is required\n");
        return NULL;
    }

    tcpserver_t* server = malloc(sizeof(*server));
    if (server == NULL) {
        perror("malloc server");
        return NULL;
    }

    server->config             = *config;
    server->shutdown_requested = false;

    // Auto-detect CPU count if not specified
    server->num_workers = config->num_threads;
    if (server->num_workers <= 0) {
        server->num_workers = get_nprocs();
        if (server->num_workers < 1) {
            server->num_workers = 1;
        }
    }

    server->workers = calloc((size_t)server->num_workers, sizeof(tcpserver_worker_t));
    if (server->workers == NULL) {
        perror("calloc workers");
        free(server);
        return NULL;
    }

    return server;
}

int tcpserver_run(tcpserver_t* server) {
    if (server == NULL) return -1;

    // Ignore SIGPIPE (write to closed socket)
    signal(SIGPIPE, SIG_IGN);

    printf("Starting server on port %d with %d workers\n", server->config.port, server->num_workers);

    // Spawn worker threads
    for (int i = 0; i < server->num_workers; i++) {
        server->workers[i].server    = server;
        server->workers[i].thread_id = i;

        if (pthread_create(&server->workers[i].thread, NULL, worker_routine, &server->workers[i]) != 0) {
            perror("pthread_create");
            server->shutdown_requested = true;

            // Wait for already-started threads
            for (int j = 0; j < i; j++) {
                pthread_join(server->workers[j].thread, NULL);
            }
            return -1;
        }
    }

    // Wait for all workers to complete
    for (int i = 0; i < server->num_workers; i++) {
        pthread_join(server->workers[i].thread, NULL);
    }

    return 0;
}

void tcpserver_shutdown(tcpserver_t* server) {
    if (server == NULL) return;
    server->shutdown_requested = true;
}

void tcpserver_destroy(tcpserver_t* server) {
    if (server == NULL) return;
    free(server->workers);
    free(server);
}

// --- Connection API Implementation ---

ssize_t tcpserver_write(tcpserver_connection_t* conn, const char* data, size_t length) {
    if (conn == NULL || data == NULL || length == 0) return -1;

    // Compact buffer if partially written
    if (conn->write_buffer_pos > 0 && conn->write_buffer_pos < conn->write_buffer_len) {
        size_t pending = conn->write_buffer_len - conn->write_buffer_pos;
        memmove(conn->write_buffer, conn->write_buffer + conn->write_buffer_pos, pending);
        conn->write_buffer_len = pending;
        conn->write_buffer_pos = 0;
    } else if (conn->write_buffer_pos == conn->write_buffer_len) {
        conn->write_buffer_len = 0;
        conn->write_buffer_pos = 0;
    }

    size_t space = conn->write_buffer_size - conn->write_buffer_len;
    if (space < length) {
        // Not enough space
        return -1;
    }

    memcpy(conn->write_buffer + conn->write_buffer_len, data, length);
    conn->write_buffer_len += length;

    // Try to write immediately
    handle_write(conn);

    return (ssize_t)length;
}

ssize_t tcpserver_printf(tcpserver_connection_t* conn, const char* format, ...) {
    if (conn == NULL || format == NULL) return -1;

    // Compact buffer
    if (conn->write_buffer_pos > 0 && conn->write_buffer_pos < conn->write_buffer_len) {
        size_t pending = conn->write_buffer_len - conn->write_buffer_pos;
        memmove(conn->write_buffer, conn->write_buffer + conn->write_buffer_pos, pending);
        conn->write_buffer_len = pending;
        conn->write_buffer_pos = 0;
    } else if (conn->write_buffer_pos == conn->write_buffer_len) {
        conn->write_buffer_len = 0;
        conn->write_buffer_pos = 0;
    }

    size_t space = conn->write_buffer_size - conn->write_buffer_len;

    va_list args;
    va_start(args, format);
    int written = vsnprintf(conn->write_buffer + conn->write_buffer_len, space, format, args);
    va_end(args);

    if (written < 0 || (size_t)written >= space) {
        return -1;
    }

    conn->write_buffer_len += (size_t)written;
    handle_write(conn);

    return written;
}

void tcpserver_close_after_write(tcpserver_connection_t* conn) {
    if (conn == NULL) return;
    conn->close_after_write = true;
}

void tcpserver_close_now(tcpserver_connection_t* conn) {
    if (conn == NULL) return;
    epoll_ctl(conn->epfd, EPOLL_CTL_DEL, conn->fd, NULL);
    if (conn->config->on_close) {
        conn->config->on_close(conn, conn->config->userdata);
    }
    close(conn->fd);
    free(conn->read_buffer);
    free(conn->write_buffer);
    free(conn);
}

int tcpserver_get_fd(const tcpserver_connection_t* conn) {
    return conn ? conn->fd : -1;
}

int tcpserver_get_peer_addr(const tcpserver_connection_t* conn, char* addr_buf, size_t addr_buf_size,
                            uint16_t* port_out) {
    if (conn == NULL || addr_buf == NULL || addr_buf_size == 0) return -1;

    if (inet_ntop(AF_INET, &conn->peer_addr.sin_addr, addr_buf, (socklen_t)addr_buf_size) == NULL) {
        return -1;
    }

    if (port_out != NULL) {
        *port_out = ntohs(conn->peer_addr.sin_port);
    }

    return 0;
}

void tcpserver_set_connection_userdata(tcpserver_connection_t* conn, void* userdata) {
    if (conn != NULL) {
        conn->userdata = userdata;
    }
}

void* tcpserver_get_connection_userdata(const tcpserver_connection_t* conn) {
    return conn ? conn->userdata : NULL;
}
