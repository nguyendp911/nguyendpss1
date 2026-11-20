"""Cold Steel combat HUD atlas for the in-game bottom interface.

One RGBA atlas, 2048x1024, written as Client\\Data\\Interface\\Menu04.ozt (texture 233, originally the dragon corners).
All art is cut from the user's pack C:\\Users\\NGUYENDP\\Downloads\\ColdSteel_Combat_HUD_v1. The layout in the game is drawn by
Source\\Client\\Main\\HudBar.cpp (keep the ATLAS table below and that file in sync).

ATLAS (x, y, w, h) in px:
  orb frame left      (0,0,279,291)      orb frame right  (300,0,281,292)     orb backing (600,0,231,236)
  orb glass           (0,310,230,232)    fill hp (260,310,235,237)            fill mp (520,310,234,238)
  hotbar frame        (0,570,741,232)    slot (760,570,234,229)               skill slot (1010,570,225,224)
  icon button         (1250,570,199,196) secondary button (1470,570,339,128)
  coordinate plate    (760,810,368,128)  ag frame (0,810,358,42)              exp frame (0,860,348,27)
  ag fill             (0,900,253,46)     exp fill (270,900,244,39)
  glyphs 64x64        person (1470,710) magnifier (1540,710) chest (1610,710) swords (1680,710)
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

import muimg

PACK = r"C:\Users\NGUYENDP\Downloads\ColdSteel_Combat_HUD_v1"
SRC = r"C:\Muemu\SkinBackup\Hud"
OUT = r"C:\Muemu\Client\Data\Interface"
CYAN = (32, 216, 245, 255)

_J = {}


def clean(img, thr=24):
    a = np.array(img)
    a[a[..., 3] < thr] = 0
    return Image.fromarray(a, "RGBA")


def sprite(png, name, scale=1.0):
    if png not in _J:
        _J[png] = json.load(open(os.path.join(PACK, png.replace(".png", ".json"))))
    j = _J[png]
    fr = j["frames"] if "frames" in j else j
    f = fr[name]["frame"]
    im = clean(Image.open(os.path.join(PACK, png)).convert("RGBA")).crop((f["x"], f["y"], f["x"] + f["w"], f["y"] + f["h"]))
    if scale != 1.0:
        im = im.resize((int(round(im.width * scale)), int(round(im.height * scale))), Image.LANCZOS)
    return im


def glyph(kind):
    S = 8
    im = Image.new("RGBA", (64 * S, 64 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w = 4 * S
    if kind == "party":
        for dx in (-9, 9):
            d.ellipse([(24 + dx) * S - 8 * S, 10 * S, (24 + dx) * S + 8 * S, 26 * S], outline=CYAN, width=3 * S)
            d.rounded_rectangle([(24 + dx) * S - 13 * S, 32 * S, (24 + dx) * S + 13 * S, 56 * S], radius=10 * S, outline=CYAN, width=3 * S)
        d.rectangle([40 * S, 0, 64 * S, 64 * S], fill=(0, 0, 0, 0))
    elif kind == "person":
        d.ellipse([22 * S, 8 * S, 42 * S, 28 * S], outline=CYAN, width=w)
        d.rounded_rectangle([12 * S, 34 * S, 52 * S, 60 * S], radius=14 * S, outline=CYAN, width=w)
    elif kind == "magnifier":
        d.ellipse([8 * S, 8 * S, 40 * S, 40 * S], outline=CYAN, width=w)
        d.line([(36 * S, 36 * S), (56 * S, 56 * S)], fill=CYAN, width=6 * S)
    elif kind == "chest":
        d.rounded_rectangle([10 * S, 30 * S, 54 * S, 56 * S], radius=3 * S, outline=CYAN, width=w)
        d.rounded_rectangle([6 * S, 12 * S, 58 * S, 30 * S], radius=6 * S, outline=CYAN, width=w)
        d.rectangle([28 * S, 26 * S, 36 * S, 40 * S], fill=CYAN)
    elif kind == "swords":
        d.line([(8 * S, 8 * S), (56 * S, 56 * S)], fill=CYAN, width=5 * S)
        d.line([(56 * S, 8 * S), (8 * S, 56 * S)], fill=CYAN, width=5 * S)
        d.line([(14 * S, 30 * S), (30 * S, 14 * S)], fill=CYAN, width=4 * S)
        d.line([(34 * S, 14 * S), (50 * S, 30 * S)], fill=CYAN, width=4 * S)
    return im.resize((64, 64), Image.LANCZOS)


def make_atlas():
    R = "atlases/hud_01_resources.png"
    HB = "atlases/hud_02_hotbar.png"
    MB = "atlases/hud_05_microbars.png"
    SL = "common/04_slots.png"
    BT = "common/02_buttons.png"
    out = Image.new("RGBA", (2048, 1024), (0, 0, 0, 0))

    def put(img, xy):
        out.paste(img, xy)

    put(sprite(R, "cs.hud.orb.frame_left"), (0, 0))
    put(sprite(R, "cs.hud.orb.frame_right"), (300, 0))
    put(sprite(R, "cs.hud.orb.backing"), (600, 0))
    put(sprite(R, "cs.hud.orb.glass"), (0, 310))
    put(sprite(R, "cs.hud.orb.fill_hp"), (260, 310))
    put(sprite(R, "cs.hud.orb.fill_mp"), (520, 310))
    put(sprite(HB, "cs.hud.hotbar.full_backplate"), (0, 570))
    put(sprite(SL, "cs.slot.normal"), (760, 570))
    put(sprite(SL, "cs.skill.normal"), (1010, 570))
    put(sprite(BT, "cs.button.icon_blank.normal"), (1250, 570))
    put(sprite(BT, "cs.button.secondary.normal"), (1470, 570))
    put(sprite("atlases/hud_04_minimap.png", "cs.hud.minimap.coordinate_plate", 0.5), (760, 810))
    put(sprite(MB, "cs.hud.meter.ag_frame", 0.5), (0, 810))
    put(sprite(MB, "cs.hud.meter.exp_frame", 0.5), (0, 860))
    put(sprite(R, "cs.hud.resource.ag_fill"), (0, 900))
    put(sprite(R, "cs.hud.resource.exp_fill"), (270, 900))
    for i, k in enumerate(("party", "person", "chest", "swords")):
        put(glyph(k), (1470 + 70 * i, 710))
    return out


if __name__ == "__main__":
    if len(sys.argv) > 1:
        OUT = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    atlas = make_atlas()
    im, hdr = muimg.read_any(os.path.join(SRC, "Menu04.ozt"))
    muimg.write_ozt(atlas, hdr, os.path.join(OUT, "Menu04.ozt"))
    atlas.save(os.path.join(SRC, "atlas_preview.png"))
    print("wrote Menu04.ozt", atlas.size)
    # the old bar textures are no longer drawn: put the originals back
    if OUT == r"C:\Muemu\Client\Data\Interface":
        import shutil
        for n in ("Menu01_new.ozj", "Menu02.ozj", "Menu03_new.ozj", "Menu03_new_ag.ozj"):
            shutil.copyfile(os.path.join(SRC, n), os.path.join(OUT, n))
        print("restored original bar textures")
