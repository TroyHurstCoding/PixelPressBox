#!/usr/bin/env python3
"""LED-look contact sheet of some sim frames: python3 sheet.py 'out/A*.ppm' /path/to/sheet.png"""
import glob, sys
from PIL import Image, ImageDraw
S = 8
files = sorted(glob.glob(sys.argv[1]))
tiles = []
for f in files:
    im = Image.open(f).convert("RGB")
    w, h = im.size
    s = S if w >= h else S // 2
    t = Image.new("RGB", (64 * S, 32 * S + 14), (12, 12, 12))
    d = ImageDraw.Draw(t)
    ox = (64 * S - w * s) // 2
    for y in range(h):
        for x in range(w):
            c = im.getpixel((x, y))
            d.ellipse([ox + x * s + 1, y * s + 1, ox + x * s + s - 2, y * s + s - 2], fill=c if sum(c) else (28, 28, 28))
    d.text((4, 32 * S + 1), f.split("/")[-1], fill=(200, 200, 200))
    tiles.append(t)
cols = 3
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (64 * S + 8), max(1, rows) * (32 * S + 22)), (0, 0, 0))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * (64 * S + 8), (i // cols) * (32 * S + 22)))
sheet.save(sys.argv[2])
print(len(tiles), "frames ->", sys.argv[2])
