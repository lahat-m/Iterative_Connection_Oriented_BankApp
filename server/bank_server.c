/*
 * Banking System - Server networking implementation
 */

#include "bank_server.h"
#include "bank_account.h"
#include "bank_persistence.h"
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Global server variables */
int server_socket = -1; /* Server socket */
int running = 1;        /* Server running flag */

/* Signal handler for graceful shutdown */
void shutdown_server(int signal) {
    running = 0;
    printf("Received signal %d, shutting down server...\n", signal);
    
    // Save data before exiting
    save_data();
    
    // Close server socket
    if (server_socket > 0) {
        close(server_socket);
    }
    exit(0);
}

/* Handle a client connection */
void handle_client(int client_socket) {
    request_t request;
    response_t response;
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    // Get client IP address
    if (getpeername(client_socket, (struct sockaddr *)&addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &(addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(addr.sin_port);
        printf("Handling new client connection from %s:%d\n", client_ip, client_port);
    } else {
        strcpy(client_ip, "unknown");
        printf("Handling new client connection from unknown address\n");
    }

    sleep(SHORT_WAIT);

    while (1)
    {
        // Reset response structure
        memset(&response, 0, sizeof(response));

        printf("Waiting to receive request from client %s...\n", client_ip);

        // Receive client request
        ssize_t bytes_received = recv(client_socket, &request, sizeof(request), 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                printf("Client %s disconnected\n", client_ip);
            }
            else
            {
                printf("Error receiving data from client %s: %s\n", client_ip, strerror(errno));
            }
            sleep(SHORT_WAIT);
            break;
        }

        printf("Received command %d from client %s (bytes: %ld)\n",
               request.command, client_ip, bytes_received);
        sleep(SHORT_WAIT);

        // Process request based on command
        switch (request.command)
        {
        case OPEN:
        {
            printf("Processing OPEN ACCOUNT command...\n");
            sleep(SHORT_WAIT);

            account_t *account = open_account(request.name, request.nat_id, request.account_type);
            if (account)
            {
                response.status = STATUS_OK;
                response.account_number = account->number;
                response.pin = account->pin;
                response.balance = account->balance;
                snprintf(response.message, sizeof(response.message),
                         "Account created. Number=%d Pin=%04d Balance=%d",
                         account->number, account->pin, account->balance);

                printf("Account created successfully: Number=%d, PIN=%04d\n",
                       account->number, account->pin);
            }
            else
            {
                response.status = STATUS_ERROR;
                strcpy(response.message, "Failed to create account: Bank full or error");
                printf("Failed to create account for client %s\n", client_ip);
            }
            sleep(SHORT_WAIT);
            break;
        }

        case CLOSE:
        {
            printf("Processing CLOSE ACCOUNT command...\n");
            sleep(SHORT_WAIT);

            int result = close_account(request.account_number, request.pin);
            response.status = result;
            if (result == STATUS_OK)
            {
                strcpy(response.message, "Account closed successfully");
                printf("Successfully closed account %d\n", request.account_number);
            }
            else
            {
                strcpy(response.message, "Failed to close account: Account not found or wrong PIN");
                printf("Failed to close account %d (not found or wrong PIN)\n",
                       request.account_number);
            }
            sleep(SHORT_WAIT);
            break;
        }

        case DEPOSIT:
        {
            printf("Processing DEPOSIT command...\n");
            sleep(SHORT_WAIT);

            int result = deposit(request.account_number, request.pin, request.amount);
            response.status = result;
            if (result == STATUS_OK)
            {
                int bal;
                balance(request.account_number, request.pin, &bal);
                response.balance = bal;
                snprintf(response.message, sizeof(response.message),
                         "Deposit successful. New balance: %d", bal);
                printf("Deposit successful: Account=%d, Amount=%d, New Balance=%d\n",
                       request.account_number, request.amount, bal);
            }
            else if (result == STATUS_INVALID)
            {
                snprintf(response.message, sizeof(response.message),
                         "Deposit rejected: Amount must be at least %d", MIN_DEPOSIT);
                printf("Deposit rejected: Amount %d is below minimum %d\n",
                       request.amount, MIN_DEPOSIT);
            }
            else
            {
                strcpy(response.message, "Deposit failed: Account not found or wrong PIN");
                printf("Deposit failed: Account %d not found or wrong PIN\n",
                       request.account_number);
            }
            sleep(SHORT_WAIT);
            break;
        }

        case WITHDRAW:
        {
            printf("Processing WITHDRAW command for client %s\n", client_ip);

            int result = withdraw(request.account_number, request.pin, request.amount);
            response.status = result;
            if (result == STATUS_OK)
            {
                int bal;
                balance(request.account_number, request.pin, &bal);
                response.balance = bal;
                snprintf(response.message, sizeof(response.message),
                         "Withdrawal successful. New balance: %d", bal);
                printf("Withdrawal successful: Account=%d, Amount=%d, New Balance=%d\n",
                       request.account_number, request.amount, bal);
            }
            else if (result == STATUS_MIN_AMT)
            {
                strcpy(response.message, "Withdrawal rejected: Would break minimum balance");
                printf("Withdrawal rejected: Would break minimum balance for account %d\n",
                       request.account_number);
            }
            else if (result == STATUS_INVALID)
            {
                snprintf(response.message, sizeof(response.message),
                         "Withdrawal rejected: Must be >= %d and multiple of %d",
                         MIN_WITHDRAW, MIN_WITHDRAW);
                printf("Withdrawal rejected: Amount %d not valid for account %d\n",
                       request.amount, request.account_number);
            }
            else
            {
                strcpy(response.message, "Withdrawal failed: Account not found or wrong PIN");
                printf("Withdrawal failed: Account %d not found or wrong PIN\n",
                       request.account_number);
            }
            break;
        }

        case BALANCE:
        {
            printf("Processing BALANCE command for client %s\n", client_ip);

            int bal;
            int result = balance(request.account_number, request.pin, &bal);
            response.status = result;
            if (result == STATUS_OK)
            {
                response.balance = bal;
                snprintf(response.message, sizeof(response.message), "Balance: %d", bal);
                printf("Balance request successful: Account=%d, Balance=%d\n",
                       request.account_number, bal);
            }
            else
            {
                strcpy(response.message, "Balance inquiry failed: Account not found or wrong PIN");
                printf("Balance inquiry failed: Account %d not found or wrong PIN\n",
                       request.account_number);
            }
            break;
        }

        case STATEMENT:
        {
            printf("Processing STATEMENT command for client %s\n", client_ip);

            int result = statement(request.account_number, request.pin, &response);
            response.status = result;
            if (result == STATUS_OK)
            {
                strcpy(response.message, "Statement retrieved successfully");
                printf("Statement request successful: Account=%d, Transactions=%d\n",
                       request.account_number, response.transaction_count);
            }
            else
            {
                strcpy(response.message, "Statement request failed: Account not found or wrong PIN");
                printf("Statement request failed: Account %d not found or wrong PIN\n",
                       request.account_number);
            }
            break;
        }

        case QUIT:
        {
            printf("Client %s requested to quit\n", client_ip);
            // Send termination response
            response.status = STATUS_OK;
            strcpy(response.message, "Shutting Down...");
            printf("Sending termination message to client %s\n", client_ip);
            send(client_socket, &response, sizeof(response), 0);
            printf("Closing connection with client %s\n", client_ip);
            close(client_socket);
            return;
        }

        default:
        {
            printf("Unknown command %d from client %s\n", request.command, client_ip);
            response.status = STATUS_ERROR;
            strcpy(response.message, "Unknown command");
            break;
        }
        }

        printf("Preparing to send response to client %s...\n", client_ip);
        sleep(SHORT_WAIT);

        // Send response back to client
        ssize_t bytes_sent = send(client_socket, &response, sizeof(response), 0);
        if (bytes_sent < 0)
        {
            printf("Error sending response to client %s: %s\n",
                   client_ip, strerror(errno));
            sleep(SHORT_WAIT);
            break;
        }

        printf("Response sent to client %s (bytes: %ld)\n", client_ip, bytes_sent);
        printf("Ready for next request from client %s\n", client_ip);
        sleep(SHORT_WAIT);
    }

    close(client_socket);
    printf("Connection with client %s closed\n", client_ip);
}

/* Start the server and run the main loop */
int start_server(int port)
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    printf("Bank server starting on port %d\n", port);
    
    // Create and configure socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return -1;
    }
    
    // Set socket to reuse address
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        return -1;
    }
    
    // Start listening
    if (listen(server_socket, 5) < 0) {
        perror("Failed to listen on socket");
        close(server_socket);
        return -1;
    }
    
    printf("Bank server running on port %d\n", port);
    
    // Main server loop
    while (running) {
        printf("\nWaiting for incoming connection...\n");
        
        // Accept connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR && !running) {
                break;  // Server is shutting down
            }
            perror("Failed to accept connection");
            sleep(SHORT_WAIT);
            continue;
        }
        
        // Get client details
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("Connection accepted from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        
        // Handle client
        handle_client(client_socket);
        printf("Finished handling client %s\n", client_ip);
    }
    
    // Cleanup
    close(server_socket);
    printf("Bank server shutdown complete\n");
    return 0;
}