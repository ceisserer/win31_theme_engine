/* backend_sw.c - software implementation of rt_backend.h for host builds. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rt_backend.h"
#include "backend_sw.h"
#include "font_data.h"

typedef struct SwFont {
    int bold;
    int italic;
    int underline;
} SwFont;

typedef struct SwImage {
    int w, h;
    rt_u32 *px;         /* 0x00RRGGBB */
    int keyed;
    rt_u32 key;
} SwImage;

static rt_u32 to_px(RtColor c)
{
    return ((rt_u32)RT_R(c) << 16) | ((rt_u32)RT_G(c) << 8) | RT_B(c);
}

RtSurface *sw_surface_create(int w, int h, RtColor bg)
{
    RtSurface *s = (RtSurface *)calloc(1, sizeof(RtSurface));
    int i;
    s->w = w;
    s->h = h;
    s->px = (rt_u32 *)malloc((size_t)w * h * sizeof(rt_u32));
    for (i = 0; i < w * h; i++)
        s->px[i] = to_px(bg);
    return s;
}

void sw_surface_free(RtSurface *s)
{
    free(s->px);
    free(s);
}

void sw_set_origin(RtSurface *s, int x, int y)
{
    s->ox = x;
    s->oy = y;
}

/* Visible: inside the surface and every pushed clip, outside every
 * pushed exclusion. Coordinates are surface-relative (origin applied). */
static int visible(const RtSurface *s, int x, int y)
{
    int i;
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return 0;
    for (i = 0; i < s->depth; i++) {
        const SwClip *c = &s->clips[i];
        if (c->has_clip && !(x >= c->clip.left && x < c->clip.right && y >= c->clip.top && y < c->clip.bottom))
            return 0;
        if (c->has_excl && x >= c->excl.left && x < c->excl.right && y >= c->excl.top && y < c->excl.bottom)
            return 0;
    }
    return 1;
}

static void put(RtSurface *s, int x, int y, rt_u32 p)
{
    x += s->ox;
    y += s->oy;
    if (visible(s, x, y))
        s->px[y * s->w + x] = p;
}

static rt_u32 get(const RtSurface *s, int x, int y)
{
    x += s->ox;
    y += s->oy;
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return 0;
    return s->px[y * s->w + x];
}

void bk_fill(RtSurface *s, const RtRect *r, RtColor c)
{
    int x, y;
    rt_u32 p = to_px(c);
    for (y = r->top; y < r->bottom; y++)
        for (x = r->left; x < r->right; x++)
            put(s, x, y, p);
}

void bk_dither(RtSurface *s, const RtRect *r, RtColor c1, RtColor c2)
{
    int x, y;
    rt_u32 p1 = to_px(c1), p2 = to_px(c2);
    for (y = r->top; y < r->bottom; y++)
        for (x = r->left; x < r->right; x++)
            put(s, x, y, ((x + s->ox + y + s->oy) & 1) ? p2 : p1);
}

void bk_focus(RtSurface *s, const RtRect *r)
{
    int x, y;
    for (x = r->left; x < r->right; x++) {
        if (((x - r->left) & 1) == 0) {
            put(s, x, r->top, get(s, x, r->top) ^ 0xFFFFFF);
            put(s, x, r->bottom - 1, get(s, x, r->bottom - 1) ^ 0xFFFFFF);
        }
    }
    for (y = r->top + 1; y < r->bottom - 1; y++) {
        if (((y - r->top) & 1) == 0) {
            put(s, r->left, y, get(s, r->left, y) ^ 0xFFFFFF);
            put(s, r->right - 1, y, get(s, r->right - 1, y) ^ 0xFFFFFF);
        }
    }
}

RtFontH bk_font_create(const RtFontSpec *spec)
{
    SwFont *f = (SwFont *)calloc(1, sizeof(SwFont));
    f->bold = spec->weight >= 600;
    f->italic = spec->italic;
    f->underline = spec->underline;
    return f;
}

void bk_font_destroy(RtFontH f)
{
    free(f);
}

static int advance(const SwFont *f, int ch)
{
    if (ch < 32 || ch > 126)
        ch = '?';
    return font_advance[ch - 32] + (f && f->bold ? 1 : 0);
}

int bk_text_width(RtSurface *s, RtFontH fh, const char *text, int len)
{
    int i, w = 0;
    (void)s;
    for (i = 0; i < len; i++)
        w += advance((const SwFont *)fh, (unsigned char)text[i]);
    return w;
}

int bk_text_height(RtSurface *s, RtFontH f)
{
    (void)s;
    (void)f;
    return font_height;
}

void bk_text(RtSurface *s, RtFontH fh, int x, int y, const RtRect *clip,
             const char *text, int len, RtColor c)
{
    const SwFont *f = (const SwFont *)fh;
    rt_u32 p = to_px(c);
    int i, row, col;
    bk_clip_push(s, clip, 0);
    for (i = 0; i < len; i++) {
        int ch = (unsigned char)text[i];
        if (ch < 32 || ch > 126)
            ch = '?';
        for (row = 0; row < font_height; row++) {
            unsigned bits = font_rows[ch - 32][row];
            int slant = (f && f->italic) ? (font_ascent - row) / 4 : 0;
            for (col = 0; col < 16; col++) {
                if (bits & (1u << col)) {
                    put(s, x + col + slant, y + row, p);
                    if (f && f->bold)
                        put(s, x + col + slant + 1, y + row, p);
                }
            }
        }
        x += advance(f, ch);
    }
    bk_clip_pop(s);
}

RtImageH bk_image_create(const RtDib *d, int keyed, RtColor key)
{
    SwImage *im = (SwImage *)calloc(1, sizeof(SwImage));
    int x, y;
    im->w = d->w;
    im->h = d->h;
    im->keyed = keyed;
    im->key = to_px(key);
    im->px = (rt_u32 *)malloc((size_t)d->w * d->h * sizeof(rt_u32));
    for (y = 0; y < d->h; y++) {
        const rt_u8 *row = d->bits + (rt_u32)(d->h - 1 - y) * d->stride;   /* bottom-up */
        for (x = 0; x < d->w; x++) {
            rt_u32 p;
            if (d->bpp == 24) {
                p = ((rt_u32)row[x * 3 + 2] << 16) | ((rt_u32)row[x * 3 + 1] << 8) | row[x * 3];
            } else {
                int idx;
                const rt_u8 *q;
                if (d->bpp == 8)
                    idx = row[x];
                else if (d->bpp == 4)
                    idx = (x & 1) ? (row[x / 2] & 15) : (row[x / 2] >> 4);
                else
                    idx = (row[x / 8] >> (7 - (x & 7))) & 1;
                if (idx >= d->ncolors)
                    idx = 0;
                q = d->palette + idx * 4;
                p = ((rt_u32)q[2] << 16) | ((rt_u32)q[1] << 8) | q[0];
            }
            im->px[y * d->w + x] = p;
        }
    }
    return im;
}

void bk_image_destroy(RtImageH h)
{
    SwImage *im = (SwImage *)h;
    free(im->px);
    free(im);
}

void bk_image_draw(RtSurface *s, RtImageH h, const RtRect *src, const RtRect *dst)
{
    const SwImage *im = (const SwImage *)h;
    int sw = RT_RECT_W(src), sh = RT_RECT_H(src), dw = RT_RECT_W(dst), dh = RT_RECT_H(dst);
    int x, y;
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;
    for (y = 0; y < dh; y++) {
        int sy = src->top + y * sh / dh;
        for (x = 0; x < dw; x++) {
            int sx = src->left + x * sw / dw;
            rt_u32 p;
            if (sx < 0 || sy < 0 || sx >= im->w || sy >= im->h)
                continue;
            p = im->px[sy * im->w + sx];
            if (im->keyed && p == im->key)
                continue;
            put(s, dst->left + x, dst->top + y, p);
        }
    }
}

void bk_clip_push(RtSurface *s, const RtRect *clip, const RtRect *exclude)
{
    SwClip *c;
    if (s->depth >= SW_MAX_CLIP)
        abort();
    c = &s->clips[s->depth++];
    c->has_clip = clip != 0;
    c->has_excl = exclude != 0;
    if (clip) {
        c->clip = *clip;
        c->clip.left = (rt_s16)(c->clip.left + s->ox);
        c->clip.right = (rt_s16)(c->clip.right + s->ox);
        c->clip.top = (rt_s16)(c->clip.top + s->oy);
        c->clip.bottom = (rt_s16)(c->clip.bottom + s->oy);
    }
    if (exclude) {
        c->excl = *exclude;
        c->excl.left = (rt_s16)(c->excl.left + s->ox);
        c->excl.right = (rt_s16)(c->excl.right + s->ox);
        c->excl.top = (rt_s16)(c->excl.top + s->oy);
        c->excl.bottom = (rt_s16)(c->excl.bottom + s->oy);
    }
}

void bk_clip_pop(RtSurface *s)
{
    if (s->depth > 0)
        s->depth--;
}

/* ---- PNG output (stored deflate blocks, no zlib needed) ---- */

static rt_u32 crc_table[256];

static void crc_init(void)
{
    rt_u32 c;
    int n, k;
    if (crc_table[1])
        return;
    for (n = 0; n < 256; n++) {
        c = (rt_u32)n;
        for (k = 0; k < 8; k++)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static rt_u32 crc(rt_u32 c, const rt_u8 *buf, size_t len)
{
    size_t i;
    c ^= 0xFFFFFFFFu;
    for (i = 0; i < len; i++)
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void be32(rt_u8 *p, rt_u32 v)
{
    p[0] = (rt_u8)(v >> 24);
    p[1] = (rt_u8)(v >> 16);
    p[2] = (rt_u8)(v >> 8);
    p[3] = (rt_u8)v;
}

unsigned char *sw_encode_png(const RtSurface *s, size_t *out_len)
{
    static const rt_u8 sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    size_t raw_len = (size_t)s->h * (1 + (size_t)s->w * 3);
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t idat_len = 2 + raw_len + nblocks * 5 + 4;
    size_t total = 8 + 25 + 12 + idat_len + 12;
    rt_u8 *raw = (rt_u8 *)malloc(raw_len), *png = (rt_u8 *)malloc(total), *q, *idat;
    rt_u32 a = 1, b = 0;
    size_t i, off;
    int x, y;

    crc_init();
    for (y = 0, q = raw; y < s->h; y++) {
        *q++ = 0;   /* filter: none */
        for (x = 0; x < s->w; x++) {
            rt_u32 p = s->px[y * s->w + x];
            *q++ = (rt_u8)(p >> 16);
            *q++ = (rt_u8)(p >> 8);
            *q++ = (rt_u8)p;
        }
    }
    q = png;
    memcpy(q, sig, 8);
    q += 8;
    /* IHDR */
    be32(q, 13);
    memcpy(q + 4, "IHDR", 4);
    be32(q + 8, (rt_u32)s->w);
    be32(q + 12, (rt_u32)s->h);
    q[16] = 8;      /* bit depth */
    q[17] = 2;      /* RGB */
    q[18] = q[19] = q[20] = 0;
    be32(q + 21, crc(0, q + 4, 17));
    q += 25;
    /* IDAT: zlib stream of stored deflate blocks */
    be32(q, (rt_u32)idat_len);
    memcpy(q + 4, "IDAT", 4);
    idat = q + 4;
    q += 8;
    *q++ = 0x78;
    *q++ = 0x01;
    for (off = 0; off < raw_len;) {
        size_t n = raw_len - off > 65535 ? 65535 : raw_len - off;
        *q++ = (rt_u8)(off + n >= raw_len ? 1 : 0);
        *q++ = (rt_u8)(n & 0xFF);
        *q++ = (rt_u8)(n >> 8);
        *q++ = (rt_u8)(~n & 0xFF);
        *q++ = (rt_u8)((~n >> 8) & 0xFF);
        memcpy(q, raw + off, n);
        q += n;
        off += n;
    }
    for (i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    be32(q, (b << 16) | a);
    q += 4;
    be32(q, crc(0, idat, 4 + idat_len));
    q += 4;
    /* IEND */
    be32(q, 0);
    memcpy(q + 4, "IEND", 4);
    be32(q + 8, crc(0, q + 4, 4));
    q += 12;
    free(raw);
    *out_len = (size_t)(q - png);
    return png;
}

int sw_write_png(const RtSurface *s, const char *path)
{
    size_t len;
    unsigned char *png = sw_encode_png(s, &len);
    FILE *f = fopen(path, "wb");
    int rc = -1;
    if (f) {
        rc = fwrite(png, 1, len, f) == len ? 0 : -1;
        fclose(f);
    }
    free(png);
    return rc;
}
