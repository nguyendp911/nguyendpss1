using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

// "Quản lý Drop" tab: manage all server drop rates (Common.dat, MapManager.txt, ItemDrop.txt, EventItemBag/*.txt, ItemOptionRate.txt)
// with live GameServer reload via WM_COMMAND.
class DropPanel : Panel, IShown
{
    const uint CMD_RELOAD_COMMON = 32710;
    const uint CMD_RELOAD_BAG = 32713;
    const uint CMD_RELOAD_ITEM = 32715;
    const uint CMD_RELOAD_ALL = 32722;

    readonly MainForm form;
    readonly string baseDir = AppDomain.CurrentDomain.BaseDirectory;

    // Catalogs
    List<DropData.ItemDef> items = new List<DropData.ItemDef>();
    Dictionary<string, string> itemNames = new Dictionary<string, string>();
    List<DropData.MonsterDef> monsters = new List<DropData.MonsterDef>();
    Dictionary<int, string> monsterNames = new Dictionary<int, string>();

    // Loaded configs
    DropData.CommonDropCfg commonCfg = new DropData.CommonDropCfg();
    List<DropData.MapRow> mapRows = new List<DropData.MapRow>();
    List<DropData.ItemDropRow> dropRows = new List<DropData.ItemDropRow>();
    List<DropData.BagFile> bagFiles = new List<DropData.BagFile>();
    List<DropData.OptRateRow> optRows = new List<DropData.OptRateRow>();

    // Sub-tabs
    readonly List<RBtn> subBtns = new List<RBtn>();
    readonly List<Panel> subPages = new List<Panel>();
    int curSub = 0;
    Label lblHint;

    // Sub 1: Common & Map
    NumericUpDown numItemTime, numMoneyTime;
    NumericUpDown[] numExpRate = new NumericUpDown[4];
    NumericUpDown[] numEventExpRate = new NumericUpDown[4];
    NumericUpDown[] numItemRate = new NumericUpDown[4];
    NumericUpDown[] numMoneyRate = new NumericUpDown[4];
    DataGridView gridMaps;
    NumericUpDown numBulkDrop, numBulkExc;

    // Sub 2: ItemDrop
    TextBox tbDropSearch;
    DataGridView gridDrops;
    List<int> dropViewIdx = new List<int>();

    // Sub 3: EventItemBag
    TextBox tbBagSearch;
    ListBox lbBags;
    List<int> bagViewIdx = new List<int>();
    DropData.BagFile curBag;
    Label lblBagTitle;
    NumericUpDown numBagZen, numBagRate, numBagCount;
    ComboBox cbBagType;
    CheckBox chkBagFw;
    DataGridView gridBagNorm, gridBagAdv;
    bool bagUpdating;

    // Sub 4: OptionRate
    ComboBox cbOptSec;
    DataGridView gridOpts;
    List<int> optViewIdx = new List<int>();

    bool loadedOnce;

    public DropPanel(MainForm f)
    {
        form = f;
        BackColor = Theme.Panel;
        Padding = new Padding(12, 10, 12, 10);

        // ---- Top sub-tab bar ----
        var topBar = new Panel { Dock = DockStyle.Top, Height = 44, BackColor = Theme.Panel };
        var leftFlow = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, WrapContents = false, BackColor = Theme.Panel, Padding = new Padding(0, 4, 0, 0) };
        var rightFlow = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, BackColor = Theme.Panel, Padding = new Padding(0, 4, 0, 0) };

        string[] names = new[] { "1. Chung & Map", "2. Rớt đồ (ItemDrop)", "3. Túi & Boss (Bag)", "4. Tỉ lệ Option" };
        for (int i = 0; i < names.Length; i++)
        {
            int idx = i;
            var b = new RBtn(names[i], Theme.Blue, i != 0) { Height = 32, Margin = new Padding(0, 0, 6, 0) };
            b.Click += delegate { SelectSub(idx); };
            subBtns.Add(b);
            leftFlow.Controls.Add(b);
        }

        var btnReloadFiles = MakeBtn("↻ Tải lại file", Theme.Muted, true, delegate { LoadAllData(); });
        var btnReloadGs = MakeBtn("⚡ Reload GameServer", Theme.Green, false, delegate { form.ReloadGameServer(CMD_RELOAD_ALL, "Reload All"); });
        btnReloadFiles.Height = 32;
        btnReloadGs.Height = 32;
        rightFlow.Controls.Add(btnReloadFiles);
        rightFlow.Controls.Add(btnReloadGs);

        topBar.Controls.Add(leftFlow);
        topBar.Controls.Add(rightFlow);

        lblHint = new Label
        {
            Dock = DockStyle.Bottom,
            Height = 22,
            ForeColor = Theme.Muted,
            BackColor = Theme.Panel,
            TextAlign = ContentAlignment.MiddleLeft,
            Text = "Mẹo: Nhấn đúp chuột vào dòng trong bảng để chỉnh sửa nhanh. Bấm Lưu sẽ tự động gửi lệnh Reload tới GameServer nếu đang chạy."
        };

        var bodyHost = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel, Padding = new Padding(0, 6, 0, 4) };

        var p1 = BuildCommonMapPage();
        var p2 = BuildItemDropPage();
        var p3 = BuildBagPage();
        var p4 = BuildOptRatePage();
        subPages.Add(p1); subPages.Add(p2); subPages.Add(p3); subPages.Add(p4);
        foreach (var p in subPages) { p.Dock = DockStyle.Fill; p.Visible = false; bodyHost.Controls.Add(p); }
        p1.Visible = true;

        Controls.Add(bodyHost);
        Controls.Add(topBar);
        Controls.Add(lblHint);
    }

    RBtn MakeBtn(string text, Color accent, bool ghost, EventHandler h)
    {
        var b = new RBtn(text, accent, ghost) { Height = 30, Margin = new Padding(0, 0, 6, 0) };
        b.Click += h;
        return b;
    }

    void SelectSub(int idx)
    {
        curSub = idx;
        for (int i = 0; i < subPages.Count; i++)
        {
            subPages[i].Visible = (i == idx);
        }
        // Refresh button visual style by toggling accent/text indicator
        string[] names = new[] { "1. Chung & Map", "2. Rớt đồ (ItemDrop)", "3. Túi & Boss (Bag)", "4. Tỉ lệ Option" };
        for (int i = 0; i < subBtns.Count; i++)
        {
            subBtns[i].Text = (i == idx ? "● " : "") + names[i];
        }
    }

    public void OnShown()
    {
        if (!loadedOnce)
        {
            loadedOnce = true;
            SelectSub(0);
            LoadAllData();
        }
    }

    void LoadAllData()
    {
        try
        {
            items = DropData.LoadItems(baseDir);
            itemNames.Clear();
            foreach (var it in items) itemNames[it.Key] = it.Name;

            monsters = DropData.LoadMonsters(baseDir);
            monsterNames.Clear();
            foreach (var m in monsters) monsterNames[m.Id] = m.Name;

            commonCfg = DropData.LoadCommon(baseDir);
            mapRows = DropData.LoadMaps(baseDir);
            dropRows = DropData.LoadItemDrops(baseDir);
            bagFiles = DropData.LoadAllBags(baseDir);
            optRows = DropData.LoadOptionRates(baseDir);

            BindCommonMap();
            BindItemDrops();
            BindBagList();
            BindOptRates();

            lblHint.Text = string.Format("Đã tải: {0} item, {1} quái, {2} map, {3} quy tắc ItemDrop, {4} EventItemBag.",
                items.Count, monsters.Count, mapRows.Count, dropRows.Count, bagFiles.Count);
        }
        catch (Exception ex)
        {
            MessageBox.Show("Lỗi tải dữ liệu Drop: " + ex.Message, "Quản lý Drop", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    string GetItemName(int cat, int id)
    {
        string key = string.Format("{0:00},{1:000}", cat, id);
        string n;
        return itemNames.TryGetValue(key, out n) ? n : "Unknown";
    }

    string GetMapLabel(string mapNum)
    {
        if (mapNum == "*") return "* (Tất cả)";
        int id;
        if (int.TryParse(mapNum, out id))
        {
            foreach (var m in mapRows) if (m.Index == id) return id + " - " + m.Name;
        }
        return mapNum;
    }

    string GetMonsterLabel(string mon)
    {
        if (mon == "*") return "* (Tất cả)";
        int id;
        string n;
        if (int.TryParse(mon, out id) && monsterNames.TryGetValue(id, out n)) return id + " - " + n;
        return mon;
    }

    // =========================================================================
    // SUB-TAB 1: COMMON & MAP
    // =========================================================================
    Panel BuildCommonMapPage()
    {
        var page = new Panel { BackColor = Theme.Panel };

        // Top: Common Drop Card
        var card = new Panel { Dock = DockStyle.Top, Height = 186, BackColor = Theme.PanelHi, Padding = new Padding(12, 8, 12, 8) };
        var title = new Label { Text = "Tỉ lệ EXP & Rớt đồ toàn server (GameServerInfo - Common.dat)", Dock = DockStyle.Top, Height = 24, ForeColor = Theme.Text, Font = new Font("Segoe UI Semibold", 10f) };

        var gridBox = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, BackColor = Theme.PanelHi };

        var r0 = K.Row(gridBox);
        r0.Controls.Add(MakeFieldLbl("Server Exp (x):"));
        for (int i = 0; i < 4; i++)
        {
            r0.Controls.Add(MakeSmallLbl("AL" + i + ":"));
            numExpRate[i] = K.Num(1, 10000, 5, 54);
            numExpRate[i].Margin = new Padding(0, 2, 8, 0);
            r0.Controls.Add(numExpRate[i]);
        }
        r0.Controls.Add(MakeFieldLbl("Event Exp (x):"));
        for (int i = 0; i < 4; i++)
        {
            r0.Controls.Add(MakeSmallLbl("AL" + i + ":"));
            numEventExpRate[i] = K.Num(1, 10000, 10, 54);
            numEventExpRate[i].Margin = new Padding(0, 2, 8, 0);
            r0.Controls.Add(numEventExpRate[i]);
        }

        var r1 = K.Row(gridBox);
        r1.Controls.Add(MakeFieldLbl("ItemDropRate (%):"));
        for (int i = 0; i < 4; i++)
        {
            r1.Controls.Add(MakeSmallLbl("AL" + i + ":"));
            numItemRate[i] = K.Num(0, 10000, 100, 66);
            numItemRate[i].Margin = new Padding(0, 2, 10, 0);
            r1.Controls.Add(numItemRate[i]);
        }
        r1.Controls.Add(MakeSmallLbl("Thời gian đồ (s):"));
        numItemTime = K.Num(5, 600, 30, 58);
        numItemTime.Margin = new Padding(0, 2, 10, 0);
        r1.Controls.Add(numItemTime);
        r1.Controls.Add(MakeBtn("Copy AL0 → AL1..3", Theme.Muted, true, delegate
        {
            for (int i = 1; i < 4; i++) numItemRate[i].Value = numItemRate[0].Value;
        }));

        var r2 = K.Row(gridBox);
        r2.Controls.Add(MakeFieldLbl("ZenDropRate (%):"));
        for (int i = 0; i < 4; i++)
        {
            r2.Controls.Add(MakeSmallLbl("AL" + i + ":"));
            numMoneyRate[i] = K.Num(0, 10000, 50, 66);
            numMoneyRate[i].Margin = new Padding(0, 2, 10, 0);
            r2.Controls.Add(numMoneyRate[i]);
        }
        r2.Controls.Add(MakeSmallLbl("Thời gian Zen (s):"));
        numMoneyTime = K.Num(5, 600, 30, 58);
        numMoneyTime.Margin = new Padding(0, 2, 10, 0);
        r2.Controls.Add(numMoneyTime);
        r2.Controls.Add(MakeBtn("Copy AL0 → AL1..3", Theme.Muted, true, delegate
        {
            for (int i = 1; i < 4; i++) numMoneyRate[i].Value = numMoneyRate[0].Value;
        }));

        var r3 = K.Row(gridBox);
        r3.Controls.Add(MakeBtn("💾 Lưu Chung & Map + Reload GameServer", Theme.Green, false, delegate { SaveCommonAndMaps(); }));
        r3.Controls.Add(MakeSmallLbl("   Chỉnh hàng loạt Map — DropRate:"));
        numBulkDrop = K.Num(0, 10000, 100, 64);
        numBulkDrop.Margin = new Padding(0, 3, 8, 0);
        r3.Controls.Add(numBulkDrop);
        r3.Controls.Add(MakeSmallLbl("Exc (%):"));
        numBulkExc = MakePctNum(3, 0m, 100m, 0.020m, 0.01m, 76);
        numBulkExc.Margin = new Padding(0, 3, 8, 0);
        r3.Controls.Add(numBulkExc);
        r3.Controls.Add(MakeBtn("Áp dụng tất cả Map", Theme.Blue, true, delegate
        {
            int excRaw = (int)Math.Round(numBulkExc.Value * 10000m);
            foreach (var m in mapRows) { m.ItemDropRate = (int)numBulkDrop.Value; m.ExcDropRate = excRaw; }
            BindMapGrid();
        }));

        card.Controls.Add(gridBox);
        card.Controls.Add(title);

        var spacer = new Panel { Dock = DockStyle.Top, Height = 8, BackColor = Theme.Panel };

        // Bottom: MapManager grid
        var mapWrap = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel };
        var mapTop = new Panel { Dock = DockStyle.Top, Height = 34, BackColor = Theme.Panel };
        var mapTitle = new Label { Text = "Tỉ lệ rớt đồ theo từng bản đồ (Data\\MapManager.txt) — Bấm đúp vào Map để sửa", Dock = DockStyle.Left, AutoSize = true, ForeColor = Theme.Text, Font = new Font("Segoe UI Semibold", 9.5f), Padding = new Padding(0, 7, 0, 0) };
        var btnEditMap = MakeBtn("✎ Sửa Map đang chọn", Theme.Blue, true, delegate { EditSelectedMap(); });
        btnEditMap.Dock = DockStyle.Right;
        mapTop.Controls.Add(mapTitle);
        mapTop.Controls.Add(btnEditMap);

        gridMaps = new DataGridView { Dock = DockStyle.Fill };
        K.StyleGrid(gridMaps);
        gridMaps.Columns.Add("idx", "ID");
        gridMaps.Columns.Add("name", "Tên bản đồ");
        gridMaps.Columns.Add("nonpk", "NonPK");
        gridMaps.Columns.Add("exp", "ExpRate (%)");
        gridMaps.Columns.Add("drop", "ItemDropRate (%)");
        gridMaps.Columns.Add("excPct", "Tỉ lệ Exc (%)");
        gridMaps.Columns[0].FillWeight = 35;
        gridMaps.Columns[1].FillWeight = 140;
        gridMaps.Columns[2].FillWeight = 55;
        gridMaps.Columns[3].FillWeight = 85;
        gridMaps.Columns[4].FillWeight = 95;
        gridMaps.Columns[5].FillWeight = 95;
        gridMaps.CellDoubleClick += delegate { EditSelectedMap(); };

        mapWrap.Controls.Add(gridMaps);
        mapWrap.Controls.Add(mapTop);

        page.Controls.Add(mapWrap);
        page.Controls.Add(spacer);
        page.Controls.Add(card);
        return page;
    }

    void BindCommonMap()
    {
        numItemTime.Value = Clamp(commonCfg.ItemDropTime, 5, 600);
        numMoneyTime.Value = Clamp(commonCfg.MoneyDropTime, 5, 600);
        for (int i = 0; i < 4; i++)
        {
            numExpRate[i].Value = Clamp(commonCfg.AddExperienceRate[i], 1, 10000);
            numEventExpRate[i].Value = Clamp(commonCfg.AddEventExperienceRate[i], 1, 10000);
            numItemRate[i].Value = Clamp(commonCfg.ItemDropRate[i], 0, 10000);
            numMoneyRate[i].Value = Clamp(commonCfg.MoneyAmountDropRate[i], 0, 10000);
        }
        BindMapGrid();
    }

    void BindMapGrid()
    {
        int selIdx = gridMaps.CurrentRow != null ? gridMaps.CurrentRow.Index : -1;
        gridMaps.Rows.Clear();
        foreach (var m in mapRows)
        {
            gridMaps.Rows.Add(m.Index, m.Name, m.NonPK, m.ExpRate + "%", m.ItemDropRate + "%", string.Format("{0:0.000}%", m.ExcDropRate / 10000.0));
        }
        if (selIdx >= 0 && selIdx < gridMaps.Rows.Count) gridMaps.Rows[selIdx].Selected = true;
    }

    void EditSelectedMap()
    {
        if (gridMaps.CurrentRow == null) return;
        int idx = gridMaps.CurrentRow.Index;
        if (idx < 0 || idx >= mapRows.Count) return;
        var m = mapRows[idx];

        using (var d = MakeDialog("Chỉnh sửa Map: " + m.Name + " (ID " + m.Index + ")", 360, 250))
        {
            var nExp = K.Num(0, 100000, Clamp(m.ExpRate, 0, 100000), 120);
            var nDrop = K.Num(0, 10000, Clamp(m.ItemDropRate, 0, 10000), 120);
            var nExc = MakePctNum(3, 0m, 100m, ClampDec(m.ExcDropRate / 10000m, 0m, 100m), 0.01m, 120);
            var cbPk = K.Cb(120);
            cbPk.Items.AddRange(new object[] { "*", "0", "1" });
            cbPk.SelectedItem = m.NonPK;
            if (cbPk.SelectedIndex < 0) cbPk.SelectedIndex = 0;

            AddDialogRow(d, 16, "ExperienceRate (%):", nExp);
            AddDialogRow(d, 52, "ItemDropRate (%):", nDrop);
            AddDialogRow(d, 88, "Tỉ lệ đồ Exc (%):", nExc);
            AddDialogRow(d, 124, "NonPK (*=Mặc định, 1=Safe):", cbPk);

            var ok = MakeBtn("Cập nhật", Theme.Green, false, delegate { d.DialogResult = DialogResult.OK; d.Close(); });
            ok.Location = new Point(150, 170);
            var cancel = MakeBtn("Hủy", Theme.Muted, true, delegate { d.Close(); });
            cancel.Location = new Point(255, 170);
            d.Controls.Add(ok); d.Controls.Add(cancel);

            if (d.ShowDialog(this) == DialogResult.OK)
            {
                m.ExpRate = (int)nExp.Value;
                m.ItemDropRate = (int)nDrop.Value;
                m.ExcDropRate = (int)Math.Round(nExc.Value * 10000m);
                m.NonPK = cbPk.SelectedItem.ToString();
                BindMapGrid();
            }
        }
    }

    void SaveCommonAndMaps()
    {
        try
        {
            commonCfg.ItemDropTime = (int)numItemTime.Value;
            commonCfg.MoneyDropTime = (int)numMoneyTime.Value;
            for (int i = 0; i < 4; i++)
            {
                commonCfg.AddExperienceRate[i] = (int)numExpRate[i].Value;
                commonCfg.AddEventExperienceRate[i] = (int)numEventExpRate[i].Value;
                commonCfg.ItemDropRate[i] = (int)numItemRate[i].Value;
                commonCfg.MoneyAmountDropRate[i] = (int)numMoneyRate[i].Value;
            }
            DropData.SaveCommon(baseDir, commonCfg);
            DropData.SaveMaps(baseDir, mapRows);
            bool reloaded = form.ReloadGameServer(CMD_RELOAD_COMMON, "Reload Common & Map");
            lblHint.Text = reloaded
                ? "Đã lưu Common.dat & MapManager.txt và Reload GameServer thành công!"
                : "Đã lưu Common.dat & MapManager.txt (GameServer chưa chạy — sẽ áp dụng khi bật server).";
        }
        catch (Exception ex)
        {
            MessageBox.Show("Lỗi lưu Common/Map: " + ex.Message, "Quản lý Drop", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    // =========================================================================
    // SUB-TAB 2: ITEMDROP.TXT
    // =========================================================================
    Panel BuildItemDropPage()
    {
        var page = new Panel { BackColor = Theme.Panel };

        var bar = new Panel { Dock = DockStyle.Top, Height = 38, BackColor = Theme.Panel };
        var lf = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, WrapContents = false, BackColor = Theme.Panel };
        var rf = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, BackColor = Theme.Panel };

        lf.Controls.Add(MakeSmallLbl("Tìm:"));
        tbDropSearch = K.Tb(210);
        tbDropSearch.Margin = new Padding(0, 4, 10, 0);
        tbDropSearch.TextChanged += delegate { BindItemDrops(); };
        lf.Controls.Add(tbDropSearch);

        lf.Controls.Add(MakeBtn("+ Thêm", Theme.Blue, false, delegate { AddOrEditDrop(-1); }));
        lf.Controls.Add(MakeBtn("✎ Sửa", Theme.Muted, true, delegate { EditSelectedDrop(); }));
        lf.Controls.Add(MakeBtn("⧉ Nhân bản", Theme.Muted, true, delegate { DupSelectedDrop(); }));
        lf.Controls.Add(MakeBtn("✕ Xóa", Theme.Red, true, delegate { DelSelectedDrop(); }));

        rf.Controls.Add(MakeBtn("💾 Lưu ItemDrop & Reload Item", Theme.Green, false, delegate { SaveItemDrops(); }));

        bar.Controls.Add(lf);
        bar.Controls.Add(rf);

        gridDrops = new DataGridView { Dock = DockStyle.Fill };
        K.StyleGrid(gridDrops);
        gridDrops.Columns.Add("item", "Vật phẩm");
        gridDrops.Columns.Add("lvl", "Lvl");
        gridDrops.Columns.Add("exc", "ExcIdx");
        gridDrops.Columns.Add("map", "Bản đồ");
        gridDrops.Columns.Add("mon", "Quái");
        gridDrops.Columns.Add("mlvl", "Cấp quái");
        gridDrops.Columns.Add("pct", "Tỉ lệ rơi (%)");
        gridDrops.Columns.Add("cmt", "Ghi chú");
        gridDrops.Columns[0].FillWeight = 165;
        gridDrops.Columns[1].FillWeight = 36;
        gridDrops.Columns[2].FillWeight = 48;
        gridDrops.Columns[3].FillWeight = 100;
        gridDrops.Columns[4].FillWeight = 100;
        gridDrops.Columns[5].FillWeight = 66;
        gridDrops.Columns[6].FillWeight = 80;
        gridDrops.Columns[7].FillWeight = 120;
        gridDrops.CellDoubleClick += delegate { EditSelectedDrop(); };

        page.Controls.Add(gridDrops);
        page.Controls.Add(bar);
        return page;
    }

    void BindItemDrops()
    {
        string q = (tbDropSearch.Text ?? "").Trim().ToLowerInvariant();
        gridDrops.Rows.Clear();
        dropViewIdx.Clear();
        for (int i = 0; i < dropRows.Count; i++)
        {
            var r = dropRows[i];
            string itemLabel = string.Format("[{0:00},{1:000}] {2}", r.Cat, r.Id, GetItemName(r.Cat, r.Id));
            string mapLabel = GetMapLabel(r.MapNum);
            string monLabel = GetMonsterLabel(r.Monster);
            if (q.Length > 0)
            {
                string hay = (itemLabel + " " + mapLabel + " " + monLabel + " " + r.Comment).ToLowerInvariant();
                if (!hay.Contains(q)) continue;
            }
            dropViewIdx.Add(i);
            gridDrops.Rows.Add(itemLabel, r.Level, r.ExcRate, mapLabel, monLabel,
                r.MinLvl + " ~ " + r.MaxLvl, string.Format("{0:0.000}%", r.Rate / 10000.0), r.Comment);
        }
    }

    int SelectedDropRealIndex()
    {
        if (gridDrops.CurrentRow == null) return -1;
        int vi = gridDrops.CurrentRow.Index;
        return (vi >= 0 && vi < dropViewIdx.Count) ? dropViewIdx[vi] : -1;
    }

    void EditSelectedDrop() { int idx = SelectedDropRealIndex(); if (idx >= 0) AddOrEditDrop(idx); }

    void DupSelectedDrop()
    {
        int idx = SelectedDropRealIndex();
        if (idx < 0) return;
        var s = dropRows[idx];
        var copy = new DropData.ItemDropRow
        {
            Cat = s.Cat, Id = s.Id, Level = s.Level, Grade = s.Grade,
            LevelRate = s.LevelRate, SkillRate = s.SkillRate, LuckRate = s.LuckRate, OptRate = s.OptRate, ExcRate = s.ExcRate,
            MapNum = s.MapNum, Monster = s.Monster, MinLvl = s.MinLvl, MaxLvl = s.MaxLvl, Rate = s.Rate, Comment = s.Comment
        };
        dropRows.Insert(idx + 1, copy);
        BindItemDrops();
    }

    void DelSelectedDrop()
    {
        int idx = SelectedDropRealIndex();
        if (idx < 0) return;
        dropRows.RemoveAt(idx);
        BindItemDrops();
    }

    void AddOrEditDrop(int editIdx)
    {
        var r = editIdx >= 0 ? dropRows[editIdx] : new DropData.ItemDropRow { Cat = 14, Id = 13, Rate = 10000, MinLvl = 1, MaxLvl = 400 };
        using (var d = MakeDialog(editIdx >= 0 ? "Sửa quy tắc ItemDrop" : "Thêm quy tắc ItemDrop mới", 470, 440))
        {
            int curCat = r.Cat, curId = r.Id;
            var lblItem = new Label { Text = string.Format("[{0:00},{1:000}] {2}", curCat, curId, GetItemName(curCat, curId)), Location = new Point(160, 18), Width = 200, ForeColor = Theme.Green, Font = new Font("Segoe UI Semibold", 9f) };
            var btnPick = MakeBtn("Chọn...", Theme.Blue, true, delegate
            {
                int nc, ni;
                if (PickItem(curCat, curId, out nc, out ni))
                {
                    curCat = nc; curId = ni;
                    lblItem.Text = string.Format("[{0:00},{1:000}] {2}", curCat, curId, GetItemName(curCat, curId));
                }
            });
            btnPick.Location = new Point(370, 12);
            d.Controls.Add(new Label { Text = "Vật phẩm:", Location = new Point(16, 18), AutoSize = true, ForeColor = Theme.Muted });
            d.Controls.Add(lblItem);
            d.Controls.Add(btnPick);

            var nLvl = K.Num(0, 15, Clamp(r.Level, 0, 15), 80);
            var nGrade = K.Num(0, 15, Clamp(r.Grade, 0, 15), 80);
            var tbLvlR = K.Tb(60); tbLvlR.Text = r.LevelRate;
            var tbSklR = K.Tb(60); tbSklR.Text = r.SkillRate;
            var tbLckR = K.Tb(60); tbLckR.Text = r.LuckRate;
            var tbOptR = K.Tb(60); tbOptR.Text = r.OptRate;
            var tbExcR = K.Tb(60); tbExcR.Text = r.ExcRate;

            var cbMap = K.Cb(220);
            cbMap.Items.Add("* (Tất cả bản đồ)");
            foreach (var m in mapRows) cbMap.Items.Add(m.Index + " - " + m.Name);
            SelectComboPrefix(cbMap, r.MapNum);

            var cbMon = K.Cb(220);
            cbMon.Items.Add("* (Tất cả quái)");
            foreach (var m in monsters) cbMon.Items.Add(m.Id + " - " + m.Name + " (Lv." + m.Level + ")");
            SelectComboPrefix(cbMon, r.Monster);

            var nMin = K.Num(0, 1000, Clamp(r.MinLvl, 0, 1000), 80);
            var nMax = K.Num(0, 1000, Clamp(r.MaxLvl, 0, 1000), 80);
            var nRate = MakePctNum(4, 0.0001m, 100m, ClampDec(r.Rate / 10000m, 0.0001m, 100m), 0.01m, 110);
            var lblPct = new Label { Text = "% (0.0001% ~ 100%)", Location = new Point(280, 284), AutoSize = true, ForeColor = Theme.Muted };
            var tbCmt = K.Tb(260); tbCmt.Text = string.IsNullOrEmpty(r.Comment) ? GetItemName(curCat, curId) : r.Comment;

            AddDialogRow(d, 52, "Level item (0~15):", nLvl);
            AddDialogRow2(d, 52, 255, "Grade:", nGrade);

            AddDialogRow(d, 86, "LvlRate / SkillRate:", tbLvlR);
            tbSklR.Location = new Point(230, 84); d.Controls.Add(tbSklR);
            AddDialogRow(d, 120, "Luck / Opt / Exc Idx:", tbLckR);
            tbOptR.Location = new Point(230, 118); d.Controls.Add(tbOptR);
            tbExcR.Location = new Point(300, 118); d.Controls.Add(tbExcR);

            var lblTipOpt = new Label { Text = "(Dùng * hoặc Index trong ItemOptionRate.txt: Exc=0 thường, Exc=1 đồ Exc)", Location = new Point(16, 148), AutoSize = true, ForeColor = Theme.Muted, Font = new Font("Segoe UI", 8f) };
            d.Controls.Add(lblTipOpt);

            AddDialogRow(d, 174, "Bản đồ (MapNum):", cbMap);
            AddDialogRow(d, 210, "Quái vật (Monster):", cbMon);
            AddDialogRow(d, 246, "Cấp quái (Min ~ Max):", nMin);
            AddDialogRow2(d, 246, 255, "đến:", nMax);
            AddDialogRow(d, 282, "Tỉ lệ rơi (%):", nRate);
            d.Controls.Add(lblPct);
            AddDialogRow(d, 318, "Ghi chú:", tbCmt);

            var ok = MakeBtn("Lưu dòng", Theme.Green, false, delegate { d.DialogResult = DialogResult.OK; d.Close(); });
            ok.Location = new Point(240, 362);
            var cancel = MakeBtn("Hủy", Theme.Muted, true, delegate { d.Close(); });
            cancel.Location = new Point(350, 362);
            d.Controls.Add(ok); d.Controls.Add(cancel);

            if (d.ShowDialog(this) == DialogResult.OK)
            {
                r.Cat = curCat; r.Id = curId;
                r.Level = (int)nLvl.Value; r.Grade = (int)nGrade.Value;
                r.LevelRate = CleanToken(tbLvlR.Text, "*");
                r.SkillRate = CleanToken(tbSklR.Text, "*");
                r.LuckRate = CleanToken(tbLckR.Text, "*");
                r.OptRate = CleanToken(tbOptR.Text, "*");
                r.ExcRate = CleanToken(tbExcR.Text, "0");
                r.MapNum = ExtractComboPrefix(cbMap);
                r.Monster = ExtractComboPrefix(cbMon);
                r.MinLvl = (int)nMin.Value; r.MaxLvl = (int)nMax.Value;
                r.Rate = Math.Max(1, (int)Math.Round(nRate.Value * 10000m));
                r.Comment = string.IsNullOrEmpty(tbCmt.Text.Trim()) ? GetItemName(curCat, curId) : tbCmt.Text.Trim();
                if (editIdx < 0) dropRows.Add(r);
                BindItemDrops();
            }
        }
    }

    void SaveItemDrops()
    {
        try
        {
            DropData.SaveItemDrops(baseDir, dropRows);
            bool reloaded = form.ReloadGameServer(CMD_RELOAD_ITEM, "Reload ItemDrop");
            lblHint.Text = reloaded
                ? "Đã lưu ItemDrop.txt và Reload Item trên GameServer!"
                : "Đã lưu ItemDrop.txt (GameServer chưa chạy).";
        }
        catch (Exception ex)
        {
            MessageBox.Show("Lỗi lưu ItemDrop.txt: " + ex.Message, "Quản lý Drop", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    // =========================================================================
    // SUB-TAB 3: EVENTITEMBAG
    // =========================================================================
    Panel BuildBagPage()
    {
        var page = new Panel { BackColor = Theme.Panel };

        // Left: bag list
        var left = new Panel { Dock = DockStyle.Left, Width = 250, BackColor = Theme.Panel, Padding = new Padding(0, 0, 10, 0) };
        tbBagSearch = K.Tb(238);
        tbBagSearch.Dock = DockStyle.Top;
        tbBagSearch.TextChanged += delegate { BindBagList(); };
        var lblSearch = new Label { Text = "Danh sách EventItemBag (37 túi):", Dock = DockStyle.Top, Height = 22, ForeColor = Theme.Muted };
        lbBags = new ListBox { Dock = DockStyle.Fill, BackColor = Theme.PanelHi, ForeColor = Theme.Text, BorderStyle = BorderStyle.FixedSingle, IntegralHeight = false };
        lbBags.SelectedIndexChanged += delegate { OnSelectBag(); };
        left.Controls.Add(lbBags);
        left.Controls.Add(tbBagSearch);
        left.Controls.Add(lblSearch);

        // Right: bag editor
        var right = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Panel };

        var headCard = new Panel { Dock = DockStyle.Top, Height = 104, BackColor = Theme.PanelHi, Padding = new Padding(10, 6, 10, 6) };
        lblBagTitle = new Label { Text = "Chọn 1 túi ở danh sách bên trái", Dock = DockStyle.Top, Height = 24, ForeColor = Theme.Green, Font = new Font("Segoe UI Semibold", 10f) };
        var hFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, BackColor = Theme.PanelHi };

        var hr1 = K.Row(hFlow);
        hr1.Controls.Add(MakeSmallLbl("DropZen:"));
        numBagZen = K.Num(0, 2000000000, 1000, 88); numBagZen.Margin = new Padding(0, 2, 10, 0);
        hr1.Controls.Add(numBagZen);
        hr1.Controls.Add(MakeSmallLbl("ItemDropRate (%):"));
        numBagRate = K.Num(0, 100, 100, 56); numBagRate.Margin = new Padding(0, 2, 10, 0);
        hr1.Controls.Add(numBagRate);
        hr1.Controls.Add(MakeSmallLbl("Số lượng:"));
        numBagCount = K.Num(1, 20, 1, 50); numBagCount.Margin = new Padding(0, 2, 10, 0);
        hr1.Controls.Add(numBagCount);

        var hr2 = K.Row(hFlow);
        hr2.Controls.Add(MakeSmallLbl("Chế độ:"));
        cbBagType = K.Cb(170);
        cbBagType.Items.AddRange(new object[] { "0 - Normal (Section 1)", "1 - Advanced (Section 4)" });
        cbBagType.SelectedIndex = 0;
        cbBagType.Margin = new Padding(0, 2, 10, 0);
        hr2.Controls.Add(cbBagType);
        chkBagFw = K.Ck("Pháo hoa (Fireworks)", false);
        chkBagFw.Margin = new Padding(0, 4, 14, 0);
        hr2.Controls.Add(chkBagFw);
        hr2.Controls.Add(MakeBtn("💾 Lưu túi & Reload Bag", Theme.Green, false, delegate { SaveCurrentBag(); }));

        EventHandler syncHead = delegate
        {
            if (bagUpdating || curBag == null) return;
            curBag.DropZen = (int)numBagZen.Value;
            curBag.ItemDropRate = (int)numBagRate.Value;
            curBag.ItemDropCount = (int)numBagCount.Value;
            curBag.ItemDropType = cbBagType.SelectedIndex;
            curBag.Fireworks = chkBagFw.Checked ? 1 : 0;
        };
        numBagZen.ValueChanged += syncHead;
        numBagRate.ValueChanged += syncHead;
        numBagCount.ValueChanged += syncHead;
        cbBagType.SelectedIndexChanged += syncHead;
        chkBagFw.CheckedChanged += syncHead;

        headCard.Controls.Add(hFlow);
        headCard.Controls.Add(lblBagTitle);

        // Split: Normal (top) & Advanced (bottom)
        var split = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, BackColor = Theme.Border, SplitterWidth = 4 };
        split.Panel1.BackColor = Theme.Panel;
        split.Panel2.BackColor = Theme.Panel;

        // Normal Drop panel
        var normBar = new Panel { Dock = DockStyle.Top, Height = 34, BackColor = Theme.Panel };
        var normLbl = new Label { Text = "Normal Drop (Section 1 — dùng khi Chế độ = 0)", Dock = DockStyle.Left, AutoSize = true, ForeColor = Theme.Text, Font = new Font("Segoe UI Semibold", 9f), Padding = new Padding(0, 7, 0, 0) };
        var normBtns = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, BackColor = Theme.Panel };
        normBtns.Controls.Add(MakeBtn("+ Thêm", Theme.Blue, false, delegate { AddOrEditBagNormal(-1); }));
        normBtns.Controls.Add(MakeBtn("✎ Sửa", Theme.Muted, true, delegate { EditBagNormal(); }));
        normBtns.Controls.Add(MakeBtn("✕ Xóa", Theme.Red, true, delegate { DelBagNormal(); }));
        normBar.Controls.Add(normLbl); normBar.Controls.Add(normBtns);

        gridBagNorm = new DataGridView { Dock = DockStyle.Fill };
        K.StyleGrid(gridBagNorm);
        gridBagNorm.Columns.Add("item", "Vật phẩm");
        gridBagNorm.Columns.Add("lvl", "Min~Max Lvl");
        gridBagNorm.Columns.Add("sk", "Skill");
        gridBagNorm.Columns.Add("lk", "Luck");
        gridBagNorm.Columns.Add("op", "Opt");
        gridBagNorm.Columns.Add("ex", "Exc");
        gridBagNorm.Columns.Add("cmt", "Ghi chú");
        gridBagNorm.Columns[0].FillWeight = 160;
        gridBagNorm.Columns[1].FillWeight = 70;
        gridBagNorm.Columns[2].FillWeight = 42;
        gridBagNorm.Columns[3].FillWeight = 42;
        gridBagNorm.Columns[4].FillWeight = 42;
        gridBagNorm.Columns[5].FillWeight = 42;
        gridBagNorm.Columns[6].FillWeight = 110;
        gridBagNorm.CellDoubleClick += delegate { EditBagNormal(); };
        split.Panel1.Controls.Add(gridBagNorm);
        split.Panel1.Controls.Add(normBar);

        // Advanced Drop panel
        var advBar = new Panel { Dock = DockStyle.Top, Height = 34, BackColor = Theme.Panel };
        var advLbl = new Label { Text = "Advanced Drop (Section 4 — dùng khi Chế độ = 1)", Dock = DockStyle.Left, AutoSize = true, ForeColor = Theme.Text, Font = new Font("Segoe UI Semibold", 9f), Padding = new Padding(0, 7, 0, 0) };
        var advBtns = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, BackColor = Theme.Panel };
        advBtns.Controls.Add(MakeBtn("+ Thêm", Theme.Blue, false, delegate { AddOrEditBagAdv(-1); }));
        advBtns.Controls.Add(MakeBtn("✎ Sửa", Theme.Muted, true, delegate { EditBagAdv(); }));
        advBtns.Controls.Add(MakeBtn("✕ Xóa", Theme.Red, true, delegate { DelBagAdv(); }));
        advBar.Controls.Add(advLbl); advBar.Controls.Add(advBtns);

        gridBagAdv = new DataGridView { Dock = DockStyle.Fill };
        K.StyleGrid(gridBagAdv);
        gridBagAdv.Columns.Add("item", "Vật phẩm");
        gridBagAdv.Columns.Add("lvl", "Lvl");
        gridBagAdv.Columns.Add("grd", "Grade");
        gridBagAdv.Columns.Add("lr", "LvlRate");
        gridBagAdv.Columns.Add("sr", "SkillRate");
        gridBagAdv.Columns.Add("lkr", "LuckRate");
        gridBagAdv.Columns.Add("or", "OptRate");
        gridBagAdv.Columns.Add("er", "ExcRate");
        gridBagAdv.Columns.Add("cmt", "Ghi chú");
        gridBagAdv.Columns[0].FillWeight = 150;
        gridBagAdv.Columns[8].FillWeight = 100;
        gridBagAdv.CellDoubleClick += delegate { EditBagAdv(); };
        split.Panel2.Controls.Add(gridBagAdv);
        split.Panel2.Controls.Add(advBar);

        right.Controls.Add(split);
        right.Controls.Add(new Panel { Dock = DockStyle.Top, Height = 6, BackColor = Theme.Panel });
        right.Controls.Add(headCard);

        page.Controls.Add(right);
        page.Controls.Add(left);
        return page;
    }

    void BindBagList()
    {
        string q = (tbBagSearch.Text ?? "").Trim().ToLowerInvariant();
        int prevSel = lbBags.SelectedIndex;
        lbBags.Items.Clear();
        bagViewIdx.Clear();
        for (int i = 0; i < bagFiles.Count; i++)
        {
            var b = bagFiles[i];
            if (q.Length > 0 && !b.FileName.ToLowerInvariant().Contains(q)) continue;
            bagViewIdx.Add(i);
            int total = b.NormalItems.Count + b.AdvItems.Count;
            string label = Path.GetFileNameWithoutExtension(b.FileName) + (total > 0 ? "  [" + total + "]" : "");
            lbBags.Items.Add(label);
        }
        if (lbBags.Items.Count > 0)
        {
            lbBags.SelectedIndex = (prevSel >= 0 && prevSel < lbBags.Items.Count) ? prevSel : 0;
        }
    }

    void OnSelectBag()
    {
        int si = lbBags.SelectedIndex;
        if (si < 0 || si >= bagViewIdx.Count) { curBag = null; return; }
        curBag = bagFiles[bagViewIdx[si]];
        bagUpdating = true;
        lblBagTitle.Text = curBag.FileName;
        numBagZen.Value = Clamp(curBag.DropZen, 0, 2000000000);
        numBagRate.Value = Clamp(curBag.ItemDropRate, 0, 100);
        numBagCount.Value = Clamp(curBag.ItemDropCount, 1, 20);
        cbBagType.SelectedIndex = curBag.ItemDropType == 1 ? 1 : 0;
        chkBagFw.Checked = curBag.Fireworks != 0;
        bagUpdating = false;
        BindBagGrids();
    }

    void BindBagGrids()
    {
        gridBagNorm.Rows.Clear();
        gridBagAdv.Rows.Clear();
        if (curBag == null) return;
        foreach (var it in curBag.NormalItems)
        {
            gridBagNorm.Rows.Add(string.Format("[{0:00},{1:000}] {2}", it.Cat, it.Id, GetItemName(it.Cat, it.Id)),
                it.MinLvl + "~" + it.MaxLvl, it.Skill, it.Luck, it.Opt, it.Exc, it.Comment);
        }
        foreach (var it in curBag.AdvItems)
        {
            gridBagAdv.Rows.Add(string.Format("[{0:00},{1:000}] {2}", it.Cat, it.Id, GetItemName(it.Cat, it.Id)),
                it.Level, it.Grade, it.LevelRate, it.SkillRate, it.LuckRate, it.OptRate, it.ExcRate, it.Comment);
        }
    }

    void EditBagNormal() { if (gridBagNorm.CurrentRow != null) AddOrEditBagNormal(gridBagNorm.CurrentRow.Index); }
    void DelBagNormal()
    {
        if (curBag == null || gridBagNorm.CurrentRow == null) return;
        int i = gridBagNorm.CurrentRow.Index;
        if (i >= 0 && i < curBag.NormalItems.Count) { curBag.NormalItems.RemoveAt(i); BindBagGrids(); }
    }

    void AddOrEditBagNormal(int editIdx)
    {
        if (curBag == null) return;
        var it = editIdx >= 0 ? curBag.NormalItems[editIdx] : new DropData.BagNormalRow { Cat = 0, Id = 0, Skill = 1, Luck = 1, Opt = 1, Exc = 1 };
        using (var d = MakeDialog(editIdx >= 0 ? "Sửa vật phẩm (Normal)" : "Thêm vật phẩm (Normal)", 440, 310))
        {
            int curCat = it.Cat, curId = it.Id;
            var lblItem = new Label { Text = string.Format("[{0:00},{1:000}] {2}", curCat, curId, GetItemName(curCat, curId)), Location = new Point(150, 18), Width = 190, ForeColor = Theme.Green, Font = new Font("Segoe UI Semibold", 9f) };
            var btnPick = MakeBtn("Chọn...", Theme.Blue, true, delegate
            {
                int nc, ni;
                if (PickItem(curCat, curId, out nc, out ni)) { curCat = nc; curId = ni; lblItem.Text = string.Format("[{0:00},{1:000}] {2}", curCat, curId, GetItemName(curCat, curId)); }
            });
            btnPick.Location = new Point(345, 12);
            d.Controls.Add(new Label { Text = "Vật phẩm:", Location = new Point(16, 18), AutoSize = true, ForeColor = Theme.Muted });
            d.Controls.Add(lblItem); d.Controls.Add(btnPick);

            var nMin = K.Num(0, 15, Clamp(it.MinLvl, 0, 15), 70);
            var nMax = K.Num(0, 15, Clamp(it.MaxLvl, 0, 15), 70);
            var ckSk = K.Ck("Skill", it.Skill != 0); ckSk.Location = new Point(150, 92);
            var ckLk = K.Ck("Luck", it.Luck != 0); ckLk.Location = new Point(220, 92);
            var ckOp = K.Ck("Option", it.Opt != 0); ckOp.Location = new Point(290, 92);
            var ckEx = K.Ck("Excellent", it.Exc != 0); ckEx.Location = new Point(150, 124);
            var tbCmt = K.Tb(240); tbCmt.Text = string.IsNullOrEmpty(it.Comment) ? GetItemName(curCat, curId) : it.Comment;

            AddDialogRow(d, 52, "Cấp Min ~ Max:", nMin);
            AddDialogRow2(d, 52, 235, "đến:", nMax);
            d.Controls.Add(new Label { Text = "Thuộc tính:", Location = new Point(16, 94), AutoSize = true, ForeColor = Theme.Muted });
            d.Controls.Add(ckSk); d.Controls.Add(ckLk); d.Controls.Add(ckOp); d.Controls.Add(ckEx);
            AddDialogRow(d, 160, "Ghi chú:", tbCmt);

            var ok = MakeBtn("Lưu", Theme.Green, false, delegate { d.DialogResult = DialogResult.OK; d.Close(); });
            ok.Location = new Point(230, 215);
            var cancel = MakeBtn("Hủy", Theme.Muted, true, delegate { d.Close(); });
            cancel.Location = new Point(325, 215);
            d.Controls.Add(ok); d.Controls.Add(cancel);

            if (d.ShowDialog(this) == DialogResult.OK)
            {
                it.Cat = curCat; it.Id = curId;
                it.MinLvl = (int)nMin.Value; it.MaxLvl = (int)nMax.Value;
                it.Skill = ckSk.Checked ? 1 : 0; it.Luck = ckLk.Checked ? 1 : 0;
                it.Opt = ckOp.Checked ? 1 : 0; it.Exc = ckEx.Checked ? 1 : 0;
                it.Comment = string.IsNullOrEmpty(tbCmt.Text.Trim()) ? GetItemName(curCat, curId) : tbCmt.Text.Trim();
                if (editIdx < 0) curBag.NormalItems.Add(it);
                BindBagGrids();
            }
        }
    }

    void EditBagAdv() { if (gridBagAdv.CurrentRow != null) AddOrEditBagAdv(gridBagAdv.CurrentRow.Index); }
    void DelBagAdv()
    {
        if (curBag == null || gridBagAdv.CurrentRow == null) return;
        int i = gridBagAdv.CurrentRow.Index;
        if (i >= 0 && i < curBag.AdvItems.Count) { curBag.AdvItems.RemoveAt(i); BindBagGrids(); }
    }

    void AddOrEditBagAdv(int editIdx)
    {
        if (curBag == null) return;
        var it = editIdx >= 0 ? curBag.AdvItems[editIdx] : new DropData.BagAdvRow { Cat = 0, Id = 0, LevelRate = "*", SkillRate = "*", LuckRate = "*", OptRate = "*", ExcRate = "1" };
        using (var d = MakeDialog(editIdx >= 0 ? "Sửa vật phẩm (Advanced)" : "Thêm vật phẩm (Advanced)", 450, 320))
        {
            int curCat = it.Cat, curId = it.Id;
            var lblItem = new Label { Text = string.Format("[{0:00},{1:000}] {2}", curCat, curId, GetItemName(curCat, curId)), Location = new Point(155, 18), Width = 190, ForeColor = Theme.Green, Font = new Font("Segoe UI Semibold", 9f) };
            var btnPick = MakeBtn("Chọn...", Theme.Blue, true, delegate
            {
                int nc, ni;
                if (PickItem(curCat, curId, out nc, out ni)) { curCat = nc; curId = ni; lblItem.Text = string.Format("[{0:00},{1:000}] {2}", curCat, curId, GetItemName(curCat, curId)); }
            });
            btnPick.Location = new Point(350, 12);
            d.Controls.Add(new Label { Text = "Vật phẩm:", Location = new Point(16, 18), AutoSize = true, ForeColor = Theme.Muted });
            d.Controls.Add(lblItem); d.Controls.Add(btnPick);

            var nLvl = K.Num(0, 15, Clamp(it.Level, 0, 15), 70);
            var nGrd = K.Num(0, 15, Clamp(it.Grade, 0, 15), 70);
            var tbLr = K.Tb(60); tbLr.Text = it.LevelRate;
            var tbSr = K.Tb(60); tbSr.Text = it.SkillRate;
            var tbLkr = K.Tb(60); tbLkr.Text = it.LuckRate;
            var tbOr = K.Tb(60); tbOr.Text = it.OptRate;
            var tbEr = K.Tb(60); tbEr.Text = it.ExcRate;
            var tbCmt = K.Tb(250); tbCmt.Text = string.IsNullOrEmpty(it.Comment) ? GetItemName(curCat, curId) : it.Comment;

            AddDialogRow(d, 54, "Level / Grade:", nLvl);
            AddDialogRow2(d, 54, 240, "Grade:", nGrd);
            AddDialogRow(d, 90, "LvlRate / SkillRate:", tbLr);
            tbSr.Location = new Point(230, 88); d.Controls.Add(tbSr);
            AddDialogRow(d, 126, "Luck / Opt / Exc:", tbLkr);
            tbOr.Location = new Point(230, 124); d.Controls.Add(tbOr);
            tbEr.Location = new Point(300, 124); d.Controls.Add(tbEr);
            AddDialogRow(d, 166, "Ghi chú:", tbCmt);

            var ok = MakeBtn("Lưu", Theme.Green, false, delegate { d.DialogResult = DialogResult.OK; d.Close(); });
            ok.Location = new Point(240, 224);
            var cancel = MakeBtn("Hủy", Theme.Muted, true, delegate { d.Close(); });
            cancel.Location = new Point(335, 224);
            d.Controls.Add(ok); d.Controls.Add(cancel);

            if (d.ShowDialog(this) == DialogResult.OK)
            {
                it.Cat = curCat; it.Id = curId;
                it.Level = (int)nLvl.Value; it.Grade = (int)nGrd.Value;
                it.LevelRate = CleanToken(tbLr.Text, "*");
                it.SkillRate = CleanToken(tbSr.Text, "*");
                it.LuckRate = CleanToken(tbLkr.Text, "*");
                it.OptRate = CleanToken(tbOr.Text, "*");
                it.ExcRate = CleanToken(tbEr.Text, "*");
                it.Comment = string.IsNullOrEmpty(tbCmt.Text.Trim()) ? GetItemName(curCat, curId) : tbCmt.Text.Trim();
                if (editIdx < 0) curBag.AdvItems.Add(it);
                BindBagGrids();
            }
        }
    }

    void SaveCurrentBag()
    {
        if (curBag == null) return;
        try
        {
            DropData.SaveBag(curBag);
            BindBagList();
            bool reloaded = form.ReloadGameServer(CMD_RELOAD_BAG, "Reload EventItemBag");
            lblHint.Text = reloaded
                ? "Đã lưu " + curBag.FileName + " và Reload EventItemBag trên GameServer!"
                : "Đã lưu " + curBag.FileName + " (GameServer chưa chạy).";
        }
        catch (Exception ex)
        {
            MessageBox.Show("Lỗi lưu túi đồ: " + ex.Message, "Quản lý Drop", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    // =========================================================================
    // SUB-TAB 4: ITEMOPTIONRATE.TXT
    // =========================================================================
    Panel BuildOptRatePage()
    {
        var page = new Panel { BackColor = Theme.Panel };

        var bar = new Panel { Dock = DockStyle.Top, Height = 38, BackColor = Theme.Panel };
        var lf = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, WrapContents = false, BackColor = Theme.Panel };
        var rf = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, BackColor = Theme.Panel };

        lf.Controls.Add(MakeSmallLbl("Nhóm tỉ lệ:"));
        cbOptSec = K.Cb(240);
        cbOptSec.Items.AddRange(new object[]
        {
            "0 - Level Option (+0 ~ +11)",
            "1 - Skill Option (Không / Có Skill)",
            "2 - Luck Option (Không / Có Luck)",
            "3 - JoL Option (+0 ~ +28)",
            "4 - Excellent Option (0 ~ 6 dòng Exc)"
        });
        cbOptSec.SelectedIndex = 4;
        cbOptSec.Margin = new Padding(0, 4, 10, 0);
        cbOptSec.SelectedIndexChanged += delegate { BindOptRates(); };
        lf.Controls.Add(cbOptSec);
        lf.Controls.Add(MakeBtn("✎ Sửa dòng đang chọn", Theme.Blue, true, delegate { EditSelectedOpt(); }));

        rf.Controls.Add(MakeBtn("💾 Lưu ItemOptionRate & Reload Item", Theme.Green, false, delegate { SaveOptRates(); }));
        bar.Controls.Add(lf); bar.Controls.Add(rf);

        gridOpts = new DataGridView { Dock = DockStyle.Fill };
        K.StyleGrid(gridOpts);
        gridOpts.CellDoubleClick += delegate { EditSelectedOpt(); };

        page.Controls.Add(gridOpts);
        page.Controls.Add(bar);
        return page;
    }

    void BindOptRates()
    {
        int sec = cbOptSec.SelectedIndex;
        if (sec < 0) sec = 0;
        int[] colCounts = new[] { 12, 2, 2, 8, 7 };
        int n = colCounts[sec];

        gridOpts.Columns.Clear();
        gridOpts.Rows.Clear();
        optViewIdx.Clear();

        gridOpts.Columns.Add("idx", "Idx");
        gridOpts.Columns[0].FillWeight = 36;
        for (int c = 0; c < n; c++)
        {
            string h = (sec == 1 || sec == 2) ? (c == 0 ? "Không" : "Có") : (sec == 4 ? (c + " dòng") : ("+" + c));
            gridOpts.Columns.Add("r" + c, h);
            gridOpts.Columns[c + 1].FillWeight = 55;
        }
        gridOpts.Columns.Add("sum", "Tổng");
        gridOpts.Columns[n + 1].FillWeight = 60;
        gridOpts.Columns.Add("cmt", "Ghi chú");
        gridOpts.Columns[n + 2].FillWeight = 160;

        for (int i = 0; i < optRows.Count; i++)
        {
            var r = optRows[i];
            if (r.Section != sec) continue;
            optViewIdx.Add(i);
            var vals = new object[n + 3];
            vals[0] = r.Index;
            int sum = 0;
            for (int c = 0; c < n; c++)
            {
                int v = c < r.Rates.Count ? r.Rates[c] : 0;
                vals[c + 1] = string.Format("{0:0.##}%", v / 100.0);
                sum += v;
            }
            vals[n + 1] = string.Format("{0:0.##}%", sum / 100.0);
            vals[n + 2] = r.Comment;
            int ri = gridOpts.Rows.Add(vals);
            gridOpts.Rows[ri].Cells[n + 1].Style.ForeColor = (sum == 10000) ? Theme.Green : Theme.Amber;
        }
    }

    void EditSelectedOpt()
    {
        if (gridOpts.CurrentRow == null) return;
        int vi = gridOpts.CurrentRow.Index;
        if (vi < 0 || vi >= optViewIdx.Count) return;
        var r = optRows[optViewIdx[vi]];

        int count = r.Rates.Count;
        int rowsNeeded = (count + 1) / 2;
        using (var d = MakeDialog("Sửa OptionRate #" + r.Index + " (" + r.Comment + ")", 460, 145 + rowsNeeded * 34))
        {
            var nums = new NumericUpDown[count];
            var lblSum = new Label { Location = new Point(16, 16 + rowsNeeded * 34), AutoSize = true, Font = new Font("Segoe UI Semibold", 9.5f) };
            Action updateSum = delegate
            {
                decimal s = 0m;
                for (int i = 0; i < count; i++) s += nums[i].Value;
                lblSum.Text = string.Format("Tổng = {0:0.00}% / 100.00%", s) + (s == 100m ? "  (Chuẩn)" : "  (Nên bằng 100%)");
                lblSum.ForeColor = (s == 100m) ? Theme.Green : Theme.Amber;
            };

            for (int i = 0; i < count; i++)
            {
                int col = i % 2;
                int row = i / 2;
                int x = col == 0 ? 16 : 235;
                int y = 16 + row * 34;
                d.Controls.Add(new Label { Text = "Mức " + i + " (%):", Location = new Point(x, y + 3), AutoSize = true, ForeColor = Theme.Muted });
                nums[i] = MakePctNum(2, 0m, 100m, ClampDec(r.Rates[i] / 100m, 0m, 100m), 1m, 88);
                nums[i].Location = new Point(x + 82, y);
                nums[i].ValueChanged += delegate { updateSum(); };
                d.Controls.Add(nums[i]);
            }
            updateSum();
            d.Controls.Add(lblSum);

            int by = 50 + rowsNeeded * 34;
            var ok = MakeBtn("Cập nhật", Theme.Green, false, delegate { d.DialogResult = DialogResult.OK; d.Close(); });
            ok.Location = new Point(240, by);
            var cancel = MakeBtn("Hủy", Theme.Muted, true, delegate { d.Close(); });
            cancel.Location = new Point(345, by);
            d.Controls.Add(ok); d.Controls.Add(cancel);

            if (d.ShowDialog(this) == DialogResult.OK)
            {
                for (int i = 0; i < count; i++) r.Rates[i] = (int)Math.Round(nums[i].Value * 100m);
                BindOptRates();
            }
        }
    }

    void SaveOptRates()
    {
        try
        {
            DropData.SaveOptionRates(baseDir, optRows);
            bool reloaded = form.ReloadGameServer(CMD_RELOAD_ITEM, "Reload ItemOptionRate");
            lblHint.Text = reloaded
                ? "Đã lưu ItemOptionRate.txt và Reload Item trên GameServer!"
                : "Đã lưu ItemOptionRate.txt (GameServer chưa chạy).";
        }
        catch (Exception ex)
        {
            MessageBox.Show("Lỗi lưu ItemOptionRate.txt: " + ex.Message, "Quản lý Drop", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    // =========================================================================
    // SEARCHABLE ITEM PICKER DIALOG
    // =========================================================================
    bool PickItem(int curCat, int curId, out int outCat, out int outId)
    {
        outCat = curCat; outId = curId;
        using (var d = MakeDialog("Chọn vật phẩm từ Item.txt", 480, 480))
        {
            var cbCat = K.Cb(200);
            cbCat.Location = new Point(12, 12);
            cbCat.Items.Add("Tất cả nhóm (0 ~ 15)");
            foreach (string cn in DropData.CatNames) cbCat.Items.Add(cn);
            cbCat.SelectedIndex = 0;

            var tbQ = K.Tb(238);
            tbQ.Location = new Point(220, 12);

            var lb = new ListBox { Location = new Point(12, 44), Size = new Size(446, 345), BackColor = Theme.PanelHi, ForeColor = Theme.Text, BorderStyle = BorderStyle.FixedSingle, Font = K.Mono };
            var filtered = new List<DropData.ItemDef>();

            Action refresh = delegate
            {
                string q = (tbQ.Text ?? "").Trim().ToLowerInvariant();
                int catFilter = cbCat.SelectedIndex - 1;
                lb.Items.Clear();
                filtered.Clear();
                foreach (var it in items)
                {
                    if (catFilter >= 0 && it.Cat != catFilter) continue;
                    string s = it.ToString();
                    if (q.Length > 0 && !s.ToLowerInvariant().Contains(q)) continue;
                    filtered.Add(it);
                    lb.Items.Add(s);
                }
                if (lb.Items.Count > 0) lb.SelectedIndex = 0;
            };
            cbCat.SelectedIndexChanged += delegate { refresh(); };
            tbQ.TextChanged += delegate { refresh(); };
            lb.DoubleClick += delegate { if (lb.SelectedIndex >= 0) { d.DialogResult = DialogResult.OK; d.Close(); } };
            refresh();

            var ok = MakeBtn("Chọn", Theme.Green, false, delegate { if (lb.SelectedIndex >= 0) { d.DialogResult = DialogResult.OK; d.Close(); } });
            ok.Location = new Point(275, 400);
            var cancel = MakeBtn("Hủy", Theme.Muted, true, delegate { d.Close(); });
            cancel.Location = new Point(370, 400);

            d.Controls.Add(cbCat); d.Controls.Add(tbQ); d.Controls.Add(lb); d.Controls.Add(ok); d.Controls.Add(cancel);
            if (d.ShowDialog(this) == DialogResult.OK && lb.SelectedIndex >= 0 && lb.SelectedIndex < filtered.Count)
            {
                outCat = filtered[lb.SelectedIndex].Cat;
                outId = filtered[lb.SelectedIndex].Id;
                return true;
            }
        }
        return false;
    }

    // =========================================================================
    // UI HELPERS
    // =========================================================================
    static Label MakeFieldLbl(string text)
    {
        return new Label { Text = text, Width = 115, ForeColor = Theme.Text, Font = new Font("Segoe UI Semibold", 9f), Margin = new Padding(0, 5, 4, 0) };
    }

    static Label MakeSmallLbl(string text)
    {
        return new Label { Text = text, AutoSize = true, ForeColor = Theme.Muted, Margin = new Padding(0, 5, 4, 0) };
    }

    static Form MakeDialog(string title, int w, int h)
    {
        return new Form
        {
            Text = title,
            Size = new Size(w, h),
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false,
            MinimizeBox = false,
            StartPosition = FormStartPosition.CenterParent,
            BackColor = Theme.Panel,
            ForeColor = Theme.Text,
            Font = new Font("Segoe UI", 9f)
        };
    }

    static void AddDialogRow(Form d, int y, string label, Control c)
    {
        d.Controls.Add(new Label { Text = label, Location = new Point(16, y + 3), AutoSize = true, ForeColor = Theme.Muted });
        c.Location = new Point(160, y);
        d.Controls.Add(c);
    }

    static void AddDialogRow2(Form d, int y, int x, string label, Control c)
    {
        d.Controls.Add(new Label { Text = label, Location = new Point(x, y + 3), AutoSize = true, ForeColor = Theme.Muted });
        c.Location = new Point(x + 48, y);
        d.Controls.Add(c);
    }

    static int Clamp(int v, int min, int max) { return v < min ? min : (v > max ? max : v); }
    static decimal ClampDec(decimal v, decimal min, decimal max) { return v < min ? min : (v > max ? max : v); }

    static NumericUpDown MakePctNum(int decimals, decimal min, decimal max, decimal val, decimal inc, int width)
    {
        return new NumericUpDown
        {
            DecimalPlaces = decimals,
            Minimum = min,
            Maximum = max,
            Value = ClampDec(val, min, max),
            Increment = inc,
            Width = width,
            BackColor = Theme.PanelHi,
            ForeColor = Theme.Text,
            BorderStyle = BorderStyle.FixedSingle
        };
    }

    static string CleanToken(string s, string def)
    {
        string t = (s ?? "").Trim();
        return t.Length == 0 ? def : t;
    }

    static void SelectComboPrefix(ComboBox cb, string val)
    {
        if (val == "*") { cb.SelectedIndex = 0; return; }
        for (int i = 1; i < cb.Items.Count; i++)
        {
            string s = cb.Items[i].ToString();
            if (s.StartsWith(val + " ")) { cb.SelectedIndex = i; return; }
        }
        cb.SelectedIndex = 0;
    }

    static string ExtractComboPrefix(ComboBox cb)
    {
        if (cb.SelectedIndex <= 0) return "*";
        string s = cb.SelectedItem.ToString();
        int sp = s.IndexOf(' ');
        return sp > 0 ? s.Substring(0, sp) : "*";
    }
}
