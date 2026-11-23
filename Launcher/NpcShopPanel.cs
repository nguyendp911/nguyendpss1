using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

class NpcShopItemRow
{
    public int Section = 14;
    public int Index = 1;
    public int Level = 0;
    public int Dur = 255;
    public int Skill = 0;
    public int Luck = 0;
    public int Opt = 0;
    public int Exc = 0;
    public string SlotX = "*";
    public string SlotY = "*";
    public string Comment = "";

    public string AttrSummary()
    {
        var parts = new List<string>();
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

class NpcShopItemDialog : Form
{
    static readonly string[] SectionNames = {
        "0 - Swords (Kiếm)", "1 - Axes (Rìu)", "2 - Maces (Chùy)", "3 - Spears (Giáo)",
        "4 - Bows / Crossbows (Cung)", "5 - Staffs (Gậy)", "6 - Shields (Khiên)",
        "7 - Helms (Mũ)", "8 - Armors (Áo)", "9 - Pants (Quần)", "10 - Gloves (Găng)",
        "11 - Boots (Giày)", "12 - Wings / Orbs / Chaos", "13 - Pets / Rings / Pendants",
        "14 - Jewels / Potions / Tickets", "15 - Scrolls (Sách phép)"
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
    public NpcShopItemRow Data;

    ComboBox cbSec;
    TextBox tbSearch, tbSlotX, tbSlotY, tbComment;
    ListBox lbItems;
    NumericUpDown numSec, numIdx, numLv, numDur, numOpt;
    CheckBox ckSkill, ckLuck;
    CheckBox[] ckExc = new CheckBox[6];

    public NpcShopItemDialog(List<MuItemDef> items, NpcShopItemRow existing)
    {
        catalog = items;
        Data = existing != null ? new NpcShopItemRow
        {
            Section = existing.Section, Index = existing.Index, Level = existing.Level,
            Dur = existing.Dur, Skill = existing.Skill, Luck = existing.Luck,
            Opt = existing.Opt, Exc = existing.Exc, SlotX = existing.SlotX,
            SlotY = existing.SlotY, Comment = existing.Comment
        } : new NpcShopItemRow();

        Text = existing == null ? "Thêm vật phẩm vào NPC Shop" : "Chỉnh sửa vật phẩm NPC Shop";
        Size = new Size(640, 540);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        StartPosition = FormStartPosition.CenterParent;
        BackColor = Theme.Panel;
        ForeColor = Theme.Text;
        Font = new Font("Segoe UI", 9f);

        // Top Item Picker
        var pnlPick = new Panel { Location = new Point(14, 12), Size = new Size(598, 206), BackColor = Theme.Bg };
        var lpTitle = new Label { Text = "Chọn vật phẩm từ Item.txt", Location = new Point(10, 8), AutoSize = true, ForeColor = Theme.Blue, Font = new Font("Segoe UI Semibold", 9f) };
        var lpSec = K.Lbl("Nhóm:", true); lpSec.Location = new Point(10, 34);

        cbSec = new ComboBox
        {
            Location = new Point(58, 30), Width = 210, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Theme.Panel, ForeColor = Theme.Text, FlatStyle = FlatStyle.Flat
        };
        cbSec.Items.AddRange(SectionNames);
        cbSec.SelectedIndex = Math.Max(0, Math.Min(15, Data.Section));

        var lpFind = K.Lbl("Tìm nhanh:", true); lpFind.Location = new Point(280, 34);
        tbSearch = K.Tb(140); tbSearch.Location = new Point(350, 30);
        var lpMan = K.Lbl("Sec / Idx:", true); lpMan.Location = new Point(496, 34);
        numSec = K.Num(0, 15, Math.Max(0, Math.Min(15, Data.Section)), 44); numSec.Location = new Point(496, 56);
        numIdx = K.Num(0, 511, Math.Max(0, Math.Min(511, Data.Index)), 44); numIdx.Location = new Point(544, 56);

        var lpSlot = K.Lbl("Slot X / Y:", true); lpSlot.Location = new Point(496, 92);
        tbSlotX = K.Tb(42); tbSlotX.Location = new Point(496, 114); tbSlotX.Text = Data.SlotX;
        tbSlotY = K.Tb(42); tbSlotY.Location = new Point(544, 114); tbSlotY.Text = Data.SlotY;
        var lpStar = K.Lbl("* = tự xếp", true); lpStar.Location = new Point(496, 144);

        lbItems = new ListBox
        {
            Location = new Point(10, 60), Size = new Size(478, 134),
            BackColor = Theme.Panel, ForeColor = Theme.Text, BorderStyle = BorderStyle.FixedSingle, IntegralHeight = false
        };

        pnlPick.Controls.AddRange(new Control[] { lpTitle, lpSec, cbSec, lpFind, tbSearch, lpMan, numSec, numIdx, lpSlot, tbSlotX, tbSlotY, lpStar, lbItems });
        Controls.Add(pnlPick);

        // Attributes Panel
        var pnlAttr = new Panel { Location = new Point(14, 226), Size = new Size(598, 224), BackColor = Theme.Bg };
        var laTitle = new Label { Text = "Thuộc tính vật phẩm (Cấp, Độ bền, Skill, Luck, Option, Hoàn Hảo)", Location = new Point(10, 8), AutoSize = true, ForeColor = Theme.Amber, Font = new Font("Segoe UI Semibold", 9f) };

        var laLv = K.Lbl("Cấp (+0..11):", true); laLv.Location = new Point(10, 36);
        numLv = K.Num(0, 15, Math.Max(0, Math.Min(15, Data.Level)), 58); numLv.Location = new Point(92, 33);

        var laDur = K.Lbl("Độ bền (Dur):", true); laDur.Location = new Point(164, 36);
        numDur = K.Num(0, 255, Math.Max(0, Math.Min(255, Data.Dur)), 62); numDur.Location = new Point(246, 33);

        var laOpt = K.Lbl("Opt (0..4):", true); laOpt.Location = new Point(322, 36);
        numOpt = K.Num(0, 7, Math.Max(0, Math.Min(7, Data.Opt)), 54); numOpt.Location = new Point(390, 33);

        ckSkill = K.Ck("Có Skill", Data.Skill != 0); ckSkill.Location = new Point(458, 35);
        ckLuck = K.Ck("Có Luck", Data.Luck != 0); ckLuck.Location = new Point(530, 35);

        var laCmt = K.Lbl("Ghi chú:", true); laCmt.Location = new Point(10, 66);
        tbComment = K.Tb(250); tbComment.Location = new Point(66, 62); tbComment.Text = Data.Comment;

        var btnFullExc = new RBtn("Full 6 dòng Exc", Theme.Blue, true) { Location = new Point(332, 60), Height = 26 };
        btnFullExc.Click += delegate { foreach (var c in ckExc) c.Checked = true; };
        var btnClearExc = new RBtn("Bỏ Exc", Theme.Muted, true) { Location = new Point(470, 60), Height = 26 };
        btnClearExc.Click += delegate { foreach (var c in ckExc) c.Checked = false; };

        pnlAttr.Controls.AddRange(new Control[] { laTitle, laLv, numLv, laDur, numDur, laOpt, numOpt, ckSkill, ckLuck, laCmt, tbComment, btnFullExc, btnClearExc });

        for (int i = 0; i < 6; i++)
        {
            ckExc[i] = K.Ck(ExcNames[i], (Data.Exc & (1 << i)) != 0);
            ckExc[i].Location = new Point(10 + (i % 2) * 294, 102 + (i / 2) * 36);
            pnlAttr.Controls.Add(ckExc[i]);
        }
        Controls.Add(pnlAttr);

        // Footer
        var btnOk = new RBtn("Lưu vật phẩm", Theme.Green, false) { Location = new Point(392, 460), Width = 115 };
        btnOk.Click += delegate { OnOk(); };
        var btnCancel = new RBtn("Hủy bỏ", Theme.Muted, true) { Location = new Point(516, 460), Width = 96 };
        btnCancel.Click += delegate { DialogResult = DialogResult.Cancel; Close(); };
        Controls.Add(btnOk);
        Controls.Add(btnCancel);

        cbSec.SelectedIndexChanged += delegate { FillItems(); };
        tbSearch.TextChanged += delegate { FillItems(); };
        lbItems.SelectedIndexChanged += delegate
        {
            var it = lbItems.SelectedItem as MuItemDef;
            if (it == null) return;
            numSec.Value = it.Section;
            numIdx.Value = it.Index;
            tbComment.Text = it.Name;
        };

        FillItems();
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
        Data.SlotX = string.IsNullOrEmpty(tbSlotX.Text.Trim()) ? "*" : tbSlotX.Text.Trim();
        Data.SlotY = string.IsNullOrEmpty(tbSlotY.Text.Trim()) ? "*" : tbSlotY.Text.Trim();
        Data.Comment = tbComment.Text.Trim();
        DialogResult = DialogResult.OK;
        Close();
    }
}

class NpcShopSetDialog : Form
{
    class SetGroup
    {
        public int Index;
        public string Name;
        public MuItemDef[] Parts = new MuItemDef[5]; // 0:Helm(7), 1:Armor(8), 2:Pants(9), 3:Gloves(10), 4:Boots(11)
        public int Count() { int c = 0; foreach (var p in Parts) if (p != null) c++; return c; }
        public override string ToString() { return string.Format("[Set {0:00}] {1} ({2} món)", Index, Name, Count()); }
    }

    static readonly string[] ExcNames = {
        "Exc 1: +4% Max HP",
        "Exc 2: +4% Max Mana",
        "Exc 3: -4% Giảm sát thương",
        "Exc 4: +5% Phản đòn",
        "Exc 5: +10% Tỉ lệ phòng thủ",
        "Exc 6: +40% Zen rơi"
    };

    readonly List<SetGroup> allSets = new List<SetGroup>();
    public readonly List<NpcShopItemRow> ResultRows = new List<NpcShopItemRow>();

    TextBox tbSearch;
    ListBox lbSets;
    Label lblPreview;
    CheckBox[] ckParts = new CheckBox[5];
    NumericUpDown numLv, numDur, numOpt;
    CheckBox ckLuck;
    CheckBox[] ckExc = new CheckBox[6];

    public NpcShopSetDialog(List<MuItemDef> catalog)
    {
        BuildSets(catalog);

        Text = "Thêm nguyên bộ trang bị vào NPC Shop";
        Size = new Size(620, 505);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        StartPosition = FormStartPosition.CenterParent;
        BackColor = Theme.Panel;
        ForeColor = Theme.Text;
        Font = new Font("Segoe UI", 9f);

        // Top: Set Picker
        var pnlSet = new Panel { Location = new Point(14, 12), Size = new Size(578, 198), BackColor = Theme.Bg };
        var lTitle = new Label { Text = "Chọn bộ trang bị (tự động gom đủ Mũ / Áo / Quần / Găng / Giày từ Item.txt)", Location = new Point(10, 8), AutoSize = true, ForeColor = Theme.Blue, Font = new Font("Segoe UI Semibold", 9f) };
        var lFind = K.Lbl("Tìm nhanh:", true); lFind.Location = new Point(10, 34);
        tbSearch = K.Tb(220); tbSearch.Location = new Point(82, 30);

        lbSets = new ListBox
        {
            Location = new Point(10, 60), Size = new Size(340, 128),
            BackColor = Theme.Panel, ForeColor = Theme.Text, BorderStyle = BorderStyle.FixedSingle, IntegralHeight = false
        };

        var lParts = new Label { Text = "Các món trong bộ:", Location = new Point(362, 34), AutoSize = true, ForeColor = Theme.Amber, Font = new Font("Segoe UI Semibold", 9f) };
        string[] pNames = { "Mũ (Helm - Sec 7)", "Áo (Armor - Sec 8)", "Quần (Pants - Sec 9)", "Găng (Gloves - Sec 10)", "Giày (Boots - Sec 11)" };
        for (int i = 0; i < 5; i++)
        {
            ckParts[i] = K.Ck(pNames[i], true);
            ckParts[i].Location = new Point(362, 60 + i * 24);
            pnlSet.Controls.Add(ckParts[i]);
        }

        lblPreview = K.Lbl("", true);
        lblPreview.Location = new Point(362, 180);
        pnlSet.Controls.AddRange(new Control[] { lTitle, lFind, tbSearch, lbSets, lParts, lblPreview });
        Controls.Add(pnlSet);

        // Attributes for whole set
        var pnlAttr = new Panel { Location = new Point(14, 218), Size = new Size(578, 196), BackColor = Theme.Bg };
        var laTitle = new Label { Text = "Thuộc tính áp dụng chung cho cả bộ", Location = new Point(10, 8), AutoSize = true, ForeColor = Theme.Amber, Font = new Font("Segoe UI Semibold", 9f) };

        var laLv = K.Lbl("Cấp (+0..11):", true); laLv.Location = new Point(10, 36);
        numLv = K.Num(0, 15, 7, 58); numLv.Location = new Point(92, 33);

        var laDur = K.Lbl("Độ bền (Dur):", true); laDur.Location = new Point(164, 36);
        numDur = K.Num(0, 255, 255, 62); numDur.Location = new Point(246, 33);

        var laOpt = K.Lbl("Opt (0..4):", true); laOpt.Location = new Point(322, 36);
        numOpt = K.Num(0, 7, 3, 54); numOpt.Location = new Point(390, 33);

        ckLuck = K.Ck("Có Luck", true); ckLuck.Location = new Point(460, 35);

        var btnFullExc = new RBtn("Full 6 dòng Exc", Theme.Blue, true) { Location = new Point(10, 64), Height = 26 };
        btnFullExc.Click += delegate { foreach (var c in ckExc) c.Checked = true; };
        var btnClearExc = new RBtn("Bỏ chọn Exc", Theme.Muted, true) { Location = new Point(145, 64), Height = 26 };
        btnClearExc.Click += delegate { foreach (var c in ckExc) c.Checked = false; };

        pnlAttr.Controls.AddRange(new Control[] { laTitle, laLv, numLv, laDur, numDur, laOpt, numOpt, ckLuck, btnFullExc, btnClearExc });

        for (int i = 0; i < 6; i++)
        {
            ckExc[i] = K.Ck(ExcNames[i], false);
            ckExc[i].Location = new Point(10 + (i % 2) * 284, 100 + (i / 2) * 30);
            pnlAttr.Controls.Add(ckExc[i]);
        }
        Controls.Add(pnlAttr);

        // Footer
        var btnOk = new RBtn("＋ Thêm nguyên bộ", Theme.Green, false) { Location = new Point(346, 424), Width = 140 };
        btnOk.Click += delegate { OnOk(); };
        var btnCancel = new RBtn("Hủy bỏ", Theme.Muted, true) { Location = new Point(496, 424), Width = 96 };
        btnCancel.Click += delegate { DialogResult = DialogResult.Cancel; Close(); };
        Controls.Add(btnOk);
        Controls.Add(btnCancel);

        tbSearch.TextChanged += delegate { FillSets(); };
        lbSets.SelectedIndexChanged += delegate { OnSelectSet(); };
        FillSets();
    }

    void BuildSets(List<MuItemDef> catalog)
    {
        var map = new SortedDictionary<int, SetGroup>();
        foreach (var it in catalog)
        {
            if (it.Section < 7 || it.Section > 11) continue;
            SetGroup g;
            if (!map.TryGetValue(it.Index, out g))
            {
                g = new SetGroup { Index = it.Index, Name = StripPartSuffix(it.Name) };
                map[it.Index] = g;
            }
            g.Parts[it.Section - 7] = it;
            if (it.Section == 8) g.Name = StripPartSuffix(it.Name); // Armor name is cleanest
        }
        foreach (var g in map.Values)
            if (g.Count() >= 2) allSets.Add(g);
    }

    static string StripPartSuffix(string name)
    {
        string s = Regex.Replace(name ?? "", @"\b(Helm|Armor|Pants|Gloves|Boots|Mask)\b", "", RegexOptions.IgnoreCase).Trim();
        return s.Length > 0 ? Regex.Replace(s, @"\s+", " ") : (name ?? "Set");
    }

    void FillSets()
    {
        string q = tbSearch.Text.Trim();
        lbSets.BeginUpdate();
        lbSets.Items.Clear();
        foreach (var g in allSets)
        {
            if (q.Length > 0 && g.Name.IndexOf(q, StringComparison.OrdinalIgnoreCase) < 0) continue;
            lbSets.Items.Add(g);
        }
        if (lbSets.Items.Count > 0) lbSets.SelectedIndex = 0;
        lbSets.EndUpdate();
    }

    void OnSelectSet()
    {
        var g = lbSets.SelectedItem as SetGroup;
        if (g == null) return;
        for (int i = 0; i < 5; i++)
        {
            bool has = g.Parts[i] != null;
            ckParts[i].Enabled = has;
            ckParts[i].Checked = has;
        }
    }

    void OnOk()
    {
        var g = lbSets.SelectedItem as SetGroup;
        if (g == null) return;
        int exc = 0;
        for (int i = 0; i < 6; i++) if (ckExc[i].Checked) exc |= (1 << i);

        ResultRows.Clear();
        for (int i = 0; i < 5; i++)
        {
            if (!ckParts[i].Checked || g.Parts[i] == null) continue;
            var it = g.Parts[i];
            ResultRows.Add(new NpcShopItemRow
            {
                Section = it.Section,
                Index = it.Index,
                Level = (int)numLv.Value,
                Dur = (int)numDur.Value,
                Skill = 0,
                Luck = ckLuck.Checked ? 1 : 0,
                Opt = (int)numOpt.Value,
                Exc = exc,
                SlotX = "*",
                SlotY = "*",
                Comment = it.Name
            });
        }
        if (ResultRows.Count == 0)
        {
            MessageBox.Show("Hãy chọn ít nhất 1 món trong bộ!", "NPC Shop", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        DialogResult = DialogResult.OK;
        Close();
    }
}

class NpcShopPanel : Panel
{
    readonly MainForm form;
    readonly Func<List<MuItemDef>> getCatalog;
    readonly string shopDir;
    readonly Encoding utf8NoBom = new UTF8Encoding(false);

    ComboBox cbShop;
    DataGridView grid;
    Label lblStatus;
    readonly List<string> shopFiles = new List<string>();
    readonly List<NpcShopItemRow> rows = new List<NpcShopItemRow>();
    Dictionary<string, string> itemNames;

    public NpcShopPanel(MainForm f, Func<List<MuItemDef>> catalogProvider)
    {
        form = f;
        getCatalog = catalogProvider;
        shopDir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"Data\Shop");
        BackColor = Theme.Panel;
        Padding = new Padding(14, 10, 14, 10);

        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, BackColor = Theme.Panel, WrapContents = false };
        var lNpc = K.Lbl("NPC Shop:", true); lNpc.Margin = new Padding(0, 8, 6, 0);
        cbShop = new ComboBox
        {
            Width = 230, DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Theme.Bg, ForeColor = Theme.Text, FlatStyle = FlatStyle.Flat,
            Margin = new Padding(0, 5, 10, 0)
        };
        cbShop.SelectedIndexChanged += delegate { LoadSelectedShop(); };

        top.Controls.Add(lNpc);
        top.Controls.Add(cbShop);
        top.Controls.Add(MakeBtn("＋ Thêm đồ", Theme.Blue, false, delegate { AddItem(); }));
        top.Controls.Add(MakeBtn("＋ Nguyên bộ", Theme.Blue, true, delegate { AddFullSet(); }));
        top.Controls.Add(MakeBtn("Sửa", Theme.Amber, true, delegate { EditSelected(); }));
        top.Controls.Add(MakeBtn("－ Xóa", Theme.Red, true, delegate { DeleteSelected(); }));
        top.Controls.Add(MakeBtn("Lưu + Reload Shop", Theme.Green, false, delegate { SaveSelectedShop(true); }));
        top.Controls.Add(MakeBtn("Mở .txt", Theme.Muted, true, delegate { OpenCurrentFile(); }));

        grid = new DataGridView();
        K.StyleGrid(grid);
        grid.Dock = DockStyle.Fill;
        grid.Columns.Add("idx", "#");
        grid.Columns.Add("code", "Mã (Sec,Idx)");
        grid.Columns.Add("name", "Tên vật phẩm");
        grid.Columns.Add("lv", "Cấp");
        grid.Columns.Add("dur", "Độ bền");
        grid.Columns.Add("attr", "Thuộc tính (Skill / Luck / Opt / Exc)");
        grid.Columns.Add("slot", "Vị trí (X,Y)");
        grid.Columns["idx"].FillWeight = 28;
        grid.Columns["code"].FillWeight = 62;
        grid.Columns["name"].FillWeight = 140;
        grid.Columns["lv"].FillWeight = 42;
        grid.Columns["dur"].FillWeight = 48;
        grid.Columns["attr"].FillWeight = 125;
        grid.Columns["slot"].FillWeight = 55;
        grid.CellDoubleClick += delegate { EditSelected(); };

        lblStatus = K.Lbl("Chọn NPC Shop để thêm/sửa/xóa vật phẩm. Bấm 'Lưu + Reload Shop' để áp dụng ngay trong game.", true);
        lblStatus.Dock = DockStyle.Bottom;
        lblStatus.Height = 24;

        Controls.Add(grid);
        Controls.Add(lblStatus);
        Controls.Add(top);
    }

    RBtn MakeBtn(string text, Color accent, bool ghost, EventHandler h)
    {
        var b = new RBtn(text, accent, ghost);
        b.Margin = new Padding(0, 0, 6, 0);
        b.Click += h;
        return b;
    }

    string LookupItemName(int sec, int idx, string fallback)
    {
        if (itemNames == null)
        {
            itemNames = new Dictionary<string, string>();
            foreach (var it in getCatalog())
                itemNames[it.Section + "," + it.Index] = it.Name;
        }
        string nm;
        if (itemNames.TryGetValue(sec + "," + idx, out nm)) return nm;
        return !string.IsNullOrEmpty(fallback) ? fallback : ("Item " + sec + "," + idx);
    }

    public void LoadShopList()
    {
        int prev = cbShop.SelectedIndex;
        shopFiles.Clear();
        cbShop.Items.Clear();
        if (!Directory.Exists(shopDir)) { lblStatus.Text = "Không tìm thấy thư mục " + shopDir; return; }
        var files = Directory.GetFiles(shopDir, "*.txt");
        Array.Sort(files, StringComparer.OrdinalIgnoreCase);
        foreach (string f in files)
        {
            shopFiles.Add(f);
            cbShop.Items.Add(Path.GetFileNameWithoutExtension(f));
        }
        if (cbShop.Items.Count > 0)
        {
            cbShop.SelectedIndex = (prev >= 0 && prev < cbShop.Items.Count) ? prev : 0;
        }
    }

    string CurrentFile()
    {
        int i = cbShop.SelectedIndex;
        return (i >= 0 && i < shopFiles.Count) ? shopFiles[i] : null;
    }

    void LoadSelectedShop()
    {
        rows.Clear();
        string path = CurrentFile();
        if (path == null || !File.Exists(path)) { RefreshGrid(); return; }

        foreach (string raw in File.ReadAllLines(path))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith("//")) continue;
            if (line.Equals("end", StringComparison.OrdinalIgnoreCase)) break;

            string comment = "";
            int cpos = line.IndexOf("//");
            if (cpos >= 0)
            {
                comment = line.Substring(cpos + 2).Trim();
                line = line.Substring(0, cpos).Trim();
            }
            if (line.Length == 0) continue;

            var tok = Regex.Split(line, @"[\s,]+");
            if (tok.Length < 8) continue;
            int sec, idx, lv, dur, sk, lk, op, ex;
            if (!int.TryParse(tok[0], out sec) || !int.TryParse(tok[1], out idx)) continue;
            int.TryParse(tok[2], out lv);
            int.TryParse(tok[3], out dur);
            int.TryParse(tok[4], out sk);
            int.TryParse(tok[5], out lk);
            int.TryParse(tok[6], out op);
            int.TryParse(tok[7], out ex);
            string sx = tok.Length >= 9 ? tok[8] : "*";
            string sy = tok.Length >= 10 ? tok[9] : "*";

            rows.Add(new NpcShopItemRow
            {
                Section = sec, Index = idx, Level = lv, Dur = dur,
                Skill = sk, Luck = lk, Opt = op, Exc = ex,
                SlotX = sx, SlotY = sy, Comment = comment
            });
        }
        RefreshGrid();
        lblStatus.Text = "Đã tải " + rows.Count + " vật phẩm từ " + Path.GetFileName(path) + " (nhấn đúp dòng để sửa).";
    }

    void RefreshGrid()
    {
        grid.Rows.Clear();
        for (int i = 0; i < rows.Count; i++)
        {
            var r = rows[i];
            string code = string.Format("{0},{1:000}", r.Section, r.Index);
            string nm = LookupItemName(r.Section, r.Index, r.Comment);
            grid.Rows.Add(i + 1, code, nm, "+" + r.Level, r.Dur, r.AttrSummary(), r.SlotX + ", " + r.SlotY);
        }
    }

    void AddItem()
    {
        if (CurrentFile() == null) return;
        using (var dlg = new NpcShopItemDialog(getCatalog(), null))
        {
            if (dlg.ShowDialog(FindForm()) == DialogResult.OK)
            {
                rows.Add(dlg.Data);
                RefreshGrid();
                lblStatus.Text = "Đã thêm '" + LookupItemName(dlg.Data.Section, dlg.Data.Index, dlg.Data.Comment) + "'. Bấm 'Lưu + Reload Shop' để áp dụng.";
            }
        }
    }

    void AddFullSet()
    {
        if (CurrentFile() == null) return;
        using (var dlg = new NpcShopSetDialog(getCatalog()))
        {
            if (dlg.ShowDialog(FindForm()) == DialogResult.OK && dlg.ResultRows.Count > 0)
            {
                rows.AddRange(dlg.ResultRows);
                RefreshGrid();
                lblStatus.Text = "Đã thêm nguyên bộ (" + dlg.ResultRows.Count + " món). Bấm 'Lưu + Reload Shop' để áp dụng vào game.";
            }
        }
    }

    void EditSelected()
    {
        if (grid.SelectedRows.Count == 0) return;
        int i = grid.SelectedRows[0].Index;
        if (i < 0 || i >= rows.Count) return;
        using (var dlg = new NpcShopItemDialog(getCatalog(), rows[i]))
        {
            if (dlg.ShowDialog(FindForm()) == DialogResult.OK)
            {
                rows[i] = dlg.Data;
                RefreshGrid();
                lblStatus.Text = "Đã cập nhật dòng #" + (i + 1) + ". Bấm 'Lưu + Reload Shop' để áp dụng.";
            }
        }
    }

    void DeleteSelected()
    {
        if (grid.SelectedRows.Count == 0) return;
        int i = grid.SelectedRows[0].Index;
        if (i < 0 || i >= rows.Count) return;
        string nm = LookupItemName(rows[i].Section, rows[i].Index, rows[i].Comment);
        if (MessageBox.Show("Xóa '" + nm + "' khỏi shop này?", "NPC Shop", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        rows.RemoveAt(i);
        RefreshGrid();
        lblStatus.Text = "Đã xóa '" + nm + "'. Bấm 'Lưu + Reload Shop' để áp dụng.";
    }

    void SaveSelectedShop(bool reload)
    {
        string path = CurrentFile();
        if (path == null) return;
        try
        {
            var sb = new StringBuilder();
            sb.AppendLine("//==============================================================");
            sb.AppendLine("// SHOP - " + Path.GetFileNameWithoutExtension(path));
            sb.AppendLine("//==============================================================");
            sb.AppendLine("//SLOTS X = 0~7 , Y = 0~14 , * = Auto");
            sb.AppendLine("//Index\tLevel\tDur\tSkill\tLuck\tOption\tExcOp\tSlotX\tSlotY\tComment");
            sb.AppendLine();
            foreach (var r in rows)
            {
                string cmt = !string.IsNullOrEmpty(r.Comment) ? r.Comment : LookupItemName(r.Section, r.Index, "");
                sb.AppendFormat("{0},{1:000}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t{8}\t{9}\t// {10}\r\n",
                    r.Section, r.Index, r.Level, r.Dur, r.Skill, r.Luck, r.Opt, r.Exc, r.SlotX, r.SlotY, cmt);
            }
            sb.AppendLine();
            sb.AppendLine("end");
            File.WriteAllText(path, sb.ToString(), utf8NoBom);

            int n = reload ? form.ReloadGameServers(32719, "Reload Shop") : 0;
            lblStatus.Text = n > 0
                ? "Đã lưu " + Path.GetFileName(path) + " (" + rows.Count + " món, NO BOM) + Reload Shop tới " + n + " GameServer!"
                : "Đã lưu " + Path.GetFileName(path) + " (" + rows.Count + " món, NO BOM). Sẽ áp dụng khi bật GameServer.";
            form.Say(lblStatus.Text);
        }
        catch (Exception ex) { lblStatus.Text = "Lỗi lưu shop: " + ex.Message; }
    }

    void OpenCurrentFile()
    {
        string path = CurrentFile();
        if (path != null && File.Exists(path))
            try { Process.Start("notepad.exe", path); } catch { }
    }
}
