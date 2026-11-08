#!/usr/bin/env python3
"""Generates the bitmaps of themes/CUPPLAT, "Cupertino Platinum" (original
artwork in a grey, bevelled style; public domain).

Drawn with the supersampled Canvas of gen_redexp.py: edge pixels are blended
with the colour the part sits on, fully transparent pixels become the colour
key magenta (255,0,255).

  python3 tools/gen_cupplat.py               writes themes/CUPPLAT/*.BMP
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_redexp import Canvas, mix, strip, rows  # noqa: E402
from PIL import Image  # noqa: E402
import gen_redexp  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "themes", "CUPPLAT")

# ---- palette (Platinum) ----
# keep in sync with [SysColors] / [Colors] in themes/CUPPLAT/THEME.INI
FACE = (221, 221, 221)          # BtnFace, captions, menus
WHITE = (255, 255, 255)
LIGHT = (238, 238, 238)         # the lit half of a bevel
SHADOW = (153, 153, 153)        # BtnShadow
DARK = (102, 102, 102)          # the dark half of a bevel
OUTLINE = (34, 34, 34)          # outlines of buttons and windows
STRIPE_DARK = (153, 153, 153)   # the grooves of an active caption
LAVENDER = (204, 204, 255)      # scroll bar thumbs
LAV_LIGHT = (232, 232, 255)
LAV_DARK = (136, 136, 204)
LAV_LINE = (68, 68, 136)
TRACK = (170, 170, 170)


def save(im, name):
    gen_redexp.save(im, name)


# ---- window frame: 12x12 nine-grid cells (margins 4) ----
# a dark outline, a white line at the top left and a grey one at the bottom
# right (raised), then the face

def frame(active):
    im = Image.new("RGB", (12, 12), FACE)
    px = im.load()
    out = OUTLINE if active else (119, 119, 119)
    for i in range(12):
        px[i, 0] = px[i, 11] = px[0, i] = px[11, i] = out
    for i in range(1, 11):
        px[i, 1] = px[1, i] = WHITE if active else LIGHT
        px[i, 10] = px[10, i] = SHADOW if active else (187, 187, 187)
    return im


# ---- the grooves of the active caption: a 4x12 tile ----
# six ridges: a dark row with a white row below

def stripes():
    im = Image.new("RGB", (4, 12), FACE)
    px = im.load()
    for y in range(12):
        for x in range(4):
            px[x, y] = STRIPE_DARK if y % 2 == 0 else WHITE
    return im


# ---- caption buttons: 17x17 cells on the face (they hide the grooves) ----
# an 11x11 box: dark outline, grey gradient inside, white rim below and on
# the right (it is pressed into the bar)

BW, BH = 17, 17


def box(c, state):
    x0, y0 = (BW - 11) // 2, (BH - 11) // 2
    x1, y1 = x0 + 10, y0 + 10
    if state == "disabled":
        c.rect(x0, y0, x1, y1, (187, 187, 187))
        c.rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, FACE)
        return x0, y0, x1, y1
    # the white rim of the groove the box sits in
    c.rect(x0 + 1, y0 + 1, x1 + 1, y1 + 1, WHITE)
    c.rect(x0, y0, x1, y1, OUTLINE)
    if state == "pressed":
        stops = [(0, (102, 102, 102)), (1, (170, 170, 170))]
    else:
        stops = [(0, (255, 255, 255)), (0.5, (221, 221, 221)), (1, (170, 170, 170))]
    c.vgrad_rrect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 0, stops)
    return x0, y0, x1, y1


def capbtn(kind, state):
    c = Canvas(BW, BH)
    c.rect(0, 0, BW - 1, BH - 1, FACE)
    x0, y0, x1, y1 = box(c, state)
    col = OUTLINE if state != "disabled" else (187, 187, 187)
    if state == "disabled":
        return c.render(FACE)
    if kind in ("max", "restore"):
        # zoom box: a smaller box in the top left corner
        c.rect(x0 + 1, y0 + 1, x0 + 5, y0 + 1, col)
        c.rect(x0 + 1, y0 + 5, x0 + 5, y0 + 5, col)
        c.rect(x0 + 5, y0 + 1, x0 + 5, y0 + 5, col)
        if kind == "restore":
            c.rect(x0 + 3, y0 + 3, x1 - 2, y0 + 3, col)
            c.rect(x1 - 2, y0 + 3, x1 - 2, y1 - 2, col)
            c.rect(x0 + 3, y1 - 2, x1 - 2, y1 - 2, col)
            c.rect(x0 + 3, y0 + 5, x0 + 3, y1 - 2, col)
    elif kind == "min":
        # collapse box: two lines across the middle
        c.rect(x0 + 1, y0 + 4, x1 - 1, y0 + 4, col)
        c.rect(x0 + 1, y0 + 6, x1 - 1, y0 + 6, col)
    return c.render(FACE)


def capbtns():
    cells = []
    for kind in ("close", "min", "max", "restore"):
        for state in ("normal", "pressed", "disabled"):
            cells.append(capbtn(kind, state))
    return strip(cells)


# ---- push buttons: 24x24 nine-grid cells (margins 6) ----
# rounded (radius 4), a dark outline, a light-to-grey gradient; the default
# button has the thick ring around it

def pushbtn(state):
    w, h = 24, 24
    c = Canvas(w, h)
    if state == "default":
        # the ring: outline, a grey band, outline
        c.rrect(0, 0, w - 1, h - 1, 6, OUTLINE)
        c.vgrad_rrect(1, 1, w - 2, h - 2, 5, [(0, (238, 238, 238)), (1, (153, 153, 153))])
        x0, y0, x1, y1, r = 3, 3, w - 4, h - 4, 3
    else:
        x0, y0, x1, y1, r = 0, 0, w - 1, h - 1, 4
    out = OUTLINE if state != "disabled" else (153, 153, 153)
    c.rrect(x0, y0, x1, y1, r, out)
    if state == "pressed":
        stops = [(0, (102, 102, 102)), (1, (153, 153, 153))]
    elif state == "disabled":
        stops = [(0, (238, 238, 238)), (1, (221, 221, 221))]
    else:
        stops = [(0, (255, 255, 255)), (0.45, (234, 234, 234)), (1, (187, 187, 187))]
    c.vgrad_rrect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, r - 1, stops)
    if state != "pressed" and state != "disabled":
        # a white line inside the top and a grey one inside the bottom
        c.rect(x0 + 3, y0 + 1, x1 - 3, y0 + 1, WHITE)
        c.rect(x0 + 3, y1 - 1, x1 - 3, y1 - 1, (153, 153, 153))
    return c.render(FACE)


def pushbtns():
    return strip([pushbtn(s) for s in ("normal", "pressed", "default", "disabled")])


# ---- check boxes and radio buttons: 13x13 cells ----

def boxface(c, state, round_):
    out = OUTLINE if state != "disabled" else (153, 153, 153)
    if state == "pressed":
        stops = [(0, (119, 119, 119)), (1, (187, 187, 187))]
    elif state == "disabled":
        stops = [(0, (238, 238, 238)), (1, (221, 221, 221))]
    else:
        stops = [(0, (255, 255, 255)), (1, (187, 187, 187))]
    if round_:
        c.ellipse(0, 0, 12, 12, out)
        c.vgrad_ellipse(1, 1, 11, 11, stops)
    else:
        c.rrect(0, 0, 12, 12, 1, out)
        c.vgrad_rrect(1, 1, 11, 11, 0, stops)
    return out


def checkcell(state, mark):
    c = Canvas(13, 13)
    col = boxface(c, state, False)
    if mark == "check":
        c.line([(2.5, 6), (5, 9), (11, 1.5)], 1.7, col)
    elif mark == "mixed":
        c.rect(3, 6, 9, 7, col)
    return c.render(FACE)


def radiocell(state, dot):
    c = Canvas(13, 13)
    col = boxface(c, state, True)
    if dot:
        c.ellipse(4, 4, 8, 8, col)
    return c.render(FACE)


def checks():
    boxes = [checkcell(s, m) for m in (None, "check", "mixed")
             for s in ("normal", "pressed", "disabled")]
    radios = [radiocell(s, d) for d in (False, True) for s in ("normal", "pressed", "disabled")]
    return rows([strip(boxes), strip(radios)])


# ---- scroll bars: arrow boxes, the lavender thumb with its grip, tracks ----
# SCROLL.BMP: 16x16 cells: arrow box normal, pressed, disabled; thumb
# vertical, thumb horizontal; then the track cells (vertical 16x16: dark
# at the left, horizontal: dark at the top)

def arrowbox(state):
    im = Image.new("RGB", (16, 16), FACE)
    px = im.load()
    for i in range(16):
        px[i, 0] = px[0, i] = px[i, 15] = px[15, i] = OUTLINE
    if state == "disabled":
        for y in range(1, 15):
            for x in range(1, 15):
                px[x, y] = FACE
        return im
    for y in range(1, 15):
        for x in range(1, 15):
            if state == "pressed":
                px[x, y] = mix((119, 119, 119), (170, 170, 170), (x + y) / 28)
            else:
                px[x, y] = mix((250, 250, 250), (187, 187, 187), (x + y) / 28)
    if state != "pressed":
        for i in range(1, 15):
            px[i, 1] = px[1, i] = WHITE
            px[i, 14] = px[14, i] = SHADOW
    return im


def thumb(horizontal):
    c = Canvas(16, 16)
    c.rect(0, 0, 15, 15, OUTLINE)
    c.rect(1, 1, 14, 14, LAVENDER)
    c.rect(1, 1, 14, 1, LAV_LIGHT)
    c.rect(1, 1, 1, 14, LAV_LIGHT)
    c.rect(1, 14, 14, 14, LAV_DARK)
    c.rect(14, 1, 14, 14, LAV_DARK)
    # the grip: four ridges across the middle
    for i in range(4):
        p = 4 + i * 2
        if horizontal:
            c.rect(p, 4, p, 10, LAV_LINE)
            c.rect(p + 1, 5, p + 1, 11, LAV_LIGHT)
        else:
            c.rect(4, p, 10, p, LAV_LINE)
            c.rect(5, p + 1, 11, p + 1, LAV_LIGHT)
    return c.render(FACE)


def track(horizontal):
    im = Image.new("RGB", (16, 16), TRACK)
    px = im.load()
    for i in range(16):
        for j in range(16):
            d = j if horizontal else i      # distance from the shadowed edge
            if d == 0:
                px[i, j] = (119, 119, 119)
            elif d == 1:
                px[i, j] = (153, 153, 153)
            elif d == 15:
                px[i, j] = (204, 204, 204)
    return im


def scroll():
    return strip([arrowbox("normal"), arrowbox("pressed"), arrowbox("disabled"),
                  thumb(False), thumb(True), track(False), track(True)])


def main():
    gen_redexp.OUT = OUT
    os.makedirs(OUT, exist_ok=True)
    save(strip([frame(True), frame(False)]), "FRAME.BMP")
    save(stripes(), "STRIPES.BMP")
    save(capbtns(), "CAPBTN.BMP")
    save(pushbtns(), "BUTTON.BMP")
    save(checks(), "CHECKS.BMP")
    save(scroll(), "SCROLL.BMP")


if __name__ == "__main__":
    main()
