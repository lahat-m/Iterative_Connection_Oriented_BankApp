/*
 * Banking System Client - Interpreter Module
 */

#include "bank_client.h"

/* Interpret server response and display details */
void interpret_response(response_t *response, command_t command, int account_number) {
    printf("\n----- SERVER RESPONSE INTERPRETATION -----\n");
    
    // Interpret status code
    printf("Status code: %d - ", response->status);
    switch (response->status) {
        case 0:  // STATUS_OK
            printf("SUCCESS\n");
            break;
        case -1:  // STATUS_ERROR
            printf("ERROR (General error)\n");
            break;
        case -2:  // STATUS_MIN_AMT
            printf("ERROR (Minimum amount/balance constraint)\n");
            break;
        case -3:  // STATUS_INVALID
            printf("ERROR (Invalid parameters)\n");
            break;
        default:
            printf("UNKNOWN STATUS\n");
            break;
    }
    
    // Interpret command-specific data
    printf("Server message: \"%s\"\n", response->message);
    
    switch (command) {
        case OPEN:
            if (response->status == 0) {
                printf("Created account: %d\n", response->account_number);
                printf("Assigned PIN: %04d\n", response->pin);
                printf("Initial balance: %d\n", response->balance);
            } else {
                printf("Account creation failed - likely reasons:\n");
                if (response->status == -1) {
                    printf("- Bank reached maximum account limit\n");
                    printf("- Server database error\n");
                }
            }
            break;
            
        case CLOSE:
            if (response->status == 0) {
                printf("Account %d successfully closed\n", account_number);
            } else {
                printf("Account closure failed - likely reasons:\n");
                printf("- Account number does not exist\n");
                printf("- Incorrect PIN provided\n");
            }
            break;
            
        case DEPOSIT:
            if (response->status == 0) {
                printf("Deposit successful\n");
                printf("New balance: %d\n", response->balance);
            } else {
                printf("Deposit failed - likely reasons:\n");
                if (response->status == -3) {
                    printf("- Amount below minimum deposit (%d)\n", 500); // MIN_DEPOSIT
                } else if (response->status == -1) {
                    printf("- Account number does not exist\n");
                    printf("- Incorrect PIN provided\n");
                }
            }
            break;
            
        case WITHDRAW:
            if (response->status == 0) {
                printf("Withdrawal successful\n");
                printf("New balance: %d\n", response->balance);
            } else {
                printf("Withdrawal failed - likely reasons:\n");
                if (response->status == -2) {
                    printf("- Withdrawal would break minimum balance requirement (%d)\n", 1000); // MIN_BALANCE
                } else if (response->status == -3) {
                    printf("- Amount must be at least %d and a multiple of %d\n", 500, 500); // MIN_WITHDRAW
                } else if (response->status == -1) {
                    printf("- Account number does not exist\n");
                    printf("- Incorrect PIN provided\n");
                }
            }
            break;
            
        case BALANCE:
            if (response->status == 0) {
                printf("Current balance: %d\n", response->balance);
            } else {
                printf("Balance inquiry failed - likely reasons:\n");
                printf("- Account number does not exist\n");
                printf("- Incorrect PIN provided\n");
            }
            break;
            
        case STATEMENT:
            if (response->status == 0) {
                printf("Successfully retrieved %d transactions\n", response->transaction_count);
                
                if (response->transaction_count > 0) {
                    printf("Transaction details:\n");
                    
                    for (int i = 0; i < response->transaction_count; i++) {
                        transaction_t *t = &response->transactions[i];
                        char buf[32];
                        strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&t->when));
                        
                        printf("  #%d: %s, Type=%c, Amount=%d, Balance=%d\n", 
                               i+1, buf, t->type, t->amount, t->balance_after);
                    }
                } else {
                    printf("No transactions found for this account\n");
                }
            } else {
                printf("Statement retrieval failed - likely reasons:\n");
                printf("- Account number does not exist\n");
                printf("- Incorrect PIN provided\n");
            }
            break;
            
        case QUIT:
            printf("Server acknowledged disconnect request\n");
            break;
            
        default:
            printf("Unknown command - cannot interpret response\n");
            break;
    }
    
    printf("----------------------------------------\n\n");
    sleep(SHORT_WAIT);
}