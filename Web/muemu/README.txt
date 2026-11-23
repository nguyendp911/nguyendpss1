=== HUONG DAN CAI DAT SEPAY WEBHOOK TAI https://vie.info.vn/muemu ===

1. Upload toan bo thu muc nay len host:
   public_html/muemu/
     - config.php
     - webhook.php
     - index.php
     - .htaccess

2. Mo file config.php va dien thong tin that:
   - 'hmac_secret': Da dat san 'muemu2026' (HMAC-SHA256)
   - 'bank_code', 'bank_account', 'bank_owner': Thong tin ngan hang nhan tien
   - 'mode':
       + 'direct' : Neu web host vie.info.vn ket noi duoc thang toi MariaDB (dien db_host, db_port, db_name, db_user, db_pass)
       + 'relay'  : Neu MariaDB chay tren may/VPS khac (dien 'relay_url' => 'http://<IP_VPS>:8088/sepay/webhook' va bat Webhook trong MuLauncher)

3. Khai bao tren trang quan tri https://my.sepay.vn -> Webhooks -> Them Webhook:
   - URL Webhook: https://vie.info.vn/muemu/webhook.php
   - Su kien: Co tien vao
   - Kieu xac thuc: HMAC-SHA256
   - Secret Key: muemu2026
   - Bo qua neu noi dung khong co code: TAT (vi noi dung chuyen khoan chinh la ten tai khoan game)

4. Kiem tra nhanh:
   - Mo trinh duyet vao: https://vie.info.vn/muemu/webhook.php
     Neu hien {"success":true,"service":"MuOnline SePay Webhook",...} la thanh cong!
   - Trang nap QR tren web (neu muon chia se cho nguoi choi):
     https://vie.info.vn/muemu/
