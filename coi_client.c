/*
 * Banking System Client - Connection-oriented client
 *
 * Compile: gcc -std=c99 -Wall -o bank_client client.c
 * Run: ./bank_client [server_ip] [port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_PORT   8888
#define BUFFER_SIZE    1024
#define LOG_FILE       "client.log"    /* log file name                   */
#define SHORT_WAIT     1             /* short wait in seconds           */
#define MEDIUM_WAIT    2             /* medium wait in seconds          */

/* Log levels */
typedef enum {
    LOG_INFO = 0,
    LOG_WARNING = 1,
    LOG_ERROR = 2
} log_level_t;

/* Message commands between client and server */
typedef enum {
    CMD_OPEN = 1,         /* Open an account               */
    CMD_CLOSE = 2,        /* Close an account              */
    CMD_DEPOSIT = 3,      /* Deposit money                 */
    CMD_WITHDRAW = 4,     /* Withdraw money                */
    CMD_BALANCE = 5,      /* Check balance                 */
    CMD_STATEMENT = 6,    /* Get transaction statement     */
    CMD_QUIT = 0          /* Quit                          */
} command_t;

/* Server response codes */
typedef enum {
    STATUS_OK = 0,        /* Operation successful          */
    STATUS_ERROR = -1,    /* General error                 */
    STATUS_MIN_AMT = -2,  /* Below minimum amount          */
    STATUS_INVALID = -3   /* Invalid parameters            */
} status_t;

typedef enum {
    SAVINGS = 1,
    CHECKING = 2
} acct_type_t;

typedef struct {
    char type;                /* 'D' = deposit, 'W' = withdraw       */
    int  amount;              /* amount of the transaction           */
    time_t when;              /* time of transaction                 */
    int  balance_after;       /* balance after posting               */
} transaction_t;

/* A structure for client request messages */
typedef struct {
    command_t command;
    int account_number;
    int pin;
    int amount;
    acct_type_t account_type;
    char name[40];
    char nat_id[20];
} request_t;

/* A structure for server response messages */
typedef struct {
    status_t status;
    int account_number;
    int pin;
    int balance;
    char message[256];
    int transaction_count;
    transaction_t transactions[5]; // TRANS_KEEP = 5
} response_t;

/* Global variables */
static int client_socket = -1;
static FILE *log_file = NULL;      /* Log file handle */

/* Function prototypes */
static void log_init(void);
static void log_close(void);
static void log_message(log_level_t level, const char *format, ...);
static int connect_to_server(const char *server_ip, int port);
static void disconnect_from_server(void);
static void interpret_response(response_t *response, command_t command, int account_number);
static void open_account(void);
static void close_account(void);
static void deposit(void);
static void withdraw(void);
static void check_balance(void);
static void get_statement(void);
static void display_banner(void);

/* ---------- Logging functions --------------------------------------- */
static void log_init(void) {
    // Only initialize if not already initialized
    if (log_file) {
        return;
    }
    
    log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Failed to open log file");
        log_file = stderr; /* Fallback to stderr if file cannot be opened */
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(log_file, "\n[%s] [INFO] ========== BANK CLIENT STARTED ==========\n", timestamp);
    fflush(log_file);
}

static void log_close(void) {
    if (log_file && log_file != stderr) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        fprintf(log_file, "[%s] [INFO] ========== BANK CLIENT STOPPED ==========\n", timestamp);
        fflush(log_file);
        fclose(log_file);
        log_file = NULL; // Prevent double closing
    }
}

static void log_message(log_level_t level, const char *format, ...) {
    if (!log_file) {
        log_init();
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    const char *level_str;
    switch (level) {
        case LOG_INFO:    level_str = "INFO"; break;
        case LOG_WARNING: level_str = "WARNING"; break;
        case LOG_ERROR:   level_str = "ERROR"; break;
        default:          level_str = "UNKNOWN"; break;
    }
    
    fprintf(log_file, "[%s] [%s] ", timestamp, level_str);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}

/* Interpret server response and log details */
static void interpret_response(response_t *response, command_t command, int account_number) {
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

/* Connect to the banking server */
static int connect_to_server(const char *server_ip, int port) {
    struct sockaddr_in server_addr;
    
    log_message(LOG_INFO, "Attempting to connect to server at %s:%d", server_ip, port);
    printf("Attempting to connect to server at %s:%d...\n", server_ip, port);
    sleep(SHORT_WAIT);
    
    // Create socket
    log_message(LOG_INFO, "Creating client socket");
    printf("Creating client socket...\n");
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        log_message(LOG_ERROR, "Failed to create socket: %s", strerror(errno));
        perror("Failed to create socket");
        return -1;
    }
    sleep(SHORT_WAIT);
    
    // Prepare server address structure
    log_message(LOG_INFO, "Preparing server address structure");
    printf("Preparing server address structure...\n");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    sleep(SHORT_WAIT);
    
    // Convert IP address from text to binary form
    log_message(LOG_INFO, "Converting IP address from text to binary form");
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        log_message(LOG_ERROR, "Invalid address/ Address not supported: %s", strerror(errno));
        perror("Invalid address/ Address not supported");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    sleep(SHORT_WAIT);
    
    // Connect to server
    log_message(LOG_INFO, "Connecting to server");
    printf("Connecting to server...\n");
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_ERROR, "Connection failed: %s", strerror(errno));
        perror("Connection failed");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    log_message(LOG_INFO, "Successfully connected to bank server at %s:%d", server_ip, port);
    printf("Connected to bank server at %s:%d\n", server_ip, port);
    sleep(SHORT_WAIT);
    return 0;
}

/* Disconnect from the banking server */
static void disconnect_from_server(void) {
    if (client_socket >= 0) {
        // Send quit command to server
        request_t request;
        response_t response;
        
        log_message(LOG_INFO, "Preparing to disconnect from server");
        printf("Preparing to disconnect from server...\n");
        memset(&request, 0, sizeof(request));
        request.command = CMD_QUIT;
        
        log_message(LOG_INFO, "Sending QUIT command to server");
        printf("Sending QUIT command to server...\n");
        if (send(client_socket, &request, sizeof(request), 0) < 0) {
            log_message(LOG_ERROR, "Failed to send QUIT command: %s", strerror(errno));
            perror("Failed to send QUIT command");
        } else {
            log_message(LOG_INFO, "QUIT command sent successfully");
            printf("QUIT command sent successfully\n");
        }
        sleep(SHORT_WAIT);
        
        // Wait for server response
        log_message(LOG_INFO, "Waiting for server acknowledgment");
        printf("Waiting for server acknowledgment...\n");
        if (recv(client_socket, &response, sizeof(response), 0) > 0) {
            log_message(LOG_INFO, "Received server acknowledgment: %s", response.message);
            
            // Interpret the disconnect response
            interpret_response(&response, CMD_QUIT, 0);
        } else {
            log_message(LOG_WARNING, "No acknowledgment received from server");
            printf("No acknowledgment received from server\n");
        }
        sleep(SHORT_WAIT);
        
        log_message(LOG_INFO, "Closing client socket");
        printf("Closing client socket...\n");
        close(client_socket);
        client_socket = -1;
        log_message(LOG_INFO, "Disconnected from bank server");
        printf("Disconnected from bank server\n");
        sleep(SHORT_WAIT);
    }
}

/* Open a new account */
static void open_account(void) {
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
static void close_account(void) {
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
static void deposit(void) {
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
static void withdraw(void) {
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
static void check_balance(void) {
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
static void get_statement(void) {
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

/* Display the main menu banner */
static void display_banner(void) {
    log_message(LOG_INFO, "Displaying main menu banner");
    printf("\n============== BANK CLIENT (Network Version) ==============\n");
    printf("1: Open  2: Close  3: Deposit  4: Withdraw  5: Balance\n");
    printf("6: Statement  0: Quit\n");
    printf("----------------------------------------------------------\n");
}

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
