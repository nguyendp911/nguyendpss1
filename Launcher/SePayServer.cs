using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

// Lightweight embedded SePay Webhook HTTP server for automatic QR top-up -> MEMB_INFO.WCoin
static class SePayServer
{
    static HttpListener listener;
    static Thread worker;
    static Thread poller;
    static volatile bool running;
    static readonly object sync = new object();

    public static bool IsRunning { get { return running; } }
    public static int ActivePort { get; private set; }
    public static string LastStatus { get; private set; }
    public static Action<string> OnLog;

    static string IniPath { get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"Data\Custom\SePay.ini"); } }

    public class Config
    {
        public bool Enable = true;
        public string BankCode = "MB";
        public string BankAccount = "";
        public string BankOwner = "";
        public string ApiKey = "";
        public string HmacSecret = "muemu2026";
        public string PullUrl = "https://vie.info.vn/muemu/webhook.php";
        public int WebhookPort = 8088;
        public long RateVnd = 100000;
        public long RateWcoin = 100000;
        public long MinAmount = 10000;
        public bool AutoStartWebhook = true;
    }

    public static Config LoadConfig()
    {
        var cfg = new Config();
        if (!File.Exists(IniPath)) return cfg;
        foreach (string raw in File.ReadAllLines(IniPath))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("[")) continue;
            int eq = line.IndexOf('=');
            if (eq <= 0) continue;
            string k = line.Substring(0, eq).Trim();
            string v = line.Substring(eq + 1).Trim();
            long lv; int iv;
            if (k.Equals("Enable", StringComparison.OrdinalIgnoreCase)) cfg.Enable = v != "0";
            else if (k.Equals("BankCode", StringComparison.OrdinalIgnoreCase)) cfg.BankCode = v;
            else if (k.Equals("BankAccount", StringComparison.OrdinalIgnoreCase)) cfg.BankAccount = v;
            else if (k.Equals("BankOwner", StringComparison.OrdinalIgnoreCase)) cfg.BankOwner = v;
            else if (k.Equals("ApiKey", StringComparison.OrdinalIgnoreCase)) cfg.ApiKey = v;
            else if (k.Equals("HmacSecret", StringComparison.OrdinalIgnoreCase)) cfg.HmacSecret = v;
            else if (k.Equals("PullUrl", StringComparison.OrdinalIgnoreCase)) cfg.PullUrl = v;
            else if (k.Equals("WebhookPort", StringComparison.OrdinalIgnoreCase) && int.TryParse(v, out iv) && iv > 0 && iv < 65535) cfg.WebhookPort = iv;
            else if (k.Equals("RateVnd", StringComparison.OrdinalIgnoreCase) && long.TryParse(v, out lv) && lv > 0) cfg.RateVnd = lv;
            else if (k.Equals("RateWcoin", StringComparison.OrdinalIgnoreCase) && long.TryParse(v, out lv) && lv > 0) cfg.RateWcoin = lv;
            else if (k.Equals("MinAmount", StringComparison.OrdinalIgnoreCase) && long.TryParse(v, out lv) && lv >= 0) cfg.MinAmount = lv;
            else if (k.Equals("AutoStartWebhook", StringComparison.OrdinalIgnoreCase)) cfg.AutoStartWebhook = v != "0";
        }
        return cfg;
    }

    public static void SaveConfig(Config cfg)
    {
        var sb = new StringBuilder();
        sb.AppendLine("[SePay]");
        sb.AppendLine("Enable=" + (cfg.Enable ? "1" : "0"));
        sb.AppendLine("BankCode=" + (cfg.BankCode ?? "MB").Trim());
        sb.AppendLine("BankAccount=" + (cfg.BankAccount ?? "").Trim());
        sb.AppendLine("BankOwner=" + (cfg.BankOwner ?? "").Trim());
        sb.AppendLine("ApiKey=" + (cfg.ApiKey ?? "").Trim());
        sb.AppendLine("HmacSecret=" + (cfg.HmacSecret ?? "").Trim());
        sb.AppendLine("PullUrl=" + (cfg.PullUrl ?? "").Trim());
        sb.AppendLine("WebhookPort=" + cfg.WebhookPort);
        sb.AppendLine("RateVnd=" + cfg.RateVnd);
        sb.AppendLine("RateWcoin=" + cfg.RateWcoin);
        sb.AppendLine("MinAmount=" + cfg.MinAmount);
        sb.AppendLine("AutoStartWebhook=" + (cfg.AutoStartWebhook ? "1" : "0"));
        Directory.CreateDirectory(Path.GetDirectoryName(IniPath));
        File.WriteAllText(IniPath, sb.ToString(), new UTF8Encoding(false));
    }

    public static void EnsureTable()
    {
        if (!Db.Available) return;
        string err;
        Db.Exec(
            "CREATE TABLE IF NOT EXISTS SePayTransactions (" +
            "SePayId BIGINT PRIMARY KEY," +
            "AccountID VARCHAR(16) NOT NULL DEFAULT ''," +
            "BankCode VARCHAR(32) NOT NULL DEFAULT ''," +
            "AmountVND BIGINT NOT NULL DEFAULT 0," +
            "WCoinAdded BIGINT NOT NULL DEFAULT 0," +
            "Content VARCHAR(255) NOT NULL DEFAULT ''," +
            "RefCode VARCHAR(64) NOT NULL DEFAULT ''," +
            "Status VARCHAR(32) NOT NULL DEFAULT ''," +
            "CreatedAt DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP" +
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;", out err);
    }

    public static bool Start(out string message)
    {
        lock (sync)
        {
            if (running) { message = "Webhook đang chạy trên port " + ActivePort; return true; }
            var cfg = LoadConfig();
            EnsureTable();

            listener = new HttpListener();
            string prefixAll = string.Format("http://+:{0}/sepay/", cfg.WebhookPort);
            string prefixLocal = string.Format("http://localhost:{0}/sepay/", cfg.WebhookPort);
            try
            {
                listener.Prefixes.Add(prefixAll);
                listener.Start();
            }
            catch
            {
                try
                {
                    listener = new HttpListener();
                    listener.Prefixes.Add(prefixLocal);
                    listener.Start();
                }
                catch (Exception ex)
                {
                    message = "Không mở được port " + cfg.WebhookPort + ": " + ex.Message;
                    LastStatus = message;
                    return false;
                }
            }

            ActivePort = cfg.WebhookPort;
            running = true;
            worker = new Thread(ListenLoop) { IsBackground = true, Name = "SePayWebhook" };
            worker.Start();
            poller = new Thread(PollLoop) { IsBackground = true, Name = "SePayPoller" };
            poller.Start();
            message = "SePay Sync đang chạy (Port " + ActivePort + " + đồng bộ tự động từ web)";
            LastStatus = message;
            Log(message);
            return true;
        }
    }

    public static void Stop()
    {
        lock (sync)
        {
            running = false;
            try { if (listener != null) listener.Stop(); } catch { }
            try { if (listener != null) listener.Close(); } catch { }
            listener = null;
            LastStatus = "SePay Webhook đã dừng.";
            Log(LastStatus);
        }
    }

    static void Log(string msg)
    {
        try { if (OnLog != null) OnLog(msg); } catch { }
    }

    static void ListenLoop()
    {
        while (running && listener != null)
        {
            try
            {
                var ctx = listener.GetContext();
                ThreadPool.QueueUserWorkItem(delegate { HandleRequest(ctx); });
            }
            catch
            {
                if (!running) break;
                Thread.Sleep(200);
            }
        }
    }

    static void PollLoop()
    {
        try { ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072 | SecurityProtocolType.Tls12; } catch { }
        while (running)
        {
            try
            {
                var cfg = LoadConfig();
                if (!string.IsNullOrEmpty(cfg.PullUrl) && !string.IsNullOrEmpty(cfg.HmacSecret) && Db.Available)
                {
                    string sep = cfg.PullUrl.Contains("?") ? "&" : "?";
                    string pullUrl = cfg.PullUrl + sep + "action=pull&secret=" + Uri.EscapeDataString(cfg.HmacSecret);
                    using (var wc = new WebClient())
                    {
                        wc.Encoding = Encoding.UTF8;
                        string json = wc.DownloadString(pullUrl);
                        foreach (Match m in Regex.Matches(json, @"\{[^{}]*""id""\s*:\s*\d+[^{}]*\}"))
                        {
                            string item = m.Value;
                            long txId = JsonLong(item, "id");
                            string gateway = JsonStr(item, "gateway");
                            string content = JsonStr(item, "content");
                            long amount = JsonLong(item, "transferAmount");
                            string refCode = JsonStr(item, "referenceCode");
                            if (txId > 0 && amount > 0)
                            {
                                string detail;
                                ProcessIncomingTransfer(txId, gateway, content, amount, refCode, out detail);
                                Log("[SePay Pull] " + detail);
                                try { wc.DownloadString(cfg.PullUrl + sep + "action=ack&secret=" + Uri.EscapeDataString(cfg.HmacSecret) + "&id=" + txId); } catch { }
                            }
                        }
                    }
                }
            }
            catch { }
            for (int i = 0; i < 50 && running; i++) Thread.Sleep(100);
        }
    }

    static void WriteJson(HttpListenerResponse res, int code, string json)
    {
        try
        {
            res.StatusCode = code;
            res.ContentType = "application/json; charset=utf-8";
            byte[] data = Encoding.UTF8.GetBytes(json);
            res.ContentLength64 = data.Length;
            res.OutputStream.Write(data, 0, data.Length);
            res.OutputStream.Close();
        }
        catch { }
    }

    static string JsonStr(string json, string key)
    {
        var m = Regex.Match(json, "\"" + Regex.Escape(key) + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"", RegexOptions.IgnoreCase);
        return m.Success ? Regex.Unescape(m.Groups[1].Value) : "";
    }

    static long JsonLong(string json, string key)
    {
        var m = Regex.Match(json, "\"" + Regex.Escape(key) + "\"\\s*:\\s*(-?\\d+)", RegexOptions.IgnoreCase);
        long v;
        return (m.Success && long.TryParse(m.Groups[1].Value, out v)) ? v : 0;
    }

    static string SqlEsc(string s)
    {
        if (s == null) return "";
        return s.Replace("\\", "\\\\").Replace("'", "''");
    }

    static void HandleRequest(HttpListenerContext ctx)
    {
        try
        {
            string path = (ctx.Request.Url != null ? ctx.Request.Url.AbsolutePath : "").ToLowerInvariant();
            if (ctx.Request.HttpMethod == "GET")
            {
                WriteJson(ctx.Response, 200, "{\"success\":true,\"service\":\"MuSePayWebhook\",\"status\":\"online\"}");
                return;
            }
            if (ctx.Request.HttpMethod != "POST" || !path.Contains("webhook"))
            {
                WriteJson(ctx.Response, 404, "{\"success\":false,\"message\":\"Not found\"}");
                return;
            }

            string body;
            using (var sr = new StreamReader(ctx.Request.InputStream, Encoding.UTF8))
                body = sr.ReadToEnd();

            var cfg = LoadConfig();
            if (!string.IsNullOrEmpty(cfg.HmacSecret))
            {
                string sig = ctx.Request.Headers["X-SePay-Signature"]
                    ?? ctx.Request.Headers["X-Signature"]
                    ?? ctx.Request.Headers["X-Hub-Signature-256"]
                    ?? ctx.Request.Headers["Signature"]
                    ?? "";
                string ts = ctx.Request.Headers["X-SePay-Timestamp"] ?? "";
                sig = sig.Trim();
                if (sig.StartsWith("sha256=", StringComparison.OrdinalIgnoreCase)) sig = sig.Substring(7);

                string payloadWithTs = (ts.Length > 0) ? (ts + "." + body) : body;
                using (var hmac = new HMACSHA256(Encoding.UTF8.GetBytes(cfg.HmacSecret)))
                {
                    byte[] hTs = hmac.ComputeHash(Encoding.UTF8.GetBytes(payloadWithTs));
                    string hexTs = BitConverter.ToString(hTs).Replace("-", "").ToLowerInvariant();
                    string b64Ts = Convert.ToBase64String(hTs);

                    byte[] hRaw = hmac.ComputeHash(Encoding.UTF8.GetBytes(body));
                    string hexRaw = BitConverter.ToString(hRaw).Replace("-", "").ToLowerInvariant();
                    string b64Raw = Convert.ToBase64String(hRaw);

                    bool valid = sig.Length > 0 && (
                        sig.Equals(hexTs, StringComparison.OrdinalIgnoreCase) ||
                        sig.Equals(b64Ts, StringComparison.Ordinal) ||
                        sig.Equals(hexRaw, StringComparison.OrdinalIgnoreCase) ||
                        sig.Equals(b64Raw, StringComparison.Ordinal));

                    if (!valid)
                    {
                        Log("[SePay] Từ chối request sai chữ ký HMAC-SHA256 từ " + ctx.Request.RemoteEndPoint);
                        WriteJson(ctx.Response, 401, "{\"success\":false,\"message\":\"Invalid HMAC-SHA256 signature\"}");
                        return;
                    }
                }
            }

            if (!string.IsNullOrEmpty(cfg.ApiKey) && !cfg.ApiKey.Equals("CHANGE_ME_SEPAY_API_KEY", StringComparison.OrdinalIgnoreCase))
            {
                string auth = ctx.Request.Headers["Authorization"] ?? "";
                string expected = "Apikey " + cfg.ApiKey;
                if (!auth.Trim().Equals(expected, StringComparison.OrdinalIgnoreCase))
                {
                    Log("[SePay] Từ chối request sai API Key từ " + ctx.Request.RemoteEndPoint);
                    WriteJson(ctx.Response, 401, "{\"success\":false,\"message\":\"Invalid API Key\"}");
                    return;
                }
            }

            long txId = JsonLong(body, "id");
            string gateway = JsonStr(body, "gateway");
            string content = JsonStr(body, "content");
            string transferType = JsonStr(body, "transferType");
            long amount = JsonLong(body, "transferAmount");
            string refCode = JsonStr(body, "referenceCode");

            if (!transferType.Equals("in", StringComparison.OrdinalIgnoreCase) || amount <= 0)
            {
                WriteJson(ctx.Response, 200, "{\"success\":true,\"ignored\":true}");
                return;
            }

            if (txId <= 0) txId = DateTime.UtcNow.Ticks / 10000;
            string detail;
            bool ok = ProcessIncomingTransfer(txId, gateway, content, amount, refCode, out detail);
            Log("[SePay] " + detail);
            WriteJson(ctx.Response, 200, "{\"success\":" + (ok ? "true" : "false") + ",\"message\":\"" + detail.Replace("\"", "'") + "\"}");
        }
        catch (Exception ex)
        {
            Log("[SePay] Lỗi xử lý webhook: " + ex.Message);
            WriteJson(ctx.Response, 500, "{\"success\":false,\"message\":\"Internal error\"}");
        }
    }

    static string ResolveAccountFromContent(string content)
    {
        if (string.IsNullOrEmpty(content)) return null;
        string trimmed = content.Trim();
        string err;

        // 1. Exact match if content is purely the account ID
        if (Regex.IsMatch(trimmed, @"^[A-Za-z0-9_]{3,10}$"))
        {
            var r = Db.Query("SELECT memb___id FROM MEMB_INFO WHERE LOWER(memb___id)=LOWER('" + SqlEsc(trimmed) + "') LIMIT 1", out err);
            if (err == null && r.Count > 0 && r[0].Length > 0) return r[0][0];
        }

        // 2. Scan tokens separated by whitespace / punctuation (banks often wrap transfer note with codes)
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (Match m in Regex.Matches(trimmed, @"[A-Za-z0-9_]{3,10}"))
        {
            string tok = m.Value;
            if (!seen.Add(tok)) continue;
            var r = Db.Query("SELECT memb___id FROM MEMB_INFO WHERE LOWER(memb___id)=LOWER('" + SqlEsc(tok) + "') LIMIT 1", out err);
            if (err == null && r.Count > 0 && r[0].Length > 0) return r[0][0];
        }
        return null;
    }

    public static bool ProcessIncomingTransfer(long txId, string bankCode, string content, long amountVnd, string refCode, out string resultMsg)
    {
        if (!Db.Available) { resultMsg = "MariaDB chưa sẵn sàng."; return false; }
        EnsureTable();

        var cfg = LoadConfig();
        string err;

        // Idempotency check by SePayId
        var dup = Db.Query("SELECT COUNT(*) FROM SePayTransactions WHERE SePayId=" + txId, out err);
        if (err == null && dup.Count > 0 && dup[0].Length > 0 && dup[0][0] != "0")
        {
            resultMsg = "Giao dịch #" + txId + " đã được xử lý trước đó (bỏ qua trùng lặp).";
            return true;
        }

        string acc = ResolveAccountFromContent(content);
        if (string.IsNullOrEmpty(acc))
        {
            Db.Exec(string.Format(
                "INSERT IGNORE INTO SePayTransactions (SePayId,AccountID,BankCode,AmountVND,WCoinAdded,Content,RefCode,Status) VALUES ({0},'','{1}',{2},0,'{3}','{4}','ACCOUNT_NOT_FOUND')",
                txId, SqlEsc(bankCode), amountVnd, SqlEsc(content), SqlEsc(refCode)), out err);
            resultMsg = "Không tìm thấy tài khoản khớp nội dung CK '" + content + "' (" + amountVnd.ToString("N0") + " VND).";
            return false;
        }

        if (amountVnd < cfg.MinAmount)
        {
            Db.Exec(string.Format(
                "INSERT IGNORE INTO SePayTransactions (SePayId,AccountID,BankCode,AmountVND,WCoinAdded,Content,RefCode,Status) VALUES ({0},'{1}','{2}',{3},0,'{4}','{5}','BELOW_MIN')",
                txId, SqlEsc(acc), SqlEsc(bankCode), amountVnd, SqlEsc(content), SqlEsc(refCode)), out err);
            resultMsg = "Số tiền " + amountVnd.ToString("N0") + " VND thấp hơn mức tối thiểu (" + cfg.MinAmount.ToString("N0") + ").";
            return false;
        }

        long wcoin = (cfg.RateVnd > 0) ? ((amountVnd * cfg.RateWcoin) / cfg.RateVnd) : amountVnd;
        if (wcoin <= 0)
        {
            resultMsg = "Số WCoin quy đổi = 0.";
            return false;
        }

        Db.Exec(string.Format("UPDATE MEMB_INFO SET WCoin = WCoin + {0} WHERE memb___id = '{1}'", wcoin, SqlEsc(acc)), out err);
        if (err != null)
        {
            resultMsg = "Lỗi cộng WCoin: " + err;
            return false;
        }

        Db.Exec(string.Format(
            "INSERT IGNORE INTO SePayTransactions (SePayId,AccountID,BankCode,AmountVND,WCoinAdded,Content,RefCode,Status) VALUES ({0},'{1}','{2}',{3},{4},'{5}','{6}','SUCCESS')",
            txId, SqlEsc(acc), SqlEsc(bankCode), amountVnd, wcoin, SqlEsc(content), SqlEsc(refCode)), out err);

        resultMsg = string.Format("Nạp thành công +{0:N0} WCoin cho tài khoản [{1}] ({2:N0} VND, TX #{3})!", wcoin, acc, amountVnd, txId);
        return true;
    }

    public static bool SimulatePayment(string account, long amountVnd, out string resultMsg)
    {
        long fakeId = 9000000000L + (DateTime.UtcNow.Ticks % 900000000L);
        var cfg = LoadConfig();
        return ProcessIncomingTransfer(fakeId, cfg.BankCode, account, amountVnd, "TEST_SIM", out resultMsg);
    }
}
