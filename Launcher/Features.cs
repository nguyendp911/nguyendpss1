using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

interface IShown { void OnShown(); }

// Launcher options, stored next to the exe in MuLauncher.ini.
static class Settings
{
    public static bool AutoDb = true;
    public static bool Watchdog = true;
    public static bool MinToTray = false;
    public static bool BackupOnStop = true;
    public static int BackupEveryHours = 0;
    public static int KeepBackups = 20;

    static string FilePath { get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "MuLauncher.ini"); } }

    static bool B(string v, bool d) { return v == "1" ? true : (v == "0" ? false : d); }
    static int I(string v, int d) { int n; return int.TryParse(v, out n) ? n : d; }

    public static void Load()
    {
        try
        {
            if (!File.Exists(FilePath)) return;
            foreach (string line in File.ReadAllLines(FilePath))
            {
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                string k = line.Substring(0, eq).Trim(), v = line.Substring(eq + 1).Trim();
                switch (k)
                {
                    case "AutoDb": AutoDb = B(v, AutoDb); break;
                    case "Watchdog": Watchdog = B(v, Watchdog); break;
                    case "MinToTray": MinToTray = B(v, MinToTray); break;
                    case "BackupOnStop": BackupOnStop = B(v, BackupOnStop); break;
                    case "BackupEveryHours": BackupEveryHours = I(v, BackupEveryHours); break;
                    case "KeepBackups": KeepBackups = Math.Max(1, I(v, KeepBackups)); break;
                }
            }
        }
        catch { }
    }

    public static void Save()
    {
        try
        {
            File.WriteAllText(FilePath,
                "AutoDb=" + (AutoDb ? 1 : 0) + "\r\n" +
                "Watchdog=" + (Watchdog ? 1 : 0) + "\r\n" +
                "MinToTray=" + (MinToTray ? 1 : 0) + "\r\n" +
                "BackupOnStop=" + (BackupOnStop ? 1 : 0) + "\r\n" +
                "BackupEveryHours=" + BackupEveryHours + "\r\n" +
                "KeepBackups=" + KeepBackups + "\r\n");
        }
        catch { }
    }
}

partial class MainForm
{
    NotifyIcon tray;
    bool reallyExit;
    bool skipAutoBackup;
    LogPanel logPanel;
    AccountPanel accPanel;
    GameAdminPanel gameAdminPanel;
    ToolsPanel toolsPanel;
    DateTime lastBackup = DateTime.Now;
    DateTime nextOnline = DateTime.MinValue;
    bool onlineBusy;

    // ---------- setup ----------
    void InitFeatures(bool autostart, bool trayStart)
    {
        Settings.Load();
        chkDb.Checked = Settings.AutoDb;
        chkDb.CheckedChanged += delegate { Settings.AutoDb = chkDb.Checked; Settings.Save(); };

        var menu = new ContextMenuStrip { Renderer = new DarkRenderer(), BackColor = Theme.Panel, ForeColor = Theme.Text };
        menu.Items.Add("Mở cửa sổ", null, delegate { ShowFromTray(); });
        menu.Items.Add("Chạy tất cả", null, delegate { StartAll(); });
        menu.Items.Add("Tắt tất cả", null, delegate { StopAll(); });
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Thoát", null, delegate { reallyExit = true; ShowFromTray(); Close(); });
        tray = new NotifyIcon { Text = "MuServer Launcher", ContextMenuStrip = menu, Visible = true };
        try { tray.Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { tray.Icon = SystemIcons.Application; }
        tray.DoubleClick += delegate { ShowFromTray(); };
        FormClosed += delegate { tray.Visible = false; tray.Dispose(); };

        Resize += delegate { if (Settings.MinToTray && WindowState == FormWindowState.Minimized) Hide(); };

        if (autostart) Shown += delegate { StartAll(); };
        if (trayStart) Shown += delegate { Hide(); };
    }

    void ShowFromTray()
    {
        Show();
        if (WindowState == FormWindowState.Minimized) WindowState = FormWindowState.Normal;
        Activate();
    }

    bool HideToTrayInstead(FormClosingEventArgs e)
    {
        if (!reallyExit && Settings.MinToTray && e.CloseReason == CloseReason.UserClosing)
        {
            e.Cancel = true;
            Hide();
            tray.ShowBalloonTip(3000, "MuServer Launcher", "Launcher vẫn chạy ở khay hệ thống. Bấm đúp biểu tượng để mở lại.", ToolTipIcon.Info);
            return true;
        }
        return false;
    }

    void Notify(string msg, bool error)
    {
        Log(msg);
        try { tray.ShowBalloonTip(5000, "MuServer Launcher", msg, error ? ToolTipIcon.Error : ToolTipIcon.Info); } catch { }
    }

    // ---------- extra tabs ----------
    void AddExtraTabs(FlowLayoutPanel tabFlow, Panel card)
    {
        logPanel = new LogPanel();
        accPanel = new AccountPanel(this);
        gameAdminPanel = new GameAdminPanel(this);
        toolsPanel = new ToolsPanel(this);
        AddExtra(tabFlow, card, "Nhật ký", logPanel);
        AddExtra(tabFlow, card, "Tài khoản", accPanel);
        AddExtra(tabFlow, card, "Quản lý Game", gameAdminPanel);
        AddExtra(tabFlow, card, "Công cụ", toolsPanel);
    }

    void AddExtra(FlowLayoutPanel tabFlow, Panel card, string title, Control c)
    {
        var page = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel, Visible = false };
        c.Dock = DockStyle.Fill;
        page.Controls.Add(c);
        card.Controls.Add(page);
        int idx = allBtns.Count;
        var b = new TabBtn(title) { NoDot = true, Width = 130 };
        b.Click += delegate { SelectTab(idx); };
        tabFlow.Controls.Add(b);
        allBtns.Add(b);
        allPages.Add(page);
    }

    void OnTabShown(int i)
    {
        if (i < servers.Count) return;
        var page = allPages[i];
        if (page.Controls.Count > 0 && page.Controls[0] is IShown) ((IShown)page.Controls[0]).OnShown();
    }

    // ---------- periodic work (called from the 500ms timer) ----------
    void FeatureTick()
    {
        DateTime now = DateTime.Now;

        // watchdog: restart servers that died without the user asking
        foreach (var s in servers)
        {
            if (Running(s)) { s.ExitSeen = DateTime.MinValue; continue; }
            if (!s.Wanted || s.GaveUp || !Settings.Watchdog || dbPending || startQueue.Count > 0) continue;
            if (s.ExitSeen == DateTime.MinValue) { s.ExitSeen = now; continue; }
            if ((now - s.ExitSeen).TotalSeconds < 3) continue;
            s.Restarts.RemoveAll(delegate (DateTime t) { return (now - t).TotalMinutes > 5; });
            if (s.Restarts.Count >= 3)
            {
                s.GaveUp = true;
                Notify(s.Name + " sập liên tục (3 lần trong 5 phút), đã ngừng tự khởi động lại. Xem tab Nhật ký.", true);
                continue;
            }
            s.Restarts.Add(now);
            s.ExitSeen = DateTime.MinValue;
            Notify(s.Name + " vừa bị tắt đột ngột, đang tự khởi động lại.", true);
            StartSrv(s);
        }

        // scheduled backups
        if (Settings.BackupEveryHours > 0 && Db.Available && !backupBusy && (now - lastBackup).TotalHours >= Settings.BackupEveryHours && PortListening(DbPort))
        {
            lastBackup = now;
            BackupAsync("auto");
        }

        // number of players online
        if (now >= nextOnline && !onlineBusy && Db.Available && PortListening(DbPort))
        {
            nextOnline = now.AddSeconds(10);
            onlineBusy = true;
            ThreadPool.QueueUserWorkItem(delegate
            {
                string err;
                var rows = Db.Query("SELECT COUNT(*) FROM MEMB_STAT WHERE ConnectStat=1", out err);
                int n = -1;
                if (err == null && rows.Count > 0) int.TryParse(rows[0][0], out n);
                try
                {
                    BeginInvoke(new Action(delegate
                    {
                        onlineBusy = false;
                        lblOnline.Text = "Online: " + (n >= 0 ? n.ToString() : "-");
                        lblOnline.ForeColor = n > 0 ? Theme.Blue : Theme.Muted;
                    }));
                }
                catch { onlineBusy = false; }
            });
        }
    }

    // ---------- backup / restore ----------
    bool backupBusy;

    internal string DoBackup(string tag, out string err)
    {
        err = null;
        string name = Db.Database + "_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + (string.IsNullOrEmpty(tag) ? "" : "_" + tag) + ".sql";
        string path = Path.Combine(Db.BackupDir, name);
        if (!Db.Dump(path, out err)) { Log("Sao lưu lỗi: " + err); return null; }
        lastBackup = DateTime.Now;
        RotateBackups();
        Log("Đã sao lưu: " + name);
        return path;
    }

    void BackupAsync(string tag)
    {
        backupBusy = true;
        ThreadPool.QueueUserWorkItem(delegate
        {
            string err;
            string p = DoBackup(tag, out err);
            try { BeginInvoke(new Action(delegate { backupBusy = false; toolsPanel.RefreshBackups(); })); } catch { backupBusy = false; }
        });
    }

    void RotateBackups()
    {
        try
        {
            var files = new List<string>(Directory.GetFiles(Db.BackupDir, Db.Database + "_*.sql"));
            files.Sort(StringComparer.OrdinalIgnoreCase);
            files.Reverse(); // newest first (names start with a timestamp)
            for (int i = Settings.KeepBackups; i < files.Count; i++) { try { File.Delete(files[i]); } catch { } }
        }
        catch { }
    }

    void AfterStopAll()
    {
        if (skipAutoBackup || !Settings.BackupOnStop || !Db.Available || !PortListening(DbPort)) return;
        string err;
        DoBackup("tat-server", out err);
    }

    internal void RestoreBackup(string file)
    {
        if (!File.Exists(file)) return;
        var r = MessageBox.Show("Khôi phục database từ:\n" + Path.GetFileName(file) + "\n\nTOÀN BỘ dữ liệu hiện tại (tài khoản, nhân vật, đồ) sẽ bị thay thế.\nLauncher sẽ tắt 4 server và tự sao lưu bản hiện tại trước.\n\nTiếp tục?",
            "Khôi phục database", MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2);
        if (r != DialogResult.Yes) return;
        Cursor = Cursors.WaitCursor;
        try
        {
            skipAutoBackup = true;
            StopAll();
            skipAutoBackup = false;
            string err;
            string safety = DoBackup("truoc-khoi-phuc", out err);
            if (safety == null) { MessageBox.Show("Không sao lưu được bản hiện tại nên đã dừng, không khôi phục.\n" + err, "Khôi phục database", MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
            if (!Db.Restore(file, out err)) { MessageBox.Show("Khôi phục lỗi:\n" + err + "\n\nBản sao lưu trước khi khôi phục: " + Path.GetFileName(safety), "Khôi phục database", MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
            Log("Đã khôi phục database từ " + Path.GetFileName(file));
            MessageBox.Show("Đã khôi phục xong.\nBấm Chạy tất cả để bật lại server.", "Khôi phục database", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        finally { skipAutoBackup = false; Cursor = Cursors.Default; }
    }

    internal static bool GetRunAtLogon()
    {
        try { using (var k = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run")) return k != null && k.GetValue("MuLauncher") != null; }
        catch { return false; }
    }

    internal static void SetRunAtLogon(bool on)
    {
        try
        {
            using (var k = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run", true))
            {
                if (k == null) return;
                if (on) k.SetValue("MuLauncher", "\"" + Application.ExecutablePath + "\" --tray");
                else k.DeleteValue("MuLauncher", false);
            }
        }
        catch { }
    }

    // ---------- network / client package ----------
    internal static List<string> LocalIps()
    {
        var list = new List<string>();
        try
        {
            foreach (var ni in NetworkInterface.GetAllNetworkInterfaces())
            {
                if (ni.OperationalStatus != OperationalStatus.Up || ni.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
                foreach (var ua in ni.GetIPProperties().UnicastAddresses)
                    if (ua.Address.AddressFamily == AddressFamily.InterNetwork) list.Add(ua.Address.ToString());
            }
        }
        catch { }
        return list;
    }

    string ClientDir() { return Path.GetFullPath(Path.Combine(baseDir, @"..\Client")); }
    string EncoderDir() { return Path.GetFullPath(Path.Combine(baseDir, @"..\Encoder")); }

    // Point the server list and the client at `ip`: ServerList.dat, MainInfo.ini -> InfoEncoder -> ClientInfo.bmd.
    internal bool ApplyServerIp(string ip, out string msg)
    {
        IPAddress parsed;
        if (!IPAddress.TryParse(ip, out parsed) || parsed.AddressFamily != AddressFamily.InterNetwork) { msg = "Địa chỉ IP không hợp lệ."; return false; }
        string list = Path.Combine(baseDir, @"ConnectServer\ServerList.dat");
        string ini = Path.Combine(EncoderDir(), "MainInfo.ini");
        string enc = Path.Combine(EncoderDir(), "InfoEncoder.exe");
        string bmdSrc = Path.Combine(EncoderDir(), @"Client\Data\Local\ClientInfo.bmd");
        string bmdDst = Path.Combine(ClientDir(), @"Data\Local\ClientInfo.bmd");
        foreach (string f in new[] { list, ini, enc })
            if (!File.Exists(f)) { msg = "Không thấy file: " + f; return false; }

        Encoding latin = Encoding.GetEncoding(28591);
        string t = File.ReadAllText(list, latin);
        t = Regex.Replace(t, "^(\\s*\\d+\\s+\"[^\"]*\"\\s+)\"[^\"]*\"", "$1\"" + ip + "\"", RegexOptions.Multiline);
        File.WriteAllText(list, t, latin);

        string m = File.ReadAllText(ini, latin);
        m = Regex.Replace(m, "^IpAddress=.*$", "IpAddress=" + ip, RegexOptions.Multiline);
        File.WriteAllText(ini, m, latin);

        DateTime before = File.Exists(bmdSrc) ? File.GetLastWriteTime(bmdSrc) : DateTime.MinValue;
        var psi = new ProcessStartInfo(enc) { WorkingDirectory = EncoderDir(), UseShellExecute = false, CreateNoWindow = true };
        Process p;
        try { p = Process.Start(psi); } catch (Exception e) { msg = "Không chạy được InfoEncoder: " + e.Message; return false; }
        bool done = false;
        for (int i = 0; i < 40; i++)
        {
            Thread.Sleep(500);
            if (File.Exists(bmdSrc) && File.GetLastWriteTime(bmdSrc) > before) { Thread.Sleep(700); done = true; break; }
        }
        try { if (!p.HasExited) p.Kill(); } catch { }
        if (!done) { msg = "InfoEncoder không tạo ra ClientInfo.bmd mới (sau 20 giây)."; return false; }

        Directory.CreateDirectory(Path.GetDirectoryName(bmdDst));
        File.Copy(bmdSrc, bmdDst, true);

        Srv cs = servers[0];
        if (Running(cs)) { StopSrv(cs); StartSrv(cs); }
        msg = "Đã đặt địa chỉ server là " + ip + ".\nClient (Client\\Data\\Local\\ClientInfo.bmd) và ConnectServer đã được cập nhật.";
        Log("Đã đặt IP server: " + ip);
        return true;
    }

    internal void OpenFirewall()
    {
        string args = "/c netsh advfirewall firewall delete rule name=\"MuServer\" >nul 2>&1 & netsh advfirewall firewall add rule name=\"MuServer\" dir=in action=allow protocol=TCP localport=44405,55901 profile=private,domain";
        var psi = new ProcessStartInfo("cmd.exe", args) { Verb = "runas", UseShellExecute = true, WindowStyle = ProcessWindowStyle.Hidden };
        try
        {
            var p = Process.Start(psi);
            p.WaitForExit(20000);
            if (p.ExitCode == 0) { Log("Đã mở firewall cổng 44405, 55901"); MessageBox.Show("Đã mở firewall (TCP 44405 và 55901) cho mạng riêng/miền (private, domain).\nMạng public (quán cà phê...) vẫn bị chặn.", "Firewall", MessageBoxButtons.OK, MessageBoxIcon.Information); }
            else MessageBox.Show("Lệnh mở firewall báo lỗi (mã " + p.ExitCode + ").", "Firewall", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        catch (Win32Exception) { Log("Bạn đã từ chối quyền Administrator, firewall không đổi"); }
    }

    internal void ZipClientAsync(string ip)
    {
        string dir = ClientDir();
        if (!Directory.Exists(dir)) { MessageBox.Show("Không thấy thư mục Client.", "Đóng gói client"); return; }
        string zip = Path.Combine(Db.BackupDir, "MuClient_" + ip + "_" + DateTime.Now.ToString("yyyyMMdd_HHmm") + ".zip");
        Directory.CreateDirectory(Db.BackupDir);
        Log("Đang nén client...");
        ThreadPool.QueueUserWorkItem(delegate
        {
            string result;
            try
            {
                using (var za = ZipFile.Open(zip, ZipArchiveMode.Create))
                {
                    foreach (string f in Directory.GetFiles(dir, "*", SearchOption.AllDirectories))
                    {
                        string rel = f.Substring(dir.Length).TrimStart('\\');
                        if (rel.StartsWith("ScreenShots", StringComparison.OrdinalIgnoreCase)) continue;
                        string ext = Path.GetExtension(f).ToLowerInvariant();
                        if (ext == ".orig" || ext == ".new") continue;
                        za.CreateEntryFromFile(f, "MuClient/" + rel.Replace('\\', '/'), CompressionLevel.Fastest);
                    }
                }
                result = "Đã nén client:\n" + zip + "\n(" + (new FileInfo(zip).Length / 1048576) + " MB). Gửi file này cho người chơi, giải nén rồi chạy main.exe.";
            }
            catch (Exception e) { result = "Nén client lỗi: " + e.Message; try { File.Delete(zip); } catch { } }
            try
            {
                BeginInvoke(new Action(delegate
                {
                    Log("Nén client xong");
                    MessageBox.Show(result, "Đóng gói client", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    toolsPanel.RefreshBackups();
                }));
            }
            catch { }
        });
    }

    internal void Say(string m) { Log(m); }
}
