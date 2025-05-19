/*
 * Banking System - Main program (Concurrent Server with processes)
 *
 * Compile: gcc -std=c99 -Wall -o bank_server_concurrent main_concurrent.c bank_server_concurrent.c ../server/bank_account.c ../server/bank_persistence.c
 * Run: ./bank_server_concurrent [port]
 */

#include "../server/bank_common.h"
#include "../server/bank_persistence.h"
#include "../server/bank_account.h"
#include "bank_server_concurrent.h"
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

    /* Initialize random number generator */
    srand(time(NULL));

    // Load existing data
    if (load_data() != 0)
    {
        fprintf(stderr, "Warning: Could not load existing data. Starting fresh.\n");
    }

    /* Start the server */
    if (start_concurrent_server(port) != 0)
    {
        fprintf(stderr, "Failed to start server. Exiting.\n");
        return EXIT_FAILURE;
    }

    // Save data before exiting (though this should be handled by the signal handler)
    save_data();

    return EXIT_SUCCESS;
}