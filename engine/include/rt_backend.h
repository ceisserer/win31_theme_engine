/*
 * rt_backend.h - the drawing interface between the renderer and a platform.
 *
 * backend/gdi16.c implements it with Win16 GDI, host/backend_sw.c with a
 * software rasteriser (tests, previews). The renderer only uses these calls.
 */
#ifndef RT_BACKEND_H
#define RT_BACKEND_H

#include "rt_types.h"

typedef struct RtSurface RtSurface;     /* defined by each backend */
typedef void *RtFontH;
typedef void *RtImageH;

typedef struct RtFontSpec {
    char   face[32];
    rt_s16 points;
    rt_s16 weight;          /* 400 normal, 700 bold */
    rt_u8  italic;
    rt_u8  underline;
} RtFontSpec;

/* A decoded Windows DIB. 'info' points at a BITMAPINFOHEADER followed by its
 * colour table, 'bits' at the pixel rows (bottom-up, rows padded to 4 bytes),
 * both usually inside the buffer of the loaded .BMP file. */
typedef struct RtDib {
    void   *info;
    rt_u8  *bits;
    rt_s16  w, h;
    rt_u16  bpp;            /* 1, 4, 8 or 24 */
    rt_u16  ncolors;        /* entries in the colour table */
    rt_u8  *palette;        /* RGBQUADs (B, G, R, 0), inside 'info' */
    rt_u32  stride;
} RtDib;

void bk_fill(RtSurface *s, const RtRect *r, RtColor c);
/* 50% checkerboard of c1/c2, pattern anchored at the surface origin. */
void bk_dither(RtSurface *s, const RtRect *r, RtColor c1, RtColor c2);
/* Dotted focus rectangle (XOR on GDI). */
void bk_focus(RtSurface *s, const RtRect *r);

RtFontH bk_font_create(const RtFontSpec *spec);
void    bk_font_destroy(RtFontH f);
int     bk_text_width(RtSurface *s, RtFontH f, const char *text, int len);
int     bk_text_height(RtSurface *s, RtFontH f);
/* Transparent text with its cell's top-left corner at (x, y), clipped. */
void    bk_text(RtSurface *s, RtFontH f, int x, int y, const RtRect *clip,
                const char *text, int len, RtColor c);

RtImageH bk_image_create(const RtDib *dib, int keyed, RtColor key);
void     bk_image_destroy(RtImageH img);
/* Copies src (image coordinates) to dst, stretching if the sizes differ;
 * pixels of the key colour are left untouched if the image is keyed. */
void     bk_image_draw(RtSurface *s, RtImageH img, const RtRect *src, const RtRect *dst);

/* Restricts drawing to 'clip' (may be NULL) minus 'exclude' (may be NULL)
 * until the matching bk_clip_pop. Calls nest. */
void bk_clip_push(RtSurface *s, const RtRect *clip, const RtRect *exclude);
void bk_clip_pop(RtSurface *s);

#endif
