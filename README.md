# 💬 ChatOnline - Full-Featured Chat Application

**Client-Server Chat Application for Windows**

Ứng dụng chat C/C++ hoàn chỉnh với TCP Socket, hỗ trợ chat 1-1, chat nhóm, quản lý bạn bè.

---

## ✨ Tính Năng

### 🔐 Account
- Register, Login, Logout
- User authentication
- Password protection

### 💬 Chat
- **Private Chat** (1-1): Gửi tin nhắn riêng tư
- **Group Chat**: Tạo nhóm, tham gia, chat nhiều người
- **Offline Messages**: Nhận tin nhắn khi offline

### 👥 Friends
- Gửi/nhận friend request
- Accept/reject friend request
- Xóa bạn bè
- Xem danh sách bạn

### 📊 Lists
- Xem người dùng online
- Xem danh sách nhóm
- Xem thành viên nhóm

---

## 📁 Cấu Trúc Project

```
ChatOnline/
├── server/                    # Server code
│   ├── server.c              # Main server
│   ├── server_handlers.c     # Message handlers
│   ├── server_utils.c        # Utilities
│   ├── server.h              # Header
│   ├── users.txt             # User database
│   ├── groups.txt            # Groups database
│   ├── friendships.txt       # Friends database
│   └── offline_messages.txt  # Offline messages
│
├── client/                    # Client code & Protocol
│   ├── client.c              # Full-featured client
│   ├── protocol.c            # Protocol implementation (shared)
│   ├── protocol.h            # Protocol headers (shared)
│   └── README.md             # Client guide
│
├── build_server.bat          # Build server script
├── build_client.bat          # Build client script
├── build_all.bat             # Build all script
├── QUICK_START.md            # Quick start guide
└── README.md                 # This file
```

---

## 🚀 Quick Start

### 1️⃣ Yêu Cầu
- **Windows 7+** (Windows 10/11 recommended)
- **GCC compiler** (MinGW-w64 hoặc Visual Studio)
- **Port 8888** (default)

### 2️⃣ Build

**Cách 1: Build Script (Khuyên dùng)**
```cmd
build_all.bat
```

**Cách 2: Build Thủ Công**
```cmd
REM Build Server (dùng protocol từ client)
cd server
gcc -o chat_server.exe server.c server_handlers.c server_utils.c ..\client\protocol.c -I..\client -lws2_32 -Wall

REM Build Client
cd client
gcc -o client.exe client.c protocol.c -lws2_32 -Wall
```

### 3️⃣ Chạy

**Terminal 1 - Server:**
```cmd
cd server
chat_server.exe
```

**Terminal 2+ - Client(s):**
```cmd
cd client
client.exe
```

Hoặc kết nối từ xa:
```cmd
client.exe <server_ip> [port]
```

---

## 🌐 Kết Nối Từ Xa (Remote Connection)

### 📌 3 Phương Pháp Kết Nối

#### 1️⃣ **Localhost** (Cùng máy)
```cmd
client.exe
# Chọn option [1] → 127.0.0.1
```
✅ Test local, debug

#### 2️⃣ **LAN** (Cùng mạng wifi/router)
```cmd
# Máy Server: Xem IP LAN
ipconfig
# → Tìm IPv4 Address: 192.168.1.100

# Máy Client:
client.exe 192.168.1.100
# Hoặc chọn option [2] → nhập IP
```
✅ Cùng nhà/văn phòng

#### 3️⃣ **Tailscale** (Khác mạng, từ xa) ⭐ Khuyến nghị
```cmd
# Cài Tailscale trên cả 2 máy
# https://tailscale.com/download

# Máy Server: Xem IP Tailscale
tailscale ip -4
# → 100.89.12.34

# Máy Client:
client.exe 100.89.12.34
# Hoặc chọn option [3] → nhập IP Tailscale
```
✅ An toàn, không cần port forwarding
📖 **Chi tiết**: Xem `TAILSCALE_GUIDE.md`

---

### 🔥 Windows Firewall

**Quan trọng**: Cho phép server qua firewall:

**Cách 1: Command Line (Nhanh)**
```cmd
netsh advfirewall firewall add rule name="ChatServer" dir=in action=allow protocol=TCP localport=8888
```

**Cách 2: GUI**
1. Settings → Firewall & Network Protection
2. "Allow an app through firewall"
3. Thêm `chat_server.exe`
4. Cho phép **Private networks**

---

### 4️⃣ Sử Dụng

1. **Đăng ký**: Menu → 1 → Nhập username/password
2. **Đăng nhập**: Menu → 2 → Nhập username/password
3. **Chat**: Menu → 4 → Nhập tên người nhận và tin nhắn
4. **Tạo group**: Menu → 6 → Nhập tên nhóm
5. **Gửi tin nhóm**: Menu → 5 → Chọn nhóm và gửi

---

## 📖 Client Menu

```
╔══════════════════════════════════════════════════════════╗
║              CHATONLINE CLIENT - MAIN MENU               ║
╠══════════════════════════════════════════════════════════╣
║  ACCOUNT:                                                ║
║   1. Register          2. Login          3. Logout       ║
║  CHAT:                                                   ║
║   4. Send Private Message      5. Send Group Message     ║
║  GROUP:                                                  ║
║   6. Create Group      7. Join Group     8. Leave Group  ║
║  FRIENDS:                                                ║
║   9. Send Friend Request       10. Accept Friend Request ║
║   11. Reject Friend Request    12. Remove Friend         ║
║   13. View Friends List                                  ║
║  LISTS:                                                  ║
║   14. View Online Users        15. View Groups           ║
║  SYSTEM:                                                 ║
║   0. Exit                                                ║
╚══════════════════════════════════════════════════════════╝
```

---

## 🔧 Kiến Trúc Kỹ Thuật

### Network
- **Protocol**: TCP/IP
- **Socket**: Windows Sockets 2 (Winsock2)
- **Port**: 8888 (default)
- **Threading**: Windows Threads (`_beginthreadex`)

### Message Protocol
```
Format: TYPE|FROM:username|TO:recipient|CONTENT:text|TIME:timestamp|EXTRA:data

Example: 30|FROM:alice|TO:bob|CONTENT:Hello!|TIME:2025-11-26 10:00:00|EXTRA:
```

### Message Types
| Code | Type | Description |
|------|------|-------------|
| 1 | MSG_REGISTER | Đăng ký |
| 2 | MSG_LOGIN | Đăng nhập |
| 3 | MSG_LOGOUT | Đăng xuất |
| 30 | MSG_PRIVATE_MESSAGE | Chat riêng |
| 40 | MSG_GROUP_CREATE | Tạo nhóm |
| 44 | MSG_GROUP_MESSAGE | Chat nhóm |
| 20-24 | MSG_FRIEND_* | Quản lý bạn bè |
| 70-72 | MSG_GET_* | Lấy danh sách |

**Full list**: Xem `client/protocol.h`

### Database
- **File-based**: Text files (users.txt, groups.txt, ...)
- **Format**: Pipe-delimited (`|`)
- **Thread-safe**: Mutex protection

---

## 🎨 Client Features

### Console UI
- ✅ **Colorful interface**: Colors cho các loại message khác nhau
- ✅ **Beautiful menus**: Box-drawing characters
- ✅ **Real-time updates**: Nhận message ngay lập tức
- ✅ **Status indicators**: Online/offline, typing indicators
- ✅ **Emojis**: 🟢 🔴 💬 👋 📬

### Colors
- 🟢 **Green**: Success, online users
- 🔴 **Red**: Errors, offline users
- 🟡 **Yellow**: Info, warnings
- 🔵 **Blue**: Your sent messages
- 🟣 **Purple**: Private messages received
- 🔷 **Cyan**: Group messages, headers

---

## 📚 Documentation

### Build & Setup
- **`BUILD_WINDOWS.md`**: Chi tiết cách build, troubleshooting

### Client Development
- **`client/README.md`**: Hướng dẫn sử dụng client

### Protocol
- **`client/protocol.h`**: Message types, constants, structures (shared với server)

---

## 🔥 Test Scenario

### Test 1: Basic Chat
```
1. Chạy server
2. Chạy 2 clients
3. Client 1: Register → Login (alice/pass123)
4. Client 2: Register → Login (bob/pass456)
5. Client 1: Send private message to bob → "Hello Bob!"
6. Client 2: Should receive message
```

### Test 2: Group Chat
```
1. Client 1 (alice): Create group "TestGroup"
2. Client 2 (bob): Join group "TestGroup"
3. Client 1: Send group message "Hi everyone!"
4. Client 2: Should receive message in group
```

### Test 3: Offline Messages
```
1. Client 1 (alice): Login
2. Client 1: Send message to bob (bob is offline)
3. Client 2 (bob): Login → Should receive offline message
```

### Test 4: Friends
```
1. Client 1 (alice): Send friend request to bob
2. Client 2 (bob): Accept friend request
3. Both: View friends list → Should see each other
```

---

## ⚠️ Troubleshooting

### Build Errors

**Error: `ws2_32.lib not found`**
```cmd
# Solution: Install MinGW-w64 hoặc Visual Studio
# Check: gcc --version
```

**Error: `cannot find -lws2_32`**
```cmd
# Solution: Use full MinGW-w64 installation
# Not: MinGW (old version)
```

### Runtime Errors

**Server: "Address already in use"**
```cmd
# Find process using port 8888
netstat -ano | findstr :8888

# Kill process
taskkill /PID <process_id> /F
```

**Client: "Connection failed"**
- ✅ Check server đã chạy chưa
- ✅ Check port 8888 không bị firewall block
- ✅ Check IP address (127.0.0.1 cho localhost)

**Client: "Not receiving messages"**
- ✅ Đảm bảo đã login
- ✅ Restart client
- ✅ Check server logs

### Windows Firewall

Lần đầu chạy, Windows sẽ hỏi:
- ✅ **Allow** cho Private networks
- ⚠️ **Block** cho Public networks (nếu chỉ test local)

---

## 🛠️ Development Setup

### MinGW-w64 Installation

1. Download MSYS2: https://www.msys2.org/
2. Install và mở MSYS2 MinGW 64-bit terminal
3. Install GCC:
```bash
pacman -S mingw-w64-x86_64-gcc
```
4. Add to PATH: `C:\msys64\mingw64\bin`

### Visual Studio Setup

1. Install Visual Studio 2022 Community
2. Select: "Desktop development with C++"
3. Create Console App project
4. Add files: `client.c`, `protocol.c`, `protocol.h`
5. Project Properties → Linker → Input → Add `ws2_32.lib`

---

## 📊 Project Stats

- **Language**: C
- **Lines of Code**: ~3000+
- **Platform**: Windows (7/10/11)
- **Socket Type**: TCP/IP
- **Architecture**: Multi-threaded Client-Server
- **Database**: File-based text files

---

## 🎯 Future Enhancements

Có thể mở rộng:
- [ ] File transfer (already implemented in server)
- [ ] Voice chat
- [ ] Video chat
- [ ] GUI client (WinForms/WPF)
- [ ] Encryption
- [ ] Database (SQLite/MySQL)
- [ ] Web client
- [ ] Mobile app

---




For detailed documentation, see:
- `BUILD_WINDOWS.md` - Build guide
- `client/README.md` - Client guide
- `shared/protocol.h` - Protocol reference
