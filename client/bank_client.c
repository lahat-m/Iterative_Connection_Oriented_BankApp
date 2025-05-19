/*
 * Banking System Client - Main Program
 *
 * Compile: gcc -std=c99 -Wall -o bank_client main.c network.c interpreter.c
 * Run: ./bank_client [server_ip] [port] or make
 */

#include "bank_client.h"

int main(int argc, char *argv[]) {
    char server_ip[16] = DEFAULT_SERVER;
    int port = DEFAULT_PORT;
    int choice;
    
    printf("Bank client starting...\n");
    sleep(SHORT_WAIT);
    
    // Parse command line arguments
    if (argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
    }
    
    if (argc > 2) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number. Using default port %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }
    
    printf("Target server: %s:%d\n", server_ip, port);
    sleep(SHORT_WAIT);
    
    // Connect to the server
    if (connect_to_server(server_ip, port) != 0) {
        fprintf(stderr, "Failed to connect to server at %s:%d\n", server_ip, port);
        return EXIT_FAILURE;
    }
    
    // Main client loop
    display_banner();
    
    while (1) {
        printf("\n> "); 
        fflush(stdout);
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input, please try again\n");
            // Clear input buffer
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            continue;
        }
        
        switch (choice) {
            case OPEN:
                open_account();
                break;
                
            case CLOSE:
                close_account();
                break;
                
            case DEPOSIT:
                deposit();
                break;
                
            case WITHDRAW:
                withdraw();
                break;
                
            case BALANCE:
                check_balance();
                break;
                
            case STATEMENT:
                get_statement();
                break;
                
            case QUIT:
                disconnect_from_server();
                printf("Bye.\n");
                return EXIT_SUCCESS;
                
            default:
                printf("!! Invalid choice\n");
                break;
        }
    }
    
    // Clean up (shouldn't reach here)
    disconnect_from_server();
    return EXIT_SUCCESS;
}