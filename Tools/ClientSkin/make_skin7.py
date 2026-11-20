"""Flat 'digital' login skin (matches the approved Digital Login demo).

Rectangles must match g_New in Source\\Client\\Main\\LoginScale.cpp (same layout as make_skin6).
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont
import muimg

SRC = r"C:\Muemu\SkinBackup\Logo"
DST = r"C:\Muemu\Client\Data\Logo"
FONTS = r"C:\Windows\Fonts"
S = 4  # pixels per design unit (4x keeps the original UV mapping)

PANEL = (47, 42, 203, 176)
FIELDS = [(60, 76, 190, 91), (60, 105, 190, 120)]
CONNECT = (60, 147, 121, 162)
EXIT = (129, 147, 190, 162)
PAD = (0.0, 0.0)
FIELD_PAD = (0.0, 0.0)

C_PANEL = (21, 26, 31)
C_FIELD = (12, 16, 20)
C_LINE = (62, 74, 86)
C_LINE_DIM = (48, 58, 68)
C_ACCENT = (111, 182, 196)
C_INK = (228, 233, 238)


def u(v):
    return int(round(v * S))


def font(names, size_units):
    for n in names:
        p = os.path.join(FONTS, n)
        if os.path.exists(p):
            return ImageFont.truetype(p, u(size_units))
    return ImageFont.load_default()


def rect(d, box, fill, outline=None, w=0.5, pad=(0, 0)):
    x0, y0, x1, y1 = box
    r = [u(x0 - pad[0]), u(y0 - pad[1]), u(x1 + pad[0]) - 1, u(y1 + pad[1]) - 1]
    d.rectangle(r, fill=fill)
    if outline is not None:
        d.rectangle(r, outline=outline, width=max(1, u(w)))


def spaced(d, cx, cy, text, fnt, fill, spacing_u):
    widths = [d.textlength(ch, font=fnt) for ch in text]
    total = sum(widths) + u(spacing_u) * (len(text) - 1)
    x = u(cx) - total / 2
    for ch, w in zip(text, widths):
        d.text((x, u(cy)), ch, font=fnt, fill=fill, anchor="lm")
        x += w + u(spacing_u)


def login_frame():
    img = Image.new("RGBA", (250 * S, 216 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    rect(d, PANEL, C_PANEL + (244,), C_LINE + (255,), 0.5)
    # refined Cold Steel accent corner marks
    x0, y0, x1, y1 = PANEL
    d.rectangle([u(x0 - 0.3), u(y0 - 0.3), u(x0 + 8.0), u(y0 + 1.2)], fill=C_ACCENT + (255,))
    d.rectangle([u(x0 - 0.3), u(y0 - 0.3), u(x0 + 1.2), u(y0 + 8.0)], fill=C_ACCENT + (255,))
    d.rectangle([u(x1 - 8.0), u(y1 - 1.2), u(x1 + 0.3), u(y1 + 0.3)], fill=C_ACCENT + (255,))
    d.rectangle([u(x1 - 1.2), u(y1 - 8.0), u(x1 + 0.3), u(y1 + 0.3)], fill=C_ACCENT + (255,))

    f = font(["bahnschrift.ttf", "segoeuib.ttf"], 7.2)
    spaced(d, 125, 53.5, "MU ONLINE", f, C_INK + (255,), 2.0)

    # subtle header divider with small cyan center tick
    d.rectangle([u(60), u(62.2), u(190) - 1, u(62.7)], fill=C_LINE_DIM + (255,))
    d.rectangle([u(115), u(62.0), u(135) - 1, u(62.9)], fill=C_ACCENT + (210,))

    for b in FIELDS:
        rect(d, b, C_FIELD + (255,), C_LINE_DIM + (255,), 0.5, FIELD_PAD)
    rect(d, CONNECT, (46, 108, 124, 255), C_ACCENT + (255,), 0.5, PAD)
    rect(d, EXIT, (28, 34, 40, 255), C_LINE + (255,), 0.5, PAD)
    return img


def plate_texture(box, fill, outline, tw, th):
    """Hover plate drawn at its final aspect, then squashed into the texture size (the hook stretches it back)."""
    w_u = (box[2] - box[0]) + 2 * PAD[0]
    h_u = (box[3] - box[1]) + 2 * PAD[1]
    W, H = u(w_u), u(h_u)
    im = Image.new("RGB", (W, H), fill)
    ImageDraw.Draw(im).rectangle([0, 0, W - 1, H - 1], outline=outline, width=max(1, u(0.5)))
    return im.resize((tw * S, th * S), Image.LANCZOS)


def server_box(w_u, h_u):
    W, H = w_u * S, h_u * S
    im = Image.new("RGB", (W, H), (58, 70, 82))     # mid slate: the client draws white text and a black load meter on top
    ImageDraw.Draw(im).rectangle([0, 0, W - 1, H - 1], outline=C_ACCENT, width=max(1, u(0.6)))
    return im


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else DST
    _, hdr = muimg.read_any(os.path.join(SRC, "0account_new.ozt"))
    frame = login_frame()
    muimg.write_ozt(frame, hdr, os.path.join(out, "0account_new.ozt"))
    print("wrote 0account_new.ozt", frame.size)
    if out != DST:
        frame.save(os.path.join(out, "preview_login.png"))

    for name, box, fill, outline, tw, th in (
        ("0on_botton.ozj", CONNECT, (66, 144, 162), (170, 220, 230), 70, 19),
        ("0on_botton2.ozj", EXIT, (38, 46, 54), C_INK, 70, 20),
    ):
        _, h2 = muimg.read_any(os.path.join(SRC, name))
        img = plate_texture(box, fill, outline, tw, th)
        muimg.write_ozj(img, h2, os.path.join(out, name), quality=96)
        print("wrote", name, img.size)

    im, h3 = muimg.read_any(os.path.join(SRC, "0box.ozj"))
    box = server_box(im.width, im.height)
    muimg.write_ozj(box, h3, os.path.join(out, "0box.ozj"), quality=96)
    print("wrote 0box.ozj", box.size)


if __name__ == "__main__":
    main()
