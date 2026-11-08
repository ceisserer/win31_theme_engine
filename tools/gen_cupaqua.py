#!/usr/bin/env python3
"""Generates the bitmaps of themes/CUPAQUA, "Cupertino Aqua" (original artwork in
a light, glossy style: pinstripes, gel buttons; public
domain).

Drawn with the supersampled Canvas of gen_redexp.py: edge pixels are blended
with the colour the part sits on, fully transparent pixels become the colour
key magenta (255,0,255), and the rounded top corners of the window frame
become yellow (255,255,0), which THEME.INI recolours to the desktop colour.

  python3 tools/gen_cupaqua.py               writes themes/CUPAQUA/*.BMP
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_redexp import Canvas, CORNER, mix, strip, rows  # noqa: E402
from PIL import Image  # noqa: E402
import gen_redexp  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "themes", "CUPAQUA")

# ---- palette ----
# keep in sync with [SysColors] / [Colors] in themes/CUPAQUA/THEME.INI
FACE = (232, 232, 232)          # BtnFace: dialogs
STRIPE_A = (246, 246, 246)      # pinstripes: two light rows...
STRIPE_B = (232, 232, 232)      # ...and two darker rows
DESKTOP = (58, 110, 190)
OUTLINE = (128, 128, 128)       # window outline
WHITE = (255, 255, 255)

# gel: (top highlight from, to), (body from, to) - the body brightens
# towards the bottom, as light shines through
BLUE = [(0, (196, 222, 255)), (0.45, (92, 156, 236)), (0.5, (52, 120, 222)),
        (1, (150, 210, 255))]
BLUE_DOWN = [(0, (150, 180, 230)), (0.45, (52, 98, 190)), (0.5, (24, 70, 170)),
             (1, (100, 160, 230))]
CLEAR = [(0, (255, 255, 255)), (0.45, (238, 238, 238)), (0.5, (218, 218, 218)),
         (1, (250, 250, 250))]
CLEAR_DOWN = [(0, (220, 220, 220)), (0.45, (190, 190, 190)), (0.5, (168, 168, 168)),
              (1, (214, 214, 214))]
FADED = [(0, (250, 250, 250)), (1, (238, 238, 238))]
RED = [(0, (255, 200, 190)), (0.45, (238, 90, 74)), (0.5, (214, 50, 36)),
       (1, (255, 150, 120))]
YELLOW = [(0, (255, 240, 190)), (0.45, (246, 190, 60)), (0.5, (232, 160, 20)),
          (1, (255, 220, 110))]
GREEN = [(0, (210, 250, 190)), (0.45, (110, 200, 80)), (0.5, (60, 170, 40)),
         (1, (150, 230, 120))]
GREY = [(0, (245, 245, 245)), (0.45, (210, 210, 210)), (0.5, (190, 190, 190)),
        (1, (225, 225, 225))]


def darker(stops, f=0.75):
    return [(p, tuple(int(v * f) for v in c)) for p, c in stops]


def save(im, name):
    gen_redexp.save(im, name)


def pin_row(y):
    return STRIPE_A if (y // 2) % 2 == 0 else STRIPE_B


# ---- pinstripes: a 4x4 tile ----

def pinstripes(light=False):
    im = Image.new("RGB", (4, 4))
    px = im.load()
    for y in range(4):
        c = pin_row(y)
        if light:
            c = mix(c, WHITE, 0.5)
        for x in range(4):
            px[x, y] = c
    return im


# ---- window frame: 12x12 nine-grid cells (margins 4) ----
# a grey outline, pinstripes inside, the top corners rounded

def frame(active):
    c = Canvas(12, 12)
    out = OUTLINE if active else (170, 170, 170)
    c.rrect(0, 0, 11, 11, 4, out)
    c.rect(0, 6, 11, 11, out)
    c.rrect(1, 1, 10, 10, 3, STRIPE_A)
    c.rect(1, 5, 10, 10, STRIPE_A)
    im = c.render(DESKTOP, key=CORNER)
    # the inside: pinstripes (rows counted from the top of the window)
    px = im.load()
    for y in range(12):
        for x in range(12):
            if px[x, y] == STRIPE_A:
                p = pin_row(y)
                px[x, y] = p if active else mix(p, WHITE, 0.5)
    return im


# ---- caption buttons: 20x16 cells, gel circles (14 pixels) ----

BW, BH = 20, 16


def gel_circle(c, x0, y0, d, stops, ring):
    c.ellipse(x0, y0, x0 + d - 1, y0 + d - 1, ring)
    c.vgrad_ellipse(x0 + 1, y0 + 1, x0 + d - 2, y0 + d - 2, stops)
    # the glossy highlight in the upper half
    c.d.ellipse(((x0 + 3) * 4, (y0 + 1.5) * 4, (x0 + d - 3) * 4, (y0 + d * 0.5) * 4),
                fill=(255, 255, 255, 140))


def capbtn(kind, state):
    c = Canvas(BW, BH)
    d = 14
    x0, y0 = (BW - d) // 2, (BH - d) // 2
    stops = {"close": RED, "min": YELLOW, "max": GREEN, "restore": GREEN}[kind]
    if state == "disabled":
        stops = GREY
    elif state == "pressed":
        stops = darker(stops)
    ring = mix(stops[2][1], (0, 0, 0), 0.35)
    gel_circle(c, x0, y0, d, stops, ring)
    if state != "disabled":
        # the symbol, dark, like the glyphs Aqua shows under the pointer
        col = mix(stops[2][1], (0, 0, 0), 0.55)
        cx, cy = x0 + d / 2 - 0.5, y0 + d / 2 - 0.5
        if kind == "close":
            c.line([(cx - 2.5, cy - 2.5), (cx + 2.5, cy + 2.5)], 1.3, col)
            c.line([(cx + 2.5, cy - 2.5), (cx - 2.5, cy + 2.5)], 1.3, col)
        elif kind == "min":
            c.line([(cx - 3, cy), (cx + 3, cy)], 1.4, col)
        else:
            c.line([(cx - 3, cy), (cx + 3, cy)], 1.4, col)
            c.line([(cx, cy - 3), (cx, cy + 3)], 1.4, col)
    # transparent around the circle: the pinstripes show through
    return c.render(STRIPE_A)


def capbtns():
    cells = []
    for kind in ("close", "min", "max", "restore"):
        for state in ("normal", "pressed", "disabled"):
            cells.append(capbtn(kind, state))
    return strip(cells)


# ---- push buttons: 28x22 nine-grid cells (margins 10,5,10,5): gel pills ----

def pill(c, x0, y0, x1, y1, stops, ring):
    r = (y1 - y0 + 1) / 2
    c.rrect(x0, y0, x1, y1, r, ring)
    c.vgrad_rrect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, r - 1, stops)
    hl = Image.new("RGBA", c.im.size, (0, 0, 0, 0))
    from PIL import ImageDraw
    ImageDraw.Draw(hl).rounded_rectangle(
        ((x0 + r * 0.6) * 4, (y0 + 1.5) * 4, (x1 + 1 - r * 0.6) * 4, (y0 + (y1 - y0) * 0.45) * 4),
        radius=r * 2, fill=(255, 255, 255, 150))
    c.im.alpha_composite(hl)


def pushbtn(state):
    w, h = 28, 22
    c = Canvas(w, h)
    stops, ring = {
        "normal": (CLEAR, (140, 140, 140)),
        "pressed": (BLUE_DOWN, (40, 70, 140)),
        "default": (BLUE, (60, 100, 170)),
        "disabled": (FADED, (190, 190, 190)),
    }[state]
    # a soft shadow under the pill
    c.rrect(1, 1, w - 2, h - 1, (h - 1) / 2, (200, 200, 200))
    pill(c, 0, 0, w - 1, h - 2, stops, ring)
    return c.render(FACE)


def pushbtns():
    return strip([pushbtn(s) for s in ("normal", "pressed", "default", "disabled")])


# ---- check boxes and radio buttons: 13x13 cells ----

def checkcell(state, mark):
    c = Canvas(13, 13)
    if mark is None:
        stops = {"pressed": CLEAR_DOWN, "disabled": FADED}.get(state, CLEAR)
        ring = (150, 150, 150) if state != "disabled" else (200, 200, 200)
    else:
        stops = {"pressed": BLUE_DOWN, "disabled": GREY}.get(state, BLUE)
        ring = (60, 100, 170) if state != "disabled" else (180, 180, 180)
    c.rrect(0, 0, 12, 12, 3, ring)
    c.vgrad_rrect(1, 1, 11, 11, 2, stops)
    col = (20, 20, 20) if state != "disabled" else (150, 150, 150)
    if mark == "check":
        c.line([(3, 6), (5.5, 9), (10.5, 2)], 1.6, col)
    elif mark == "mixed":
        c.rect(3, 6, 9, 7, col)
    return c.render(FACE)


def radiocell(state, dot):
    c = Canvas(13, 13)
    if not dot:
        stops = {"pressed": CLEAR_DOWN, "disabled": FADED}.get(state, CLEAR)
        ring = (150, 150, 150) if state != "disabled" else (200, 200, 200)
    else:
        stops = {"pressed": BLUE_DOWN, "disabled": GREY}.get(state, BLUE)
        ring = (60, 100, 170) if state != "disabled" else (180, 180, 180)
    c.ellipse(0, 0, 12, 12, ring)
    c.vgrad_ellipse(1, 1, 11, 11, stops)
    if dot:
        c.ellipse(4, 4, 8, 8, (20, 20, 20) if state != "disabled" else (150, 150, 150))
    return c.render(FACE)


def checks():
    boxes = [checkcell(s, m) for m in (None, "check", "mixed")
             for s in ("normal", "pressed", "disabled")]
    radios = [radiocell(s, d) for d in (False, True) for s in ("normal", "pressed", "disabled")]
    return rows([strip(boxes), strip(radios)])


# ---- scroll bars (SCROLL.BMP, 16x16 cells) ----
# track vertical, track horizontal (a groove, darker at the edge), arrow
# button normal and pressed (white gel), thumb vertical and horizontal (blue
# gel pills)

def groove(horizontal):
    im = Image.new("RGB", (16, 16))
    px = im.load()
    prof = [(170, 170, 170), (196, 196, 196), (214, 214, 214), (226, 226, 226)] + \
        [(236, 236, 236)] * 8 + [(240, 240, 240), (244, 244, 244), (238, 238, 238), (200, 200, 200)]
    for i in range(16):
        for j in range(16):
            px[i, j] = prof[j] if horizontal else prof[i]
    return im


def arrowbtn(pressed):
    c = Canvas(16, 16)
    c.rect(0, 0, 15, 15, (150, 150, 150))
    c.vgrad_rrect(1, 1, 14, 14, 0, CLEAR_DOWN if pressed else CLEAR)
    return c.render(FACE)


def thumb(horizontal):
    c = Canvas(16, 16)
    if horizontal:
        pill(c, 0, 1, 15, 14, BLUE, (60, 100, 170))
        return c.render((236, 236, 236))
    # vertical: draw it lying down and turn it
    pill(c, 0, 1, 15, 14, BLUE, (60, 100, 170))
    im = c.render((236, 236, 236))
    # turn, then mirror so the light falls from the left
    return im.transpose(Image.ROTATE_90).transpose(Image.FLIP_LEFT_RIGHT)


def scroll():
    return strip([groove(False), groove(True), arrowbtn(False), arrowbtn(True),
                  thumb(False), thumb(True)])


# ---- small parts: the blue gel highlight of menus (4x18, stretched) ----

def menusel():
    im = Image.new("RGB", (4, 18))
    px = im.load()
    for y in range(18):
        t = y / 17
        c = mix((110, 160, 240), (40, 100, 220), t) if t < 0.5 else mix((40, 100, 220), (80, 140, 240), (t - 0.5) * 2)
        for x in range(4):
            px[x, y] = c
    return im


def main():
    gen_redexp.OUT = OUT
    os.makedirs(OUT, exist_ok=True)
    save(strip([pinstripes(False), pinstripes(True)]), "PINSTRIP.BMP")
    save(strip([frame(True), frame(False)]), "FRAME.BMP")
    save(capbtns(), "CAPBTN.BMP")
    save(pushbtns(), "BUTTON.BMP")
    save(checks(), "CHECKS.BMP")
    save(scroll(), "SCROLL.BMP")
    save(menusel(), "MENUSEL.BMP")


if __name__ == "__main__":
    main()
