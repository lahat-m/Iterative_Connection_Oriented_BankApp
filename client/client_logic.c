/*
 * Banking System Client - Logic Module
 */

#include "bank_client.h"

/* Display the main menu banner */
void display_banner(void) {
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
    
    printf("Starting OPEN ACCOUNT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = OPEN;
    
    printf("Name: ");
    scanf(" %39s", name);
    strncpy(request.name, name, sizeof(request.name) - 1);
    
    printf("Nat-ID: ");
    scanf(" %19s", nat_id);
    strncpy(request.nat_id, nat_id, sizeof(request.nat_id) - 1);
    
    printf("1=Savings 2=Checking : ");
    scanf("%d", &account_type);
    request.account_type = (account_type == 1) ? SAVINGS : CHECKING;
    
    printf("Processing account creation, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        perror("Send failed");
        return;
    }
    sleep(SHORT_WAIT);
    
    // Receive response from server
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        perror("Receive failed");
        return;
    }
    
    // Interpret the response in detail
    interpret_response(&response, OPEN, 0);
    
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
    
    printf("Starting CLOSE ACCOUNT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = CLOSE;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    printf("Processing account closure, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        perror("Send failed");
        return;
    }
    sleep(SHORT_WAIT);
    
    // Receive response from server
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        perror("Receive failed");
        return;
    }
    
    // Interpret the response in detail
    interpret_response(&response, CLOSE, request.account_number);
}

/* Deposit money into an account */
void deposit(void) {
    request_t request;
    response_t response;
    
    printf("Starting DEPOSIT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = DEPOSIT;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    printf("Amount: ");
    scanf("%d", &request.amount);
    
    printf("Processing deposit, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        perror("Send failed");
        return;
    }
    sleep(SHORT_WAIT);
    
    // Receive response from server
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        perror("Receive failed");
        return;
    }
    
    // Interpret the response in detail
    interpret_response(&response, DEPOSIT, request.account_number);
}

/* Withdraw money from an account */
void withdraw(void) {
    request_t request;
    response_t response;
    
    printf("Starting WITHDRAW operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = WITHDRAW;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    printf("Amount: ");
    scanf("%d", &request.amount);
    
    printf("Processing withdrawal, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        perror("Send failed");
        return;
    }
    sleep(SHORT_WAIT);
    
    // Receive response from server
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        perror("Receive failed");
        return;
    }
    
    // Interpret the response in detail
    interpret_response(&response, WITHDRAW, request.account_number);
}

/* Check account balance */
void check_balance(void) {
    request_t request;
    response_t response;
    printf("Starting BALANCE operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = BALANCE;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    printf("Retrieving balance information, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    printf("Sending BALANCE request to server...\n");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        perror("Send failed");
        return;
    }
    sleep(SHORT_WAIT);
    
    // Receive response from server
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        perror("Receive failed");
        return;
    }
    
    // Interpret the response in detail
    interpret_response(&response, BALANCE, request.account_number);
}

/* Get account statement */
void get_statement(void) {
    request_t request;
    response_t response;
    
    printf("Starting STATEMENT operation...\n");
    
    memset(&request, 0, sizeof(request));
    request.command = STATEMENT;
    
    printf("Account Number: ");
    scanf("%d", &request.account_number);
    
    printf("PIN: ");
    scanf("%d", &request.pin);
    
    printf("Generating account statement, please wait...\n");
    sleep(SHORT_WAIT);
    
    // Send request to server
    printf("Sending STATEMENT request to server...\n");
    if (send(client_socket, &request, sizeof(request), 0) < 0) {
        perror("Send failed");
        return;
    }
    sleep(SHORT_WAIT);
    
    printf("Waiting for server response...\n");
    if (recv(client_socket, &response, sizeof(response), 0) < 0) {
        perror("Receive failed");
        return;
    }
    
    // Interpret the response in detail
    interpret_response(&response, STATEMENT, request.account_number);
    
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