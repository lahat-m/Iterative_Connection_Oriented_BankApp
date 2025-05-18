/* ---------- persistence functions ----------------------------------- */
static int save_data(void)
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

static int load_data(void)
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
