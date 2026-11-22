using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

// Parsers and writers for all MU 0.97k server drop configuration files.
static class DropData
{
    public class ItemDef
    {
        public int Cat;
        public int Id;
        public string Name;
        public string Key { get { return string.Format("{0:00},{1:000}", Cat, Id); } }
        public override string ToString() { return string.Format("[{0:00},{1:000}] {2}", Cat, Id, Name); }
    }

    public class MonsterDef
    {
        public int Id;
        public string Name;
        public int Level;
        public override string ToString() { return string.Format("[{0}] {1} (Lv.{2})", Id, Name, Level); }
    }

    public class CommonDropCfg
    {
        public int ItemDropTime = 30;
        public int[] ItemDropRate = new int[] { 100, 100, 100, 100 };
        public int MoneyDropTime = 30;
        public int[] MoneyAmountDropRate = new int[] { 50, 50, 50, 50 };
        public int[] AddExperienceRate = new int[] { 1, 1, 1, 1 };
        public int[] AddEventExperienceRate = new int[] { 2, 2, 2, 2 };
    }

    public class MapRow
    {
        public int Index;
        public string NonPK = "*";
        public int View = 15;
        public int ExpRate = 100;
        public int ItemDropRate = 100;
        public int ExcDropRate = 0;
        public int DeadGate = 17;
        public int SpawnInPlace = 0;
        public int FlyingDragons = 1;
        public string Name = "";
    }

    public class ItemDropRow
    {
        public int Cat;
        public int Id;
        public int Level;
        public int Grade;
        public string LevelRate = "1";
        public string SkillRate = "3";
        public string LuckRate = "5";
        public string OptRate = "1";
        public string ExcRate = "0";
        public string MapNum = "*";
        public string Monster = "*";
        public int MinLvl = 1;
        public int MaxLvl = 400;
        public int Rate = 10000;
        public string Comment = "";
    }

    public class BagNormalRow
    {
        public int Cat;
        public int Id;
        public int MinLvl;
        public int MaxLvl;
        public int Skill;
        public int Luck;
        public int Opt;
        public int Exc;
        public string Comment = "";
    }

    public class BagAdvRow
    {
        public int Cat;
        public int Id;
        public int Level;
        public int Grade;
        public string LevelRate = "*";
        public string SkillRate = "*";
        public string LuckRate = "*";
        public string OptRate = "*";
        public string ExcRate = "*";
        public string Comment = "";
    }

    public class BagFile
    {
        public string FilePath;
        public string FileName;
        public string EventName = "EventName";
        public int DropZen = 1000;
        public int ItemDropRate = 100;
        public int ItemDropCount = 1;
        public int ItemDropType = 0;
        public int Fireworks = 0;
        public List<BagNormalRow> NormalItems = new List<BagNormalRow>();
        public List<BagAdvRow> AdvItems = new List<BagAdvRow>();
    }

    public class OptRateRow
    {
        public int Section;
        public int Index;
        public List<int> Rates = new List<int>();
        public string Comment = "";
    }

    public static readonly string[] CatNames = new string[]
    {
        "0 - Kiáº¿m (Swords)", "1 - RÃ¬u (Axes)", "2 - ChÃ¹y (Maces)", "3 - ThÆ°Æ¡ng (Spears)",
        "4 - Cung/Ná» (Bows)", "5 - Gáº­y (Staffs)", "6 - KhiÃªn (Shields)", "7 - MÅ© (Helms)",
        "8 - Ão (Armors)", "9 - Quáº§n (Pants)", "10 - GÄƒng (Gloves)", "11 - GiÃ y (Boots)",
        "12 - CÃ¡nh/Ngá»c (Wings/Orbs)", "13 - Phá»¥ kiá»‡n (Pets/Rings)", "14 - Ngá»c/Box (Jewels/Potions)", "15 - SÃ¡ch phÃ©p (Scrolls)"
    };

    static string StripComment(string line, out string comment)
    {
        comment = "";
        bool inQuotes = false;
        for (int i = 0; i < line.Length - 1; i++)
        {
            if (line[i] == '"') inQuotes = !inQuotes;
            if (!inQuotes && line[i] == '/' && line[i + 1] == '/')
            {
                comment = line.Substring(i + 2).Trim();
                return line.Substring(0, i).Trim();
            }
        }
        return line.Trim();
    }

    static List<string> Tokenize(string line)
    {
        var list = new List<string>();
        int i = 0, n = line.Length;
        while (i < n)
        {
            while (i < n && char.IsWhiteSpace(line[i])) i++;
            if (i >= n) break;
            if (line[i] == '"')
            {
                i++;
                int s = i;
                while (i < n && line[i] != '"') i++;
                list.Add(line.Substring(s, i - s));
                if (i < n) i++;
            }
            else
            {
                int s = i;
                while (i < n && !char.IsWhiteSpace(line[i])) i++;
                list.Add(line.Substring(s, i - s));
            }
        }
        return list;
    }

    static int ToInt(string s, int def)
    {
        int v;
        return int.TryParse(s, out v) ? v : def;
    }

    public static bool ParseItemKey(string token, out int cat, out int id)
    {
        cat = 0; id = 0;
        if (string.IsNullOrEmpty(token)) return false;
        string[] parts = token.Split(',');
        if (parts.Length == 2 && int.TryParse(parts[0], out cat) && int.TryParse(parts[1], out id)) return true;
        int raw;
        if (int.TryParse(token, out raw)) { cat = raw / 32; id = raw % 32; return true; }
        return false;
    }

    // ---- Catalogs ----
    public static List<ItemDef> LoadItems(string baseDir)
    {
        var list = new List<ItemDef>();
        string path = Path.Combine(baseDir, @"Data\Item\Item.txt");
        if (!File.Exists(path)) return list;
        int curCat = -1;
        foreach (string raw in File.ReadAllLines(path))
        {
            string cmt;
            string line = StripComment(raw, out cmt);
            if (string.IsNullOrEmpty(line)) continue;
            if (line.Equals("end", StringComparison.OrdinalIgnoreCase)) { curCat = -1; continue; }
            var tok = Tokenize(line);
            if (curCat < 0)
            {
                int c;
                if (tok.Count == 1 && int.TryParse(tok[0], out c)) curCat = c;
                continue;
            }
            if (tok.Count >= 9)
            {
                int id;
                if (int.TryParse(tok[0], out id))
                {
                    list.Add(new ItemDef { Cat = curCat, Id = id, Name = tok[8] });
                }
            }
        }
        return list;
    }

    public static List<MonsterDef> LoadMonsters(string baseDir)
    {
        var list = new List<MonsterDef>();
        string path = Path.Combine(baseDir, @"Data\Monster\Monster.txt");
        if (!File.Exists(path)) return list;
        foreach (string raw in File.ReadAllLines(path))
        {
            string cmt;
            string line = StripComment(raw, out cmt);
            if (string.IsNullOrEmpty(line) || line.Equals("end", StringComparison.OrdinalIgnoreCase)) continue;
            var tok = Tokenize(line);
            int id;
            if (tok.Count >= 4 && int.TryParse(tok[0], out id))
            {
                list.Add(new MonsterDef { Id = id, Name = tok[2], Level = ToInt(tok[3], 1) });
            }
        }
        return list;
    }

    // ---- 1. Common.dat ----
    public static CommonDropCfg LoadCommon(string baseDir)
    {
        var cfg = new CommonDropCfg();
        string path = Path.Combine(baseDir, @"GameServer\DATA\GameServerInfo - Common.dat");
        if (!File.Exists(path)) return cfg;
        foreach (string raw in File.ReadAllLines(path))
        {
            string s = raw.Trim();
            if (s.Length == 0 || s.StartsWith(";") || s.StartsWith("[")) continue;
            int eq = s.IndexOf('=');
            if (eq <= 0) continue;
            string k = s.Substring(0, eq).Trim();
            string v = s.Substring(eq + 1).Trim();
            switch (k)
            {
                case "ItemDropTime": cfg.ItemDropTime = ToInt(v, cfg.ItemDropTime); break;
                case "ItemDropRate_AL0": cfg.ItemDropRate[0] = ToInt(v, 100); break;
                case "ItemDropRate_AL1": cfg.ItemDropRate[1] = ToInt(v, 100); break;
                case "ItemDropRate_AL2": cfg.ItemDropRate[2] = ToInt(v, 100); break;
                case "ItemDropRate_AL3": cfg.ItemDropRate[3] = ToInt(v, 100); break;
                case "MoneyDropTime": cfg.MoneyDropTime = ToInt(v, cfg.MoneyDropTime); break;
                case "MoneyAmountDropRate_AL0": cfg.MoneyAmountDropRate[0] = ToInt(v, 50); break;
                case "MoneyAmountDropRate_AL1": cfg.MoneyAmountDropRate[1] = ToInt(v, 50); break;
                case "MoneyAmountDropRate_AL2": cfg.MoneyAmountDropRate[2] = ToInt(v, 50); break;
                case "MoneyAmountDropRate_AL3": cfg.MoneyAmountDropRate[3] = ToInt(v, 50); break;
                case "AddExperienceRate_AL0": cfg.AddExperienceRate[0] = ToInt(v, 1); break;
                case "AddExperienceRate_AL1": cfg.AddExperienceRate[1] = ToInt(v, 1); break;
                case "AddExperienceRate_AL2": cfg.AddExperienceRate[2] = ToInt(v, 1); break;
                case "AddExperienceRate_AL3": cfg.AddExperienceRate[3] = ToInt(v, 1); break;
                case "AddEventExperienceRate_AL0": cfg.AddEventExperienceRate[0] = ToInt(v, 2); break;
                case "AddEventExperienceRate_AL1": cfg.AddEventExperienceRate[1] = ToInt(v, 2); break;
                case "AddEventExperienceRate_AL2": cfg.AddEventExperienceRate[2] = ToInt(v, 2); break;
                case "AddEventExperienceRate_AL3": cfg.AddEventExperienceRate[3] = ToInt(v, 2); break;
            }
        }
        return cfg;
    }

    public static void SaveCommon(string baseDir, CommonDropCfg cfg)
    {
        string path = Path.Combine(baseDir, @"GameServer\DATA\GameServerInfo - Common.dat");
        if (!File.Exists(path)) return;
        var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            { "ItemDropTime", cfg.ItemDropTime.ToString() },
            { "ItemDropRate_AL0", cfg.ItemDropRate[0].ToString() },
            { "ItemDropRate_AL1", cfg.ItemDropRate[1].ToString() },
            { "ItemDropRate_AL2", cfg.ItemDropRate[2].ToString() },
            { "ItemDropRate_AL3", cfg.ItemDropRate[3].ToString() },
            { "MoneyDropTime", cfg.MoneyDropTime.ToString() },
            { "MoneyAmountDropRate_AL0", cfg.MoneyAmountDropRate[0].ToString() },
            { "MoneyAmountDropRate_AL1", cfg.MoneyAmountDropRate[1].ToString() },
            { "MoneyAmountDropRate_AL2", cfg.MoneyAmountDropRate[2].ToString() },
            { "MoneyAmountDropRate_AL3", cfg.MoneyAmountDropRate[3].ToString() },
            { "AddExperienceRate_AL0", cfg.AddExperienceRate[0].ToString() },
            { "AddExperienceRate_AL1", cfg.AddExperienceRate[1].ToString() },
            { "AddExperienceRate_AL2", cfg.AddExperienceRate[2].ToString() },
            { "AddExperienceRate_AL3", cfg.AddExperienceRate[3].ToString() },
            { "AddEventExperienceRate_AL0", cfg.AddEventExperienceRate[0].ToString() },
            { "AddEventExperienceRate_AL1", cfg.AddEventExperienceRate[1].ToString() },
            { "AddEventExperienceRate_AL2", cfg.AddEventExperienceRate[2].ToString() },
            { "AddEventExperienceRate_AL3", cfg.AddEventExperienceRate[3].ToString() }
        };
        string[] lines = File.ReadAllLines(path);
        for (int i = 0; i < lines.Length; i++)
        {
            string s = lines[i].Trim();
            if (s.Length == 0 || s.StartsWith(";") || s.StartsWith("[")) continue;
            int eq = s.IndexOf('=');
            if (eq <= 0) continue;
            string k = s.Substring(0, eq).Trim();
            string nv;
            if (map.TryGetValue(k, out nv))
            {
                lines[i] = k + "=" + nv;
            }
        }
        File.WriteAllLines(path, lines);
    }

    // ---- 2. MapManager.txt ----
    public static List<MapRow> LoadMaps(string baseDir)
    {
        var list = new List<MapRow>();
        string path = Path.Combine(baseDir, @"Data\MapManager.txt");
        if (!File.Exists(path)) return list;
        foreach (string raw in File.ReadAllLines(path))
        {
            string cmt;
            string line = StripComment(raw, out cmt);
            if (string.IsNullOrEmpty(line) || line.Equals("end", StringComparison.OrdinalIgnoreCase)) continue;
            var tok = Tokenize(line);
            int idx;
            if (tok.Count >= 10 && int.TryParse(tok[0], out idx))
            {
                list.Add(new MapRow
                {
                    Index = idx,
                    NonPK = tok[1],
                    View = ToInt(tok[2], 15),
                    ExpRate = ToInt(tok[3], 100),
                    ItemDropRate = ToInt(tok[4], 100),
                    ExcDropRate = ToInt(tok[5], 0),
                    DeadGate = ToInt(tok[6], 17),
                    SpawnInPlace = ToInt(tok[7], 0),
                    FlyingDragons = ToInt(tok[8], 1),
                    Name = tok[9]
                });
            }
        }
        return list;
    }

    public static void SaveMaps(string baseDir, List<MapRow> rows)
    {
        string path = Path.Combine(baseDir, @"Data\MapManager.txt");
        var sb = new StringBuilder();
        sb.AppendLine("//Index\tNonPK\tView\tExperienceRate\tItemDropRate\tExcDropRate\tDeadGate\tSpawnInPlace\tFlyingDragons\tName");
        foreach (var r in rows)
        {
            sb.AppendFormat("{0}\t{1}\t{2}\t{3}\t\t{4}\t\t{5}\t\t{6}\t\t{7}\t\t{8}\t\t\"{9}\"\r\n",
                r.Index, r.NonPK, r.View, r.ExpRate, r.ItemDropRate, r.ExcDropRate, r.DeadGate, r.SpawnInPlace, r.FlyingDragons, r.Name);
        }
        sb.Append("end");
        File.WriteAllText(path, sb.ToString());
    }

    // ---- 3. ItemDrop.txt ----
    public static List<ItemDropRow> LoadItemDrops(string baseDir)
    {
        var list = new List<ItemDropRow>();
        string path = Path.Combine(baseDir, @"Data\Item\ItemDrop.txt");
        if (!File.Exists(path)) return list;
        foreach (string raw in File.ReadAllLines(path))
        {
            string cmt;
            string line = StripComment(raw, out cmt);
            if (string.IsNullOrEmpty(line) || line.Equals("end", StringComparison.OrdinalIgnoreCase)) continue;
            var tok = Tokenize(line);
            int cat, id;
            if (tok.Count >= 13 && ParseItemKey(tok[0], out cat, out id))
            {
                list.Add(new ItemDropRow
                {
                    Cat = cat,
                    Id = id,
                    Level = ToInt(tok[1], 0),
                    Grade = ToInt(tok[2], 0),
                    LevelRate = tok[3],
                    SkillRate = tok[4],
                    LuckRate = tok[5],
                    OptRate = tok[6],
                    ExcRate = tok[7],
                    MapNum = tok[8],
                    Monster = tok[9],
                    MinLvl = ToInt(tok[10], 1),
                    MaxLvl = ToInt(tok[11], 400),
                    Rate = ToInt(tok[12], 1000),
                    Comment = cmt
                });
            }
        }
        return list;
    }

    public static void SaveItemDrops(string baseDir, List<ItemDropRow> rows)
    {
        string path = Path.Combine(baseDir, @"Data\Item\ItemDrop.txt");
        var sb = new StringBuilder();
        sb.AppendLine("// ItemGrade works when ExcellentRate = 0");
        sb.AppendLine("// Level, Skill, Luck, Option and Exe");
        sb.AppendLine("// Uses ItemOptionRate Indexes");
        sb.AppendLine();
        sb.AppendLine("//Index\tLevel\tGrade\tLevel\tSkill\tLuck\tOption\tExe\tMapNum\tMonster\tMonsterLvlMin\tMonsterLvlMax\tRate\t\tComment");
        foreach (var r in rows)
        {
            sb.AppendFormat("{0:00},{1:000}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t{8}\t{9}\t{10}\t{11}\t\t{12}\t\t{13}\t\t//{14}\r\n",
                r.Cat, r.Id, r.Level, r.Grade, r.LevelRate, r.SkillRate, r.LuckRate, r.OptRate, r.ExcRate,
                r.MapNum, r.Monster, r.MinLvl, r.MaxLvl, r.Rate, r.Comment);
        }
        sb.Append("end");
        File.WriteAllText(path, sb.ToString());
    }

    // ---- 4. EventItemBag/*.txt ----
    public static List<BagFile> LoadAllBags(string baseDir)
    {
        var list = new List<BagFile>();
        string dir = Path.Combine(baseDir, @"Data\EventItemBag");
        if (!Directory.Exists(dir)) return list;
        string[] files = Directory.GetFiles(dir, "*.txt");
        Array.Sort(files, StringComparer.OrdinalIgnoreCase);
        foreach (string f in files)
        {
            list.Add(LoadBag(f));
        }
        return list;
    }

    public static BagFile LoadBag(string path)
    {
        var bag = new BagFile { FilePath = path, FileName = Path.GetFileName(path) };
        if (!File.Exists(path)) return bag;
        int section = -1;
        foreach (string raw in File.ReadAllLines(path))
        {
            string cmt;
            string line = StripComment(raw, out cmt);
            if (string.IsNullOrEmpty(line)) continue;
            if (line.Equals("end", StringComparison.OrdinalIgnoreCase)) { section = -1; continue; }
            var tok = Tokenize(line);
            if (section < 0)
            {
                int sec;
                if (tok.Count == 1 && int.TryParse(tok[0], out sec)) section = sec;
                continue;
            }
            if (section == 0 && tok.Count >= 6)
            {
                bag.EventName = tok[0];
                bag.DropZen = ToInt(tok[1], 1000);
                bag.ItemDropRate = ToInt(tok[2], 100);
                bag.ItemDropCount = ToInt(tok[3], 1);
                bag.ItemDropType = ToInt(tok[4], 0);
                bag.Fireworks = ToInt(tok[5], 0);
            }
            else if (section == 1 && tok.Count >= 7)
            {
                int cat, id;
                if (ParseItemKey(tok[0], out cat, out id))
                {
                    bag.NormalItems.Add(new BagNormalRow
                    {
                        Cat = cat,
                        Id = id,
                        MinLvl = ToInt(tok[1], 0),
                        MaxLvl = ToInt(tok[2], 0),
                        Skill = ToInt(tok[3], 0),
                        Luck = ToInt(tok[4], 0),
                        Opt = ToInt(tok[5], 0),
                        Exc = ToInt(tok[6], 0),
                        Comment = cmt
                    });
                }
            }
            else if (section == 4 && tok.Count >= 8)
            {
                int cat, id;
                if (ParseItemKey(tok[0], out cat, out id))
                {
                    bag.AdvItems.Add(new BagAdvRow
                    {
                        Cat = cat,
                        Id = id,
                        Level = ToInt(tok[1], 0),
                        Grade = ToInt(tok[2], 0),
                        LevelRate = tok[3],
                        SkillRate = tok[4],
                        LuckRate = tok[5],
                        OptRate = tok[6],
                        ExcRate = tok[7],
                        Comment = cmt
                    });
                }
            }
        }
        return bag;
    }

    public static void SaveBag(BagFile bag)
    {
        var sb = new StringBuilder();
        sb.AppendLine("//======================================");
        sb.AppendLine("// NORMAL DROP");
        sb.AppendLine("//======================================");
        sb.AppendLine();
        sb.AppendLine("0");
        sb.AppendLine("//EventName\t\tDropZen\tItemDropRate\tItemDropCount\tItemDropType\tFireworks");
        sb.AppendFormat("\"{0}\"\t\t{1}\t{2}\t\t{3}\t\t{4}\t\t{5}\r\n",
            bag.EventName, bag.DropZen, bag.ItemDropRate, bag.ItemDropCount, bag.ItemDropType, bag.Fireworks);
        sb.AppendLine("end");
        sb.AppendLine();
        sb.AppendLine("1");
        sb.AppendLine("//Item\tMinLvl\tMaxLvl\tSkill\tLuck\tOpt\tExce\tComment");
        foreach (var it in bag.NormalItems)
        {
            sb.AppendFormat("{0:00},{1:000}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t//{8}\r\n",
                it.Cat, it.Id, it.MinLvl, it.MaxLvl, it.Skill, it.Luck, it.Opt, it.Exc, it.Comment);
        }
        sb.AppendLine("end");
        sb.AppendLine();
        sb.AppendLine("//======================================");
        sb.AppendLine("// ADVANCED DROP");
        sb.AppendLine("//======================================");
        sb.AppendLine();
        sb.AppendLine("2");
        sb.AppendLine("//Index\tDropRate");
        sb.AppendLine("0\t10000");
        sb.AppendLine("end");
        sb.AppendLine();
        sb.AppendLine("3");
        sb.AppendLine("//Index\tSection\tSectionRate\tMoneyAmount\tOptionValue\tDW\tDK\tFE\tMG");
        if (bag.AdvItems.Count > 0)
        {
            sb.AppendLine("0\t4\t10000\t\t0\t\t0\t\t1\t1\t1\t1");
        }
        else
        {
            sb.AppendLine("//0\t4\t10000\t\t0\t\t0\t\t1\t1\t1\t1");
        }
        sb.AppendLine("end");
        sb.AppendLine();
        sb.AppendLine("// ItemGrade works when ExcellentRate = 0");
        sb.AppendLine("// LevelRate, SkillRate, LuckRate, OptionRate and ExcellentRate");
        sb.AppendLine("// Uses ItemOptionRate Indexes");
        sb.AppendLine("4");
        sb.AppendLine("//ItemIndex\tItemLevel\tItemGrade\tLevelRate\tSkillRate\tLuckRate\tOptionRate\tExcellentRate\tComment");
        foreach (var it in bag.AdvItems)
        {
            sb.AppendFormat("{0:00},{1:000}\t\t{2}\t\t{3}\t\t{4}\t\t{5}\t\t{6}\t\t{7}\t\t{8}\t\t//{9}\r\n",
                it.Cat, it.Id, it.Level, it.Grade, it.LevelRate, it.SkillRate, it.LuckRate, it.OptRate, it.ExcRate, it.Comment);
        }
        sb.Append("end");
        File.WriteAllText(bag.FilePath, sb.ToString());
    }

    // ---- 5. ItemOptionRate.txt ----
    public static List<OptRateRow> LoadOptionRates(string baseDir)
    {
        var list = new List<OptRateRow>();
        string path = Path.Combine(baseDir, @"Data\Item\ItemOptionRate.txt");
        if (!File.Exists(path)) return list;
        int section = -1;
        foreach (string raw in File.ReadAllLines(path))
        {
            string cmt;
            string line = StripComment(raw, out cmt);
            if (string.IsNullOrEmpty(line)) continue;
            if (line.Equals("end", StringComparison.OrdinalIgnoreCase)) { section = -1; continue; }
            var tok = Tokenize(line);
            if (section < 0)
            {
                int sec;
                if (tok.Count == 1 && int.TryParse(tok[0], out sec)) section = sec;
                continue;
            }
            int idx;
            if (tok.Count >= 3 && int.TryParse(tok[0], out idx))
            {
                var row = new OptRateRow { Section = section, Index = idx, Comment = cmt };
                for (int i = 1; i < tok.Count; i++) row.Rates.Add(ToInt(tok[i], 0));
                list.Add(row);
            }
        }
        return list;
    }

    public static void SaveOptionRates(string baseDir, List<OptRateRow> rows)
    {
        string path = Path.Combine(baseDir, @"Data\Item\ItemOptionRate.txt");
        var sb = new StringBuilder();
        string[] headers = new string[]
        {
            "//Index\tRate0\tRate1\tRate2\tRate3\tRate4\tRate5\tRate6\tRate7\tRate8\tRate9\tRate10\tRate11\tComment",
            "//Index\tRate0\tRate1\tComment",
            "//Index\tRate0\tRate1\tComment",
            "//Index\tRate0\tRate1\tRate2\tRate3\tRate4\tRate5\tRate6\tRate7\tComment",
            "//Index\tRate0\tRate1\tRate2\tRate3\tRate4\tRate5\tRate6\tComment"
        };
        for (int sec = 0; sec <= 4; sec++)
        {
            sb.AppendLine(sec.ToString());
            sb.AppendLine(headers[sec]);
            foreach (var r in rows)
            {
                if (r.Section != sec) continue;
                sb.Append(r.Index);
                for (int i = 0; i < r.Rates.Count; i++) sb.Append("\t" + r.Rates[i]);
                sb.AppendLine("\t//" + r.Comment);
            }
            sb.AppendLine("end");
            if (sec < 4) sb.AppendLine();
        }
        File.WriteAllText(path, sb.ToString());
    }
}


