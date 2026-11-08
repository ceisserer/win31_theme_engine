#!/usr/bin/env python3
"""Generates the bitmaps of themes/RED11, "Redmond-11" (original artwork in
a light, flat style; public domain).

Everything is drawn at 4x and scaled down (the Canvas of gen_redexp.py), so
rounded shapes get anti-aliased edges. Edge pixels are blended with the
colour the part sits on (the caption for caption buttons, the dialog face for
controls, the menu for menu highlights); fully transparent pixels become the
colour key magenta (255,0,255), and the corners of the window frame become
yellow (255,255,0), which THEME.INI recolours to the desktop colour.

Writes 24-bpp Windows 3.x DIBs (bottom-up, BI_RGB), like gen_redexp.py.

  python3 tools/gen_red11.py             writes themes/RED11/*.BMP
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_redexp import Canvas, KEY, CORNER, mix, strip, rows  # noqa: E402
import gen_redexp  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "themes", "RED11")

# ---- palette (light) ----
# keep in sync with [SysColors] / [Colors] in themes/RED11/THEME.INI
FACE = (243, 243, 243)          # BtnFace: dialogs
CAP_ACT = (238, 242, 248)       # ActiveCaption: the light, slightly blue "Mica" bar
CAP_INACT = (249, 249, 249)     # InactiveCaption
DESKTOP = (0, 84, 166)          # Background
MENU = (249, 249, 249)          # Menu
ACCENT = (0, 95, 184)           # default buttons, checked boxes
ACCENT_DARK = (0, 74, 150)
ACCENT_PRESSED = (0, 78, 156)
GLYPH = (26, 26, 26)            # caption button glyphs
GLYPH_OFF = (160, 160, 160)
CLOSE_RED = (196, 43, 28)
TRACK = (240, 240, 240)         # scroll bar track
THUMB = (133, 133, 133)
CTRL_BORDER = (138, 138, 138)   # check box / radio outline


# ---- window frame: 12x12 nine-grid cells (margins 4), all corners round ----

def frame(fill, outer):
    c = Canvas(12, 12)
    c.rrect(0, 0, 11, 11, 4, outer)
    c.rrect(1, 1, 10, 10, 3, fill)
    # the inner part of the ring is plain fill (it continues into the caption)
    c.rect(4, 1, 7, 10, fill)
    c.rect(1, 4, 10, 7, fill)
    return c.render(DESKTOP, key=CORNER)


# ---- caption buttons: 28x18 cells, flat ----

BW, BH = 28, 18


def glyph(c, kind, col):
    x0, y0 = (BW - 10) // 2, (BH - 10) // 2     # a 10x10 glyph box
    if kind == "close":
        c.line([(x0, y0), (x0 + 9, y0 + 9)], 1.1, col)
        c.line([(x0 + 9, y0), (x0, y0 + 9)], 1.1, col)
    elif kind == "min":
        c.rect(x0, y0 + 5, x0 + 9, y0 + 5, col)
    elif kind == "max":
        c.rect(x0, y0, x0 + 9, y0, col)
        c.rect(x0, y0 + 9, x0 + 9, y0 + 9, col)
        c.rect(x0, y0, x0, y0 + 9, col)
        c.rect(x0 + 9, y0, x0 + 9, y0 + 9, col)
    elif kind == "restore":
        # front window
        c.rect(x0, y0 + 2, x0 + 7, y0 + 2, col)
        c.rect(x0, y0 + 9, x0 + 7, y0 + 9, col)
        c.rect(x0, y0 + 2, x0, y0 + 9, col)
        c.rect(x0 + 7, y0 + 2, x0 + 7, y0 + 9, col)
        # the back window peeks out at the top right
        c.rect(x0 + 2, y0, x0 + 9, y0, col)
        c.rect(x0 + 9, y0, x0 + 9, y0 + 7, col)


def capbtn(kind, state):
    c = Canvas(BW, BH)
    col = GLYPH
    if state == "pressed":
        if kind == "close":
            c.rect(0, 0, BW - 1, BH - 1, CLOSE_RED)
            col = (255, 255, 255)
        else:
            c.rect(0, 0, BW - 1, BH - 1, mix(CAP_ACT, (0, 0, 0), 0.10))
    elif state == "disabled":
        col = GLYPH_OFF
    glyph(c, kind, col)
    return c.render(CAP_ACT)


def capbtns():
    cells = []
    for kind in ("close", "min", "max", "restore"):
        for state in ("normal", "pressed", "disabled"):
            cells.append(capbtn(kind, state))
    return strip(cells)


# ---- push buttons: 16x23 nine-grid cells (margins 4), radius 4 ----

def pushbtn(state):
    w, h = 16, 23
    c = Canvas(w, h)
    if state == "default":
        border, bottom, fill = ACCENT_DARK, (0, 58, 120), ACCENT
    elif state == "pressed":
        border, bottom, fill = (229, 229, 229), (229, 229, 229), (245, 245, 245)
    elif state == "disabled":
        border, bottom, fill = (229, 229, 229), (229, 229, 229), (249, 249, 249)
    else:
        border, bottom, fill = (229, 229, 229), (204, 204, 204), (253, 253, 253)
    # the darker bottom edge: the border shape drawn twice, one pixel apart
    c.rrect(0, 1, w - 1, h - 1, 4, bottom)
    c.rrect(0, 0, w - 1, h - 2, 4, border)
    c.rrect(1, 1, w - 2, h - 2, 3, fill)
    return c.render(FACE)


def pushbtns():
    return strip([pushbtn(s) for s in ("normal", "pressed", "default", "disabled")])


# ---- check boxes and radio buttons: 13x13 cells ----

def checkcell(state, mark):
    c = Canvas(13, 13)
    disabled = state == "disabled"
    if mark is None:
        border = CTRL_BORDER if not disabled else (200, 200, 200)
        fill = {"pressed": (229, 229, 229), "disabled": (249, 249, 249)}.get(state, (251, 251, 251))
        c.rrect(0, 0, 12, 12, 3, border)
        c.rrect(1, 1, 11, 11, 2, fill)
        return c.render(FACE)
    fill = {"pressed": ACCENT_PRESSED, "disabled": (200, 200, 200)}.get(state, ACCENT)
    c.rrect(0, 0, 12, 12, 3, fill)
    white = (255, 255, 255)
    if mark == "check":
        c.line([(3, 6.5), (5.2, 8.7), (9.5, 4)], 1.3, white)
    else:
        c.rect(3, 6, 9, 6, white)
    return c.render(FACE)


def radiocell(state, dot):
    c = Canvas(13, 13)
    disabled = state == "disabled"
    if not dot:
        border = CTRL_BORDER if not disabled else (200, 200, 200)
        fill = {"pressed": (229, 229, 229), "disabled": (249, 249, 249)}.get(state, (251, 251, 251))
        c.ellipse(0, 0, 12, 12, border)
        c.ellipse(1, 1, 11, 11, fill)
        return c.render(FACE)
    ring = {"pressed": ACCENT_PRESSED, "disabled": (200, 200, 200)}.get(state, ACCENT)
    c.ellipse(0, 0, 12, 12, ring)
    # a white dot in the accent ring, smaller while pressed
    r = 2 if state == "pressed" else 3
    c.ellipse(6 - r, 6 - r, 6 + r, 6 + r, (255, 255, 255))
    return c.render(FACE)


def checks():
    boxes = [checkcell(s, m) for m in (None, "check", "mixed")
             for s in ("normal", "pressed", "disabled")]
    radios = [radiocell(s, d) for d in (False, True) for s in ("normal", "pressed", "disabled")]
    return rows([strip(boxes), strip(radios)])


# ---- small parts: menu highlight (12x12, radius 4) and scroll thumbs ----

def menusel(bg, col):
    c = Canvas(12, 12)
    c.rrect(0, 0, 11, 11, 4, col)
    return c.render(bg)


def pill(col):
    """8x8 nine-grid cell (margins 3) of a rounded scroll bar thumb."""
    c = Canvas(8, 8)
    c.rrect(0, 0, 7, 7, 3, col)
    return c.render(TRACK)


def misc():
    # menu popup item, menu bar item (on the caption-coloured bar), thumb
    return strip([menusel(MENU, (233, 233, 233)), menusel(CAP_ACT, (222, 226, 232)),
                  pill(THUMB)])


def main():
    gen_redexp.OUT = OUT
    os.makedirs(OUT, exist_ok=True)
    fr = strip([frame(CAP_ACT, (112, 112, 112)), frame(CAP_INACT, (170, 170, 170))])
    gen_redexp.save(fr, "FRAME.BMP")
    gen_redexp.save(capbtns(), "CAPBTN.BMP")
    gen_redexp.save(pushbtns(), "BUTTON.BMP")
    gen_redexp.save(checks(), "CHECKS.BMP")
    gen_redexp.save(misc(), "MISC.BMP")


if __name__ == "__main__":
    main()
