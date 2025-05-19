/*
 * network.c - Network operations for banking client
 */

#include "bank_client.h"

/* Global client socket */
int client_socket = -1;

/* Connect to the banking server */
int connect_to_server(const char *server_ip, int port) {
    struct sockaddr_in server_addr;
    
    printf("Attempting to connect to server at %s:%d...\n", server_ip, port);
    sleep(SHORT_WAIT);
    
    // Create socket
    printf("Creating client socket...\n");
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Failed to create socket");
        return -1;
    }
    sleep(SHORT_WAIT);
    
    // Prepare server address structure
    printf("Preparing server address structure...\n");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    sleep(SHORT_WAIT);
    
    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    sleep(SHORT_WAIT);
    
    // Connect to server
    printf("Connecting to server...\n");
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    printf("Connected to bank server at %s:%d\n", server_ip, port);
    sleep(SHORT_WAIT);
    return 0;
}

/* Disconnect from the banking server */
void disconnect_from_server(void) {
    if (client_socket >= 0) {
        // Send quit command to server
        request_t request;
        response_t response;
        
        printf("Preparing to disconnect from server...\n");
        memset(&request, 0, sizeof(request));
        request.command = QUIT;
        
        printf("Sending QUIT command to server...\n");
        if (send(client_socket, &request, sizeof(request), 0) < 0) {
            perror("Failed to send QUIT command");
        } else {
            printf("QUIT command sent successfully\n");
        }
        sleep(SHORT_WAIT);
        
        // Wait for server response
        printf("Waiting for server acknowledgment...\n");
        if (recv(client_socket, &response, sizeof(response), 0) > 0) {
            // Interpret the disconnect response
            interpret_response(&response, QUIT, 0);
        } else {
            printf("No acknowledgment received from server\n");
        }
        sleep(SHORT_WAIT);
        
        printf("Closing client socket...\n");
        close(client_socket);
        client_socket = -1;
        printf("Disconnected from bank server\n");
        sleep(SHORT_WAIT);
    }
}