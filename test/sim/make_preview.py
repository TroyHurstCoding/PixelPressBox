#!/usr/bin/env python3
"""Turn the simulator's 64x32 frames into one LED-look preview sheet: ../../screens_preview.png"""
import glob, os
from PIL import Image, ImageDraw, ImageFont

S = 7  # pixels per LED
files = sorted(glob.glob(os.path.join(os.path.dirname(os.path.abspath(__file__)), "out", "*.ppm")))
cols = 3
tiles = []
for f in files:
    im = Image.open(f).convert("RGB")
    t = Image.new("RGB", (64 * S, 32 * S), (12, 12, 12))
    d = ImageDraw.Draw(t)
    for y in range(32):
        for x in range(64):
            c = im.getpixel((x, y))
            if c == (0, 0, 0):
                c = (28, 28, 28)
            d.ellipse([x * S + 1, y * S + 1, x * S + S - 2, y * S + S - 2], fill=c)
    tiles.append((os.path.basename(f)[3:-4].replace("_", " "), t))
rows = (len(tiles) + cols - 1) // cols
W, H = 64 * S, 32 * S
sheet = Image.new("RGB", (cols * (W + 24) + 24, rows * (H + 48) + 24), (245, 245, 245))
d = ImageDraw.Draw(sheet)
try:
    font = ImageFont.truetype("DejaVuSans.ttf", 18)
except OSError:
    font = ImageFont.load_default()
for i, (name, t) in enumerate(tiles):
    x = 24 + (i % cols) * (W + 24)
    y = 24 + (i // cols) * (H + 48)
    d.text((x, y), name, fill=(40, 40, 40), font=font)
    sheet.paste(t, (x, y + 24))
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "screens_preview.png")
sheet.save(out)
print("wrote", os.path.abspath(out))
