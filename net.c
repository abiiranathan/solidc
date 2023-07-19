#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net.h"

// Function to create a socket and establish a TCP connection
int connectToServer(const char* serverIP, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(port);

    if (inet_pton(AF_INET, serverIP, &(serverAddr.sin_addr)) <= 0) {
        perror("Invalid server IP address");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) ==
        -1) {
        perror("Connection failed");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

// Function to send data over the network
int sendData(int sockfd, const char* data) {
    ssize_t bytesSent = send(sockfd, data, strlen(data), 0);
    if (bytesSent == -1) {
        perror("Send failed");
        return -1;
    }
    return bytesSent;
}

// Function to receive data from the network
int receiveData(int sockfd, char* buffer, int bufferSize) {
    ssize_t bytesRead = recv(sockfd, buffer, bufferSize - 1, 0);
    if (bytesRead == -1) {
        perror("Receive failed");
        return -1;
    }
    buffer[bytesRead] = '\0';
    return bytesRead;
}

// Function to close the socket connection
void closeConnection(int sockfd) {
    close(sockfd);
}

// Function to bind and listen on a server socket
int startServer(int port, int backlog) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) ==
        -1) {
        perror("Bind failed");
        close(serverSocket);
        return -1;
    }

    if (listen(serverSocket, backlog) == -1) {
        perror("Listen failed");
        close(serverSocket);
        return -1;
    }

    return serverSocket;
}

// Function to accept incoming client connections
int acceptClient(int serverSocket) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    int clientSocket =
        accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket == -1) {
        perror("Accept failed");
        return -1;
    }

    return clientSocket;
}

// Function to send data to a specific client
int sendToClient(int clientSocket, const char* data) {
    ssize_t bytesSent = send(clientSocket, data, strlen(data), 0);
    if (bytesSent == -1) {
        perror("Send failed");
        return -1;
    }

    return bytesSent;
}

// Function to receive data from a specific client
int receiveFromClient(int clientSocket, char* buffer, int bufferSize) {
    ssize_t bytesRead = recv(clientSocket, buffer, bufferSize - 1, 0);
    if (bytesRead == -1) {
        perror("Receive failed");
        return -1;
    }

    buffer[bytesRead] = '\0';
    return bytesRead;
}

// Function to close a client connection
void closeClient(int clientSocket) {
    close(clientSocket);
}

// Function to stop the server and close the server socket
void stopServer(int serverSocket) {
    close(serverSocket);
}
