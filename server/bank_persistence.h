/*
 * Banking System - Data persistence functions
 */

#ifndef BANK_PERSISTENCE_H
#define BANK_PERSISTENCE_H

#include "bank_common.h"

/* JSON serialization/deserialization prototypes */
void write_json_string(FILE *f, const char *str);
void write_transaction_json(FILE *f, const transaction_t *t);
void write_account_json(FILE *f, const account_t *a);
void skip_whitespace(FILE *f);
int read_json_string(FILE *f, char *buffer, size_t bufsize);
int skip_to_char(FILE *f, char target);
int match_json_key(FILE *f, const char *key);
int read_transaction_json(FILE *f, transaction_t *t);

/* Persistence function prototypes */
int save_data(void);
int load_data(void);

#endif /* BANK_PERSISTENCE_H */