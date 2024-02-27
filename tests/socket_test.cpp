#define SOCKET_IMPL
#define OS_IMPL

#include "../socket.h"  //For using Socket
#include "../os.h"      //For using Thread

#include <gtest/gtest.h>

static void init_afinet_addr(struct sockaddr_in* addr, int port) {
    addr->sin_family      = AF_INET;
    addr->sin_port        = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;  // inet_addr("127.0.0.1")
}

class SocketTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifdef _WIN32
        initialize_winsock();
#endif
        s = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ASSERT_TRUE(s != NULL) << "Unable to create socket";
    }

    void TearDown() override {
        socket_close(s);
#ifdef _WIN32
        cleanup_winsock();
#endif
    }

    Socket* s;
};

TEST_F(SocketTest, SocketBindAndListen) {
    struct sockaddr_in addr;
    init_afinet_addr(&addr, 99997);

    int enable = 1;
    socket_reuse_port(s, enable);

    ASSERT_EQ(socket_bind(s, (struct sockaddr*)&addr, sizeof(addr)), 0) << "Unable to bind socket";

    ASSERT_EQ(socket_listen(s, 3), 0) << "Unable to listen on socket";
}

TEST_F(SocketTest, SocketAccept) {
    struct sockaddr_in addr;
    init_afinet_addr(&addr, 99997);

    int enable = 1;
    socket_reuse_port(s, enable);

    ASSERT_EQ(socket_bind(s, (struct sockaddr*)&addr, sizeof(addr)), 0) << "Unable to bind socket";
    ASSERT_EQ(socket_listen(s, 3), 0) << "Unable to listen on socket";

    // connect to the socket from another thread
    // the callback has the same signature as the thread function. Use lambda to capture the socket
    Thread t;
    ThreadData args = {.arg = nullptr, .retval = nullptr};
    thread_create(
        &t,
        [](void* arg) -> void* {
            (void)arg;

            struct sockaddr_in addr;
            init_afinet_addr(&addr, 99997);

            Socket* s2 = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            // ASSERT_TRUE(s2 != nullptr) << "Unable to create socket";
            // Wait for the server to start
            printf("Waiting for server to start...\n");
            sleep(2);

            socket_connect(s2, (struct sockaddr*)&addr, sizeof(addr));

            // send some data
            const char* msg = "Hello, World!";
            ssize_t n       = socket_send(s2, msg, strlen(msg), 0);
            if (n != 13) {
                printf("Unable to send data\n");
                exit(1);
            }

            socket_close(s2);
            return nullptr;
        },
        &args);

    // accept the connection
    printf("Waiting for connection...\n");
    socklen_t len      = sizeof addr;
    Socket* new_socket = socket_accept(s, (struct sockaddr*)&addr, &len);
    ASSERT_TRUE(new_socket != nullptr) << "Unable to accept connection";

    // receive the data
    char buffer[1024] = {0};
    ssize_t n         = socket_recv(new_socket, buffer, sizeof(buffer), 0);
    ASSERT_EQ(n, 13) << "Unable to receive data";

    // Test getpeer_address
    struct sockaddr_in addr2;
    len = sizeof addr2;
    ASSERT_EQ(socket_get_peer_address(new_socket, (struct sockaddr*)&addr2, &len), 0)
        << "Unable to get peer address";
    ASSERT_EQ(addr.sin_family, addr2.sin_family) << "Address family mismatch";
    ASSERT_EQ(addr.sin_port, addr2.sin_port) << "Port mismatch";
    ASSERT_EQ(addr.sin_addr.s_addr, addr2.sin_addr.s_addr) << "Address mismatch";

    socket_close(new_socket);
}

// test socket_get_address
TEST_F(SocketTest, SocketGetAddress) {
    struct sockaddr_in addr;
    init_afinet_addr(&addr, 99997);

    int enable = 1;
    socket_reuse_port(s, enable);

    ASSERT_EQ(socket_bind(s, (struct sockaddr*)&addr, sizeof(addr)), 0) << "Unable to bind socket";
    ASSERT_EQ(socket_listen(s, 3), 0) << "Unable to listen on socket";

    struct sockaddr_in addr2;
    socklen_t len = sizeof addr2;
    ASSERT_EQ(socket_get_address(s, (struct sockaddr*)&addr2, &len), 0) << "Unable to get address";
    ASSERT_EQ(addr.sin_family, addr2.sin_family) << "Address family mismatch";
    ASSERT_EQ(addr.sin_port, addr2.sin_port) << "Port mismatch";
    ASSERT_EQ(addr.sin_addr.s_addr, addr2.sin_addr.s_addr) << "Address mismatch";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
