/*
 * Banking System - Concurrent Server implementation - using processes
 */

// #define _POSIX_C_SOURCE 200809L

#include "bank_server_concurrent.h"
#include "../server/bank_account.h"
#include "../server/bank_persistence.h"
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Global server variables */
int server_socket = -1;    /* Server socket */
int running = 1;           /* Server running flag */
int child_count = 0;       /* Number of active child processes */
pid_t *child_pids = NULL;  /* Array to track child PIDs */
int child_array_size = 0;  /* Size of the child PID array */

/* Track a child PID in the parent's tracking array */
void track_child_process(pid_t pid) {
    if (child_count >= child_array_size) {
        int new_size = child_array_size == 0 ? 10 : child_array_size * 2;
        pid_t *new_array = realloc(child_pids, new_size * sizeof(pid_t));
        
        if (new_array == NULL) {
            perror("Failed to allocate memory for child PID array");
            return;
        }
        
        child_pids = new_array;
        child_array_size = new_size;
    }
    
    child_pids[child_count - 1] = pid;
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
        printf("[CHILD %d] Handling client connection from %s:%d\n", 
               getpid(), client_ip, client_port);
    } else {
        strcpy(client_ip, "unknown");
        printf("[CHILD %d] Handling client connection from unknown address\n", getpid());
    }

    sleep(SHORT_WAIT);

    while (1) {
        // Reset response structure
        memset(&response, 0, sizeof(response));

        printf("[CHILD %d] Waiting for request from client %s...\n", getpid(), client_ip);

        // Receive client request
        ssize_t bytes_received = recv(client_socket, &request, sizeof(request), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("[CHILD %d] Client %s disconnected\n", getpid(), client_ip);
            } else {
                printf("[CHILD %d] Error receiving data from client %s: %s\n", 
                       getpid(), client_ip, strerror(errno));
            }
            sleep(SHORT_WAIT);
            break;
        }

        printf("[CHILD %d] Received command %d from client %s (bytes: %ld)\n",
               getpid(), request.command, client_ip, bytes_received);
        sleep(SHORT_WAIT);

        // Process request based on command
        switch (request.command) {
        case OPEN: {
            printf("[CHILD %d] Processing OPEN ACCOUNT command...\n", getpid());
            sleep(SHORT_WAIT);

            account_t *account = open_account(request.name, request.nat_id, request.account_type);
            if (account) {
                response.status = STATUS_OK;
                response.account_number = account->number;
                response.pin = account->pin;
                response.balance = account->balance;
                snprintf(response.message, sizeof(response.message),
                         "Account created. Number=%d Pin=%04d Balance=%d",
                         account->number, account->pin, account->balance);

                printf("[CHILD %d] Account created: Number=%d, PIN=%04d\n",
                       getpid(), account->number, account->pin);
            } else {
                response.status = STATUS_ERROR;
                strcpy(response.message, "Failed to create account: Bank full or error");
                printf("[CHILD %d] Failed to create account\n", getpid());
            }
            sleep(SHORT_WAIT);
            break;
        }

        case CLOSE: {
            printf("[CHILD %d] Processing CLOSE ACCOUNT command...\n", getpid());
            sleep(SHORT_WAIT);

            int result = close_account(request.account_number, request.pin);
            response.status = result;
            if (result == STATUS_OK) {
                strcpy(response.message, "Account closed successfully");
                printf("[CHILD %d] Successfully closed account %d\n", 
                       getpid(), request.account_number);
            } else {
                strcpy(response.message, "Failed to close account: Account not found or wrong PIN");
                printf("[CHILD %d] Failed to close account %d\n", 
                       getpid(), request.account_number);
            }
            sleep(SHORT_WAIT);
            break;
        }

        case DEPOSIT: {
            printf("[CHILD %d] Processing DEPOSIT command...\n", getpid());
            sleep(SHORT_WAIT);

            int result = deposit(request.account_number, request.pin, request.amount);
            response.status = result;
            if (result == STATUS_OK) {
                int bal;
                balance(request.account_number, request.pin, &bal);
                response.balance = bal;
                snprintf(response.message, sizeof(response.message),
                         "Deposit successful. New balance: %d", bal);
                printf("[CHILD %d] Deposit successful: Account=%d, New Balance=%d\n",
                       getpid(), request.account_number, bal);
            } else if (result == STATUS_INVALID) {
                snprintf(response.message, sizeof(response.message),
                         "Deposit rejected: Amount must be at least %d", MIN_DEPOSIT);
                printf("[CHILD %d] Deposit rejected: Amount too small\n", getpid());
            } else {
                strcpy(response.message, "Deposit failed: Account not found or wrong PIN");
                printf("[CHILD %d] Deposit failed: Invalid account/PIN\n", getpid());
            }
            sleep(SHORT_WAIT);
            break;
        }

        case WITHDRAW: {
            printf("[CHILD %d] Processing WITHDRAW command...\n", getpid());

            int result = withdraw(request.account_number, request.pin, request.amount);
            response.status = result;
            if (result == STATUS_OK) {
                int bal;
                balance(request.account_number, request.pin, &bal);
                response.balance = bal;
                snprintf(response.message, sizeof(response.message),
                         "Withdrawal successful. New balance: %d", bal);
                printf("[CHILD %d] Withdrawal successful: New Balance=%d\n",
                       getpid(), bal);
            } else if (result == STATUS_MIN_AMT) {
                strcpy(response.message, "Withdrawal rejected: Would break minimum balance");
                printf("[CHILD %d] Withdrawal rejected: Below minimum balance\n", getpid());
            } else if (result == STATUS_INVALID) {
                snprintf(response.message, sizeof(response.message),
                         "Withdrawal rejected: Must be >= %d and multiple of %d",
                         MIN_WITHDRAW, MIN_WITHDRAW);
                printf("[CHILD %d] Withdrawal rejected: Invalid amount\n", getpid());
            } else {
                strcpy(response.message, "Withdrawal failed: Account not found or wrong PIN");
                printf("[CHILD %d] Withdrawal failed: Invalid account/PIN\n", getpid());
            }
            break;
        }

        case BALANCE: {
            printf("[CHILD %d] Processing BALANCE command...\n", getpid());

            int bal;
            int result = balance(request.account_number, request.pin, &bal);
            response.status = result;
            if (result == STATUS_OK) {
                response.balance = bal;
                snprintf(response.message, sizeof(response.message), "Balance: %d", bal);
                printf("[CHILD %d] Balance request successful: %d\n", getpid(), bal);
            } else {
                strcpy(response.message, "Balance inquiry failed: Account not found or wrong PIN");
                printf("[CHILD %d] Balance inquiry failed\n", getpid());
            }
            break;
        }

        case STATEMENT: {
            printf("[CHILD %d] Processing STATEMENT command...\n", getpid());

            int result = statement(request.account_number, request.pin, &response);
            response.status = result;
            if (result == STATUS_OK) {
                strcpy(response.message, "Statement retrieved successfully");
                printf("[CHILD %d] Statement request successful\n", getpid());
            } else {
                strcpy(response.message, "Statement request failed: Account not found or wrong PIN");
                printf("[CHILD %d] Statement request failed\n", getpid());
            }
            break;
        }

        case QUIT: {
            printf("[CHILD %d] Client requested to quit\n", getpid());
            response.status = STATUS_OK;
            strcpy(response.message, "Shutting Down...");
            send(client_socket, &response, sizeof(response), 0);
            close(client_socket);
            return;
        }

        default: {
            printf("[CHILD %d] Unknown command %d\n", getpid(), request.command);
            response.status = STATUS_ERROR;
            strcpy(response.message, "Unknown command");
            break;
        }
        }

        printf("[CHILD %d] Sending response to client...\n", getpid());
        sleep(SHORT_WAIT);

        // Send response back to client
        ssize_t bytes_sent = send(client_socket, &response, sizeof(response), 0);
        if (bytes_sent < 0) {
            printf("[CHILD %d] Error sending response: %s\n", getpid(), strerror(errno));
            sleep(SHORT_WAIT);
            break;
        }

        printf("[CHILD %d] Response sent (bytes: %ld)\n", getpid(), bytes_sent);
        sleep(SHORT_WAIT);
    }

    close(client_socket);
    printf("[CHILD %d] Connection closed\n", getpid());
}

/* Start the concurrent server */
int start_concurrent_server(int port) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    
    printf("[PARENT %d] Starting concurrent bank server on port %d\n", getpid(), port);
    
    // Initialize child tracking array
    child_pids = malloc(10 * sizeof(pid_t));
    if (child_pids == NULL) {
        perror("Failed to allocate memory for child tracking");
        return -1;
    }
    child_array_size = 10;
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        free(child_pids);
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
        free(child_pids);
        return -1;
    }
    
    // Start listening
    if (listen(server_socket, 10) < 0) {
        perror("Failed to listen on socket");
        close(server_socket);
        free(child_pids);
        return -1;
    }
    
    printf("[PARENT %d] Concurrent server ready (using processes)\n", getpid());
    
    // Main server loop
    while (running) {
        printf("\n[PARENT %d] Waiting for connection...\n", getpid());
        
        // Accept connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR) {
                // Check if we should continue running
                if (!running) break;
                continue;
            }
            perror("Failed to accept connection");
            sleep(SHORT_WAIT);
            continue;
        }
        
        // Get client details
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("[PARENT %d] Connection from %s:%d\n", getpid(), client_ip, ntohs(client_addr.sin_port));
        
        // Fork to handle client
        pid_t pid = fork();
        
        if (pid < 0) {
            // Fork failed
            perror("Fork failed");
            close(client_socket);
            continue;
        } 
        else if (pid == 0) {
            // Child process
            if (child_pids != NULL) {
                free(child_pids);
                child_pids = NULL;
            }
            
            close(server_socket); /* Close listening socket in child */
            
            printf("[CHILD %d] Process created to handle client %s\n", getpid(), client_ip);
            
            // Handle client
            handle_client(client_socket);
            
            // Child exits after handling client
            printf("[CHILD %d] Finished handling client, exiting\n", getpid());
            exit(0);
        } 
        else {
            // Parent process
            child_count++;
            track_child_process(pid);
            close(client_socket); // Close client socket in parent
            
            printf("[PARENT %d] Created child process %d (active children: %d)\n",
                   getpid(), pid, child_count);
                   
            // Non-blocking wait for any finished children
            int status;
            pid_t finished_pid;
            while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
                printf("[PARENT %d] Child %d terminated\n", getpid(), finished_pid);
                child_count--;
            }
        }
    }
    
    // Cleanup before exit
    if (child_pids != NULL) {
        free(child_pids);
        child_pids = NULL;
    }
    
    close(server_socket);
    save_data();
    
    printf("[PARENT %d] Server shutdown complete\n", getpid());
    return 0;
}