#ifndef __NET_H__
#define __NET_H__

// Create a socket and establish a TCP connection
int connectToServer(const char* serverIP, int port);

// Close the socket connection
void closeConnection(int sockfd);

// Send data over the network
int sendData(int sockfd, const char* data);

// Receive data from the network
int receiveData(int sockfd, char* buffer, int bufferSize);

// Function to bind and listen on a server socket
int startServer(int port, int backlog);

// Accept incoming client connections
int acceptClient(int serverSocket);

// Send data to a specific client
int sendToClient(int clientSocket, const char* data);

// Receive data from a specific client
int receiveFromClient(int clientSocket, char* buffer, int bufferSize);

#endif /* __NET_H__ */
