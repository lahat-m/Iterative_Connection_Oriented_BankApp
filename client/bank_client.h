/*
 * Banking System Client - Common Header File
 */

#ifndef BANK_CLIENT_H
#define BANK_CLIENT_H

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

/* Default settings */
#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_PORT   8888
#define BUFFER_SIZE    1024
#define SHORT_WAIT     1               /* short wait in seconds           */
#define MEDIUM_WAIT    2               /* medium wait in seconds          */


/* Message commands between client and server */
typedef enum {
    OPEN = 1,         /* Open an account               */
    CLOSE = 2,        /* Close an account              */
    DEPOSIT = 3,      /* Deposit money                 */
    WITHDRAW = 4,     /* Withdraw money                */
    BALANCE = 5,      /* Check balance                 */
    STATEMENT = 6,    /* Get transaction statement     */
    QUIT = 0          /* Quit                          */
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

/* Global variables - extern declaration */
extern int client_socket;
extern FILE *log_file;



/* Function prototypes for networking */
int connect_to_server(const char *server_ip, int port);
void disconnect_from_server(void);

/* Function prototypes for interpreter */
void interpret_response(response_t *response, command_t command, int account_number);

/* Function prototypes for banking logic */
void open_account(void);
void close_account(void);
void deposit(void);
void withdraw(void);
void check_balance(void);
void get_statement(void);
void display_banner(void);

#endif /* BANK_CLIENT_H */