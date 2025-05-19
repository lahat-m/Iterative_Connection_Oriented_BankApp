/*
 * Banking System - Common definitions and structures
 */

#ifndef BANK_COMMON_H
#define BANK_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

/* Configuration constants */
#define MAX_ACCTS 1000        /* maximum simultaneous accounts   */
#define MIN_BALANCE 1000      /* Ksh. minimum account balance    */
#define MIN_DEPOSIT 500       /* Ksh. minimum single deposit     */
#define MIN_WITHDRAW 500      /* Ksh. minimum withdrawal unit    */
#define TRANS_KEEP 5          /* how many transactions to keep   */
#define DATA_FILE "bank.txt" /* data file name                  */
#define CURRENT_VERSION 1     /* current data format version     */
#define SHORT_WAIT 1          /* short wait in seconds           */
#define MEDIUM_WAIT 2         /* medium wait in seconds          */
#define DEFAULT_PORT 8888     /* default server port             */
#define BUFFER_SIZE 1024      /* socket buffer size              */

/* Log levels */
typedef enum
{
    LOG_INFO = 0,
    LOG_WARNING = 1,
    LOG_ERROR = 2
} log_level_t;

/* Message commands between client and server */
typedef enum
{
    OPEN = 1,      /* Open an account               */
    CLOSE = 2,     /* Close an account              */
    DEPOSIT = 3,   /* Deposit money                 */
    WITHDRAW = 4,  /* Withdraw money                */
    BALANCE = 5,   /* Check balance                 */
    STATEMENT = 6, /* Get transaction statement     */
    QUIT = 0       /* Quit                          */
} command_t;

/* Server response codes */
typedef enum
{
    STATUS_OK = 0,       /* Operation successful          */
    STATUS_ERROR = -1,   /* General error                 */
    STATUS_MIN_AMT = -2, /* Below minimum amount          */
    STATUS_INVALID = -3  /* Invalid parameters            */
} status_t;

typedef enum
{
    SAVINGS = 1,
    CHECKING = 2
} acct_type_t;

typedef struct
{
    char type;         /* 'D' = deposit, 'W' = withdraw       */
    int amount;        /* amount of the transaction           */
    time_t when;       /* time of transaction                 */
    int balance_after; /* balance after posting               */
} transaction_t;

typedef struct
{
    int number;               /* automatically generated       */
    int pin;                  /* generated 4-digit pin         */
    char name[40];
    char nat_id[20];
    acct_type_t type;
    int balance;              /* balance in Ksh.              */
    transaction_t last[TRANS_KEEP]; /* ring buffer of last 5  */
    int ntran;                /* total number ever done       */
} account_t;

/* A structure for client request messages */
typedef struct
{
    command_t command;
    int account_number;
    int pin;
    int amount;
    acct_type_t account_type;
    char name[40];
    char nat_id[20];
} request_t;

/* A structure for server response messages */
typedef struct
{
    status_t status;
    int account_number;
    int pin;
    int balance;
    char message[256];
    int transaction_count;
    transaction_t transactions[TRANS_KEEP];
} response_t;

/* Global variables declarations (to be defined in separate implementation files) */
extern account_t bank[MAX_ACCTS];
extern int accounts_in_use;
extern int next_number;
extern int server_socket;
extern int running;

#endif /* BANK_COMMON_H */