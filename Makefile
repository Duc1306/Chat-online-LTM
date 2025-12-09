# ChatOnline Makefile for Linux
# Build server and client applications

CC = gcc
CFLAGS = -Wall -Wextra -pthread
SERVER_DIR = server
CLIENT_DIR = client

# Targets
all: server client

server:
	@echo "Building server..."
	$(CC) $(CFLAGS) -o $(SERVER_DIR)/chat_server \
		$(SERVER_DIR)/server.c \
		$(SERVER_DIR)/server_handlers.c \
		$(SERVER_DIR)/server_utils.c \
		$(CLIENT_DIR)/protocol.c \
		-I$(CLIENT_DIR)
	@echo "Server build complete: $(SERVER_DIR)/chat_server"

client:
	@echo "Building client..."
	$(CC) $(CFLAGS) -o $(CLIENT_DIR)/client \
		$(CLIENT_DIR)/client.c \
		$(CLIENT_DIR)/protocol.c
	@echo "Client build complete: $(CLIENT_DIR)/client"

clean:
	@echo "Cleaning up..."
	rm -f $(SERVER_DIR)/chat_server
	rm -f $(CLIENT_DIR)/client
	@echo "Clean complete"

run-server:
	@echo "Starting server..."
	cd $(SERVER_DIR) && ./chat_server

run-client:
	@echo "Starting client..."
	cd $(CLIENT_DIR) && ./client

help:
	@echo "ChatOnline Makefile for Linux"
	@echo ""
	@echo "Available targets:"
	@echo "  make all         - Build server and client (default)"
	@echo "  make server      - Build only server"
	@echo "  make client      - Build only client"
	@echo "  make clean       - Remove built executables"
	@echo "  make run-server  - Start the server"
	@echo "  make run-client  - Start the client"
	@echo "  make help        - Show this help message"
	@echo ""
	@echo "To test:"
	@echo "  1. Terminal 1: make run-server"
	@echo "  2. Terminal 2: make run-client"

.PHONY: all server client clean run-server run-client help
