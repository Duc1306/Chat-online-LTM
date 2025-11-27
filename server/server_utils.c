#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <winsock2.h>
#include <sys/stat.h>
#include <direct.h>

#define mkdir(path, mode) _mkdir(path)

// ===========================
// 7. OFFLINE MESSAGES
// ===========================

/**
 * Lưu tin nhắn offline
 * Format: TO|FROM|TYPE|CONTENT|TIMESTAMP|EXTRA
 */
int save_offline_message(const Message *msg) {
    if (msg == NULL) return -1;
    
    FILE *fp = fopen("offline_messages.txt", "a");
    if (fp == NULL) {
        perror("Failed to open offline_messages.txt");
        return -1;
    }
    
    fprintf(fp, "%s|%s|%d|%s|%s|%s\n",
            msg->to,
            msg->from,
            msg->type,
            msg->content,
            msg->timestamp,
            msg->extra);
    
    fclose(fp);
    
    printf("[OFFLINE] Saved message for '%s' from '%s'\n", msg->to, msg->from);
    
    return 0;
}

/**
 * Gửi tất cả offline messages cho user
 */
int send_offline_messages(int socket_fd, const char *username) {
    if (username == NULL) return -1;
    
    FILE *fp = fopen("offline_messages.txt", "r");
    if (fp == NULL) {
        // File không tồn tại hoặc không có offline messages
        return 0;
    }
    
    // Đọc tất cả messages
    char line[BUFFER_SIZE];
    Message *messages = malloc(sizeof(Message) * 1000);  // Max 1000 messages
    int msg_count = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL && msg_count < 1000) {
        char to[MAX_USERNAME_LEN];
        char from[MAX_USERNAME_LEN];
        int type;
        char content[MAX_MESSAGE_LEN];
        char timestamp[32];
        char extra[MAX_MESSAGE_LEN];
        
        // Parse line
        char *token = strtok(line, "|");
        if (token == NULL) continue;
        strncpy(to, token, MAX_USERNAME_LEN - 1);
        
        token = strtok(NULL, "|");
        if (token == NULL) continue;
        strncpy(from, token, MAX_USERNAME_LEN - 1);
        
        token = strtok(NULL, "|");
        if (token == NULL) continue;
        type = atoi(token);
        
        token = strtok(NULL, "|");
        if (token == NULL) continue;
        strncpy(content, token, MAX_MESSAGE_LEN - 1);
        
        token = strtok(NULL, "|");
        if (token == NULL) continue;
        strncpy(timestamp, token, 31);
        
        token = strtok(NULL, "|\n");
        if (token != NULL) {
            strncpy(extra, token, MAX_MESSAGE_LEN - 1);
        } else {
            extra[0] = '\0';
        }
        
        // Kiểm tra xem message có dành cho user này không
        if (strcmp(to, username) == 0) {
            Message *msg = &messages[msg_count];
            msg->type = type;
            strncpy(msg->from, from, MAX_USERNAME_LEN - 1);
            strncpy(msg->to, to, MAX_USERNAME_LEN - 1);
            strncpy(msg->content, content, MAX_MESSAGE_LEN - 1);
            strncpy(msg->timestamp, timestamp, 31);
            strncpy(msg->extra, extra, MAX_MESSAGE_LEN - 1);
            msg_count++;
        }
    }
    
    fclose(fp);
    
    // Gửi các messages cho user
    printf("[OFFLINE] Sending %d offline messages to '%s'\n", msg_count, username);
    
    for (int i = 0; i < msg_count; i++) {
        send_message_struct(socket_fd, &messages[i]);
    }
    
    free(messages);
    
    // Xóa các messages đã gửi khỏi file
    // (Đọc lại file, skip các messages đã gửi, ghi lại)
    if (msg_count > 0) {
        fp = fopen("offline_messages.txt", "r");
        FILE *temp_fp = fopen("offline_messages_temp.txt", "w");
        
        if (fp != NULL && temp_fp != NULL) {
            while (fgets(line, sizeof(line), fp) != NULL) {
                char to[MAX_USERNAME_LEN];
                char *token = strtok(line, "|");
                if (token != NULL) {
                    strncpy(to, token, MAX_USERNAME_LEN - 1);
                    
                    // Nếu không phải message của user này, giữ lại
                    if (strcmp(to, username) != 0) {
                        // Phải đọc lại line vì strtok đã modify
                        fseek(fp, -(strlen(line) + 1), SEEK_CUR);
                        fgets(line, sizeof(line), fp);
                        fprintf(temp_fp, "%s", line);
                    }
                }
            }
            
            fclose(fp);
            fclose(temp_fp);
            
            // Replace file
            remove("offline_messages.txt");
            rename("offline_messages_temp.txt", "offline_messages.txt");
        }
    }
    
    return msg_count;
}

/**
 * Đếm số lượng offline messages
 */
int count_offline_messages(const char *username) {
    if (username == NULL) return 0;
    
    FILE *fp = fopen("offline_messages.txt", "r");
    if (fp == NULL) return 0;
    
    int count = 0;
    char line[BUFFER_SIZE];
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char to[MAX_USERNAME_LEN];
        char *token = strtok(line, "|");
        if (token != NULL) {
            strncpy(to, token, MAX_USERNAME_LEN - 1);
            if (strcmp(to, username) == 0) {
                count++;
            }
        }
    }
    
    fclose(fp);
    return count;
}

// ===========================
// 8. FILE TRANSFER
// ===========================

/**
 * Xử lý nhận file từ sender
 */
int handle_file_transfer(ClientConnection *client, const Message *msg) {
    if (client == NULL || msg == NULL) return -1;
    
    // msg->extra chứa filename
    // msg->content chứa file size
    // msg->to chứa receiver username
    
    const char *filename = msg->extra;
    const char *receiver = msg->to;
    uint32_t file_size = (uint32_t)atoi(msg->content);
    
    printf("[FILE] User '%s' wants to send file '%s' (%u bytes) to '%s'\n",
           msg->from, filename, file_size, receiver);
    
    // Kiểm tra file size (giới hạn 10MB)
    if (file_size > 10 * 1024 * 1024) {
        Message response;
        create_response_message(&response, MSG_ERROR, "SERVER", msg->from,
                               "File too large (max 10MB)");
        send_message_struct(client->socket_fd, &response);
        return -1;
    }
    
    // Tạo thư mục temp nếu chưa có
    struct stat st = {0};
    if (stat("temp", &st) == -1) {
        mkdir("temp", 0700);
    }
    
    // Nhận file data
    char *file_data = malloc(file_size);
    if (file_data == NULL) {
        perror("Failed to allocate memory for file");
        return -1;
    }
    
    // Đọc file data từ socket
    uint32_t total_received = 0;
    while (total_received < file_size) {
        int bytes = recv(client->socket_fd, file_data + total_received,
                        file_size - total_received, 0);
        if (bytes <= 0) {
            free(file_data);
            return -1;
        }
        total_received += bytes;
    }
    
    // Lưu file vào temp/
    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "temp/%s_%s_%ld",
             msg->from, filename, time(NULL));
    
    FILE *fp = fopen(temp_path, "wb");
    if (fp == NULL) {
        perror("Failed to save file");
        free(file_data);
        return -1;
    }
    
    fwrite(file_data, 1, file_size, fp);
    fclose(fp);
    free(file_data);
    
    printf("[FILE] Saved file to '%s'\n", temp_path);
    
    // Chuyển tiếp thông báo file đến receiver
    return relay_file(temp_path, msg->from, receiver);
}

/**
 * Chuyển tiếp file đến receiver
 */
int relay_file(const char *filepath, const char *from, const char *to) {
    if (filepath == NULL || from == NULL || to == NULL) return -1;
    
    int receiver_socket = find_user_socket(to);
    
    // Extract filename từ filepath
    const char *filename = strrchr(filepath, '/');
    if (filename == NULL) {
        filename = filepath;
    } else {
        filename++;  // Skip '/'
    }
    
    if (receiver_socket == -1) {
        // Receiver offline, lưu notification
        printf("[FILE] Receiver '%s' offline, saving notification\n", to);
        
        Message offline_msg;
        create_response_message(&offline_msg, MSG_FILE_SEND, from, to, filepath);
        strncpy(offline_msg.extra, filename, MAX_MESSAGE_LEN - 1);
        save_offline_message(&offline_msg);
        
        return 0;
    }
    
    // Đọc file
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("Failed to open file");
        return -1;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    uint32_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Đọc file data
    char *file_data = malloc(file_size);
    if (file_data == NULL) {
        fclose(fp);
        return -1;
    }
    
    fread(file_data, 1, file_size, fp);
    fclose(fp);
    
    // Gửi notification message
    Message file_msg;
    create_response_message(&file_msg, MSG_FILE_SEND, from, to, "");
    snprintf(file_msg.content, sizeof(file_msg.content), "%u", file_size);
    strncpy(file_msg.extra, filename, MAX_MESSAGE_LEN - 1);
    
    send_message_struct(receiver_socket, &file_msg);
    
    // Gửi file data
    send(receiver_socket, file_data, file_size, 0);
    
    free(file_data);
    
    printf("[FILE] Sent file '%s' to '%s'\n", filename, to);
    
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "File transfer: %s -> %s (%s)",
             from, to, filename);
    log_server_event("FILE_TRANSFER", log_msg);
    
    return 0;
}

/**
 * Xử lý chấp nhận nhận file
 */
int handle_file_accept(const Message *msg) {
    if (msg == NULL) return -1;
    
    printf("[FILE] User '%s' accepted file from '%s'\n", msg->from, msg->to);
    
    // Thông báo cho sender
    int sender_socket = find_user_socket(msg->to);
    if (sender_socket != -1) {
        Message response;
        create_response_message(&response, MSG_FILE_ACCEPT, msg->from, msg->to,
                               "File accepted");
        send_message_struct(sender_socket, &response);
    }
    
    return 0;
}

/**
 * Xử lý từ chối nhận file
 */
int handle_file_reject(const Message *msg) {
    if (msg == NULL) return -1;
    
    printf("[FILE] User '%s' rejected file from '%s'\n", msg->from, msg->to);
    
    // Thông báo cho sender
    int sender_socket = find_user_socket(msg->to);
    if (sender_socket != -1) {
        Message response;
        create_response_message(&response, MSG_FILE_REJECT, msg->from, msg->to,
                               "File rejected");
        send_message_struct(sender_socket, &response);
    }
    
    return 0;
}

// ===========================
// 9. FRIEND MANAGEMENT
// ===========================

/**
 * Load friendships từ file
 */
int load_friendships_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fp = fopen(filename, "w");
        if (fp != NULL) fclose(fp);
        return 0;
    }
    
    fclose(fp);
    printf("[SERVER] Loaded friendships from %s\n", filename);
    return 0;
}

/**
 * Save friendship vào file
 */
int save_friendship_to_file(const char *filename, const Friendship *friendship) {
    if (friendship == NULL) return -1;
    
    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
        perror("Failed to open friendships file");
        return -1;
    }
    
    fprintf(fp, "%s|%s|%d|%ld\n",
            friendship->user1,
            friendship->user2,
            friendship->status,
            friendship->created_at);
    
    fclose(fp);
    return 0;
}

/**
 * Gửi lời mời kết bạn
 */
// ===========================
// 10. UTILITY FUNCTIONS
// ===========================

/**
 * Broadcast message đến tất cả clients (trừ sender)
 */
int broadcast_message(const Message *msg, const char *exclude_username) {
    if (msg == NULL) return -1;
    
    mutex_lock(&server_state.clients_mutex);
    
    for (int i = 0; i < server_state.client_count; i++) {
        ClientConnection *client = &server_state.clients[i];
        
        if (client->is_authenticated && client->socket_fd > 0) {
            if (exclude_username == NULL || 
                strcmp(client->username, exclude_username) != 0) {
                send_message_struct(client->socket_fd, msg);
            }
        }
    }
    
    mutex_unlock(&server_state.clients_mutex);
    
    return 0;
}

/**
 * Tìm client connection theo username
 */
ClientConnection *find_client_by_username(const char *username) {
    if (username == NULL) return NULL;
    
    mutex_lock(&server_state.clients_mutex);
    
    for (int i = 0; i < server_state.client_count; i++) {
        if (server_state.clients[i].is_authenticated &&
            strcmp(server_state.clients[i].username, username) == 0) {
            ClientConnection *client = &server_state.clients[i];
            mutex_unlock(&server_state.clients_mutex);
            return client;
        }
    }
    
    mutex_unlock(&server_state.clients_mutex);
    return NULL;
}

/**
 * Tìm client connection theo socket fd
 */
ClientConnection *find_client_by_socket(int socket_fd) {
    mutex_lock(&server_state.clients_mutex);
    
    for (int i = 0; i < server_state.client_count; i++) {
        if (server_state.clients[i].socket_fd == socket_fd) {
            ClientConnection *client = &server_state.clients[i];
            mutex_unlock(&server_state.clients_mutex);
            return client;
        }
    }
    
    mutex_unlock(&server_state.clients_mutex);
    return NULL;
}

/**
 * Log server events
 */
void log_server_event(const char *event, const char *details) {
    FILE *fp = fopen("server.log", "a");
    if (fp == NULL) return;
    
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    
    fprintf(fp, "[%s] %s: %s\n", timestamp, event, details);
    fclose(fp);
}

/**
 * Log message
 */
void log_message(const Message *msg) {
    if (msg == NULL) return;
    
    FILE *fp = fopen("messages.log", "a");
    if (fp == NULL) return;
    
    fprintf(fp, "[%s] Type:%d From:%s To:%s Content:%s\n",
            msg->timestamp, msg->type, msg->from, msg->to, msg->content);
    
    fclose(fp);
}
