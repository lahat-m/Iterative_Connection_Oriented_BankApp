/*
 * Banking System - Server networking functionality
 */

#ifndef BANK_SERVER_H
#define BANK_SERVER_H

#include "bank_common.h"

/* Server function prototypes */
void shutdown_server(int signal);
void handle_client(int client_socket);
int init_server(int port);
void run_server(void);

#endif /* BANK_SERVER_H */