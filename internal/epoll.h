#ifndef EPOLL_H
#define EPOLL_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/types.h>

#ifndef MAXEVENTS
#define MAXEVENTS 1024  // Maximum number of events to handle simultaneously
#endif

#ifndef SEND_BUFFER_SIZE
#define SEND_BUFFER_SIZE 8192  // Default send buffer size
#endif

#ifndef READ_BUFFER_SIZE
#define READ_BUFFER_SIZE 4096  // Default read buffer size
#endif

#ifndef MAX_INBOUND_MSG
// Size of our temporary read buffer
#define MAX_INBOUND_MSG (1 << 24)  // 16MB max message size
#endif

typedef struct epoll_connection EpollConn;

/**
 * @brief Sets a file descriptor to non-blocking mode.
 *
 * @param sock_fd The file descriptor of the socket.
 * @return true on success, false on failure.
 */
bool set_nonblocking(int sock_fd);

/**
 * @brief Enables TCP keep-alive probes for a socket.
 *
 *        This function configures the socket to send keep-alive probes after a period of inactivity
 *        to detect dead peers.
 *
 * @param sock_fd The file descriptor of the socket.
 * @return true on success, false on failure.
 */
bool enable_keepalive(int sock_fd);

/**
 * @brief Adds a file descriptor to the epoll instance.
 *
 * @param epoll_fd The file descriptor of the epoll instance.
 * @param sock_fd  The file descriptor of the socket to add.
 * @param event    The epoll_event struct defining the events to monitor.
 * @return true on success, false on failure.
 */
bool epoll_ctl_add(int epoll_fd, int sock_fd, struct epoll_event* event);

/**
 * @brief Modifies the events being monitored for a file descriptor in the epoll instance.
 *
 * @param epoll_fd The file descriptor of the epoll instance.
 * @param sock_fd  The file descriptor of the socket to modify.
 * @param event    The epoll_event struct defining the new events to monitor.
 * @param events   The new event flags (EPOLLIN, EPOLLOUT, etc.)
 * @return true on success, false on failure.
 */
bool epoll_ctl_mod(int epoll_fd, int sock_fd, struct epoll_event* event, uint32_t events);

/**
 * @brief Deletes a file descriptor from the epoll instance.
 *
 * @param epoll_fd The file descriptor of the epoll instance.
 * @param sock_fd  The file descriptor of the socket to delete.
 * @return true on success, false on failure.
 */
bool epoll_ctl_del(int epoll_fd, int sock_fd);

/**
 * @brief Removes a file descriptor from epoll and closes it.
 *
 * @param epoll_fd  The file descriptor of the epoll instance.
 * @param client_fd The file descriptor of the client socket to close.
 */
void epoll_close(int epoll_fd, int client_fd);

/**
 * @brief Retrieves the IP address of the peer connected to a socket.
 *
 * @param client_fd The file descriptor of the client socket.
 * @param ipstr     A buffer to store the IP address string (must be at least INET6_ADDRSTRLEN bytes).
 * @return true on success, false on failure.
 */
bool get_peer_address(int client_fd, char ipstr[INET6_ADDRSTRLEN]);

/**
 * @brief Creates a socket, binds it to the specified port, and sets it to listen for connections.
 *
 * @param port      The port number to bind the socket to.
 * @param reuseport A flag indicating whether to allow reuse of the port.
 * @return The file descriptor of the listening socket on success, -1 on failure.
 */
int epoll_create_and_bind_socket(const char* port, bool reuseport);

/**
 * @brief Sends all data in a buffer to a socket, handling partial sends and retries.
 *
 * @param sock_fd The file descriptor of the socket to send data to.
 * @param buf     A pointer to the data buffer.
 * @param size    The size of the data buffer in bytes.
 * @return The total number of bytes sent on success, -1 on failure.
 */
ssize_t epoll_sendall(int sock_fd, const void* buf, size_t size);

/**
 * @brief Peeks at the data available on a socket without consuming it.
 *
 * @param client_fd  The file descriptor of the socket to peek at.
 * @param buffer     A buffer to store the peeked data.
 * @param buffer_len The size of the buffer.
 * @return The number of bytes peeked on success, -1 on error.
 */
ssize_t epoll_peek(int client_fd, char* buffer, size_t buffer_len);

/**
 * @brief Reads data from a socket.
 *
 * @param client_fd  The file descriptor of the socket to read from.
 * @param buffer     A buffer to store the received data.
 * @param buffer_len The maximum number of bytes to read.
 * @return The number of bytes read on success, 0 on connection close, -1 on error.
 */
ssize_t epoll_read(int client_fd, char* buffer, size_t buffer_len);

/**
 * @brief Reads data from a socket, waiting until the requested number of bytes have been read.
 *
 * @param client_fd  The file descriptor of the socket to read from.
 * @param buffer     A buffer to store the received data.
 * @param buffer_len The number of bytes to read.
 * @return The number of bytes read on success, -1 on error.  Will only return less than buffer_len
 *         bytes if a signal interrupts the call or an error occurs.
 */
ssize_t epoll_read_all(int client_fd, char* buffer, size_t buffer_len);

/**
 * @brief Sets a signal handler for a given signal.
 *
 *        This function registers a handler function to be called when the specified signal is received.
 *        It also ignores the SIGPIPE signal, which can occur when writing to a closed socket.
 *
 * @param handler A pointer to the signal handler function.
 * @return 0 on success, -1 on failure.
 */
int epoll_set_handler(void (*handler)(int sig));

/**
 * @brief Defines a structure for holding I/O callback functions.
 */
typedef struct {
    // Request handler.
    void (*request_handler)(EpollConn* conn);

    // This callback should report EOM.
    bool (*end_of_message)(const char* write_buffer, size_t length);
} io_callbacks;

/**
 * @brief Runs the main epoll event loop.
 *
 *        This function listens for incoming connections on the specified server socket and processes events
 *        using the provided I/O callback functions.
 *
 * @param server_fd The file descriptor of the server bound socket.
 * @param io_cb     A pointer to the io_callbacks structure containing the I/O callback functions.
 * @return 0 on success, 1 on failure.
 */
int epoll_eventloop(int server_fd, io_callbacks* io_cb);

/**
 * @brief Shutsdown the server eventloop.
 */
void epoll_shutdown(void);

/**
 * @brief Write message of length "length" to the connection buffer, growing it if necessary.

    @return true on success, false otherwise.
 */
bool epoll_write(EpollConn* conn, const void* msg, size_t length);

/**
 * @brief Parse request line, populating rh HTTP method, Path, http version and start of headers.
 * @return true on success, false otherwise.
 */
bool epoll_parse_request_line(char* req_data, char** method, char** uri, char** http_version,
                              char** header_start);

#endif  // EPOLL_H
