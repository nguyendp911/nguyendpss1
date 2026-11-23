<?php
// =========================================================================
// SePay Webhook Endpoint: https://vie.info.vn/muemu/webhook.php
// Nhan bien dong so du tu SePay -> tu dong cong MEMB_INFO.WCoin (100k = 100k WCoin)
// Noi dung chuyen khoan chinh la ten tai khoan (MEMB_INFO.memb___id)
// =========================================================================

header('Content-Type: application/json; charset=utf-8');

$cfg = require __DIR__ . '/config.php';

function sepay_log(string $msg): void {
    $line = '[' . date('Y-m-d H:i:s') . '] ' . $msg . PHP_EOL;
    @file_put_contents(__DIR__ . '/sepay.log', $line, FILE_APPEND | LOCK_EX);
}

function json_out(int $code, array $payload): void {
    http_response_code($code);
    echo json_encode($payload, JSON_UNESCAPED_UNICODE);
    exit;
}

// GET: Health check hoac MuLauncher dong bo hang doi (action=pull / action=ack)
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    $action = (string)($_GET['action'] ?? '');
    if ($action === 'pull' || $action === 'ack') {
        $sec = trim((string)($_GET['secret'] ?? ''));
        $hmacSecret = trim((string)($cfg['hmac_secret'] ?? ''));
        if ($hmacSecret === '' || !hash_equals($hmacSecret, $sec)) {
            json_out(401, ['success' => false, 'message' => 'Unauthorized']);
        }
        $qFile = __DIR__ . '/queue.json';
        $fp = @fopen($qFile, 'c+');
        if (!$fp) json_out(500, ['success' => false, 'message' => 'Cannot open queue']);
        flock($fp, LOCK_EX);
        $raw = stream_get_contents($fp);
        $list = $raw ? json_decode($raw, true) : [];
        if (!is_array($list)) $list = [];

        if ($action === 'pull') {
            flock($fp, LOCK_UN);
            fclose($fp);
            json_out(200, ['success' => true, 'items' => array_values($list)]);
        } else {
            $ackId = (string)($_GET['id'] ?? '');
            $newList = [];
            foreach ($list as $it) {
                if ((string)($it['id'] ?? '') !== $ackId) $newList[] = $it;
            }
            ftruncate($fp, 0);
            rewind($fp);
            fwrite($fp, json_encode($newList, JSON_UNESCAPED_UNICODE));
            fflush($fp);
            flock($fp, LOCK_UN);
            fclose($fp);
            json_out(200, ['success' => true, 'acked' => $ackId]);
        }
    }

    json_out(200, [
        'success' => true,
        'service' => 'MuOnline SePay Webhook',
        'mode'    => $cfg['mode'],
        'rate'    => $cfg['rate_vnd'] . ' VND = ' . $cfg['rate_wcoin'] . ' WCoin',
    ]);
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    json_out(405, ['success' => false, 'message' => 'Method not allowed']);
}

$rawBody = file_get_contents('php://input');
if (!$rawBody) {
    json_out(400, ['success' => false, 'message' => 'Empty body']);
}

$allHeaders = [];
if (function_exists('getallheaders')) {
    $allHeaders = array_change_key_case(getallheaders(), CASE_LOWER);
} elseif (function_exists('apache_request_headers')) {
    $allHeaders = array_change_key_case(apache_request_headers(), CASE_LOWER);
}
$authHeader = $_SERVER['HTTP_AUTHORIZATION']
    ?? $_SERVER['REDIRECT_HTTP_AUTHORIZATION']
    ?? ($allHeaders['authorization'] ?? '');

$hmacSecret  = trim((string)($cfg['hmac_secret'] ?? ''));
$expectedKey = trim((string)($cfg['sepay_api_key'] ?? ''));
$hasHmacCfg  = ($hmacSecret !== '');
$hasApiCfg   = ($expectedKey !== '' && strcasecmp($expectedKey, 'CHANGE_ME_SEPAY_API_KEY') !== 0);

$sigHeader = $_SERVER['HTTP_X_SEPAY_SIGNATURE']
    ?? $_SERVER['HTTP_X_SIGNATURE']
    ?? $_SERVER['HTTP_X_HUB_SIGNATURE_256']
    ?? $_SERVER['HTTP_SIGNATURE']
    ?? ($allHeaders['x-sepay-signature'] ?? $allHeaders['x-signature'] ?? $allHeaders['x-hub-signature-256'] ?? $allHeaders['signature'] ?? '');
$tsHeader = $_SERVER['HTTP_X_SEPAY_TIMESTAMP']
    ?? ($allHeaders['x-sepay-timestamp'] ?? '');

$hmacOk = false;
$apiOk  = false;

// 1A. Kiem tra HMAC-SHA256 (neu request gui header chu ky)
if ($hasHmacCfg && trim($sigHeader) !== '') {
    $sigClean = trim($sigHeader);
    if (stripos($sigClean, 'sha256=') === 0) {
        $sigClean = substr($sigClean, 7);
    }
    $sigLower = strtolower($sigClean);

    $payloadWithTs = ($tsHeader !== '') ? ($tsHeader . '.' . $rawBody) : $rawBody;
    $calcTsHex  = hash_hmac('sha256', $payloadWithTs, $hmacSecret);
    $calcTsB64  = base64_encode(hash_hmac('sha256', $payloadWithTs, $hmacSecret, true));
    $calcRawHex = hash_hmac('sha256', $rawBody, $hmacSecret);
    $calcRawB64 = base64_encode(hash_hmac('sha256', $rawBody, $hmacSecret, true));

    $hmacOk = hash_equals($calcTsHex, $sigLower) ||
              hash_equals($calcTsB64, $sigClean) ||
              hash_equals($calcRawHex, $sigLower) ||
              hash_equals($calcRawB64, $sigClean);
    if (!$hmacOk) {
        sepay_log('REJECT 401 Invalid HMAC-SHA256 from ' . ($_SERVER['REMOTE_ADDR'] ?? 'unknown') . ' ts=' . $tsHeader . ' sig=' . $sigHeader);
        json_out(401, ['success' => false, 'message' => 'Invalid HMAC-SHA256 signature']);
    }
}

// 1B. Kiem tra API Key (Authorization: Apikey <sepay_api_key>)
if ($hasApiCfg && trim($authHeader) !== '') {
    $expectedHeader = 'Apikey ' . $expectedKey;
    $apiOk = (strcasecmp(trim($authHeader), $expectedHeader) === 0) ||
             (strcasecmp(trim($authHeader), 'Bearer ' . $expectedKey) === 0) ||
             (strcasecmp(trim($authHeader), $expectedKey) === 0);
    if (!$apiOk) {
        sepay_log('REJECT 401 Invalid API Key from ' . ($_SERVER['REMOTE_ADDR'] ?? 'unknown'));
        json_out(401, ['success' => false, 'message' => 'Invalid API Key']);
    }
}

// Bat buoc it nhat 1 phuong thuc xac thuc hop le (HMAC-SHA256 hoac API Key)
if (($hasHmacCfg || $hasApiCfg) && !$hmacOk && !$apiOk) {
    $hdrDump = json_encode($allHeaders, JSON_UNESCAPED_UNICODE);
    sepay_log('REJECT 401 Missing Auth/HMAC from ' . ($_SERVER['REMOTE_ADDR'] ?? 'unknown') . ' headers=' . $hdrDump);
    json_out(401, ['success' => false, 'message' => 'Missing Authorization (Apikey) or X-SePay-Signature header', 'seen_headers' => array_keys($allHeaders)]);
}

// 2. Che do RELAY: chuyen tiep thang ve MuLauncher Webhook
if (($cfg['mode'] ?? 'direct') === 'relay') {
    $relayHeaders = ['Content-Type: application/json'];
    if ($sigHeader !== '') $relayHeaders[] = 'X-SePay-Signature: ' . $sigHeader;
    if ($tsHeader !== '')  $relayHeaders[] = 'X-SePay-Timestamp: ' . $tsHeader;
    if ($authHeader !== '') $relayHeaders[] = 'Authorization: ' . $authHeader;
    elseif ($expectedKey !== '') $relayHeaders[] = 'Authorization: Apikey ' . $expectedKey;

    $ch = curl_init($cfg['relay_url']);
    curl_setopt_array($ch, [
        CURLOPT_POST           => true,
        CURLOPT_POSTFIELDS     => $rawBody,
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT        => 12,
        CURLOPT_HTTPHEADER     => $relayHeaders,
    ]);
    $resp = curl_exec($ch);
    $httpCode = (int)curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $curlErr = curl_error($ch);
    curl_close($ch);

    if ($curlErr || $httpCode < 200 || $httpCode >= 300) {
        sepay_log("RELAY ERROR HTTP={$httpCode} ERR={$curlErr} RESP={$resp}");
        json_out(502, ['success' => false, 'message' => 'Relay failed: ' . $curlErr]);
    }
    sepay_log("RELAY OK HTTP={$httpCode} RESP={$resp}");
    http_response_code(200);
    echo $resp;
    exit;
}

// 2B. Che do QUEUE: luu giao dich da xac thuc vao queue.json de MuLauncher tu dong lay ve cong WCoin
if (($cfg['mode'] ?? 'queue') === 'queue') {
    $data = json_decode($rawBody, true);
    if (!is_array($data)) {
        json_out(400, ['success' => false, 'message' => 'Invalid JSON']);
    }
    $txId         = isset($data['id']) ? (int)$data['id'] : (int)(microtime(true) * 1000);
    $gateway      = trim((string)($data['gateway'] ?? $cfg['bank_code']));
    $content      = trim((string)($data['content'] ?? ''));
    $transferType = strtolower(trim((string)($data['transferType'] ?? '')));
    $amountVnd    = isset($data['transferAmount']) ? (int)$data['transferAmount'] : 0;
    $refCode      = trim((string)($data['referenceCode'] ?? ''));

    if ($transferType !== 'in' || $amountVnd <= 0) {
        json_out(200, ['success' => true, 'ignored' => true]);
    }

    $qFile = __DIR__ . '/queue.json';
    $fp = @fopen($qFile, 'c+');
    if (!$fp) {
        sepay_log("QUEUE ERROR cannot open {$qFile}");
        json_out(500, ['success' => false, 'message' => 'Cannot write queue.json (check folder permission)']);
    }
    flock($fp, LOCK_EX);
    $raw = stream_get_contents($fp);
    $list = $raw ? json_decode($raw, true) : [];
    if (!is_array($list)) $list = [];

    $exists = false;
    foreach ($list as $it) {
        if ((int)($it['id'] ?? 0) === $txId) { $exists = true; break; }
    }
    if (!$exists) {
        $list[] = [
            'id'             => $txId,
            'gateway'        => $gateway,
            'content'        => $content,
            'transferAmount' => $amountVnd,
            'referenceCode'  => $refCode,
            'receivedAt'     => date('Y-m-d H:i:s'),
        ];
        ftruncate($fp, 0);
        rewind($fp);
        fwrite($fp, json_encode($list, JSON_UNESCAPED_UNICODE));
        fflush($fp);
    }
    flock($fp, LOCK_UN);
    fclose($fp);

    sepay_log("QUEUED TX #{$txId} Gateway={$gateway} Amount={$amountVnd} Content='{$content}'");
    json_out(200, ['success' => true, 'queued' => true, 'id' => $txId]);
}

// 3. Che do DIRECT: ket noi thang MariaDB (MuOnline97) va cong MEMB_INFO.WCoin
$data = json_decode($rawBody, true);
if (!is_array($data)) {
    json_out(400, ['success' => false, 'message' => 'Invalid JSON']);
}

$txId         = isset($data['id']) ? (int)$data['id'] : (int)(microtime(true) * 1000);
$gateway      = trim((string)($data['gateway'] ?? $cfg['bank_code']));
$content      = trim((string)($data['content'] ?? ''));
$transferType = strtolower(trim((string)($data['transferType'] ?? '')));
$amountVnd    = isset($data['transferAmount']) ? (int)$data['transferAmount'] : 0;
$refCode      = trim((string)($data['referenceCode'] ?? ''));

// Chi xu ly tien vao
if ($transferType !== 'in' || $amountVnd <= 0) {
    json_out(200, ['success' => true, 'ignored' => true]);
}

try {
    $dsn = sprintf('mysql:host=%s;port=%d;dbname=%s;charset=utf8mb4', $cfg['db_host'], (int)$cfg['db_port'], $cfg['db_name']);
    $pdo = new PDO($dsn, $cfg['db_user'], $cfg['db_pass'], [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);

    // Tao bang lich su giao dich neu chua co
    $pdo->exec("
        CREATE TABLE IF NOT EXISTS SePayTransactions (
            SePayId BIGINT PRIMARY KEY,
            AccountID VARCHAR(16) NOT NULL DEFAULT '',
            BankCode VARCHAR(32) NOT NULL DEFAULT '',
            AmountVND BIGINT NOT NULL DEFAULT 0,
            WCoinAdded BIGINT NOT NULL DEFAULT 0,
            Content VARCHAR(255) NOT NULL DEFAULT '',
            RefCode VARCHAR(64) NOT NULL DEFAULT '',
            Status VARCHAR(32) NOT NULL DEFAULT '',
            CreatedAt DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
    ");

    // Chong cong trung giao dich (Idempotency)
    $stDup = $pdo->prepare('SELECT COUNT(*) FROM SePayTransactions WHERE SePayId = ?');
    $stDup->execute([$txId]);
    if ((int)$stDup->fetchColumn() > 0) {
        sepay_log("DUPLICATE TX #{$txId} ignored.");
        json_out(200, ['success' => true, 'message' => 'Duplicate transaction ignored']);
    }

    // Tim ten tai khoan trong MEMB_INFO khop voi noi dung chuyen khoan
    $accountId = null;
    $stAcc = $pdo->prepare('SELECT memb___id FROM MEMB_INFO WHERE LOWER(memb___id) = LOWER(?) LIMIT 1');

    if (preg_match('/^[A-Za-z0-9_]{3,10}$/', $content)) {
        $stAcc->execute([$content]);
        $row = $stAcc->fetch();
        if ($row) $accountId = $row['memb___id'];
    }

    if ($accountId === null && preg_match_all('/[A-Za-z0-9_]{3,10}/', $content, $matches)) {
        $seen = [];
        foreach ($matches[0] as $tok) {
            $low = strtolower($tok);
            if (isset($seen[$low])) continue;
            $seen[$low] = true;
            $stAcc->execute([$tok]);
            $row = $stAcc->fetch();
            if ($row) {
                $accountId = $row['memb___id'];
                break;
            }
        }
    }

    $stIns = $pdo->prepare('INSERT IGNORE INTO SePayTransactions (SePayId, AccountID, BankCode, AmountVND, WCoinAdded, Content, RefCode, Status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)');

    if ($accountId === null) {
        $stIns->execute([$txId, '', $gateway, $amountVnd, 0, $content, $refCode, 'ACCOUNT_NOT_FOUND']);
        sepay_log("NOT FOUND TX #{$txId} Amount={$amountVnd} Content='{$content}'");
        json_out(200, ['success' => false, 'message' => 'Account not found in content']);
    }

    if ($amountVnd < (int)$cfg['min_amount']) {
        $stIns->execute([$txId, $accountId, $gateway, $amountVnd, 0, $content, $refCode, 'BELOW_MIN']);
        sepay_log("BELOW MIN TX #{$txId} Acc={$accountId} Amount={$amountVnd}");
        json_out(200, ['success' => false, 'message' => 'Amount below minimum']);
    }

    $rateVnd = max(1, (int)$cfg['rate_vnd']);
    $rateWc  = max(1, (int)$cfg['rate_wcoin']);
    $wcoin   = intdiv($amountVnd * $rateWc, $rateVnd);

    $pdo->beginTransaction();
    $stUpd = $pdo->prepare('UPDATE MEMB_INFO SET WCoin = WCoin + ? WHERE memb___id = ?');
    $stUpd->execute([$wcoin, $accountId]);
    $stIns->execute([$txId, $accountId, $gateway, $amountVnd, $wcoin, $content, $refCode, 'SUCCESS']);
    $pdo->commit();

    sepay_log("SUCCESS TX #{$txId} Acc={$accountId} Amount={$amountVnd} VND -> +{$wcoin} WCoin");
    json_out(200, [
        'success' => true,
        'account' => $accountId,
        'amount'  => $amountVnd,
        'wcoin'   => $wcoin,
    ]);
} catch (Throwable $e) {
    if (isset($pdo) && $pdo->inTransaction()) $pdo->rollBack();
    sepay_log('ERROR: ' . $e->getMessage());
    json_out(500, ['success' => false, 'message' => 'Database error']);
}
