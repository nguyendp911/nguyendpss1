using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

// "Nhật ký" tab: read the log files of the four servers (and the launcher) with filter / errors-only / live tail.
class LogPanel : Panel, IShown
{
    class Src
    {
        public string Title;
        public string Dir;     // folder holding yyyy-MM-dd.txt files, or null for a single file
        public string File;    // single file (launcher log)
        public override string ToString() { return Title; }
    }

    static readonly Regex ErrRx = new Regex("error|fail|exception|could not|cannot|invalid|disconnect|!!|crash|denied", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    readonly string baseDir = AppDomain.CurrentDomain.BaseDirectory;
    readonly ComboBox cbSrc = K.Cb(260), cbFile = K.Cb(150);
    readonly TextBox tbFilter = K.Tb(200);
    readonly CheckBox ckErr = K.Ck("Chỉ dòng lỗi", false), ckAuto = K.Ck("Tự cập nhật", true);
    readonly RichTextBox rt = new RichTextBox();
    readonly Label lblInfo = K.Lbl("", true);
    readonly Timer tm = new Timer { Interval = 2000 };
    string lastPath;
    DateTime lastWrite = DateTime.MinValue;
    long lastLen = -1;

    public LogPanel()
    {
        BackColor = Theme.Panel;
        var top = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 46, BackColor = Theme.Panel, Padding = new Padding(14, 10, 14, 0), WrapContents = false };
        top.Controls.Add(Wrap(K.Lbl("Nguồn", true), cbSrc));
        top.Controls.Add(Wrap(K.Lbl("Ngày", true), cbFile));
        top.Controls.Add(Wrap(K.Lbl("Lọc", true), tbFilter));
        ckErr.Margin = new Padding(10, 4, 0, 0);
        ckAuto.Margin = new Padding(10, 4, 0, 0);
        top.Controls.Add(ckErr);
        top.Controls.Add(ckAuto);
        lblInfo.Margin = new Padding(14, 6, 0, 0);
        top.Controls.Add(lblInfo);

        rt.Dock = DockStyle.Fill;
        rt.ReadOnly = true;
        rt.BorderStyle = BorderStyle.None;
        rt.BackColor = Color.Black;
        rt.ForeColor = Color.Gainsboro;
        rt.Font = K.Mono;
        rt.WordWrap = false;
        rt.DetectUrls = false;

        var host = new Panel { Dock = DockStyle.Fill, Padding = new Padding(14, 0, 14, 12), BackColor = Theme.Panel };
        host.Controls.Add(rt);
        Controls.Add(host);
        Controls.Add(top);

        cbSrc.SelectedIndexChanged += delegate { LoadFiles(); };
        cbFile.SelectedIndexChanged += delegate { Reload(true); };
        tbFilter.TextChanged += delegate { Reload(true); };
        ckErr.CheckedChanged += delegate { Reload(true); };
        tm.Tick += delegate { if (Visible && ckAuto.Checked) Reload(false); };
        tm.Start();
        LoadSources();
    }

    static Control Wrap(Control label, Control c)
    {
        var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, BackColor = Theme.Panel, Margin = new Padding(0, 0, 12, 0) };
        label.Margin = new Padding(0, 6, 6, 0);
        f.Controls.Add(label);
        f.Controls.Add(c);
        return f;
    }

    public void OnShown() { LoadSources(); Reload(true); }

    void LoadSources()
    {
        object keep = cbSrc.SelectedItem;
        string keepTitle = keep != null ? keep.ToString() : null;
        cbSrc.Items.Clear();
        cbSrc.Items.Add(new Src { Title = "MuLauncher", File = Path.Combine(baseDir, "MuLauncher.log") });
        foreach (string srv in new[] { "ConnectServer", "JoinServer", "DataServer", "GameServer" })
        {
            string sd = Path.Combine(baseDir, srv);
            if (!Directory.Exists(sd)) continue;
            var dirs = new List<string>(Directory.GetDirectories(sd));
            dirs.Sort(StringComparer.OrdinalIgnoreCase);
            foreach (string d in dirs)
            {
                string n = Path.GetFileName(d);
                if (Regex.IsMatch(n, "^(LOG.*|.*_LOG)$", RegexOptions.IgnoreCase))
                    cbSrc.Items.Add(new Src { Title = srv + " / " + n, Dir = d });
            }
        }
        int sel = 0;
        for (int i = 0; i < cbSrc.Items.Count; i++) if (cbSrc.Items[i].ToString() == keepTitle) sel = i;
        // default to GameServer / LOG on first load
        if (keepTitle == null) for (int i = 0; i < cbSrc.Items.Count; i++) if (cbSrc.Items[i].ToString() == "GameServer / LOG") sel = i;
        if (cbSrc.Items.Count > 0) cbSrc.SelectedIndex = sel;
    }

    void LoadFiles()
    {
        var s = cbSrc.SelectedItem as Src;
        cbFile.Items.Clear();
        if (s == null) return;
        if (s.File != null) { cbFile.Items.Add(Path.GetFileName(s.File)); cbFile.SelectedIndex = 0; return; }
        try
        {
            var files = new List<string>(Directory.GetFiles(s.Dir, "*.txt"));
            files.Sort(StringComparer.OrdinalIgnoreCase);
            files.Reverse();
            foreach (string f in files) cbFile.Items.Add(Path.GetFileName(f));
        }
        catch { }
        if (cbFile.Items.Count > 0) cbFile.SelectedIndex = 0;
        else { rt.Text = ""; lblInfo.Text = "Chưa có file log"; }
    }

    string CurrentPath()
    {
        var s = cbSrc.SelectedItem as Src;
        if (s == null || cbFile.SelectedItem == null) return null;
        return s.File != null ? s.File : Path.Combine(s.Dir, cbFile.SelectedItem.ToString());
    }

    static string ReadTail(string path, int maxBytes)
    {
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
        {
            long start = Math.Max(0, fs.Length - maxBytes);
            fs.Seek(start, SeekOrigin.Begin);
            using (var sr = new StreamReader(fs, Encoding.UTF8, true))
            {
                string t = sr.ReadToEnd();
                if (start > 0) { int nl = t.IndexOf('\n'); if (nl >= 0) t = t.Substring(nl + 1); }
                return t;
            }
        }
    }

    void Reload(bool force)
    {
        string path = CurrentPath();
        if (path == null || !File.Exists(path)) { if (force) { rt.Text = ""; lblInfo.Text = path == null ? "" : "File không tồn tại"; } return; }
        var fi = new FileInfo(path);
        if (!force && path == lastPath && fi.LastWriteTime == lastWrite && fi.Length == lastLen) return;
        lastPath = path; lastWrite = fi.LastWriteTime; lastLen = fi.Length;

        string text;
        try { text = ReadTail(path, 400 * 1024); } catch (Exception e) { lblInfo.Text = e.Message; return; }
        string[] lines = text.Replace("\r", "").Split('\n');
        string flt = tbFilter.Text.Trim();
        bool onlyErr = ckErr.Checked;
        var shown = new List<string>();
        foreach (string l in lines)
        {
            if (l.Length == 0) continue;
            if (onlyErr && !ErrRx.IsMatch(l)) continue;
            if (flt.Length > 0 && l.IndexOf(flt, StringComparison.OrdinalIgnoreCase) < 0) continue;
            shown.Add(l);
        }
        if (shown.Count > 1500) shown.RemoveRange(0, shown.Count - 1500);

        rt.SuspendLayout();
        rt.Text = string.Join("\n", shown.ToArray());
        int pos = 0;
        foreach (string l in shown)
        {
            if (ErrRx.IsMatch(l)) { rt.Select(pos, l.Length); rt.SelectionColor = Color.FromArgb(255, 110, 110); }
            pos += l.Length + 1;
        }
        rt.SelectionLength = 0;
        if (ckAuto.Checked) { rt.SelectionStart = rt.TextLength; rt.ScrollToCaret(); }
        rt.ResumeLayout();
        lblInfo.Text = shown.Count + " dòng · " + (fi.Length / 1024) + " KB";
    }
}
