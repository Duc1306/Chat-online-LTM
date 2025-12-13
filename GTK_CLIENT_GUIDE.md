# ChatOnline GTK Client - Hướng dẫn sử dụng

## Tổng quan

Client GTK là giao diện đồ họa đầy đủ chức năng cho ChatOnline trên Linux, sử dụng thư viện GTK+3.

## Cài đặt dependencies

```bash
# Ubuntu/Debian
sudo apt install libgtk-3-dev pkg-config

# Fedora
sudo dnf install gtk3-devel pkgconfig

# Arch Linux
sudo pacman -S gtk3 pkgconfig
```

## Build

```bash
make client_gtk
```

## Chạy

```bash
cd client
./client_gtk
```

hoặc

```bash
make run-client-gtk
```

## Các chức năng

### 1. Đăng nhập/Đăng ký
- Nhập Host IP (mặc định: 127.0.0.1)
- Nhập Port (mặc định: 8888)
- Nhập Username và Password
- Click "LOGIN" để đăng nhập hoặc "REGISTER" để đăng ký tài khoản mới

### 2. Chat riêng tư (Private Chat)
- Chọn tab "Online" để xem danh sách người dùng online
- Click vào tên người dùng để chọn
- Nhập tin nhắn vào ô input phía dưới
- Click "SEND" hoặc nhấn Enter để gửi

### 3. Quản lý bạn bè
- Chọn tab "Friends" để xem danh sách bạn bè
- Nhập username vào ô "Username"
- Click "Add Friend" để gửi lời mời kết bạn

### 4. Quản lý nhóm
- Chọn tab "Groups" để xem danh sách nhóm
- Nhập tên nhóm vào ô "Group name"
- Click "Create Group" để tạo nhóm mới

### 5. Nhận thông báo
- Thông báo người dùng online/offline
- Lời mời kết bạn
- Tin nhắn offline (khi đăng nhập lại)
- Thông báo từ server

## Giao diện

```
┌─────────────────────────────────────────────────────────┐
│                  ChatOnline - GTK Client                │
├──────────────┬──────────────────────────────────────────┤
│              │  Chatting with: [username]               │
│   Online     ├──────────────────────────────────────────┤
│   Friends    │                                          │
│   Groups     │         Chat History Area                │
│              │                                          │
│ User List    │                                          │
│   - user1    │                                          │
│   - user2    │                                          │
│   - user3    ├──────────────────────────────────────────┤
│              │  [Type message...]          [SEND]       │
│ [Username ]  │                                          │
│ [Add Friend] │                                          │
└──────────────┴──────────────────────────────────────────┘
```

## Troubleshooting

### Lỗi: cannot open display
```bash
export DISPLAY=:0
./client_gtk
```

### Lỗi: GTK không tìm thấy
Cài đặt lại GTK+3:
```bash
sudo apt install libgtk-3-0 libgtk-3-dev
```

### Client không kết nối được
1. Kiểm tra server đang chạy
2. Kiểm tra IP và Port đúng
3. Kiểm tra firewall:
```bash
sudo ufw allow 8888/tcp
```

## Tính năng hiện có

✅ Đăng ký/Đăng nhập  
✅ Chat riêng tư 1-1  
✅ Danh sách người dùng online  
✅ Thông báo online/offline  
✅ Quản lý bạn bè (gửi lời mời)  
✅ Tạo nhóm chat  
✅ Nhận tin nhắn offline  
✅ Giao diện đẹp, dễ sử dụng

## Tính năng đang phát triển

🔨 Chat nhóm đầy đủ  
🔨 Chấp nhận/từ chối lời mời kết bạn  
🔨 Truyền file  
🔨 Emoji và sticker  
🔨 Thông báo desktop  

## Ghi chú kỹ thuật

- Sử dụng GTK+3 cho giao diện
- Thread-safe với g_idle_add cho UI updates
- Hỗ trợ cả X11 và Wayland
- Tự động cuộn xuống tin nhắn mới
- Xử lý phân mảnh TCP đầy đủ
