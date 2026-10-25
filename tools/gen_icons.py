#!/usr/bin/env python3
"""Writes the 32x32 16-colour icon of RETHEME.EXE (original pixel art): a
Win95-style window. (SETUP.EXE has Win16Setup's icon.)

  gen_icons.py            writes loader/retheme.ico
"""
import os
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

# standard VGA palette (index -> RGB)
PAL = [(0, 0, 0), (128, 0, 0), (0, 128, 0), (128, 128, 0), (0, 0, 128), (128, 0, 128), (0, 128, 128),
       (192, 192, 192), (128, 128, 128), (255, 0, 0), (0, 255, 0), (255, 255, 0), (0, 0, 255),
       (255, 0, 255), (0, 255, 255), (255, 255, 255)]
K, NAVY, TEAL, SILVER, GREY, WHITE, BLUE = 0, 4, 6, 7, 8, 15, 12
T = None  # transparent


def blank():
    return [[T] * 32 for _ in range(32)]


def rect(px, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            px[y][x] = c


def window(px, x0, y0, x1, y1):
    """Raised Win95 window with caption, close box and white client."""
    rect(px, x0, y0, x1, y1, SILVER)
    rect(px, x0, y0, x1, y0, SILVER)
    rect(px, x0, y1, x1, y1, K)
    rect(px, x1, y0, x1, y1, K)
    rect(px, x0 + 1, y0 + 1, x1 - 1, y0 + 1, WHITE)
    rect(px, x0 + 1, y0 + 1, x0 + 1, y1 - 1, WHITE)
    rect(px, x0 + 1, y1 - 1, x1 - 1, y1 - 1, GREY)
    rect(px, x1 - 1, y0 + 1, x1 - 1, y1 - 1, GREY)
    # caption
    rect(px, x0 + 3, y0 + 3, x1 - 3, y0 + 8, NAVY)
    rect(px, x0 + 4, y0 + 4, x0 + 7, y0 + 7, TEAL)    # app icon
    # close box
    bx = x1 - 9
    rect(px, bx, y0 + 4, bx + 5, y0 + 7, SILVER)
    for i in range(3):
        px[y0 + 4 + i][bx + 1 + i] = K
        px[y0 + 4 + i][bx + 4 - i] = K
    # client area (sunken)
    rect(px, x0 + 3, y0 + 10, x1 - 3, y1 - 3, GREY)
    rect(px, x0 + 4, y0 + 11, x1 - 3, y1 - 3, WHITE)
    for y in range(y0 + 13, y1 - 4, 3):
        rect(px, x0 + 6, y, min(x1 - 6, x0 + 6 + (y * 7) % 13 + 6), y, GREY)


def write_ico(path, px):
    xor = b""
    mask = b""
    for row in reversed(px):
        bits = 0
        line = bytearray()
        for x in range(0, 32, 2):
            a = row[x] if row[x] is not None else 0
            b = row[x + 1] if row[x + 1] is not None else 0
            line.append((a << 4) | b)
        xor += bytes(line)
        for x in range(32):
            bits = (bits << 1) | (1 if row[x] is None else 0)
        mask += struct.pack(">I", bits)
    pal = b"".join(struct.pack("BBBB", b, g, r, 0) for r, g, b in PAL)
    bih = struct.pack("<IiiHHIIiiII", 40, 32, 64, 1, 4, 0, len(xor) + len(mask), 0, 0, 16, 0)
    img = bih + pal + xor + mask
    hdr = struct.pack("<HHH", 0, 1, 1) + struct.pack("<BBBBHHII", 32, 32, 16, 0, 0, 0, len(img), 22)
    with open(path, "wb") as f:
        f.write(hdr + img)
    print(path)


def main():
    px = blank()
    window(px, 1, 3, 30, 28)
    write_ico(os.path.join(ROOT, "loader", "retheme.ico"), px)


if __name__ == "__main__":
    main()
