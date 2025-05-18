/*
 * Simple on–line banking demo with JSON persistence (stand-alone version)
 *
 * gcc -std=c99 -Wall -o bank bank_standalone.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>  /* For sleep() function */

#define MAX_ACCTS      1000          /* maximum simultaneous accounts   */
#define MIN_BALANCE    1000          /* Ksh. minimum account balance    */
#define MIN_DEPOSIT     500          /* Ksh. minimum single deposit     */
#define MIN_WITHDRAW    500          /* Ksh. minimum withdrawal unit    */
#define TRANS_KEEP        5          /* how many transactions to keep   */
#define DATA_FILE      "bank.json"   /* data file name                  */
#define CURRENT_VERSION  1           /* current data format version     */
#define LOG_FILE       "bank.log"    /* log file name                   */
#define SHORT_WAIT     1             /* short wait in seconds           */
#define MEDIUM_WAIT    2             /* medium wait in seconds          */

/* Log levels */
typedef enum {
    LOG_INFO = 0,
    LOG_WARNING = 1,
    LOG_ERROR = 2
} log_level_t;

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

typedef struct {
    int             number;                 /* automatically generated  */
    int             pin;                    /* generated 4-digit pin     */
    char            name[40];
    char            nat_id[20];
    acct_type_t     type;
    int             balance;                /* balance in Ksh.          */

    transaction_t   last[TRANS_KEEP];       /* ring buffer of last 5    */
    int             ntran;                  /* total number ever done   */
} account_t;

static account_t bank[MAX_ACCTS];
static int       accounts_in_use = 0;
static int       next_number     = 100001;  /* first account number     */
static FILE     *log_file = NULL;           /* Log file handle          */


/* ---------- Logging functions --------------------------------------- */
static void log_init(void) {
    log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Failed to open log file");
        log_file = stderr; /* Fallback to stderr if file cannot be opened */
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(log_file, "\n[%s] [INFO] ========== BANK SYSTEM STARTED ==========\n", timestamp);
    fflush(log_file);
}

static void log_close(void) {
    if (log_file && log_file != stderr) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        fprintf(log_file, "[%s] [INFO] ========== BANK SYSTEM STOPPED ==========\n", timestamp);
        fflush(log_file);
        fclose(log_file);
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
    
    // va_list args;
    // va_start(args, format);
    // vfprintf(log_file, format, args);
    // va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}

/* ---------- JSON serialization helpers --------------------------- */

/* Write a string with proper JSON escaping */
static void write_json_string(FILE *f, const char *str) {
    fputc('"', f);
    for (const char *p = str;*p;p++) {
        switch (*p) {
            case '\\': fputs("\\\\", f); break;
            case '"': fputs("\\\"", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (*p < 32) {
                    fprintf(f, "\\u%04x", *p);
                } else {
                    fputc(*p, f);
                }
        }
    }
    fputc('"', f);
}

/* Writes a transaction as JSON */
static void write_transaction_json(FILE *f, const transaction_t *t) {
    fprintf(f, "{"
                "\"type\":\"%c\","
                "\"amount\":%d,"
                "\"when\":%ld,"
                "\"balance_after\":%d"
               "}",
            t->type, t->amount, (long)t->when, t->balance_after);
}

/* Writes an account as JSON */
static void write_account_json(FILE *f, const account_t *a) {
    fprintf(f, "{"
                "\"number\":%d,"
                "\"pin\":%d,",
            a->number, a->pin);

    fputs("\"name\":", f);
    write_json_string(f, a->name);
    fputs(",\"nat_id\":", f);
    write_json_string(f, a->nat_id);

    fprintf(f, ","
                "\"type\":%d,"
                "\"balance\":%d,"
                "\"ntran\":%d,"
                "\"last\":[",
            a->type, a->balance, a->ntran);

    /* Write transactions */
    int start_index = a->ntran > TRANS_KEEP ? a->ntran - TRANS_KEEP : 0;
    int written = 0;

    for (int i = start_index;i < a->ntran;i++) {
        if (written > 0) {
            fputc(',', f);
        }
        write_transaction_json(f, &a->last[i % TRANS_KEEP]);
        written++;
    }

    fputs("]}", f);
}

/* Skip whitespace in a JSON file */
static void skip_whitespace(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            ungetc(c, f);
            break;
        }
    }
}

/* Read a string with proper JSON */
static int read_json_string(FILE *f, char *buffer, size_t bufsize) {
    int c;
    size_t pos = 0;
    
    /* Skip opening quote */
    c = fgetc(f);
    if (c != '"') {
        ungetc(c, f);
        return -1;
    }

    while ((c = fgetc(f)) != EOF) {
        if (c == '"') {
            /* End of string */
            buffer[pos] = '\0';
            return 0;
        } else if (c == '\\') {
            /* Escape sequence */
            c = fgetc(f);
            switch (c) {
                case '\\': 
                case '"': 
                case '/': 
                    if (pos < bufsize - 1) buffer[pos++] = c; 
                    break;
                case 'b': if (pos < bufsize - 1) buffer[pos++] = '\b'; break;
                case 'f': if (pos < bufsize - 1) buffer[pos++] = '\f'; break;
                case 'n': if (pos < bufsize - 1) buffer[pos++] = '\n'; break;
                case 'r': if (pos < bufsize - 1) buffer[pos++] = '\r'; break;
                case 't': if (pos < bufsize - 1) buffer[pos++] = '\t'; break;
                case 'u': {
                    /* Unicode escape - handle simple cases only */
                    char hex[5];
                    if (fread(hex, 1, 4, f) != 4) return -1;
                    hex[4] = '\0';
                    int val;
                    if (sscanf(hex, "%x", &val) != 1) return -1;
                    if (val < 256 && pos < bufsize - 1) {
                        buffer[pos++] = (char)val;
                    }
                    break;
                }
                default:
                    /* Invalid escape - copy the character */
                    if (pos < bufsize - 1) buffer[pos++] = c;
            }
        } else {
            /* Regular character */
            if (pos < bufsize - 1) buffer[pos++] = c;
        }
    }
    
    return -1; /* Unexpected EOF */
}

/* Skip to a specific character in a JSON file */
static int skip_to_char(FILE *f, char target) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == target) {
            return 0;
        }
    }
    return -1; /* EOF reached */
}

/* Read a JSON key name and match it */
static int match_json_key(FILE *f, const char *key) {
    skip_whitespace(f);
    
    char buffer[64];
    if (read_json_string(f, buffer, sizeof(buffer)) < 0) {
        return -1;
    }
    
    if (strcmp(buffer, key) != 0) {
        return -1;
    }
    
    skip_whitespace(f);
    if (fgetc(f) != ':') {
        return -1;
    }
    
    skip_whitespace(f);
    return 0;
}

/* Read a transaction from JSON */
static int read_transaction_json(FILE *f, transaction_t *t) {
    skip_whitespace(f);
    if (fgetc(f) != '{') {
        return -1;
    }
    
    /* Read type */
    if (match_json_key(f, "type") < 0) {
        return -1;
    }
    if (fgetc(f) != '"') {
        return -1;
    }
    t->type = fgetc(f);
    if (fgetc(f) != '"') {
        return -1;
    }
    
    /* Read amount */
    if (skip_to_char(f, ',') < 0) {
        return -1;
    }
    if (match_json_key(f, "amount") < 0) {
        return -1;
    }
    if (fscanf(f, "%d", &t->amount) != 1) {
        return -1;
    }
    
    /* Read when */
    if (skip_to_char(f, ',') < 0) {
        return -1;
    }
    if (match_json_key(f, "when") < 0) {
        return -1;
    }
    long when;
    if (fscanf(f, "%ld", &when) != 1) {
        return -1;
    }
    t->when = (time_t)when;
    
    /* Read balance_after */
    if (skip_to_char(f, ',') < 0) {
        return -1;
    }
    if (match_json_key(f, "balance_after") < 0) {
        return -1;
    }
    if (fscanf(f, "%d", &t->balance_after) != 1) {
        return -1;
    }
    
    /* Find closing brace */
    if (skip_to_char(f, '}') < 0) {
        return -1;
    }
    
    return 0;
}

/* ---------- persistence functions ----------------------------------- */
static int save_data(void) {
    log_message(LOG_INFO, "Saving data to %s", DATA_FILE);
    
    printf("System waiting while saving data...\n");
    sleep(SHORT_WAIT);
    
    FILE *f = fopen(DATA_FILE, "w");
    if (!f) {
        log_message(LOG_ERROR, "Failed to open data file for writing: %s", strerror(errno));
        perror("Failed to open data file for writing");
        return -1;
    }
    
    /* Start the main JSON object */
    fprintf(f, "{\n"
               "  \"version\": %d,\n"
               "  \"accounts_in_use\": %d,\n"
               "  \"next_number\": %d,\n"
               "  \"accounts\": [\n",
               CURRENT_VERSION, accounts_in_use, next_number);
    
    /* Write all accounts */
    for (int i=0;i<accounts_in_use;i++) {
        fprintf(f, "    ");
        write_account_json(f, &bank[i]);
        if (i < accounts_in_use - 1) {
            fprintf(f, ",\n");
        } else {
            fprintf(f, "\n");
        }
    }
    
    /* Close the JSON structure */
    fprintf(f, "  ]\n}\n");
    
    fclose(f);
    log_message(LOG_INFO, "Data saved successfully (%d accounts)", accounts_in_use);
    
    printf("Data saved.\n");
    return 0;
}

static int load_data(void) {
    log_message(LOG_INFO, "Loading data from %s", DATA_FILE);
    
    printf("System waiting while loading data...\n");
    sleep(MEDIUM_WAIT);
    
    FILE *f = fopen(DATA_FILE, "r");
    if (!f) {
        // File doesn't exist yet - first run
        if (errno == ENOENT) {
            log_message(LOG_INFO, "No existing data file found. Starting with empty file.");
            printf("No existing data.\n");
            return 0;
        }
        log_message(LOG_ERROR, "Failed to open data file: %s", strerror(errno));
        perror("Failed to open data file");
        return -1;
    }
    
    // Basic JSON parsing
    skip_whitespace(f);
    
    /* Check for opening brace */
    if (fgetc(f) != '{') {
        fclose(f);
        return -1;
    }
    
    /* Read version */
    if (match_json_key(f, "version") < 0) {
        fclose(f);
        return -1;
    }
    
    int version;
    if (fscanf(f, "%d", &version) != 1) {
        fclose(f);
        return -1;
    }
    
    /* Check version compatibility */
    if (version > CURRENT_VERSION) {
        log_message(LOG_WARNING, "Data file version %d is newer than supported version %d", 
                    version, CURRENT_VERSION);
        fprintf(stderr, "Warning: Data file version %d is newer than supported version %d.\n", 
                version, CURRENT_VERSION);
    }
    
    /* Read accounts_in_use */
    if (skip_to_char(f, ',') < 0) {
        log_message(LOG_ERROR, "Failed to find accounts_in_use in the file");
        fclose(f);
        return -1;
    }
    if (match_json_key(f, "accounts_in_use") < 0) {
        fclose(f);
        return -1;
    }
    if (fscanf(f, "%d", &accounts_in_use) != 1) {
        fclose(f);
        return -1;
    }
    
    /* Read next_number */
    if (skip_to_char(f, ',') < 0) {
        fclose(f);
        return -1;
    }
    if (match_json_key(f, "next_number") < 0) {
        fclose(f);
        return -1;
    }
    if (fscanf(f, "%d", &next_number) != 1) {
        fclose(f);
        return -1;
    }
    
    /* Find accounts array */
    if (skip_to_char(f, ',') < 0) {
        fclose(f);
        return -1;
    }
    if (match_json_key(f, "accounts") < 0) {
        fclose(f);
        return -1;
    }
    
    skip_whitespace(f);
    if (fgetc(f) != '[') {
        fclose(f);
        return -1;
    }
    
    /* Read each account */
    for (int i = 0; i < accounts_in_use; i++) {
        skip_whitespace(f);
        
        if (fgetc(f) != '{') {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        account_t *a = &bank[i];
        
        /* Read account number */
        if (match_json_key(f, "number") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->number) != 1) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        /* Read PIN */
        if (skip_to_char(f, ',') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "pin") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->pin) != 1) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        /* Read name */
        if (skip_to_char(f, ',') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "name") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (read_json_string(f, a->name, sizeof(a->name)) < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        /* Read national ID */
        if (skip_to_char(f, ',') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "nat_id") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (read_json_string(f, a->nat_id, sizeof(a->nat_id)) < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        /* Read account type */
        if (skip_to_char(f, ',') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "type") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        int type;
        if (fscanf(f, "%d", &type) != 1) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        a->type = (acct_type_t)type;
        
        /* Read balance */
        if (skip_to_char(f, ',') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "balance") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->balance) != 1) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        /* Read ntran */
        if (skip_to_char(f, ',') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "ntran") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->ntran) != 1) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        /* Read last transactions */
        if (skip_to_char(f, ',') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "last") < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        skip_whitespace(f);
        if (fgetc(f) != '[') {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        /* Read up to TRANS_KEEP transactions */
        int trans_read = 0;
        int next_char;
        
        skip_whitespace(f);
        next_char = fgetc(f);
        
        while (next_char != ']' && trans_read < TRANS_KEEP) {
            ungetc(next_char, f);
            
            if (read_transaction_json(f, &a->last[trans_read % TRANS_KEEP]) < 0) {
                accounts_in_use = i;
                fclose(f);
                return -1;
            }
            
            trans_read++;
            
            skip_whitespace(f);
            next_char = fgetc(f);
            
            if (next_char == ',') {
                skip_whitespace(f);
                next_char = fgetc(f);
            }
        }
        
        /* Skip to end of account object */
        if (skip_to_char(f, '}') < 0) {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        
        skip_whitespace(f);
        next_char = fgetc(f);
        
        /* Check for comma or end of array */
        if (next_char != ',' && next_char != ']') {
            accounts_in_use = i + 1;
            fclose(f);
            return -1;
        }
        
        if (next_char == ']' && i < accounts_in_use - 1) {
            /* Fewer accounts than expected */
            accounts_in_use = i + 1;
            break;
        }
    }
    
    fclose(f);
    log_message(LOG_INFO, "Data loaded successfully (%d accounts)", accounts_in_use);
    
    printf("Data loaded. System online.\n");
    return 0;
}

/* ---------- helpers ------------------------------------------------ */
static int gen_pin(void) {
    return 1000 + rand()%9000;              /* random 4-digit pin       */
}

static transaction_t *slot_for(account_t *a)
{
    return &a->last[a->ntran % TRANS_KEEP];
}

static void remember(account_t *a, char typ, int amt)
{
    transaction_t *t = slot_for(a);
    a->ntran++;
    t->type   = typ;
    t->amount = amt;
    t->when   = time(NULL);
    t->balance_after = a->balance;
}

/* ---------- API required in the statement -------------------------- */
account_t *open_account(const char *name, const char *nid, acct_type_t t)
{
    log_message(LOG_INFO, "Opening new account for %s (ID: %s, Type: %d)", 
                name, nid, t);
    sleep(SHORT_WAIT);

    if (accounts_in_use >= MAX_ACCTS) {
        log_message(LOG_ERROR, "Cannot open account: maximum accounts limit reached");
        return NULL;
    }

    printf("Processing account creation, please wait...\n");
    sleep(MEDIUM_WAIT);

    account_t *a = &bank[accounts_in_use++];
    memset(a, 0, sizeof(*a));

    a->number  = next_number++;
    a->pin     = gen_pin();
    strncpy(a->name,   name, sizeof(a->name)-1);
    strncpy(a->nat_id, nid,  sizeof(a->nat_id)-1);
    a->type    = t;
    a->balance = MIN_BALANCE;              /* initial 1 k mandatory    */
    remember(a, 'D', MIN_BALANCE);
    
    log_message(LOG_INFO, "Account created for %s. Account Details: Number=%d, PIN=%04d, Balance=%d", a->name, 
                a->number, a->pin, a->balance);
    
    // Save after modification
    save_data();
    return a;
}

int close_account(int acc_no, int pin)
{
    log_message(LOG_INFO, "Closing account %d", acc_no);
    
    printf("Processing account closure, please wait...\n");
    sleep(SHORT_WAIT);
    
    for(int i=0;i<accounts_in_use;i++)
        if(bank[i].number == acc_no && bank[i].pin == pin)
        {
            log_message(LOG_INFO, "Closing account %d with balance %d", 
                        acc_no, bank[i].balance);
            
            /* shift the tail of the array left */
            bank[i] = bank[--accounts_in_use];
            save_data();
            return 0;                       /* success */
        }
    
    log_message(LOG_WARNING, "Failed to close account %d: account not found or wrong PIN", acc_no);
    return -1;                              /* not found / wrong pin */
}

int deposit(int acc_no, int pin, int amount)
{
    log_message(LOG_INFO, "Deposit request: Account %d, Amount %d", acc_no, amount);
    
    if(amount < MIN_DEPOSIT) {
        log_message(LOG_WARNING, "Deposit rejected: Amount %d less than minimum deposit %d", 
                    amount, MIN_DEPOSIT);
        return -3;
    }
    
    printf("Processing deposit, please wait...\n");
    sleep(SHORT_WAIT);
    
    for(int i=0;i<accounts_in_use;i++)
        if(bank[i].number==acc_no && bank[i].pin==pin)
        {
            bank[i].balance += amount;
            remember(&bank[i],'D',amount);
            
            log_message(LOG_INFO, "Deposit successful: Account %d, Amount %d, New Balance %d", 
                        acc_no, amount, bank[i].balance);
            
            save_data();
            return 0;
        }
    
    log_message(LOG_WARNING, "Deposit failed: Account %d not found or wrong PIN", acc_no);
    return -1;                              /* wrong acc/pin */
}

int withdraw(int acc_no, int pin, int amount)
{
    log_message(LOG_INFO, "Withdrawal request: Account %d, Amount %d", acc_no, amount);
    
    if(amount < MIN_WITHDRAW || amount%MIN_WITHDRAW) {
        log_message(LOG_WARNING, "Withdrawal rejected: Amount %d not valid (must be >= %d and multiple of %d)", 
                    amount, MIN_WITHDRAW, MIN_WITHDRAW);
        return -3;
    }
    
    printf("Processing withdrawal, please wait...\n");
    sleep(SHORT_WAIT);
    
    for(int i=0;i<accounts_in_use;i++)
        if(bank[i].number==acc_no && bank[i].pin==pin)
        {
            if(bank[i].balance - amount < MIN_BALANCE) {
                log_message(LOG_WARNING, "Withdrawal rejected: Would break minimum balance (Current: %d, After: %d, Min: %d)", 
                            bank[i].balance, bank[i].balance - amount, MIN_BALANCE);
                return -2;
            }
            
            bank[i].balance -= amount;
            remember(&bank[i],'W',amount);
            
            log_message(LOG_INFO, "Withdrawal successful: Account %d, Amount %d, New Balance %d", 
                        acc_no, amount, bank[i].balance);
            
            save_data();
            return 0;
        }
    
    log_message(LOG_WARNING, "Withdrawal failed: Account %d not found or wrong PIN", acc_no);
    return -1;
}

int balance(int acc_no, int pin, int *bal_out)
{
    log_message(LOG_INFO, "Balance inquiry: Account %d", acc_no);
    
    printf("Retrieving balance information, please wait...\n");
    sleep(SHORT_WAIT);
    
    for(int i=0;i<accounts_in_use;i++)
        if(bank[i].number==acc_no && bank[i].pin==pin)
        {
            *bal_out = bank[i].balance;
            log_message(LOG_INFO, "Balance reported: Account %d, Balance %d", acc_no, *bal_out);
            return 0;
        }
    
    log_message(LOG_WARNING, "Balance inquiry failed: Account %d not found or wrong PIN", acc_no);
    return -1;
}

int statement(int acc_no, int pin)
{
    log_message(LOG_INFO, "Statement request: Account %d", acc_no);
    
    printf("Generating account statement, please wait...\n");
    sleep(MEDIUM_WAIT);
    
    for(int i=0;i<accounts_in_use;i++)
        if(bank[i].number==acc_no && bank[i].pin==pin)
        {
            account_t *a = &bank[i];
            int start = (a->ntran > TRANS_KEEP)? a->ntran-TRANS_KEEP : 0;
            
            log_message(LOG_INFO, "Generating statement for account %d with %d transactions", 
                        acc_no, a->ntran > TRANS_KEEP ? TRANS_KEEP : a->ntran);

            puts("\nLast five transactions:");
            for(int j=start;j<a->ntran;j++)
            {
                transaction_t *t = &a->last[j % TRANS_KEEP];
                char buf[32];
                strftime(buf,sizeof(buf),"%d/%m/%Y %H:%M",localtime(&t->when));
                printf("%s  %c  %d  NewBal:%d\n",
                       buf, t->type, t->amount, t->balance_after);
            }
            return 0;
        }
    
    log_message(LOG_WARNING, "Statement request failed: Account %d not found or wrong PIN", acc_no);
    return -1;
}

/* ---------- trivial console UI ------------------------------------ */
static void banner(void)
{
    puts("============== SIMPLE BANK (with JSON persistence) =============");
    puts("1: Open  2: Close  3: Deposit  4: Withdraw  5: Balance");
    puts("6: Statement  0: Quit");
    puts("-------------------------------------------------------");
}



int main(void)
{
    srand(time(NULL));
    
    /* Initialize logging */
    log_init();
    log_message(LOG_INFO, "Bank system initialized");
    
    // Load existing data at startup
    if (load_data() != 0) {
        log_message(LOG_WARNING, "Could not load existing data. Starting fresh.");
        puts("Warning: Could not load existing data. Starting fresh.");
    }
    
    int choice;
    banner();
    while(1)
    {
        printf("\n> "); fflush(stdout);
        if(scanf("%d",&choice)!=1) break;

        if(choice==0) {
            log_message(LOG_INFO, "User requested exit");
            break;
        }
        else if(choice==1) {               /* open */
            char name[40], nid[20]; int t;
            printf("Name: ");   scanf(" %39s", name);
            printf("Nat-ID: "); scanf(" %19s", nid);
            printf("1=Savings 2=Checking : "); scanf("%d",&t);
            
            log_message(LOG_INFO, "User requested new account: Name=%s, ID=%s, Type=%d", 
                       name, nid, t);
            
            account_t *a = open_account(name,nid,(t==1)?SAVINGS:CHECKING);
            if(a) {
                printf("Account created. Number=%d  Pin=%04d  Balance=%d\n",
                      a->number,a->pin,a->balance);
            }
            else {
                log_message(LOG_ERROR, "Failed to create account: bank full or error");
                puts("!! Bank full / error");
            }
        }
        else if(choice==2) {          /* close */
            int no,p;
            printf("AccNo Pin? "); scanf("%d%d",&no,&p);
            
            log_message(LOG_INFO, "User requested account closure: Account=%d", no);
            
            int result = close_account(no,p);
            puts(result ? "!! Fail" : "** Closed OK");
        }
        else if(choice==3) {          /* deposit */
            int no,p,amt;
            printf("AccNo? "); scanf("%d",&no);
            printf("Pin? "); scanf("%d",&p);
            printf("Amount? "); scanf("%d",&amt);
            
            log_message(LOG_INFO, "User requested deposit: Account=%d, Amount=%d", no, amt);
            
            int rv=deposit(no,p,amt);
            puts(rv?"!! Error":"** OK");
        }
        else if(choice==4) {          /* withdraw */
            int no,p,amt;
            printf("AccNo? "); scanf("%d",&no);
            printf("Pin? "); scanf("%d",&p);
            printf("Amount? "); scanf("%d",&amt);
            
            log_message(LOG_INFO, "User requested withdrawal: Account=%d, Amount=%d", no, amt);
            
            int rv=withdraw(no,p,amt);
            if(rv==0) puts("** OK");
            else if(rv==-2) puts("!! Would break minimum balance");
            else if(rv==-3) puts("!! Must be >=500 and multiple of 500");
            else puts("!! Error");
        }
        else if(choice==5) {          /* balance */
            int no,p,b;
            printf("AccNo? "); scanf("%d",&no);
            printf("Pin? "); scanf("%d",&p);
            
            log_message(LOG_INFO, "User requested balance: Account=%d", no);
            
            if(balance(no,p,&b)==0) printf("Balance = %d\n",b);
            else puts("!! Error");
        }
        else if(choice==6) {          /* statement */
            int no,p;
            printf("AccNo? "); scanf("%d",&no);
            printf("Pin? "); scanf("%d",&p);
            
            log_message(LOG_INFO, "User requested statement: Account=%d", no);
            
            if(statement(no,p)!=0) puts("!! Error");
        }
        else {
            log_message(LOG_WARNING, "User entered invalid choice: %d", choice);
            puts("!! Invalid choice");
        }
    }
    
    // Save data before exiting
    if (save_data() != 0) {
        log_message(LOG_ERROR, "Could not save data on exit!");
        puts("Warning: Could not save data on exit!");
    }
    
    log_message(LOG_INFO, "Bank system shutting down");
    log_close();
    
    puts("Bye.");
    return 0;
}