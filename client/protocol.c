#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * Initialize network - Linux (no-op)
 */
int init_network(void) {
    printf("[NETWORK] Linux sockets ready\n");
    return 0;
}

/**
 * Cleanup network - Linux (no-op)
 */
void cleanup_network(void) {
    printf("[NETWORK] Linux sockets cleaned up\n");
}

/**
 * Linux sleep function
 */
void sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}



/**
 * Parse raw message string thành struct Message
 * Format: TYPE|FROM:username|TO:recipient|CONTENT:text|TIME:timestamp|EXTRA:data
 */
int parse_message(const char *raw, Message *msg) {
    if (raw == NULL || msg == NULL) {
        return -1;
    }
    
    // Clear message struct
    memset(msg, 0, sizeof(Message));
    
    // Create a copy to work with
    char buffer[BUFFER_SIZE];
    strncpy(buffer, raw, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    // Parse TYPE 
    char *ptr = buffer;
    char *pipe = strchr(ptr, '|');
    if (pipe == NULL) {
        // Only type, no other fields
        msg->type = (MessageType)atoi(ptr);
        return 0;
    }
    
    *pipe = '\0';
    msg->type = (MessageType)atoi(ptr);
    ptr = pipe + 1;
    
    // Parse remaining fields: FROM:, TO:, CONTENT:, TIME:, EXTRA:
    while (*ptr != '\0') {
        // Find next field (KEY:VALUE|...)
        char *colon = strchr(ptr, ':');
        if (colon == NULL) break;
        
        *colon = '\0';
        char *key = ptr;
        char *value_start = colon + 1;
        
        // Find end of value (next | or end of string)
        char *value_end = value_start;
        char *next_pipe = NULL;
        
        // Special handling for CONTENT and EXTRA - they may contain |
        if (strcmp(key, "CONTENT") == 0) {
            // CONTENT: read until |TIME: or |EXTRA: or end
            next_pipe = strstr(value_start, "|TIME:");
            if (next_pipe == NULL) next_pipe = strstr(value_start, "|EXTRA:");
            if (next_pipe == NULL) {
                // CONTENT is last field
                strncpy(msg->content, value_start, MAX_MESSAGE_LEN - 1);
                msg->content[MAX_MESSAGE_LEN - 1] = '\0';
                break;
            } else {
                size_t len = next_pipe - value_start;
                if (len >= MAX_MESSAGE_LEN) len = MAX_MESSAGE_LEN - 1;
                strncpy(msg->content, value_start, len);
                msg->content[len] = '\0';
                ptr = next_pipe + 1;
                continue;
            }
        } else if (strcmp(key, "EXTRA") == 0) {
            // EXTRA: read until end (usually last field)
            strncpy(msg->extra, value_start, MAX_MESSAGE_LEN - 1);
            msg->extra[MAX_MESSAGE_LEN - 1] = '\0';
            break;
        }
        
        // For FROM, TO, TIME: find next |
        next_pipe = strchr(value_start, '|');
        if (next_pipe != NULL) {
            *next_pipe = '\0';
            value_end = next_pipe;
        } else {
            value_end = value_start + strlen(value_start);
        }
        
        // Store value
        if (strcmp(key, "FROM") == 0) {
            strncpy(msg->from, value_start, MAX_USERNAME_LEN - 1);
            msg->from[MAX_USERNAME_LEN - 1] = '\0';
        } else if (strcmp(key, "TO") == 0) {
            strncpy(msg->to, value_start, MAX_USERNAME_LEN - 1);
            msg->to[MAX_USERNAME_LEN - 1] = '\0';
        } else if (strcmp(key, "TIME") == 0) {
            strncpy(msg->timestamp, value_start, 31);
            msg->timestamp[31] = '\0';
        }
        
        // Move to next field
        if (next_pipe == NULL) break;
        ptr = value_end + 1;
    }
    
    return 0;
}

/**
 * Serialize struct Message thành raw string
 * Format: TYPE|FROM:username|TO:recipient|CONTENT:text|TIME:timestamp|EXTRA:data
 */
int serialize_message(const Message *msg, char *buffer, size_t buffer_size) {
    if (msg == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    int len = 0;
    
    // TYPE
    len = snprintf(buffer, buffer_size, "%d", msg->type);
    if (len < 0 || (size_t)len >= buffer_size) return -1;
    
    // FROM
    if (msg->from[0] != '\0') {
        int ret = snprintf(buffer + len, buffer_size - len, "|FROM:%s", msg->from);
        if (ret < 0 || (size_t)ret >= buffer_size - len) return -1;
        len += ret;
    }
    
    // TO
    if (msg->to[0] != '\0') {
        int ret = snprintf(buffer + len, buffer_size - len, "|TO:%s", msg->to);
        if (ret < 0 || (size_t)ret >= buffer_size - len) return -1;
        len += ret;
    }
    
    // CONTENT
    if (msg->content[0] != '\0') {
        int ret = snprintf(buffer + len, buffer_size - len, "|CONTENT:%s", msg->content);
        if (ret < 0 || (size_t)ret >= buffer_size - len) return -1;
        len += ret;
    }
    
    // TIME
    if (msg->timestamp[0] != '\0') {
        int ret = snprintf(buffer + len, buffer_size - len, "|TIME:%s", msg->timestamp);
        if (ret < 0 || (size_t)ret >= buffer_size - len) return -1;
        len += ret;
    }
    
    // EXTRA
    if (msg->extra[0] != '\0') {
        int ret = snprintf(buffer + len, buffer_size - len, "|EXTRA:%s", msg->extra);
        if (ret < 0 || (size_t)ret >= buffer_size - len) return -1;
        len += ret;
    }
    
    return len;
}

/**
 * Tạo timestamp hiện tại
 * Format: YYYY-MM-DD HH:MM:SS
 */
void get_current_timestamp(char *timestamp, size_t size) {
    if (timestamp == NULL || size == 0) return;
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    strftime(timestamp, size, "%Y-%m-%d %H:%M:%S", t);
}

/**
 * Tạo message response đơn giản
 */
void create_response_message(Message *msg, MessageType type, const char *from, 
                             const char *to, const char *content) {
    if (msg == NULL) return;
    
    memset(msg, 0, sizeof(Message));
    msg->type = type;
    
    if (from != NULL) {
        strncpy(msg->from, from, MAX_USERNAME_LEN - 1);
    }
    
    if (to != NULL) {
        strncpy(msg->to, to, MAX_USERNAME_LEN - 1);
    }
    
    if (content != NULL) {
        strncpy(msg->content, content, MAX_MESSAGE_LEN - 1);
    }
    
    get_current_timestamp(msg->timestamp, sizeof(msg->timestamp));
}
