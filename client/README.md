# ChatOnline Client - Full-Featured

## 📋 Tổng Quan

Client hoàn chỉnh với đầy đủ chức năng chat, group, friends, và giao diện console đẹp mắt.

## ✨ Tính Năng

### 🔐 Account Management
- ✅ Register (Đăng ký tài khoản mới)
- ✅ Login (Đăng nhập)
- ✅ Logout (Đăng xuất)

### 💬 Private Chat
- ✅ Send Private Message (Gửi tin nhắn 1-1)
- ✅ Receive messages in real-time
- ✅ Offline messages sync

### 👥 Group Chat
- ✅ Create Group (Tạo nhóm)
- ✅ Join Group (Tham gia nhóm)
- ✅ Leave Group (Rời nhóm)
- ✅ Send Group Message (Gửi tin nhắn nhóm)
- ✅ View Groups List (Xem danh sách nhóm)

### 🤝 Friend Management
- ✅ Send Friend Request (Gửi lời mời kết bạn)
- ✅ Accept Friend Request (Chấp nhận lời mời)
- ✅ Reject Friend Request (Từ chối lời mời)
- ✅ Remove Friend (Xóa bạn)
- ✅ View Friends List (Xem danh sách bạn bè)

### 📊 Lists & Info
- ✅ View Online Users (Xem người dùng online)
- ✅ View Groups (Xem danh sách nhóm)
- ✅ View Friends (Xem danh sách bạn)

### 🎨 UI Features
- ✅ Colorful console interface
- ✅ Beautiful menus và headers
- ✅ Real-time message notifications
- ✅ Status indicators (online/offline)
- ✅ Emojis và symbols

## 🏗️ Build

### Quick Build:
```cmd
gcc -o client.exe client.c protocol.c -lws2_32 -Wall
```

### Hoặc từ thư mục gốc:
```cmd
build_client.bat
```

## ▶️ Run

```cmd
client.exe
```

Hoặc kết nối tới server khác:
```cmd
client.exe 192.168.1.100 8888
```

## 📖 Hướng Dẫn Sử Dụng

### 1. Đăng Ký & Đăng Nhập
```
Menu > 1. Register
Username: myname
Password: mypass

Menu > 2. Login
Username: myname
Password: mypass
```

### 2. Chat Riêng Tư
```
Menu > 4. Send Private Message
To: friend_name
Message: Hello!
```

### 3. Tạo & Tham Gia Group
```
Menu > 6. Create Group
Group name: MyGroup

Menu > 7. Join Group
Group name: MyGroup
```

### 4. Chat Nhóm
```
Menu > 5. Send Group Message
Group name: MyGroup
Message: Hi everyone!
```

## 🎮 Menu

| # | Chức năng |
|---|-----------|
| 1 | Register |
| 2 | Login |
| 3 | Logout |
| 4 | Send Private Message |
| 5 | Send Group Message |
| 6 | Create Group |
| 7 | Join Group |
| 8 | Leave Group |
| 9 | Send Friend Request |
| 10 | Accept Friend Request |
| 11 | Reject Friend Request |
| 12 | Remove Friend |
| 13 | View Friends |
| 14 | View Online Users |
| 15 | View Groups |
| 0 | Exit |

## 📁 Files

```
client/
├── client.c      # Main client code (full-featured)
├── protocol.c    # Protocol implementation
├── protocol.h    # Protocol headers
└── client.exe    # Compiled executable
```

## 🎨 Colors

- 🟢 Green: Success, online
- 🔴 Red: Errors, offline
- 🟡 Yellow: Info, warnings
- 🔵 Blue: Your messages
- 🟣 Purple: Private messages
- 🔷 Cyan: Group messages, headers

## 💡 Tips

1. ✅ Đăng nhập trước khi dùng chức năng khác
2. ✅ Username phải unique
3. ✅ Tin nhắn offline sẽ nhận khi login
4. ✅ Check online users trước khi chat

## 🔧 Troubleshooting

**Không kết nối được:**
- Kiểm tra server đã chạy
- Check port 8888
- Windows Firewall

**Build lỗi:**
```cmd
gcc --version
gcc -v -o client.exe client.c protocol.c -lws2_32
```

## 🚀 Enjoy Chatting! 💬
