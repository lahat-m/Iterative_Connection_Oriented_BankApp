/*
 * Banking System - Main program (Concurrent Server with processes)
 *
 * Compile: gcc -std=c99 -Wall -o bank_server_concurrent main_concurrent.c bank_server_concurrent.c bank_account.c bank_persistence.c bank_log.c
 * Run: ./bank_server_concurrent [port]
 */

#include "../server/bank_common.h"
#include "../server/bank_log.h"
#include "../server/bank_persistence.h"
#include "../server/bank_account.h"
#include "bank_server_concurrent.h"
#include <signal.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;

    // Set port from command line if provided
    if (argc > 1)
    {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535)
        {
            fprintf(stderr, "Invalid port number. Using default port %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }

    // Initialize random number generator
    srand(time(NULL));

    // Set up signal handlers for graceful shutdown
    signal(SIGINT, shutdown_server);
    signal(SIGTERM, shutdown_server);

    // Initialize logging
    log_init();
    log_message(LOG_INFO, "Starting concurrent server (using processes)");

    // Load existing data
    if (load_data() != 0)
    {
        log_message(LOG_WARNING, "Could not load existing data. Starting fresh.");
        fprintf(stderr, "Warning: Could not load existing data. Starting fresh.\n");
    }

    // Initialize and run the server
    if (init_server(port) != 0)
    {
        log_message(LOG_ERROR, "Failed to initialize server. Exiting.");
        return EXIT_FAILURE;
    }

    run_server();

    // Save data before exiting (though this should be handled by the signal handler)
    save_data();

    return EXIT_SUCCESS;
}