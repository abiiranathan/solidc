#include <stdio.h>
#include <stdlib.h>
#include "net.h"

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024

int main() {
    // Connect to the server
    int sockfd = connectToServer(SERVER_IP, PORT);
    if (sockfd == -1) {
        fprintf(stderr, "Failed to connect to the server\n");
        return 1;
    }

    printf("Connected to server\n");

    // Send data to server
    const char* message = "Hello from client!";
    int bytesSent = sendData(sockfd, message);

    if (bytesSent == -1) {
        fprintf(stderr, "Failed to send data to server\n");
    } else {
        printf("Sent data to server\n");
    }

    // Receive response from server
    char buffer[BUFFER_SIZE];
    int bytesRead = receiveData(sockfd, buffer, BUFFER_SIZE);
    if (bytesRead == -1) {
        fprintf(stderr, "Failed to receive data from server\n");
    } else {
        printf("Received response from server: %s\n", buffer);
    }

    // Close the connection
    closeConnection(sockfd);
    return 0;
}
