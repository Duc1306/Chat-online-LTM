/*
 * ChatOnline Client - GTK+3 GUI for Linux
 * 
 * === COMPILE ===
 * gcc -o client_gtk client_gtk.c protocol.c `pkg-config --cflags --libs gtk+-3.0` -pthread -Wall
 * 
 * === RUN ===
 * ./client_gtk
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "protocol.h"

// ===========================
// GLOBAL STATE
// ===========================
int client_socket = -1;
char current_username[MAX_USERNAME_LEN] = "";
bool is_logged_in = false;
char target_name[MAX_USERNAME_LEN] = "";
bool is_group_chat = false; // Flag to know if chatting with group
pthread_t recv_thread_id;
bool is_running = true;

// ===========================
// GTK WIDGETS
// ===========================
GtkWidget *window;
GtkWidget *main_stack;

// Login widgets
GtkWidget *login_box;
GtkWidget *entry_host;
GtkWidget *entry_port;
GtkWidget *entry_username;
GtkWidget *entry_password;
GtkWidget *btn_login;
GtkWidget *btn_register;

// Chat widgets
GtkWidget *chat_box;
GtkWidget *notebook_tabs;
GtkWidget *listbox_users;
GtkWidget *listbox_friends;
GtkWidget *listbox_groups;
GtkWidget *textview_chat;
GtkWidget *entry_message;
GtkWidget *btn_send;
GtkWidget *label_chatting_with;
GtkTextBuffer *chat_buffer;

// Friend/Group widgets
GtkWidget *entry_friend_username;
GtkWidget *btn_add_friend;
GtkWidget *entry_group_name;
GtkWidget *entry_join_group_name;
GtkWidget *btn_create_group;

// ===========================
// NETWORK FUNCTIONS
// ===========================

int send_packet(const char *data, int len) {
    if (client_socket < 0) return -1;
    
    uint32_t net_len = htonl((uint32_t)len);
    if (send(client_socket, &net_len, sizeof(net_len), 0) <= 0) return -1;
    
    int total_sent = 0;
    while (total_sent < len) {
        int sent = send(client_socket, data + total_sent, len - total_sent, 0);
        if (sent <= 0) return -1;
        total_sent += sent;
    }
    return total_sent;
}

int recv_packet(char *buffer, size_t buffer_size) {
    uint32_t msg_length;
    int total_received = 0;
    
    while (total_received < sizeof(uint32_t)) {
        int bytes = recv(client_socket, ((char *)&msg_length) + total_received,
                        sizeof(uint32_t) - total_received, 0);
        if (bytes <= 0) return bytes;
        total_received += bytes;
    }
    
    msg_length = ntohl(msg_length);
    if (msg_length == 0 || msg_length > buffer_size - 1) return -1;
    
    total_received = 0;
    while (total_received < msg_length) {
        int bytes = recv(client_socket, buffer + total_received,
                        msg_length - total_received, 0);
        if (bytes <= 0) return bytes;
        total_received += bytes;
    }
    
    buffer[total_received] = '\0';
    return total_received;
}

void send_request(int type, const char *content, const char *to) {
    Message msg;
    create_response_message(&msg, type, current_username, to, content);
    
    char buffer[BUFFER_SIZE];
    int len = serialize_message(&msg, buffer, sizeof(buffer));
    if (len > 0) {
        send_packet(buffer, len);
    }
}

// ===========================
// GUI UPDATE FUNCTIONS (Thread-safe)
// ===========================

gboolean append_chat_idle(gpointer data) {
    char *text = (char *)data;
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(chat_buffer, &iter);
    gtk_text_buffer_insert(chat_buffer, &iter, text, -1);
    
    // Auto scroll to bottom
    GtkTextMark *mark = gtk_text_buffer_get_insert(chat_buffer);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textview_chat), mark, 0.0, TRUE, 0.0, 1.0);
    
    g_free(text);
    return FALSE;
}

gboolean update_user_list_idle(gpointer data) {
    char *user_data = (char *)data;
    
    // Clear list
    GList *children = gtk_container_get_children(GTK_CONTAINER(listbox_users));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    if (!user_data || strlen(user_data) == 0) {
        g_free(user_data);
        return FALSE;
    }
    
    char *token = strtok(user_data, ",");
    while (token) {
        // Trim whitespace
        while (*token == ' ') token++;
        
        if (strcmp(token, current_username) != 0) {
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *label = gtk_label_new(token);
            gtk_label_set_xalign(GTK_LABEL(label), 0.0);
            gtk_container_add(GTK_CONTAINER(row), label);
            gtk_widget_show_all(row);
            gtk_container_add(GTK_CONTAINER(listbox_users), row);
        }
        token = strtok(NULL, ",");
    }
    
    g_free(user_data);
    return FALSE;
}

void update_user_list(const char *data) {
    if (!data) return;
    g_idle_add(update_user_list_idle, g_strdup(data));
}

typedef struct {
    char *message;
    int is_error;
} DialogData;

gboolean show_dialog_idle(gpointer data) {
    DialogData *dialog_data = (DialogData *)data;
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               dialog_data->is_error ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK,
                                               "%s", dialog_data->message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    g_free(dialog_data->message);
    g_free(dialog_data);
    return FALSE;
}

void show_error_dialog(const char *message) {
    DialogData *data = g_malloc(sizeof(DialogData));
    data->message = g_strdup(message);
    data->is_error = 1;
    g_idle_add(show_dialog_idle, data);
}

void show_info_dialog(const char *message) {
    DialogData *data = g_malloc(sizeof(DialogData));
    data->message = g_strdup(message);
    data->is_error = 0;
    g_idle_add(show_dialog_idle, data);
}

// Friend list update
gboolean update_friend_list_idle(gpointer data) {
    char *friend_data = (char *)data;
    
    // Clear list
    GList *children = gtk_container_get_children(GTK_CONTAINER(listbox_friends));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    if (!friend_data || strlen(friend_data) == 0) {
        g_free(friend_data);
        return FALSE;
    }
    
    char *token = strtok(friend_data, ",");
    while (token) {
        while (*token == ' ') token++;
        
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(token);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_widget_show_all(row);
        gtk_container_add(GTK_CONTAINER(listbox_friends), row);
        
        token = strtok(NULL, ",");
    }
    
    g_free(friend_data);
    return FALSE;
}

void update_friend_list(const char *data) {
    if (!data) return;
    g_idle_add(update_friend_list_idle, g_strdup(data));
}

// Group list update
gboolean update_group_list_idle(gpointer data) {
    char *group_data = (char *)data;
    
    // Clear list
    GList *children = gtk_container_get_children(GTK_CONTAINER(listbox_groups));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    if (!group_data || strlen(group_data) == 0) {
        g_free(group_data);
        return FALSE;
    }
    
    char *token = strtok(group_data, ",");
    while (token) {
        while (*token == ' ') token++;
        
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(token);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_widget_show_all(row);
        gtk_container_add(GTK_CONTAINER(listbox_groups), row);
        
        token = strtok(NULL, ",");
    }
    
    g_free(group_data);
    return FALSE;
}

void update_group_list(const char *data) {
    if (!data) return;
    g_idle_add(update_group_list_idle, g_strdup(data));
}

// Friend request dialog
typedef struct {
    char *username;
} FriendRequestData;

gboolean show_friend_request_dialog_idle(gpointer data) {
    FriendRequestData *req_data = (FriendRequestData *)data;
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_YES_NO,
                                               "Friend request from: %s\nAccept?", req_data->username);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    // Send response to server
    // to = người gửi request (req_data->username)
    // from = current_username (tự động set trong send_request)
    if (response == GTK_RESPONSE_YES) {
        send_request(MSG_FRIEND_ACCEPT, "", req_data->username);
    } else {
        send_request(MSG_FRIEND_REJECT, "", req_data->username);
    }
    
    g_free(req_data->username);
    g_free(req_data);
    return FALSE;
}

void show_friend_request_dialog(const char *username) {
    FriendRequestData *data = g_malloc(sizeof(FriendRequestData));
    data->username = g_strdup(username);
    g_idle_add(show_friend_request_dialog_idle, data);
}

// ===========================
// MESSAGE PROCESSING
// ===========================

typedef struct {
    bool switch_to_chat;
} SwitchViewData;

gboolean switch_view_idle(gpointer data) {
    SwitchViewData *view_data = (SwitchViewData *)data;
    gtk_stack_set_visible_child_name(GTK_STACK(main_stack), 
                                     view_data->switch_to_chat ? "chat" : "login");
    g_free(view_data);
    return FALSE;
}

void process_message(const char *raw_data) {
    Message msg;
    if (parse_message(raw_data, &msg) < 0) return;
    
    char buffer[BUFFER_SIZE];
    
    switch (msg.type) {
        case MSG_RESPONSE_SUCCESS:
        case MSG_SUCCESS:
            if (!is_logged_in) {
                if (strstr(msg.content, "Login successful")) {
                    is_logged_in = true;
                    
                    // Switch to chat screen
                    SwitchViewData *view_data = g_malloc(sizeof(SwitchViewData));
                    view_data->switch_to_chat = true;
                    g_idle_add(switch_view_idle, view_data);
                    
                    show_info_dialog("Login successful!");
                    send_request(MSG_GET_ONLINE_USERS, "", "");
                } else if (strstr(msg.content, "Registration successful")) {
                    show_info_dialog("Registration successful! Please log in.");
                }
            } else {
                g_idle_add(append_chat_idle, g_strdup_printf("[INFO] %s\n", msg.content));
            }
            break;
            
        case MSG_PRIVATE_MESSAGE:
            snprintf(buffer, sizeof(buffer), "[%s]: %s\n", msg.from, msg.content);
            g_idle_add(append_chat_idle, g_strdup(buffer));
            break;
            
        case MSG_GROUP_MESSAGE:
            snprintf(buffer, sizeof(buffer), "[%s @ %s]: %s\n", msg.from, msg.extra, msg.content);
            g_idle_add(append_chat_idle, g_strdup(buffer));
            break;
            
        case MSG_ONLINE_USERS_LIST:
            update_user_list(msg.content);
            break;
            
        case MSG_USER_ONLINE:
            snprintf(buffer, sizeof(buffer), "--- %s is now online ---\n", msg.content);
            g_idle_add(append_chat_idle, g_strdup(buffer));
            send_request(MSG_GET_ONLINE_USERS, "", "");
            break;
            
        case MSG_USER_OFFLINE:
            snprintf(buffer, sizeof(buffer), "--- %s is now offline ---\n", msg.content);
            g_idle_add(append_chat_idle, g_strdup(buffer));
            send_request(MSG_GET_ONLINE_USERS, "", "");
            break;
            
        case MSG_FRIEND_LIST:
            update_friend_list(msg.content);
            break;
            
        case MSG_GROUP_LIST:
            update_group_list(msg.content);
            break;
            
        case MSG_RESPONSE_ERROR:
        case MSG_ERROR:
            show_error_dialog(msg.content);
            break;
            
        case MSG_NOTIFICATION:
            g_idle_add(append_chat_idle, g_strdup_printf("[NOTIFICATION] %s\n", msg.content));
            break;
            
        case MSG_FRIEND_REQUEST:
            snprintf(buffer, sizeof(buffer), "Friend request from: %s", msg.from);
            show_friend_request_dialog(msg.from);
            break;
            
        case MSG_OFFLINE_SYNC:
            snprintf(buffer, sizeof(buffer), "[OFFLINE MSG from %s]: %s\n", msg.from, msg.content);
            g_idle_add(append_chat_idle, g_strdup(buffer));
            break;
            
        default:
            break;
    }
}

// ===========================
// RECEIVE THREAD
// ===========================

void *receive_thread(void *arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    
    while (is_running) {
        int bytes = recv_packet(buffer, sizeof(buffer));
        
        if (bytes <= 0) {
            if (is_running) {
                g_idle_add(append_chat_idle, g_strdup("--- Disconnected from server ---"));
            }
            break;
        }
        
        process_message(buffer);
    }
    
    return NULL;
}

bool connect_to_server(const char *ip, int port) {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) return false;
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(client_socket);
        client_socket = -1;
        return false;
    }
    
    pthread_create(&recv_thread_id, NULL, receive_thread, NULL);
    pthread_detach(recv_thread_id);
    
    return true;
}

// ===========================
// CALLBACK FUNCTIONS
// ===========================

void on_btn_login_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    const char *ip = gtk_entry_get_text(GTK_ENTRY(entry_host));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(entry_port));
    const char *username = gtk_entry_get_text(GTK_ENTRY(entry_username));
    const char *password = gtk_entry_get_text(GTK_ENTRY(entry_password));
    
    if (strlen(username) == 0 || strlen(password) == 0) {
        show_error_dialog("Please enter username and password!");
        return;
    }
    
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        show_error_dialog("Invalid port number!");
        return;
    }
    
    if (client_socket < 0) {
        if (!connect_to_server(ip, port)) {
            show_error_dialog("Cannot connect to server!\nCheck IP/Port and ensure server is running.");
            return;
        }
    }
    
    strncpy(current_username, username, MAX_USERNAME_LEN - 1);
    
    char content[256];
    snprintf(content, sizeof(content), "%s|%s", username, password);
    send_request(MSG_LOGIN, content, "");
}

void on_btn_register_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    const char *ip = gtk_entry_get_text(GTK_ENTRY(entry_host));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(entry_port));
    const char *username = gtk_entry_get_text(GTK_ENTRY(entry_username));
    const char *password = gtk_entry_get_text(GTK_ENTRY(entry_password));
    
    if (strlen(username) == 0 || strlen(password) == 0) {
        show_error_dialog("Please enter username and password!");
        return;
    }
    
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        show_error_dialog("Invalid port number!");
        return;
    }
    
    if (client_socket < 0) {
        if (!connect_to_server(ip, port)) {
            show_error_dialog("Cannot connect to server!\nCheck IP/Port and ensure server is running.");
            return;
        }
    }
    
    char content[256];
    snprintf(content, sizeof(content), "%s|%s", username, password);
    send_request(MSG_REGISTER, content, "");
}

void on_btn_send_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    if (strlen(target_name) == 0) {
        show_error_dialog("Please select a user/group to chat with!");
        return;
    }
    
    const char *message = gtk_entry_get_text(GTK_ENTRY(entry_message));
    if (strlen(message) == 0) return;
    
    if (is_group_chat) {
        send_request(MSG_GROUP_MESSAGE, message, target_name);
    } else {
        send_request(MSG_PRIVATE_MESSAGE, message, target_name);
    }
    
    char display[BUFFER_SIZE];
    snprintf(display, sizeof(display), "Me -> %s: %s\n", target_name, message);
    g_idle_add(append_chat_idle, g_strdup(display));
    
    gtk_entry_set_text(GTK_ENTRY(entry_message), "");
}

void on_entry_message_activate(GtkWidget *widget, gpointer data) {
    on_btn_send_clicked(widget, data);
}

void on_user_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    (void)data;
    
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
    const char *username = gtk_label_get_text(GTK_LABEL(label));
    
    strncpy(target_name, username, MAX_USERNAME_LEN - 1);
    is_group_chat = false;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Chatting with: %s", target_name);
    gtk_label_set_text(GTK_LABEL(label_chatting_with), buffer);
}

void on_friend_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    (void)data;
    
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
    const char *username = gtk_label_get_text(GTK_LABEL(label));
    
    strncpy(target_name, username, MAX_USERNAME_LEN - 1);
    is_group_chat = false;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Chatting with friend: %s", target_name);
    gtk_label_set_text(GTK_LABEL(label_chatting_with), buffer);
}

void on_group_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    (void)data;
    
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
    const char *group_name = gtk_label_get_text(GTK_LABEL(label));
    
    // Show dialog to join group or send group message
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               "Group: %s", group_name);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Join Group", 1);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Chat in Group", 2);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == 1) {
        // Join group
        send_request(MSG_GROUP_JOIN, group_name, "");
        show_info_dialog("Join request sent!");
    } else if (response == 2) {
        // Set as chat target
        strncpy(target_name, group_name, MAX_USERNAME_LEN - 1);
        is_group_chat = true;
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Group chat: %s", target_name);
        gtk_label_set_text(GTK_LABEL(label_chatting_with), buffer);
    }
}

void on_btn_add_friend_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    const char *username = gtk_entry_get_text(GTK_ENTRY(entry_friend_username));
    if (strlen(username) == 0) {
        show_error_dialog("Please enter a username!");
        return;
    }
    
    send_request(MSG_FRIEND_REQUEST, "", username);
    gtk_entry_set_text(GTK_ENTRY(entry_friend_username), "");
}

void on_btn_create_group_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    const char *group_name = gtk_entry_get_text(GTK_ENTRY(entry_group_name));
    if (strlen(group_name) == 0) {
        show_error_dialog("Please enter a group name!");
        return;
    }
    
    send_request(MSG_GROUP_CREATE, group_name, "");
    gtk_entry_set_text(GTK_ENTRY(entry_group_name), "");
}

void on_btn_join_group_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    const char *group_name = gtk_entry_get_text(GTK_ENTRY(entry_join_group_name));
    if (strlen(group_name) == 0) {
        show_error_dialog("Please enter a group name!");
        return;
    }
    
    // MSG_GROUP_JOIN sử dụng extra field để chứa group name
    Message msg;
    create_response_message(&msg, MSG_GROUP_JOIN, current_username, "", "");
    strncpy(msg.extra, group_name, sizeof(msg.extra) - 1);
    
    char buffer[BUFFER_SIZE];
    int len = serialize_message(&msg, buffer, sizeof(buffer));
    if (len > 0) {
        send_packet(buffer, len);
    }
    
    gtk_entry_set_text(GTK_ENTRY(entry_join_group_name), "");
}

void on_window_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    is_running = false;
    
    if (is_logged_in && client_socket >= 0) {
        send_request(MSG_LOGOUT, "", "");
        usleep(100000); // 100ms
    }
    
    if (client_socket >= 0) {
        close(client_socket);
    }
    
    gtk_main_quit();
}

void on_notebook_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer data) {
    (void)notebook;
    (void)page;
    (void)data;
    
    switch (page_num) {
        case 0: // Online users
            send_request(MSG_GET_ONLINE_USERS, "", "");
            break;
        case 1: // Friends
            send_request(MSG_GET_FRIENDS, "", "");
            break;
        case 2: // Groups
            send_request(MSG_GET_GROUPS, "", "");
            break;
    }
}

// ===========================
// GUI CREATION
// ===========================

void create_login_screen() {
    login_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(login_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(login_box, GTK_ALIGN_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(login_box), 20);
    
    // Title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span font='20'><b>ChatOnline Client</b></span>");
    gtk_box_pack_start(GTK_BOX(login_box), title, FALSE, FALSE, 20);
    
    // Grid for form
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    // Host
    GtkWidget *label_host = gtk_label_new("Host IP:");
    gtk_widget_set_halign(label_host, GTK_ALIGN_END);
    entry_host = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_host), "127.0.0.1");
    gtk_entry_set_width_chars(GTK_ENTRY(entry_host), 20);
    gtk_grid_attach(GTK_GRID(grid), label_host, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_host, 1, 0, 1, 1);
    
    // Port
    GtkWidget *label_port = gtk_label_new("Port:");
    gtk_widget_set_halign(label_port, GTK_ALIGN_END);
    entry_port = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_port), "8888");
    gtk_entry_set_width_chars(GTK_ENTRY(entry_port), 20);
    gtk_grid_attach(GTK_GRID(grid), label_port, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_port, 1, 1, 1, 1);
    
    // Username
    GtkWidget *label_user = gtk_label_new("Username:");
    gtk_widget_set_halign(label_user, GTK_ALIGN_END);
    entry_username = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry_username), 20);
    gtk_grid_attach(GTK_GRID(grid), label_user, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 2, 1, 1);
    
    // Password
    GtkWidget *label_pass = gtk_label_new("Password:");
    gtk_widget_set_halign(label_pass, GTK_ALIGN_END);
    entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
    gtk_entry_set_width_chars(GTK_ENTRY(entry_password), 20);
    gtk_grid_attach(GTK_GRID(grid), label_pass, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_password, 1, 3, 1, 1);
    
    gtk_box_pack_start(GTK_BOX(login_box), grid, FALSE, FALSE, 10);
    
    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    
    btn_login = gtk_button_new_with_label("LOGIN");
    gtk_widget_set_size_request(btn_login, 100, 35);
    g_signal_connect(btn_login, "clicked", G_CALLBACK(on_btn_login_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), btn_login, FALSE, FALSE, 0);
    
    btn_register = gtk_button_new_with_label("REGISTER");
    gtk_widget_set_size_request(btn_register, 100, 35);
    g_signal_connect(btn_register, "clicked", G_CALLBACK(on_btn_register_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), btn_register, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(login_box), button_box, FALSE, FALSE, 20);
    
    gtk_stack_add_named(GTK_STACK(main_stack), login_box, "login");
}

void create_chat_screen() {
    chat_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    // Left panel with tabs
    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(left_panel, 250, -1);
    gtk_container_set_border_width(GTK_CONTAINER(left_panel), 5);
    
    // Notebook with tabs
    notebook_tabs = gtk_notebook_new();
    g_signal_connect(notebook_tabs, "switch-page", G_CALLBACK(on_notebook_switch_page), NULL);
    
    // Online users tab
    GtkWidget *scroll_users = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_users),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    listbox_users = gtk_list_box_new();
    g_signal_connect(listbox_users, "row-activated", G_CALLBACK(on_user_row_activated), NULL);
    gtk_container_add(GTK_CONTAINER(scroll_users), listbox_users);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_tabs), scroll_users, gtk_label_new("Online"));
    
    // Friends tab
    GtkWidget *friends_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *scroll_friends = gtk_scrolled_window_new(NULL, NULL);
    listbox_friends = gtk_list_box_new();
    g_signal_connect(listbox_friends, "row-activated", G_CALLBACK(on_friend_row_activated), NULL);
    gtk_container_add(GTK_CONTAINER(scroll_friends), listbox_friends);
    gtk_box_pack_start(GTK_BOX(friends_box), scroll_friends, TRUE, TRUE, 0);
    
    // Add friend section
    GtkWidget *add_friend_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry_friend_username = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_friend_username), "Username");
    btn_add_friend = gtk_button_new_with_label("Add Friend");
    g_signal_connect(btn_add_friend, "clicked", G_CALLBACK(on_btn_add_friend_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(add_friend_box), entry_friend_username, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(add_friend_box), btn_add_friend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(friends_box), add_friend_box, FALSE, FALSE, 0);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_tabs), friends_box, gtk_label_new("Friends"));
    
    // Groups tab
    GtkWidget *groups_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *scroll_groups = gtk_scrolled_window_new(NULL, NULL);
    listbox_groups = gtk_list_box_new();
    g_signal_connect(listbox_groups, "row-activated", G_CALLBACK(on_group_row_activated), NULL);
    gtk_container_add(GTK_CONTAINER(scroll_groups), listbox_groups);
    gtk_box_pack_start(GTK_BOX(groups_box), scroll_groups, TRUE, TRUE, 0);
    
    // Create group section
    GtkWidget *create_group_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry_group_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_group_name), "Group name");
    btn_create_group = gtk_button_new_with_label("Create Group");
    g_signal_connect(btn_create_group, "clicked", G_CALLBACK(on_btn_create_group_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(create_group_box), entry_group_name, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(create_group_box), btn_create_group, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(groups_box), create_group_box, FALSE, FALSE, 0);
    
    // Join group section
    GtkWidget *join_group_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry_join_group_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_join_group_name), "Group name to join");
    GtkWidget *btn_join_group = gtk_button_new_with_label("Join Group");
    g_signal_connect(btn_join_group, "clicked", G_CALLBACK(on_btn_join_group_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(join_group_box), entry_join_group_name, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(join_group_box), btn_join_group, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(groups_box), join_group_box, FALSE, FALSE, 0);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_tabs), groups_box, gtk_label_new("Groups"));
    
    gtk_box_pack_start(GTK_BOX(left_panel), notebook_tabs, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(chat_box), left_panel, FALSE, FALSE, 0);
    
    // Right panel - chat area
    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(right_panel), 5);
    
    // Chatting with label
    label_chatting_with = gtk_label_new("Select a user to chat...");
    gtk_widget_set_halign(label_chatting_with, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(right_panel), label_chatting_with, FALSE, FALSE, 0);
    
    // Chat history
    GtkWidget *scroll_chat = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_chat),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    textview_chat = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textview_chat), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview_chat), GTK_WRAP_WORD);
    chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_chat));
    gtk_container_add(GTK_CONTAINER(scroll_chat), textview_chat);
    gtk_box_pack_start(GTK_BOX(right_panel), scroll_chat, TRUE, TRUE, 0);
    
    // Message input
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry_message = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_message), "Type your message...");
    g_signal_connect(entry_message, "activate", G_CALLBACK(on_entry_message_activate), NULL);
    btn_send = gtk_button_new_with_label("SEND");
    g_signal_connect(btn_send, "clicked", G_CALLBACK(on_btn_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(input_box), entry_message, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_box), btn_send, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_panel), input_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(chat_box), right_panel, TRUE, TRUE, 0);
    
    gtk_stack_add_named(GTK_STACK(main_stack), chat_box, "chat");
}

// ===========================
// MAIN
// ===========================

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Initialize network
    if (init_network() < 0) {
        fprintf(stderr, "Failed to initialize network\n");
        return 1;
    }
    
    // Create window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ChatOnline - GTK Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // Create stack for switching between login and chat
    main_stack = gtk_stack_new();
    gtk_container_add(GTK_CONTAINER(window), main_stack);
    
    // Create screens
    create_login_screen();
    create_chat_screen();
    
    // Show login screen first
    gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "login");
    
    gtk_widget_show_all(window);
    gtk_main();
    
    cleanup_network();
    return 0;
}
