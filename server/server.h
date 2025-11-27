#ifndef SERVER_H 
#define SERVER_H 
 
#include "../client/protocol.h" 
#include <stdbool.h> 
#include <windows.h> 
#include <process.h> 

// ===========================
// WINDOWS THREAD & MUTEX TYPES
// ===========================

typedef HANDLE thread_t; 
typedef CRITICAL_SECTION mutex_t; 

#define THREAD_RETURN unsigned int __stdcall 
#define THREAD_RETURN_VALUE return 0 
#define mutex_init(m, attr) InitializeCriticalSection(m) 
#define mutex_destroy(m) DeleteCriticalSection(m) 
#define mutex_lock(m) EnterCriticalSection(m) 
#define mutex_unlock(m) LeaveCriticalSection(m) 

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

#endif
