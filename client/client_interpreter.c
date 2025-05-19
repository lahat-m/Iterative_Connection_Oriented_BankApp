/*
 * Banking System Client - Interpreter Module
 */

#include "bank_client.h"

/* Interpret server response and log details */
void interpret_response(response_t *response, command_t command, int account_number) {
    log_message(LOG_INFO, "Interpreting server response for command %d (Account: %d)", command, account_number);
    
    printf("\n----- SERVER RESPONSE INTERPRETATION -----\n");
    
    // Interpret status code
    printf("Status code: %d - ", response->status);
    switch (response->status) {
        case 0:  // STATUS_OK
            printf("SUCCESS\n");
            log_message(LOG_INFO, "Response status indicates SUCCESS");
            break;
        case -1:  // STATUS_ERROR
            printf("ERROR (General error)\n");
            log_message(LOG_WARNING, "Response status indicates GENERAL ERROR");
            break;
        case -2:  // STATUS_MIN_AMT
            printf("ERROR (Minimum amount/balance constraint)\n");
            log_message(LOG_WARNING, "Response status indicates MINIMUM AMOUNT/BALANCE ERROR");
            break;
        case -3:  // STATUS_INVALID
            printf("ERROR (Invalid parameters)\n");
            log_message(LOG_WARNING, "Response status indicates INVALID PARAMETERS");
            break;
        default:
            printf("UNKNOWN STATUS\n");
            log_message(LOG_WARNING, "Response contains UNKNOWN STATUS CODE: %d", response->status);
            break;
    }
    
    // Interpret command-specific data
    printf("Server message: \"%s\"\n", response->message);
    log_message(LOG_INFO, "Server message: %s", response->message);
    
    switch (command) {
        case CMD_OPEN:
            if (response->status == 0) {
                printf("Created account: %d\n", response->account_number);
                printf("Assigned PIN: %04d\n", response->pin);
                printf("Initial balance: %d\n", response->balance);
                log_message(LOG_INFO, "Response contains new account details - Number: %d, PIN: %04d, Balance: %d",
                           response->account_number, response->pin, response->balance);
            } else {
                printf("Account creation failed - likely reasons:\n");
                if (response->status == -1) {
                    printf("- Bank reached maximum account limit\n");
                    printf("- Server database error\n");
                    log_message(LOG_WARNING, "Possible causes: Maximum account limit reached or database error");
                }
            }
            break;
            
        case CMD_CLOSE:
            if (response->status == 0) {
                printf("Account %d successfully closed\n", account_number);
                log_message(LOG_INFO, "Account %d successfully closed", account_number);
            } else {
                printf("Account closure failed - likely reasons:\n");
                printf("- Account number does not exist\n");
                printf("- Incorrect PIN provided\n");
                log_message(LOG_WARNING, "Account closure failed - likely: non-existent account or wrong PIN");
            }
            break;
            
        case CMD_DEPOSIT:
            if (response->status == 0) {
                printf("Deposit successful\n");
                printf("New balance: %d\n", response->balance);
                log_message(LOG_INFO, "Deposit successful - New balance: %d", response->balance);
            } else {
                printf("Deposit failed - likely reasons:\n");
                if (response->status == -3) {
                    printf("- Amount below minimum deposit (%d)\n", 500); // MIN_DEPOSIT
                    log_message(LOG_WARNING, "Deposit failed - Amount below minimum");
                } else if (response->status == -1) {
                    printf("- Account number does not exist\n");
                    printf("- Incorrect PIN provided\n");
                    log_message(LOG_WARNING, "Deposit failed - likely: non-existent account or wrong PIN");
                }
            }
            break;
            
        case CMD_WITHDRAW:
            if (response->status == 0) {
                printf("Withdrawal successful\n");
                printf("New balance: %d\n", response->balance);
                log_message(LOG_INFO, "Withdrawal successful - New balance: %d", response->balance);
            } else {
                printf("Withdrawal failed - likely reasons:\n");
                if (response->status == -2) {
                    printf("- Withdrawal would break minimum balance requirement (%d)\n", 1000); // MIN_BALANCE
                    log_message(LOG_WARNING, "Withdrawal failed - Would break minimum balance");
                } else if (response->status == -3) {
                    printf("- Amount must be at least %d and a multiple of %d\n", 500, 500); // MIN_WITHDRAW
                    log_message(LOG_WARNING, "Withdrawal failed - Invalid amount format");
                } else if (response->status == -1) {
                    printf("- Account number does not exist\n");
                    printf("- Incorrect PIN provided\n");
                    log_message(LOG_WARNING, "Withdrawal failed - likely: non-existent account or wrong PIN");
                }
            }
            break;
            
        case CMD_BALANCE:
            if (response->status == 0) {
                printf("Current balance: %d\n", response->balance);
                log_message(LOG_INFO, "Balance inquiry successful - Current balance: %d", response->balance);
            } else {
                printf("Balance inquiry failed - likely reasons:\n");
                printf("- Account number does not exist\n");
                printf("- Incorrect PIN provided\n");
                log_message(LOG_WARNING, "Balance inquiry failed - likely: non-existent account or wrong PIN");
            }
            break;
            
        case CMD_STATEMENT:
            if (response->status == 0) {
                printf("Successfully retrieved %d transactions\n", response->transaction_count);
                log_message(LOG_INFO, "Statement retrieval successful - %d transactions", 
                           response->transaction_count);
                
                if (response->transaction_count > 0) {
                    printf("Transaction details:\n");
                    log_message(LOG_INFO, "Transaction summary:");
                    
                    for (int i = 0; i < response->transaction_count; i++) {
                        transaction_t *t = &response->transactions[i];
                        char buf[32];
                        strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&t->when));
                        
                        log_message(LOG_INFO, "  #%d: %s, Type=%c, Amount=%d, Balance=%d", 
                                  i+1, buf, t->type, t->amount, t->balance_after);
                    }
                } else {
                    printf("No transactions found for this account\n");
                    log_message(LOG_INFO, "No transactions found for this account");
                }
            } else {
                printf("Statement retrieval failed - likely reasons:\n");
                printf("- Account number does not exist\n");
                printf("- Incorrect PIN provided\n");
                log_message(LOG_WARNING, "Statement retrieval failed - likely: non-existent account or wrong PIN");
            }
            break;
            
        case CMD_QUIT:
            printf("Server acknowledged disconnect request\n");
            log_message(LOG_INFO, "Server acknowledged client disconnect request");
            break;
            
        default:
            printf("Unknown command - cannot interpret response\n");
            log_message(LOG_WARNING, "Unknown command (%d) - cannot interpret response", command);
            break;
    }
    
    printf("----------------------------------------\n\n");
    sleep(SHORT_WAIT);
}