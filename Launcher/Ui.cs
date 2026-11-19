using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Windows.Forms;

static class Theme
{
    public static readonly Color Bg = ColorTranslator.FromHtml("#13151B");
    public static readonly Color Panel = ColorTranslator.FromHtml("#1B1E27");
    public static readonly Color PanelHi = ColorTranslator.FromHtml("#262A36");
    public static readonly Color Border = ColorTranslator.FromHtml("#2D3240");
    public static readonly Color Text = ColorTranslator.FromHtml("#E8EAF0");
    public static readonly Color Muted = ColorTranslator.FromHtml("#8E95A8");
    public static readonly Color Green = ColorTranslator.FromHtml("#2FBF71");
    public static readonly Color Red = ColorTranslator.FromHtml("#E5575C");
    public static readonly Color Blue = ColorTranslator.FromHtml("#3E7BFA");
    public static readonly Color Amber = ColorTranslator.FromHtml("#E0A030");

    public static GraphicsPath Round(Rectangle r, int rad)
    {
        var p = new GraphicsPath();
        int d = rad * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    public static Color Mix(Color a, Color b, double t)
    {
        return Color.FromArgb((int)(a.R + (b.R - a.R) * t), (int)(a.G + (b.G - a.G) * t), (int)(a.B + (b.B - a.B) * t));
    }
}

// Flat rounded button. Filled (accent colour) or ghost (outline).
class RBtn : Button
{
    readonly Color accent;
    readonly bool ghost;
    bool hover, down;

    public RBtn(string text, Color accent, bool ghost)
    {
        this.accent = accent;
        this.ghost = ghost;
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderSize = 0;
        TabStop = false;
        Cursor = Cursors.Hand;
        Font = new Font("Segoe UI Semibold", 9.5f);
        BackColor = Theme.Panel;
        Height = 36;
        Text = text;
        FitWidth();
    }

    void FitWidth()
    {
        Size sz = TextRenderer.MeasureText(Text, Font);
        Width = sz.Width + 34;
    }

    protected override void OnTextChanged(EventArgs e) { base.OnTextChanged(e); FitWidth(); Invalidate(); }
    protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { hover = false; down = false; Invalidate(); base.OnMouseLeave(e); }
    protected override void OnMouseDown(MouseEventArgs e) { down = true; Invalidate(); base.OnMouseDown(e); }
    protected override void OnMouseUp(MouseEventArgs e) { down = false; Invalidate(); base.OnMouseUp(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(Parent != null ? Parent.BackColor : Theme.Panel);
        var r = new Rectangle(0, 0, Width - 1, Height - 1);
        Color fill, border, fore;
        if (ghost)
        {
            fill = down ? Theme.Mix(Theme.PanelHi, Color.Black, 0.2) : (hover ? Theme.PanelHi : Theme.Panel);
            bool tinted = accent != Theme.Muted;
            border = hover ? (tinted ? accent : Theme.Muted) : Theme.Border;
            fore = (hover && tinted) ? accent : Theme.Text;
        }
        else
        {
            fill = down ? Theme.Mix(accent, Color.Black, 0.25) : (hover ? Theme.Mix(accent, Color.White, 0.15) : accent);
            border = fill;
            fore = Color.White;
        }
        using (var path = Theme.Round(r, 8))
        using (var b = new SolidBrush(fill))
        using (var pen = new Pen(border))
        {
            g.FillPath(b, path);
            g.DrawPath(pen, path);
        }
        TextRenderer.DrawText(g, Text, Font, new Rectangle(0, 0, Width, Height), fore,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
    }
}

// Tab header with a status dot.
class TabBtn : Control
{
    public bool Running;
    public bool Selected;
    public bool NoDot;
    bool hover;

    public TabBtn(string text)
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        Text = text;
        Font = new Font("Segoe UI Semibold", 9.5f);
        Cursor = Cursors.Hand;
        Size = new Size(158, 36);
        Margin = new Padding(0, 0, 4, 0);
    }

    protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(Theme.Bg);
        var body = new Rectangle(0, 0, Width - 1, Height + 8);
        Color fill = Selected ? Theme.Panel : (hover ? Theme.Mix(Theme.Bg, Theme.PanelHi, 0.6) : Theme.Bg);
        using (var path = Theme.Round(body, 8))
        using (var b = new SolidBrush(fill))
            g.FillPath(b, path);
        if (Selected)
            using (var b = new SolidBrush(Theme.Blue))
                g.FillRectangle(b, 12, 0, Width - 25, 2);
        if (!NoDot)
        {
            Color dot = Running ? Theme.Green : Theme.Border;
            if (Running)
                using (var pen = new Pen(Color.FromArgb(70, Theme.Green), 3))
                    g.DrawEllipse(pen, 12, Height / 2 - 7, 14, 14);
            using (var b = new SolidBrush(dot))
                g.FillEllipse(b, 14, Height / 2 - 5, 10, 10);
        }
        TextRenderer.DrawText(g, Text, Font, new Rectangle(NoDot ? 16 : 32, 0, Width - 36, Height),
            Selected ? Theme.Text : Theme.Muted, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
    }
}

class DarkColors : ProfessionalColorTable
{
    public override Color MenuItemSelected { get { return Theme.PanelHi; } }
    public override Color MenuItemSelectedGradientBegin { get { return Theme.PanelHi; } }
    public override Color MenuItemSelectedGradientEnd { get { return Theme.PanelHi; } }
    public override Color MenuItemBorder { get { return Theme.PanelHi; } }
    public override Color MenuItemPressedGradientBegin { get { return Theme.PanelHi; } }
    public override Color MenuItemPressedGradientMiddle { get { return Theme.PanelHi; } }
    public override Color MenuItemPressedGradientEnd { get { return Theme.PanelHi; } }
    public override Color MenuBorder { get { return Theme.Border; } }
    public override Color ToolStripDropDownBackground { get { return Theme.Panel; } }
    public override Color ImageMarginGradientBegin { get { return Theme.Panel; } }
    public override Color ImageMarginGradientMiddle { get { return Theme.Panel; } }
    public override Color ImageMarginGradientEnd { get { return Theme.Panel; } }
    public override Color SeparatorDark { get { return Theme.Border; } }
    public override Color SeparatorLight { get { return Theme.Border; } }
    public override Color MenuStripGradientBegin { get { return Theme.Panel; } }
    public override Color MenuStripGradientEnd { get { return Theme.Panel; } }
    public override Color CheckBackground { get { return Theme.Blue; } }
    public override Color CheckSelectedBackground { get { return Theme.Blue; } }
    public override Color CheckPressedBackground { get { return Theme.Blue; } }
}

class DarkRenderer : ToolStripProfessionalRenderer
{
    public DarkRenderer() : base(new DarkColors()) { RoundedEdges = false; }

    protected override void OnRenderItemText(ToolStripItemTextRenderEventArgs e)
    {
        e.TextColor = e.Item.Enabled ? Theme.Text : Theme.Muted;
        base.OnRenderItemText(e);
    }

    protected override void OnRenderToolStripBorder(ToolStripRenderEventArgs e) { }
}
