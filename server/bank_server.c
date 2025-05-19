/*
 * Banking System - Server networking implementation
 */

#include "bank_server.h"
#include "bank_log.h"
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
void shutdown_server(int signal)
{
    running = 0;
    log_message(LOG_INFO, "Received signal %d, shutting down server...", signal);

    // Save data before exiting
    save_data();

    // Close server socket
    if (server_socket > 0)
    {
        close(server_socket);
    }

    log_message(LOG_INFO, "Server shutdown complete");
    exit(0);
}

/* Handle a client connection */
void handle_client(int client_socket)
{
    request_t request;
    response_t response;
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    // Get client IP address
    if (getpeername(client_socket, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &(addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(addr.sin_port);
        log_message(LOG_INFO, "Handling new client connection from %s:%d", client_ip, client_port);
        printf("Handling new client connection from %s:%d\n", client_ip, client_port);
    }
    else
    {
        strcpy(client_ip, "unknown");
        log_message(LOG_INFO, "Handling new client connection from unknown address (getpeername failed: %s)",
                    strerror(errno));
        printf("Handling new client connection from unknown address\n");
    }

    sleep(SHORT_WAIT);

    while (1)
    {
        // Reset response structure
        memset(&response, 0, sizeof(response));

        log_message(LOG_INFO, "Waiting to receive request from client %s", client_ip);
        printf("Waiting to receive request from client %s...\n", client_ip);

        // Receive client request
        ssize_t bytes_received = recv(client_socket, &request, sizeof(request), 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                log_message(LOG_INFO, "Client %s disconnected (recv returned 0)", client_ip);
                printf("Client %s disconnected\n", client_ip);
            }
            else
            {
                log_message(LOG_ERROR, "Error receiving data from client %s: %s",
                            client_ip, strerror(errno));
                printf("Error receiving data from client %s: %s\n", client_ip, strerror(errno));
            }
            sleep(SHORT_WAIT);
            break;
        }

        log_message(LOG_INFO, "Received command %d from client %s (bytes: %ld)",
                    request.command, client_ip, bytes_received);
        printf("Received command %d from client %s (bytes: %ld)\n",
               request.command, client_ip, bytes_received);
        sleep(SHORT_WAIT);

        // Process request based on command
        switch (request.command)
        {
        case OPEN:
        {
            log_message(LOG_INFO, "Processing OPEN ACCOUNT command for client %s", client_ip);
            printf("Processing OPEN ACCOUNT command...\n");
            sleep(SHORT_WAIT);

            log_message(LOG_INFO, "Request details: Name=%s, ID=%s, Type=%d",
                        request.name, request.nat_id, request.account_type);

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

                log_message(LOG_INFO, "Account created successfully: Number=%d, PIN=%04d",
                            account->number, account->pin);
                printf("Account created successfully: Number=%d, PIN=%04d\n",
                       account->number, account->pin);
            }
            else
            {
                response.status = STATUS_ERROR;
                strcpy(response.message, "Failed to create account: Bank full or error");
                log_message(LOG_ERROR, "Failed to create account for client %s", client_ip);
                printf("Failed to create account for client %s\n", client_ip);
            }
            sleep(SHORT_WAIT);
            break;
        }

        case CLOSE:
        {
            log_message(LOG_INFO, "Processing CLOSE ACCOUNT command for client %s", client_ip);
            printf("Processing CLOSE ACCOUNT command...\n");
            sleep(SHORT_WAIT);

            log_message(LOG_INFO, "Request details: Account=%d, PIN=%d",
                        request.account_number, request.pin);

            int result = close_account(request.account_number, request.pin);
            response.status = result;
            if (result == STATUS_OK)
            {
                strcpy(response.message, "Account closed successfully");
                log_message(LOG_INFO, "Successfully closed account %d", request.account_number);
                printf("Successfully closed account %d\n", request.account_number);
            }
            else
            {
                strcpy(response.message, "Failed to close account: Account not found or wrong PIN");
                log_message(LOG_WARNING, "Failed to close account %d (not found or wrong PIN)",
                            request.account_number);
                printf("Failed to close account %d (not found or wrong PIN)\n",
                       request.account_number);
            }
            sleep(SHORT_WAIT);
            break;
        }

        case DEPOSIT:
        {
            log_message(LOG_INFO, "Processing DEPOSIT command for client %s", client_ip);
            printf("Processing DEPOSIT command...\n");
            sleep(SHORT_WAIT);

            log_message(LOG_INFO, "Request details: Account=%d, PIN=%d, Amount=%d",
                        request.account_number, request.pin, request.amount);

            int result = deposit(request.account_number, request.pin, request.amount);
            response.status = result;
            if (result == STATUS_OK)
            {
                int bal;
                balance(request.account_number, request.pin, &bal);
                response.balance = bal;
                snprintf(response.message, sizeof(response.message),
                         "Deposit successful. New balance: %d", bal);
                log_message(LOG_INFO, "Deposit successful: Account=%d, Amount=%d, New Balance=%d",
                            request.account_number, request.amount, bal);
                printf("Deposit successful: Account=%d, Amount=%d, New Balance=%d\n",
                       request.account_number, request.amount, bal);
            }
            else if (result == STATUS_INVALID)
            {
                snprintf(response.message, sizeof(response.message),
                         "Deposit rejected: Amount must be at least %d", MIN_DEPOSIT);
                log_message(LOG_WARNING, "Deposit rejected: Amount %d is below minimum %d",
                            request.amount, MIN_DEPOSIT);
                printf("Deposit rejected: Amount %d is below minimum %d\n",
                       request.amount, MIN_DEPOSIT);
            }
            else
            {
                strcpy(response.message, "Deposit failed: Account not found or wrong PIN");
                log_message(LOG_WARNING, "Deposit failed: Account %d not found or wrong PIN",
                            request.account_number);
                printf("Deposit failed: Account %d not found or wrong PIN\n",
                       request.account_number);
            }
            sleep(SHORT_WAIT);
            break;
        }

        case WITHDRAW:
        {
            log_message(LOG_INFO, "Processing WITHDRAW command for client %s", client_ip);
            log_message(LOG_INFO, "Request details: Account=%d, PIN=%d, Amount=%d",
                        request.account_number, request.pin, request.amount);

            int result = withdraw(request.account_number, request.pin, request.amount);
            response.status = result;
            if (result == STATUS_OK)
            {
                int bal;
                balance(request.account_number, request.pin, &bal);
                response.balance = bal;
                snprintf(response.message, sizeof(response.message),
                         "Withdrawal successful. New balance: %d", bal);
                log_message(LOG_INFO, "Withdrawal successful: Account=%d, Amount=%d, New Balance=%d",
                            request.account_number, request.amount, bal);
            }
            else if (result == STATUS_MIN_AMT)
            {
                strcpy(response.message, "Withdrawal rejected: Would break minimum balance");
                log_message(LOG_WARNING, "Withdrawal rejected: Would break minimum balance for account %d",
                            request.account_number);
            }
            else if (result == STATUS_INVALID)
            {
                snprintf(response.message, sizeof(response.message),
                         "Withdrawal rejected: Must be >= %d and multiple of %d",
                         MIN_WITHDRAW, MIN_WITHDRAW);
                log_message(LOG_WARNING, "Withdrawal rejected: Amount %d not valid for account %d",
                            request.amount, request.account_number);
            }
            else
            {
                strcpy(response.message, "Withdrawal failed: Account not found or wrong PIN");
                log_message(LOG_WARNING, "Withdrawal failed: Account %d not found or wrong PIN",
                            request.account_number);
            }
            break;
        }

        case BALANCE:
        {
            log_message(LOG_INFO, "Processing BALANCE command for client %s", client_ip);
            log_message(LOG_INFO, "Request details: Account=%d, PIN=%d",
                        request.account_number, request.pin);

            int bal;
            int result = balance(request.account_number, request.pin, &bal);
            response.status = result;
            if (result == STATUS_OK)
            {
                response.balance = bal;
                snprintf(response.message, sizeof(response.message), "Balance: %d", bal);
                log_message(LOG_INFO, "Balance request successful: Account=%d, Balance=%d",
                            request.account_number, bal);
            }
            else
            {
                strcpy(response.message, "Balance inquiry failed: Account not found or wrong PIN");
                log_message(LOG_WARNING, "Balance inquiry failed: Account %d not found or wrong PIN",
                            request.account_number);
            }
            break;
        }

        case STATEMENT:
        {
            log_message(LOG_INFO, "Processing STATEMENT command for client %s", client_ip);
            log_message(LOG_INFO, "Request details: Account=%d, PIN=%d",
                        request.account_number, request.pin);

            int result = statement(request.account_number, request.pin, &response);
            response.status = result;
            if (result == STATUS_OK)
            {
                strcpy(response.message, "Statement retrieved successfully");
                log_message(LOG_INFO, "Statement request successful: Account=%d, Transactions=%d",
                            request.account_number, response.transaction_count);
            }
            else
            {
                strcpy(response.message, "Statement request failed: Account not found or wrong PIN");
                log_message(LOG_WARNING, "Statement request failed: Account %d not found or wrong PIN",
                            request.account_number);
            }
            break;
        }

        case QUIT:
        {
            log_message(LOG_INFO, "Client %s requested to quit", client_ip);
            // Send termination response
            response.status = STATUS_OK;
            strcpy(response.message, "Shutting Down...");
            log_message(LOG_INFO, "Sending termination message to client %s", client_ip);
            send(client_socket, &response, sizeof(response), 0);
            log_message(LOG_INFO, "Closing connection with client %s", client_ip);
            close(client_socket);
            return;
        }

        default:
        {
            log_message(LOG_WARNING, "Unknown command %d from client %s",
                        request.command, client_ip);
            response.status = STATUS_ERROR;
            strcpy(response.message, "Unknown command");
            break;
        }
        }

        log_message(LOG_INFO, "Preparing to send response to client %s (status: %d)",
                    client_ip, response.status);
        printf("Preparing to send response to client %s...\n", client_ip);
        sleep(SHORT_WAIT);

        // Send response back to client
        ssize_t bytes_sent = send(client_socket, &response, sizeof(response), 0);
        if (bytes_sent < 0)
        {
            log_message(LOG_ERROR, "Error sending response to client %s: %s",
                        client_ip, strerror(errno));
            printf("Error sending response to client %s: %s\n",
                   client_ip, strerror(errno));
            sleep(SHORT_WAIT);
            break;
        }

        log_message(LOG_INFO, "Response sent to client %s (bytes: %ld)", client_ip, bytes_sent);
        printf("Response sent to client %s (bytes: %ld)\n", client_ip, bytes_sent);
        log_message(LOG_INFO, "Ready for next request from client %s", client_ip);
        printf("Ready for next request from client %s\n", client_ip);
        sleep(SHORT_WAIT);
    }

    close(client_socket);
    log_message(LOG_INFO, "Connection with client %s closed", client_ip);
    printf("Connection with client %s closed\n", client_ip);
}

/* Initialize the server */
int init_server(int port)
{
    struct sockaddr_in server_addr;

    log_message(LOG_INFO, "Bank server starting on port %d", port);
    printf("Bank server starting...\n");
    sleep(SHORT_WAIT);

    printf("Creating server socket...\n");
    sleep(SHORT_WAIT);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        log_message(LOG_ERROR, "Failed to create socket: %s", strerror(errno));
        perror("Failed to create socket");
        return -1;
    }
    log_message(LOG_INFO, "Server socket created successfully");
    printf("Server socket created successfully\n");
    sleep(SHORT_WAIT);

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        log_message(LOG_WARNING, "Failed to set socket options: %s", strerror(errno));
        perror("Failed to set socket options");
    }
    else
    {
        log_message(LOG_INFO, "Socket options set successfully (SO_REUSEADDR)");
        printf("Socket options set successfully\n");
    }
    sleep(SHORT_WAIT);

    // Prepare server address structure
    printf("Preparing server address structure...\n");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    sleep(SHORT_WAIT);

    // Bind socket
    printf("Binding socket to port %d...\n", port);
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        log_message(LOG_ERROR, "Failed to bind socket: %s", strerror(errno));
        perror("Failed to bind socket");
        close(server_socket);
        return -1;
    }
    log_message(LOG_INFO, "Socket successfully bound to port %d", port);
    printf("Socket successfully bound to port %d\n", port);
    sleep(SHORT_WAIT);

    // Listen for connections
    printf("Setting up listening queue...\n");
    if (listen(server_socket, 5) < 0)
    {
        log_message(LOG_ERROR, "Failed to listen on socket: %s", strerror(errno));
        perror("Failed to listen on socket");
        close(server_socket);
        return -1;
    }
    log_message(LOG_INFO, "Server now listening for connections (backlog: 5)");
    printf("Server now listening for connections\n");
    sleep(SHORT_WAIT);

    printf("Bank server running on port %d\n", port);
    log_message(LOG_INFO, "Bank server ready to accept connections");

    return 0;
}

/* Run the server main loop */
void run_server(void)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Main server loop
    while (running)
    {
        printf("\nWaiting for incoming connection...\n");
        log_message(LOG_INFO, "Waiting for incoming connection...");

        // Accept connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal, check if we're still running
                log_message(LOG_INFO, "accept() interrupted by signal, checking if server should continue running");
                printf("Connection interrupted by signal\n");
                sleep(SHORT_WAIT);
                continue;
            }
            log_message(LOG_ERROR, "Failed to accept connection: %s", strerror(errno));
            perror("Failed to accept connection");
            sleep(SHORT_WAIT);
            continue;
        }

        // Get client details for logging
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        log_message(LOG_INFO, "Connection accepted from %s:%d (socket fd: %d)",
                    client_ip, client_port, client_socket);
        printf("Connection accepted from %s:%d\n", client_ip, client_port);
        sleep(SHORT_WAIT);

        // Handle client in the same process (iterative server)
        printf("Handling client requests...\n");
        handle_client(client_socket);
        log_message(LOG_INFO, "Finished handling client %s:%d, returning to accept loop",
                    client_ip, client_port);
        printf("Finished handling client %s:%d\n", client_ip, client_port);
        sleep(SHORT_WAIT);
    }

    // Close server socket
    printf("Closing server socket...\n");
    close(server_socket);

    log_message(LOG_INFO, "Bank server shutdown complete");
    printf("Bank server shutdown complete\n");
}