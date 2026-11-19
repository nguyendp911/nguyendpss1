"""Flat digital skin for the in-game menu buttons (Exit Game / Options / Cancel) and message boxes.

Texture 240 = Interface\\Message_box.ozj (213x64). The menu draws it three times, squashed to 120x22 virtual units, so the art is
rendered at that final aspect and then stretched into the 213x64 texture (the squash in game restores it).
Message_box1 = body piece, Message_box2 = bottom edge of the message boxes.
"""
import os
import sys
from PIL import Image, ImageDraw
import muimg

SRC = r"C:\Muemu\SkinBackup\Interface"
DST = r"C:\Muemu\Client\Data\Interface"
S = 4

PANEL = (23, 28, 33)
LINE = (59, 70, 81)
ACCENT = (111, 182, 196)


def button_art(w_units=120, h_units=22):
    W, H = w_units * 4, h_units * 4          # 4 px per unit while drawing
    im = Image.new("RGB", (W, H), PANEL)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W - 1, H - 1], outline=LINE, width=4)
    d.rectangle([0, 0, 11, H - 1], fill=ACCENT)           # accent bar on the left
    return im


def body_piece(w, h):
    im = Image.new("RGB", (w, h), PANEL)
    d = ImageDraw.Draw(im)
    d.rectangle([0, -1, w - 1, h], outline=LINE, width=max(4, S))
    d.rectangle([0, 0, max(4, S) * 2, h], fill=LINE)
    return im


def bottom_piece(w, h):
    im = Image.new("RGB", (w, h), PANEL)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, w - 1, h - 1], fill=LINE)
    return im


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else DST
    for name, make in (
        ("Message_box.ozj", lambda w, h: button_art().resize((w, h), Image.LANCZOS)),
        ("Message_box1.ozj", body_piece),
        ("Message_box2.ozj", bottom_piece),
    ):
        im, hdr = muimg.read_any(os.path.join(SRC, name))
        w, h = im.width * S, im.height * S
        img = make(w, h)
        muimg.write_ozj(img, hdr, os.path.join(out, name), quality=96)
        print("wrote", name, img.size)


if __name__ == "__main__":
    main()
