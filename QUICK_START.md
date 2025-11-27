# 🚀 QUICK START - Test ChatOnline

## Build & Run trong 3 bước

### Bước 1: Build
```cmd
build_all.bat
```

### Bước 2: Chạy Server
```cmd
cd server
chat_server.exe
```

### Bước 3: Chạy Client (Terminal mới)
```cmd
cd client
client.exe
```

---

## Test Scenario Nhanh

### 1. Đăng ký & Đăng nhập
```
Client 1:
  Menu → 1 (Register) → alice / pass123
  Menu → 2 (Login) → alice / pass123

Client 2 (Terminal mới):
  Menu → 1 (Register) → bob / pass456
  Menu → 2 (Login) → bob / pass456
```

### 2. Chat riêng
```
Client 1 (alice):
  Menu → 4 (Send Private Message)
  To: bob
  Message: Hi Bob!

Client 2 (bob):
  → Sẽ nhận được message!
```

### 3. Tạo Group
```
Client 1 (alice):
  Menu → 6 (Create Group)
  Group name: TestGroup

Client 2 (bob):
  Menu → 7 (Join Group)
  Group name: TestGroup
```

### 4. Chat nhóm
```
Client 1 (alice):
  Menu → 5 (Send Group Message)
  Group name: TestGroup
  Message: Hello everyone!

Client 2 (bob):
  → Sẽ nhận được message trong nhóm!
```

### 5. Xem Online Users
```
Menu → 14 (View Online Users)
→ Sẽ thấy: alice, bob
```

---

## ⚠️ Lưu Ý

- **Port 8888**: Phải available
- **Firewall**: Allow khi Windows hỏi
- **Login trước**: Mới dùng được các chức năng khác

---

## 🐛 Troubleshooting

**Build lỗi?**
```cmd
gcc --version
```

**Port busy?**
```cmd
netstat -ano | findstr :8888
taskkill /PID <pid> /F
```

**Không kết nối?**
- Check server đã chạy chưa
- Check Windows Firewall

---

**Happy Testing! 🎉**
