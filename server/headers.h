/*
 * Banking System Server - Connection-oriented iterative server
 *
 * Compile: gcc -std=c99 -Wall -o bank_server coi_server.c
 * Run: ./bank_server [port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_ACCTS 1000        /* maximum simultaneous accounts   */
#define MIN_BALANCE 1000      /* Ksh. minimum account balance    */
#define MIN_DEPOSIT 500       /* Ksh. minimum single deposit     */
#define MIN_WITHDRAW 500      /* Ksh. minimum withdrawal unit    */
#define TRANS_KEEP 5          /* how many transactions to keep   */
#define DATA_FILE "bank.json" /* data file name                  */
#define CURRENT_VERSION 1     /* current data format version     */
#define LOG_FILE "bank.log"   /* log file name                   */
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
    int number; /* automatically generated  */
    int pin;    /* generated 4-digit pin     */
    char name[40];
    char nat_id[20];
    acct_type_t type;
    int balance; /* balance in Ksh.          */

    transaction_t last[TRANS_KEEP]; /* ring buffer of last 5    */
    int ntran;                      /* total number ever done   */
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

/* Global variables */
static account_t bank[MAX_ACCTS];
static int accounts_in_use = 0;
static int next_number = 100001; /* first account number     */
static FILE *log_file = NULL;    /* Log file handle          */
static int server_socket;        /* Server socket            */
static int running = 1;          /* Server running flag      */


static int gen_pin(void);
static transaction_t *slot_for(account_t *a);
static void remember(account_t *a, char typ, int amt);
static account_t *open_account(const char *name, const char *nid, acct_type_t t);
static int close_account(int acc_no, int pin);
static int deposit(int acc_no, int pin, int amount);
static int withdraw(int acc_no, int pin, int amount);
static int balance(int acc_no, int pin, int *bal_out);
static int statement(int acc_no, int pin, response_t *resp);
static void handle_client(int client_socket);
static void shutdown_server(int signal);