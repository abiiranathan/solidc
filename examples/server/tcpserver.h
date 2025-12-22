/**
 * @file tcpserver.h
 * @brief High-performance multi-threaded TCP server library with epoll
 *
 * A lightweight, production-ready TCP server library featuring:
 * - Multi-threaded SO_REUSEPORT architecture for load balancing
 * - Non-blocking I/O with edge-triggered epoll
 * - Automatic worker thread scaling to CPU cores
 * - Zero-copy buffer management where possible
 * - Connection lifecycle management
 *
 * Thread Safety: All public APIs are thread-safe unless noted otherwise.
 */

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Configuration Constants ---

/** Maximum number of events processed per epoll_wait call */
#define TCPSERVER_MAX_EVENTS 64

/** Default read buffer size per connection (4KB) */
#define TCPSERVER_READ_BUFFER_SIZE 4096

/** Default write buffer size per connection (16KB) */
#define TCPSERVER_WRITE_BUFFER_SIZE 16384

// --- Forward Declarations ---

typedef struct tcpserver_config tcpserver_config_t;
typedef struct tcpserver tcpserver_t;
typedef struct tcpserver_connection tcpserver_connection_t;

// --- Callback Types ---

/**
 * Called when a new connection is established.
 * @param conn The new connection object
 * @param userdata User-provided context pointer
 */
typedef void (*tcpserver_on_connect_fn)(tcpserver_connection_t* conn, void* userdata);

/**
 * Called when data is available to read from a connection.
 * @param conn The connection with available data
 * @param buffer Pointer to the read buffer
 * @param length Number of bytes available
 * @param userdata User-provided context pointer
 * @return Number of bytes consumed from the buffer
 */
typedef size_t (*tcpserver_on_read_fn)(tcpserver_connection_t* conn, const char* buffer, size_t length, void* userdata);

/**
 * Called when a connection is about to be closed.
 * @param conn The connection being closed
 * @param userdata User-provided context pointer
 */
typedef void (*tcpserver_on_close_fn)(tcpserver_connection_t* conn, void* userdata);

/**
 * Called when the write buffer has been fully flushed.
 * @param conn The connection that finished writing
 * @param userdata User-provided context pointer
 */
typedef void (*tcpserver_on_write_complete_fn)(tcpserver_connection_t* conn, void* userdata);

// --- Configuration Structure ---

/**
 * Configuration for TCP server initialization.
 * Initialize with tcpserver_config_default() before use.
 */
struct tcpserver_config {
    /** Port number to bind to (required) */
    uint16_t port;

    /** Number of worker threads (0 = auto-detect CPU count) */
    int num_threads;

    /** Read buffer size per connection */
    size_t read_buffer_size;

    /** Write buffer size per connection */
    size_t write_buffer_size;

    /** Enable TCP_NODELAY (disable Nagle's algorithm) */
    bool nodelay;

    /** SO_RCVBUF size (0 = system default) */
    int rcvbuf_size;

    /** SO_SNDBUF size (0 = system default) */
    int sndbuf_size;

    /** Connection callback (optional) */
    tcpserver_on_connect_fn on_connect;

    /** Read callback (required) */
    tcpserver_on_read_fn on_read;

    /** Close callback (optional) */
    tcpserver_on_close_fn on_close;

    /** Write complete callback (optional) */
    tcpserver_on_write_complete_fn on_write_complete;

    /** User-provided context pointer passed to all callbacks */
    void* userdata;
};

// --- Public API ---

/**
 * Initializes a config structure with sensible defaults.
 * @param config Pointer to config structure to initialize
 */
void tcpserver_config_default(tcpserver_config_t* config);

/**
 * Creates and initializes a new TCP server.
 * @param config Server configuration (must remain valid during server lifetime)
 * @return Pointer to server object on success, nullptr on failure
 * @note The server is not yet running after this call. Use tcpserver_run() to start.
 */
tcpserver_t* tcpserver_create(const tcpserver_config_t* config);

/**
 * Starts the TCP server and blocks until shutdown.
 * This spawns worker threads and enters the event loop.
 * @param server The server to run
 * @return 0 on clean shutdown, -1 on error
 * @note This function blocks until tcpserver_shutdown() is called from another thread.
 */
int tcpserver_run(tcpserver_t* server);

/**
 * Initiates graceful server shutdown.
 * Safe to call from signal handlers or other threads.
 * @param server The server to shutdown
 */
void tcpserver_shutdown(tcpserver_t* server);

/**
 * Destroys the server and frees all resources.
 * Must only be called after tcpserver_run() has returned.
 * @param server The server to destroy
 */
void tcpserver_destroy(tcpserver_t* server);

// --- Connection API ---

/**
 * Writes data to a connection's output buffer.
 * Data is queued and sent asynchronously.
 * @param conn The connection to write to
 * @param data Pointer to data to write
 * @param length Number of bytes to write
 * @return Number of bytes queued, or -1 if buffer is full
 * @note This function does not block. Data is copied to internal buffers.
 */
ssize_t tcpserver_write(tcpserver_connection_t* conn, const char* data, size_t length);

/**
 * Writes formatted data to a connection (like fprintf).
 * @param conn The connection to write to
 * @param format Printf-style format string
 * @param ... Format arguments
 * @return Number of bytes queued, or -1 on error
 */
ssize_t tcpserver_printf(tcpserver_connection_t* conn, const char* format, ...) __attribute__((format(printf, 2, 3)));

/**
 * Marks a connection to be closed after all pending writes complete.
 * @param conn The connection to close
 */
void tcpserver_close_after_write(tcpserver_connection_t* conn);

/**
 * Immediately closes a connection, discarding any pending writes.
 * @param conn The connection to close
 */
void tcpserver_close_now(tcpserver_connection_t* conn);

/**
 * Gets the file descriptor associated with a connection.
 * @param conn The connection
 * @return File descriptor, or -1 if invalid
 * @note For advanced use cases only. Do not modify socket options or close directly.
 */
int tcpserver_get_fd(const tcpserver_connection_t* conn);

/**
 * Retrieves the remote address of a connection.
 * @param conn The connection
 * @param addr_buf Buffer to store address string (e.g., "192.168.1.1")
 * @param addr_buf_size Size of addr_buf
 * @param port_out Optional pointer to store remote port number
 * @return 0 on success, -1 on failure
 */
int tcpserver_get_peer_addr(const tcpserver_connection_t* conn, char* addr_buf, size_t addr_buf_size,
                            uint16_t* port_out);

/**
 * Associates user data with a connection.
 * @param conn The connection
 * @param userdata Pointer to store
 */
void tcpserver_set_connection_userdata(tcpserver_connection_t* conn, void* userdata);

/**
 * Retrieves user data associated with a connection.
 * @param conn The connection
 * @return Previously stored pointer, or nullptr if not set
 */
void* tcpserver_get_connection_userdata(const tcpserver_connection_t* conn);

#ifdef __cplusplus
}
#endif

#endif  // TCPSERVER_H
