/*
 * Banking System - Data persistence implementation
 */

#include "bank_persistence.h"
#include <errno.h>
#include <unistd.h>

/* Global variables defined here */
account_t bank[MAX_ACCTS];
int accounts_in_use = 0;
int next_number = 100001; /* first account number */
FILE *log_file = NULL;    /* Log file handle */

/* Save data to binary file */
int save_data(void)
{
    printf("System waiting while saving data...\n");
    sleep(SHORT_WAIT);

    FILE *f = fopen(DATA_FILE, "wb");
    if (!f)
    {
        perror("Failed to open data file for writing");
        return -1;
    }

    /* Write file header with counters */
    int header[2] = {accounts_in_use, next_number};
    if (fwrite(header, sizeof(int), 2, f) != 2)
    {
        perror("Failed to write file header");
        fclose(f);
        return -1;
    }

    /* Write all accounts */
    if (fwrite(bank, sizeof(account_t), accounts_in_use, f) != accounts_in_use)
    {
        perror("Failed to write account data");
        fclose(f);
        return -1;
    }

    fclose(f);
    printf("Data saved to %s (%d accounts).\n", DATA_FILE, accounts_in_use);
    return 0;
}

/* Load data from binary file */
int load_data(void)
{
    printf("System waiting while loading data...\n");
    sleep(MEDIUM_WAIT);

    FILE *f = fopen(DATA_FILE, "rb");
    if (!f)
    {
        // File doesn't exist yet - first run
        if (errno == ENOENT)
        {
            printf("No existing data file found. Starting with empty database.\n");
            return 0;
        }
        perror("Failed to open data file");
        return -1;
    }

    /* Read file header */
    int header[2];
    if (fread(header, sizeof(int), 2, f) != 2)
    {
        perror("Failed to read file header");
        fclose(f);
        return -1;
    }

    accounts_in_use = header[0];
    next_number = header[1];

    /* Validate number of accounts */
    if (accounts_in_use < 0 || accounts_in_use > MAX_ACCTS)
    {
        fprintf(stderr, "Error: Invalid number of accounts in data file: %d\n", accounts_in_use);
        fclose(f);
        accounts_in_use = 0;
        return -1;
    }

    /* Read account data */
    if (accounts_in_use > 0)
    {
        size_t accounts_read = fread(bank, sizeof(account_t), accounts_in_use, f);
        if (accounts_read != accounts_in_use)
        {
            fprintf(stderr, "Warning: Could only read %zu of %d accounts\n", 
                    accounts_read, accounts_in_use);
            accounts_in_use = accounts_read;
        }
    }

    fclose(f);
    printf("Data loaded from %s (%d accounts). System online.\n", DATA_FILE, accounts_in_use);
    return 0;
}