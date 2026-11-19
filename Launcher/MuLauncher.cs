using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

// MuLauncher: one window with a tab per MuServer process (ConnectServer, JoinServer, DataServer, GameServer).
// The servers are unmodified: their own windows are re-parented into the tabs.
// Put MuLauncher.exe in the MuServer folder (next to the ConnectServer/JoinServer/DataServer/GameServer folders).

class Srv
{
    public string Name;
    public Process P;
    public IntPtr Hwnd = IntPtr.Zero;
    public long OrigStyle;
    public bool Embedded;
    public Panel Host;
    public Panel Page;
    public TabBtn Btn;
    public bool Wanted;                                   // started by the launcher and not stopped by the user
    public List<DateTime> Restarts = new List<DateTime>(); // watchdog history
    public bool GaveUp;
    public DateTime ExitSeen = DateTime.MinValue;
    public IntPtr Menu = IntPtr.Zero;
    public MenuStrip Ms;
}

partial class MainForm : Form
{
    // ---- Win32 ----
    delegate bool EnumProc(IntPtr h, IntPtr lp);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr lp);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Auto)] static extern int GetClassName(IntPtr h, StringBuilder sb, int max);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] static extern IntPtr SetParent(IntPtr child, IntPtr parent);
    [DllImport("user32.dll")] static extern int GetWindowLong(IntPtr h, int idx);
    [DllImport("user32.dll")] static extern int SetWindowLong(IntPtr h, int idx, int val);
    [DllImport("user32.dll")] static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hgt, bool repaint);
    [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] static extern IntPtr GetMenu(IntPtr h);
    [DllImport("user32.dll")] static extern bool SetMenu(IntPtr h, IntPtr menu);
    [DllImport("user32.dll")] static extern IntPtr GetSubMenu(IntPtr menu, int pos);
    [DllImport("user32.dll")] static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] static extern uint GetMenuItemID(IntPtr menu, int pos);
    [DllImport("user32.dll")] static extern uint GetMenuState(IntPtr menu, uint pos, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Auto)] static extern int GetMenuString(IntPtr menu, uint pos, StringBuilder sb, int max, uint flags);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    const uint MF_BYPOSITION = 0x400;
    const uint MF_GRAYED = 0x1;
    const uint MF_DISABLED = 0x2;
    const uint MF_CHECKED = 0x8;
    const uint MF_SEPARATOR = 0x800;
    const uint WM_COMMAND = 0x0111;
    const uint WM_SIZE = 0x0005;

    const int GWL_STYLE = -16;
    const int WS_POPUP = unchecked((int)0x80000000);
    const int WS_CHILD = 0x40000000;
    const int WS_VISIBLE = 0x10000000;
    const int WS_CAPTION = 0x00C00000;
    const int WS_THICKFRAME = 0x00040000;
    const int WS_SYSMENU = 0x00080000;
    const int WS_MINIMIZEBOX = 0x00020000;
    const int WS_MAXIMIZEBOX = 0x00010000;
    const uint SWP_NOZORDER = 0x0004;
    const uint SWP_FRAMECHANGED = 0x0020;
    const uint SWP_SHOWWINDOW = 0x0040;

    // ---- State ----
    readonly string baseDir = AppDomain.CurrentDomain.BaseDirectory;
    readonly List<Srv> servers = new List<Srv>();
    readonly Queue<Srv> startQueue = new Queue<Srv>();
    DateTime nextStart = DateTime.MinValue;
    DateTime dbWaitSince = DateTime.MinValue;
    bool dbPending;
    Process dbProcess;
    const int DbPort = 3308;

    RBtn btnStopGame;
    int sel = 0;
    CheckBox chkDb;
    Label lblStatus, lblDb, lblGame, lblSrv;
    ToolTip tip = new ToolTip();
    Timer timer;
    string logPath;

    readonly List<TabBtn> allBtns = new List<TabBtn>();
    readonly List<Panel> allPages = new List<Panel>();
    Label lblOnline;

    public MainForm(bool autostart, bool trayStart)
    {
        logPath = Path.Combine(baseDir, "MuLauncher.log");
        Text = "MuServer Launcher";
        Size = new Size(1200, 760);
        MinimumSize = new Size(1080, 560);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        Font = new Font("Segoe UI", 9f);

        // ---------- toolbar ----------
        var bar = new Panel { Dock = DockStyle.Top, Height = 62, BackColor = Theme.Panel, Padding = new Padding(14, 0, 14, 0) };
        var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, BackColor = Theme.Panel, WrapContents = false, Padding = new Padding(0, 13, 0, 0) };

        flow.Controls.Add(MakeBtn("▶  Chạy tất cả", Theme.Green, false, "Bật MariaDB (nếu cần) rồi bật 4 server lần lượt", delegate { StartAll(); }));
        flow.Controls.Add(MakeBtn("■  Tắt tất cả", Theme.Red, true, "Tắt GameServer, DataServer, JoinServer, ConnectServer", delegate { StopAll(); }));
        flow.Controls.Add(Divider());
        flow.Controls.Add(MakeBtn("▶  Chạy tab", Theme.Muted, true, "Chạy server của tab đang chọn", delegate { StartSelected(); }));
        flow.Controls.Add(MakeBtn("■  Tắt tab", Theme.Muted, true, "Tắt server của tab đang chọn", delegate { StopSelected(); }));
        flow.Controls.Add(MakeBtn("⧉  Tách / Gắn", Theme.Muted, true, "Tách cửa sổ server ra riêng (có menu gốc) hoặc gắn lại vào tab", delegate { ToggleSelected(); }));
        flow.Controls.Add(Divider());
        flow.Controls.Add(MakeBtn("◆  Mở game", Theme.Blue, false, "Mở client game (thư mục Client)", delegate { StartGame(); }));
        btnStopGame = MakeBtn("Tắt game", Theme.Muted, true, "Tắt tất cả cửa sổ game", delegate { StopGame(); });
        flow.Controls.Add(btnStopGame);

        var right = new Panel { Dock = DockStyle.Right, Width = 150, BackColor = Theme.Panel };
        chkDb = new CheckBox { Text = "Tự bật MariaDB", Checked = true, AutoSize = true, ForeColor = Theme.Text, BackColor = Theme.Panel, FlatStyle = FlatStyle.Standard, Location = new Point(8, 22) };
        right.Controls.Add(chkDb);
        tip.SetToolTip(chkDb, "Khi bấm Chạy tất cả: tự bật MariaDB của MU nếu cổng 3308 chưa mở");
        bar.Controls.Add(flow);
        bar.Controls.Add(right);

        // ---------- tab strip ----------
        var strip = new Panel { Dock = DockStyle.Top, Height = 44, BackColor = Theme.Bg, Padding = new Padding(14, 8, 14, 0) };
        var tabFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, BackColor = Theme.Bg, WrapContents = false };
        strip.Controls.Add(tabFlow);

        // ---------- content card ----------
        var content = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Bg, Padding = new Padding(14, 0, 14, 8) };
        var card = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel, Padding = new Padding(1) };
        content.Controls.Add(card);

        int idx = 0;
        foreach (string n in new[] { "ConnectServer", "JoinServer", "DataServer", "GameServer" })
        {
            var s = new Srv { Name = n };
            s.Host = new Panel { Dock = DockStyle.Fill, BackColor = Color.Black };
            var sref = s;
            s.Host.Resize += delegate { Fit(sref); };
            s.Ms = new MenuStrip { Dock = DockStyle.Top, Visible = false, BackColor = Theme.Panel, ForeColor = Theme.Text, Renderer = new DarkRenderer(), Padding = new Padding(6, 4, 0, 4) };
            s.Page = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel, Visible = (idx == 0) };
            s.Page.Controls.Add(s.Host);
            s.Page.Controls.Add(s.Ms);
            card.Controls.Add(s.Page);

            int myIdx = idx;
            s.Btn = new TabBtn(n);
            s.Btn.Selected = (idx == 0);
            s.Btn.Click += delegate { SelectTab(myIdx); };
            tabFlow.Controls.Add(s.Btn);
            servers.Add(s);
            allBtns.Add(s.Btn);
            allPages.Add(s.Page);
            idx++;
        }
        AddExtraTabs(tabFlow, card);

        // ---------- status bar ----------
        var status = new Panel { Dock = DockStyle.Bottom, Height = 30, BackColor = Theme.Panel, Padding = new Padding(14, 0, 14, 0) };
        lblStatus = new Label { Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft, ForeColor = Theme.Muted, BackColor = Theme.Panel, Text = "Sẵn sàng" };
        var pills = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, BackColor = Theme.Panel, WrapContents = false, FlowDirection = FlowDirection.RightToLeft };
        lblGame = MakePill("Game: 0");
        lblDb = MakePill("MariaDB: tắt");
        lblSrv = MakePill("Server: 0/4");
        lblOnline = MakePill("Online: -");
        pills.Controls.Add(lblGame);
        pills.Controls.Add(lblOnline);
        pills.Controls.Add(lblDb);
        pills.Controls.Add(lblSrv);
        status.Controls.Add(lblStatus);
        status.Controls.Add(pills);

        Controls.Add(content);
        Controls.Add(strip);
        Controls.Add(bar);
        Controls.Add(status);

        timer = new Timer { Interval = 500 };
        timer.Tick += OnTick;
        timer.Start();

        InitFeatures(autostart, trayStart);
    }

    RBtn MakeBtn(string text, Color accent, bool ghost, string tooltip, EventHandler h)
    {
        var b = new RBtn(text, accent, ghost);
        b.Margin = new Padding(0, 0, 8, 0);
        b.Click += h;
        tip.SetToolTip(b, tooltip);
        return b;
    }

    Control Divider()
    {
        return new Panel { Width = 1, Height = 34, BackColor = Theme.Border, Margin = new Padding(6, 0, 14, 0) };
    }

    Label MakePill(string text)
    {
        return new Label { Text = text, AutoSize = true, ForeColor = Theme.Muted, BackColor = Theme.Panel, Padding = new Padding(10, 7, 0, 0), Margin = new Padding(0) };
    }

    void SelectTab(int i)
    {
        sel = i;
        for (int k = 0; k < allPages.Count; k++)
        {
            allPages[k].Visible = (k == i);
            allBtns[k].Selected = (k == i);
            allBtns[k].Invalidate();
        }
        var s = Selected();
        if (s != null) Refresh(s);
        OnTabShown(i);
    }

    void Log(string m)
    {
        try { File.AppendAllText(logPath, DateTime.Now.ToString("HH:mm:ss ") + m + Environment.NewLine, Encoding.UTF8); } catch { }
        lblStatus.Text = m;
    }

    static bool PortListening(int port)
    {
        foreach (var ep in IPGlobalProperties.GetIPGlobalProperties().GetActiveTcpListeners())
            if (ep.Port == port) return true;
        return false;
    }

    Srv Selected() { return sel < servers.Count ? servers[sel] : null; }
    static bool Running(Srv s) { return s.P != null && !s.P.HasExited; }

    // ---- Start / stop ----
    void StartAll()
    {
        startQueue.Clear();
        if (chkDb.Checked && !PortListening(DbPort))
        {
            StartDb();
        }
        foreach (var s in servers) if (!Running(s)) startQueue.Enqueue(s);
        nextStart = DateTime.Now;
    }

    void StartDb()
    {
        string exe = @"C:\Program Files\MariaDB 13.0\bin\mariadbd.exe";
        string ini = Path.Combine(baseDir, @"mariadb\data\my.ini");
        if (!File.Exists(exe) || !File.Exists(ini)) { Log("Không tìm thấy MariaDB/mariadb\\data\\my.ini"); return; }
        var psi = new ProcessStartInfo(exe, "--defaults-file=\"" + ini + "\" --bind-address=127.0.0.1 --skip-name-resolve");
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        try { dbProcess = Process.Start(psi); dbPending = true; dbWaitSince = DateTime.Now; Log("Đang bật MariaDB..."); }
        catch (Exception e) { Log("Lỗi bật MariaDB: " + e.Message); }
    }

    void StartSrv(Srv s)
    {
        if (Running(s)) return;
        string dir = Path.Combine(baseDir, s.Name);
        string exe = Path.Combine(dir, s.Name + ".exe");
        if (!File.Exists(exe)) { Log("Không thấy " + exe); return; }
        var psi = new ProcessStartInfo(exe);
        psi.WorkingDirectory = dir;
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        try { s.P = Process.Start(psi); s.Hwnd = IntPtr.Zero; s.Embedded = false; s.Wanted = true; s.GaveUp = false; Log("Đã chạy " + s.Name); }
        catch (Exception e) { Log("Lỗi chạy " + s.Name + ": " + e.Message); }
    }

    void StopSrv(Srv s)
    {
        s.Wanted = false;
        if (Running(s))
        {
            try { s.P.Kill(); s.P.WaitForExit(3000); } catch { }
            Log("Đã tắt " + s.Name);
        }
        s.Hwnd = IntPtr.Zero;
        s.Embedded = false;
    }

    void StopAll()
    {
        startQueue.Clear();
        bool any = false;
        foreach (var s in servers) if (Running(s)) any = true;
        for (int i = servers.Count - 1; i >= 0; i--) StopSrv(servers[i]); // Game, Data, Join, Connect
        if (any) AfterStopAll();
    }

    void StartSelected() { var s = Selected(); if (s != null) StartSrv(s); else Log("Tab này không phải server"); }
    void StopSelected() { var s = Selected(); if (s != null) StopSrv(s); else Log("Tab này không phải server"); }

    void StopDb()
    {
        string adm = @"C:\Program Files\MariaDB 13.0\bin\mariadb-admin.exe";
        string cnf = Path.Combine(baseDir, @"mariadb\root.cnf");
        if (!File.Exists(adm) || !File.Exists(cnf)) return;
        var psi = new ProcessStartInfo(adm, "--defaults-extra-file=\"" + cnf + "\" shutdown");
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        try { var p = Process.Start(psi); p.WaitForExit(15000); Log("Đã tắt MariaDB"); } catch { }
    }

    // ---- Game client (Client\main.exe) ----
    string ClientExe() { return Path.GetFullPath(Path.Combine(baseDir, @"..\Client\main.exe")); }

    List<Process> GameProcesses()
    {
        var list = new List<Process>();
        string exe = ClientExe();
        foreach (var p in Process.GetProcessesByName("main"))
        {
            try { if (string.Equals(p.MainModule.FileName, exe, StringComparison.OrdinalIgnoreCase)) list.Add(p); } catch { }
        }
        return list;
    }

    void StartGame()
    {
        string exe = ClientExe();
        if (!File.Exists(exe)) { Log("Không thấy client: " + exe); return; }
        bool ready = Running(servers[0]) && Running(servers[3]);
        var psi = new ProcessStartInfo(exe);
        psi.WorkingDirectory = Path.GetDirectoryName(exe);
        psi.UseShellExecute = false;
        try { Process.Start(psi); Log(ready ? "Đã mở game" : "Đã mở game (nhưng ConnectServer/GameServer chưa chạy)"); }
        catch (Exception e) { Log("Lỗi mở game: " + e.Message); }
    }

    void StopGame()
    {
        int n = 0;
        foreach (var p in GameProcesses()) { try { p.Kill(); n++; } catch { } }
        Log(n > 0 ? "Đã tắt " + n + " cửa sổ game" : "Không có game nào đang chạy");
    }

    // ---- Window embedding ----
    IntPtr FindWindowOf(int pid)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate (IntPtr h, IntPtr lp)
        {
            uint wp;
            GetWindowThreadProcessId(h, out wp);
            if (wp != (uint)pid) return true;
            var sb = new StringBuilder(256);
            GetClassName(h, sb, 256);
            string cls = sb.ToString();
            if (cls == "ConsoleWindowClass" || cls.StartsWith("CASCADIA") || cls == "IME" || cls == "MSCTFIME UI") return true;
            if (!IsWindowVisible(h)) return true;
            found = h;
            return false;
        }, IntPtr.Zero);
        return found;
    }

    void Embed(Srv s)
    {
        s.Menu = GetMenu(s.Hwnd);
        BuildMenu(s);
        int style = GetWindowLong(s.Hwnd, GWL_STYLE);
        s.OrigStyle = style;
        style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        style |= WS_CHILD | WS_VISIBLE;
        SetWindowLong(s.Hwnd, GWL_STYLE, style);
        SetParent(s.Hwnd, s.Host.Handle);
        s.Embedded = true;
        SetWindowPos(s.Hwnd, IntPtr.Zero, 0, 0, s.Host.ClientSize.Width, s.Host.ClientSize.Height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        Log("Đã gắn " + s.Name + " vào tab");
    }

    void Detach(Srv s)
    {
        if (s.Hwnd == IntPtr.Zero || !IsWindow(s.Hwnd) || !s.Embedded) return;
        SetParent(s.Hwnd, IntPtr.Zero);
        SetWindowLong(s.Hwnd, GWL_STYLE, (int)s.OrigStyle);
        if (s.Menu != IntPtr.Zero) SetMenu(s.Hwnd, s.Menu);
        s.Ms.Visible = false;
        s.Ms.Items.Clear();
        SetWindowPos(s.Hwnd, IntPtr.Zero, 80, 80, 700, 600, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        s.Embedded = false;
        Log("Đã tách " + s.Name + " ra cửa sổ riêng (có menu)");
    }

    void ToggleSelected()
    {
        var s = Selected();
        if (s == null) { Log("Tab này không phải server"); return; }
        if (s.Hwnd == IntPtr.Zero || !IsWindow(s.Hwnd)) { Log(s.Name + " chưa chạy"); return; }
        if (s.Embedded) Detach(s); else Embed(s);
    }

    // Mirror the server's own menu bar into the tab (child windows cannot show a menu bar).
    void BuildMenu(Srv s)
    {
        s.Ms.Items.Clear();
        if (s.Menu == IntPtr.Zero) return;
        int n = GetMenuItemCount(s.Menu);
        for (int i = 0; i < n; i++)
        {
            var item = MakeItem(s, s.Menu, i);
            if (item != null) s.Ms.Items.Add(item);
        }
        s.Ms.Visible = s.Ms.Items.Count > 0;
    }

    ToolStripItem MakeItem(Srv s, IntPtr menu, int pos)
    {
        if ((GetMenuState(menu, (uint)pos, MF_BYPOSITION) & MF_SEPARATOR) != 0) return new ToolStripSeparator();
        var sb = new StringBuilder(256);
        GetMenuString(menu, (uint)pos, sb, 256, MF_BYPOSITION);
        string text = sb.ToString().Replace("	", "    ");
        var mi = new ToolStripMenuItem(text);
        IntPtr sub = GetSubMenu(menu, pos);
        if (sub != IntPtr.Zero)
        {
            int cnt = GetMenuItemCount(sub);
            for (int j = 0; j < cnt; j++) mi.DropDownItems.Add(MakeItem(s, sub, j));
            var sref = s; var menuRef = menu; int posRef = pos;
            mi.DropDownOpening += delegate { RefreshStates(mi, sub); };
        }
        else
        {
            uint id = GetMenuItemID(menu, pos);
            var sref = s;
            mi.Tag = new int[] { pos };
            mi.Click += delegate { if (sref.Hwnd != IntPtr.Zero) PostMessage(sref.Hwnd, WM_COMMAND, (IntPtr)id, IntPtr.Zero); };
        }
        return mi;
    }

    void RefreshStates(ToolStripMenuItem parent, IntPtr sub)
    {
        for (int j = 0; j < parent.DropDownItems.Count; j++)
        {
            var it = parent.DropDownItems[j] as ToolStripMenuItem;
            if (it == null) continue;
            uint st = GetMenuState(sub, (uint)j, MF_BYPOSITION);
            if (st == 0xFFFFFFFF) continue;
            it.Checked = (st & MF_CHECKED) != 0;
            it.Enabled = (st & (MF_GRAYED | MF_DISABLED)) == 0;
        }
    }

    // The servers paint their log with GetDC() (no WM_PAINT handler), so anything that wipes the window
    // (switching tabs, resizing, layout changes) leaves it black until the server repaints on WM_SIZE.
    void Refresh(Srv s)
    {
        if (s.Embedded && s.Hwnd != IntPtr.Zero && IsWindow(s.Hwnd))
        {
            int w = s.Host.ClientSize.Width, h = s.Host.ClientSize.Height;
            PostMessage(s.Hwnd, WM_SIZE, IntPtr.Zero, (IntPtr)((h << 16) | (w & 0xFFFF)));
        }
    }

    void Fit(Srv s)
    {
        if (s.Embedded && s.Hwnd != IntPtr.Zero && IsWindow(s.Hwnd))
            MoveWindow(s.Hwnd, 0, 0, s.Host.ClientSize.Width, s.Host.ClientSize.Height, true);
        Refresh(s);
    }

    // ---- Timer ----
    void OnTick(object sender, EventArgs e)
    {
        if (dbPending)
        {
            if (PortListening(DbPort)) { dbPending = false; Log("MariaDB sẵn sàng"); }
            else if ((DateTime.Now - dbWaitSince).TotalSeconds > 40) { dbPending = false; Log("MariaDB không lên được sau 40 giây"); }
        }
        else if (startQueue.Count > 0 && DateTime.Now >= nextStart)
        {
            StartSrv(startQueue.Dequeue());
            nextStart = DateTime.Now.AddSeconds(2);
        }

        var curSrv = Selected();
        if (curSrv != null) Refresh(curSrv);

        int games = GameProcesses().Count;
        string gt = games > 0 ? "Tắt game (" + games + ")" : "Tắt game";
        if (btnStopGame.Text != gt) btnStopGame.Text = gt;
        lblGame.Text = "Game: " + games;
        lblGame.ForeColor = games > 0 ? Theme.Blue : Theme.Muted;
        bool dbUp = PortListening(DbPort);
        lblDb.Text = "MariaDB: " + (dbUp ? "chạy" : (dbPending ? "đang bật…" : "tắt"));
        lblDb.ForeColor = dbUp ? Theme.Green : (dbPending ? Theme.Amber : Theme.Red);
        int runCount = 0;
        foreach (var sv in servers) if (Running(sv)) runCount++;
        lblSrv.Text = "Server: " + runCount + "/4";
        lblSrv.ForeColor = runCount == 4 ? Theme.Green : (runCount > 0 ? Theme.Amber : Theme.Muted);

        foreach (var s in servers)
        {
            bool run = Running(s);
            if (run && s.Hwnd == IntPtr.Zero)
            {
                IntPtr h = FindWindowOf(s.P.Id);
                if (h != IntPtr.Zero) { s.Hwnd = h; Embed(s); }
            }
            if (!run) { s.Hwnd = IntPtr.Zero; s.Embedded = false; }
            if (s.Btn.Running != run) { s.Btn.Running = run; s.Btn.Invalidate(); }
        }

        FeatureTick();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        if (HideToTrayInstead(e)) return;
        bool any = false;
        foreach (var s in servers) if (Running(s)) any = true;
        if (any || (dbProcess != null && !dbProcess.HasExited))
        {
            var r = MessageBox.Show("Tắt tất cả server khi thoát?\n\nYes = tắt hết (và MariaDB nếu launcher đã bật)\nNo = thoát launcher, server vẫn chạy (cửa sổ được tách ra)\nCancel = ở lại",
                "MuServer Launcher", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);
            if (r == DialogResult.Cancel) { e.Cancel = true; return; }
            if (r == DialogResult.Yes)
            {
                StopGame();
                StopAll();
                if (dbProcess != null) StopDb();
            }
            else
            {
                foreach (var s in servers) Detach(s);
            }
        }
        base.OnFormClosing(e);
    }
}

static class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        bool tray = Array.IndexOf(args, "--tray") >= 0;
        bool auto = tray || Array.IndexOf(args, "--autostart") >= 0;
        Application.Run(new MainForm(auto, tray));
    }
}
