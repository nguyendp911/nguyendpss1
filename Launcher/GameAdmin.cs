using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

// Item definition parsed from MuServer\Data\Item\Item.txt
class MuItemDef
{
    public int Section;
    public int Index;
    public string Name;
    public override string ToString() { return string.Format("[{0},{1}] {2}", Section, Index, Name); }
}

class CashShopRowData
{
    public int Type = 0;       // 0=Item, 1=VIP
    public int Category = 2;   // 1..4
    public int Section = 14;
    public int Index = 13;
    public int Level = 0;
    public int Dur = 1;
    public int Skill = 0;
    public int Luck = 0;
    public int Opt = 0;        // 0..4 (+0..+16)
    public int Exc = 0;        // bitmask 0..63
    public int Param = 1;      // VIP days
    public int Price = 500;
    public string Name = "";
    public string Desc = "";

    public string AttrSummary()
    {
        if (Type == 1) return Param + " ngày VIP";
        if (Type == 2) return "Đổi Class (DW/DK/ELF/MG)";
        var parts = new List<string>();
        if (Level > 0) parts.Add("+" + Level);
        if (Skill != 0) parts.Add("Skill");
        if (Luck != 0) parts.Add("Luck");
        if (Opt > 0) parts.Add("Opt+" + (Opt * 4));
        if (Exc > 0)
        {
            int cnt = 0;
            for (int b = 0; b < 6; b++) if ((Exc & (1 << b)) != 0) cnt++;
            parts.Add("Exc(" + cnt + " dòng)");
        }
        return parts.Count > 0 ? string.Join(" ", parts.ToArray()) : "Thường";
    }
}

// Interactive dark dialog for adding / editing a Cash Shop product with full item picker & attributes
class CashShopItemDialog : Form
{
    static readonly string[] SectionNames = {
        "0 - Swords (Kiếm)", "1 - Axes (Rìu)", "2 - Maces (Chùy)", "3 - Spears (Giáo)",
        "4 - Bows / Crossbows (Cung)", "5 - Staffs (Gậy)", "6 - Shields (Khiên)",
        "7 - Helms (Mũ)", "8 - Armors (Áo)", "9 - Pants (Quần)", "10 - Gloves (Găng)",
        "11 - Boots (Giày)", "12 - Wings / Orbs / Chaos", "13 - Pets / Rings / Pendants",
        "14 - Jewels / Potions / Tickets", "15 - Scrolls (Sách phép)"
    };

    static readonly string[] CatNames = {
        "1 - Pets & Mounts", "2 - Jewels & Items", "3 - Event Tickets", "4 - VIP Services"
    };

    static readonly string[] ExcNames = {
        "Exc 1: +10% Sát thương hoàn hảo / +4% Max HP",
        "Exc 2: +2% Sát thương / +4% Max Mana",
        "Exc 3: +Cấp độ/20 sát thương / -4% Giảm sát thương",
        "Exc 4: +7 Tốc độ tấn công / +5% Phản đòn",
        "Exc 5: +Hồi HP/8 khi giết quái / +10% Tỉ lệ phòng thủ",
        "Exc 6: +Hồi Mana/8 khi giết quái / +40% Zen rơi"
    };

    readonly List<MuItemDef> catalog;
    public CashShopRowData Data;

    ComboBox cbType, cbCat, cbSec;
    TextBox tbSearch, tbName, tbDesc;
    ListBox lbItems;
    NumericUpDown numSec, numIdx, numLv, numDur, numOpt, numVipDays, numPrice;
    CheckBox ckSkill, ckLuck;
    CheckBox[] ckExc = new CheckBox[6];
    Panel pnlItemPicker, pnlAttrs, pnlVip;

    public CashShopItemDialog(List<MuItemDef> items, CashShopRowData existing)
    {
        catalog = items;
        Data = existing ?? new CashShopRowData();

        Text = existing == null ? "Thêm sản phẩm Cash Shop" : "Chỉnh sửa sản phẩm Cash Shop";
        Size = new Size(640, 585);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        StartPosition = FormStartPosition.CenterParent;
        BackColor = Theme.Panel;
        ForeColor = Theme.Text;
        Font = new Font("Segoe UI", 9f);

        int y = 12;

        // Row 1: Type + Category + Price
        AddLbl("Loại SP:", 14, y + 4);
        cbType = MakeCombo(74, y, 134, new[] { "0 - Vật phẩm", "1 - Gói VIP", "2 - Đổi Class" });
        cbType.SelectedIndex = Math.Max(0, Math.Min(2, Data.Type));

        AddLbl("Danh mục:", 216, y + 4);
        cbCat = MakeCombo(284, y, 152, CatNames);
        cbCat.SelectedIndex = Math.Max(0, Math.Min(3, Data.Category - 1));

        AddLbl("Giá WCoin:", 448, y + 4);
        numPrice = K.Num(1, 2000000000, Math.Max(1, Data.Price), 94);
        numPrice.Location = new Point(518, y);
        Controls.Add(numPrice);

        y += 34;

        // Row 2: Name + Desc
        AddLbl("Tên hiển thị:", 14, y + 4);
        tbName = K.Tb(210); tbName.Location = new Point(92, y); tbName.MaxLength = 31; tbName.Text = Data.Name;
        Controls.Add(tbName);

        AddLbl("Mô tả:", 314, y + 4);
        tbDesc = K.Tb(254); tbDesc.Location = new Point(358, y); tbDesc.MaxLength = 59; tbDesc.Text = Data.Desc;
        Controls.Add(tbDesc);

        y += 36;

        // Panel VIP Days (shown when Type == 1)
        pnlVip = new Panel { Location = new Point(14, y), Size = new Size(598, 44), BackColor = Theme.Bg };
        var lv1 = K.Lbl("Số ngày VIP kích hoạt:", true); lv1.Location = new Point(12, 13);
        numVipDays = K.Num(1, 365, Math.Max(1, Data.Param), 80); numVipDays.Location = new Point(160, 10);
        var lv2 = K.Lbl("Gói VIP sẽ kích hoạt AccountLevel=1 và cộng đúng số ngày vào thời hạn VIP.", true); lv2.Location = new Point(252, 13);
        pnlVip.Controls.AddRange(new Control[] { lv1, numVipDays, lv2 });
        Controls.Add(pnlVip);

        // Panel Item Picker (shown when Type == 0)
        pnlItemPicker = new Panel { Location = new Point(14, y), Size = new Size(598, 198), BackColor = Theme.Bg };
        var lpTitle = new Label { Text = "Chọn vật phẩm từ Item.txt", Location = new Point(10, 8), AutoSize = true, ForeColor = Theme.Blue, Font = new Font("Segoe UI Semibold", 9f) };
        var lpSec = K.Lbl("Nhóm:", true); lpSec.Location = new Point(10, 34);
        cbSec = MakeCombo(58, 30, 210, SectionNames);
        cbSec.Parent = pnlItemPicker;
        cbSec.SelectedIndex = Math.Max(0, Math.Min(15, Data.Section));

        var lpFind = K.Lbl("Tìm nhanh:", true); lpFind.Location = new Point(280, 34);
        tbSearch = K.Tb(140); tbSearch.Location = new Point(350, 30);
        var lpMan = K.Lbl("Sec / Idx:", true); lpMan.Location = new Point(496, 34);
        numSec = K.Num(0, 15, Math.Max(0, Math.Min(15, Data.Section)), 44); numSec.Location = new Point(496, 56);
        numIdx = K.Num(0, 511, Math.Max(0, Math.Min(511, Data.Index)), 44); numIdx.Location = new Point(544, 56);

        lbItems = new ListBox
        {
            Location = new Point(10, 60),
            Size = new Size(478, 128),
            BackColor = Theme.Panel,
            ForeColor = Theme.Text,
            BorderStyle = BorderStyle.FixedSingle,
            IntegralHeight = false
        };

        pnlItemPicker.Controls.AddRange(new Control[] { lpTitle, lpSec, lpFind, tbSearch, lpMan, numSec, numIdx, lbItems });
        Controls.Add(pnlItemPicker);

        // Panel Item Attributes (Level, Dur, Opt, Skill, Luck, 6 Excellent options)
        pnlAttrs = new Panel { Location = new Point(14, y + 206), Size = new Size(598, 206), BackColor = Theme.Bg };
        var laTitle = new Label { Text = "Thuộc tính vật phẩm (Level, Skill, Luck, Option, Dòng Hoàn Hảo)", Location = new Point(10, 8), AutoSize = true, ForeColor = Theme.Amber, Font = new Font("Segoe UI Semibold", 9f) };

        var laLv = K.Lbl("Cấp (+0..11):", true); laLv.Location = new Point(10, 36);
        numLv = K.Num(0, 15, Math.Max(0, Math.Min(15, Data.Level)), 58); numLv.Location = new Point(92, 33);

        var laDur = K.Lbl("Độ bền (Dur):", true); laDur.Location = new Point(164, 36);
        numDur = K.Num(0, 255, Math.Max(0, Math.Min(255, Data.Dur)), 62); numDur.Location = new Point(246, 33);

        var laOpt = K.Lbl("Opt (0..4 = +0..16):", true); laOpt.Location = new Point(322, 36);
        numOpt = K.Num(0, 7, Math.Max(0, Math.Min(7, Data.Opt)), 54); numOpt.Location = new Point(434, 33);

        ckSkill = K.Ck("Có Skill", Data.Skill != 0); ckSkill.Location = new Point(502, 35);
        ckLuck = K.Ck("Có Luck", Data.Luck != 0); ckLuck.Location = new Point(10, 64);

        var btnFullExc = new RBtn("Chọn Full 6 dòng Exc", Theme.Blue, true) { Location = new Point(100, 58), Height = 26 };
        btnFullExc.Click += delegate { foreach (var c in ckExc) c.Checked = true; };
        var btnClearExc = new RBtn("Bỏ chọn Exc", Theme.Muted, true) { Location = new Point(268, 58), Height = 26 };
        btnClearExc.Click += delegate { foreach (var c in ckExc) c.Checked = false; };

        pnlAttrs.Controls.AddRange(new Control[] { laTitle, laLv, numLv, laDur, numDur, laOpt, numOpt, ckSkill, ckLuck, btnFullExc, btnClearExc });

        for (int i = 0; i < 6; i++)
        {
            ckExc[i] = K.Ck(ExcNames[i], (Data.Exc & (1 << i)) != 0);
            ckExc[i].Location = new Point(10 + (i % 2) * 294, 94 + (i / 2) * 32);
            pnlAttrs.Controls.Add(ckExc[i]);
        }
        Controls.Add(pnlAttrs);

        // Footer OK / Cancel
        var btnOk = new RBtn("Lưu sản phẩm", Theme.Green, false) { Location = new Point(392, 502), Width = 115 };
        btnOk.Click += delegate { OnOk(); };
        var btnCancel = new RBtn("Hủy bỏ", Theme.Muted, true) { Location = new Point(516, 502), Width = 96 };
        btnCancel.Click += delegate { DialogResult = DialogResult.Cancel; Close(); };
        Controls.Add(btnOk);
        Controls.Add(btnCancel);

        // Events
        cbType.SelectedIndexChanged += delegate { UpdateMode(); };
        cbSec.SelectedIndexChanged += delegate { FillItems(); };
        tbSearch.TextChanged += delegate { FillItems(); };
        lbItems.SelectedIndexChanged += delegate
        {
            var it = lbItems.SelectedItem as MuItemDef;
            if (it == null) return;
            numSec.Value = it.Section;
            numIdx.Value = it.Index;
            if (tbName.Text.Trim().Length == 0 || existing == null) tbName.Text = it.Name;
        };

        UpdateMode();
        FillItems();
    }

    void AddLbl(string text, int x, int y)
    {
        var l = K.Lbl(text, true);
        l.Location = new Point(x, y);
        Controls.Add(l);
    }

    ComboBox MakeCombo(int x, int y, int w, string[] items)
    {
        var c = new ComboBox
        {
            Location = new Point(x, y),
            Width = w,
            DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Theme.Bg,
            ForeColor = Theme.Text,
            FlatStyle = FlatStyle.Flat
        };
        c.Items.AddRange(items);
        Controls.Add(c);
        return c;
    }

    void UpdateMode()
    {
        bool isItem = cbType.SelectedIndex == 0;
        bool isVip = cbType.SelectedIndex == 1;
        pnlVip.Visible = isVip;
        pnlItemPicker.Visible = isItem;
        pnlAttrs.Visible = isItem;
        if (!isItem && cbCat.SelectedIndex != 3) cbCat.SelectedIndex = 3;
    }

    void FillItems()
    {
        string q = tbSearch.Text.Trim();
        int sec = cbSec.SelectedIndex;
        lbItems.BeginUpdate();
        lbItems.Items.Clear();
        int selIdx = -1;
        foreach (var it in catalog)
        {
            if (q.Length > 0)
            {
                if (it.Name.IndexOf(q, StringComparison.OrdinalIgnoreCase) < 0) continue;
            }
            else if (it.Section != sec) continue;

            int idx = lbItems.Items.Add(it);
            if (it.Section == (int)numSec.Value && it.Index == (int)numIdx.Value) selIdx = idx;
        }
        if (selIdx >= 0) lbItems.SelectedIndex = selIdx;
        lbItems.EndUpdate();
    }

    void OnOk()
    {
        string name = tbName.Text.Trim();
        if (name.Length == 0)
        {
            MessageBox.Show("Vui lòng nhập tên sản phẩm.", "Cash Shop", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        Data.Type = cbType.SelectedIndex;
        Data.Category = cbCat.SelectedIndex + 1;
        Data.Section = (int)numSec.Value;
        Data.Index = (int)numIdx.Value;
        Data.Level = (int)numLv.Value;
        Data.Dur = (int)numDur.Value;
        Data.Opt = (int)numOpt.Value;
        Data.Skill = ckSkill.Checked ? 1 : 0;
        Data.Luck = ckLuck.Checked ? 1 : 0;
        int exc = 0;
        for (int i = 0; i < 6; i++) if (ckExc[i].Checked) exc |= (1 << i);
        Data.Exc = exc;
        Data.Param = (int)numVipDays.Value;
        Data.Price = (int)numPrice.Value;
        Data.Name = name;
        Data.Desc = tbDesc.Text.Trim();

        DialogResult = DialogResult.OK;
        Close();
    }
}

// "Quản lý Game" tab: unified control center for Server/VIP rates, CashShop.ini, Marketplace DB, Ranking & Backup.
class GameAdminPanel : Panel, IShown
{
    readonly MainForm form;
    readonly string baseDir = AppDomain.CurrentDomain.BaseDirectory;
    readonly Encoding utf8NoBom = new UTF8Encoding(false);

    // Sub-tab navigation
    readonly List<RBtn> subBtns = new List<RBtn>();
    readonly List<Panel> subPages = new List<Panel>();
    int curSub = 0;

    // Sub-tab 1: Server & VIP Rates
    NumericUpDown n1MaxLv, n1Exp, n1Drop0, n1Drop1, n1Drop2, n1Drop3, n1Zen, n1MaxIp, n1Soul, n1Life;
    CheckBox ck1NonPk, ck1AntiHack;
    NumericUpDown n2MaxLv, n2Exp, n2Drop0, n2Drop1, n2Drop2, n2Drop3, n2Zen, n2MaxIp, n2Soul, n2Life, n2Lock;
    CheckBox ck2NonPk, ck2AntiHack;
    Label lblRatesStatus;

    // Sub-tab 2: CashShop.ini
    CheckBox ckCsEnable;
    DataGridView gridCs;
    Label lblCsStatus;
    readonly List<CashShopRowData> csItems = new List<CashShopRowData>();
    List<MuItemDef> itemCatalog;

    // Sub-tab 3: Marketplace DB
    DataGridView gridMk;
    Label lblMkStatus;

    // Sub-tab 4: Ranking & Backup
    DataGridView gridTopLv, gridTopGuild;
    Label lblRankStatus;

    public GameAdminPanel(MainForm f)
    {
        form = f;
        BackColor = Theme.Panel;

        var nav = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 46, BackColor = Theme.Bg, Padding = new Padding(14, 7, 14, 0), WrapContents = false };
        var host = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel };

        AddSub(nav, host, "Server + Tỉ lệ", BuildRatesPage());
        AddSub(nav, host, "Cash Shop", BuildCashShopPage());
        AddSub(nav, host, "Chợ Trời", BuildMarketPage());
        AddSub(nav, host, "BXH + Backup", BuildRankBackupPage());

        Controls.Add(host);
        Controls.Add(nav);
        SelectSub(0);
    }

    void AddSub(FlowLayoutPanel nav, Panel host, string title, Panel page)
    {
        int idx = subPages.Count;
        var b = new RBtn(title, Theme.Blue, true);
        b.Margin = new Padding(0, 0, 8, 0);
        b.Click += delegate { SelectSub(idx); };
        nav.Controls.Add(b);
        subBtns.Add(b);

        page.Dock = DockStyle.Fill;
        page.Visible = false;
        host.Controls.Add(page);
        subPages.Add(page);
    }

    void SelectSub(int idx)
    {
        curSub = idx;
        for (int i = 0; i < subPages.Count; i++)
        {
            subPages[i].Visible = (i == idx);
            subBtns[i].BackColor = (i == idx) ? Theme.Blue : Theme.Panel;
            subBtns[i].Invalidate();
        }
        RefreshActiveSub();
    }

    public void OnShown()
    {
        RefreshActiveSub();
    }

    void RefreshActiveSub()
    {
        if (curSub == 0) LoadRates();
        else if (curSub == 1) LoadCashShop();
        else if (curSub == 2) LoadMarket();
        else if (curSub == 3) LoadRankings();
    }

    RBtn Btn(string text, Color accent, bool ghost, EventHandler h)
    {
        var b = new RBtn(text, accent, ghost);
        b.Margin = new Padding(0, 0, 8, 0);
        b.Click += h;
        return b;
    }

    List<MuItemDef> GetItemCatalog()
    {
        if (itemCatalog != null) return itemCatalog;
        itemCatalog = new List<MuItemDef>();
        string path = Path.Combine(baseDir, @"Data\Item\Item.txt");
        if (!File.Exists(path)) return itemCatalog;
        int curSec = -1;
        var rxItem = new Regex(@"^\s*(\d+)\s+.*?""([^""]+)""");
        foreach (string raw in File.ReadAllLines(path))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith("//")) continue;
            if (line.Equals("end", StringComparison.OrdinalIgnoreCase)) { curSec = -1; continue; }
            int secNum;
            if (curSec < 0 && int.TryParse(line, out secNum)) { curSec = secNum; continue; }
            if (curSec >= 0)
            {
                var m = rxItem.Match(line);
                if (m.Success)
                {
                    int idx;
                    if (int.TryParse(m.Groups[1].Value, out idx))
                        itemCatalog.Add(new MuItemDef { Section = curSec, Index = idx, Name = m.Groups[2].Value });
                }
            }
        }
        return itemCatalog;
    }

    // =========================================================================
    // SUB-TAB 1: SERVER 1 & SERVER VIP CONFIG (NO BOM)
    // =========================================================================
    Panel BuildRatesPage()
    {
        var p = new Panel { BackColor = Theme.Panel, Padding = new Padding(14, 10, 14, 10), AutoScroll = true };

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, BackColor = Theme.Panel, WrapContents = false };
        var lCap = K.Lbl("Mở cấp nhanh (cả 2 SV):", true); lCap.Margin = new Padding(0, 8, 8, 0);
        top.Controls.Add(lCap);
        top.Controls.Add(Btn("Lv 300", Theme.Blue, true, delegate { SetBothMaxLv(300); }));
        top.Controls.Add(Btn("Lv 450", Theme.Blue, true, delegate { SetBothMaxLv(450); }));
        top.Controls.Add(Btn("Lv 700", Theme.Blue, true, delegate { SetBothMaxLv(700); }));
        top.Controls.Add(Btn("Lv 1000", Theme.Blue, true, delegate { SetBothMaxLv(1000); }));
        top.Controls.Add(Btn("Lưu + Reload Common", Theme.Green, false, delegate { SaveRates(true); }));
        top.Controls.Add(Btn("Tải lại", Theme.Muted, true, delegate { LoadRates(); }));

        var cols = new TableLayoutPanel { Dock = DockStyle.Top, Height = 360, ColumnCount = 2, RowCount = 1, BackColor = Theme.Panel };
        cols.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));
        cols.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));

        cols.Controls.Add(BuildServerCard("SERVER 1 (Thường — Port 55901)", false), 0, 0);
        cols.Controls.Add(BuildServerCard("SERVER VIP (VIP — Port 55903)", true), 1, 0);

        lblRatesStatus = K.Lbl("Chỉnh sửa thông số rồi bấm Lưu + Reload Common để áp dụng ngay không cần tắt server.", true);
        lblRatesStatus.Dock = DockStyle.Top;
        lblRatesStatus.Height = 28;
        lblRatesStatus.Padding = new Padding(0, 8, 0, 0);

        p.Controls.Add(lblRatesStatus);
        p.Controls.Add(cols);
        p.Controls.Add(top);
        return p;
    }

    void SetBothMaxLv(int lv)
    {
        n1MaxLv.Value = lv;
        n2MaxLv.Value = lv;
        lblRatesStatus.Text = "Đã đặt MaxCharacterLevel = " + lv + " cho cả 2 server (bấm Lưu + Reload Common để áp dụng).";
    }

    Panel BuildServerCard(string title, bool isVip)
    {
        var card = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Bg, Margin = new Padding(0, 4, isVip ? 0 : 10, 4), Padding = new Padding(12) };
        var lblT = new Label { Text = title, Dock = DockStyle.Top, Height = 26, ForeColor = isVip ? Theme.Amber : Theme.Blue, Font = new Font("Segoe UI Semibold", 9.5f) };

        var tbl = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 4, RowCount = 7, BackColor = Theme.Bg };
        tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 128));
        tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 118));
        tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));

        var nLv = K.Num(1, 1000, 1000, 80);
        var nExp = K.Num(1, 10000, isVip ? 2 : 1, 80);
        var nD0 = K.Num(1, 100, isVip ? 15 : 8, 80);
        var nD1 = K.Num(1, 100, isVip ? 18 : 10, 80);
        var nD2 = K.Num(1, 100, isVip ? 20 : 12, 80);
        var nD3 = K.Num(1, 100, isVip ? 22 : 15, 80);
        var nZen = K.Num(1, 1000, isVip ? 4 : 2, 80);
        var nIp = K.Num(1, 50, 1, 80);
        var nSoul = K.Num(1, 100, isVip ? 65 : 50, 80);
        var nLife = K.Num(1, 100, isVip ? 65 : 50, 80);
        var ckPk = K.Ck("Non-PK (Cấm PK)", isVip);
        var ckAh = K.Ck("Bật Anti-Hack", true);

        AddPair(tbl, 0, 0, "Max Level:", nLv);
        AddPair(tbl, 0, 2, "EXP Quái:", nExp);
        AddPair(tbl, 1, 0, "Drop AL0 (Free):", nD0);
        AddPair(tbl, 1, 2, "Drop AL1 (VIP1):", nD1);
        AddPair(tbl, 2, 0, "Drop AL2 (VIP2):", nD2);
        AddPair(tbl, 2, 2, "Drop AL3 (VIP3):", nD3);
        AddPair(tbl, 3, 0, "Tỉ lệ Zen:", nZen);
        AddPair(tbl, 3, 2, "Acc / PC (IP):", nIp);
        AddPair(tbl, 4, 0, "Ép Soul (%):", nSoul);
        AddPair(tbl, 4, 2, "Ép Life (%):", nLife);

        tbl.Controls.Add(ckPk, 0, 5); tbl.SetColumnSpan(ckPk, 2);
        tbl.Controls.Add(ckAh, 2, 5); tbl.SetColumnSpan(ckAh, 2);

        if (isVip)
        {
            n2Lock = K.Num(0, 3, 1, 80);
            AddPair(tbl, 6, 0, "ServerLock (AL):", n2Lock);
            var hint = K.Lbl("0=Mở, 1=VIP1+", true); hint.Margin = new Padding(0, 6, 0, 0);
            tbl.Controls.Add(hint, 2, 6); tbl.SetColumnSpan(hint, 2);

            n2MaxLv = nLv; n2Exp = nExp; n2Drop0 = nD0; n2Drop1 = nD1; n2Drop2 = nD2; n2Drop3 = nD3;
            n2Zen = nZen; n2MaxIp = nIp; n2Soul = nSoul; n2Life = nLife; ck2NonPk = ckPk; ck2AntiHack = ckAh;
        }
        else
        {
            n1MaxLv = nLv; n1Exp = nExp; n1Drop0 = nD0; n1Drop1 = nD1; n1Drop2 = nD2; n1Drop3 = nD3;
            n1Zen = nZen; n1MaxIp = nIp; n1Soul = nSoul; n1Life = nLife; ck1NonPk = ckPk; ck1AntiHack = ckAh;
        }

        card.Controls.Add(tbl);
        card.Controls.Add(lblT);
        return card;
    }

    void AddPair(TableLayoutPanel t, int row, int col, string label, Control ctrl)
    {
        var l = K.Lbl(label, true); l.Margin = new Padding(0, 6, 4, 0);
        ctrl.Margin = new Padding(0, 3, 8, 3);
        t.Controls.Add(l, col, row);
        t.Controls.Add(ctrl, col + 1, row);
    }

    string CommonPath(bool vip) { return Path.Combine(baseDir, (vip ? "GameServerVIP" : "GameServer") + @"\DATA\GameServerInfo - Common.dat"); }
    string StartupPath(bool vip) { return Path.Combine(baseDir, (vip ? "GameServerVIP" : "GameServer") + @"\DATA\GameServerInfo - StartUp.dat"); }

    static int ReadIniInt(string path, string key, int def)
    {
        try
        {
            if (!File.Exists(path)) return def;
            var m = Regex.Match(File.ReadAllText(path), @"^" + Regex.Escape(key) + @"=(-?\d+)", RegexOptions.Multiline);
            int v; return (m.Success && int.TryParse(m.Groups[1].Value, out v)) ? v : def;
        }
        catch { return def; }
    }

    void WriteIniKeys(string path, Dictionary<string, string> kv)
    {
        if (!File.Exists(path)) return;
        string txt = File.ReadAllText(path);
        foreach (var pair in kv)
        {
            var rx = new Regex(@"^(" + Regex.Escape(pair.Key) + @"=)[^\r\n]*", RegexOptions.Multiline);
            if (rx.IsMatch(txt)) txt = rx.Replace(txt, pair.Key + "=" + pair.Value, 1);
        }
        File.WriteAllText(path, txt, utf8NoBom);
    }

    static decimal Clamp(NumericUpDown n, int v) { return Math.Max(n.Minimum, Math.Min(n.Maximum, v)); }

    void LoadRates()
    {
        string p1 = CommonPath(false), p2 = CommonPath(true);
        n1MaxLv.Value = Clamp(n1MaxLv, ReadIniInt(p1, "MaxCharacterLevel", 1000));
        n1Exp.Value = Clamp(n1Exp, ReadIniInt(p1, "AddExperienceRate_AL0", 1));
        n1Drop0.Value = Clamp(n1Drop0, ReadIniInt(p1, "ItemDropRate_AL0", 8));
        n1Drop1.Value = Clamp(n1Drop1, ReadIniInt(p1, "ItemDropRate_AL1", 10));
        n1Drop2.Value = Clamp(n1Drop2, ReadIniInt(p1, "ItemDropRate_AL2", 12));
        n1Drop3.Value = Clamp(n1Drop3, ReadIniInt(p1, "ItemDropRate_AL3", 15));
        n1Zen.Value = Clamp(n1Zen, ReadIniInt(p1, "MoneyAmountDropRate_AL0", 2));
        n1MaxIp.Value = Clamp(n1MaxIp, ReadIniInt(p1, "MaxIpConnection", 1));
        n1Soul.Value = Clamp(n1Soul, ReadIniInt(p1, "JewelOfSoulSuccessRate_AL0", 50));
        n1Life.Value = Clamp(n1Life, ReadIniInt(p1, "JewelOfLifeSuccessRate_AL0", 50));
        ck1NonPk.Checked = ReadIniInt(p1, "NonPK", 0) != 0;
        ck1AntiHack.Checked = ReadIniInt(p1, "CheckSpeedHack", 1) != 0;

        n2MaxLv.Value = Clamp(n2MaxLv, ReadIniInt(p2, "MaxCharacterLevel", 1000));
        n2Exp.Value = Clamp(n2Exp, ReadIniInt(p2, "AddExperienceRate_AL0", 2));
        n2Drop0.Value = Clamp(n2Drop0, ReadIniInt(p2, "ItemDropRate_AL0", 15));
        n2Drop1.Value = Clamp(n2Drop1, ReadIniInt(p2, "ItemDropRate_AL1", 18));
        n2Drop2.Value = Clamp(n2Drop2, ReadIniInt(p2, "ItemDropRate_AL2", 20));
        n2Drop3.Value = Clamp(n2Drop3, ReadIniInt(p2, "ItemDropRate_AL3", 22));
        n2Zen.Value = Clamp(n2Zen, ReadIniInt(p2, "MoneyAmountDropRate_AL0", 4));
        n2MaxIp.Value = Clamp(n2MaxIp, ReadIniInt(p2, "MaxIpConnection", 1));
        n2Soul.Value = Clamp(n2Soul, ReadIniInt(p2, "JewelOfSoulSuccessRate_AL0", 65));
        n2Life.Value = Clamp(n2Life, ReadIniInt(p2, "JewelOfLifeSuccessRate_AL0", 65));
        ck2NonPk.Checked = ReadIniInt(p2, "NonPK", 1) != 0;
        ck2AntiHack.Checked = ReadIniInt(p2, "CheckSpeedHack", 1) != 0;
        n2Lock.Value = Clamp(n2Lock, ReadIniInt(StartupPath(true), "ServerLock", 1));
    }

    void SaveRates(bool reload)
    {
        try
        {
            var d1 = new Dictionary<string, string>
            {
                { "MaxCharacterLevel", ((int)n1MaxLv.Value).ToString() },
                { "AddExperienceRate_AL0", ((int)n1Exp.Value).ToString() },
                { "AddExperienceRate_AL1", ((int)n1Exp.Value).ToString() },
                { "AddExperienceRate_AL2", ((int)n1Exp.Value).ToString() },
                { "AddExperienceRate_AL3", ((int)n1Exp.Value).ToString() },
                { "ItemDropRate_AL0", ((int)n1Drop0.Value).ToString() },
                { "ItemDropRate_AL1", ((int)n1Drop1.Value).ToString() },
                { "ItemDropRate_AL2", ((int)n1Drop2.Value).ToString() },
                { "ItemDropRate_AL3", ((int)n1Drop3.Value).ToString() },
                { "MoneyAmountDropRate_AL0", ((int)n1Zen.Value).ToString() },
                { "MaxIpConnection", ((int)n1MaxIp.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL0", ((int)n1Soul.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL1", ((int)n1Soul.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL2", ((int)n1Soul.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL3", ((int)n1Soul.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL0", ((int)n1Life.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL1", ((int)n1Life.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL2", ((int)n1Life.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL3", ((int)n1Life.Value).ToString() },
                { "NonPK", ck1NonPk.Checked ? "1" : "0" },
                { "CheckSpeedHack", ck1AntiHack.Checked ? "1" : "0" },
                { "CheckLatencyHack", ck1AntiHack.Checked ? "1" : "0" },
                { "CheckAutoPotionHack", ck1AntiHack.Checked ? "1" : "0" }
            };
            WriteIniKeys(CommonPath(false), d1);

            var d2 = new Dictionary<string, string>
            {
                { "MaxCharacterLevel", ((int)n2MaxLv.Value).ToString() },
                { "AddExperienceRate_AL0", ((int)n2Exp.Value).ToString() },
                { "AddExperienceRate_AL1", ((int)n2Exp.Value).ToString() },
                { "AddExperienceRate_AL2", ((int)n2Exp.Value).ToString() },
                { "AddExperienceRate_AL3", ((int)n2Exp.Value).ToString() },
                { "ItemDropRate_AL0", ((int)n2Drop0.Value).ToString() },
                { "ItemDropRate_AL1", ((int)n2Drop1.Value).ToString() },
                { "ItemDropRate_AL2", ((int)n2Drop2.Value).ToString() },
                { "ItemDropRate_AL3", ((int)n2Drop3.Value).ToString() },
                { "MoneyAmountDropRate_AL0", ((int)n2Zen.Value).ToString() },
                { "MaxIpConnection", ((int)n2MaxIp.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL0", ((int)n2Soul.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL1", ((int)n2Soul.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL2", ((int)n2Soul.Value).ToString() },
                { "JewelOfSoulSuccessRate_AL3", ((int)n2Soul.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL0", ((int)n2Life.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL1", ((int)n2Life.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL2", ((int)n2Life.Value).ToString() },
                { "JewelOfLifeSuccessRate_AL3", ((int)n2Life.Value).ToString() },
                { "NonPK", ck2NonPk.Checked ? "1" : "0" },
                { "CheckSpeedHack", ck2AntiHack.Checked ? "1" : "0" },
                { "CheckLatencyHack", ck2AntiHack.Checked ? "1" : "0" },
                { "CheckAutoPotionHack", ck2AntiHack.Checked ? "1" : "0" }
            };
            WriteIniKeys(CommonPath(true), d2);
            WriteIniKeys(StartupPath(true), new Dictionary<string, string> { { "ServerLock", ((int)n2Lock.Value).ToString() } });

            int n = reload ? form.ReloadGameServers(32710, "Reload Common") : 0;
            lblRatesStatus.Text = n > 0
                ? "Đã lưu Common.dat (NO BOM) + gửi Reload Common tới " + n + " GameServer!"
                : "Đã lưu Common.dat (NO BOM). Sẽ áp dụng khi bật GameServer.";
            form.Say(lblRatesStatus.Text);
        }
        catch (Exception ex) { lblRatesStatus.Text = "Lỗi lưu: " + ex.Message; }
    }

    // =========================================================================
    // SUB-TAB 2: CASH SHOP EDITOR (WITH ITEM PICKER & ATTRIBUTES DIALOG)
    // =========================================================================
    string CashShopPath { get { return Path.Combine(baseDir, @"Data\Custom\CashShop.ini"); } }

    Panel BuildCashShopPage()
    {
        var p = new Panel { BackColor = Theme.Panel, Padding = new Padding(14, 10, 14, 10) };

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, BackColor = Theme.Panel, WrapContents = false };
        ckCsEnable = K.Ck("Bật Cash Shop", true); ckCsEnable.Margin = new Padding(0, 7, 14, 0);
        top.Controls.Add(ckCsEnable);
        top.Controls.Add(Btn("＋ Thêm sản phẩm", Theme.Blue, false, delegate { AddCashShopProduct(); }));
        top.Controls.Add(Btn("Sửa sản phẩm", Theme.Amber, true, delegate { EditSelectedCashShopProduct(); }));
        top.Controls.Add(Btn("－ Xóa dòng", Theme.Red, true, delegate { DeleteSelectedCashShopProduct(); }));
        top.Controls.Add(Btn("Lưu + Reload Custom", Theme.Green, false, delegate { SaveCashShop(); }));
        top.Controls.Add(Btn("Tải lại", Theme.Muted, true, delegate { LoadCashShop(); }));

        gridCs = new DataGridView();
        K.StyleGrid(gridCs);
        gridCs.Dock = DockStyle.Fill;
        gridCs.Columns.Add("idx", "#");
        gridCs.Columns.Add("cat", "Danh mục");
        gridCs.Columns.Add("name", "Tên sản phẩm");
        gridCs.Columns.Add("item", "Mã Item");
        gridCs.Columns.Add("attr", "Thuộc tính (Lv / Skill / Luck / Opt / Exc)");
        gridCs.Columns.Add("price", "Giá WCoin");
        gridCs.Columns.Add("desc", "Mô tả");
        gridCs.Columns["idx"].FillWeight = 25;
        gridCs.Columns["cat"].FillWeight = 85;
        gridCs.Columns["name"].FillWeight = 115;
        gridCs.Columns["item"].FillWeight = 60;
        gridCs.Columns["attr"].FillWeight = 120;
        gridCs.Columns["price"].FillWeight = 65;
        gridCs.Columns["desc"].FillWeight = 150;
        gridCs.CellDoubleClick += delegate { EditSelectedCashShopProduct(); };

        lblCsStatus = K.Lbl("Mẹo: Bấm '＋ Thêm sản phẩm' hoặc nhấn đúp vào 1 dòng để mở bảng chọn vật phẩm từ Item.txt & chỉnh thuộc tính.", true);
        lblCsStatus.Dock = DockStyle.Bottom;
        lblCsStatus.Height = 24;

        p.Controls.Add(gridCs);
        p.Controls.Add(lblCsStatus);
        p.Controls.Add(top);
        return p;
    }

    void RefreshCashShopGrid()
    {
        string[] cats = { "", "1 - Pets", "2 - Jewels", "3 - Tickets", "4 - VIP" };
        gridCs.Rows.Clear();
        for (int i = 0; i < csItems.Count; i++)
        {
            var it = csItems[i];
            string cat = (it.Category >= 1 && it.Category <= 4) ? cats[it.Category] : it.Category.ToString();
            string code = it.Type == 1 ? "Gói VIP" : (it.Type == 2 ? "Đổi Class" : (it.Section + ", " + it.Index));
            gridCs.Rows.Add(i + 1, cat, it.Name, code, it.AttrSummary(), it.Price.ToString("N0"), it.Desc);
        }
    }

    void AddCashShopProduct()
    {
        using (var dlg = new CashShopItemDialog(GetItemCatalog(), null))
        {
            if (dlg.ShowDialog(FindForm()) == DialogResult.OK)
            {
                csItems.Add(dlg.Data);
                RefreshCashShopGrid();
                lblCsStatus.Text = "Đã thêm '" + dlg.Data.Name + "'. Bấm 'Lưu + Reload Custom' để áp dụng vào game.";
            }
        }
    }

    void EditSelectedCashShopProduct()
    {
        if (gridCs.SelectedRows.Count == 0) return;
        int idx = gridCs.SelectedRows[0].Index;
        if (idx < 0 || idx >= csItems.Count) return;
        using (var dlg = new CashShopItemDialog(GetItemCatalog(), csItems[idx]))
        {
            if (dlg.ShowDialog(FindForm()) == DialogResult.OK)
            {
                csItems[idx] = dlg.Data;
                RefreshCashShopGrid();
                lblCsStatus.Text = "Đã cập nhật '" + dlg.Data.Name + "'. Bấm 'Lưu + Reload Custom' để áp dụng.";
            }
        }
    }

    void DeleteSelectedCashShopProduct()
    {
        if (gridCs.SelectedRows.Count == 0) return;
        int idx = gridCs.SelectedRows[0].Index;
        if (idx < 0 || idx >= csItems.Count) return;
        string nm = csItems[idx].Name;
        if (MessageBox.Show("Xóa sản phẩm '" + nm + "' khỏi Cash Shop?", "Cash Shop", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        csItems.RemoveAt(idx);
        RefreshCashShopGrid();
        lblCsStatus.Text = "Đã xóa '" + nm + "'. Bấm 'Lưu + Reload Custom' để áp dụng.";
    }

    static int ParseInt(Dictionary<string, string> d, string k, int def)
    {
        int v; return (d.ContainsKey(k) && int.TryParse(d[k], out v)) ? v : def;
    }

    void LoadCashShop()
    {
        csItems.Clear();
        if (!File.Exists(CashShopPath)) { RefreshCashShopGrid(); lblCsStatus.Text = "Chưa thấy " + CashShopPath; return; }
        var sections = new Dictionary<string, Dictionary<string, string>>(StringComparer.OrdinalIgnoreCase);
        string cur = "";
        foreach (string raw in File.ReadAllLines(CashShopPath))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith(";")) continue;
            if (line.StartsWith("[") && line.EndsWith("]")) { cur = line.Substring(1, line.Length - 2); sections[cur] = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase); continue; }
            int eq = line.IndexOf('=');
            if (eq > 0 && sections.ContainsKey(cur)) sections[cur][line.Substring(0, eq).Trim()] = line.Substring(eq + 1).Trim();
        }
        if (sections.ContainsKey("CashShop") && sections["CashShop"].ContainsKey("Enable"))
            ckCsEnable.Checked = sections["CashShop"]["Enable"] != "0";

        for (int i = 1; i <= 64; i++)
        {
            string key = "Item" + i;
            if (!sections.ContainsKey(key)) continue;
            var d = sections[key];
            csItems.Add(new CashShopRowData
            {
                Type = ParseInt(d, "Type", 0),
                Category = ParseInt(d, "Category", 1),
                Section = ParseInt(d, "Section", 14),
                Index = ParseInt(d, "Index", 13),
                Level = ParseInt(d, "Level", 0),
                Dur = ParseInt(d, "Dur", 1),
                Skill = ParseInt(d, "Skill", 0),
                Luck = ParseInt(d, "Luck", 0),
                Opt = ParseInt(d, "Opt", 0),
                Exc = ParseInt(d, "Exc", 0),
                Param = ParseInt(d, "Param", 1),
                Price = ParseInt(d, "Price", 100),
                Name = d.ContainsKey("Name") ? d["Name"] : "",
                Desc = d.ContainsKey("Desc") ? d["Desc"] : ""
            });
        }
        RefreshCashShopGrid();
        lblCsStatus.Text = "Đã tải " + csItems.Count + " sản phẩm từ CashShop.ini (nhấn đúp dòng để sửa thuộc tính).";
    }

    void SaveCashShop()
    {
        try
        {
            var sb = new StringBuilder();
            sb.AppendLine("[CashShop]");
            sb.AppendLine("Enable=" + (ckCsEnable.Checked ? "1" : "0"));
            sb.AppendLine();
            for (int i = 0; i < csItems.Count && i < 64; i++)
            {
                var it = csItems[i];
                sb.AppendLine("[Item" + (i + 1) + "]");
                sb.AppendLine("Type=" + it.Type);
                sb.AppendLine("Category=" + it.Category);
                sb.AppendLine("Section=" + it.Section);
                sb.AppendLine("Index=" + it.Index);
                sb.AppendLine("Level=" + it.Level);
                sb.AppendLine("Dur=" + it.Dur);
                sb.AppendLine("Skill=" + it.Skill);
                sb.AppendLine("Luck=" + it.Luck);
                sb.AppendLine("Opt=" + it.Opt);
                sb.AppendLine("Exc=" + it.Exc);
                sb.AppendLine("Param=" + it.Param);
                sb.AppendLine("Price=" + it.Price);
                sb.AppendLine("Name=" + it.Name);
                sb.AppendLine("Desc=" + it.Desc);
                sb.AppendLine();
            }
            File.WriteAllText(CashShopPath, sb.ToString(), utf8NoBom);
            int cnt = form.ReloadGameServers(32711, "Reload Custom");
            lblCsStatus.Text = "Đã lưu " + csItems.Count + " sản phẩm (NO BOM)" + (cnt > 0 ? " + Reload Custom trên " + cnt + " GameServer!" : ".");
            form.Say(lblCsStatus.Text);
        }
        catch (Exception ex) { lblCsStatus.Text = "Lỗi lưu CashShop.ini: " + ex.Message; }
    }

    // =========================================================================
    // SUB-TAB 3: MARKETPLACE MODERATION (MariaDB MarketItems)
    // =========================================================================
    Panel BuildMarketPage()
    {
        var p = new Panel { BackColor = Theme.Panel, Padding = new Padding(14, 10, 14, 10) };

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, BackColor = Theme.Panel, WrapContents = false };
        top.Controls.Add(Btn("Tải lại Chợ Trời", Theme.Blue, true, delegate { LoadMarket(); }));
        top.Controls.Add(Btn("Xóa tin đã chọn", Theme.Red, true, delegate { DeleteSelectedMarket(); }));
        lblMkStatus = K.Lbl("", true); lblMkStatus.Margin = new Padding(10, 8, 0, 0);
        top.Controls.Add(lblMkStatus);

        gridMk = new DataGridView();
        K.StyleGrid(gridMk);
        gridMk.Dock = DockStyle.Fill;
        gridMk.Columns.Add("id", "ID");
        gridMk.Columns.Add("acc", "Tài khoản");
        gridMk.Columns.Add("char", "Người bán");
        gridMk.Columns.Add("price", "Giá");
        gridMk.Columns.Add("cur", "Tiền tệ");
        gridMk.Columns.Add("st", "Trạng thái");
        gridMk.Columns.Add("buyer", "Người mua");
        gridMk.Columns.Add("dt", "Ngày đăng");
        gridMk.Columns["id"].FillWeight = 35;

        p.Controls.Add(gridMk);
        p.Controls.Add(top);
        return p;
    }

    void LoadMarket()
    {
        gridMk.Rows.Clear();
        if (!Db.Available) { lblMkStatus.Text = "Chưa có mariadb\\mu.cnf"; return; }
        string err;
        var rows = Db.Query("SELECT Id, AccountID, CharacterName, Price, PriceType, Status, COALESCE(BuyerName,''), DATE_FORMAT(PostDate,'%Y-%m-%d %H:%i') FROM MarketItems ORDER BY Id DESC LIMIT 200", out err);
        if (err != null) { lblMkStatus.Text = "Lỗi: " + err; return; }
        string[] curs = { "Zen", "Bless", "Soul", "Chaos", "WCoin" };
        string[] sts = { "Đang bán", "Chờ nhận tiền", "Đã bán", "Đã hủy" };
        foreach (var r in rows)
        {
            if (r.Length < 8) continue;
            int ct, st;
            int.TryParse(r[4], out ct);
            int.TryParse(r[5], out st);
            gridMk.Rows.Add(r[0], r[1], r[2], r[3], (ct >= 0 && ct < curs.Length) ? curs[ct] : r[4], (st >= 0 && st < sts.Length) ? sts[st] : r[5], r[6], r[7]);
        }
        lblMkStatus.Text = rows.Count + " tin đăng gần nhất (Thuế Chợ Trời: 5%)";
    }

    void DeleteSelectedMarket()
    {
        if (gridMk.SelectedRows.Count == 0) return;
        string id = Convert.ToString(gridMk.SelectedRows[0].Cells[0].Value);
        if (MessageBox.Show("Xóa tin đăng Market ID #" + id + " khỏi cơ sở dữ liệu?", "Chợ Trời", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
        string err;
        Db.Exec("DELETE FROM MarketItems WHERE Id=" + id, out err);
        if (err != null) { MessageBox.Show("Lỗi: " + err); return; }
        LoadMarket();
        form.Say("Đã xóa tin Market #" + id);
    }

    // =========================================================================
    // SUB-TAB 4: RANKING & BACKUP
    // =========================================================================
    Panel BuildRankBackupPage()
    {
        var p = new Panel { BackColor = Theme.Panel, Padding = new Padding(14, 10, 14, 10) };

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, BackColor = Theme.Panel, WrapContents = false };
        top.Controls.Add(Btn("Tải lại BXH", Theme.Blue, true, delegate { LoadRankings(); }));
        top.Controls.Add(Btn("Backup MariaDB ngay", Theme.Green, false, delegate { RunBackupNow(); }));
        top.Controls.Add(Btn("Mở thư mục Backups", Theme.Muted, true, delegate { OpenBackupsFolder(); }));
        top.Controls.Add(Btn("Reload All GameServer", Theme.Amber, true, delegate {
            int n = form.ReloadGameServers(32722, "Reload All");
            lblRankStatus.Text = n > 0 ? "Đã gửi Reload All tới " + n + " GameServer!" : "Chưa có GameServer nào đang chạy.";
        }));

        var split = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, BackColor = Theme.Panel };
        split.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58f));
        split.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42f));

        gridTopLv = new DataGridView(); K.StyleGrid(gridTopLv); gridTopLv.Dock = DockStyle.Fill;
        gridTopLv.Columns.Add("rk", "#"); gridTopLv.Columns.Add("name", "Nhân vật"); gridTopLv.Columns.Add("cls", "Class"); gridTopLv.Columns.Add("lv", "Level");
        gridTopLv.Columns["rk"].FillWeight = 30; gridTopLv.Columns["cls"].FillWeight = 55; gridTopLv.Columns["lv"].FillWeight = 55;

        gridTopGuild = new DataGridView(); K.StyleGrid(gridTopGuild); gridTopGuild.Dock = DockStyle.Fill;
        gridTopGuild.Columns.Add("rk", "#"); gridTopGuild.Columns.Add("g", "Tên Guild"); gridTopGuild.Columns.Add("sc", "Điểm");
        gridTopGuild.Columns["rk"].FillWeight = 30; gridTopGuild.Columns["sc"].FillWeight = 60;

        var leftHost = new Panel { Dock = DockStyle.Fill, Padding = new Padding(0, 0, 8, 0), BackColor = Theme.Panel };
        leftHost.Controls.Add(gridTopLv);
        var rightHost = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel };
        rightHost.Controls.Add(gridTopGuild);
        split.Controls.Add(leftHost, 0, 0);
        split.Controls.Add(rightHost, 1, 0);

        lblRankStatus = K.Lbl("Bảng xếp hạng trực tiếp từ MariaDB (MuOnline97).", true);
        lblRankStatus.Dock = DockStyle.Bottom;
        lblRankStatus.Height = 24;

        p.Controls.Add(split);
        p.Controls.Add(lblRankStatus);
        p.Controls.Add(top);
        return p;
    }

    void LoadRankings()
    {
        gridTopLv.Rows.Clear();
        gridTopGuild.Rows.Clear();
        if (!Db.Available) { lblRankStatus.Text = "Chưa có mariadb\\mu.cnf"; return; }
        string err;
        var chars = Db.Query("SELECT Name, Class, cLevel FROM `Character` ORDER BY cLevel DESC, Experience DESC LIMIT 20", out err);
        if (err == null)
        {
            int rk = 1;
            foreach (var r in chars)
            {
                if (r.Length < 3) continue;
                int c; int.TryParse(r[1], out c);
                string cn = c >= 48 ? "MG" : (c >= 32 ? "ELF" : (c >= 16 ? "DK" : "DW"));
                gridTopLv.Rows.Add("#" + rk++, r[0], cn, "Lv." + r[2]);
            }
        }
        var guilds = Db.Query("SELECT G_Name, COALESCE(G_Score,0) FROM Guild ORDER BY G_Score DESC LIMIT 10", out err);
        if (err == null)
        {
            int rk = 1;
            foreach (var r in guilds)
            {
                if (r.Length < 2) continue;
                gridTopGuild.Rows.Add("#" + rk++, r[0], r[1]);
            }
        }
        lblRankStatus.Text = "Đã tải Top " + gridTopLv.Rows.Count + " nhân vật & Top " + gridTopGuild.Rows.Count + " Guild.";
    }

    void RunBackupNow()
    {
        string ps1 = Path.Combine(baseDir, "backup_db.ps1");
        if (!File.Exists(ps1)) { lblRankStatus.Text = "Không thấy " + ps1; return; }
        try
        {
            var psi = new ProcessStartInfo("powershell.exe", "-NoProfile -ExecutionPolicy Bypass -File \"" + ps1 + "\"");
            psi.UseShellExecute = false;
            psi.CreateNoWindow = true;
            psi.RedirectStandardOutput = true;
            var proc = Process.Start(psi);
            string output = proc.StandardOutput.ReadToEnd().Trim();
            proc.WaitForExit(15000);
            lblRankStatus.Text = output.Length > 0 ? output : "Backup hoàn tất!";
            form.Say(lblRankStatus.Text);
        }
        catch (Exception ex) { lblRankStatus.Text = "Lỗi backup: " + ex.Message; }
    }

    void OpenBackupsFolder()
    {
        string dir = Path.GetFullPath(Path.Combine(baseDir, @"..\Backups"));
        Directory.CreateDirectory(dir);
        try { Process.Start("explorer.exe", dir); } catch { }
    }
}
