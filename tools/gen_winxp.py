#!/usr/bin/env python3
"""Generates the bitmaps of themes/WINXP, "RedmondExperience" (original
artwork in the style of the Windows XP "Luna" look; public domain).

Everything is drawn procedurally at 4x and scaled down, so rounded shapes get
anti-aliased edges. Edge pixels are blended with the colour the part sits on
(the caption blue for caption buttons, the dialog face for controls); fully
transparent pixels become the colour key magenta (255,0,255), and the
corners of the window frame become yellow (255,255,0), which THEME.INI
recolours to the desktop colour.

Writes 24-bpp Windows 3.x DIBs (bottom-up, BI_RGB): exact colours on
high-colour displays (the look this theme is made for); GDI maps them to the
nearest colours on 256- and 16-colour displays.

  python3 tools/gen_winxp.py             writes themes/WINXP/*.BMP
"""
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "themes", "WINXP")
KEY = (255, 0, 255)
CORNER = (255, 255, 0)
S = 4  # supersampling factor

# ---- palette (Luna blue) ----
FACE = (236, 233, 216)          # BtnFace
CAP_ACT = (0, 84, 227)
CAP_INACT = (122, 150, 223)
BTN_BORDER = (0, 60, 116)
CHECK_BORDER = (28, 81, 128)
CHECK_GREEN = (33, 161, 33)
SCROLL_ARROW = (77, 97, 133)


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def grad(stops, n):
    """List of n colours through stops [(pos 0..1, colour), ...]."""
    out = []
    for i in range(n):
        p = i / (n - 1) if n > 1 else 0
        for j in range(len(stops) - 1):
            p0, c0 = stops[j]
            p1, c1 = stops[j + 1]
            if p <= p1 or j == len(stops) - 2:
                t = 0 if p1 == p0 else min(1, max(0, (p - p0) / (p1 - p0)))
                out.append(mix(c0, c1, t))
                break
    return out


class Canvas:
    """A supersampled RGBA canvas for one cell."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.im = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.im)

    def box(self, x0, y0, x1, y1):
        """Cell pixel box (inclusive pixel coordinates) at 4x."""
        return (x0 * S, y0 * S, (x1 + 1) * S - 1, (y1 + 1) * S - 1)

    def rrect(self, x0, y0, x1, y1, r, fill):
        self.d.rounded_rectangle(self.box(x0, y0, x1, y1), radius=r * S, fill=fill + (255,))

    def vgrad_rrect(self, x0, y0, x1, y1, r, stops):
        """Rounded rectangle filled with a vertical gradient."""
        h = (y1 - y0 + 1) * S
        g = Image.new("RGBA", self.im.size, (0, 0, 0, 0))
        gd = ImageDraw.Draw(g)
        cols = grad(stops, h)
        for i, c in enumerate(cols):
            gd.line([(x0 * S, y0 * S + i), ((x1 + 1) * S - 1, y0 * S + i)], fill=c + (255,))
        m = Image.new("L", self.im.size, 0)
        ImageDraw.Draw(m).rounded_rectangle(self.box(x0, y0, x1, y1), radius=r * S, fill=255)
        self.im.paste(g, (0, 0), m)

    def ellipse(self, x0, y0, x1, y1, fill):
        self.d.ellipse(self.box(x0, y0, x1, y1), fill=fill + (255,))

    def vgrad_ellipse(self, x0, y0, x1, y1, stops):
        h = (y1 - y0 + 1) * S
        g = Image.new("RGBA", self.im.size, (0, 0, 0, 0))
        gd = ImageDraw.Draw(g)
        for i, c in enumerate(grad(stops, h)):
            gd.line([(x0 * S, y0 * S + i), ((x1 + 1) * S - 1, y0 * S + i)], fill=c + (255,))
        m = Image.new("L", self.im.size, 0)
        ImageDraw.Draw(m).ellipse(self.box(x0, y0, x1, y1), fill=255)
        self.im.paste(g, (0, 0), m)

    def line(self, pts, width, fill):
        """pts in cell pixel centres."""
        p = [((x + 0.5) * S, (y + 0.5) * S) for x, y in pts]
        self.d.line(p, fill=fill + (255,), width=int(width * S), joint="curve")

    def rect(self, x0, y0, x1, y1, fill):
        self.d.rectangle(self.box(x0, y0, x1, y1), fill=fill + (255,))

    def render(self, bg, key=KEY):
        """Scales down; composites over bg; transparent pixels -> key."""
        small = self.im.resize((self.w, self.h), Image.LANCZOS)
        out = Image.new("RGB", (self.w, self.h), key)
        px, op = small.load(), out.load()
        for y in range(self.h):
            for x in range(self.w):
                r, g, b, a = px[x, y]
                if a < 24:
                    continue
                if a > 232:
                    op[x, y] = (r, g, b)
                else:
                    t = a / 255.0
                    op[x, y] = mix(bg, (r, g, b), t)
        return out


def strip(cells):
    w = sum(c.width for c in cells)
    h = max(c.height for c in cells)
    im = Image.new("RGB", (w, h), KEY)
    x = 0
    for c in cells:
        im.paste(c, (x, 0))
        x += c.width
    return im


def rows(rowlist):
    w = max(r.width for r in rowlist)
    h = sum(r.height for r in rowlist)
    im = Image.new("RGB", (w, h), KEY)
    y = 0
    for r in rowlist:
        im.paste(r, (0, y))
        y += r.height
    return im


def save(im, name):
    path = os.path.join(OUT, name)
    im.convert("RGB").save(path)
    size = os.path.getsize(path)
    assert size <= 60000, "%s too large (%d bytes)" % (name, size)
    print("%-12s %3dx%-3d %6d bytes" % (name, im.width, im.height, size))


# ---- caption band: 1 x 22 strips, active and inactive ----

def caption_profile(base, light, dark, n=22):
    """Luna caption: bright top edge, a light band, the body, a darker foot."""
    return grad([(0.00, mix(light, (255, 255, 255), 0.25)),
                 (0.06, light),
                 (0.20, mix(base, light, 0.45)),
                 (0.42, base),
                 (0.70, mix(base, light, 0.12)),
                 (0.90, mix(base, dark, 0.35)),
                 (1.00, dark)], n)


def caption():
    act = caption_profile(CAP_ACT, (61, 149, 255), (0, 50, 170))
    ina = caption_profile(CAP_INACT, (157, 185, 235), (98, 120, 190))
    im = Image.new("RGB", (8, 22), KEY)
    for y in range(22):
        for x in range(4):
            im.putpixel((x, y), act[y])
            im.putpixel((x + 4, y), ina[y])
    return im


# ---- window frame: 12x12 nine-grid cells (margins 4), active and inactive ----

def frame(base, outer, inner_light):
    im = Image.new("RGB", (12, 12), base)
    for i in range(12):
        im.putpixel((i, 0), outer)
        im.putpixel((i, 11), outer)
        im.putpixel((0, i), outer)
        im.putpixel((11, i), outer)
    # a lighter line inside the outer one on the top, like the caption's edge
    for i in range(1, 11):
        im.putpixel((i, 1), inner_light)
    # rounded top corners (radius 3); the yellow pixels become the desktop
    for (x, y) in ((0, 0), (1, 0), (2, 0), (0, 1), (0, 2)):
        im.putpixel((x, y), CORNER)
        im.putpixel((11 - x, y), CORNER)
    for (x, y) in ((1, 1), (3, 0), (0, 3)):
        im.putpixel((x, y), outer)
        im.putpixel((11 - x, y), outer)
    im.putpixel((2, 1), inner_light)
    im.putpixel((9, 1), inner_light)
    im.putpixel((1, 2), outer)
    im.putpixel((10, 2), outer)
    return im


# ---- caption buttons: 16x16 cells ----

BW = 16


def glyph(c, kind, col):
    if kind == "close":
        c.line([(4, 4), (11, 11)], 2, col)
        c.line([(11, 4), (4, 11)], 2, col)
    elif kind == "min":
        c.rect(4, 10, 9, 11, col)
    elif kind == "max":
        c.rect(3, 3, 12, 5, col)
        c.rect(3, 3, 3, 12, col)
        c.rect(12, 3, 12, 12, col)
        c.rect(3, 12, 12, 12, col)
    elif kind == "restore":
        # back window
        c.rect(6, 2, 13, 3, col)
        c.rect(13, 2, 13, 9, col)
        c.rect(11, 9, 13, 9, col)
        c.rect(6, 2, 6, 4, col)
        # front window
        c.rect(2, 6, 9, 8, col)
        c.rect(2, 6, 2, 13, col)
        c.rect(9, 6, 9, 13, col)
        c.rect(2, 13, 9, 13, col)


def capbtn(kind, state):
    red = kind == "close"
    c = Canvas(BW, BW)
    if red:
        top, mid, bot = (232, 130, 100), (225, 85, 50), (200, 60, 25)
        if state == "pressed":
            top, mid, bot = (190, 70, 40), (205, 75, 40), (215, 110, 80)
    else:
        top, mid, bot = (120, 170, 255), (45, 115, 240), (30, 90, 225)
        if state == "pressed":
            top, mid, bot = (20, 60, 180), (30, 80, 210), (60, 120, 240)
    if state == "disabled":
        top, mid, bot = [mix(x, CAP_ACT, 0.55) for x in (top, mid, bot)]
    border = (255, 255, 255) if state != "disabled" else mix((255, 255, 255), CAP_ACT, 0.5)
    c.rrect(0, 0, BW - 1, BW - 1, 3, border)
    c.vgrad_rrect(1, 1, BW - 2, BW - 2, 2, [(0, top), (0.5, mid), (1, bot)])
    gcol = (255, 255, 255) if state != "disabled" else mix((255, 255, 255), CAP_ACT, 0.45)
    glyph(c, kind, gcol)
    return c.render(CAP_ACT)


def capbtns():
    cells = []
    for kind in ("close", "min", "max", "restore"):
        for state in ("normal", "pressed", "disabled"):
            cells.append(capbtn(kind, state))
    return strip(cells)


# ---- push buttons: 16x23 nine-grid cells (margins 4) ----

def pushbtn(state):
    w, h = 16, 23
    c = Canvas(w, h)
    border = BTN_BORDER if state != "disabled" else (201, 199, 186)
    c.rrect(0, 0, w - 1, h - 1, 3, border)
    if state == "pressed":
        stops = [(0, (205, 202, 195)), (0.15, (226, 226, 218)), (1, (241, 241, 237))]
    elif state == "disabled":
        stops = [(0, (245, 244, 234)), (1, (245, 244, 234))]
    else:
        stops = [(0, (255, 255, 255)), (0.6, (244, 243, 238)), (0.88, (236, 235, 230)),
                 (1, (214, 208, 197))]
    if state == "default":
        # the blue glow of the default button
        c.vgrad_rrect(1, 1, w - 2, h - 2, 2, [(0, (206, 231, 255)), (1, (105, 130, 238))])
        c.vgrad_rrect(3, 3, w - 4, h - 4, 1, stops)
    else:
        c.vgrad_rrect(1, 1, w - 2, h - 2, 2, stops)
    return c.render(FACE)


def pushbtns():
    return strip([pushbtn(s) for s in ("normal", "pressed", "default", "disabled")])


# ---- check boxes and radio buttons: 13x13 cells ----

def checkcell(state, mark):
    c = Canvas(13, 13)
    disabled = state == "disabled"
    c.rect(0, 0, 12, 12, CHECK_BORDER if not disabled else (202, 200, 187))
    if disabled:
        stops = [(0, (255, 255, 255)), (1, (255, 255, 255))]
    elif state == "pressed":
        stops = [(0, (176, 176, 167)), (1, (227, 226, 219))]
    else:
        stops = [(0, (220, 220, 215)), (1, (255, 255, 255))]
    c.vgrad_rrect(1, 1, 11, 11, 0, stops)
    green = CHECK_GREEN if not disabled else (202, 200, 187)
    if mark == "check":
        c.line([(3, 6), (5, 8), (9, 3)], 2.2, green)
    elif mark == "mixed":
        c.rect(3, 3, 9, 9, mix(green, (255, 255, 255), 0.15))
    return c.render(FACE)


def radiocell(state, dot):
    c = Canvas(13, 13)
    disabled = state == "disabled"
    c.ellipse(0, 0, 12, 12, CHECK_BORDER if not disabled else (202, 200, 187))
    if disabled:
        stops = [(0, (255, 255, 255)), (1, (255, 255, 255))]
    elif state == "pressed":
        stops = [(0, (176, 176, 167)), (1, (227, 226, 219))]
    else:
        stops = [(0, (220, 220, 215)), (1, (255, 255, 255))]
    c.vgrad_ellipse(1, 1, 11, 11, stops)
    if dot:
        if disabled:
            c.ellipse(4, 4, 8, 8, (202, 200, 187))
        else:
            c.vgrad_ellipse(4, 4, 8, 8, [(0, (85, 210, 85)), (1, (25, 140, 25))])
    return c.render(FACE)


def checks():
    boxes = [checkcell(s, m) for m in (None, "check", "mixed")
             for s in ("normal", "pressed", "disabled")]
    radios = [radiocell(s, d) for d in (False, True) for s in ("normal", "pressed", "disabled")]
    return rows([strip(boxes), strip(radios)])


# ---- scroll bars: 16x16 button cells (nine-grid, margins 3), grips, arrows ----

def scrollbtn(state):
    c = Canvas(16, 16)
    if state == "pressed":
        border, stops = (131, 158, 224), [(0, (110, 142, 241)), (1, (140, 170, 247))]
    elif state == "disabled":
        border, stops = (232, 233, 228), [(0, (244, 244, 240)), (1, (244, 244, 240))]
    else:
        border, stops = (176, 197, 247), [(0, (225, 234, 254)), (0.5, (200, 216, 253)),
                                           (1, (181, 202, 251))]
    c.rrect(0, 0, 15, 15, 2, (255, 255, 255) if state != "disabled" else (248, 248, 244))
    c.rrect(1, 1, 14, 14, 2, border)
    c.vgrad_rrect(2, 2, 13, 13, 1, stops)
    return c.render((245, 244, 239))


def thumb(horizontal):
    c = Canvas(16, 16)
    stops = [(0, (202, 219, 253)), (0.5, (186, 205, 251)), (1, (171, 194, 250))]
    c.rrect(0, 0, 15, 15, 2, (255, 255, 255))
    c.rrect(1, 1, 14, 14, 2, (152, 180, 245))
    c.vgrad_rrect(2, 2, 13, 13, 1, stops)
    im = c.render((245, 244, 239))
    if horizontal:
        im = im.transpose(Image.Transpose.TRANSPOSE)
    return im


def grip(horizontal):
    """8x8: four light/blue line pairs."""
    im = Image.new("RGB", (8, 8), KEY)
    for i in range(4):
        y = 2 * i
        for x in range(8):
            im.putpixel((x, y), (238, 244, 254))
            im.putpixel((x, y + 1), (140, 176, 248))
    if horizontal:
        im = im.transpose(Image.Transpose.TRANSPOSE)
    return im


def arrow(direction, col):
    c = Canvas(9, 9)
    pts = [(1, 6), (4, 3), (7, 6)]
    c.line(pts, 1.8, col)
    im = c.render((200, 216, 253))
    rot = {"up": None, "down": Image.Transpose.FLIP_TOP_BOTTOM,
           "left": Image.Transpose.TRANSPOSE, "right": Image.Transpose.TRANSVERSE}[direction]
    if rot is not None:
        im = im.transpose(rot)
    return im


def scroll():
    buttons = strip([scrollbtn(s) for s in ("normal", "pressed", "disabled")]
                    + [thumb(False), thumb(True)])
    small = strip([grip(False), grip(True)]
                  + [arrow(d, SCROLL_ARROW) for d in ("up", "down", "left", "right")]
                  + [arrow(d, (201, 199, 186)) for d in ("up", "down", "left", "right")])
    # buttons row (16 high), then grips and arrows (9 high, cells 8 and 9 wide)
    return rows([buttons, small])


def main():
    os.makedirs(OUT, exist_ok=True)
    save(caption(), "CAPTION.BMP")
    fr = Image.new("RGB", (24, 12))
    fr.paste(frame((0, 85, 234), (0, 25, 140), (40, 130, 255)), (0, 0))
    fr.paste(frame((122, 150, 223), (90, 110, 180), (160, 185, 240)), (12, 0))
    save(fr, "FRAME.BMP")
    save(capbtns(), "CAPBTN.BMP")
    save(pushbtns(), "BUTTON.BMP")
    save(checks(), "CHECKS.BMP")
    save(scroll(), "SCROLL.BMP")


if __name__ == "__main__":
    main()
