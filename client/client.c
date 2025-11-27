/*
 * ChatOnline Client - Full-Featured Windows Client
 * 
 * === FEATURES ===
 * - Register & Login
 * - Private Chat (1-1)
 * - Group Chat
 * - Friend Management
 * - Online Users List
 * - Offline Messages Sync
 * - File Transfer
 * 
 * === COMPILE ===
 * gcc -o client.exe client.c protocol.c -lws2_32 -Wall -Wextra
 * 
 * === RUN ===
 * client.exe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#include "protocol.h"

// ===========================
// TYPE DEFINITIONS
// ===========================

typedef SOCKET socket_t;
typedef HANDLE thread_t;

#define close closesocket
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define THREAD_RETURN unsigned int __stdcall

// ===========================
// GLOBAL VARIABLES
// ===========================

socket_t server_socket = INVALID_SOCKET_VALUE;
char current_username[MAX_USERNAME_LEN] = "";
bool is_running = true;
bool is_logged_in = false;

// ===========================
// CONSOLE COLORS
// ===========================

void set_color(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

#define COLOR_RESET 7
#define COLOR_RED 12
#define COLOR_GREEN 10
#define COLOR_YELLOW 14
#define COLOR_BLUE 9
#define COLOR_MAGENTA 13
#define COLOR_CYAN 11
#define COLOR_WHITE 15

void print_header(const char *title) {
    set_color(COLOR_CYAN);
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  %-55s║\n", title);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    set_color(COLOR_RESET);
}

void print_success(const char *msg) {
    set_color(COLOR_GREEN);
    printf("✓ %s\n", msg);
    set_color(COLOR_RESET);
}

void print_error(const char *msg) {
    set_color(COLOR_RED);
    printf("✗ %s\n", msg);
    set_color(COLOR_RESET);
}

void print_info(const char *msg) {
    set_color(COLOR_YELLOW);
    printf("ℹ %s\n", msg);
    set_color(COLOR_RESET);
}

void print_message(const char *from, const char *msg) {
    set_color(COLOR_MAGENTA);
    printf("\n[%s]: ", from);
    set_color(COLOR_WHITE);
    printf("%s\n", msg);
    set_color(COLOR_RESET);
}

// ===========================
// TCP FUNCTIONS
// ===========================

int recv_message(socket_t socket_fd, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) return -1;
    
    uint32_t msg_length;
    int total_received = 0;
    int bytes_received;
    
    while (total_received < sizeof(uint32_t)) {
        bytes_received = recv(socket_fd, 
                            ((char *)&msg_length) + total_received,
                            sizeof(uint32_t) - total_received, 
                            0);
        
        if (bytes_received <= 0) return bytes_received;
        total_received += bytes_received;
    }
    
    msg_length = ntohl(msg_length);
    
    if (msg_length == 0 || msg_length > buffer_size - 1) {
        return -1;
    }
    
    total_received = 0;
    while (total_received < (int)msg_length) {
        bytes_received = recv(socket_fd, 
                            buffer + total_received,
                            msg_length - total_received, 
                            0);
        
        if (bytes_received <= 0) return bytes_received;
        total_received += bytes_received;
    }
    
    buffer[total_received] = '\0';
    return total_received;
}

int send_message(socket_t socket_fd, const char *message, size_t length) {
    if (message == NULL || length == 0) return -1;
    
    uint32_t msg_length = htonl((uint32_t)length);
    int total_sent = 0;
    int bytes_sent;
    
    while (total_sent < sizeof(uint32_t)) {
        bytes_sent = send(socket_fd, 
                         ((char *)&msg_length) + total_sent,
                         sizeof(uint32_t) - total_sent, 
                         0);
        
        if (bytes_sent < 0) return -1;
        total_sent += bytes_sent;
    }
    
    total_sent = 0;
    while (total_sent < (int)length) {
        bytes_sent = send(socket_fd, 
                         message + total_sent,
                         length - total_sent, 
                         0);
        
        if (bytes_sent < 0) return -1;
        total_sent += bytes_sent;
    }
    
    return total_sent;
}

int send_message_struct(socket_t socket_fd, const Message *msg) {
    if (msg == NULL) return -1;
    
    char buffer[BUFFER_SIZE];
    int len = serialize_message(msg, buffer, sizeof(buffer));
    
    if (len < 0) {
        print_error("Failed to serialize message");
        return -1;
    }
    
    return send_message(socket_fd, buffer, len);
}

// ===========================
// RECEIVE THREAD
// ===========================

THREAD_RETURN receive_thread(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    Message msg;
    
    while (is_running) {
        int bytes = recv_message(server_socket, buffer, sizeof(buffer));
        
        if (bytes <= 0) {
            if (is_running) {
                print_error("Disconnected from server");
                is_running = false;
            }
            break;
        }
        
        if (parse_message(buffer, &msg) < 0) {
            continue;
        }
        
        // Handle different message types
        switch (msg.type) {
            case MSG_RESPONSE_SUCCESS:
                printf("\n");
                print_success(msg.content);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_RESPONSE_ERROR:
                printf("\n");
                print_error(msg.content);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_NOTIFICATION:
                printf("\n");
                print_info(msg.content);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_PRIVATE_MESSAGE:
                print_message(msg.from, msg.content);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_GROUP_MESSAGE:
                set_color(COLOR_CYAN);
                printf("\n[%s @ %s]: ", msg.from, msg.extra);
                set_color(COLOR_WHITE);
                printf("%s\n", msg.content);
                set_color(COLOR_RESET);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_USER_ONLINE:
                set_color(COLOR_GREEN);
                printf("\n🟢 %s is now online\n", msg.content);
                set_color(COLOR_RESET);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_USER_OFFLINE:
                set_color(COLOR_YELLOW);
                printf("\n🔴 %s is now offline\n", msg.content);
                set_color(COLOR_RESET);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_FRIEND_REQUEST:
                set_color(COLOR_MAGENTA);
                printf("\n👋 Friend request from: %s\n", msg.from);
                set_color(COLOR_RESET);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_GET_ONLINE_USERS:
            case MSG_ONLINE_USERS_LIST:
            case MSG_FRIEND_LIST:
            case MSG_GROUP_LIST:
                printf("\n%s\n", msg.content);
                printf("> ");
                fflush(stdout);
                break;
                
            case MSG_OFFLINE_SYNC:
                set_color(COLOR_YELLOW);
                printf("\n📬 Offline message from %s: %s\n", msg.from, msg.content);
                set_color(COLOR_RESET);
                printf("> ");
                fflush(stdout);
                break;
                
            default:
                break;
        }
    }
    
    return 0;
}

// ===========================
// CONNECTION
// ===========================

int connect_to_server(const char *host, int port) {
    struct sockaddr_in server_addr;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET_VALUE) {
        print_error("Socket creation failed");
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(host);
    
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        print_error("Invalid server address");
        close(server_socket);
        return -1;
    }
    
    printf("Connecting to %s:%d...\n", host, port);
    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        print_error("Connection failed");
        close(server_socket);
        return -1;
    }
    
    print_success("Connected to server!");
    return 0;
}

// ===========================
// USER FUNCTIONS
// ===========================

void register_account() {
    print_header("REGISTER NEW ACCOUNT");
    
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    
    printf("Username: ");
    if (scanf("%49s", username) != 1) return;
    
    printf("Password: ");
    if (scanf("%63s", password) != 1) return;
    
    // Format: username|password trong content
    char content[MAX_MESSAGE_LEN];
    snprintf(content, sizeof(content), "%s|%s", username, password);
    
    Message msg;
    create_response_message(&msg, MSG_REGISTER, "", "", content);
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Registration request sent. Waiting for response...");
    } else {
        print_error("Failed to send registration request");
    }
}

void login() {
    print_header("LOGIN");
    
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    
    printf("Username: ");
    if (scanf("%49s", username) != 1) return;
    
    printf("Password: ");
    if (scanf("%63s", password) != 1) return;
    
    // Format: username|password trong content
    char content[MAX_MESSAGE_LEN];
    snprintf(content, sizeof(content), "%s|%s", username, password);
    
    Message msg;
    create_response_message(&msg, MSG_LOGIN, "", "", content);
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Login request sent. Waiting for response...");
        strncpy(current_username, username, MAX_USERNAME_LEN - 1);
        is_logged_in = true;
    } else {
        print_error("Failed to send login request");
    }
}

void logout() {
    if (!is_logged_in) {
        print_error("You are not logged in!");
        return;
    }
    
    Message msg;
    create_response_message(&msg, MSG_LOGOUT, current_username, "", "");
    send_message_struct(server_socket, &msg);
    
    is_logged_in = false;
    current_username[0] = '\0';
    print_success("Logged out successfully");
}

// ===========================
// CHAT FUNCTIONS
// ===========================

void send_private_message() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("SEND PRIVATE MESSAGE");
    
    char recipient[MAX_USERNAME_LEN];
    char content[MAX_MESSAGE_LEN];
    
    printf("To (username): ");
    if (scanf("%49s", recipient) != 1) return;
    
    printf("Message: ");
    getchar(); // Clear newline
    if (fgets(content, sizeof(content), stdin) == NULL) return;
    
    // Remove trailing newline
    content[strcspn(content, "\n")] = 0;
    
    Message msg;
    create_response_message(&msg, MSG_PRIVATE_MESSAGE, current_username, recipient, content);
    
    if (send_message_struct(server_socket, &msg) > 0) {
        set_color(COLOR_BLUE);
        printf("You → %s: %s\n", recipient, content);
        set_color(COLOR_RESET);
    } else {
        print_error("Failed to send message");
    }
}

void create_group() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("CREATE GROUP");
    
    char group_name[MAX_GROUP_NAME_LEN];
    
    printf("Group name: ");
    if (scanf("%99s", group_name) != 1) return;
    
    Message msg;
    create_response_message(&msg, MSG_GROUP_CREATE, current_username, "", group_name);
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Group creation request sent...");
    } else {
        print_error("Failed to send request");
    }
}

void join_group() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("JOIN GROUP");
    
    char group_name[MAX_GROUP_NAME_LEN];
    
    printf("Group name: ");
    if (scanf("%99s", group_name) != 1) return;
    
    Message msg;
    create_response_message(&msg, MSG_GROUP_JOIN, current_username, "", group_name);
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Join request sent...");
    } else {
        print_error("Failed to send request");
    }
}

void leave_group() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("LEAVE GROUP");
    
    char group_name[MAX_GROUP_NAME_LEN];
    
    printf("Group name: ");
    if (scanf("%99s", group_name) != 1) return;
    
    Message msg;
    create_response_message(&msg, MSG_GROUP_LEAVE, current_username, "", group_name);
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Leave request sent...");
    } else {
        print_error("Failed to send request");
    }
}

void send_group_message() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("SEND GROUP MESSAGE");
    
    char group_name[MAX_GROUP_NAME_LEN];
    char content[MAX_MESSAGE_LEN];
    
    printf("Group name: ");
    if (scanf("%99s", group_name) != 1) return;
    
    printf("Message: ");
    getchar(); // Clear newline
    if (fgets(content, sizeof(content), stdin) == NULL) return;
    
    content[strcspn(content, "\n")] = 0;
    
    Message msg;
    create_response_message(&msg, MSG_GROUP_MESSAGE, current_username, "", content);
    strncpy(msg.extra, group_name, MAX_MESSAGE_LEN - 1);
    
    if (send_message_struct(server_socket, &msg) > 0) {
        set_color(COLOR_CYAN);
        printf("You → [%s]: %s\n", group_name, content);
        set_color(COLOR_RESET);
    } else {
        print_error("Failed to send message");
    }
}

// ===========================
// FRIEND FUNCTIONS
// ===========================

void send_friend_request() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("SEND FRIEND REQUEST");
    
    char friend_name[MAX_USERNAME_LEN];
    
    printf("Friend username: ");
    if (scanf("%49s", friend_name) != 1) return;
    
    Message msg;
    create_response_message(&msg, MSG_FRIEND_REQUEST, current_username, friend_name, "");
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Friend request sent!");
    } else {
        print_error("Failed to send request");
    }
}

void accept_friend_request() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("ACCEPT FRIEND REQUEST");
    
    char friend_name[MAX_USERNAME_LEN];
    
    printf("Friend username: ");
    if (scanf("%49s", friend_name) != 1) return;
    
    Message msg;
    create_response_message(&msg, MSG_FRIEND_ACCEPT, current_username, friend_name, "");
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Acceptance sent!");
    } else {
        print_error("Failed to send");
    }
}

void reject_friend_request() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("REJECT FRIEND REQUEST");
    
    char friend_name[MAX_USERNAME_LEN];
    
    printf("Friend username: ");
    if (scanf("%49s", friend_name) != 1) return;
    
    Message msg;
    create_response_message(&msg, MSG_FRIEND_REJECT, current_username, friend_name, "");
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Rejection sent!");
    } else {
        print_error("Failed to send");
    }
}

void remove_friend() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("REMOVE FRIEND");
    
    char friend_name[MAX_USERNAME_LEN];
    
    printf("Friend username: ");
    if (scanf("%49s", friend_name) != 1) return;
    
    Message msg;
    create_response_message(&msg, MSG_FRIEND_REMOVE, current_username, friend_name, "");
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Friend removed!");
    } else {
        print_error("Failed to send");
    }
}

void view_friends_list() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("FRIENDS LIST");
    
    Message msg;
    create_response_message(&msg, MSG_GET_FRIENDS, current_username, "", "");
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Fetching friends list...");
    } else {
        print_error("Failed to send request");
    }
}

// ===========================
// LIST FUNCTIONS
// ===========================

void view_online_users() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("ONLINE USERS");
    
    Message msg;
    create_response_message(&msg, MSG_GET_ONLINE_USERS, current_username, "", "");
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Fetching online users...");
    } else {
        print_error("Failed to send request");
    }
}

void view_groups_list() {
    if (!is_logged_in) {
        print_error("Please login first!");
        return;
    }
    
    print_header("GROUPS LIST");
    
    Message msg;
    create_response_message(&msg, MSG_GET_GROUPS, current_username, "", "");
    
    if (send_message_struct(server_socket, &msg) > 0) {
        print_info("Fetching groups list...");
    } else {
        print_error("Failed to send request");
    }
}

// ===========================
// MENU
// ===========================

void print_main_menu() {
    set_color(COLOR_CYAN);
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║              CHATONLINE CLIENT - MAIN MENU               ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    set_color(COLOR_YELLOW);
    printf("║  ACCOUNT:                                                ║\n");
    set_color(COLOR_WHITE);
    printf("║   1. Register          2. Login          3. Logout       ║\n");
    set_color(COLOR_YELLOW);
    printf("║  CHAT:                                                   ║\n");
    set_color(COLOR_WHITE);
    printf("║   4. Send Private Message      5. Send Group Message     ║\n");
    set_color(COLOR_YELLOW);
    printf("║  GROUP:                                                  ║\n");
    set_color(COLOR_WHITE);
    printf("║   6. Create Group      7. Join Group     8. Leave Group  ║\n");
    set_color(COLOR_YELLOW);
    printf("║  FRIENDS:                                                ║\n");
    set_color(COLOR_WHITE);
    printf("║   9. Send Friend Request       10. Accept Friend Request ║\n");
    printf("║   11. Reject Friend Request    12. Remove Friend         ║\n");
    printf("║   13. View Friends List                                  ║\n");
    set_color(COLOR_YELLOW);
    printf("║  LISTS:                                                  ║\n");
    set_color(COLOR_WHITE);
    printf("║   14. View Online Users        15. View Groups           ║\n");
    set_color(COLOR_YELLOW);
    printf("║  SYSTEM:                                                 ║\n");
    set_color(COLOR_WHITE);
    printf("║   0. Exit                                                ║\n");
    set_color(COLOR_CYAN);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    set_color(COLOR_RESET);
    
    if (is_logged_in) {
        set_color(COLOR_GREEN);
        printf("Logged in as: %s\n", current_username);
        set_color(COLOR_RESET);
    } else {
        set_color(COLOR_RED);
        printf("Not logged in\n");
        set_color(COLOR_RESET);
    }
}

// ===========================
// MAIN
// ===========================

int main(int argc, char *argv[]) {
    // Initialize network
    if (init_network() < 0) {
        print_error("Failed to initialize network");
        return 1;
    }
    
    // Set console title
    SetConsoleTitle("ChatOnline Client");
    
    // Print banner
    system("cls");
    set_color(COLOR_CYAN);
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════╗\n");
    printf("  ║                                                       ║\n");
    printf("  ║              CHATONLINE CLIENT v1.0                   ║\n");
    printf("  ║              Full-Featured Chat Client                ║\n");
    printf("  ║                                                       ║\n");
    printf("  ╚═══════════════════════════════════════════════════════╝\n");
    set_color(COLOR_RESET);
    printf("\n");
    
    // Server connection info
    char host[256] = "";
    int port = PORT;
    
    // Nhận IP từ command line hoặc hỏi user
    if (argc > 1) {
        strncpy(host, argv[1], sizeof(host) - 1);
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    
    // Nếu không có IP từ command line, hỏi user
    if (strlen(host) == 0) {
        print_header("SERVER CONNECTION");
        set_color(COLOR_YELLOW);
        printf("  Connection Options:\n");
        printf("  [1]  Localhost (Same PC)         → 127.0.0.1\n");
        printf("  [2]  LAN (Same Network)          → e.g., 192.168.1.100\n");
        printf("  [3]  Tailscale (Remote Network)  → e.g., 100.x.x.x\n");
        set_color(COLOR_RESET);
        printf("\n");
        
        printf("Choose connection type [1-3]: ");
        int conn_type;
        if (scanf("%d", &conn_type) == 1) {
            while (getchar() != '\n'); // Clear buffer
            
            switch (conn_type) {
                case 1:
                    strcpy(host, "127.0.0.1");
                    set_color(COLOR_GREEN);
                    printf("✓ Using Localhost (127.0.0.1)\n");
                    set_color(COLOR_RESET);
                    break;
                    
                case 2:
                case 3:
                    printf("\nEnter server IP address: ");
                    if (fgets(host, sizeof(host), stdin) != NULL) {
                        host[strcspn(host, "\n")] = 0; // Remove newline
                        
                        if (strlen(host) == 0) {
                            strcpy(host, "127.0.0.1");
                        }
                        
                        if (conn_type == 3) {
                            set_color(COLOR_CYAN);
                            printf(" Tip: Install Tailscale on both machines first!\n");
                            printf("   Visit: https://tailscale.com/download\n");
                            set_color(COLOR_RESET);
                        }
                    }
                    break;
                    
                default:
                    strcpy(host, "127.0.0.1");
                    set_color(COLOR_YELLOW);
                    printf(" Invalid option, using Localhost\n");
                    set_color(COLOR_RESET);
            }
        } else {
            strcpy(host, "127.0.0.1");
        }
        
        printf("\n");
    }
    
    // Connect to server
    if (connect_to_server(host, port) < 0) {
        cleanup_network();
        return 1;
    }
    
    // Start receive thread
    thread_t recv_thread;
    recv_thread = (HANDLE)_beginthreadex(NULL, 0, receive_thread, NULL, 0, NULL);
    if (recv_thread == 0) {
        print_error("Failed to create receive thread");
        close(server_socket);
        cleanup_network();
        return 1;
    }
    
    // Main loop
    int choice;
    while (is_running) {
        print_main_menu();
        printf("\n> ");
        
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1: register_account(); break;
            case 2: login(); break;
            case 3: logout(); break;
            case 4: send_private_message(); break;
            case 5: send_group_message(); break;
            case 6: create_group(); break;
            case 7: join_group(); break;
            case 8: leave_group(); break;
            case 9: send_friend_request(); break;
            case 10: accept_friend_request(); break;
            case 11: reject_friend_request(); break;
            case 12: remove_friend(); break;
            case 13: view_friends_list(); break;
            case 14: view_online_users(); break;
            case 15: view_groups_list(); break;
            case 0:
                is_running = false;
                print_info("Shutting down...");
                break;
            default:
                print_error("Invalid choice!");
                break;
        }
        
        Sleep(100);
    }
    
    // Cleanup
    close(server_socket);
    WaitForSingleObject(recv_thread, 2000);
    CloseHandle(recv_thread);
    cleanup_network();
    
    set_color(COLOR_CYAN);
    printf("\nThank you for using ChatOnline! Goodbye!\n\n");
    set_color(COLOR_RESET);
    
    return 0;
}
