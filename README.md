# MuEmu 0.97k (kayito) — MariaDB + launcher

Bản dựng lại của [nicomuratona/MuEmu-0.97k-kayito](https://github.com/nicomuratona/MuEmu-0.97k-kayito) để chạy với **MariaDB** (thay SQL Server 2008), build **không cần Visual Studio**, kèm một cửa sổ quản lý chung cho 4 server.

> Thư mục `Source/` là mã nguồn của tác giả gốc (repo gốc không ghi giấy phép), chỉ sửa phần hiển thị
> trong `ServerDisplayer.cpp` (xem commit "Restyle server window banner"). Mọi quyền thuộc về tác giả gốc.

## Cấu trúc

| Thư mục | Nội dung |
|---|---|
| `Source/` | Mã nguồn server (C++), client `Main.dll`, `InfoEncoder`, tool C# |
| `Launcher/` | `MuLauncher` — cửa sổ có tab cho ConnectServer / JoinServer / DataServer / GameServer |
| `Build/` | Script build bằng `cl.exe` (MSVC portable), không cần cài Visual Studio |

Không có trong repo (cố ý): thư mục `Client` (dữ liệu game ~170 MB), file `.exe` dựng sẵn, file cấu hình có mật khẩu, dữ liệu database.

## Database (MariaDB / MySQL)

Server có sẵn chế độ MySQL (`Release_MySQL`, macro `MYSQL`) dùng MySQL Connector/C++ 8.0.33.

1. Tạo một instance MariaDB riêng cho MU (ví dụ cổng 3308), tạo user riêng (đừng dùng root).
2. Import `MuServer/MySQL/DB/CreateDatabase.sql` rồi `PoblateDatabase.sql` của repo gốc.
3. Sửa `DataServer.ini` và `JoinServer.ini` (`DataBaseHost/Port/User/Pass/Name`).

## Build không cần Visual Studio

Dùng [portable-msvc](https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977) để tải MSVC + Windows SDK vào một thư mục
(cần chấp nhận giấy phép Visual Studio Build Tools; GameServer cần thêm gói ATL: `microsoft.vc.<ver>.atl.headers.base` và `...atl.x86.base`).
Sau đó chỉnh đường dẫn trong `Build/*.bat` và chạy:

```bat
Build\build-server.bat DataServer MYSQL MD
Build\build-server.bat JoinServer MYSQL MD
Build\build-server.bat ConnectServer NONE MT
Build\build-server.bat GameServer NONE MT
Build\build-main.bat
```

Các project ghi toolset `v141` nhưng build bằng `v143` vẫn được (không cần sửa source). Script mặc định dùng đường dẫn `C:\Muemu\...`.

## Launcher

`Launcher/*.cs`, build bằng `csc` có sẵn của .NET Framework:

```bat
cd Launcher
C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe /target:winexe /platform:x86 /codepage:65001 ^
  /out:MuLauncher.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll ^
  /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll ^
  MuLauncher.cs Ui.cs Ui2.cs Features.cs Logs.cs Accounts.cs Tools.cs Db.cs
```

Đặt `MuLauncher.exe` trong thư mục `MuServer` (cạnh ConnectServer, JoinServer, DataServer, GameServer, `mariadb`).
Tham số: `--autostart` (tự chạy tất cả), `--tray` (mở dưới khay hệ thống và tự chạy tất cả).

- **4 tab server**: nhúng cửa sổ của từng server, dựng lại menu gốc của server, nút chạy/tắt/tách cửa sổ.
- **Mở/Tắt game** (client trong `..\Client`).
- **Nhật ký**: xem file log của 4 server và launcher, lọc, chỉ dòng lỗi, tự cập nhật.
- **Tài khoản**: danh sách, tạo, đổi mật khẩu, đặt level, khóa/mở khóa, xem ai đang online (đọc `MEMB_INFO`/`MEMB_STAT`).
- **Công cụ**: sao lưu / khôi phục database (`mariadb-dump`, giữ N bản gần nhất, tự sao lưu khi tắt hoặc định kỳ),
  tự khởi động lại server khi sập, thu nhỏ xuống khay, chạy cùng Windows, đổi địa chỉ server cho client
  (sửa `ServerList.dat` + `MainInfo.ini`, chạy `InfoEncoder`), mở firewall, nén gói client `.zip`.

Launcher gọi `mariadb.exe`/`mariadb-dump.exe` với file `mariadb\mu.cnf` (user riêng, không đưa vào repo) và `mariadb\root.cnf` (khôi phục cần quyền root vì stored procedure có `DEFINER=root`).

## Giao diện client (Main.dll)

Giao diện đăng nhập và các bảng trong game được vẽ lại theo phong cách phẳng ("digital"):

- **Đăng nhập** (`LoginScale.cpp`, `LoginRemember.cpp`): form thu gọn ở giữa màn hình, ô "Remember me", danh sách tài khoản đã lưu
  (mã hóa DPAPI, file `LoginAccounts.dat` cạnh client). Chọn server có khoảng cách giữa nút nhóm và danh sách (`ServerList.cpp`).
- **Menu Esc / Options** (`OptionsMenu.cpp`): một cửa sổ có sidebar (General, Antilag, Screen, Font, Close), độ phân giải xếp 2 cột, nút mũi tên phẳng.
- **Event Timer (H) / Teleport (M)** (`EventTimer.cpp`, `MoveList.cpp`): bảng phẳng, nút Close, hàng sáng khi rê chuột.
- **Chat** (`Interface.cpp`): khung chat ở góc dưới trái, khung nhập chat phẳng (ô chat, ô thì thầm, nút gửi) ngay bên dưới.
  Khung nền chỉ hiện khi đang mở ô nhập chat. Vị trí chỉnh bằng `CHAT_LOG_POS_X/Y`, `CHAT_INPUT_Y`, `CHAT_LOG_WIDTH`.

Ảnh skin được sinh bằng `Tools/ClientSkin` (`make_skin7.py` cho màn hình đăng nhập, `make_menu_skin.py` cho menu Esc và hộp thoại;
cần Pillow). Build bình thường bằng `Build/build-main.bat`; `Build/build-main-trace.bat` bật `BITMAP_TRACE` để ghi lại các lệnh vẽ khi cần dò vị trí giao diện.
