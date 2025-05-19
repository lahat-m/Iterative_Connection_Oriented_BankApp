# Simple Makefile for Banking System Server

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall

# Server target
all: bank_server

# Compile server
bank_server: main.c bank_server.c bank_account.c bank_persistence.c bank_log.c bank_common.h bank_server.h bank_account.h bank_persistence.h bank_log.h
	$(CC) $(CFLAGS) -o bank_server main.c bank_server.c bank_account.c bank_persistence.c bank_log.c

# Clean
clean:
	rm -f bank_server

.PHONY: all clean