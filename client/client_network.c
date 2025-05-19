/*
 * network.c - Network operations for banking client
 */

#include "bank_client.h"

/* Global client socket */
int client_socket = -1;

/* Connect to the banking server */
int connect_to_server(const char *server_ip, int port) {
    struct sockaddr_in server_addr;
    
    log_message(LOG_INFO, "Attempting to connect to server at %s:%d", server_ip, port);
    printf("Attempting to connect to server at %s:%d...\n", server_ip, port);
    sleep(SHORT_WAIT);
    
    // Create socket
    log_message(LOG_INFO, "Creating client socket");
    printf("Creating client socket...\n");
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        log_message(LOG_ERROR, "Failed to create socket: %s", strerror(errno));
        perror("Failed to create socket");
        return -1;
    }
    sleep(SHORT_WAIT);
    
    // Prepare server address structure
    log_message(LOG_INFO, "Preparing server address structure");
    printf("Preparing server address structure...\n");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    sleep(SHORT_WAIT);
    
    // Convert IP address from text to binary form
    log_message(LOG_INFO, "Converting IP address from text to binary form");
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        log_message(LOG_ERROR, "Invalid address/ Address not supported: %s", strerror(errno));
        perror("Invalid address/ Address not supported");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    sleep(SHORT_WAIT);
    
    // Connect to server
    log_message(LOG_INFO, "Connecting to server");
    printf("Connecting to server...\n");
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_ERROR, "Connection failed: %s", strerror(errno));
        perror("Connection failed");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    log_message(LOG_INFO, "Successfully connected to bank server at %s:%d", server_ip, port);
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
        
        log_message(LOG_INFO, "Preparing to disconnect from server");
        printf("Preparing to disconnect from server...\n");
        memset(&request, 0, sizeof(request));
        request.command = CMD_QUIT;
        
        log_message(LOG_INFO, "Sending QUIT command to server");
        printf("Sending QUIT command to server...\n");
        if (send(client_socket, &request, sizeof(request), 0) < 0) {
            log_message(LOG_ERROR, "Failed to send QUIT command: %s", strerror(errno));
            perror("Failed to send QUIT command");
        } else {
            log_message(LOG_INFO, "QUIT command sent successfully");
            printf("QUIT command sent successfully\n");
        }
        sleep(SHORT_WAIT);
        
        // Wait for server response
        log_message(LOG_INFO, "Waiting for server acknowledgment");
        printf("Waiting for server acknowledgment...\n");
        if (recv(client_socket, &response, sizeof(response), 0) > 0) {
            log_message(LOG_INFO, "Received server acknowledgment: %s", response.message);
            
            // Interpret the disconnect response
            interpret_response(&response, CMD_QUIT, 0);
        } else {
            log_message(LOG_WARNING, "No acknowledgment received from server");
            printf("No acknowledgment received from server\n");
        }
        sleep(SHORT_WAIT);
        
        log_message(LOG_INFO, "Closing client socket");
        printf("Closing client socket...\n");
        close(client_socket);
        client_socket = -1;
        log_message(LOG_INFO, "Disconnected from bank server");
        printf("Disconnected from bank server\n");
        sleep(SHORT_WAIT);
    }
}