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
    readonly NumericUpDown numLevel = K.Num(0, 255, 0, 60);
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
        grid.Columns.Add("lv", "Level");
        grid.Columns.Add("exp", "Hết hạn");
        grid.Columns.Add("blk", "Trạng thái");
        grid.Columns.Add("on", "Online");
        grid.Columns.Add("ip", "IP");
        grid.Columns.Add("hrs", "Giờ online");
        grid.Columns["lv"].FillWeight = 40;
        grid.Columns["hrs"].FillWeight = 50;
        grid.SelectionChanged += delegate { OnSelect(); };

        var gridHost = new Panel { Dock = DockStyle.Fill, Padding = new Padding(14, 0, 14, 0), BackColor = Theme.Panel };
        gridHost.Controls.Add(grid);

        var bottom = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 96, BackColor = Theme.Panel, Padding = new Padding(14, 10, 14, 0), WrapContents = true };
        var l1 = K.Lbl("Tài khoản", true); l1.Margin = new Padding(0, 8, 6, 0);
        var l2 = K.Lbl("Mật khẩu", true); l2.Margin = new Padding(10, 8, 6, 0);
        var l3 = K.Lbl("Level", true); l3.Margin = new Padding(10, 8, 6, 0);
        tbId.Margin = new Padding(0, 5, 0, 0); tbPw.Margin = new Padding(0, 5, 0, 0); numLevel.Margin = new Padding(0, 5, 14, 0);
        tbId.MaxLength = 10; tbPw.MaxLength = 10;
        bottom.Controls.AddRange(new Control[] { l1, tbId, l2, tbPw, l3, numLevel });
        bottom.Controls.Add(Btn("＋  Tạo tài khoản", Theme.Green, false, delegate { Create(); }));
        bottom.Controls.Add(Btn("Đổi mật khẩu", Theme.Muted, true, delegate { SetPassword(); }));
        bottom.Controls.Add(Btn("Đặt level", Theme.Muted, true, delegate { SetLevel(); }));
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
        var r = Db.Query("SELECT i.memb___id, i.AccountLevel, DATE_FORMAT(i.AccountExpireDate,'%Y-%m-%d'), i.bloc_code, IFNULL(s.ConnectStat,0), IFNULL(s.IP,''), IFNULL(s.OnlineHours,0) FROM MEMB_INFO i LEFT JOIN MEMB_STAT s ON s.memb___id=i.memb___id ORDER BY i.memb___id LIMIT 1000", out err);
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
            if (r.Length < 7) continue;
            bool on = r[4] == "1";
            if (on) online++;
            if (onlyOn && !on) continue;
            if (q.Length > 0 && r[0].IndexOf(q, StringComparison.OrdinalIgnoreCase) < 0) continue;
            bool blocked = r[3] == "1";
            int i = grid.Rows.Add(r[0], r[1], r[2], blocked ? "Bị khóa" : "Bình thường", on ? "● Online" : "", on ? r[5] : "", r[6]);
            if (blocked) grid.Rows[i].Cells[3].Style.ForeColor = Theme.Red;
            if (on) grid.Rows[i].Cells[4].Style.ForeColor = Theme.Green;
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
        Run("INSERT INTO MEMB_INFO (memb___id, memb__pwd, memb_name, sno__numb, bloc_code, AccountLevel, AccountExpireDate, Bloc_Expire, mail_addr) VALUES ('" + id + "','" + pw + "','" + id + "','111111111111111111','0'," + (int)numLevel.Value + ",'2099-12-31 00:00:00','1900-01-01 00:00:00',NULL)", "Đã tạo tài khoản " + id);
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

    void SetLevel()
    {
        string id = Existing(); if (id == null) return;
        Run("UPDATE MEMB_INFO SET AccountLevel=" + (int)numLevel.Value + " WHERE memb___id='" + id + "'", "Đã đặt level " + (int)numLevel.Value + " cho " + id);
    }

    void ToggleBlock()
    {
        string id = Existing(); if (id == null) return;
        bool blocked = false;
        foreach (var r in rows) if (r[0] == id && r.Length > 3) blocked = r[3] == "1";
        string q = blocked ? "Mở khóa tài khoản " + id + "?" : "Khóa tài khoản " + id + "? Người chơi sẽ không đăng nhập được.";
        if (MessageBox.Show(q, "Tài khoản", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        Run("UPDATE MEMB_INFO SET bloc_code='" + (blocked ? "0" : "1") + "' WHERE memb___id='" + id + "'", (blocked ? "Đã mở khóa " : "Đã khóa ") + id);
    }
}
