/*
 * Banking System Client - Main Program
 *
 * Compile: gcc -std=c99 -Wall -o bank_client main.c network.c interpreter.c logger.c
 * Run: ./bank_client [server_ip] [port]
 */

#include "bank_client.h"

int main(int argc, char *argv[]) {
    char server_ip[16] = DEFAULT_SERVER;
    int port = DEFAULT_PORT;
    int choice;
    
    // Initialize logging
    log_init();
    log_message(LOG_INFO, "Bank client starting");
    printf("Bank client starting...\n");
    sleep(SHORT_WAIT);
    
    // Parse command line arguments
    if (argc > 1) {
        log_message(LOG_INFO, "Using server IP from command line: %s", argv[1]);
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
    }
    
    if (argc > 2) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            log_message(LOG_WARNING, "Invalid port number %d. Using default port %d", port, DEFAULT_PORT);
            fprintf(stderr, "Invalid port number. Using default port %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        } else {
            log_message(LOG_INFO, "Using port from command line: %d", port);
        }
    }
    
    log_message(LOG_INFO, "Target server: %s:%d", server_ip, port);
    printf("Target server: %s:%d\n", server_ip, port);
    sleep(SHORT_WAIT);
    
    // Connect to the server
    if (connect_to_server(server_ip, port) != 0) {
        log_message(LOG_ERROR, "Failed to connect to server at %s:%d", server_ip, port);
        fprintf(stderr, "Failed to connect to server at %s:%d\n", server_ip, port);
        log_close();
        return EXIT_FAILURE;
    }
    
    // Main client loop
    log_message(LOG_INFO, "Entering main client loop");
    display_banner();
    
    while (1) {
        printf("\n> "); 
        fflush(stdout);
        
        if (scanf("%d", &choice) != 1) {
            log_message(LOG_WARNING, "Invalid input received");
            printf("Invalid input, please try again\n");
            // Clear input buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            continue;
        }
        
        log_message(LOG_INFO, "User selected option: %d", choice);
        
        switch (choice) {
            case CMD_OPEN:
                log_message(LOG_INFO, "User requested to open a new account");
                open_account();
                break;
                
            case CMD_CLOSE:
                log_message(LOG_INFO, "User requested to close an account");
                close_account();
                break;
                
            case CMD_DEPOSIT:
                log_message(LOG_INFO, "User requested to make a deposit");
                deposit();
                break;
                
            case CMD_WITHDRAW:
                log_message(LOG_INFO, "User requested to make a withdrawal");
                withdraw();
                break;
                
            case CMD_BALANCE:
                log_message(LOG_INFO, "User requested to check account balance");
                check_balance();
                break;
                
            case CMD_STATEMENT:
                log_message(LOG_INFO, "User requested account statement");
                get_statement();
                break;
                
            case CMD_QUIT:
                log_message(LOG_INFO, "User requested to quit the application");
                disconnect_from_server();
                log_message(LOG_INFO, "Client exiting normally");
                printf("Bye.\n");
                log_close();
                return EXIT_SUCCESS;
                
            default:
                log_message(LOG_WARNING, "Invalid choice: %d", choice);
                printf("!! Invalid choice\n");
                break;
        }
    }
    
    // Clean up (shouldn't reach here)
    log_message(LOG_INFO, "Unexpected exit from main loop");
    disconnect_from_server();
    log_close();
    return EXIT_SUCCESS;
}