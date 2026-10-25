/*
 * gdi16.c - rt_backend.h on Win16 GDI.
 *
 * Called with the engine's patches suspended (ncpaint.c), so plain GDI
 * calls reach the real functions. GDI resources are scarce (64 KB heap for
 * the whole system): brushes are cached, one memory DC is shared.
 */
#include "rt16.h"

#define BRUSH_CACHE 8

typedef struct Img {
    HBITMAP bmp;
    HBITMAP mask;       /* 1 where transparent, or 0 */
    int w, h;
} Img;

static struct { COLORREF c; HBRUSH b; } brushes[BRUSH_CACHE];
static int   brush_next;
static HDC   mem_dc, mask_dc;
static HBRUSH dither_brush;
static HBITMAP dither_bmp;

void gdi_init(void)
{
    static const WORD pattern[8] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA };
    HDC screen = GetDC(NULL);
    mem_dc = CreateCompatibleDC(screen);
    mask_dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    dither_bmp = CreateBitmap(8, 8, 1, 1, pattern);
    dither_brush = CreatePatternBrush(dither_bmp);
}

void gdi_shutdown(void)
{
    int i;
    for (i = 0; i < BRUSH_CACHE; i++) {
        if (brushes[i].b)
            DeleteObject(brushes[i].b);
        brushes[i].b = 0;
    }
    if (dither_brush)
        DeleteObject(dither_brush);
    if (dither_bmp)
        DeleteObject(dither_bmp);
    if (mem_dc)
        DeleteDC(mem_dc);
    if (mask_dc)
        DeleteDC(mask_dc);
    dither_brush = 0;
    dither_bmp = 0;
    mem_dc = mask_dc = 0;
}

void gdi_surface(RtSurface FAR *s, HDC hdc)
{
    s->hdc = hdc;
    s->depth = 0;
}

static HBRUSH brush(COLORREF c)
{
    int i;
    for (i = 0; i < BRUSH_CACHE; i++)
        if (brushes[i].b && brushes[i].c == c)
            return brushes[i].b;
    i = brush_next;
    brush_next = (brush_next + 1) % BRUSH_CACHE;
    if (brushes[i].b)
        DeleteObject(brushes[i].b);
    brushes[i].c = c;
    brushes[i].b = CreateSolidBrush(c);
    return brushes[i].b;
}

void bk_fill(RtSurface *s, const RtRect *r, RtColor c)
{
    HBRUSH old = SelectObject(s->hdc, brush((COLORREF)c));
    PatBlt(s->hdc, r->left, r->top, RT_RECT_W(r), RT_RECT_H(r), PATCOPY);
    SelectObject(s->hdc, old);
}

void bk_dither(RtSurface *s, const RtRect *r, RtColor c1, RtColor c2)
{
    COLORREF ot = SetTextColor(s->hdc, (COLORREF)c1);
    COLORREF ob = SetBkColor(s->hdc, (COLORREF)c2);
    HBRUSH old = SelectObject(s->hdc, dither_brush);
    PatBlt(s->hdc, r->left, r->top, RT_RECT_W(r), RT_RECT_H(r), PATCOPY);
    SelectObject(s->hdc, old);
    SetTextColor(s->hdc, ot);
    SetBkColor(s->hdc, ob);
}

void bk_focus(RtSurface *s, const RtRect *r)
{
    RECT rc;
    SetRect(&rc, r->left, r->top, r->right, r->bottom);
    DrawFocusRect(s->hdc, &rc);
}

RtFontH bk_font_create(const RtFontSpec *spec)
{
    HDC screen = GetDC(NULL);
    int h = -MulDiv(spec->points, GetDeviceCaps(screen, LOGPIXELSY), 72);
    HFONT f;
    ReleaseDC(NULL, screen);
    f = CreateFont(h, 0, 0, 0, spec->weight, spec->italic, spec->underline, 0, ANSI_CHARSET,
                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                   VARIABLE_PITCH | FF_SWISS, spec->face);
    return (RtFontH)f;
}

void bk_font_destroy(RtFontH f)
{
    if (f)
        DeleteObject((HFONT)f);
}

int bk_text_width(RtSurface *s, RtFontH f, const char *text, int len)
{
    HFONT old = 0;
    DWORD ext;
    if (f)
        old = SelectObject(s->hdc, (HFONT)f);
    ext = GetTextExtent(s->hdc, text, len);
    if (f)
        SelectObject(s->hdc, old);
    return LOWORD(ext);
}

int bk_text_height(RtSurface *s, RtFontH f)
{
    TEXTMETRIC tm;
    HFONT old = 0;
    if (f)
        old = SelectObject(s->hdc, (HFONT)f);
    GetTextMetrics(s->hdc, &tm);
    if (f)
        SelectObject(s->hdc, old);
    return tm.tmHeight;
}

void bk_text(RtSurface *s, RtFontH f, int x, int y, const RtRect *clip,
             const char *text, int len, RtColor c)
{
    HFONT old = 0;
    RECT rc;
    int mode = SetBkMode(s->hdc, TRANSPARENT);
    COLORREF oc = SetTextColor(s->hdc, (COLORREF)c);
    if (f)
        old = SelectObject(s->hdc, (HFONT)f);
    SetRect(&rc, clip->left, clip->top, clip->right, clip->bottom);
    ExtTextOut(s->hdc, x, y, ETO_CLIPPED, &rc, text, len, NULL);
    if (f)
        SelectObject(s->hdc, old);
    SetTextColor(s->hdc, oc);
    SetBkMode(s->hdc, mode);
}

RtImageH bk_image_create(const RtDib *d, int keyed, RtColor key)
{
    Img *im = (Img *)rt_alloc(sizeof(Img));
    HDC screen;
    if (!im)
        return 0;
    screen = GetDC(NULL);
    im->w = d->w;
    im->h = d->h;
    im->bmp = CreateDIBitmap(screen, (BITMAPINFOHEADER FAR *)d->info, CBM_INIT, d->bits,
                             (BITMAPINFO FAR *)d->info, DIB_RGB_COLORS);
    ReleaseDC(NULL, screen);
    if (!im->bmp) {
        rt_free(im);
        return 0;
    }
    if (keyed) {
        HBITMAP ob, om;
        im->mask = CreateBitmap(d->w, d->h, 1, 1, NULL);
        ob = SelectObject(mem_dc, im->bmp);
        om = SelectObject(mask_dc, im->mask);
        /* colour -> mono: pixels of the background colour become 1 */
        SetBkColor(mem_dc, (COLORREF)key);
        BitBlt(mask_dc, 0, 0, d->w, d->h, mem_dc, 0, 0, SRCCOPY);
        /* clear the key pixels in the image: dst AND NOT mask */
        SetBkColor(mem_dc, RGB(255, 255, 255));
        SetTextColor(mem_dc, RGB(0, 0, 0));
        BitBlt(mem_dc, 0, 0, d->w, d->h, mask_dc, 0, 0, 0x00220326L);
        SelectObject(mask_dc, om);
        SelectObject(mem_dc, ob);
    }
    return (RtImageH)im;
}

void bk_image_destroy(RtImageH h)
{
    Img *im = (Img *)h;
    if (!im)
        return;
    if (im->bmp)
        DeleteObject(im->bmp);
    if (im->mask)
        DeleteObject(im->mask);
    rt_free(im);
}

void bk_image_draw(RtSurface *s, RtImageH h, const RtRect *src, const RtRect *dst)
{
    Img *im = (Img *)h;
    HBITMAP ob;
    int sw = RT_RECT_W(src), sh = RT_RECT_H(src), dw = RT_RECT_W(dst), dh = RT_RECT_H(dst);
    int mode = SetStretchBltMode(s->hdc, COLORONCOLOR);
    if (im->mask) {
        COLORREF ot = SetTextColor(s->hdc, RGB(0, 0, 0));
        COLORREF ob2 = SetBkColor(s->hdc, RGB(255, 255, 255));
        HBITMAP om = SelectObject(mask_dc, im->mask);
        StretchBlt(s->hdc, dst->left, dst->top, dw, dh, mask_dc, src->left, src->top, sw, sh, SRCAND);
        SelectObject(mask_dc, om);
        SetTextColor(s->hdc, ot);
        SetBkColor(s->hdc, ob2);
        ob = SelectObject(mem_dc, im->bmp);
        StretchBlt(s->hdc, dst->left, dst->top, dw, dh, mem_dc, src->left, src->top, sw, sh, SRCPAINT);
    } else {
        ob = SelectObject(mem_dc, im->bmp);
        StretchBlt(s->hdc, dst->left, dst->top, dw, dh, mem_dc, src->left, src->top, sw, sh, SRCCOPY);
    }
    SelectObject(mem_dc, ob);
    SetStretchBltMode(s->hdc, mode);
}

void bk_clip_push(RtSurface *s, const RtRect *clip, const RtRect *exclude)
{
    SaveDC(s->hdc);
    s->depth++;
    if (clip)
        IntersectClipRect(s->hdc, clip->left, clip->top, clip->right, clip->bottom);
    if (exclude)
        ExcludeClipRect(s->hdc, exclude->left, exclude->top, exclude->right, exclude->bottom);
}

void bk_clip_pop(RtSurface *s)
{
    if (s->depth > 0) {
        RestoreDC(s->hdc, -1);
        s->depth--;
    }
}
