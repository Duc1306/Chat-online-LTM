# ChatOnline - Linux Build Guide

## Hệ thống yêu cầu

- **OS**: Linux (Ubuntu, Debian, Fedora, etc.)
- **Compiler**: GCC với hỗ trợ C99 hoặc mới hơn
- **Libraries**: pthread (thường đã có sẵn)

## Cài đặt Dependencies

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential gcc make libgtk-3-dev pkg-config
```

### Fedora/RHEL
```bash
sudo dnf install gcc make gtk3-devel pkgconfig
```

### Arch Linux
```bash
sudo pacman -S base-devel gcc make gtk3 pkgconfig
```

## Build

### Build tất cả (Server + Client Console + Client GTK GUI)
```bash
make
```

hoặc

```bash
make all
```

### Build riêng lẻ
```bash
# Chỉ build server
make server

# Chỉ build console client
make client

# Chỉ build GTK GUI client
make client_gtk
```

### Xóa file build
```bash
make clean
```

## Chạy ứng dụng

### 1. Khởi động Server
Mở terminal thứ nhất:
```bash
cd server
./chat_server
```

Hoặc dùng Makefile:
```bash
make run-server
```

Server sẽ lắng nghe trên port 8888.

### 2. Khởi động Client
Mở terminal thứ hai (hoặc nhiều terminal cho nhiều client):

**Console Client:**
```bash
cd client
./client
```

Hoặc dùng Makefile:
```bash
make run-client
```

**GTK GUI Client (khuyến nghị):**
```bash
cd client
./client_gtk
```

Hoặc dùng Makefile:
```bash
make run-client-gtk
```

## Tính năng

- ✅ Đăng ký & Đăng nhập
- ✅ Chat riêng tư (1-1)
- ✅ Chat nhóm
- ✅ Quản lý bạn bè
- ✅ Danh sách người dùng online
- ✅ Đồng bộ tin nhắn offline
- ✅ Truyền file

## Cấu trúc thư mục

```
ChatOnline/
├── Makefile              # Build script cho Linux
├── BUILD_LINUX.md        # Hướng dẫn này
├── client/
│   ├── client            # Console client executable (sau khi build)
│   ├── client_gtk        # GTK GUI client executable (sau khi build)
│   ├── client.c          # Console client source code
│   ├── client_gtk.c      # GTK GUI client source code
│   ├── protocol.c        # Protocol implementation
│   └── protocol.h        # Protocol headers
└── server/
    ├── chat_server       # Server executable (sau khi build)
    ├── server.c          # Server main
    ├── server.h          # Server headers
    ├── server_handlers.c # Message handlers
    ├── server_utils.c    # Utility functions
    ├── users.txt         # User database
    ├── friendships.txt   # Friend relationships
    ├── groups.txt        # Group data
    └── offline_messages.txt # Offline messages
```

## Khắc phục sự cố

### Lỗi: "Permission denied" khi chạy
```bash
chmod +x server/chat_server
chmod +x client/client
```

### Lỗi: "Address already in use"
```bash
# Tìm và kill process đang dùng port 8888
sudo lsof -i :8888
sudo kill -9 <PID>
```

### Lỗi: "pthread not found"
```bash
# Đảm bảo compile với flag -pthread
gcc ... -pthread
```

### Lỗi: "Package gtk+-3.0 was not found"
```bash
# Ubuntu/Debian
sudo apt install libgtk-3-dev pkg-config

# Fedora
sudo dnf install gtk3-devel pkgconfig

# Arch
sudo pacman -S gtk3 pkgconfig
```

### Client GTK không hiển thị
```bash
# Kiểm tra biến môi trường DISPLAY
echo $DISPLAY

# Nếu trống, set lại (cho X11)
export DISPLAY=:0

# Hoặc dùng Wayland (GTK tự động hỗ trợ)
```

## Thay đổi từ Windows

Code đã được chuyển đổi hoàn toàn từ Windows sang Linux:

1. **Sockets**: Winsock2 → Berkeley sockets (sys/socket.h)
2. **Threads**: Windows threads → POSIX threads (pthread)
3. **Mutex**: CRITICAL_SECTION → pthread_mutex_t
4. **Console Colors**: Windows Console API → ANSI escape codes
5. **Build**: batch file (.bat) → Makefile
6. **Network Init**: WSAStartup/WSACleanup → không cần thiết trên Linux

## Lưu ý

- **GUI Client**: Client GTK (client_gtk.c) là phiên bản GUI hoàn chỉnh cho Linux dùng GTK+3. Console client (client.c) vẫn có sẵn cho môi trường terminal.
- **Port mặc định**: 8888 (có thể thay đổi trong protocol.h)
- **File data**: Server lưu data vào thư mục server/
- **Dependencies**: GTK+3 cần thiết cho GUI client. Console client không cần GTK.

## Liên hệ & Đóng góp

Nếu gặp vấn đề hoặc muốn đóng góp, vui lòng tạo issue hoặc pull request trên repository.
