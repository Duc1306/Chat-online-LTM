# ChatOnline Makefile for Linux
# Build server and client applications

CC = gcc
CFLAGS = -Wall -Wextra -pthread
SERVER_DIR = server
CLIENT_DIR = client
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

# Targets
all: server client client_gtk

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

client_gtk:
	@echo "Building GTK GUI client..."
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $(CLIENT_DIR)/client_gtk \
		$(CLIENT_DIR)/client_gtk.c \
		$(CLIENT_DIR)/protocol.c \
		$(GTK_LIBS)
	@echo "GTK client build complete: $(CLIENT_DIR)/client_gtk"

clean:
	@echo "Cleaning up..."
	rm -f $(SERVER_DIR)/chat_server
	rm -f $(CLIENT_DIR)/client
	rm -f $(CLIENT_DIR)/client_gtk
	@echo "Clean complete"
run-client-gtk:
	@echo "Starting GTK GUI client..."
	cd $(CLIENT_DIR) && ./client_gtk

help:
	@echo "ChatOnline Makefile for Linux"
	@echo ""
	@echo "Available targets:"
	@echo "  make all           - Build server, console client and GTK client (default)"
	@echo "  make server        - Build only server"
	@echo "  make client        - Build only console client"
	@echo "  make client_gtk    - Build only GTK GUI client"
	@echo "  make clean         - Remove built executables"
	@echo "  make run-server    - Start the server"
	@echo "  make run-client    - Start the console client"
	@echo "  make run-client-gtk - Start the GTK GUI client"
	@echo "  make help          - Show this help message"
	@echo ""
	@echo "To test:"
	@echo "  1. Terminal 1: make run-server"
	@echo "  2. Terminal 2: make run-client (or make run-client-gtk for GUI)"

.PHONY: all server client client_gtk clean run-server run-client run-client-gtk"
	@echo "  make run-server  - Start the server"
	@echo "  make run-client  - Start the client"
	@echo "  make help        - Show this help message"
	@echo ""
	@echo "To test:"
	@echo "  1. Terminal 1: make run-server"
	@echo "  2. Terminal 2: make run-client"

.PHONY: all server client clean run-server run-client help
