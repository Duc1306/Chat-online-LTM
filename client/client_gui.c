/*
 * ChatOnline Client - Fixed Event Handling
 * Fix: Buttons are now direct children of Main Window to ensure clicks are detected.
 * Compile: gcc -o client_gui.exe client_gui.c protocol.c -lws2_32 -lgdi32 -luser32 -lcomctl32 -Wall
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <process.h>
#include "protocol.h"

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

// ===========================
// CONSTANTS & ID
// ===========================
#define WM_SOCKET_DATA   (WM_USER + 1)

// --- ID SCHEME ---
// Login Controls: 100 - 110
#define IDC_LBL_HOST     100
#define IDC_EDIT_HOST    101
#define IDC_LBL_PORT     102
#define IDC_EDIT_PORT    103
#define IDC_LBL_USER     104
#define IDC_EDIT_USER    105
#define IDC_LBL_PASS     106
#define IDC_EDIT_PASS    107
#define IDC_BTN_LOGIN    108
#define IDC_BTN_REG      109

// Chat Controls: 200 - 220
#define IDC_TAB_MAIN     200
#define IDC_LIST_MAIN    201
#define IDC_EDIT_ACTION  202
#define IDC_BTN_ACTION1  203
#define IDC_BTN_ACTION2  204
#define IDC_LBL_TARGET   205
#define IDC_CHAT_HIST    206
#define IDC_EDIT_MSG     207
#define IDC_BTN_SEND     208

// ===========================
// GLOBAL STATE
// ===========================
SOCKET client_socket = INVALID_SOCKET;
char current_username[50] = "";
BOOL is_logged_in = FALSE;
char target_name[50] = "";
HFONT hFont;
HWND hMainWnd;

// Handles để thao tác nhanh
HWND hTabCtrl, hListMain, hChatHist, hEditMsg, hLblTarget;
HWND hEditHost, hEditPort, hEditUser, hEditPass;

// ===========================
// HELPER FUNCTIONS
// ===========================

// Hàm chuyển đổi giao diện (Ẩn Login hiện Chat và ngược lại)
void SwitchView(HWND hwnd, BOOL showChat) {
    int i;
    // Login Controls (100-110): Ẩn nếu showChat=TRUE
    for (i = 100; i <= 110; i++) {
        HWND hItem = GetDlgItem(hwnd, i);
        if (hItem) ShowWindow(hItem, showChat ? SW_HIDE : SW_SHOW);
    }
    // Chat Controls (200-220): Hiện nếu showChat=TRUE
    for (i = 200; i <= 220; i++) {
        HWND hItem = GetDlgItem(hwnd, i);
        if (hItem) ShowWindow(hItem, showChat ? SW_SHOW : SW_HIDE);
    }
}

void AppendChat(const char* text) {
    int len = GetWindowTextLength(hChatHist);
    SendMessage(hChatHist, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hChatHist, EM_REPLACESEL, 0, (LPARAM)text);
    SendMessage(hChatHist, EM_REPLACESEL, 0, (LPARAM)"\r\n");
    SendMessage(hChatHist, WM_VSCROLL, SB_BOTTOM, 0);
}

void UpdateList(const char* data) {
    SendMessage(hListMain, LB_RESETCONTENT, 0, 0);
    if (!data || strlen(data) == 0) return;
    char* temp = strdup(data);
    char* token = strtok(temp, ",");
    while (token) {
        if (strcmp(token, current_username) != 0) {
            SendMessage(hListMain, LB_ADDSTRING, 0, (LPARAM)token);
        }
        token = strtok(NULL, ",");
    }
    free(temp);
}

// ===========================
// NETWORK CORE
// ===========================

int SendPacket(const char* data, int len) {
    if (client_socket == INVALID_SOCKET) return -1;
    uint32_t net_len = htonl((uint32_t)len);
    if (send(client_socket, (char*)&net_len, sizeof(net_len), 0) <= 0) return -1;
    return send(client_socket, data, len, 0);
}

unsigned int __stdcall ReceiveThread(void* arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    uint32_t msg_length;
    int bytes_received;

    while (1) {
        int total = 0;
        while (total < sizeof(uint32_t)) {
            bytes_received = recv(client_socket, ((char *)&msg_length) + total, sizeof(uint32_t) - total, 0);
            if (bytes_received <= 0) goto disconnected;
            total += bytes_received;
        }
        msg_length = ntohl(msg_length);
        if (msg_length > BUFFER_SIZE - 1) msg_length = BUFFER_SIZE - 1;

        total = 0;
        while (total < (int)msg_length) {
            bytes_received = recv(client_socket, buffer + total, msg_length - total, 0);
            if (bytes_received <= 0) goto disconnected;
            total += bytes_received;
        }
        buffer[total] = '\0';
        char* msgCopy = strdup(buffer);
        PostMessage(hMainWnd, WM_SOCKET_DATA, 1, (LPARAM)msgCopy);
    }
disconnected:
    PostMessage(hMainWnd, WM_SOCKET_DATA, 0, (LPARAM)"Lost connection to server.");
    return 0;
}

BOOL ConnectToServer(const char* ip, int port) {
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    
    // Set timeout 3 giây cho connect để đỡ bị treo lâu
    // (Optional, giữ đơn giản thì connect blocking mặc định)
    
    if (connect(client_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) return FALSE;
    _beginthreadex(NULL, 0, ReceiveThread, NULL, 0, NULL);
    return TRUE;
}

void SendRequest(int type, const char* content, const char* to) {
    Message msg;
    create_response_message(&msg, type, current_username, to, content);
    char buffer[BUFFER_SIZE];
    int len = serialize_message(&msg, buffer, sizeof(buffer));
    SendPacket(buffer, len);
}

// ===========================
// PROCESS MESSAGE
// ===========================

void ProcessMessage(char* rawData) {
    Message msg;
    if (parse_message(rawData, &msg) < 0) return;
    char buf[2048];

    switch (msg.type) {
        case MSG_RESPONSE_SUCCESS:
        case MSG_SUCCESS:
            if (!is_logged_in) {
                if (strstr(msg.content, "Login successful")) {
                    is_logged_in = TRUE;
                    SwitchView(hMainWnd, TRUE); // Chuyển sang màn hình Chat
                    MessageBox(hMainWnd, "Login successful", "Notification", MB_OK);
                    SendRequest(MSG_GET_ONLINE_USERS, "", "");
                } else if (strstr(msg.content, "Registration successful")) {
                    MessageBox(hMainWnd, "Registration successful! Please log in.", "Notification", MB_OK);
                }
            }
            break;
        case MSG_PRIVATE_MESSAGE:
            sprintf(buf, "[%s]: %s", msg.from, msg.content);
            AppendChat(buf);
            break;
        case MSG_GET_ONLINE_USERS:
        case MSG_ONLINE_USERS_LIST:
            UpdateList(msg.content);
            break;
        case MSG_USER_ONLINE:
            sprintf(buf, "--- %s is Online ---", msg.content);
            AppendChat(buf);
            SendRequest(MSG_GET_ONLINE_USERS, "", "");
            break;
        case MSG_USER_OFFLINE:
            sprintf(buf, "--- %s is Offline ---", msg.content);
            AppendChat(buf);
            SendRequest(MSG_GET_ONLINE_USERS, "", "");
            break;
        case MSG_RESPONSE_ERROR:
        case MSG_ERROR:
            MessageBox(hMainWnd, msg.content, "Server Error", MB_ICONERROR);
            break;
    }
}

// ===========================
// GUI INIT
// ===========================

void InitGUI(HWND hwnd) {
    hFont = CreateFont(19,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Segoe UI");

    // --- LOGIN CONTROLS (ID 100-110) ---
    // Tất cả đều là child của 'hwnd' để nhận được sự kiện click
    CreateWindow("STATIC", "Host IP:", WS_CHILD|WS_VISIBLE, 250, 150, 100, 20, hwnd, (HMENU)IDC_LBL_HOST, NULL, NULL);
    hEditHost = CreateWindow("EDIT", "127.0.0.1", WS_CHILD|WS_VISIBLE|WS_BORDER, 350, 150, 200, 25, hwnd, (HMENU)IDC_EDIT_HOST, NULL, NULL);
    
    CreateWindow("STATIC", "Port:", WS_CHILD|WS_VISIBLE, 250, 180, 100, 20, hwnd, (HMENU)IDC_LBL_PORT, NULL, NULL);
    hEditPort = CreateWindow("EDIT", "8888", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER, 350, 180, 200, 25, hwnd, (HMENU)IDC_EDIT_PORT, NULL, NULL);

    CreateWindow("STATIC", "Username:", WS_CHILD|WS_VISIBLE, 250, 220, 100, 20, hwnd, (HMENU)IDC_LBL_USER, NULL, NULL);
    hEditUser = CreateWindow("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER, 350, 220, 200, 25, hwnd, (HMENU)IDC_EDIT_USER, NULL, NULL);

    CreateWindow("STATIC", "Password:", WS_CHILD|WS_VISIBLE, 250, 250, 100, 20, hwnd, (HMENU)IDC_LBL_PASS, NULL, NULL);
    hEditPass = CreateWindow("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_PASSWORD, 350, 250, 200, 25, hwnd, (HMENU)IDC_EDIT_PASS, NULL, NULL);

    CreateWindow("BUTTON", "LOGIN", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON, 250, 300, 145, 35, hwnd, (HMENU)IDC_BTN_LOGIN, NULL, NULL);
    CreateWindow("BUTTON", "REGISTER", WS_CHILD|WS_VISIBLE, 405, 300, 145, 35, hwnd, (HMENU)IDC_BTN_REG, NULL, NULL);

    // --- CHAT CONTROLS (ID 200-220) ---
    // Khởi tạo ẩn (WS_CHILD nhưng KHÔNG CÓ WS_VISIBLE)
    hTabCtrl = CreateWindow(WC_TABCONTROL, "", WS_CHILD, 10, 10, 200, 540, hwnd, (HMENU)IDC_TAB_MAIN, NULL, NULL);
    TCITEM tie; tie.mask = TCIF_TEXT; tie.pszText = "Online"; TabCtrl_InsertItem(hTabCtrl, 0, &tie);
    tie.pszText = "Friends"; TabCtrl_InsertItem(hTabCtrl, 1, &tie);
    tie.pszText = "Groups"; TabCtrl_InsertItem(hTabCtrl, 2, &tie);

    hListMain = CreateWindow("LISTBOX", "", WS_CHILD|WS_BORDER|LBS_NOTIFY, 20, 40, 180, 400, hwnd, (HMENU)IDC_LIST_MAIN, NULL, NULL);
    
    HWND hAct = CreateWindow("EDIT", "", WS_CHILD|WS_BORDER|ES_READONLY, 20, 450, 180, 25, hwnd, (HMENU)IDC_EDIT_ACTION, NULL, NULL);
    SendMessage(hAct, EM_SETCUEBANNER, TRUE, (LPARAM)L"Coming Soon...");
    CreateWindow("BUTTON", "+ Friend", WS_CHILD, 20, 480, 85, 25, hwnd, (HMENU)IDC_BTN_ACTION1, NULL, NULL);
    CreateWindow("BUTTON", "+ Group", WS_CHILD, 115, 480, 85, 25, hwnd, (HMENU)IDC_BTN_ACTION2, NULL, NULL);

    hLblTarget = CreateWindow("STATIC", "Select a user to chat...", WS_CHILD, 220, 10, 550, 20, hwnd, (HMENU)IDC_LBL_TARGET, NULL, NULL);
    hChatHist = CreateWindow("EDIT", "", WS_CHILD|WS_BORDER|ES_MULTILINE|WS_VSCROLL|ES_READONLY, 220, 35, 550, 465, hwnd, (HMENU)IDC_CHAT_HIST, NULL, NULL);
    hEditMsg = CreateWindow("EDIT", "", WS_CHILD|WS_BORDER, 220, 510, 450, 30, hwnd, (HMENU)IDC_EDIT_MSG, NULL, NULL);
    CreateWindow("BUTTON", "SEND", WS_CHILD|BS_DEFPUSHBUTTON, 680, 510, 90, 30, hwnd, (HMENU)IDC_BTN_SEND, NULL, NULL);

    // Set Font cho toàn bộ
    int i;
    for(i=100; i<=220; i++) {
        HWND h = GetDlgItem(hwnd, i);
        if(h) SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
}

// ===========================
// WINDOW PROC
// ===========================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        case WM_CREATE:
            InitGUI(hwnd);
            break;

        case WM_NOTIFY: 
            if (((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
                int idx = TabCtrl_GetCurSel(hTabCtrl);
                if (idx != 0) {
                    MessageBox(hwnd, "Feature under development!", "Info", MB_OK);
                    TabCtrl_SetCurSel(hTabCtrl, 0); 
                } else {
                    SendMessage(hListMain, LB_RESETCONTENT, 0, 0);
                    SendRequest(MSG_GET_ONLINE_USERS, "", "");
                }
            }
            break;

        case WM_COMMAND:
            // ListBox Select
            if (LOWORD(wParam) == IDC_LIST_MAIN && HIWORD(wParam) == LBN_SELCHANGE) {
                int idx = SendMessage(hListMain, LB_GETCURSEL, 0, 0);
                if (idx != LB_ERR) {
                    SendMessage(hListMain, LB_GETTEXT, idx, (LPARAM)target_name);
                    char buff[100]; sprintf(buff, "Chatting with: %s", target_name);
                    SetWindowText(hLblTarget, buff);
                }
            }

            // LOGIN / REGISTER BUTTONS
            if (LOWORD(wParam) == IDC_BTN_LOGIN || LOWORD(wParam) == IDC_BTN_REG) {
                // MessageBox(hwnd, "Debug: Đã bấm nút!", "Test", MB_OK); // Uncomment để test nếu cần
                
                char ip[30], portStr[10], user[50], pass[50];
                GetWindowText(hEditHost, ip, 30);
                GetWindowText(hEditPort, portStr, 10);
                GetWindowText(hEditUser, user, 50);
                GetWindowText(hEditPass, pass, 50);

                if (strlen(user) == 0 || strlen(pass) == 0) {
                    MessageBox(hwnd, "Please enter User/Pass", "Error", MB_OK); break;
                }

                if (client_socket == INVALID_SOCKET) {
                    if (!ConnectToServer(ip, atoi(portStr))) {
                        MessageBox(hwnd, "Cannot connect to Server!\nCheck IP/Port and ensure Server is running.", "Connection Error", MB_ICONERROR);
                        break;
                    }
                }

                char content[100]; sprintf(content, "%s|%s", user, pass);

                if (LOWORD(wParam) == IDC_BTN_LOGIN) {
                    SendRequest(MSG_LOGIN, content, "");
                    strcpy(current_username, user);
                } else {
                    SendRequest(MSG_REGISTER, content, "");
                }
            }

            // NÚT GỬI TIN
            if (LOWORD(wParam) == IDC_BTN_SEND) {
                char msgTxt[1024]; GetWindowText(hEditMsg, msgTxt, 1024);
                if (strlen(target_name) == 0) {
                    MessageBox(hwnd, "No recipient selected!", "Error", MB_OK);
                } else if (strlen(msgTxt) > 0) {
                    SendRequest(MSG_PRIVATE_MESSAGE, msgTxt, target_name);
                    char display[1100]; sprintf(display, "Me -> %s: %s", target_name, msgTxt);
                    AppendChat(display);
                    SetWindowText(hEditMsg, "");
                }
            }

            // BUTTONS COMING SOON
            if (LOWORD(wParam) == IDC_BTN_ACTION1 || LOWORD(wParam) == IDC_BTN_ACTION2) {
                MessageBox(hwnd, "Coming Soon!", "Info", MB_OK);
            }
            break;

        case WM_SOCKET_DATA:
            if (wParam == 0) {
                MessageBox(hwnd, (char*)lParam, "Network Error", MB_ICONERROR);
                is_logged_in = FALSE;
                SwitchView(hwnd, FALSE); // Return to Login
            } else {
                char* r = (char*)lParam; ProcessMessage(r); free(r);
            }
            break;

        case WM_CLOSE:
            // Gửi logout trước khi đóng cửa sổ
            if (is_logged_in && client_socket != INVALID_SOCKET) {
                SendRequest(MSG_LOGOUT, "", "");
                Sleep(100); // Đợi 100ms để gửi xong
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            // Cleanup socket và thoát
            if (client_socket != INVALID_SOCKET) {
                closesocket(client_socket);
                client_socket = INVALID_SOCKET;
            }
            WSACleanup();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR args, int nShow) {
    INITCOMMONCONTROLSEX icex; icex.dwSize = sizeof(icex); icex.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    WNDCLASS wc = {0}; wc.lpfnWndProc = WindowProc; wc.hInstance = hInst; wc.lpszClassName = "ChatClientGUI";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    hMainWnd = CreateWindow("ChatClientGUI", "Chat Online (Fixed Buttons)", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInst, NULL);
    ShowWindow(hMainWnd, nShow);
    MSG msg; while(GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
