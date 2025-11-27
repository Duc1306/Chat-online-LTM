# 🔒 Hướng dẫn kết nối 2 máy từ xa bằng Tailscale

## 📌 Tổng quan

Tailscale tạo mạng ảo riêng (VPN) giữa các thiết bị, cho phép chúng kết nối với nhau như thể cùng mạng LAN, **không cần port forwarding**.

---

## 🚀 Cài đặt Tailscale

### Windows (Cả 2 máy)
1. Tải Tailscale: https://tailscale.com/download/windows
2. Cài đặt file `.msi`
3. Đăng nhập bằng Google/Microsoft/GitHub
4. ✅ Xong! Máy đã có IP Tailscale

### Linux
```bash
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up
```

### macOS
```bash
brew install tailscale
sudo tailscale up
```

---

## 📋 Các bước thực hiện

### 🖥️ **Máy 1 (Server)**

1. **Kiểm tra IP Tailscale:**
   ```cmd
   tailscale ip -4
   ```
   VD: `100.89.12.34`

2. **Chạy server:**
   ```cmd
   cd server
   chat_server.exe
   ```
   Server lắng nghe trên `0.0.0.0:8888` (tất cả interfaces)

3. **Kiểm tra Windows Firewall (quan trọng!):**
   - Mở Settings → Firewall & Network Protection
   - Nhấn "Allow an app through firewall"
   - Thêm `chat_server.exe`, cho phép **Private networks**

---

### 💻 **Máy 2 (Client)**

1. **Kiểm tra kết nối Tailscale:**
   ```cmd
   tailscale status
   ```
   Phải thấy máy Server trong danh sách

2. **Ping thử:**
   ```cmd
   ping 100.89.12.34
   ```
   (Thay bằng IP Tailscale của Server)

3. **Chạy client:**
   ```cmd
   cd client
   client.exe
   ```

4. **Khi client hỏi:**
   ```
   Choose connection type [1-3]: 3
   Enter server IP address: 100.89.12.34
   ```

---

## ✅ Test thành công

Nếu kết nối OK, bạn sẽ thấy:
```
✓ Connected to server!
```

---

## 🔧 Troubleshooting

### ❌ "Connection failed"

**1. Kiểm tra Tailscale đã chạy:**
```cmd
tailscale status
```
Phải thấy `●` (xanh) ở cả 2 máy

**2. Kiểm tra server đã start:**
Trên máy server:
```cmd
netstat -an | findstr 8888
```
Phải thấy: `0.0.0.0:8888` hoặc `:::8888`

**3. Tắt Windows Firewall tạm:**
```cmd
netsh advfirewall set allprofiles state off
```
(Nhớ bật lại sau khi test!)

**4. Thử ping:**
```cmd
# Từ Client ping Server
ping <Server_Tailscale_IP>

# Từ Server ping Client
ping <Client_Tailscale_IP>
```

**5. Kiểm tra port có bị block:**
Trên Client:
```cmd
telnet <Server_IP> 8888
```
Nếu kết nối được → Port OK
Nếu không → Firewall đang block

---

## 📊 So sánh các phương pháp

| Phương pháp | Ưu điểm | Nhược điểm | Khuyến nghị |
|-------------|---------|------------|-------------|
| **Localhost** | Đơn giản, nhanh | Chỉ cùng máy | Test local ✅ |
| **LAN** | Nhanh, miễn phí | Phải cùng wifi | Cùng nhà/văn phòng ✅ |
| **Tailscale** | Mọi nơi, bảo mật | Cần cài đặt | Từ xa, khác mạng ✅✅✅ |
| **Port Forward** | Không cần app | Phức tạp, rủi ro | Không khuyến nghị ⚠️ |

---

## 🎯 Các lệnh hữu ích

```cmd
# Xem IP Tailscale
tailscale ip

# Xem trạng thái
tailscale status

# Xem danh sách thiết bị
tailscale status --peers

# Logout
tailscale logout

# Login lại
tailscale login
```

---

## 🌐 Sử dụng trên mạng LAN (không cần Tailscale)

Nếu 2 máy **cùng wifi/mạng**:

1. **Máy Server - Xem IP LAN:**
   ```cmd
   ipconfig
   ```
   Tìm dòng `IPv4 Address`: VD `192.168.1.100`

2. **Máy Client - Kết nối:**
   ```cmd
   client.exe 192.168.1.100
   ```
   Hoặc chọn option [2] khi chạy

---

## 🔐 Bảo mật

Tailscale sử dụng:
- ✅ WireGuard protocol (nhanh, bảo mật)
- ✅ End-to-end encryption
- ✅ Không lưu trữ dữ liệu của bạn
- ✅ Miễn phí cho personal use (tối đa 100 thiết bị)

---

## 📞 Hỗ trợ

- Tailscale Docs: https://tailscale.com/kb
- GitHub Issues: https://github.com/tailscale/tailscale/issues
- Discord: https://tailscale.com/contact/support

---

**✨ Chúc bạn chat vui vẻ từ mọi nơi!**
