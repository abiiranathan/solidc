#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>  // Added for threading
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>  // To detect CPU count
#include <unistd.h>

// Epoll shim for FreeBSD/Mac
#include "../include/epoll.h"

#define MAX_EVENTS 64
#define SERVER_PORT 8080
#define READ_BUFFER_SIZE 4096
#define WRITE_BUFFER_SIZE 16384

// --- Structures ---

typedef struct {
    int fd;
    char read_buffer[READ_BUFFER_SIZE];
    size_t read_buffer_len;
    char write_buffer[WRITE_BUFFER_SIZE];
    size_t write_buffer_len;
    size_t write_buffer_pos;
    bool close_after_write;
} Connection;

// --- Helper Functions ---

static int update_epoll_events(int epfd, int fd, Connection* conn, bool want_write) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (want_write) ev.events |= EPOLLOUT;
    ev.data.ptr = conn;
    return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

// --- Logic Implementation ---

static void handle_write(int epfd, Connection* conn) {
    if (conn->write_buffer_pos == conn->write_buffer_len) {
        conn->write_buffer_pos = 0;
        conn->write_buffer_len = 0;
        if (conn->close_after_write) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            free(conn);
            return;
        }
        update_epoll_events(epfd, conn->fd, conn, false);
        return;
    }

    while (conn->write_buffer_pos < conn->write_buffer_len) {
        size_t remaining = conn->write_buffer_len - conn->write_buffer_pos;
        ssize_t written = write(conn->fd, conn->write_buffer + conn->write_buffer_pos, remaining);

        if (written > 0) {
            conn->write_buffer_pos += (size_t)written;
        } else if (written == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                update_epoll_events(epfd, conn->fd, conn, true);
                return;
            } else {
                epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
                close(conn->fd);
                free(conn);
                return;
            }
        }
    }

    conn->write_buffer_pos = 0;
    conn->write_buffer_len = 0;

    if (conn->close_after_write) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
        close(conn->fd);
        free(conn);
    } else {
        update_epoll_events(epfd, conn->fd, conn, false);
    }
}

static void queue_response(int epfd, Connection* conn, const char* status, const char* content_type, const char* body) {
    if (conn->write_buffer_pos > 0 && conn->write_buffer_pos < conn->write_buffer_len) {
        size_t pending = conn->write_buffer_len - conn->write_buffer_pos;
        memmove(conn->write_buffer, conn->write_buffer + conn->write_buffer_pos, pending);
        conn->write_buffer_len = pending;
        conn->write_buffer_pos = 0;
    } else if (conn->write_buffer_pos == conn->write_buffer_len) {
        conn->write_buffer_len = 0;
        conn->write_buffer_pos = 0;
    }

    size_t space = WRITE_BUFFER_SIZE - conn->write_buffer_len;
    if (space <= 1) {
        conn->close_after_write = true;
        return;
    }

    const char* conn_header = conn->close_after_write ? "close" : "keep-alive";
    int len = snprintf(conn->write_buffer + conn->write_buffer_len,
                       space,
                       "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: %s\r\n\r\n%s",
                       status,
                       content_type,
                       strlen(body),
                       conn_header,
                       body);

    if (len > 0 && (size_t)len < space) {
        conn->write_buffer_len += (size_t)len;
        handle_write(epfd, conn);
    } else {
        conn->close_after_write = true;
    }
}

static void handle_request(int epfd, Connection* conn) {
    while (true) {
        if (conn->read_buffer_len < READ_BUFFER_SIZE) conn->read_buffer[conn->read_buffer_len] = '\0';
        else
            conn->read_buffer[READ_BUFFER_SIZE - 1] = '\0';

        char* end_of_headers = strstr(conn->read_buffer, "\r\n\r\n");
        if (!end_of_headers) return;

        size_t header_len = (size_t)(end_of_headers - conn->read_buffer) + 4;

        char saved_char = conn->read_buffer[header_len];
        conn->read_buffer[header_len] = '\0';
        if (strcasestr(conn->read_buffer, "Connection: close")) conn->close_after_write = true;
        conn->read_buffer[header_len] = saved_char;

        // --- Routing ---
        // Added thread info to response to prove load balancing
        char response_body[512];
        if (strncmp(conn->read_buffer, "GET / ", 6) == 0) {
            snprintf(response_body,
                     sizeof(response_body),
                     "<html><body><h1>Handled by Thread ID: %lu</h1></body></html>",
                     pthread_self());
            queue_response(epfd, conn, "200 OK", "text/html", response_body);
        } else {
            queue_response(epfd, conn, "404 Not Found", "text/html", "<html><body><h1>404</h1></body></html>");
        }

        size_t remaining = conn->read_buffer_len - header_len;
        if (remaining > 0) memmove(conn->read_buffer, conn->read_buffer + header_len, remaining);
        conn->read_buffer_len = remaining;
        if (conn->read_buffer_len == 0) break;
    }
}

static void handle_read(int epfd, Connection* conn) {
    while (true) {
        size_t remaining = READ_BUFFER_SIZE - conn->read_buffer_len - 1;
        if (remaining == 0) {
            conn->close_after_write = true;
            handle_write(epfd, conn);
            return;
        }

        ssize_t nread = read(conn->fd, conn->read_buffer + conn->read_buffer_len, remaining);

        if (nread > 0) {
            conn->read_buffer_len += (size_t)nread;
            handle_request(epfd, conn);
        } else if (nread == 0) {
            if (conn->write_buffer_len > 0) {
                conn->close_after_write = true;
                handle_write(epfd, conn);
            } else {
                epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
                close(conn->fd);
                free(conn);
            }
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            free(conn);
            return;
        }
    }
}

static void handle_accept(int epfd, int listen_fd) {
    while (true) {
        struct sockaddr_in client_addr = {0};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept");
            break;
        }

        if (set_nonblocking(client_fd) == -1) {
            close(client_fd);
            continue;
        }

        Connection* conn = calloc(1, sizeof(*conn));
        if (!conn) {
            close(client_fd);
            continue;
        }

        conn->fd = client_fd;
        struct epoll_event ev = {.events = EPOLLIN | EPOLLET | EPOLLRDHUP, .data.ptr = conn};
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            free(conn);
            close(client_fd);
        }
    }
}

// --- Worker Thread ---

void* worker_routine(void* arg) {
    long thread_id = (long)arg;

    // Each thread creates its OWN socket bound to the SAME port
    int listen_fd = create_listen_socket(SERVER_PORT);
    if (listen_fd == -1) {
        fprintf(stderr, "Thread %ld failed to create socket\n", thread_id);
        return NULL;
    }

    // Each thread has its OWN epoll instance
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        close(listen_fd);
        return NULL;
    }

    // Register the listen socket with epoll.
    // Use data.ptr and NOT data.fd so we can differentiate server and clients.
    struct epoll_event ev = {.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE, .data.ptr = NULL};
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl listen");
        close(listen_fd);
        close(epfd);
        return NULL;
    }

    printf("Worker %ld listening on port %d (fd: %d)\n", thread_id, SERVER_PORT, listen_fd);

    struct epoll_event events[MAX_EVENTS];

    // Independent Event Loop
    while (true) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.ptr == NULL) {
                // Incoming connection on this thread's specific socket
                handle_accept(epfd, listen_fd);
            } else {
                Connection* conn = (Connection*)events[i].data.ptr;
                uint32_t e = events[i].events;

                if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    if (e & EPOLLRDHUP) {
                        conn->close_after_write = true;
                        handle_write(epfd, conn);
                    } else {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
                        close(conn->fd);
                        free(conn);
                    }
                    continue;
                }

                if (e & EPOLLIN) handle_read(epfd, conn);
                if (e & EPOLLOUT) handle_write(epfd, conn);
            }
        }
    }

    close(listen_fd);
    close(epfd);
    return NULL;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    // Determine core count
    int num_cores = get_nprocs();
    if (num_cores < 1) num_cores = 1;

    printf("Starting server with %d worker threads\n", num_cores);

    pthread_t* threads = calloc((size_t)num_cores, sizeof(pthread_t));
    if (!threads) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_cores; i++) {
        if (pthread_create(&threads[i], NULL, worker_routine, (void*)(long)i) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // Main thread just waits (or could handle signals/logging)
    for (int i = 0; i < num_cores; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    return EXIT_SUCCESS;
}
