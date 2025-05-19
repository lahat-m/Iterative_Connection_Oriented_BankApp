/*
 * Banking System - Server networking functionality
 */

#ifndef BANK_SERVER_H
#define BANK_SERVER_H

#include "bank_common.h"

/* Server function prototypes */
void handle_client(int client_socket);
void shutdown_server(int port);
int start_server(int port );

#endif /* BANK_SERVER_H */