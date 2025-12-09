#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// ===========================
// LINUX SOCKET TYPES
// ===========================

typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

// ===========================
// CONSTANTS - Dùng chung cho Client & Server
// ===========================

#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 64
#define MAX_MESSAGE_LEN 2048
#define MAX_GROUP_NAME_LEN 100
#define MAX_FILENAME_LEN 256
#define MAX_CLIENTS 100
#define MAX_GROUPS 50
#define MAX_GROUP_MEMBERS 20
#define PORT 8888
#define BUFFER_SIZE 4096

// ===========================
// MESSAGE TYPES - Các loại message
// ===========================

typedef enum {
    // Authentication
    MSG_REGISTER = 1,
    MSG_LOGIN = 2,
    MSG_LOGOUT = 3,
    
    // Status
    MSG_SUCCESS = 10,
    MSG_ERROR = 11,
    MSG_USER_ONLINE = 12,
    MSG_USER_OFFLINE = 13,
    
    // Response messages (for client)
    MSG_RESPONSE_SUCCESS = 14,
    MSG_RESPONSE_ERROR = 15,
    MSG_NOTIFICATION = 16,
    MSG_ONLINE_USERS_LIST = 17,
    
    // Friends
    MSG_FRIEND_REQUEST = 20,
    MSG_FRIEND_ACCEPT = 21,
    MSG_FRIEND_REJECT = 22,
    MSG_FRIEND_REMOVE = 23,
    MSG_FRIEND_LIST = 24,
    
    // Private Chat
    MSG_PRIVATE_MESSAGE = 30,
    MSG_PRIVATE_CHAT_START = 31,
    MSG_PRIVATE_CHAT_END = 32,
    MSG_USER_TYPING = 33,
    
    // Group Chat
    MSG_GROUP_CREATE = 40,
    MSG_GROUP_INVITE = 41,
    MSG_GROUP_JOIN = 42,
    MSG_GROUP_LEAVE = 43,
    MSG_GROUP_MESSAGE = 44,
    MSG_GROUP_LIST = 45,
    
    // Offline Messages
    MSG_OFFLINE_SYNC = 50,
    MSG_OFFLINE_COUNT = 51,
    
    // File Transfer
    MSG_FILE_SEND = 60,
    MSG_FILE_RECEIVE = 61,
    MSG_FILE_ACCEPT = 62,
    MSG_FILE_REJECT = 63,
    MSG_FILE_TRANSFER = 64,
    
    // List requests
    MSG_GET_ONLINE_USERS = 70,
    MSG_GET_FRIENDS = 71,
    MSG_GET_GROUPS = 72
} MessageType;

// ===========================
// MESSAGE STRUCTURE
// Format: TYPE|FROM:username|TO:recipient|CONTENT:text|TIME:timestamp|[EXTRA:data]
// ===========================

typedef struct {
    MessageType type;
    char from[MAX_USERNAME_LEN];
    char to[MAX_USERNAME_LEN];
    char content[MAX_MESSAGE_LEN];
    char timestamp[32];
    char extra[MAX_MESSAGE_LEN];  // For additional data (group name, file name, etc.)
} Message;

// ===========================
// USER STRUCTURE
// ===========================

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int is_online;
    socket_t socket_fd;
    time_t last_seen;
} User;

// ===========================
// GROUP STRUCTURE
// ===========================

typedef struct {
    char group_name[MAX_GROUP_NAME_LEN];
    char creator[MAX_USERNAME_LEN];
    char members[MAX_GROUP_MEMBERS][MAX_USERNAME_LEN];
    int member_count;
    time_t created_at;
} Group;

// ===========================
// FRIEND RELATIONSHIP
// ===========================

typedef struct {
    char user1[MAX_USERNAME_LEN];
    char user2[MAX_USERNAME_LEN];
    int status;  // 0: pending, 1: accepted
    time_t created_at;
} Friendship;

// ===========================
// FILE TRANSFER STRUCTURE
// ===========================

typedef struct {
    char filename[MAX_FILENAME_LEN];
    char sender[MAX_USERNAME_LEN];
    char receiver[MAX_USERNAME_LEN];
    uint32_t file_size;
    uint8_t *file_data;
} FileTransfer;

// ===========================
// PROTOCOL FUNCTIONS - Dùng chung
// ===========================

/**
 * Initialize network (Windows WSA startup)
 * MUST call this before any socket operations on Windows
 */
int init_network(void);

/**
 * Cleanup network (Windows WSA cleanup)
 */
void cleanup_network(void);

/**
 * Parse raw message string thành struct Message
 * Format: TYPE|FROM:user|TO:recipient|CONTENT:text|TIME:timestamp
 * Return: 0 nếu thành công, -1 nếu lỗi
 */
int parse_message(const char *raw, Message *msg);

/**
 * Serialize struct Message thành raw string
 * Return: số bytes written, -1 nếu lỗi
 */
int serialize_message(const Message *msg, char *buffer, size_t buffer_size);

/**
 * Tạo timestamp hiện tại
 */
void get_current_timestamp(char *timestamp, size_t size);

/**
 * Tạo message response đơn giản
 */
void create_response_message(Message *msg, MessageType type, const char *from, 
                             const char *to, const char *content);

/**
 * Cross-platform sleep function
 */
void sleep_ms(int milliseconds);

#endif // PROTOCOL_H
