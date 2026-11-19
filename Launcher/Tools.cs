using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

// "Công cụ" tab: database backup / restore, automation options, server address + client package.
class ToolsPanel : Panel, IShown
{
    readonly MainForm form;
    readonly ListBox lb = new ListBox();
    readonly List<string> paths = new List<string>();
    readonly ComboBox cbIp = new ComboBox();
    bool loading;

    public ToolsPanel(MainForm f)
    {
        form = f;
        BackColor = Theme.Panel;
        AutoScroll = true;
        Padding = new Padding(14, 12, 14, 12);

        // ---- build cards (added in reverse: later Dock=Top controls sit above earlier ones) ----
        FlowLayoutPanel body;

        var net = K.Card("Mạng và gói client", out body);
        var r1 = K.Row(body);
        var l = K.Lbl("Địa chỉ server mà client kết nối:", true); l.Margin = new Padding(0, 6, 8, 0);
        r1.Controls.Add(l);
        cbIp.Width = 170; cbIp.BackColor = Theme.PanelHi; cbIp.ForeColor = Theme.Text; cbIp.FlatStyle = FlatStyle.Flat; cbIp.DropDownStyle = ComboBoxStyle.DropDown;
        cbIp.Items.Add("127.0.0.1");
        foreach (string ip in MainForm.LocalIps()) cbIp.Items.Add(ip);
        cbIp.Text = "127.0.0.1";
        r1.Controls.Add(cbIp);
        var r2 = K.Row(body);
        r2.Controls.Add(Btn("Áp dụng địa chỉ", Theme.Blue, false, delegate { ApplyIp(); }));
        r2.Controls.Add(Btn("Mở firewall (cần quyền Admin)", Theme.Muted, true, delegate { form.OpenFirewall(); }));
        r2.Controls.Add(Btn("Đóng gói client (.zip)", Theme.Muted, true, delegate { Zip(); }));
        var hint = K.Lbl("Chơi cùng máy: 127.0.0.1. Cho máy khác trong mạng LAN: chọn IP LAN của máy này, bấm Áp dụng, mở firewall, rồi đóng gói client gửi đi.", true);
        hint.MaximumSize = new Size(760, 0);
        body.Controls.Add(hint);

        var auto = K.Card("Tự động hóa", out body);
        var ckWd = K.Ck("Tự khởi động lại server khi bị sập (tối đa 3 lần trong 5 phút, có thông báo)", Settings.Watchdog);
        var ckTray = K.Ck("Nút đóng cửa sổ chỉ thu nhỏ xuống khay hệ thống (thoát thật bằng chuột phải biểu tượng khay)", Settings.MinToTray);
        var ckLogon = K.Ck("Chạy cùng Windows (tự mở dưới khay và tự chạy tất cả)", MainForm.GetRunAtLogon());
        foreach (var c in new[] { ckWd, ckTray, ckLogon }) { c.Margin = new Padding(0, 4, 0, 4); body.Controls.Add(c); }
        ckWd.CheckedChanged += delegate { if (!loading) { Settings.Watchdog = ckWd.Checked; Settings.Save(); } };
        ckTray.CheckedChanged += delegate { if (!loading) { Settings.MinToTray = ckTray.Checked; Settings.Save(); } };
        ckLogon.CheckedChanged += delegate { if (!loading) MainForm.SetRunAtLogon(ckLogon.Checked); };

        var bk = K.Card("Sao lưu database", out body);
        var b1 = K.Row(body);
        b1.Controls.Add(Btn("Sao lưu ngay", Theme.Green, false, delegate { BackupNow(); }));
        b1.Controls.Add(Btn("Khôi phục bản đã chọn", Theme.Red, true, delegate { Restore(); }));
        b1.Controls.Add(Btn("Mở thư mục", Theme.Muted, true, delegate { OpenDir(); }));
        lb.Width = 760; lb.Height = 150; lb.BackColor = Theme.Panel; lb.ForeColor = Theme.Text; lb.BorderStyle = BorderStyle.FixedSingle;
        lb.Font = K.Mono; lb.Margin = new Padding(0, 6, 0, 6);
        body.Controls.Add(lb);
        var b2 = K.Row(body);
        var ckStop = K.Ck("Tự sao lưu khi bấm Tắt tất cả", Settings.BackupOnStop);
        ckStop.Margin = new Padding(0, 4, 16, 0);
        b2.Controls.Add(ckStop);
        var l2 = K.Lbl("Định kỳ:", true); l2.Margin = new Padding(0, 5, 6, 0);
        b2.Controls.Add(l2);
        var cbEvery = K.Cb(110);
        cbEvery.Items.AddRange(new object[] { "Tắt", "Mỗi 1 giờ", "Mỗi 6 giờ", "Mỗi 24 giờ" });
        cbEvery.SelectedIndex = Settings.BackupEveryHours == 1 ? 1 : (Settings.BackupEveryHours == 6 ? 2 : (Settings.BackupEveryHours == 24 ? 3 : 0));
        b2.Controls.Add(cbEvery);
        var l3 = K.Lbl("Giữ tối đa", true); l3.Margin = new Padding(16, 5, 6, 0);
        b2.Controls.Add(l3);
        var numKeep = K.Num(3, 500, Math.Min(500, Math.Max(3, Settings.KeepBackups)), 60);
        b2.Controls.Add(numKeep);
        var l4 = K.Lbl("bản", true); l4.Margin = new Padding(6, 5, 0, 0);
        b2.Controls.Add(l4);
        ckStop.CheckedChanged += delegate { if (!loading) { Settings.BackupOnStop = ckStop.Checked; Settings.Save(); } };
        cbEvery.SelectedIndexChanged += delegate { if (!loading) { Settings.BackupEveryHours = new[] { 0, 1, 6, 24 }[cbEvery.SelectedIndex]; Settings.Save(); } };
        numKeep.ValueChanged += delegate { if (!loading) { Settings.KeepBackups = (int)numKeep.Value; Settings.Save(); } };

        Controls.Add(net);
        Controls.Add(auto);
        Controls.Add(bk);
        RefreshBackups();
    }

    RBtn Btn(string text, Color accent, bool ghost, EventHandler h)
    {
        var b = new RBtn(text, accent, ghost);
        b.Margin = new Padding(0, 0, 8, 0);
        b.Click += h;
        return b;
    }

    public void OnShown() { RefreshBackups(); }

    public void RefreshBackups()
    {
        paths.Clear();
        lb.Items.Clear();
        try
        {
            if (!Directory.Exists(Db.BackupDir)) { lb.Items.Add("(chưa có bản sao lưu nào)"); return; }
            var files = new List<string>(Directory.GetFiles(Db.BackupDir));
            files.Sort(StringComparer.OrdinalIgnoreCase);
            files.Reverse();
            foreach (string f in files)
            {
                string ext = Path.GetExtension(f).ToLowerInvariant();
                if (ext != ".sql" && ext != ".zip") continue;
                var fi = new FileInfo(f);
                paths.Add(f);
                lb.Items.Add(string.Format("{0,-44} {1,8:N1} MB  {2:dd/MM/yyyy HH:mm}", fi.Name, fi.Length / 1048576.0, fi.LastWriteTime));
            }
            if (paths.Count == 0) lb.Items.Add("(chưa có bản sao lưu nào)");
        }
        catch { }
    }

    void BackupNow()
    {
        if (!Db.Available) { MessageBox.Show("Chưa có mariadb\\mu.cnf nên không sao lưu được.", "Sao lưu"); return; }
        Cursor = Cursors.WaitCursor;
        string err;
        string p = form.DoBackup("thu-cong", out err);
        Cursor = Cursors.Default;
        RefreshBackups();
        if (p == null) MessageBox.Show("Sao lưu lỗi:\n" + err, "Sao lưu", MessageBoxButtons.OK, MessageBoxIcon.Error);
    }

    void Restore()
    {
        int i = lb.SelectedIndex;
        if (i < 0 || i >= paths.Count) { MessageBox.Show("Chọn một bản sao lưu (.sql) trong danh sách.", "Khôi phục"); return; }
        if (!paths[i].EndsWith(".sql", StringComparison.OrdinalIgnoreCase)) { MessageBox.Show("Chỉ khôi phục được file .sql.", "Khôi phục"); return; }
        form.RestoreBackup(paths[i]);
        RefreshBackups();
    }

    void OpenDir()
    {
        Directory.CreateDirectory(Db.BackupDir);
        Process.Start("explorer.exe", "\"" + Db.BackupDir + "\"");
    }

    void ApplyIp()
    {
        string ip = cbIp.Text.Trim();
        Cursor = Cursors.WaitCursor;
        string msg;
        bool ok;
        try { ok = form.ApplyServerIp(ip, out msg); }
        catch (Exception e) { ok = false; msg = e.Message; }
        Cursor = Cursors.Default;
        MessageBox.Show(msg, "Địa chỉ server", MessageBoxButtons.OK, ok ? MessageBoxIcon.Information : MessageBoxIcon.Error);
    }

    void Zip()
    {
        string ip = cbIp.Text.Trim();
        var r = MessageBox.Show("Client sẽ được nén với địa chỉ server hiện đang áp dụng.\nNếu chưa bấm \"Áp dụng địa chỉ\" cho " + ip + " thì bấm trước.\n\nNén bây giờ?", "Đóng gói client", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
        if (r == DialogResult.Yes) form.ZipClientAsync(ip);
    }
}
