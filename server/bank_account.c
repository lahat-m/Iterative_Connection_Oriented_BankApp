/*
 * Banking System - Account operations implementation
 */

#include "bank_account.h"
#include "bank_persistence.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Generate a random 4-digit PIN */
int gen_pin(void)
{
    return 1000 + rand() % 9000; /* random 4-digit pin */
}

/* Get the slot for a new transaction in an account */
transaction_t *slot_for(account_t *a)
{
    if (!a)
    {
        return NULL; /* Prevent null pointer access */
    }
    return &a->last[a->ntran % TRANS_KEEP];
}

/* Record a transaction in an account's history */
void remember(account_t *a, char typ, int amt)
{
    if (!a)
    {
        return; /* Prevent null pointer access */
    }

    transaction_t *t = slot_for(a);
    if (!t)
    {
        return;
    }

    a->ntran++;
    t->type = typ;
    t->amount = amt;
    t->when = time(NULL);
    t->balance_after = a->balance;
}

/* Create a new bank account */
account_t *open_account(const char *name, const char *nid, acct_type_t t) {
    if (accounts_in_use >= MAX_ACCTS) {
        return NULL;
    }

    account_t *a = &bank[accounts_in_use++];
    memset(a, 0, sizeof(*a));

    a->number = next_number++;
    a->pin = gen_pin();
    strncpy(a->name, name, sizeof(a->name) - 1);
    strncpy(a->nat_id, nid, sizeof(a->nat_id) - 1);
    a->type = t;
    a->balance = MIN_BALANCE; /* initial 1 k mandatory */
    remember(a, 'D', MIN_BALANCE);

    // Save after modification
    save_data();
    return a;
}

/* Close an existing bank account */
int close_account(int acc_no, int pin) {
    for (int i=0;i<accounts_in_use;i++) {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            /* shift the tail of the array left */
            bank[i] = bank[--accounts_in_use];
            save_data();
            return STATUS_OK;
        }
    }

    return STATUS_ERROR;
}

/* Deposit money into an account */
int deposit(int acc_no, int pin, int amount){
    if (amount < MIN_DEPOSIT) {
        return STATUS_INVALID;
    }

    for (int i=0;i<accounts_in_use;i++){
        if (bank[i].number==acc_no && bank[i].pin == pin){
            bank[i].balance+=amount;
            remember(&bank[i], 'D', amount);

            save_data();
            return STATUS_OK;
        }
    }

    return STATUS_ERROR;
}

/* Withdraw money from an account */
int withdraw(int acc_no, int pin, int amount) {
    if (amount < MIN_WITHDRAW || amount % MIN_WITHDRAW) {
        return STATUS_INVALID;
    }

    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            if (bank[i].balance - amount < MIN_BALANCE)
            {
                return STATUS_MIN_AMT;
            }

            bank[i].balance -= amount;
            remember(&bank[i], 'W', amount);

            save_data();
            return STATUS_OK;
        }
    }

    return STATUS_ERROR;
}

/* Get account balance */
int balance(int acc_no, int pin, int *bal_out)
{
    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            *bal_out = bank[i].balance;
            return STATUS_OK;
        }
    }

    return STATUS_ERROR;
}

/* Get account statement with transaction history */
int statement(int acc_no, int pin, response_t *resp)
{
    for (int i = 0; i < accounts_in_use; i++)
    {
        if (bank[i].number == acc_no && bank[i].pin == pin)
        {
            account_t *a = &bank[i];
            int start = (a->ntran > TRANS_KEEP) ? a->ntran - TRANS_KEEP : 0;
            int count = 0;

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

    return STATUS_ERROR;
}