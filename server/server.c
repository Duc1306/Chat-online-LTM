#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

ServerState server_state;

static void signal_handler(int signum);

// ===========================
// 1. SOCKET INITIALIZATION
// ===========================

/**
 * Khởi tạo server socket
 */
int init_server_socket(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // Tạo socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    // Set socket options (cho phép reuse address)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        close(server_fd);
        return -1;
    }
    
    // Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }
    
    // Listen
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }
    
    printf("[SERVER] Server initialized on port %d\n", port);
    log_server_event("SERVER_START", "Server started successfully");
    
    return server_fd;
}

/**
 * Accept client mới
 */
int accept_client(int server_socket, ClientConnection *client) {
    if (client == NULL) return -1;
    
    socklen_t addr_len = sizeof(client->address);
    int client_socket = accept(server_socket, 
                               (struct sockaddr *)&client->address, 
                               &addr_len);
    
    if (client_socket < 0) {
        perror("Accept failed");
        return -1;
    }
    
    // Initialize client connection
    client->socket_fd = client_socket;
    client->is_authenticated = false;
    memset(client->username, 0, MAX_USERNAME_LEN);
    
    char client_ip[INET_ADDRSTRLEN];
    // Linux: Use inet_ntop
    inet_ntop(AF_INET, &(client->address.sin_addr), client_ip, INET_ADDRSTRLEN);
    
    printf("[SERVER] New connection from %s:%d (socket: %d)\n", 
           client_ip, ntohs(client->address.sin_port), client_socket);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "New connection from %s:%d", 
             client_ip, ntohs(client->address.sin_port));
    log_server_event("CLIENT_CONNECT", log_msg);
    
    return client_socket;
}

// ===========================
// 2. TCP STREAM HANDLING (PRIORITY 1)
// ===========================

/**
 * Nhận message từ socket với xử lý phân mảnh
 * Protocol: [4 bytes length][message data]
 */
int recv_message(int socket_fd, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) return -1;
    
    // Bước 1: Đọc 4 bytes header chứa message length
    uint32_t msg_length;
    int total_received = 0;
    int bytes_received;
    
    while (total_received < sizeof(uint32_t)) {
        bytes_received = recv(socket_fd, 
                            ((char *)&msg_length) + total_received,
                            sizeof(uint32_t) - total_received, 
                            0);
        
        if (bytes_received < 0) {
            if (errno == EINTR) continue;  // Interrupted, retry
            perror("recv() failed on header");
            return -1;
        }
        
        if (bytes_received == 0) {
            // Connection closed
            return 0;
        }
        
        total_received += bytes_received;
    }
    
    // Convert từ network byte order
    msg_length = ntohl(msg_length);
    
    // Kiểm tra message length hợp lệ
    if (msg_length == 0 || msg_length > buffer_size - 1) {
        fprintf(stderr, "[ERROR] Invalid message length: %u (max: %zu)\n", 
                msg_length, buffer_size - 1);
        return -1;
    }
    
    // Bước 2: Đọc message data
    total_received = 0;
    while (total_received < msg_length) {
        bytes_received = recv(socket_fd, 
                            buffer + total_received,
                            msg_length - total_received, 
                            0);
        
        if (bytes_received < 0) {
            if (errno == EINTR) continue;  // Interrupted, retry
            perror("recv() failed on data");
            return -1;
        }
        
        if (bytes_received == 0) {
            // Connection closed unexpectedly
            fprintf(stderr, "[ERROR] Connection closed while receiving data\n");
            return 0;
        }
        
        total_received += bytes_received;
    }
    
    buffer[total_received] = '\0';  // Null terminate
    
    return total_received;
}

/**
 * Gửi message qua socket với xử lý phân mảnh
 * Protocol: [4 bytes length][message data]
 */
int send_message(int socket_fd, const char *message, size_t length) {
    if (message == NULL || length == 0) return -1;
    
    // Bước 1: Gửi 4 bytes header chứa message length
    uint32_t msg_length = htonl((uint32_t)length);  // Convert to network byte order
    int total_sent = 0;
    int bytes_sent;
    
    while (total_sent < sizeof(uint32_t)) {
        bytes_sent = send(socket_fd, 
                         ((char *)&msg_length) + total_sent,
                         sizeof(uint32_t) - total_sent, 
                         0);
        
        if (bytes_sent < 0) {
            if (errno == EINTR) continue;  // Interrupted, retry
            perror("send() failed on header");
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    // Bước 2: Gửi message data
    total_sent = 0;
    while (total_sent < length) {
        bytes_sent = send(socket_fd, 
                         message + total_sent,
                         length - total_sent, 
                         0);
        
        if (bytes_sent < 0) {
            if (errno == EINTR) continue;  // Interrupted, retry
            perror("send() failed on data");
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    return total_sent;
}

/**
 * Gửi Message struct đã serialize
 */
int send_message_struct(int socket_fd, const Message *msg) {
    if (msg == NULL) return -1;
    
    char buffer[BUFFER_SIZE];
    int len = serialize_message(msg, buffer, sizeof(buffer));
    
    if (len < 0) {
        fprintf(stderr, "[ERROR] Failed to serialize message\n");
        return -1;
    }
    
    return send_message(socket_fd, buffer, len);
}

// ===========================
// 3. ACCOUNT MANAGEMENT
// ===========================

/**
 * Load users từ file
 * Format: username|password|last_seen
 */
int load_users_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        // File chưa tồn tại, tạo mới
        fp = fopen(filename, "w");
        if (fp != NULL) {
            fclose(fp);
        }
        return 0;
    }
    
    mutex_lock(&server_state.users_mutex);
    
    server_state.user_count = 0;
    char line[512];
    
    while (fgets(line, sizeof(line), fp) != NULL && 
           server_state.user_count < MAX_CLIENTS) {
        User *user = &server_state.users[server_state.user_count];
        
        // Parse line
        char *username = strtok(line, "|");
        char *password = strtok(NULL, "|");
        char *last_seen_str = strtok(NULL, "|\n");
        
        if (username != NULL && password != NULL) {
            strncpy(user->username, username, MAX_USERNAME_LEN - 1);
            strncpy(user->password, password, MAX_PASSWORD_LEN - 1);
            user->is_online = 0;
            user->socket_fd = -1;
            
            if (last_seen_str != NULL) {
                user->last_seen = (time_t)atoll(last_seen_str);
            } else {
                user->last_seen = time(NULL);
            }
            
            server_state.user_count++;
        }
    }
    
    mutex_unlock(&server_state.users_mutex);
    fclose(fp);
    
    printf("[SERVER] Loaded %d users from %s\n", server_state.user_count, filename);
    return server_state.user_count;
}

/**
 * Save user mới vào file
 */
int save_user_to_file(const char *filename, const User *user) {
    if (user == NULL) return -1;
    
    mutex_lock(&server_state.file_mutex);
    
    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
        perror("[ERROR] Failed to open users file");
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("[ERROR] Current working directory: %s\n", cwd);
        }
        mutex_unlock(&server_state.file_mutex);
        return -1;
    }
    
    fprintf(fp, "%s|%s|%ld\n", user->username, user->password, user->last_seen);
    fclose(fp);
    
    printf("[FILE] Saved user '%s' to %s\n", user->username, filename);
    
    mutex_unlock(&server_state.file_mutex);
    
    return 0;
}

/**
 * Xử lý đăng ký user mới
 */
int handle_register(ClientConnection *client, const Message *msg) {
    if (client == NULL || msg == NULL) return -1;
    
    Message response;
    
    // Parse username và password từ msg->content
    // Format: username|password
    printf("[DEBUG] REGISTER - msg->content = '%s'\n", msg->content);
    printf("[DEBUG] REGISTER - msg->from = '%s'\n", msg->from);
    printf("[DEBUG] REGISTER - msg->to = '%s'\n", msg->to);
    
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    
    char content_copy[MAX_MESSAGE_LEN];
    strncpy(content_copy, msg->content, MAX_MESSAGE_LEN - 1);
    content_copy[MAX_MESSAGE_LEN - 1] = '\0';
    
    printf("[DEBUG] content_copy = '%s'\n", content_copy);
    
    char *token = strtok(content_copy, "|");
    printf("[DEBUG] token1 (username) = '%s'\n", token ? token : "NULL");
    if (token == NULL) {
        create_response_message(&response, MSG_ERROR, "SERVER", msg->from, 
                               "Invalid registration format");
        send_message_struct(client->socket_fd, &response);
        return -1;
    }
    strncpy(username, token, MAX_USERNAME_LEN - 1);
    
    token = strtok(NULL, "|");
    if (token == NULL) {
        create_response_message(&response, MSG_ERROR, "SERVER", msg->from, 
                               "Invalid registration format");
        send_message_struct(client->socket_fd, &response);
        return -1;
    }
    strncpy(password, token, MAX_PASSWORD_LEN - 1);
    
    // Kiểm tra username đã tồn tại chưa
    mutex_lock(&server_state.users_mutex);
    
    for (int i = 0; i < server_state.user_count; i++) {
        if (strcmp(server_state.users[i].username, username) == 0) {
            mutex_unlock(&server_state.users_mutex);
            
            create_response_message(&response, MSG_ERROR, "SERVER", username, 
                                   "Username already exists");
            send_message_struct(client->socket_fd, &response);
            
            printf("[REGISTER] Failed: Username '%s' already exists\n", username);
            return -1;
        }
    }
    
    // Thêm user mới
    if (server_state.user_count >= MAX_CLIENTS) {
        mutex_unlock(&server_state.users_mutex);
        
        create_response_message(&response, MSG_ERROR, "SERVER", username, 
                               "Server full");
        send_message_struct(client->socket_fd, &response);
        return -1;
    }
    
    User new_user;
    strncpy(new_user.username, username, MAX_USERNAME_LEN - 1);
    strncpy(new_user.password, password, MAX_PASSWORD_LEN - 1);
    new_user.is_online = 0;
    new_user.socket_fd = -1;
    new_user.last_seen = time(NULL);
    
    server_state.users[server_state.user_count] = new_user;
    server_state.user_count++;
    
    mutex_unlock(&server_state.users_mutex);
    
    // Lưu vào file
    int save_result = save_user_to_file("users.txt", &new_user);
    
    if (save_result < 0) {
        printf("[ERROR] Failed to save user '%s' to file!\n", username);
    }
    
    // Gửi response thành công
    create_response_message(&response, MSG_SUCCESS, "SERVER", username, 
                           "Registration successful");
    send_message_struct(client->socket_fd, &response);
    
    printf("[REGISTER] Success: User '%s' registered\n", username);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "User registered: %s", username);
    log_server_event("REGISTER", log_msg);
    
    return 0;
}

/**
 * Xử lý đăng nhập
 */
int handle_login(ClientConnection *client, const Message *msg) {
    if (client == NULL || msg == NULL) return -1;
    
    Message response;
    
    // Parse username và password
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    
    char content_copy[MAX_MESSAGE_LEN];
    strncpy(content_copy, msg->content, MAX_MESSAGE_LEN - 1);
    
    char *token = strtok(content_copy, "|");
    if (token == NULL) {
        create_response_message(&response, MSG_ERROR, "SERVER", "", 
                               "Invalid login format");
        send_message_struct(client->socket_fd, &response);
        return -1;
    }
    strncpy(username, token, MAX_USERNAME_LEN - 1);
    
    token = strtok(NULL, "|");
    if (token == NULL) {
        create_response_message(&response, MSG_ERROR, "SERVER", username, 
                               "Invalid login format");
        send_message_struct(client->socket_fd, &response);
        return -1;
    }
    strncpy(password, token, MAX_PASSWORD_LEN - 1);
    
    // Kiểm tra username và password
    mutex_lock(&server_state.users_mutex);
    
    int user_found = -1;
    for (int i = 0; i < server_state.user_count; i++) {
        if (strcmp(server_state.users[i].username, username) == 0) {
            user_found = i;
            break;
        }
    }
    
    if (user_found == -1) {
        mutex_unlock(&server_state.users_mutex);
        
        create_response_message(&response, MSG_ERROR, "SERVER", username, 
                               "Username not found");
        send_message_struct(client->socket_fd, &response);
        
        printf("[LOGIN] Failed: Username '%s' not found\n", username);
        return -1;
    }
    
    if (strcmp(server_state.users[user_found].password, password) != 0) {
        mutex_unlock(&server_state.users_mutex);
        
        create_response_message(&response, MSG_ERROR, "SERVER", username, 
                               "Wrong password");
        send_message_struct(client->socket_fd, &response);
        
        printf("[LOGIN] Failed: Wrong password for '%s'\n", username);
        return -1;
    }
    
    // Kiểm tra user đã online chưa (không cho login 2 lần)
    if (server_state.users[user_found].is_online) {
        mutex_unlock(&server_state.users_mutex);
        
        create_response_message(&response, MSG_ERROR, "SERVER", username, 
                               "User already logged in");
        send_message_struct(client->socket_fd, &response);
        
        printf("[LOGIN] Failed: User '%s' already online\n", username);
        return -1;
    }
    
    mutex_unlock(&server_state.users_mutex);
    
    // Login thành công
    client->is_authenticated = true;
    strncpy(client->username, username, MAX_USERNAME_LEN - 1);
    
    // Cập nhật trạng thái online
    int add_result = add_online_user(username, client->socket_fd);
    printf("[LOGIN] add_online_user result: %d for user '%s'\n", add_result, username);
    
    // Gửi response thành công
    create_response_message(&response, MSG_SUCCESS, "SERVER", username, 
                           "Login successful");
    send_message_struct(client->socket_fd, &response);
    
    printf("[LOGIN] Success: User '%s' logged in (socket: %d)\n", 
           username, client->socket_fd);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "User logged in: %s", username);
    log_server_event("LOGIN", log_msg);
    
    // Gửi offline messages
    send_offline_messages(client->socket_fd, username);
    
    // Broadcast user online status
    Message online_msg;
    create_response_message(&online_msg, MSG_USER_ONLINE, "SERVER", "", username);
    broadcast_message(&online_msg, username);
    
    return 0;
}

/**
 * Xử lý logout
 */
int handle_logout(ClientConnection *client) {
    if (client == NULL || !client->is_authenticated) return -1;
    
    printf("[LOGOUT] User '%s' logging out\n", client->username);
    
    // Broadcast user offline status
    Message offline_msg;
    create_response_message(&offline_msg, MSG_USER_OFFLINE, "SERVER", "", 
                           client->username);
    broadcast_message(&offline_msg, client->username);
    
    // Remove từ online list
    remove_online_user(client->username);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "User logged out: %s", client->username);
    log_server_event("LOGOUT", log_msg);
    
    return 0;
}

// ===========================
// 11. CLIENT THREAD & MESSAGE DISPATCHER
// ===========================

/**
 * Cleanup client connection
 */
void cleanup_client(ClientConnection *client) {
    if (client == NULL) return;
    
    if (client->is_authenticated) {
        handle_logout(client);
    }
    
    if (client->socket_fd > 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    
    client->is_authenticated = false;
    memset(client->username, 0, MAX_USERNAME_LEN);
}

/**
 * Thread xử lý mỗi client
 */
void *client_thread(void *arg) {
    ClientConnection *client = (ClientConnection *)arg;
    char buffer[BUFFER_SIZE];
    Message msg;
    
    printf("[THREAD] Started thread for client socket %d\n", client->socket_fd);
    
    while (1) {
        // Nhận message từ client
        int bytes_received = recv_message(client->socket_fd, buffer, sizeof(buffer));
        
        if (bytes_received <= 0) {
            // Client disconnected hoặc error
            if (bytes_received == 0) {
                printf("[THREAD] Client socket %d disconnected\n", client->socket_fd);
            } else {
                printf("[THREAD] Error receiving from socket %d\n", client->socket_fd);
            }
            break;
        }
        
        // Parse message
        if (parse_message(buffer, &msg) < 0) {
            fprintf(stderr, "[ERROR] Failed to parse message: %s\n", buffer);
            continue;
        }
        
        printf("[THREAD] Received message type %d from socket %d\n", 
               msg.type, client->socket_fd);
        
        // Dispatch message theo type
        switch (msg.type) {
            case MSG_REGISTER:
                handle_register(client, &msg);
                break;
                
            case MSG_LOGIN:
                handle_login(client, &msg);
                break;
                
            case MSG_LOGOUT:
                handle_logout(client);
                goto cleanup;  // Exit thread
                
            case MSG_PRIVATE_MESSAGE:
                if (!client->is_authenticated) {
                    Message err;
                    create_response_message(&err, MSG_ERROR, "SERVER", "", 
                                          "Not authenticated");
                    send_message_struct(client->socket_fd, &err);
                    break;
                }
                relay_private_message(&msg);
                break;
                
            case MSG_PRIVATE_CHAT_START:
                if (!client->is_authenticated) break;
                handle_private_chat_start(&msg);
                break;
                
            case MSG_PRIVATE_CHAT_END:
                if (!client->is_authenticated) break;
                handle_private_chat_end(&msg);
                break;
                
            case MSG_GROUP_CREATE:
                if (!client->is_authenticated) break;
                create_group(msg.content, client->username);
                {
                    Message response;
                    create_response_message(&response, MSG_SUCCESS, "SERVER", 
                                          client->username, "Group created");
                    send_message_struct(client->socket_fd, &response);
                }
                break;
                
            case MSG_GROUP_INVITE:
                if (!client->is_authenticated) break;
                handle_group_invite(&msg);
                break;
                
            case MSG_GROUP_JOIN:
                if (!client->is_authenticated) break;
                // msg.extra = group_name
                handle_group_join(msg.extra, client->username);
                {
                    Message response;
                    create_response_message(&response, MSG_SUCCESS, "SERVER", 
                                          client->username, "Joined group");
                    send_message_struct(client->socket_fd, &response);
                }
                break;
                
            case MSG_GROUP_LEAVE:
                if (!client->is_authenticated) break;
                handle_group_leave(msg.extra, client->username);
                {
                    Message response;
                    create_response_message(&response, MSG_SUCCESS, "SERVER", 
                                          client->username, "Left group");
                    send_message_struct(client->socket_fd, &response);
                }
                break;
                
            case MSG_GROUP_MESSAGE:
                if (!client->is_authenticated) break;
                relay_group_message(&msg);
                break;
                
            case MSG_FRIEND_REQUEST:
                if (!client->is_authenticated) break;
                handle_friend_request(&msg);
                break;
                
            case MSG_FRIEND_ACCEPT:
                if (!client->is_authenticated) break;
                handle_friend_accept(&msg);
                break;
                
            case MSG_FRIEND_REJECT:
                if (!client->is_authenticated) break;
                handle_friend_reject(&msg);
                break;
                
            case MSG_FRIEND_REMOVE:
                if (!client->is_authenticated) break;
                handle_friend_remove(&msg);
                break;
                
            case MSG_GET_ONLINE_USERS:
                if (!client->is_authenticated) break;
                send_online_users_list(client->socket_fd);
                break;
                
            case MSG_GET_FRIENDS:
                if (!client->is_authenticated) break;
                send_friends_list(client->socket_fd, client->username);
                break;
                
            case MSG_GET_GROUPS:
                if (!client->is_authenticated) break;
                send_user_groups_list(client->socket_fd, client->username);
                break;
                
            case MSG_FILE_SEND:
                if (!client->is_authenticated) break;
                handle_file_transfer(client, &msg);
                break;
                
            case MSG_FILE_ACCEPT:
                if (!client->is_authenticated) break;
                handle_file_accept(&msg);
                break;
                
            case MSG_FILE_REJECT:
                if (!client->is_authenticated) break;
                handle_file_reject(&msg);
                break;
                
            default:
                fprintf(stderr, "[WARNING] Unknown message type: %d\n", msg.type);
                break;
        }
    }
    
cleanup:
    // Cleanup client
    cleanup_client(client);
    
    // Remove client from clients array
    mutex_lock(&server_state.clients_mutex);
    for (int i = 0; i < server_state.client_count; i++) {
        if (&server_state.clients[i] == client) {
            // Shift remaining clients
            for (int j = i; j < server_state.client_count - 1; j++) {
                server_state.clients[j] = server_state.clients[j + 1];
            }
            server_state.client_count--;
            break;
        }
    }
    mutex_unlock(&server_state.clients_mutex);
    
    printf("[THREAD] Thread exiting for client\n");
    return 0;  // Windows thread return
}

// ===========================
// 12. SERVER INITIALIZATION & MAIN
// ===========================

/**
 * Initialize server state
 */
void init_server_state(void) {
    memset(&server_state, 0, sizeof(ServerState));
    
    server_state.server_socket = -1;
    server_state.client_count = 0;
    server_state.user_count = 0;
    server_state.group_count = 0;
    
    mutex_init(&server_state.clients_mutex, NULL);
    mutex_init(&server_state.users_mutex, NULL);
    mutex_init(&server_state.groups_mutex, NULL);
    mutex_init(&server_state.file_mutex, NULL);
    
    printf("[SERVER] Server state initialized\n");
}

/**
 * Cleanup server
 */
void cleanup_server(void) {
    printf("[SERVER] Cleaning up server...\n");
    
    // Close all client connections
    mutex_lock(&server_state.clients_mutex);
    for (int i = 0; i < server_state.client_count; i++) {
        cleanup_client(&server_state.clients[i]);
    }
    mutex_unlock(&server_state.clients_mutex);
    
    // Close server socket
    if (server_state.server_socket > 0) {
        close(server_state.server_socket);
        server_state.server_socket = -1;
    }
    
    // Destroy mutexes
    mutex_destroy(&server_state.clients_mutex);
    mutex_destroy(&server_state.users_mutex);
    mutex_destroy(&server_state.groups_mutex);
    mutex_destroy(&server_state.file_mutex);
    
    log_server_event("SERVER_STOP", "Server stopped");
    
    printf("[SERVER] Cleanup complete\n");
}

/**
 * Signal handler
 */
static void signal_handler(int signum) {
    printf("\n[SERVER] Received signal %d, shutting down...\n", signum);
    cleanup_server();
    exit(0);
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    // Initialize network (Windows WSA)
    if (init_network() < 0) {
        fprintf(stderr, "Failed to initialize network\n");
        return 1;
    }
    
    printf("========================================\n");
    printf("   Chat Server - TCP Socket in C\n");
    printf("========================================\n\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    
    // Initialize server state
    init_server_state();
    
    // Load data từ files
    load_users_from_file("users.txt");
    load_groups_from_file("groups.txt");
    load_friendships_from_file("friendships.txt");
    
    // Initialize server socket
    int port = PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    server_state.server_socket = init_server_socket(port);
    if (server_state.server_socket < 0) {
        fprintf(stderr, "Failed to initialize server socket\n");
        return 1;
    }
    
    printf("[SERVER] Server is running on port %d\n", port);
    printf("[SERVER] Waiting for connections...\n\n");
    
    // Main loop - accept clients
    while (1) {
        mutex_lock(&server_state.clients_mutex);
        
        if (server_state.client_count >= MAX_CLIENTS) {
            mutex_unlock(&server_state.clients_mutex);
            printf("[WARNING] Maximum clients reached, waiting...\n");
            sleep(1);
            continue;
        }
        
        int client_index = server_state.client_count;
        ClientConnection *new_client = &server_state.clients[client_index];
        
        mutex_unlock(&server_state.clients_mutex);
        
        // Accept new client
        int client_socket = accept_client(server_state.server_socket, new_client);
        
        if (client_socket < 0) {
            continue;
        }
        
        // Create thread cho client
        mutex_lock(&server_state.clients_mutex);
        server_state.client_count++;
        mutex_unlock(&server_state.clients_mutex);
        
        // Linux pthread
        if (pthread_create(&new_client->thread_id, NULL, client_thread, new_client) != 0) {
            fprintf(stderr, "Failed to create thread\n");
            cleanup_client(new_client);
            mutex_lock(&server_state.clients_mutex);
            server_state.client_count--;
            mutex_unlock(&server_state.clients_mutex);
        } else {
            pthread_detach(new_client->thread_id);  // Auto cleanup
        }
    }
    
    cleanup_server();
    return 0;
}
