#ifndef SERVER_H 
#define SERVER_H 
 
#include "../client/protocol.h" 
#include <stdbool.h> 
#include <pthread.h>

// ===========================
// LINUX THREAD & MUTEX TYPES
// ===========================

typedef pthread_t thread_t; 
typedef pthread_mutex_t mutex_t; 

#define THREAD_RETURN void*
#define THREAD_RETURN_VALUE return NULL
#define mutex_init(m, attr) pthread_mutex_init(m, attr) 
#define mutex_destroy(m) pthread_mutex_destroy(m) 
#define mutex_lock(m) pthread_mutex_lock(m) 
#define mutex_unlock(m) pthread_mutex_unlock(m) 

typedef struct {
    socket_t socket_fd;
    char username[MAX_USERNAME_LEN];
    bool is_authenticated;
    thread_t thread_id;
    struct sockaddr_in address;
} ClientConnection;

typedef struct {
    socket_t server_socket;
    ClientConnection clients[MAX_CLIENTS];
    int client_count;
    User users[MAX_CLIENTS];
    int user_count;
    Group groups[MAX_GROUPS];
    int group_count;
    mutex_t clients_mutex;
    mutex_t users_mutex;
    mutex_t groups_mutex;
    mutex_t file_mutex;
} ServerState;

typedef struct {
    char to[MAX_USERNAME_LEN];
    Message msg;
} OfflineMessage;

extern ServerState server_state;

// ===========================
// FUNCTION DECLARATIONS
// ===========================

// Server core functions
int init_server_socket(int port);
int accept_client(int server_socket, ClientConnection *client);
int recv_message(int socket_fd, char *buffer, size_t buffer_size);
int send_message(int socket_fd, const char *message, size_t length);
int send_message_struct(int socket_fd, const Message *msg);
THREAD_RETURN client_thread(void *arg);
void cleanup_client(ClientConnection *client);
void cleanup_server(void);

// User management
int load_users_from_file(const char *filename);
int save_user_to_file(const char *filename, const User *user);
int find_user_socket(const char *username);
int add_online_user(const char *username, int socket_fd);
int remove_online_user(const char *username);
int send_online_users_list(int client_socket);
void send_friend_list(const char *username);

// Message handling
int broadcast_message(const Message *msg, const char *exclude_user);
int relay_private_message(const Message *msg);
int handle_private_chat_start(const Message *msg);
int handle_private_chat_end(const Message *msg);
void log_message(const Message *msg);

// Friend management
void handle_friend_request(Message *msg);
void handle_friend_accept(Message *msg);
void handle_friend_reject(Message *msg);
void handle_friend_remove(Message *msg);
int send_friends_list(int client_socket, const char *username);
int load_friendships_from_file(const char *filename);

// Group management
int create_group(const char *group_name, const char *creator);
int handle_group_invite(const Message *msg);
int handle_group_join(const char *group_name, const char *username);
int handle_group_leave(const char *group_name, const char *username);
int relay_group_message(const Message *msg);
int send_user_groups_list(int client_socket, const char *username);
int load_groups_from_file(const char *filename);
int save_group_to_file(const char *filename, const Group *group);

// Offline messages
int save_offline_message(const Message *msg);
int send_offline_messages(int socket_fd, const char *username);

// File transfer
int handle_file_transfer(ClientConnection *client, const Message *msg);
int handle_file_accept(const Message *msg);
int handle_file_reject(const Message *msg);
int relay_file(const char *file_path, const char *from, const char *to);

// Logging
void log_server_event(const char *event, const char *details);

#endif
