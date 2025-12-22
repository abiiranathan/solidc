/**
 * @file example_http_server.c
 * @brief Example HTTP server built with tcpserver library
 *
 * Demonstrates:
 * - Basic HTTP request parsing
 * - Response generation
 * - Connection lifecycle management
 * - Per-connection state
 *
 * Compile:
 *   gcc -O2 -Wall -Wextra -o httpd httpd.c tcpserver.c -lpthread
 *
 * Run:
 *   ./httpd
 *
 * Test:
 *   curl http://localhost:8080/
 *   ab -n 100000 -c 100 http://localhost:8080/
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "tcpserver.h"

// --- HTTP Request Parser State ---

typedef struct {
    bool headers_complete;
    bool close_connection;
} http_connection_state_t;

/**
 * Searches for a header in an HTTP request.
 * @param request HTTP request string (null-terminated)
 * @param header Header name to search for (case-insensitive)
 * @return Pointer to header value, or NULL if not found
 * @note Returned pointer is within the request buffer
 */
static const char* find_header(const char* request, const char* header) {
    // Simple case-insensitive header search
    char header_lower[256];
    snprintf(header_lower, sizeof(header_lower), "%s:", header);

    const char* line = request;
    while ((line = strchr(line, '\n')) != NULL) {
        line++;  // Skip past newline
        if (strncasecmp(line, header_lower, strlen(header_lower)) == 0) {
            const char* value = line + strlen(header_lower);
            // Skip whitespace
            while (*value == ' ' || *value == '\t')
                value++;
            return value;
        }
    }
    return NULL;
}

/**
 * Generates an HTTP response for a request.
 * @param conn Connection to send response on
 * @param method HTTP method (e.g., "GET")
 * @param path Request path
 */
static void send_http_response(tcpserver_connection_t* conn, const char* method, const char* path) {
    http_connection_state_t* state = tcpserver_get_connection_userdata(conn);
    const char* conn_header        = (state && state->close_connection) ? "close" : "keep-alive";

    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        // Root endpoint
        char body[512];
        snprintf(body, sizeof(body),
                 "<html><head><title>tcpserver Example</title></head>"
                 "<body><h1>Hello from tcpserver!</h1>"
                 "<p>Handled by thread: %lu</p>"
                 "<p>Connection: %s</p></body></html>",
                 pthread_self(), conn_header);

        tcpserver_printf(conn,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/html\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: %s\r\n"
                         "\r\n"
                         "%s",
                         strlen(body), conn_header, body);

    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/stats") == 0) {
        // Stats endpoint
        char body[256];
        snprintf(body, sizeof(body), "{\"thread_id\": %lu, \"connection\": \"%s\"}", pthread_self(), conn_header);

        tcpserver_printf(conn,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: %s\r\n"
                         "\r\n"
                         "%s",
                         strlen(body), conn_header, body);
    } else {
        // 404 Not Found
        const char* body = "<html><body><h1>404 Not Found</h1></body></html>";
        tcpserver_printf(conn,
                         "HTTP/1.1 404 Not Found\r\n"
                         "Content-Type: text/html\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: %s\r\n"
                         "\r\n"
                         "%s",
                         strlen(body), conn_header, body);
    }

    if (state && state->close_connection) {
        tcpserver_close_after_write(conn);
    }
}

/**
 * Parses and handles HTTP requests.
 * Called by tcpserver when data is available.
 */
static size_t on_read(tcpserver_connection_t* conn, const char* buffer, size_t length, void* userdata) {
    (void)userdata;  // Unused

    http_connection_state_t* state = tcpserver_get_connection_userdata(conn);
    if (state == NULL) {
        // Allocate per-connection state on first read
        state = calloc(1, sizeof(*state));
        if (state == NULL) {
            tcpserver_close_now(conn);
            return 0;
        }
        tcpserver_set_connection_userdata(conn, state);
    }

    // Look for end of HTTP headers
    const char* end_of_headers = strstr(buffer, "\r\n\r\n");
    if (end_of_headers == NULL) {
        // Headers incomplete, wait for more data
        if (length >= 8192) {
            // Request too large
            tcpserver_printf(conn,
                             "HTTP/1.1 413 Request Entity Too Large\r\n"
                             "Connection: close\r\n"
                             "\r\n");
            tcpserver_close_after_write(conn);
            return length;
        }
        return 0;  // Don't consume, wait for complete headers
    }

    size_t header_len = (size_t)(end_of_headers - buffer) + 4;

    // Check for Connection: close header
    const char* conn_header = find_header(buffer, "Connection");
    if (conn_header != NULL && strncasecmp(conn_header, "close", 5) == 0) {
        state->close_connection = true;
    }

    // Parse request line: "METHOD /path HTTP/1.1\r\n"
    char method[16] = {0};
    char path[256]  = {0};

    if (sscanf(buffer, "%15s %255s", method, path) == 2) {
        send_http_response(conn, method, path);
    } else {
        // Malformed request
        tcpserver_printf(conn,
                         "HTTP/1.1 400 Bad Request\r\n"
                         "Connection: close\r\n"
                         "\r\n");
        tcpserver_close_after_write(conn);
    }

    return header_len;  // Consume the request
}

/**
 * Called when a new connection is established.
 */
static void on_connect(tcpserver_connection_t* conn, void* userdata) {
    (void)userdata;

    char addr[INET_ADDRSTRLEN];
    uint16_t port;
    if (tcpserver_get_peer_addr(conn, addr, sizeof(addr), &port) == 0) {
        printf("New connection from %s:%u (fd: %d)\n", addr, port, tcpserver_get_fd(conn));
    }
}

/**
 * Called when a connection is about to close.
 */
static void on_close(tcpserver_connection_t* conn, void* userdata) {
    (void)userdata;

    // Free per-connection state
    http_connection_state_t* state = tcpserver_get_connection_userdata(conn);
    free(state);

    char addr[INET_ADDRSTRLEN];
    uint16_t port;

    if (tcpserver_get_peer_addr(conn, addr, sizeof(addr), &port) == 0) {
        printf("Connection closed: %s:%u (fd: %d)\n", addr, port, tcpserver_get_fd(conn));
    }
}

int main(void) {
    tcpserver_config_t config;
    tcpserver_config_default(&config);

    config.port        = 8080;
    config.num_threads = 0;     // Auto-detect CPU count
    config.nodelay     = true;  // Disable Nagle for lower latency
    config.on_connect  = on_connect;
    config.on_read     = on_read;
    config.on_close    = on_close;

    tcpserver_t* server = tcpserver_create(&config);
    if (server == NULL) {
        fprintf(stderr, "Failed to create server\n");
        return EXIT_FAILURE;
    }

    printf("HTTP server starting on port %d\n", config.port);
    printf("Try: curl http://localhost:%d/\n", config.port);

    int ret = tcpserver_run(server);

    tcpserver_destroy(server);
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
