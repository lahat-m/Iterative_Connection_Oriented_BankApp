# Simple Makefile for Banking System Client

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall

# Client target
all: bank_client

# Compile client
bank_client: bank_client.c client_network.c client_interpreter.c client_logic.c bank_client.h client_interpreter.h 
	$(CC) $(CFLAGS) -o bank_client bank_client.c client_network.c client_interpreter.c client_logic.c

# Clean
clean:
	rm -f bank_client

.PHONY: all clean