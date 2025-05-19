# Makefile for Banking System Concurrent Server

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall

# Server source directory
SERVER_DIR = ../server

# Source files
# Files from concurrent implementation
CONCURRENT_SRCS = main_concurrent.c bank_server_concurrent.c

# Files from original server implementation (in SERVER_DIR)
SERVER_SRCS = $(SERVER_DIR)/bank_account.c $(SERVER_DIR)/bank_persistence.c

# Header files
HEADERS = $(SERVER_DIR)/bank_common.h $(SERVER_DIR)/bank_account.h \
          $(SERVER_DIR)/bank_persistence.h \
          bank_server_concurrent.h

# Output executable name
TARGET = bank_server_concurrent

# Default target
all: $(TARGET)

# Compile server with vpath to find headers
$(TARGET): $(CONCURRENT_SRCS) $(SERVER_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -I$(SERVER_DIR) -o $(TARGET) $(CONCURRENT_SRCS) $(SERVER_SRCS)

# Clean
clean:
	rm -f $(TARGET)

.PHONY: all clean