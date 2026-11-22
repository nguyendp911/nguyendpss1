using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows.Forms;

// "Tài khoản" tab: list / create accounts, change password, lock, set account level (MEMB_INFO + MEMB_STAT).
class AccountPanel : Panel, IShown
{
    readonly MainForm form;
    readonly DataGridView grid = new DataGridView();
    readonly TextBox tbSearch = K.Tb(160), tbId = K.Tb(110), tbPw = K.Tb(110);
    readonly CheckBox ckOnline = K.Ck("Chỉ online", false);
    readonly NumericUpDown numLevel = K.Num(0, 255, 0, 56);
    readonly NumericUpDown numWCoin = K.Num(0, 2000000000, 10000, 92);
    readonly NumericUpDown numVipDays = K.Num(1, 365, 30, 56);
    readonly Label lblInfo = K.Lbl("", true);
    List<string[]> rows = new List<string[]>();

    public AccountPanel(MainForm f)
    {
        form = f;
        BackColor = Theme.Panel;

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 50, BackColor = Theme.Panel, Padding = new Padding(14, 8, 14, 0), WrapContents = false };
        var lbl = K.Lbl("Tìm", true); lbl.Margin = new Padding(0, 8, 6, 0);
        top.Controls.Add(lbl);
        tbSearch.Margin = new Padding(0, 5, 12, 0);
        top.Controls.Add(tbSearch);
        ckOnline.Margin = new Padding(0, 7, 12, 0);
        top.Controls.Add(ckOnline);
        top.Controls.Add(Btn("Tải lại", Theme.Muted, true, delegate { Reload(); }));
        lblInfo.Margin = new Padding(8, 8, 0, 0);
        top.Controls.Add(lblInfo);

        K.StyleGrid(grid);
        grid.Dock = DockStyle.Fill;
        grid.Columns.Add("id", "Tài khoản");
        grid.Columns.Add("lv", "VIP Lv");
        grid.Columns.Add("exp", "Hết hạn VIP");
        grid.Columns.Add("wc", "WCoin");
        grid.Columns.Add("blk", "Trạng thái");
        grid.Columns.Add("on", "Online");
        grid.Columns.Add("ip", "IP");
        grid.Columns.Add("hrs", "Giờ online");
        grid.Columns["lv"].FillWeight = 40;
        grid.Columns["wc"].FillWeight = 65;
        grid.Columns["hrs"].FillWeight = 50;
        grid.SelectionChanged += delegate { OnSelect(); };

        var gridHost = new Panel { Dock = DockStyle.Fill, Padding = new Padding(14, 0, 14, 0), BackColor = Theme.Panel };
        gridHost.Controls.Add(grid);

        var bottom = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 128, BackColor = Theme.Panel, Padding = new Padding(14, 8, 14, 0), WrapContents = true };
        var l1 = K.Lbl("Tài khoản", true); l1.Margin = new Padding(0, 8, 6, 0);
        var l2 = K.Lbl("Mật khẩu", true); l2.Margin = new Padding(8, 8, 6, 0);
        var l3 = K.Lbl("VIP Lv", true); l3.Margin = new Padding(8, 8, 6, 0);
        var l4 = K.Lbl("WCoin", true); l4.Margin = new Padding(8, 8, 6, 0);
        var l5 = K.Lbl("Ngày VIP", true); l5.Margin = new Padding(8, 8, 6, 0);
        tbId.Margin = new Padding(0, 5, 0, 0); tbPw.Margin = new Padding(0, 5, 0, 0); numLevel.Margin = new Padding(0, 5, 0, 0);
        numWCoin.Margin = new Padding(0, 5, 0, 0); numVipDays.Margin = new Padding(0, 5, 10, 0);
        tbId.MaxLength = 10; tbPw.MaxLength = 10;
        bottom.Controls.AddRange(new Control[] { l1, tbId, l2, tbPw, l3, numLevel, l4, numWCoin, l5, numVipDays });
        bottom.Controls.Add(Btn("＋  Tạo tài khoản", Theme.Green, false, delegate { Create(); }));
        bottom.Controls.Add(Btn("Đổi mật khẩu", Theme.Muted, true, delegate { SetPassword(); }));
        bottom.Controls.Add(Btn("＋ Cộng WCoin", Theme.Blue, false, delegate { AddWCoin(); }));
        bottom.Controls.Add(Btn("Đặt WCoin", Theme.Muted, true, delegate { SetWCoin(); }));
        bottom.Controls.Add(Btn("Cấp VIP (ngày)", Theme.Amber, false, delegate { GrantVipDays(); }));
        bottom.Controls.Add(Btn("Hủy VIP", Theme.Muted, true, delegate { RemoveVip(); }));
        bottom.Controls.Add(Btn("Khóa / Mở khóa", Theme.Red, true, delegate { ToggleBlock(); }));

        Controls.Add(gridHost);
        Controls.Add(bottom);
        Controls.Add(top);

        tbSearch.TextChanged += delegate { Fill(); };
        ckOnline.CheckedChanged += delegate { Fill(); };
    }

    RBtn Btn(string text, Color accent, bool ghost, EventHandler h)
    {
        var b = new RBtn(text, accent, ghost);
        b.Margin = new Padding(0, 0, 8, 0);
        b.Click += h;
        return b;
    }

    public void OnShown() { Reload(); }

    void Reload()
    {
        if (!Db.Available) { lblInfo.Text = "Chưa có mariadb\\mu.cnf"; return; }
        string err;
        var r = Db.Query("SELECT i.memb___id, i.AccountLevel, DATE_FORMAT(i.AccountExpireDate,'%Y-%m-%d %H:%i'), COALESCE(i.WCoin,0), i.bloc_code, IFNULL(s.ConnectStat,0), IFNULL(s.IP,''), IFNULL(s.OnlineHours,0) FROM MEMB_INFO i LEFT JOIN MEMB_STAT s ON s.memb___id=i.memb___id ORDER BY i.memb___id LIMIT 1000", out err);
        if (err != null) { lblInfo.Text = "Lỗi: " + err; return; }
        rows = r;
        Fill();
    }

    void Fill()
    {
        string q = tbSearch.Text.Trim();
        bool onlyOn = ckOnline.Checked;
        string selId = tbId.Text;
        grid.Rows.Clear();
        int online = 0;
        foreach (var r in rows)
        {
            if (r.Length < 8) continue;
            bool on = r[5] == "1";
            if (on) online++;
            if (onlyOn && !on) continue;
            if (q.Length > 0 && r[0].IndexOf(q, StringComparison.OrdinalIgnoreCase) < 0) continue;
            bool blocked = r[4] == "1";
            int i = grid.Rows.Add(r[0], r[1], r[2], r[3], blocked ? "Bị khóa" : "Bình thường", on ? "● Online" : "", on ? r[6] : "", r[7]);
            if (blocked) grid.Rows[i].Cells[4].Style.ForeColor = Theme.Red;
            if (on) grid.Rows[i].Cells[5].Style.ForeColor = Theme.Green;
        }
        lblInfo.Text = rows.Count + " tài khoản · " + online + " đang online";
    }

    void OnSelect()
    {
        if (grid.SelectedRows.Count == 0) return;
        var row = grid.SelectedRows[0];
        tbId.Text = Convert.ToString(row.Cells[0].Value);
        int lv;
        if (int.TryParse(Convert.ToString(row.Cells[1].Value), out lv) && lv >= 0 && lv <= 255) numLevel.Value = lv;
    }

    static bool Md5Enabled()
    {
        try
        {
            string ini = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"JoinServer\JoinServer.ini");
            var m = Regex.Match(File.ReadAllText(ini), @"^MD5Encryption=(\d+)", RegexOptions.Multiline);
            return m.Success && m.Groups[1].Value != "0";
        }
        catch { return false; }
    }

    bool CheckName(string v, string what)
    {
        if (Db.IsSafeName(v)) return true;
        MessageBox.Show(what + " chỉ gồm chữ không dấu, số, dấu gạch dưới và tối đa 10 ký tự.", "Tài khoản", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        return false;
    }

    bool Run(string sql, string okMsg)
    {
        string err;
        Db.Exec(sql, out err);
        if (err != null) { MessageBox.Show("Lỗi: " + err, "Tài khoản", MessageBoxButtons.OK, MessageBoxIcon.Error); return false; }
        form.Say(okMsg);
        Reload();
        return true;
    }

    void Create()
    {
        string id = tbId.Text.Trim(), pw = tbPw.Text.Trim();
        if (!CheckName(id, "Tên tài khoản") || !CheckName(pw, "Mật khẩu")) return;
        if (Md5Enabled()) { MessageBox.Show("JoinServer đang bật mã hóa mật khẩu (MD5Encryption khác 0).\nLauncher chỉ tạo tài khoản được khi MD5Encryption=0.", "Tài khoản", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
        foreach (var r in rows) if (r.Length > 0 && string.Equals(r[0], id, StringComparison.OrdinalIgnoreCase)) { MessageBox.Show("Tài khoản \"" + id + "\" đã tồn tại.", "Tài khoản", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
        Run("INSERT INTO MEMB_INFO (memb___id, memb__pwd, memb_name, sno__numb, bloc_code, AccountLevel, AccountExpireDate, Bloc_Expire, mail_addr, WCoin) VALUES ('" + id + "','" + pw + "','" + id + "','111111111111111111','0'," + (int)numLevel.Value + ",NOW(),'1900-01-01 00:00:00',NULL,0)", "Đã tạo tài khoản " + id);
    }

    string Existing()
    {
        string id = tbId.Text.Trim();
        if (!CheckName(id, "Tên tài khoản")) return null;
        foreach (var r in rows) if (r.Length > 0 && string.Equals(r[0], id, StringComparison.OrdinalIgnoreCase)) return r[0];
        MessageBox.Show("Không có tài khoản \"" + id + "\". Chọn một dòng trong danh sách.", "Tài khoản", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        return null;
    }

    void SetPassword()
    {
        string id = Existing(); if (id == null) return;
        string pw = tbPw.Text.Trim();
        if (!CheckName(pw, "Mật khẩu")) return;
        if (Md5Enabled()) { MessageBox.Show("JoinServer đang bật mã hóa mật khẩu (MD5Encryption khác 0), không đổi được từ đây.", "Tài khoản", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
        Run("UPDATE MEMB_INFO SET memb__pwd='" + pw + "' WHERE memb___id='" + id + "'", "Đã đổi mật khẩu " + id);
    }

    void AddWCoin()
    {
        string id = Existing(); if (id == null) return;
        long amt = (long)numWCoin.Value;
        Run("UPDATE MEMB_INFO SET WCoin = COALESCE(WCoin,0) + " + amt + " WHERE memb___id='" + id + "'", "Đã cộng " + amt + " WCoin cho " + id);
    }

    void SetWCoin()
    {
        string id = Existing(); if (id == null) return;
        long amt = (long)numWCoin.Value;
        Run("UPDATE MEMB_INFO SET WCoin = " + amt + " WHERE memb___id='" + id + "'", "Đã đặt " + amt + " WCoin cho " + id);
    }

    void GrantVipDays()
    {
        string id = Existing(); if (id == null) return;
        int lv = Math.Max(1, (int)numLevel.Value);
        int days = (int)numVipDays.Value;
        Run("UPDATE MEMB_INFO SET AccountLevel=" + lv + ", AccountExpireDate=DATE_ADD(NOW(), INTERVAL " + days + " DAY) WHERE memb___id='" + id + "'", "Đã cấp VIP Lv" + lv + " (" + days + " ngày) cho " + id);
    }

    void RemoveVip()
    {
        string id = Existing(); if (id == null) return;
        Run("UPDATE MEMB_INFO SET AccountLevel=0, AccountExpireDate=NOW() WHERE memb___id='" + id + "'", "Đã hủy VIP của " + id);
    }

    void ToggleBlock()
    {
        string id = Existing(); if (id == null) return;
        bool blocked = false;
        foreach (var r in rows) if (r[0] == id && r.Length > 4) blocked = r[4] == "1";
        string q = blocked ? "Mở khóa tài khoản " + id + "?" : "Khóa tài khoản " + id + "? Người chơi sẽ không đăng nhập được.";
        if (MessageBox.Show(q, "Tài khoản", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        Run("UPDATE MEMB_INFO SET bloc_code='" + (blocked ? "0" : "1") + "' WHERE memb___id='" + id + "'", (blocked ? "Đã mở khóa " : "Đã khóa ") + id);
    }
}
