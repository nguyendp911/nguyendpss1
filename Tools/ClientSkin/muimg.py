"""Helpers for MU Online texture containers.

OZJ = JPEG with a 24 byte header in front.
OZT = TGA with a 4 byte header in front.
"""
import io
import sys
from PIL import Image

OZJ_HDR = 24
OZT_HDR = 4


def read_ozj(path):
    data = open(path, "rb").read()
    return Image.open(io.BytesIO(data[OZJ_HDR:])).copy(), data[:OZJ_HDR]


def read_ozt(path):
    data = open(path, "rb").read()
    return Image.open(io.BytesIO(data[OZT_HDR:])).copy(), data[:OZT_HDR]


def read_any(path):
    p = path.lower()
    if p.endswith(".ozj"):
        return read_ozj(path)
    if p.endswith(".ozt"):
        return read_ozt(path)
    raise ValueError(path)


def write_ozj(img, hdr, path, quality=95):
    buf = io.BytesIO()
    img.convert("RGB").save(buf, "JPEG", quality=quality, subsampling=0)
    open(path, "wb").write(hdr + buf.getvalue())


def write_ozt(img, hdr, path):
    # MU expects a bottom-up 32 bit TGA like the originals
    buf = io.BytesIO()
    img.convert("RGBA").save(buf, "TGA", rle=False)
    open(path, "wb").write(hdr + buf.getvalue())


if __name__ == "__main__":
    # usage: muimg.py out_dir file1 file2 ...   -> dumps PNGs and prints size/mode
    out = sys.argv[1]
    for f in sys.argv[2:]:
        im, hdr = read_any(f)
        name = f.replace("\\", "/").split("/")[-1].rsplit(".", 1)[0]
        im.save(out + "/" + name + ".png")
        print(name, im.size, im.mode, hdr.hex())
