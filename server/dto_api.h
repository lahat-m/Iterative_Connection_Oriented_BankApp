/* ---------- API required in the statement -------------------------- */
static account_t *open_account(const char *name, const char *nid, acct_type_t t)
{
    log_message(LOG_INFO, "Attempting to open new account for %s (ID: %s, Type: %d)",
                name, nid, t);

    if (accounts_in_use >= MAX_ACCTS)
    {
        log_message(LOG_ERROR, "Cannot open account: maximum accounts limit reached");
        return NULL;
    }

    account_t *a = &bank[accounts_in_use++];
    memset(a, 0, sizeof(*a));

    a->number = next_number++;
    a->pin = gen_pin();
    strncpy(a->name, name, sizeof(a->name) - 1);
    strncpy(a->nat_id, nid, sizeof(a->nat_id) - 1);
    a->type = t;
    a->balance = MIN_BALANCE; /* initial 1 k mandatory    */
    remember(a, 'D', MIN_BALANCE);

    log_message(LOG_INFO, "Account created: Number=%d, PIN=%04d, Balance=%d",
                a->number, a->pin, a->balance);

    // Save after modification
    save_data();
    return a;
}

static int close_account(int acc_no, int pin)
{
    log_message(LOG_INFO, "Attempting to close account %d", acc_no);

    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            log_message(LOG_INFO, "Closing account %d with balance %d",
                        acc_no, bank[i].balance);

            /* shift the tail of the array left */
            bank[i] = bank[--accounts_in_use];
            save_data();
            return STATUS_OK;
        }
    }

    log_message(LOG_WARNING, "Failed to close account %d: account not found or wrong PIN", acc_no);
    return STATUS_ERROR;
}

static int deposit(int acc_no, int pin, int amount)
{
    log_message(LOG_INFO, "Deposit request: Account %d, Amount %d", acc_no, amount);

    if (amount < MIN_DEPOSIT)
    {
        log_message(LOG_WARNING, "Deposit rejected: Amount %d less than minimum deposit %d",
                    amount, MIN_DEPOSIT);
        return STATUS_INVALID;
    }

    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            bank[i].balance += amount;
            remember(&bank[i], 'D', amount);

            log_message(LOG_INFO, "Deposit successful: Account %d, Amount %d, New Balance %d",
                        acc_no, amount, bank[i].balance);

            save_data();
            return STATUS_OK;
        }
    }

    log_message(LOG_WARNING, "Deposit failed: Account %d not found or wrong PIN", acc_no);
    return STATUS_ERROR;
}

static int withdraw(int acc_no, int pin, int amount)
{
    log_message(LOG_INFO, "Withdrawal request: Account %d, Amount %d", acc_no, amount);

    if (amount < MIN_WITHDRAW || amount % MIN_WITHDRAW)
    {
        log_message(LOG_WARNING, "Withdrawal rejected: Amount %d not valid (must be >= %d and multiple of %d)",
                    amount, MIN_WITHDRAW, MIN_WITHDRAW);
        return STATUS_INVALID;
    }

    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            if (bank[i].balance - amount < MIN_BALANCE)
            {
                log_message(LOG_WARNING, "Withdrawal rejected: Would break minimum balance (Current: %d, After: %d, Min: %d)",
                            bank[i].balance, bank[i].balance - amount, MIN_BALANCE);
                return STATUS_MIN_AMT;
            }

            bank[i].balance -= amount;
            remember(&bank[i], 'W', amount);

            log_message(LOG_INFO, "Withdrawal successful: Account %d, Amount %d, New Balance %d",
                        acc_no, amount, bank[i].balance);

            save_data();
            return STATUS_OK;
        }
    }

    log_message(LOG_WARNING, "Withdrawal failed: Account %d not found or wrong PIN", acc_no);
    return STATUS_ERROR;
}

static int balance(int acc_no, int pin, int *bal_out)
{
    log_message(LOG_INFO, "Balance inquiry: Account %d", acc_no);

    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            *bal_out = bank[i].balance;
            log_message(LOG_INFO, "Balance reported: Account %d, Balance %d", acc_no, *bal_out);
            return STATUS_OK;
        }
    }

    log_message(LOG_WARNING, "Balance inquiry failed: Account %d not found or wrong PIN", acc_no);
    return STATUS_ERROR;
}

static int statement(int acc_no, int pin, response_t *resp)
{
    log_message(LOG_INFO, "Statement request: Account %d", acc_no);

    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            account_t *a = &bank[i];
            int start = (a->ntran > TRANS_KEEP) ? a->ntran - TRANS_KEEP : 0;
            int count = 0;

            log_message(LOG_INFO, "Generating statement for account %d with %d transactions",
                        acc_no, a->ntran > TRANS_KEEP ? TRANS_KEEP : a->ntran);

            for (int j = start; j < a->ntran; j++)
            {
                transaction_t *t = &a->last[j % TRANS_KEEP];
                // Copy the transaction to the response
                resp->transactions[count++] = *t;
            }

            resp->transaction_count = count;
            return STATUS_OK;
        }
    }

    log_message(LOG_WARNING, "Statement request failed: Account %d not found or wrong PIN", acc_no);
    return STATUS_ERROR;
}
