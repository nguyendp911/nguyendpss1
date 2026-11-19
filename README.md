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

`Launcher/MuLauncher.cs` + `Ui.cs`, build bằng `csc` có sẵn của .NET Framework:

```bat
C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe /target:winexe /platform:x86 /codepage:65001 ^
  /out:MuLauncher.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll MuLauncher.cs Ui.cs
```

Đặt `MuLauncher.exe` trong thư mục `MuServer` (cạnh ConnectServer, JoinServer, DataServer, GameServer). Launcher nhúng cửa sổ của từng server vào tab, dựng lại menu gốc của server, và có nút mở/tắt game.
