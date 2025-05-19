/*
 * Banking System - Concurrent Server implementation (using processes)
 */

#define _POSIX_C_SOURCE 200809L

#include "bank_server_concurrent.h"
#include "../server/bank_log.h"
#include "../server/bank_account.h"
#include "../server/bank_persistence.h"
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <arpa/inet.h>


/* Global server variables */
int server_socket = -1;    /* Server socket */
int running = 1;           /* Server running flag */
int child_count = 0;       /* Number of active child processes */
pid_t *child_pids = NULL;  /* Array to track child PIDs */
int child_array_size = 0;  /* Size of the child PID array */

/* Function to add a child PID to the tracking array */
void track_child_process(pid_t pid) {
    // If array is full or not initialized, resize it
    if (child_count >= child_array_size) {
        int new_size = child_array_size == 0 ? 10 : child_array_size * 2;
        pid_t *new_array = realloc(child_pids, new_size * sizeof(pid_t));
        
        if (new_array == NULL) {
            log_message(LOG_ERROR, "[PARENT %d] Failed to allocate memory for child PID array", getpid());
            return;
        }
        
        child_pids = new_array;
        child_array_size = new_size;
        log_message(LOG_INFO, "[PARENT %d] Resized child PID tracking array to %d slots", 
                    getpid(), child_array_size);
    }
    
    // Add the PID to the array
    child_pids[child_count - 1] = pid;
    log_message(LOG_INFO, "[PARENT %d] Added child PID %d to tracking array at index %d", 
                getpid(), pid, child_count - 1);
}

/* Function to remove a child PID from the tracking array */
void untrack_child_process(pid_t pid) {
    int found = 0;
    
    // Find the PID in the array
    for (int i = 0; i < child_count; i++) {
        if (child_pids[i] == pid) {
            // Remove by shifting subsequent elements
            for (int j = i; j < child_count - 1; j++) {
                child_pids[j] = child_pids[j + 1];
            }
            found = 1;
            log_message(LOG_INFO, "[PARENT %d] Removed child PID %d from tracking array at index %d", 
                        getpid(), pid, i);
            break;
        }
    }
    
    if (!found) {
        log_message(LOG_WARNING, "[PARENT %d] Child PID %d not found in tracking array", 
                    getpid(), pid);
    }
}

/* Function to report all currently tracked child processes */
void report_active_children(void) {
    if (child_count == 0) {
        log_message(LOG_INFO, "[PARENT %d] No active child processes", getpid());
        return;
    }
    
    log_message(LOG_INFO, "[PARENT %d] Currently tracking %d active child processes:", 
                getpid(), child_count);
    
    for (int i = 0; i < child_count; i++) {
        log_message(LOG_INFO, "[PARENT %d] Child %d: PID %d", 
                    getpid(), i + 1, child_pids[i]);
    }
}

/* Signal handler for graceful shutdown */
void shutdown_server(int signal)
{
    running = 0;
    log_message(LOG_INFO, "[PARENT %d] Received signal %d, shutting down server...", getpid(), signal);

    // Report any remaining children
    log_message(LOG_INFO, "[PARENT %d] Preparing to shut down with %d active child processes", 
                getpid(), child_count);
    report_active_children();
    
    // Free child tracking array
    if (child_pids != NULL) {
        free(child_pids);
        child_pids = NULL;
        log_message(LOG_INFO, "[PARENT %d] Freed child PID tracking array", getpid());
    }

    // Save data before exiting
    save_data();

    // Close server socket
    if (server_socket > 0)
    {
        close(server_socket);
    }

    log_message(LOG_INFO, "[PARENT %d] Server shutdown complete", getpid());
    exit(0);
}

/* SIGCHLD handler to reap zombie processes */
void child_handler(int sig)
{
    pid_t pid;
    int status;

    // Reap all zombie child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            log_message(LOG_INFO, "[PARENT %d] Child process %d terminated with status %d", 
                        getpid(), pid, WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            log_message(LOG_WARNING, "[PARENT %d] Child process %d terminated by signal %d", 
                        getpid(), pid, WTERMSIG(status));
        }
        
        // Remove from tracking array
        untrack_child_process(pid);
        
        child_count--;
        log_message(LOG_INFO, "[PARENT %d] Child process %d reaped (zombie removed), remaining children: %d",
                   getpid(), pid, child_count);
    }
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
        log_message(LOG_INFO, "[CHILD %d] Handling client connection from %s:%d", 
                   getpid(), client_ip, client_port);
        printf("[CHILD %d] Handling client connection from %s:%d\n", 
               getpid(), client_ip, client_port);
    }
    else
    {
        strcpy(client_ip, "unknown");
        log_message(LOG_INFO, "[CHILD %d] Handling client connection from unknown address (getpeername failed: %s)",
                    getpid(), strerror(errno));
        printf("[CHILD %d] Handling client connection from unknown address\n", getpid());
    }

    sleep(SHORT_WAIT);

    while (1)
    {
        // Reset response structure
        memset(&response, 0, sizeof(response));

        log_message(LOG_INFO, "[CHILD %d] Waiting to receive request from client %s", 
                   getpid(), client_ip);
        printf("[CHILD %d] Waiting to receive request from client %s...\n", 
              getpid(), client_ip);

        // Receive client request
        ssize_t bytes_received = recv(client_socket, &request, sizeof(request), 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                log_message(LOG_INFO, "[CHILD %d] Client %s disconnected (recv returned 0)", 
                           getpid(), client_ip);
                printf("[CHILD %d] Client %s disconnected\n", getpid(), client_ip);
            }
            else
            {
                log_message(LOG_ERROR, "[CHILD %d] Error receiving data from client %s: %s",
                            getpid(), client_ip, strerror(errno));
                printf("[CHILD %d] Error receiving data from client %s: %s\n", 
                      getpid(), client_ip, strerror(errno));
            }
            sleep(SHORT_WAIT);
            break;
        }

        log_message(LOG_INFO, "[CHILD %d] Received command %d from client %s (bytes: %ld)",
                    getpid(), request.command, client_ip, bytes_received);
        printf("[CHILD %d] Received command %d from client %s (bytes: %ld)\n",
               getpid(), request.command, client_ip, bytes_received);
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

    log_message(LOG_INFO, "[PARENT %d] Bank server starting on port %d", getpid(), port);
    printf("[PARENT %d] Bank server starting...\n", getpid());
    sleep(SHORT_WAIT);

    printf("[PARENT %d] Creating server socket...\n", getpid());
    sleep(SHORT_WAIT);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        log_message(LOG_ERROR, "[PARENT %d] Failed to create socket: %s", getpid(), strerror(errno));
        perror("Failed to create socket");
        return -1;
    }
    log_message(LOG_INFO, "[PARENT %d] Server socket created successfully", getpid());
    printf("[PARENT %d] Server socket created successfully\n", getpid());
    sleep(SHORT_WAIT);

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        log_message(LOG_WARNING, "[PARENT %d] Failed to set socket options: %s", getpid(), strerror(errno));
        perror("Failed to set socket options");
    }
    else
    {
        log_message(LOG_INFO, "[PARENT %d] Socket options set successfully (SO_REUSEADDR)", getpid());
        printf("[PARENT %d] Socket options set successfully\n", getpid());
    }
    sleep(SHORT_WAIT);

    // Prepare server address structure
    printf("[PARENT %d] Preparing server address structure...\n", getpid());
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    sleep(SHORT_WAIT);

    // Bind socket
    printf("[PARENT %d] Binding socket to port %d...\n", getpid(), port);
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        log_message(LOG_ERROR, "[PARENT %d] Failed to bind socket: %s", getpid(), strerror(errno));
        perror("Failed to bind socket");
        close(server_socket);
        return -1;
    }
    log_message(LOG_INFO, "[PARENT %d] Socket successfully bound to port %d", getpid(), port);
    printf("[PARENT %d] Socket successfully bound to port %d\n", getpid(), port);
    sleep(SHORT_WAIT);

    // Listen for connections
    printf("[PARENT %d] Setting up listening queue...\n", getpid());
    if (listen(server_socket, 10) < 0) // Increased backlog
    {
        log_message(LOG_ERROR, "[PARENT %d] Failed to listen on socket: %s", getpid(), strerror(errno));
        perror("Failed to listen on socket");
        close(server_socket);
        return -1;
    }
    log_message(LOG_INFO, "[PARENT %d] Server now listening for connections (backlog: 10)", getpid());
    printf("[PARENT %d] Server now listening for connections\n", getpid());
    sleep(SHORT_WAIT);

    printf("[PARENT %d] Bank server running on port %d\n", getpid(), port);
    log_message(LOG_INFO, "[PARENT %d] Bank server ready to accept connections", getpid());

    return 0;
}

/* Run the concurrent server main loop */
void run_server(void)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Initialize child tracking array
    child_pids = malloc(10 * sizeof(pid_t));
    if (child_pids == NULL) {
        log_message(LOG_ERROR, "[PARENT %d] Failed to allocate memory for child PID tracking", getpid());
        perror("Failed to allocate memory");
        return;
    }
    child_array_size = 10;
    log_message(LOG_INFO, "[PARENT %d] Initialized child PID tracking array with capacity for %d processes", 
                getpid(), child_array_size);
    
    // Set up signal handler for SIGCHLD to prevent zombie processes
    struct sigaction sa;
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        log_message(LOG_ERROR, "[PARENT %d] Failed to set up SIGCHLD handler: %s", getpid(), strerror(errno));
        perror("Failed to set up SIGCHLD handler");
        free(child_pids);
        return;
    }
    
    log_message(LOG_INFO, "[PARENT %d] Concurrent server ready (using processes)", getpid());
    printf("[PARENT %d] Concurrent server ready (using processes)\n", getpid());

    // Main server loop
    while (running)
    {
        printf("\n[PARENT %d] Waiting for incoming connection...\n", getpid());
        log_message(LOG_INFO, "[PARENT %d] Waiting for incoming connection...", getpid());

        // Accept connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal, check if we're still running
                log_message(LOG_INFO, "[PARENT %d] accept() interrupted by signal, checking if server should continue running", getpid());
                printf("[PARENT %d] Connection interrupted by signal\n", getpid());
                sleep(SHORT_WAIT);
                continue;
            }
            log_message(LOG_ERROR, "[PARENT %d] Failed to accept connection: %s", getpid(), strerror(errno));
            perror("Failed to accept connection");
            sleep(SHORT_WAIT);
            continue;
        }

        // Get client details for logging
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        log_message(LOG_INFO, "[PARENT %d] Connection accepted from %s:%d (socket fd: %d)",
                    getpid(), client_ip, client_port, client_socket);
        printf("[PARENT %d] Connection accepted from %s:%d\n", getpid(), client_ip, client_port);
        
        // Log process creation attempt
        log_message(LOG_INFO, "[PARENT %d] Attempting to create child process for client %s:%d",
                   getpid(), client_ip, client_port);
        
        // Fork a new process to handle the client
        pid_t pid = fork();
        
        if (pid < 0) {
            // Fork failed
            log_message(LOG_ERROR, "[PARENT %d] Fork failed: %s", getpid(), strerror(errno));
            perror("Fork failed");
            close(client_socket);
            continue;
        }
        else if (pid == 0) {
            // Child process
            
            // Free child tracking array in child (not needed in child)
            if (child_pids != NULL) {
                free(child_pids);
                child_pids = NULL;
            }
            
            close(server_socket); // Child doesn't need the listening socket
            
            log_message(LOG_INFO, "[CHILD %d] Process created by parent %d to handle client %s:%d", 
                        getpid(), getppid(), client_ip, client_port);
            printf("[CHILD %d] Process created by parent %d to handle client %s:%d\n", 
                   getpid(), getppid(), client_ip, client_port);
            
            // Log child process details
            log_message(LOG_INFO, "[CHILD %d] Child process details:", getpid());
            log_message(LOG_INFO, "[CHILD %d] - Parent PID: %d", getpid(), getppid());
            log_message(LOG_INFO, "[CHILD %d] - Process Group: %d", getpid(), getpgrp());
            log_message(LOG_INFO, "[CHILD %d] - User ID: %d", getpid(), getuid());
            
            // Handle client connection
            handle_client(client_socket);
            
            // Child exits after handling client
            log_message(LOG_INFO, "[CHILD %d] Finished handling client %s:%d, exiting",
                        getpid(), client_ip, client_port);
            printf("[CHILD %d] Finished handling client %s:%d, exiting\n",
                   getpid(), client_ip, client_port);
            
            exit(0);
        }
        else {
            // Parent process
            child_count++;
            
            // Add child to tracking array
            track_child_process(pid);
            
            close(client_socket); // Parent doesn't need the client socket
            
            log_message(LOG_INFO, "[PARENT %d] Successfully created child process %d to handle client %s:%d",
                       getpid(), pid, client_ip, client_port);
            log_message(LOG_INFO, "[PARENT %d] Created child process %d to handle client %s:%d (active children: %d)",
                        getpid(), pid, client_ip, client_port, child_count);
            printf("[PARENT %d] Created child process %d to handle client %s:%d (active children: %d)\n",
                   getpid(), pid, client_ip, client_port, child_count);
            
            // Report all active children periodically (every 5 new children)
            if (child_count % 5 == 0) {
                report_active_children();
            }
            
            log_message(LOG_INFO, "[PARENT %d] Returning to accept loop for next client", getpid());
            printf("[PARENT %d] Returning to accept loop for next client\n", getpid());
        }
    }

    // Free child tracking array
    if (child_pids != NULL) {
        free(child_pids);
        child_pids = NULL;
    }
    
    // Close server socket
    close(server_socket);
    log_message(LOG_INFO, "[PARENT %d] Bank server shutdown complete", getpid());
    printf("[PARENT %d] Bank server shutdown complete\n", getpid());
}