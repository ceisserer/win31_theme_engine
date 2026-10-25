#!/usr/bin/env python3
"""Generates the bitmaps of themes/SLATE (original artwork, public domain).

Writes 8-bpp Windows 3.x DIBs (bottom-up BI_RGB), which is what the engine
loads. Magenta (255,0,255) is the colour key.
"""
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "themes", "SLATE")
KEY = (255, 0, 255)


def rounded(draw, box, r, fill, outline):
    draw.rounded_rectangle(box, radius=r, fill=fill, outline=outline)


def save(img, name):
    img.convert("RGB").quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) \
        .save(os.path.join(OUT, name))


# push button strip: normal, pressed, default (24x22 each)
w, h = 24, 22
im = Image.new("RGB", (w * 3, h), KEY)
d = ImageDraw.Draw(im)
rounded(d, (0, 0, w - 1, h - 1), 4, (236, 239, 241), (120, 134, 148))
rounded(d, (w, 0, 2 * w - 1, h - 1), 4, (200, 208, 214), (84, 98, 112))
rounded(d, (2 * w, 0, 3 * w - 1, h - 1), 4, (236, 239, 241), (41, 128, 185))
rounded(d, (2 * w + 1, 1, 3 * w - 2, h - 2), 3, None, (41, 128, 185))
save(im, "BUTTON.BMP")

# caption buttons: normal, pressed, close-normal, close-pressed (16x14 each)
w, h = 16, 14
im = Image.new("RGB", (w * 4, h), KEY)
d = ImageDraw.Draw(im)
rounded(d, (0, 0, w - 1, h - 1), 3, (82, 103, 124), (110, 132, 154))
rounded(d, (w, 0, 2 * w - 1, h - 1), 3, (44, 62, 80), (110, 132, 154))
rounded(d, (2 * w, 0, 3 * w - 1, h - 1), 3, (192, 57, 43), (231, 76, 60))
rounded(d, (3 * w, 0, 4 * w - 1, h - 1), 3, (150, 40, 27), (231, 76, 60))
save(im, "CAPBTN.BMP")
