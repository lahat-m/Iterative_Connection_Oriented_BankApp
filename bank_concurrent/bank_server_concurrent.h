/*
 * Banking System - Concurrent Server (using processes)
 */

#ifndef BANK_SERVER_CONCURRENT_H
#define BANK_SERVER_CONCURRENT_H

#include "../server/bank_common.h"
#include <sys/types.h>  /* For pid_t definition */

/* Server function prototypes */
void handle_client(int client_socket);
int start_concurrent_server(int port);
void track_child_process(pid_t pid);

#endif /* BANK_SERVER_CONCURRENT_H */