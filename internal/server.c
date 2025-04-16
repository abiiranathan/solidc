#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/epoll.h"

void signal_handler(int sig) {
    fprintf(stderr, "Caught signal %s\n", strsignal(sig));
    epoll_shutdown();
}

static bool http_end_of_message(const char* req_data, size_t length) {
    char* end_of_headers = (char*)memmem(req_data, length, "\r\n\r\n", 4);
    if (!end_of_headers) {
        return false;
    }

    char *method, *uri, *http_version, *header_start;
    if (!epoll_parse_request_line((char*)req_data, &method, &uri, &http_version, &header_start)) {
        return false;
    };

    // If we don't expect HTTP body and we have end of headers, we are done.
    if ((strcmp(method, "GET") == 0 || strcmp(method, "OPTIONS") == 0 || strcmp(method, "HEAD") == 0)) {
        return true;
    }

    // now find content-length
    const char* content_len_header = strcasestr(header_start, "content-length:");
    if (!content_len_header || content_len_header >= end_of_headers) {
        return false;
    }

    size_t header_capacity = end_of_headers - req_data + 4;
    size_t body_size       = strtoul(content_len_header + 15, NULL, 10);
    return (header_capacity + body_size) == length;
}

bool detect_complete_message(const char* write_buffer, size_t length) {
    return http_end_of_message(write_buffer, length);
}

void request_handler(EpollConn* conn) {
    const char* msg = "HTTP/1.1 200 OK\r\n\r\n";
    epoll_write(conn, msg, strlen(msg));
    epoll_write(conn, "Hello World, my dear friends\n", 29);
}

int main() {
    const char* port = "8080";
    int server_fd    = epoll_create_and_bind_socket(port, true);
    if (server_fd == -1) {
        perror("epoll_create_and_bind_socket");
        return 1;
    }

    io_callbacks io_cb = {
        .request_handler = request_handler,
        .end_of_message  = detect_complete_message,
    };

    epoll_set_handler(signal_handler);
    return epoll_eventloop(server_fd, &io_cb);
}
