using System;
using System.Drawing;
using System.Windows.Forms;

// Small helpers to build dark themed controls consistently.
static class K
{
    public static Font Mono = new Font("Consolas", 9.5f);

    public static Label Lbl(string text, bool muted)
    {
        return new Label { Text = text, AutoSize = true, ForeColor = muted ? Theme.Muted : Theme.Text, BackColor = Color.Transparent };
    }

    public static TextBox Tb(int width)
    {
        return new TextBox { Width = width, BackColor = Theme.PanelHi, ForeColor = Theme.Text, BorderStyle = BorderStyle.FixedSingle };
    }

    public static ComboBox Cb(int width)
    {
        return new ComboBox { Width = width, BackColor = Theme.PanelHi, ForeColor = Theme.Text, FlatStyle = FlatStyle.Flat, DropDownStyle = ComboBoxStyle.DropDownList };
    }

    public static CheckBox Ck(string text, bool on)
    {
        return new CheckBox { Text = text, Checked = on, AutoSize = true, ForeColor = Theme.Text, BackColor = Color.Transparent, FlatStyle = FlatStyle.Standard };
    }

    public static NumericUpDown Num(int min, int max, int val, int width)
    {
        return new NumericUpDown { Minimum = min, Maximum = max, Value = val, Width = width, BackColor = Theme.PanelHi, ForeColor = Theme.Text, BorderStyle = BorderStyle.FixedSingle };
    }

    // A titled section ("card") that stacks its children vertically.
    public static Panel Card(string title, out FlowLayoutPanel body)
    {
        var card = new Panel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = Theme.Panel, Padding = new Padding(0, 0, 0, 14) };
        var inner = new Panel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = Theme.PanelHi, Padding = new Padding(16, 12, 16, 14) };
        var head = new Label { Text = title, Dock = DockStyle.Top, Height = 30, ForeColor = Theme.Text, Font = new Font("Segoe UI Semibold", 10.5f), BackColor = Theme.PanelHi, TextAlign = ContentAlignment.MiddleLeft };
        body = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, FlowDirection = FlowDirection.TopDown, WrapContents = false, BackColor = Theme.PanelHi };
        inner.Controls.Add(body);
        inner.Controls.Add(head);
        card.Controls.Add(inner);
        return card;
    }

    public static FlowLayoutPanel Row(FlowLayoutPanel parent)
    {
        var r = new FlowLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, WrapContents = false, BackColor = Theme.PanelHi, Margin = new Padding(0, 4, 0, 4) };
        parent.Controls.Add(r);
        return r;
    }

    public static void StyleGrid(DataGridView g)
    {
        g.BackgroundColor = Theme.Panel;
        g.BorderStyle = BorderStyle.None;
        g.EnableHeadersVisualStyles = false;
        g.ColumnHeadersDefaultCellStyle.BackColor = Theme.PanelHi;
        g.ColumnHeadersDefaultCellStyle.ForeColor = Theme.Muted;
        g.ColumnHeadersDefaultCellStyle.SelectionBackColor = Theme.PanelHi;
        g.ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None;
        g.ColumnHeadersHeight = 32;
        g.DefaultCellStyle.BackColor = Theme.Panel;
        g.DefaultCellStyle.ForeColor = Theme.Text;
        g.DefaultCellStyle.SelectionBackColor = Theme.Blue;
        g.DefaultCellStyle.SelectionForeColor = Color.White;
        g.GridColor = Theme.Border;
        g.CellBorderStyle = DataGridViewCellBorderStyle.SingleHorizontal;
        g.RowHeadersVisible = false;
        g.AllowUserToAddRows = false;
        g.AllowUserToDeleteRows = false;
        g.AllowUserToResizeRows = false;
        g.ReadOnly = true;
        g.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
        g.MultiSelect = false;
        g.RowTemplate.Height = 28;
        g.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
    }
}
