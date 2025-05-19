/*
 * Banking System Client - Logic Module
 */

#include "bank_client.h"

/* Display the main menu banner */
void display_banner(void) {
    log_message(LOG_INFO, "Displaying main menu banner");
    printf("\n============== BANK CLIENT (Network Version) ==============\n");
    printf("1: Open  2: Close  3: Deposit  4: Withdraw  5: Balance\n");
    printf("6: Statement  0: Quit\n");
    printf("----------------------------------------------------------\n");
}

/* Open a new account */
void open_account(void) {
    request_t request;
    response_t response;
    char name[40], nat_id[20];
    int account_type;
    
    log_message(LOG_INFO, "Starting OPEN ACCOUNT operation");
    printf("Starting OPEN ACCOUNT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = CMD_OPEN;
    
    printf("Name: ");
    scanf(" %39s", name);
    strncpy(request.name, name, sizeof(request.name) - 1);
    
    printf("Nat-ID: ");
    scanf(" %19s", nat_id);
    strncpy(request.nat_id, nat_id, sizeof(request.nat_id) - 1);
    
    printf("1=Savings 2=Checking : ");
    scanf("%d", &account_type);
    request.account_type = (account_type == 1) ? SAVINGS : CHECKING;
    
    log_message(LOG_INFO, "OPEN ACCOUNT details - Name: %s, ID: %s, Type: %d", 
               request.name, request.nat_id, request.account_type);
    
    printf("Processing account creation, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    log_message(LOG_INFO, "Sending OPEN ACCOUNT request to server");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        log_message(LOG_ERROR, "Failed to send OPEN ACCOUNT request: %s", strerror(errno));
        perror("Send failed");
        return;
    }
    log_message(LOG_INFO, "OPEN ACCOUNT request sent successfully");
    sleep(SHORT_WAIT);
    
    // Receive response from server
    log_message(LOG_INFO, "Waiting for server response");
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        log_message(LOG_ERROR, "Failed to receive server response: %s", strerror(errno));
        perror("Receive failed");
        return;
    }
    
    // Display server response
    log_message(LOG_INFO, "Received response from server - Status: %d, Message: %s", 
               response.status, response.message);
    
    // Interpret the response in detail
    interpret_response(&response, CMD_OPEN, 0);
    
    if (response.status == 0) {
        printf("IMPORTANT: Please save your account number and PIN for future transactions.\n");
        printf("Account Number: %d\n", response.account_number);
        printf("PIN: %04d\n", response.pin);
    }
    sleep(SHORT_WAIT);
}

/* Close an existing account */
void close_account(void) {
    request_t request;
    response_t response;
    
    log_message(LOG_INFO, "Starting CLOSE ACCOUNT operation");
    printf("Starting CLOSE ACCOUNT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = CMD_CLOSE;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    log_message(LOG_INFO, "CLOSE ACCOUNT details - Account: %d, PIN: %d", 
               request.account_number, request.pin);
    
    printf("Processing account closure, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    log_message(LOG_INFO, "Sending CLOSE ACCOUNT request to server");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        log_message(LOG_ERROR, "Failed to send CLOSE ACCOUNT request: %s", strerror(errno));
        perror("Send failed");
        return;
    }
    log_message(LOG_INFO, "CLOSE ACCOUNT request sent successfully");
    sleep(SHORT_WAIT);
    
    // Receive response from server
    log_message(LOG_INFO, "Waiting for server response");
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        log_message(LOG_ERROR, "Failed to receive server response: %s", strerror(errno));
        perror("Receive failed");
        return;
    }
    
    // Display server response
    log_message(LOG_INFO, "Received response from server - Status: %d, Message: %s", 
               response.status, response.message);
    
    // Interpret the response in detail
    interpret_response(&response, CMD_CLOSE, request.account_number);
}

/* Deposit money into an account */
void deposit(void) {
    request_t request;
    response_t response;
    
    log_message(LOG_INFO, "Starting DEPOSIT operation");
    printf("Starting DEPOSIT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = CMD_DEPOSIT;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    printf("Amount: ");
    scanf("%d", &request.amount);
    
    log_message(LOG_INFO, "DEPOSIT details - Account: %d, PIN: %d, Amount: %d", 
               request.account_number, request.pin, request.amount);
    
    printf("Processing deposit, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    log_message(LOG_INFO, "Sending DEPOSIT request to server");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        log_message(LOG_ERROR, "Failed to send DEPOSIT request: %s", strerror(errno));
        perror("Send failed");
        return;
    }
    log_message(LOG_INFO, "DEPOSIT request sent successfully");
    sleep(SHORT_WAIT);
    
    // Receive response from server
    log_message(LOG_INFO, "Waiting for server response");
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        log_message(LOG_ERROR, "Failed to receive server response: %s", strerror(errno));
        perror("Receive failed");
        return;
    }
    
    // Display server response
    log_message(LOG_INFO, "Received response from server - Status: %d, Message: %s", 
               response.status, response.message);
    
    // Interpret the response in detail
    interpret_response(&response, CMD_DEPOSIT, request.account_number);
}

/* Withdraw money from an account */
void withdraw(void) {
    request_t request;
    response_t response;
    
    log_message(LOG_INFO, "Starting WITHDRAW operation");
    printf("Starting WITHDRAW operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = CMD_WITHDRAW;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    printf("Amount: ");
    scanf("%d", &request.amount);
    
    log_message(LOG_INFO, "WITHDRAW details - Account: %d, PIN: %d, Amount: %d", 
               request.account_number, request.pin, request.amount);
    
    printf("Processing withdrawal, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    log_message(LOG_INFO, "Sending WITHDRAW request to server");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        log_message(LOG_ERROR, "Failed to send WITHDRAW request: %s", strerror(errno));
        perror("Send failed");
        return;
    }
    log_message(LOG_INFO, "WITHDRAW request sent successfully");
    sleep(SHORT_WAIT);
    
    // Receive response from server
    log_message(LOG_INFO, "Waiting for server response");
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        log_message(LOG_ERROR, "Failed to receive server response: %s", strerror(errno));
        perror("Receive failed");
        return;
    }
    
    // Display server response
    log_message(LOG_INFO, "Received response from server - Status: %d, Message: %s", 
               response.status, response.message);
    
    // Interpret the response in detail
    interpret_response(&response, CMD_WITHDRAW, request.account_number);
}

/* Check account balance */
void check_balance(void) {
    request_t request;
    response_t response;
    
    log_message(LOG_INFO, "Starting BALANCE operation");
    printf("Starting BALANCE operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = CMD_BALANCE;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    log_message(LOG_INFO, "BALANCE details - Account: %d, PIN: %d", 
               request.account_number, request.pin);
    
    printf("Retrieving balance information, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    log_message(LOG_INFO, "Sending BALANCE request to server");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        log_message(LOG_ERROR, "Failed to send BALANCE request: %s", strerror(errno));
        perror("Send failed");
        return;
    }
    log_message(LOG_INFO, "BALANCE request sent successfully");
    sleep(SHORT_WAIT);
    
    // Receive response from server
    log_message(LOG_INFO, "Waiting for server response");
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        log_message(LOG_ERROR, "Failed to receive server response: %s", strerror(errno));
        perror("Receive failed");
        return;
    }
    
    // Display server response
    log_message(LOG_INFO, "Received response from server - Status: %d, Message: %s", 
               response.status, response.message);
    
    // Interpret the response in detail
    interpret_response(&response, CMD_BALANCE, request.account_number);
}

/* Get account statement */
void get_statement(void) {
    request_t request;
    response_t response;
    
    log_message(LOG_INFO, "Starting STATEMENT operation");
    printf("Starting STATEMENT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = CMD_STATEMENT;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    log_message(LOG_INFO, "STATEMENT details - Account: %d, PIN: %d", 
               request.account_number, request.pin);
    
    printf("Generating account statement, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    log_message(LOG_INFO, "Sending STATEMENT request to server");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        log_message(LOG_ERROR, "Failed to send STATEMENT request: %s", strerror(errno));
        perror("Send failed");
        return;
    }
    log_message(LOG_INFO, "STATEMENT request sent successfully");
    sleep(SHORT_WAIT);
    
    // Receive response from server
    log_message(LOG_INFO, "Waiting for server response");
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        log_message(LOG_ERROR, "Failed to receive server response: %s", strerror(errno));
        perror("Receive failed");
        return;
    }
    
    // Display server response
    log_message(LOG_INFO, "Received response from server - Status: %d, Message: %s", 
               response.status, response.message);
    
    // Interpret the response in detail
    interpret_response(&response, CMD_STATEMENT, request.account_number);
    
    // Display transaction details in a table format if successful
    if (response.status == 0 && response.transaction_count > 0) {
        printf("\nTransaction Details:\n");
        printf("Date/Time        Type Amount  Balance\n");
        printf("---------------- ---- ------- -------\n");
        
        for (int i = 0; i < response.transaction_count; i++) {
            transaction_t *t = &response.transactions[i];
            char buf[32];
            strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&t->when));
            printf("%-16s %c    %-7d %-7d\n", buf, t->type, t->amount, t->balance_after);
        }
        printf("\n");
    }
}