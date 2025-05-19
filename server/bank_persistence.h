/*
 * Banking System - Data persistence functions
 */

#ifndef BANK_PERSISTENCE_H
#define BANK_PERSISTENCE_H

#include "bank_common.h"

/* Persistence function prototypes */
int save_data(void);
int load_data(void);

#endif /* BANK_PERSISTENCE_H */