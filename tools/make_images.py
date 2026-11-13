#!/usr/bin/env python3
"""Convert pictures and GIFs to 64x32 for PixelPressbox's image shuffle.

    pip3 install pillow
    python3 tools/make_images.py INPUT_FOLDER OUTPUT_FOLDER [--fit]

Photos (jpg/png/heic-converted/...) become 64x32 PNGs; animated GIFs stay animated (frame timing kept).
Default is "fill": the image covers the whole panel and the edges are cropped. --fit shows the whole image
with black bars instead. Copy the OUTPUT_FOLDER files into /images on the microSD card.
"""
import os
import sys

from PIL import Image, ImageOps, ImageSequence

W, H = 64, 32


def shape(frame, fit):
    frame = frame.convert("RGB")
    if fit:
        frame = ImageOps.contain(frame, (W, H), Image.LANCZOS)
        canvas = Image.new("RGB", (W, H))
        canvas.paste(frame, ((W - frame.width) // 2, (H - frame.height) // 2))
        return canvas
    return ImageOps.fit(frame, (W, H), Image.LANCZOS)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    fit = "--fit" in sys.argv
    if len(args) != 2:
        raise SystemExit(__doc__)
    src, dst = args
    os.makedirs(dst, exist_ok=True)
    for name in sorted(os.listdir(src)):
        if name.startswith("."):
            continue
        path = os.path.join(src, name)
        try:
            im = Image.open(path)
        except Exception:
            continue
        base = os.path.splitext(name)[0]
        if getattr(im, "is_animated", False):
            frames, durations = [], []
            for fr in ImageSequence.Iterator(im):
                frames.append(shape(fr, fit).quantize(colors=256))
                durations.append(fr.info.get("duration", 100) or 100)
            out = os.path.join(dst, base + ".gif")
            frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations, loop=0,
                           disposal=1, optimize=False)
        else:
            out = os.path.join(dst, base + ".png")
            shape(im, fit).save(out)
        print("wrote", out)


if __name__ == "__main__":
    main()
