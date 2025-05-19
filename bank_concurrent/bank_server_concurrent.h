/*
 * Banking System - Concurrent Server (using processes)
 */

#ifndef BANK_SERVER_CONCURRENT_H
#define BANK_SERVER_CONCURRENT_H

#include "../server/bank_common.h"
#include <signal.h>  /* For struct sigaction */

/* Server function prototypes */
void shutdown_server(int signal);
void handle_client(int client_socket);
int init_server(int port);
void run_server(void);
void child_handler(int sig);

#endif /* BANK_SERVER_CONCURRENT_H */