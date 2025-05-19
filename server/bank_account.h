/*
 * Banking System - Account operations
 */

#ifndef BANK_ACCOUNT_H
#define BANK_ACCOUNT_H

#include "bank_common.h"

/* Account operation prototypes */
int gen_pin(void);
transaction_t *slot_for(account_t *a);
void remember(account_t *a, char typ, int amt);
account_t *open_account(const char *name, const char *nid, acct_type_t t);
int close_account(int acc_no, int pin);
int deposit(int acc_no, int pin, int amount);
int withdraw(int acc_no, int pin, int amount);
int balance(int acc_no, int pin, int *bal_out);
int statement(int acc_no, int pin, response_t *resp);

#endif /* BANK_ACCOUNT_H */