/*
 * Banking System - Logging Module
 */
#include "bank_client.h"
#ifndef CLIENT_LOGGER
#define CLIENT_LOGGER



/* Server function prototypes */
void log_init(void);
void log_close(void);
void log_message(log_level_t level, const char *format, ...);

#endif /* CLIENT_LOGGER */