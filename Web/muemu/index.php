<?php
$cfg = require __DIR__ . '/config.php';
$acc = preg_replace('/[^A-Za-z0-9_]/', '', (string)($_GET['acc'] ?? ''));
$amount = max((int)$cfg['min_amount'], (int)($_GET['amount'] ?? 100000));
$wcoin = intdiv($amount * (int)$cfg['rate_wcoin'], max(1, (int)$cfg['rate_vnd']));
$qrUrl = ($acc !== '')
    ? sprintf('https://qr.sepay.vn/img?acc=%s&bank=%s&amount=%d&des=%s&template=compact',
        urlencode($cfg['bank_account']), urlencode($cfg['bank_code']), $amount, urlencode($acc))
    : '';
?>
<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MU Online 0.97k — Nạp WCoin SePay QR</title>
<style>
  body{margin:0;background:#0b1017;color:#e6f2fa;font-family:Segoe UI,system-ui,sans-serif;display:flex;justify-content:center;padding:24px}
  .card{width:100%;max-width:460px;background:#111925;border:1px solid #2b4459;border-radius:10px;padding:22px;box-shadow:0 12px 32px rgba(0,0,0,.5)}
  h1{margin:0 0 6px;font-size:20px;color:#7de3f7;text-align:center}
  .sub{text-align:center;color:#9bb5c9;font-size:13px;margin-bottom:18px}
  label{display:block;font-size:13px;color:#a8c5d9;margin:10px 0 4px}
  input{width:100%;box-sizing:border-box;padding:10px 12px;border-radius:6px;border:1px solid #2f4b63;background:#0a111a;color:#fff;font-size:15px}
  .presets{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:10px 0 14px}
  .presets button{padding:8px;border:1px solid #355873;background:#162436;color:#d8eefc;border-radius:6px;cursor:pointer;font-weight:600}
  .presets button:hover{border-color:#6fd7ee;background:#1d334c}
  .submit{width:100%;padding:11px;border:none;border-radius:6px;background:#22a06b;color:#fff;font-size:15px;font-weight:700;cursor:pointer}
  .submit:hover{background:#28b87b}
  .qrbox{margin-top:18px;text-align:center;background:#0c141f;border:1px solid #2b4459;border-radius:8px;padding:16px}
  .qrbox img{width:230px;height:230px;background:#fff;padding:8px;border-radius:8px}
  .info{text-align:left;margin-top:12px;font-size:14px;line-height:1.7}
  .hi{color:#ffe066;font-weight:700}
  .cyan{color:#73faff;font-weight:700}
  .green{color:#78f8a5;font-weight:700}
</style>
</head>
<body>
<div class="card">
  <h1>NẠP WCOIN TỰ ĐỘNG (SEPAY QR)</h1>
  <div class="sub">Tỉ lệ: <b><?= number_format($cfg['rate_vnd']) ?> VND = <?= number_format($cfg['rate_wcoin']) ?> WCoin</b> • Cộng tự động 5–15 giây</div>
  <form method="get">
    <label>Tên tài khoản game (Nội dung chuyển khoản):</label>
    <input name="acc" maxlength="10" required placeholder="Nhập tài khoản đăng nhập (VD: test2)" value="<?= htmlspecialchars($acc) ?>">
    <label>Số tiền muốn nạp (VND):</label>
    <input id="amt" name="amount" type="number" min="<?= (int)$cfg['min_amount'] ?>" step="10000" value="<?= $amount ?>">
    <div class="presets">
      <?php foreach ([20000,50000,100000,200000,500000,1000000] as $p): ?>
      <button type="button" onclick="document.getElementById('amt').value=<?= $p ?>"><?= number_format($p) ?></button>
      <?php endforeach; ?>
    </div>
    <button class="submit" type="submit">Tạo mã QR chuyển khoản</button>
  </form>
  <?php if ($qrUrl): ?>
  <div class="qrbox">
    <img src="<?= htmlspecialchars($qrUrl) ?>" alt="SePay QR">
    <div class="info">
      Ngân hàng: <b><?= htmlspecialchars($cfg['bank_code']) ?></b><br>
      Số tài khoản: <span class="hi"><?= htmlspecialchars($cfg['bank_account']) ?></span><br>
      Chủ tài khoản: <b><?= htmlspecialchars($cfg['bank_owner']) ?></b><br>
      Nội dung CK: <span class="cyan"><?= htmlspecialchars($acc) ?></span><br>
      Số tiền: <b><?= number_format($amount) ?> VND</b> → Nhận: <span class="green">+<?= number_format($wcoin) ?> WCoin</span>
    </div>
  </div>
  <?php endif; ?>
</div>
</body>
</html>
