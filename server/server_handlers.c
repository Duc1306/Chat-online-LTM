#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

// ===========================
// 4. ONLINE USER MANAGEMENT
// ===========================

/**
 * Thêm user vào danh sách online
 */
int add_online_user(const char *username, int socket_fd) {
    if (username == NULL) return -1;
    
    mutex_lock(&server_state.users_mutex);
    
    for (int i = 0; i < server_state.user_count; i++) {
        if (strcmp(server_state.users[i].username, username) == 0) {
            server_state.users[i].is_online = 1;
            server_state.users[i].socket_fd = socket_fd;
            server_state.users[i].last_seen = time(NULL);
            
            mutex_unlock(&server_state.users_mutex);
            
            printf("[ONLINE] User '%s' is now online (socket: %d)\n", username, socket_fd);
            return 0;
        }
    }
    
    mutex_unlock(&server_state.users_mutex);
    return -1;
}

/**
 * Xóa user khỏi danh sách online
 */
int remove_online_user(const char *username) {
    if (username == NULL) return -1;
    
    mutex_lock(&server_state.users_mutex);
    
    for (int i = 0; i < server_state.user_count; i++) {
        if (strcmp(server_state.users[i].username, username) == 0) {
            server_state.users[i].is_online = 0;
            server_state.users[i].socket_fd = -1;
            server_state.users[i].last_seen = time(NULL);
            
            mutex_unlock(&server_state.users_mutex);
            
            printf("[OFFLINE] User '%s' is now offline\n", username);
            return 0;
        }
    }
    
    mutex_unlock(&server_state.users_mutex);
    return -1;
}

/**
 * Tìm socket fd của user theo username
 */
int find_user_socket(const char *username) {
    if (username == NULL) return -1;
    
    mutex_lock(&server_state.users_mutex);
    
    printf("[DEBUG] find_user_socket: Looking for '%s' in %d users\n", 
           username, server_state.user_count);
    
    for (int i = 0; i < server_state.user_count; i++) {
        printf("[DEBUG]   User[%d]: '%s', online=%d, socket=%d\n", 
               i, server_state.users[i].username, 
               server_state.users[i].is_online, 
               server_state.users[i].socket_fd);
               
        if (strcmp(server_state.users[i].username, username) == 0 &&
            server_state.users[i].is_online) {
            int sock = server_state.users[i].socket_fd;
            mutex_unlock(&server_state.users_mutex);
            return sock;
        }
    }
    
    mutex_unlock(&server_state.users_mutex);
    return -1;
}

/**
 * Alias cho find_user_socket
 */
int find_client_socket(const char *username) {
    return find_user_socket(username);
}

/**
 * Kiểm tra user có online không
 */
bool is_user_online(const char *username) {
    return find_user_socket(username) != -1;
}

/**
 * Gửi danh sách online users cho client
 */
int send_online_users_list(int client_socket) {
    char user_list[BUFFER_SIZE] = "";
    int count = 0;
    
    mutex_lock(&server_state.users_mutex);
    
    for (int i = 0; i < server_state.user_count; i++) {
        if (server_state.users[i].is_online) {
            if (count > 0) {
                strcat(user_list, ",");
            }
            strcat(user_list, server_state.users[i].username);
            count++;
        }
    }
    
    mutex_unlock(&server_state.users_mutex);
    
    Message response;
    create_response_message(&response, MSG_GET_ONLINE_USERS, "SERVER", "", user_list);
    
    return send_message_struct(client_socket, &response);
}

// ===========================
// 5. PRIVATE CHAT 1-1
// ===========================

/**
 * Chuyển tiếp tin nhắn riêng tư
 */
int relay_private_message(const Message *msg) {
    if (msg == NULL) return -1;
    
    printf("[PRIVATE] From '%s' to '%s': %s\n", msg->from, msg->to, msg->content);
    
    // Tìm receiver socket
    int receiver_socket = find_user_socket(msg->to);
    
    printf("[DEBUG] find_user_socket('%s') returned: %d\n", msg->to, receiver_socket);
    
    if (receiver_socket == -1) {
        // Receiver offline, lưu tin nhắn
        printf("[PRIVATE] User '%s' is offline, saving message\n", msg->to);
        save_offline_message(msg);
        
        // Thông báo cho sender
        Message response;
        create_response_message(&response, MSG_ERROR, "SERVER", msg->from, 
                               "User is offline. Message saved.");
        int sender_socket = find_user_socket(msg->from);
        if (sender_socket != -1) {
            send_message_struct(sender_socket, &response);
        }
        
        return 0;
    }
    
    // Forward message đến receiver
    int result = send_message_struct(receiver_socket, msg);
    
    if (result < 0) {
        printf("[ERROR] Failed to send message to '%s'\n", msg->to);
        return -1;
    }
    
    // Log message
    log_message(msg);
    
    return 0;
}

/**
 * Thông báo bắt đầu cuộc trò chuyện
 */
int handle_private_chat_start(const Message *msg) {
    if (msg == NULL) return -1;
    
    printf("[CHAT] User '%s' started chat with '%s'\n", msg->from, msg->to);
    
    int receiver_socket = find_user_socket(msg->to);
    if (receiver_socket != -1) {
        send_message_struct(receiver_socket, msg);
    }
    
    return 0;
}

/**
 * Thông báo kết thúc cuộc trò chuyện
 */
int handle_private_chat_end(const Message *msg) {
    if (msg == NULL) return -1;
    
    printf("[CHAT] User '%s' ended chat with '%s'\n", msg->from, msg->to);
    
    int receiver_socket = find_user_socket(msg->to);
    if (receiver_socket != -1) {
        send_message_struct(receiver_socket, msg);
    }
    
    return 0;
}

// ===========================
// 6. GROUP CHAT
// ===========================

/**
 * Tạo nhóm chat mới
 */
int create_group(const char *group_name, const char *creator) {
    if (group_name == NULL || creator == NULL) return -1;
    
    mutex_lock(&server_state.groups_mutex);
    
    // Kiểm tra nhóm đã tồn tại chưa
    for (int i = 0; i < server_state.group_count; i++) {
        if (strcmp(server_state.groups[i].group_name, group_name) == 0) {
            mutex_unlock(&server_state.groups_mutex);
            printf("[GROUP] Group '%s' already exists\n", group_name);
            return -1;
        }
    }
    
    // Kiểm tra số lượng nhóm
    if (server_state.group_count >= MAX_GROUPS) {
        mutex_unlock(&server_state.groups_mutex);
        printf("[GROUP] Maximum groups reached\n");
        return -1;
    }
    
    // Tạo nhóm mới
    Group *new_group = &server_state.groups[server_state.group_count];
    strncpy(new_group->group_name, group_name, MAX_GROUP_NAME_LEN - 1);
    strncpy(new_group->creator, creator, MAX_USERNAME_LEN - 1);
    strncpy(new_group->members[0], creator, MAX_USERNAME_LEN - 1);
    new_group->member_count = 1;
    new_group->created_at = time(NULL);
    
    server_state.group_count++;
    
    mutex_unlock(&server_state.groups_mutex);
    
    // Lưu vào file
    save_group_to_file("groups.txt", new_group);
    
    printf("[GROUP] Created group '%s' by '%s'\n", group_name, creator);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Group created: %s by %s", group_name, creator);
    log_server_event("GROUP_CREATE", log_msg);
    
    return 0;
}

/**
 * Xử lý lời mời tham gia nhóm
 */
int handle_group_invite(const Message *msg) {
    if (msg == NULL) return -1;
    
    // msg->extra chứa group_name
    // msg->to chứa username được mời
    
    printf("[GROUP] User '%s' invited '%s' to group '%s'\n", 
           msg->from, msg->to, msg->extra);
    
    int receiver_socket = find_user_socket(msg->to);
    if (receiver_socket != -1) {
        send_message_struct(receiver_socket, msg);
    } else {
        save_offline_message(msg);
    }
    
    return 0;
}

/**
 * Xử lý tham gia nhóm
 */
int handle_group_join(const char *group_name, const char *username) {
    if (group_name == NULL || username == NULL) return -1;
    
    mutex_lock(&server_state.groups_mutex);
    
    // Tìm nhóm
    Group *group = NULL;
    for (int i = 0; i < server_state.group_count; i++) {
        if (strcmp(server_state.groups[i].group_name, group_name) == 0) {
            group = &server_state.groups[i];
            break;
        }
    }
    
    if (group == NULL) {
        mutex_unlock(&server_state.groups_mutex);
        printf("[GROUP] Group '%s' not found\n", group_name);
        return -1;
    }
    
    // Kiểm tra user đã là member chưa
    for (int i = 0; i < group->member_count; i++) {
        if (strcmp(group->members[i], username) == 0) {
            mutex_unlock(&server_state.groups_mutex);
            printf("[GROUP] User '%s' already in group '%s'\n", username, group_name);
            return -1;
        }
    }
    
    // Thêm member
    if (group->member_count >= MAX_GROUP_MEMBERS) {
        mutex_unlock(&server_state.groups_mutex);
        printf("[GROUP] Group '%s' is full\n", group_name);
        return -1;
    }
    
    strncpy(group->members[group->member_count], username, MAX_USERNAME_LEN - 1);
    group->member_count++;
    
    mutex_unlock(&server_state.groups_mutex);
    
    printf("[GROUP] User '%s' joined group '%s'\n", username, group_name);
    
    // Thông báo cho các members khác
    Message notification;
    create_response_message(&notification, MSG_GROUP_JOIN, "SERVER", "", username);
    strncpy(notification.extra, group_name, MAX_MESSAGE_LEN - 1);
    
    mutex_lock(&server_state.groups_mutex);
    for (int i = 0; i < group->member_count; i++) {
        if (strcmp(group->members[i], username) != 0) {
            int member_socket = find_user_socket(group->members[i]);
            if (member_socket != -1) {
                send_message_struct(member_socket, &notification);
            }
        }
    }
    mutex_unlock(&server_state.groups_mutex);
    
    return 0;
}

/**
 * Xử lý rời nhóm
 */
int handle_group_leave(const char *group_name, const char *username) {
    if (group_name == NULL || username == NULL) return -1;
    
    mutex_lock(&server_state.groups_mutex);
    
    // Tìm nhóm
    Group *group = NULL;
    for (int i = 0; i < server_state.group_count; i++) {
        if (strcmp(server_state.groups[i].group_name, group_name) == 0) {
            group = &server_state.groups[i];
            break;
        }
    }
    
    if (group == NULL) {
        mutex_unlock(&server_state.groups_mutex);
        return -1;
    }
    
    // Xóa member
    int found = -1;
    for (int i = 0; i < group->member_count; i++) {
        if (strcmp(group->members[i], username) == 0) {
            found = i;
            break;
        }
    }
    
    if (found == -1) {
        mutex_unlock(&server_state.groups_mutex);
        return -1;
    }
    
    // Shift members
    for (int i = found; i < group->member_count - 1; i++) {
        strcpy(group->members[i], group->members[i + 1]);
    }
    group->member_count--;
    
    mutex_unlock(&server_state.groups_mutex);
    
    printf("[GROUP] User '%s' left group '%s'\n", username, group_name);
    
    // Thông báo cho các members còn lại
    Message notification;
    create_response_message(&notification, MSG_GROUP_LEAVE, "SERVER", "", username);
    strncpy(notification.extra, group_name, MAX_MESSAGE_LEN - 1);
    
    mutex_lock(&server_state.groups_mutex);
    for (int i = 0; i < group->member_count; i++) {
        int member_socket = find_user_socket(group->members[i]);
        if (member_socket != -1) {
            send_message_struct(member_socket, &notification);
        }
    }
    mutex_unlock(&server_state.groups_mutex);
    
    return 0;
}

/**
 * Chuyển tiếp tin nhắn nhóm
 */
int relay_group_message(const Message *msg) {
    if (msg == NULL) return -1;
    
    // msg->to chứa group_name
    const char *group_name = msg->to;
    
    printf("[GROUP] From '%s' to group '%s': %s\n", 
           msg->from, group_name, msg->content);
    
    mutex_lock(&server_state.groups_mutex);
    
    // Tìm nhóm
    Group *group = NULL;
    for (int i = 0; i < server_state.group_count; i++) {
        if (strcmp(server_state.groups[i].group_name, group_name) == 0) {
            group = &server_state.groups[i];
            break;
        }
    }
    
    if (group == NULL) {
        mutex_unlock(&server_state.groups_mutex);
        printf("[GROUP] Group '%s' not found\n", group_name);
        return -1;
    }
    
    // Forward đến tất cả members (trừ sender)
    for (int i = 0; i < group->member_count; i++) {
        if (strcmp(group->members[i], msg->from) == 0) {
            continue;  // Skip sender
        }
        
        int member_socket = find_user_socket(group->members[i]);
        if (member_socket != -1) {
            send_message_struct(member_socket, msg);
        } else {
            // Member offline, save message
            save_offline_message(msg);
        }
    }
    
    mutex_unlock(&server_state.groups_mutex);
    
    // Log message
    log_message(msg);
    
    return 0;
}

/**
 * Load groups từ file
 */
int load_groups_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fp = fopen(filename, "w");
        if (fp != NULL) fclose(fp);
        return 0;
    }
    
    mutex_lock(&server_state.groups_mutex);
    
    server_state.group_count = 0;
    char line[1024];
    
    while (fgets(line, sizeof(line), fp) != NULL && 
           server_state.group_count < MAX_GROUPS) {
        Group *group = &server_state.groups[server_state.group_count];
        
        // Parse: group_name|creator|member1,member2,...|created_at
        char *group_name = strtok(line, "|");
        char *creator = strtok(NULL, "|");
        char *members_str = strtok(NULL, "|");
        char *created_str = strtok(NULL, "|\n");
        
        if (group_name != NULL && creator != NULL && members_str != NULL) {
            strncpy(group->group_name, group_name, MAX_GROUP_NAME_LEN - 1);
            strncpy(group->creator, creator, MAX_USERNAME_LEN - 1);
            
            // Parse members
            group->member_count = 0;
            char *member = strtok(members_str, ",");
            while (member != NULL && group->member_count < MAX_GROUP_MEMBERS) {
                strncpy(group->members[group->member_count], member, MAX_USERNAME_LEN - 1);
                group->member_count++;
                member = strtok(NULL, ",");
            }
            
            if (created_str != NULL) {
                group->created_at = (time_t)atoll(created_str);
            } else {
                group->created_at = time(NULL);
            }
            
            server_state.group_count++;
        }
    }
    
    mutex_unlock(&server_state.groups_mutex);
    fclose(fp);
    
    printf("[SERVER] Loaded %d groups from %s\n", server_state.group_count, filename);
    return server_state.group_count;
}

/**
 * Save group vào file
 */
int save_group_to_file(const char *filename, const Group *group) {
    if (group == NULL) return -1;
    
    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
        perror("Failed to open groups file");
        return -1;
    }
    
    fprintf(fp, "%s|%s|", group->group_name, group->creator);
    
    for (int i = 0; i < group->member_count; i++) {
        fprintf(fp, "%s", group->members[i]);
        if (i < group->member_count - 1) {
            fprintf(fp, ",");
        }
    }
    
    fprintf(fp, "|%ld\n", group->created_at);
    fclose(fp);
    
    return 0;
}

/**
 * Gửi danh sách groups của user
 */
int send_user_groups_list(int client_socket, const char *username) {
    char group_list[BUFFER_SIZE] = "";
    int count = 0;
    
    mutex_lock(&server_state.groups_mutex);
    
    for (int i = 0; i < server_state.group_count; i++) {
        // Kiểm tra user có phải member không
        for (int j = 0; j < server_state.groups[i].member_count; j++) {
            if (strcmp(server_state.groups[i].members[j], username) == 0) {
                if (count > 0) {
                    strcat(group_list, ",");
                }
                strcat(group_list, server_state.groups[i].group_name);
                count++;
                break;
            }
        }
    }
    
    mutex_unlock(&server_state.groups_mutex);
    
    Message response;
    create_response_message(&response, MSG_GROUP_LIST, "SERVER", username, group_list);
    
    return send_message_struct(client_socket, &response);
}

// ===========================
// 8. FRIEND MANAGEMENT
// ===========================

/**
 * Xử lý lời mời kết bạn
 */
void handle_friend_request(Message *msg) {
    if (msg == NULL) return;
    
    const char *from_user = msg->from;
    const char *to_user = msg->to;
    
    printf("[FRIEND_REQUEST] %s wants to add %s as friend\n", from_user, to_user);
    
    // Kiểm tra user tồn tại
    mutex_lock(&server_state.users_mutex);
    int to_exists = 0;
    for (int i = 0; i < server_state.user_count; i++) {
        if (strcmp(server_state.users[i].username, to_user) == 0) {
            to_exists = 1;
            break;
        }
    }
    mutex_unlock(&server_state.users_mutex);
    
    if (!to_exists) {
        Message error_msg;
        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "User not found");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &error_msg);
        }
        return;
    }
    
    // Kiểm tra đã là bạn chưa
    mutex_lock(&server_state.file_mutex);
    FILE *fp = fopen("friendships.txt", "r");
    if (fp != NULL) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            char user1[MAX_USERNAME_LEN], user2[MAX_USERNAME_LEN], status[20];
            if (sscanf(line, "%[^|]|%[^|]|%s", user1, user2, status) == 3) {
                if ((strcmp(user1, from_user) == 0 && strcmp(user2, to_user) == 0) ||
                    (strcmp(user1, to_user) == 0 && strcmp(user2, from_user) == 0)) {
                    fclose(fp);
                    mutex_unlock(&server_state.file_mutex);
                    
                    Message error_msg;
                    if (strcmp(status, "accepted") == 0) {
                        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Already friends");
                    } else {
                        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Friend request already sent");
                    }
                    
                    int from_socket = find_client_socket(from_user);
                    if (from_socket >= 0) {
                        send_message_struct(from_socket, &error_msg);
                    }
                    return;
                }
            }
        }
        fclose(fp);
    }
    
    // Lưu lời mời kết bạn vào file
    fp = fopen("friendships.txt", "a");
    if (fp != NULL) {
        fprintf(fp, "%s|%s|pending\n", from_user, to_user);
        fclose(fp);
    }
    mutex_unlock(&server_state.file_mutex);
    
    // Gửi notification cho người nhận
    Message notify_msg;
    char notify_content[MAX_MESSAGE_LEN];
    snprintf(notify_content, sizeof(notify_content), "Friend request from %s", from_user);
    create_response_message(&notify_msg, MSG_FRIEND_REQUEST, from_user, to_user, notify_content);
    
    int to_socket = find_client_socket(to_user);
    if (to_socket >= 0) {
        send_message_struct(to_socket, &notify_msg);
    }
    
    // Gửi success cho người gửi
    Message success_msg;
    create_response_message(&success_msg, MSG_RESPONSE_SUCCESS, "SERVER", from_user, "Friend request sent");
    int from_socket = find_client_socket(from_user);
    if (from_socket >= 0) {
        send_message_struct(from_socket, &success_msg);
    }
}

/**
 * Xử lý chấp nhận kết bạn
 */
void handle_friend_accept(Message *msg) {
    if (msg == NULL) return;
    
    const char *from_user = msg->from;  // Người chấp nhận
    const char *to_user = msg->to;      // Người gửi lời mời
    
    printf("[FRIEND_ACCEPT] %s accepted friend request from %s\n", from_user, to_user);
    
    // Đọc file và update status
    mutex_lock(&server_state.file_mutex);
    
    FILE *fp = fopen("friendships.txt", "r");
    FILE *temp = fopen("friendships_temp.txt", "w");
    
    if (fp == NULL || temp == NULL) {
        if (fp) fclose(fp);
        if (temp) fclose(temp);
        mutex_unlock(&server_state.file_mutex);
        
        Message error_msg;
        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Failed to accept friend request");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &error_msg);
        }
        return;
    }
    
    char line[512];
    int found = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        char user1[MAX_USERNAME_LEN], user2[MAX_USERNAME_LEN], status[20];
        if (sscanf(line, "%[^|]|%[^|]|%s", user1, user2, status) == 3) {
            if ((strcmp(user1, to_user) == 0 && strcmp(user2, from_user) == 0) && 
                strcmp(status, "pending") == 0) {
                // Update status thành accepted
                fprintf(temp, "%s|%s|accepted\n", user1, user2);
                found = 1;
            } else {
                fputs(line, temp);
            }
        }
    }
    
    fclose(fp);
    fclose(temp);
    
    if (found) {
        remove("friendships.txt");
        rename("friendships_temp.txt", "friendships.txt");
        
        // Thông báo cho cả 2 người
        Message notify_msg;
        char notify_content[MAX_MESSAGE_LEN];
        
        // Thông báo cho người gửi lời mời
        snprintf(notify_content, sizeof(notify_content), "%s accepted your friend request", from_user);
        create_response_message(&notify_msg, MSG_FRIEND_ACCEPT, from_user, to_user, notify_content);
        int to_socket = find_client_socket(to_user);
        if (to_socket >= 0) {
            send_message_struct(to_socket, &notify_msg);
        }
        
        // Success cho người chấp nhận
        create_response_message(&notify_msg, MSG_RESPONSE_SUCCESS, "SERVER", from_user, "Friend request accepted");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &notify_msg);
        }
    } else {
        remove("friendships_temp.txt");
        
        Message error_msg;
        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Friend request not found");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &error_msg);
        }
    }
    
    mutex_unlock(&server_state.file_mutex);
}

/**
 * Xử lý từ chối kết bạn
 */
void handle_friend_reject(Message *msg) {
    if (msg == NULL) return;
    
    const char *from_user = msg->from;  // Người từ chối
    const char *to_user = msg->to;      // Người gửi lời mời
    
    printf("[FRIEND_REJECT] %s rejected friend request from %s\n", from_user, to_user);
    
    // Xóa lời mời khỏi file
    mutex_lock(&server_state.file_mutex);
    
    FILE *fp = fopen("friendships.txt", "r");
    FILE *temp = fopen("friendships_temp.txt", "w");
    
    if (fp == NULL || temp == NULL) {
        if (fp) fclose(fp);
        if (temp) fclose(temp);
        mutex_unlock(&server_state.file_mutex);
        
        Message error_msg;
        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Failed to reject friend request");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &error_msg);
        }
        return;
    }
    
    char line[512];
    int found = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        char user1[MAX_USERNAME_LEN], user2[MAX_USERNAME_LEN], status[20];
        if (sscanf(line, "%[^|]|%[^|]|%s", user1, user2, status) == 3) {
            if ((strcmp(user1, to_user) == 0 && strcmp(user2, from_user) == 0) && 
                strcmp(status, "pending") == 0) {
                // Không ghi dòng này vào temp (xóa)
                found = 1;
            } else {
                fputs(line, temp);
            }
        }
    }
    
    fclose(fp);
    fclose(temp);
    
    if (found) {
        remove("friendships.txt");
        rename("friendships_temp.txt", "friendships.txt");
        
        // Thông báo cho người gửi lời mời
        Message notify_msg;
        char notify_content[MAX_MESSAGE_LEN];
        snprintf(notify_content, sizeof(notify_content), "%s rejected your friend request", from_user);
        create_response_message(&notify_msg, MSG_FRIEND_REJECT, from_user, to_user, notify_content);
        
        int to_socket = find_client_socket(to_user);
        if (to_socket >= 0) {
            send_message_struct(to_socket, &notify_msg);
        }
        
        // Success cho người từ chối
        create_response_message(&notify_msg, MSG_RESPONSE_SUCCESS, "SERVER", from_user, "Friend request rejected");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &notify_msg);
        }
    } else {
        remove("friendships_temp.txt");
        
        Message error_msg;
        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Friend request not found");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &error_msg);
        }
    }
    
    mutex_unlock(&server_state.file_mutex);
}

/**
 * Xử lý xóa bạn
 */
void handle_friend_remove(Message *msg) {
    if (msg == NULL) return;
    
    const char *from_user = msg->from;
    const char *to_user = msg->to;
    
    printf("[FRIEND_REMOVE] %s wants to remove friend %s\n", from_user, to_user);
    
    // Xóa friendship khỏi file
    mutex_lock(&server_state.file_mutex);
    
    FILE *fp = fopen("friendships.txt", "r");
    FILE *temp = fopen("friendships_temp.txt", "w");
    
    if (fp == NULL || temp == NULL) {
        if (fp) fclose(fp);
        if (temp) fclose(temp);
        mutex_unlock(&server_state.file_mutex);
        
        Message error_msg;
        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Failed to remove friend");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &error_msg);
        }
        return;
    }
    
    char line[512];
    int found = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        char user1[MAX_USERNAME_LEN], user2[MAX_USERNAME_LEN], status[20];
        if (sscanf(line, "%[^|]|%[^|]|%s", user1, user2, status) == 3) {
            if (((strcmp(user1, from_user) == 0 && strcmp(user2, to_user) == 0) ||
                 (strcmp(user1, to_user) == 0 && strcmp(user2, from_user) == 0)) &&
                strcmp(status, "accepted") == 0) {
                // Không ghi dòng này vào temp (xóa)
                found = 1;
            } else {
                fputs(line, temp);
            }
        }
    }
    
    fclose(fp);
    fclose(temp);
    
    if (found) {
        remove("friendships.txt");
        rename("friendships_temp.txt", "friendships.txt");
        
        // Thông báo cho bạn bị xóa
        Message notify_msg;
        char notify_content[MAX_MESSAGE_LEN];
        snprintf(notify_content, sizeof(notify_content), "%s removed you from their friend list", from_user);
        create_response_message(&notify_msg, MSG_FRIEND_REMOVE, from_user, to_user, notify_content);
        
        int to_socket = find_client_socket(to_user);
        if (to_socket >= 0) {
            send_message_struct(to_socket, &notify_msg);
        }
        
        // Success cho người xóa
        create_response_message(&notify_msg, MSG_RESPONSE_SUCCESS, "SERVER", from_user, "Friend removed");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &notify_msg);
        }
    } else {
        remove("friendships_temp.txt");
        
        Message error_msg;
        create_response_message(&error_msg, MSG_RESPONSE_ERROR, "SERVER", from_user, "Friendship not found");
        int from_socket = find_client_socket(from_user);
        if (from_socket >= 0) {
            send_message_struct(from_socket, &error_msg);
        }
    }
    
    mutex_unlock(&server_state.file_mutex);
}

/**
 * Gửi danh sách bạn bè
 */
int send_friends_list(int client_socket, const char *username) {
    if (username == NULL) return -1;
    
    char friends_list[BUFFER_SIZE] = "";
    int count = 0;
    
    mutex_lock(&server_state.file_mutex);
    
    FILE *fp = fopen("friendships.txt", "r");
    if (fp != NULL) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            char user1[MAX_USERNAME_LEN], user2[MAX_USERNAME_LEN], status[20];
            if (sscanf(line, "%[^|]|%[^|]|%s", user1, user2, status) == 3) {
                if (strcmp(status, "accepted") == 0) {
                    const char *friend_name = NULL;
                    if (strcmp(user1, username) == 0) {
                        friend_name = user2;
                    } else if (strcmp(user2, username) == 0) {
                        friend_name = user1;
                    }
                    
                    if (friend_name != NULL) {
                        if (count > 0) {
                            strcat(friends_list, ",");
                        }
                        strcat(friends_list, friend_name);
                        count++;
                    }
                }
            }
        }
        fclose(fp);
    }
    
    mutex_unlock(&server_state.file_mutex);
    
    Message response;
    create_response_message(&response, MSG_FRIEND_LIST, "SERVER", username, friends_list);
    
    return send_message_struct(client_socket, &response);
}




