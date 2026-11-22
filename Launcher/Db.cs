using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

// Thin wrapper around the MariaDB command line tools (no extra .NET libraries needed).
// Credentials come from mariadb\mu.cnf (user muadmin) next to the servers; they never appear on a command line.
static class Db
{
    public static string BinDir = @"C:\Program Files\MariaDB 13.0\bin";
    public static string Base = AppDomain.CurrentDomain.BaseDirectory;   // ...\MuServer\
    public const string Database = "MuOnline97";

    public static string UserCnf { get { return Path.Combine(Base, @"mariadb\mu.cnf"); } }
    public static string RootCnf { get { return Path.Combine(Base, @"mariadb\root.cnf"); } }
    public static string BackupDir { get { return Path.GetFullPath(Path.Combine(Base, @"..\Backups")); } }

    public static bool Available { get { return File.Exists(UserCnf) && File.Exists(Path.Combine(BinDir, "mariadb.exe")); } }

    public static int Run(string exe, string args, out string stdout, out string stderr, int timeoutMs)
    {
        var psi = new ProcessStartInfo(Path.Combine(BinDir, exe), args);
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        psi.RedirectStandardOutput = true;
        psi.RedirectStandardError = true;
        psi.StandardOutputEncoding = Encoding.UTF8;
        psi.StandardErrorEncoding = Encoding.UTF8;
        using (var p = Process.Start(psi))
        {
            var errSb = new StringBuilder();
            p.ErrorDataReceived += delegate (object s, DataReceivedEventArgs e) { if (e.Data != null) errSb.AppendLine(e.Data); };
            p.BeginErrorReadLine();
            string o = p.StandardOutput.ReadToEnd();
            if (!p.WaitForExit(timeoutMs)) { try { p.Kill(); } catch { } stdout = o; stderr = "timeout"; return -1; }
            p.WaitForExit();
            stdout = o;
            stderr = errSb.ToString();
            return p.ExitCode;
        }
    }

    // Keep only a real error from stderr (the client prints a warning about passwordless logins etc.).
    static string CleanErr(string err)
    {
        var sb = new StringBuilder();
        foreach (string line in (err ?? "").Split('\n'))
        {
            string l = line.Trim();
            if (l.Length == 0 || l.StartsWith("WARNING")) continue;
            sb.AppendLine(l);
        }
        return sb.ToString().Trim();
    }

    // Run a query and return rows (tab separated columns). Use single quotes only inside sql.
    public static List<string[]> Query(string sql, out string err)
    {
        var rows = new List<string[]>();
        err = null;
        if (!Available) { err = "Chưa có mariadb\\mu.cnf"; return rows; }
        string o, e;
        int rc;
        try { rc = Run("mariadb.exe", "--defaults-extra-file=\"" + UserCnf + "\" --batch --skip-column-names -D " + Database + " -e \"" + sql + "\"", out o, out e, 20000); }
        catch (Exception ex) { err = ex.Message; return rows; }
        if (rc != 0) { err = CleanErr(e); if (err.Length == 0) err = "mã lỗi " + rc; return rows; }
        foreach (string line in o.Split('\n'))
        {
            string l = line.TrimEnd('\r');
            if (l.Length == 0) continue;
            rows.Add(l.Split('\t'));
        }
        return rows;
    }

    public static bool Exec(string sql, out string err)
    {
        Query(sql, out err);
        return err == null;
    }

    // Account ids / passwords are limited to 10 chars of [A-Za-z0-9_] by the game, which also keeps SQL safe.
    public static bool IsSafeName(string s) { return !string.IsNullOrEmpty(s) && s.Length <= 10 && Regex.IsMatch(s, "^[A-Za-z0-9_]+$"); }

    public static bool Dump(string file, out string err)
    {
        err = null;
        if (!Available) { err = "Chưa có mariadb\\mu.cnf"; return false; }
        Directory.CreateDirectory(Path.GetDirectoryName(file));
        string o, e;
        int rc = Run("mariadb-dump.exe", "--defaults-extra-file=\"" + UserCnf + "\" --databases " + Database +
            " --routines --triggers --single-transaction --add-drop-database --result-file=\"" + file + "\"", out o, out e, 120000);
        if (rc != 0) { err = CleanErr(e); if (err.Length == 0) err = "mã lỗi " + rc; try { File.Delete(file); } catch { } return false; }
        return File.Exists(file) && new FileInfo(file).Length > 0;
    }

    public static bool Restore(string file, out string err)
    {
        err = null;
        if (!Available) { err = "Chưa có mariadb\\mu.cnf"; return false; }
        // Stored procedures in the dump have DEFINER=root, so the restore must run as root (muadmin is not allowed to create them).
        string cnf = File.Exists(RootCnf) ? RootCnf : UserCnf;
        string o, e;
        int rc = Run("mariadb.exe", "--defaults-extra-file=\"" + cnf + "\" -e \"source " + file.Replace('\\', '/') + "\"", out o, out e, 300000);
        if (rc != 0) { err = CleanErr(e); if (err.Length == 0) err = "mã lỗi " + rc; return false; }

        // verify: every procedure/function in the file must exist afterwards
        int expected = 0;
        foreach (string line in File.ReadAllLines(file))
            if (Regex.IsMatch(line, @"^\s*CREATE\b.*\b(PROCEDURE|FUNCTION)\b")) expected++;
        var sb = new StringBuilder();
        string o2, e2;
        Run("mariadb.exe", "--defaults-extra-file=\"" + cnf + "\" --batch --skip-column-names -e \"SELECT COUNT(*) FROM information_schema.routines WHERE routine_schema='" + Database + "'\"", out o2, out e2, 20000);
        int actual;
        if (!int.TryParse(o2.Trim(), out actual) || actual < expected)
        {
            err = "Khôi phục không đầy đủ: file có " + expected + " stored procedure nhưng database chỉ có " + o2.Trim() + ".";
            return false;
        }
        return true;
    }
}
