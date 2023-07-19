#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "net.h"

#define PORT 12345
#define BACKLOG 5
#define BUFFER_SIZE 1024

// Function to handle a client connection in a separate thread
void* handleClient(void* arg) {
    int clientSocket = *((int*)arg);
    free(arg);

    printf("Accepted new client connection\n");

    // Receive and display data from client
    char buffer[BUFFER_SIZE];
    int bytesRead = receiveFromClient(clientSocket, buffer, BUFFER_SIZE);
    if (bytesRead == -1) {
        fprintf(stderr, "Failed to receive data from client\n");
        closeConnection(clientSocket);
        pthread_exit(NULL);
    }

    printf("Received data from client: %s\n", buffer);

    // Send response to client
    const char* response = "Hello from server!";
    int bytesSent        = sendToClient(clientSocket, response);
    if (bytesSent == -1) {
        fprintf(stderr, "Failed to send data to client\n");
    } else {
        printf("Sent response to client\n");
    }

    // Close client connection
    closeConnection(clientSocket);
    pthread_exit(NULL);
}

int main() {
    int serverSocket = startServer(PORT, BACKLOG);
    if (serverSocket == -1) {
        fprintf(stderr, "Failed to start the server\n");
        return 1;
    }

    printf("Server started. Listening on port %d\n", PORT);

    while (1) {
        // Accept incoming client connection
        int clientSocket = acceptClient(serverSocket);
        if (clientSocket == -1) {
            fprintf(stderr, "Failed to accept client connection\n");
            continue;
        }

        // Create a new thread to handle the client connection
        pthread_t thread;
        int* socketPtr = (int*)malloc(sizeof(int));
        *socketPtr     = clientSocket;

        if (pthread_create(&thread, NULL, handleClient, (void*)socketPtr) !=
            0) {
            fprintf(stderr, "Failed to create thread for client connection\n");
            free(socketPtr);
            closeConnection(clientSocket);
            continue;
        }

        // Detach the thread to allow it to run independently
        pthread_detach(thread);
    }

    // Stop the server and close the server socket
    closeConnection(serverSocket);
    return 0;
}
