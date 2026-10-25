/*
 * glyphs.c - built-in glyphs (caption buttons, arrows, check marks, radio).
 *
 * Original pixel art at the Win95 nominal sizes, scaled nearest-neighbour
 * for other sizes; arrows are computed from the box size. Mask characters:
 *   '#'  colour 1
 *   '2'  colour 2
 *   'x'  colour 1 in the top-left half, colour 2 in the bottom-right half
 *   '.'  untouched
 */
#include "rt_theme.h"

typedef struct Mask {
    rt_u8 w, h;
    const char *const *rows;
} Mask;

static const char *const m_close[] = {
    "##....##",
    ".##..##.",
    "..####..",
    "...##...",
    "..####..",
    ".##..##.",
    "##....##"
};
static const char *const m_min[] = {
    "........",
    "........",
    "........",
    "........",
    "........",
    "######..",
    "######.."
};
static const char *const m_max[] = {
    "#########",
    "#########",
    "#.......#",
    "#.......#",
    "#.......#",
    "#.......#",
    "#.......#",
    "#.......#",
    "#########"
};
static const char *const m_restore[] = {
    "...######",
    "...######",
    "...#....#",
    "######..#",
    "######..#",
    "#....####",
    "#....#...",
    "#....#...",
    "######..."
};
static const char *const m_check[] = {
    "......#",
    ".....##",
    "#...###",
    "##.###.",
    "#####..",
    ".###...",
    "..#...."
};
static const char *const m_subarrow[] = {
    "#...",
    "##..",
    "###.",
    "####",
    "###.",
    "##..",
    "#..."
};
static const char *const m_radiodot[] = {
    ".##.",
    "####",
    "####",
    ".##."
};
static const char *const m_radiofill[] = {
    "............",
    "............",
    "....####....",
    "...######...",
    "..########..",
    "..########..",
    "..########..",
    "..########..",
    "...######...",
    "....####....",
    "............",
    "............"
};
static const char *const m_radioouter[] = {
    "....xxxx....",
    "..xx....xx..",
    ".x........x.",
    ".x........x.",
    "x..........x",
    "x..........x",
    "x..........x",
    "x..........x",
    ".x........x.",
    ".x........x.",
    "..xx....xx..",
    "....xxxx...."
};
static const char *const m_radioinner[] = {
    "............",
    "....xxxx....",
    "..xx....xx..",
    "..x......x..",
    ".x........x.",
    ".x........x.",
    ".x........x.",
    ".x........x.",
    "..x......x..",
    "..xx....xx..",
    "....xxxx....",
    "............"
};
static const char *const m_sizegrip[] = {
    "..........#2",
    ".........#22",
    "........#22.",
    ".......#22.#",
    "......#22.#2",
    ".....#22.#22",
    "....#22.#22.",
    "...#22.#22.#",
    "..#22.#22.#2",
    ".#22.#22.#22",
    "#22.#22.#22.",
    "22.#22.#22.."
};

static const Mask masks[RT_GLYPH_COUNT] = {
    { 0, 0, 0 },
    { 8, 7, m_close },
    { 8, 7, m_min },
    { 9, 9, m_max },
    { 9, 9, m_restore },
    { 0, 0, 0 },            /* arrows are procedural */
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 7, 7, m_check },
    { 7, 7, m_check },
    { 4, 7, m_subarrow },
    { 4, 4, m_radiodot },
    { 12, 12, m_radiofill },
    { 12, 12, m_radioouter },
    { 12, 12, m_radioinner },
    { 12, 12, m_sizegrip }
};

static int is_arrow(int g)
{
    return g >= RT_GLYPH_ARROWUP && g <= RT_GLYPH_ARROWRIGHT;
}

void rt_glyph_size(int glyph, const RtRect *r, int *w, int *h)
{
    if (is_arrow(glyph)) {
        int box = RT_RECT_W(r) < RT_RECT_H(r) ? RT_RECT_W(r) : RT_RECT_H(r);
        int base = (box * 7 + 8) / 16;
        if (base < 3)
            base = 3;
        if (!(base & 1))
            base--;
        if (glyph == RT_GLYPH_ARROWUP || glyph == RT_GLYPH_ARROWDOWN) {
            *w = base;
            *h = (base + 1) / 2;
        } else {
            *w = (base + 1) / 2;
            *h = base;
        }
        return;
    }
    if (glyph <= 0 || glyph >= RT_GLYPH_COUNT) {
        *w = *h = 0;
        return;
    }
    *w = masks[glyph].w;
    *h = masks[glyph].h;
}

static void hrun(RtSurface *s, int x0, int x1, int y, RtColor c)
{
    RtRect r;
    if (x1 <= x0)
        return;
    rt_rect_set(&r, x0, y, x1, y + 1);
    bk_fill(s, &r, c);
}

static void draw_arrow(RtSurface *s, int glyph, int x, int y, int w, int h, RtColor c)
{
    int i;
    switch (glyph) {
    case RT_GLYPH_ARROWUP:
        for (i = 0; i < h; i++)
            hrun(s, x + (h - 1 - i), x + w - (h - 1 - i), y + i, c);
        break;
    case RT_GLYPH_ARROWDOWN:
        for (i = 0; i < h; i++)
            hrun(s, x + i, x + w - i, y + i, c);
        break;
    case RT_GLYPH_ARROWLEFT:
        for (i = 0; i < h; i++) {
            int d = i < w ? w - 1 - i : i - (w - 1);
            hrun(s, x + d, x + w, y + i, c);
        }
        break;
    case RT_GLYPH_ARROWRIGHT:
        for (i = 0; i < h; i++) {
            int d = i < w ? i + 1 : h - i;
            hrun(s, x, x + d, y + i, c);
        }
        break;
    }
}

void rt_glyph_draw(RtSurface *s, int glyph, const RtRect *r, int w, int h, RtColor c1, RtColor c2)
{
    const Mask *m;
    int x0, y0, x, y;
    if (w <= 0 || h <= 0)
        return;
    x0 = r->left + (RT_RECT_W(r) - w) / 2;
    y0 = r->top + (RT_RECT_H(r) - h) / 2;
    if (is_arrow(glyph)) {
        draw_arrow(s, glyph, x0, y0, w, h, c1);
        return;
    }
    if (glyph <= 0 || glyph >= RT_GLYPH_COUNT || !masks[glyph].rows)
        return;
    m = &masks[glyph];
    for (y = 0; y < h; y++) {
        const char *row = m->rows[y * m->h / h];
        int start = -1;
        RtColor cur = 0;
        for (x = 0; x <= w; x++) {
            int mx = x * m->w / w;
            char ch = x < w ? row[mx] : '.';
            RtColor c = 0;
            int on = 1;
            if (ch == '#')
                c = c1;
            else if (ch == '2')
                c = c2;
            else if (ch == 'x')
                c = (mx + y * m->h / h < m->w - 1) ? c1 : c2;
            else
                on = 0;
            if (start >= 0 && (!on || c != cur)) {
                hrun(s, x0 + start, x0 + x, y0 + y, cur);
                start = -1;
            }
            if (on && start < 0) {
                start = x;
                cur = c;
            }
        }
    }
}
