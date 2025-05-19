/*
 * Banking System - Data persistence implementation
 */

#include "bank_persistence.h"
#include "bank_log.h"
#include <errno.h>
#include <unistd.h>

/* Global variables defined here */
account_t bank[MAX_ACCTS];
int accounts_in_use = 0;
int next_number = 100001; /* first account number */
FILE *log_file = NULL;    /* Log file handle */

/* JSON serialization helpers */

/* Write a string with proper JSON escaping */
void write_json_string(FILE *f, const char *str)
{
    fputc('"', f);
    for (const char *p = str; *p; p++)
    {
        switch (*p)
        {
        case '\\':
            fputs("\\\\", f);
            break;
        case '"':
            fputs("\\\"", f);
            break;
        case '\b':
            fputs("\\b", f);
            break;
        case '\f':
            fputs("\\f", f);
            break;
        case '\n':
            fputs("\\n", f);
            break;
        case '\r':
            fputs("\\r", f);
            break;
        case '\t':
            fputs("\\t", f);
            break;
        default:
            if (*p < 32)
            {
                fprintf(f, "\\u%04x", *p);
            }
            else
            {
                fputc(*p, f);
            }
        }
    }
    fputc('"', f);
}

/* Writes a transaction as JSON */
void write_transaction_json(FILE *f, const transaction_t *t)
{
    fprintf(f, "{"
               "\"type\":\"%c\","
               "\"amount\":%d,"
               "\"when\":%ld,"
               "\"balance_after\":%d"
               "}",
            t->type, t->amount, (long)t->when, t->balance_after);
}

/* Writes an account as JSON */
void write_account_json(FILE *f, const account_t *a)
{
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

    for (int i = start_index; i < a->ntran; i++)
    {
        if (written > 0)
        {
            fputc(',', f);
        }
        write_transaction_json(f, &a->last[i % TRANS_KEEP]);
        written++;
    }

    fputs("]}", f);
}

/* Skip whitespace in a JSON file */
void skip_whitespace(FILE *f)
{
    int c;
    while ((c = fgetc(f)) != EOF)
    {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
        {
            ungetc(c, f);
            break;
        }
    }
}

/* Read a string with proper JSON */
int read_json_string(FILE *f, char *buffer, size_t bufsize)
{
    int c;
    size_t pos = 0;

    /* Skip opening quote */
    c = fgetc(f);
    if (c != '"')
    {
        ungetc(c, f);
        return -1;
    }

    while ((c = fgetc(f)) != EOF)
    {
        if (c == '"')
        {
            /* End of string */
            buffer[pos] = '\0';
            return 0;
        }
        else if (c == '\\')
        {
            /* Escape sequence */
            c = fgetc(f);
            switch (c)
            {
            case '\\':
            case '"':
            case '/':
                if (pos < bufsize - 1)
                    buffer[pos++] = c;
                break;
            case 'b':
                if (pos < bufsize - 1)
                    buffer[pos++] = '\b';
                break;
            case 'f':
                if (pos < bufsize - 1)
                    buffer[pos++] = '\f';
                break;
            case 'n':
                if (pos < bufsize - 1)
                    buffer[pos++] = '\n';
                break;
            case 'r':
                if (pos < bufsize - 1)
                    buffer[pos++] = '\r';
                break;
            case 't':
                if (pos < bufsize - 1)
                    buffer[pos++] = '\t';
                break;
            case 'u':
            {
                /* Unicode escape - handle simple cases only */
                char hex[5];
                if (fread(hex, 1, 4, f) != 4)
                    return -1;
                hex[4] = '\0';
                int val;
                if (sscanf(hex, "%x", &val) != 1)
                    return -1;
                if (val < 256 && pos < bufsize - 1)
                {
                    buffer[pos++] = (char)val;
                }
                break;
            }
            default:
                /* Invalid escape - copy the character */
                if (pos < bufsize - 1)
                    buffer[pos++] = c;
            }
        }
        else
        {
            /* Regular character */
            if (pos < bufsize - 1)
                buffer[pos++] = c;
        }
    }

    return -1; /* Unexpected EOF */
}

/* Skip to a specific character in a JSON file */
int skip_to_char(FILE *f, char target)
{
    int c;
    while ((c = fgetc(f)) != EOF)
    {
        if (c == target)
        {
            return 0;
        }
    }
    return -1; /* EOF reached */
}

/* Read a JSON key name and match it */
int match_json_key(FILE *f, const char *key)
{
    skip_whitespace(f);

    char buffer[64];
    if (read_json_string(f, buffer, sizeof(buffer)) < 0)
    {
        return -1;
    }

    if (strcmp(buffer, key) != 0)
    {
        return -1;
    }

    skip_whitespace(f);
    if (fgetc(f) != ':')
    {
        return -1;
    }

    skip_whitespace(f);
    return 0;
}

/* Read a transaction from JSON */
int read_transaction_json(FILE *f, transaction_t *t)
{
    skip_whitespace(f);
    if (fgetc(f) != '{')
    {
        return -1;
    }

    /* Read type */
    if (match_json_key(f, "type") < 0)
    {
        return -1;
    }
    if (fgetc(f) != '"')
    {
        return -1;
    }
    t->type = fgetc(f);
    if (fgetc(f) != '"')
    {
        return -1;
    }

    /* Read amount */
    if (skip_to_char(f, ',') < 0)
    {
        return -1;
    }
    if (match_json_key(f, "amount") < 0)
    {
        return -1;
    }
    if (fscanf(f, "%d", &t->amount) != 1)
    {
        return -1;
    }

    /* Read when */
    if (skip_to_char(f, ',') < 0)
    {
        return -1;
    }
    if (match_json_key(f, "when") < 0)
    {
        return -1;
    }
    long when;
    if (fscanf(f, "%ld", &when) != 1)
    {
        return -1;
    }
    t->when = (time_t)when;

    /* Read balance_after */
    if (skip_to_char(f, ',') < 0)
    {
        return -1;
    }
    if (match_json_key(f, "balance_after") < 0)
    {
        return -1;
    }
    if (fscanf(f, "%d", &t->balance_after) != 1)
    {
        return -1;
    }

    /* Find closing brace */
    if (skip_to_char(f, '}') < 0)
    {
        return -1;
    }

    return 0;
}

/* Save data to file */
int save_data(void)
{
    log_message(LOG_INFO, "Saving data to %s", DATA_FILE);

    printf("System waiting while saving data...\n");
    sleep(SHORT_WAIT);

    FILE *f = fopen(DATA_FILE, "w");
    if (!f)
    {
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
    for (int i = 0; i < accounts_in_use; i++)
    {
        fprintf(f, "    ");
        write_account_json(f, &bank[i]);
        if (i < accounts_in_use - 1)
        {
            fprintf(f, ",\n");
        }
        else
        {
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

/* Load data from file */
int load_data(void)
{
    log_message(LOG_INFO, "Loading data from %s", DATA_FILE);

    printf("System waiting while loading data...\n");
    sleep(MEDIUM_WAIT);

    FILE *f = fopen(DATA_FILE, "r");
    if (!f)
    {
        // File doesn't exist yet - first run
        if (errno == ENOENT)
        {
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
    if (fgetc(f) != '{')
    {
        fclose(f);
        return -1;
    }

    /* Read version */
    if (match_json_key(f, "version") < 0)
    {
        fclose(f);
        return -1;
    }

    int version;
    if (fscanf(f, "%d", &version) != 1)
    {
        fclose(f);
        return -1;
    }

    /* Check version compatibility */
    if (version > CURRENT_VERSION)
    {
        log_message(LOG_WARNING, "Data file version %d is newer than supported version %d",
                    version, CURRENT_VERSION);
        fprintf(stderr, "Warning: Data file version %d is newer than supported version %d.\n",
                version, CURRENT_VERSION);
    }

    /* Read accounts_in_use */
    if (skip_to_char(f, ',') < 0)
    {
        log_message(LOG_ERROR, "Failed to find accounts_in_use in the file");
        fclose(f);
        return -1;
    }
    if (match_json_key(f, "accounts_in_use") < 0)
    {
        fclose(f);
        return -1;
    }
    if (fscanf(f, "%d", &accounts_in_use) != 1)
    {
        fclose(f);
        return -1;
    }

    /* Read next_number */
    if (skip_to_char(f, ',') < 0)
    {
        fclose(f);
        return -1;
    }
    if (match_json_key(f, "next_number") < 0)
    {
        fclose(f);
        return -1;
    }
    if (fscanf(f, "%d", &next_number) != 1)
    {
        fclose(f);
        return -1;
    }

    /* Find accounts array */
    if (skip_to_char(f, ',') < 0)
    {
        fclose(f);
        return -1;
    }
    if (match_json_key(f, "accounts") < 0)
    {
        fclose(f);
        return -1;
    }

    skip_whitespace(f);
    if (fgetc(f) != '[')
    {
        fclose(f);
        return -1;
    }

    /* Read each account */
    for (int i = 0; i < accounts_in_use; i++)
    {
        skip_whitespace(f);

        if (fgetc(f) != '{')
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        account_t *a = &bank[i];

        /* Read account number */
        if (match_json_key(f, "number") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->number) != 1)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        /* Read PIN */
        if (skip_to_char(f, ',') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "pin") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->pin) != 1)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        /* Read name */
        if (skip_to_char(f, ',') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "name") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (read_json_string(f, a->name, sizeof(a->name)) < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        /* Read national ID */
        if (skip_to_char(f, ',') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "nat_id") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (read_json_string(f, a->nat_id, sizeof(a->nat_id)) < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        /* Read account type */
        if (skip_to_char(f, ',') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "type") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        int type;
        if (fscanf(f, "%d", &type) != 1)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        a->type = (acct_type_t)type;

        /* Read balance */
        if (skip_to_char(f, ',') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "balance") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->balance) != 1)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        /* Read ntran */
        if (skip_to_char(f, ',') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "ntran") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (fscanf(f, "%d", &a->ntran) != 1)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        /* Read last transactions */
        if (skip_to_char(f, ',') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }
        if (match_json_key(f, "last") < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        skip_whitespace(f);
        if (fgetc(f) != '[')
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        /* Read up to TRANS_KEEP transactions */
        int trans_read = 0;
        int next_char;

        skip_whitespace(f);
        next_char = fgetc(f);

        while (next_char != ']' && trans_read < TRANS_KEEP)
        {
            ungetc(next_char, f);

            if (read_transaction_json(f, &a->last[trans_read % TRANS_KEEP]) < 0)
            {
                accounts_in_use = i;
                fclose(f);
                return -1;
            }

            trans_read++;

            skip_whitespace(f);
            next_char = fgetc(f);

            if (next_char == ',')
            {
                skip_whitespace(f);
                next_char = fgetc(f);
            }
        }

        /* Skip to end of account object */
        if (skip_to_char(f, '}') < 0)
        {
            accounts_in_use = i;
            fclose(f);
            return -1;
        }

        skip_whitespace(f);
        next_char = fgetc(f);

        /* Check for comma or end of array */
        if (next_char != ',' && next_char != ']')
        {
            accounts_in_use = i + 1;
            fclose(f);
            return -1;
        }

        if (next_char == ']' && i < accounts_in_use - 1)
        {
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