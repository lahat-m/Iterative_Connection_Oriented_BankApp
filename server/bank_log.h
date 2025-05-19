/*
 * Banking System - Logging functionality
 */

#ifndef BANK_LOG_H
#define BANK_LOG_H

#include "bank_common.h"

/* Logging function prototypes */
void log_init(void);
void log_message(log_level_t level, const char *format, ...);

#endif /* BANK_LOG_H */