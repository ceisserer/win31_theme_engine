/*
 * rt_theme.h - in-memory theme model and the renderer entry point.
 *
 * A theme is loaded from a directory holding THEME.INI and its bitmaps
 * (docs/THEME_FORMAT.md). Every "<Part>.<State>" section becomes an op; ops
 * reference colours, fonts and images by index into the theme's tables.
 */
#ifndef RT_THEME_H
#define RT_THEME_H

#include "rt_types.h"
#include "rt_parts.h"
#include "rt_backend.h"

#define RT_THEME_FORMAT   1

#define RT_MAX_COLORS   64
#define RT_MAX_FONTS    8
#define RT_MAX_IMAGES   16
#define RT_MAX_OPS      320
#define RT_MAX_LAYERS   6
#define RT_MAX_RECOLOR  8
#define RT_NOCOLOR      0xFF
#define RT_NOFONT       0xFF
#define RT_LOG_SIZE     1024

/* Win 3.1 COLOR_* indices, plus the two Win95 3D colours we emulate. */
#define RT_SYS_BTNFACE      15
#define RT_SYS_BTNSHADOW    16
#define RT_SYS_BTNHIGHLIGHT 20
#define RT_SYS_3DDKSHADOW   21
#define RT_SYS_3DLIGHT      22
#define RT_SYS_COUNT        23

enum RtOpType {
    RT_OP_NONE,
    RT_OP_FILL,         /* c[0] */
    RT_OP_DITHER,       /* c[0], c[1] checkerboard */
    RT_OP_GRADIENT,     /* c[0] -> c[1], style = direction, m[0] = bands */
    RT_OP_EDGE,         /* style = edge preset; c[0..3] = outer TL, outer BR, inner TL, inner BR; c[4] = fill */
    RT_OP_BORDER,       /* c[0], m[0] = width */
    RT_OP_GLYPH,        /* style = glyph, c[0] (c[1] second colour), m[0..1] = size (0 = auto) */
    RT_OP_BITMAP,       /* image, r = source, style = sizing, m = margins */
    RT_OP_TEXT,         /* font, c[0], c[1] = shadow, style = align, r[0..1] = offset */
    RT_OP_FOCUS,        /* dotted focus rectangle */
    RT_OP_LAYERS        /* layers[0..nlayers-1] */
};

enum RtGradDir   { RT_GRAD_H, RT_GRAD_V, RT_GRAD_CONCENTRIC };
enum RtSizing    { RT_SIZE_STRETCH, RT_SIZE_TILE, RT_SIZE_NINEGRID, RT_SIZE_CENTER };
enum RtAlign     { RT_ALIGN_AUTO, RT_ALIGN_LEFT, RT_ALIGN_CENTER, RT_ALIGN_RIGHT };

/* Edge presets (Win95 DrawEdge semantics). */
enum RtEdge {
    RT_EDGE_RAISED,         /* window frame: outer 3DLight/DkShadow, inner Highlight/Shadow */
    RT_EDGE_RAISEDSOFT,     /* push button: outer Highlight/DkShadow, inner 3DLight/Shadow */
    RT_EDGE_SUNKEN,         /* field: outer Shadow/Highlight, inner DkShadow/3DLight */
    RT_EDGE_SUNKENSOFT,     /* pressed push button: outer DkShadow/Highlight, inner Shadow/3DLight */
    RT_EDGE_ETCHED,         /* group box: outer Shadow/Highlight, inner Highlight/Shadow */
    RT_EDGE_BUMP,
    RT_EDGE_RAISEDOUTER,
    RT_EDGE_RAISEDINNER,
    RT_EDGE_SUNKENOUTER,
    RT_EDGE_SUNKENINNER,
    RT_EDGE_FLAT,           /* 1 px in the outer TL colour all around */
    RT_EDGE_COUNT
};

/* RtOp.flags */
#define RT_OPF_KEYED    0x01    /* bitmap: colour key transparency */
#define RT_OPF_EMBOSS   0x02    /* text/glyph: Win95 disabled emboss */
#define RT_OPF_SHADOW   0x04    /* text: drop shadow in c[1] */
#define RT_OPF_VCENTER  0x08    /* text: centre vertically (default) */
#define RT_OPF_INNER    0x10    /* edge: c[4] fill only inside the edge */

typedef struct RtOp {
    rt_u8  type;
    rt_u8  style;
    rt_u8  flags;
    rt_u8  font;
    rt_u8  image;
    rt_u8  nlayers;
    rt_u8  c[5];
    rt_u8  pad;
    rt_s16 inset[4];        /* applied to the target rect first (l, t, r, b) */
    rt_s16 r[4];            /* bitmap: source x, y, w, h; text: offset x, y */
    rt_s16 m[4];            /* nine-grid margins l, t, r, b; misc */
    rt_u16 layers[RT_MAX_LAYERS];
} RtOp;

enum RtColorKind { RT_COL_RGB, RT_COL_SYS, RT_COL_ALIAS };

typedef struct RtColorDef {
    char   name[20];        /* "" for anonymous literals */
    rt_u8  kind;
    rt_u8  sys;
    rt_u8  alias;
    rt_u8  pad;
    RtColor rgb;
} RtColorDef;

typedef struct RtFontDef {
    char       name[16];
    RtFontSpec spec;
    RtFontH    handle;
} RtFontDef;

typedef struct RtImageDef {
    char     file[13];      /* 8.3 */
    rt_u8    keyed;
    rt_u8    recolor;
    RtColor  key;
    RtImageH handle;
    rt_s16   w, h;
} RtImageDef;

typedef struct RtMetrics {
    rt_s16 btn_w, btn_h;    /* caption buttons */
    rt_s16 btn_gap;         /* gap between the min/max group and close */
    rt_s16 icon;            /* caption icon size */
    rt_s16 caption_h;       /* 0 = USER's */
    rt_s16 border;          /* 0 = keep; applied via SPI_SETBORDER */
    rt_s16 text_pad;        /* space between icon and title */
    rt_u8  layout[8];       /* RT_LAY_* tokens, RT_LAY_END terminated */
} RtMetrics;

enum RtLayoutTok { RT_LAY_END, RT_LAY_ICON, RT_LAY_TITLE, RT_LAY_MIN, RT_LAY_MAX, RT_LAY_CLOSE, RT_LAY_GAP };

typedef struct RtTheme {
    char        name[48];
    char        dir[80];
    struct RtTheme *base;   /* inherited theme, owned */
    rt_u8       sys_colors; /* ColorMode=System */
    RtMetrics   metrics;
    rt_u16      ncolors, nfonts, nimages, nops;
    RtColorDef  colors[RT_MAX_COLORS];
    RtColor     rgb[RT_MAX_COLORS];      /* resolved */
    RtFontDef   fonts[RT_MAX_FONTS];
    RtImageDef  images[RT_MAX_IMAGES];
    rt_u8       nrecolor;
    RtColor     recolor_from[RT_MAX_RECOLOR];
    rt_u8       recolor_to[RT_MAX_RECOLOR];
    RtOp        ops[RT_MAX_OPS];
    rt_u16      map[RT_PART_COUNT][RT_ST_COUNT];   /* op index + 1, 0 = undefined */
    rt_u16      nwarn;
    char        log[RT_LOG_SIZE];                 /* warnings, one per line */
} RtTheme;

/* Loads "<dir>/THEME.INI". Never returns a partial theme: NULL on fatal
 * errors (missing file, unsupported format); warnings go to theme->log. */
RtTheme *rt_theme_load(const char *dir);
void     rt_theme_free(RtTheme *t);
/* Base= names a sibling directory of the theme; if it is not there, it is
 * looked up in this directory (the installed THEMES directory). */
void     rt_theme_set_search_dir(const char *dir);
/* Re-resolves Sys: colours and rebuilds recoloured images. */
void     rt_theme_syscolor_changed(RtTheme *t);
/* Finds the op for (part, state) following the state fallback chain and
 * the Base= chain. 'owner' receives the theme the op belongs to. */
const RtOp *rt_theme_find(const RtTheme *t, int part, int state, const RtTheme **owner);

/* ---- rendering ---- */

#define RT_RQ_HOLE      0x0001  /* req.hole is valid: do not paint inside it */
#define RT_RQ_PREFIX    0x0002  /* text: '&' marks the mnemonic */
#define RT_RQ_LEFT      0x0010  /* text alignment requested by the caller (Align=Auto) */
#define RT_RQ_CENTER    0x0020
#define RT_RQ_RIGHT     0x0040

typedef struct RtDrawReq {
    rt_u8       part;
    rt_u8       state;
    rt_u16      flags;
    RtRect      rc;
    RtRect      hole;
    const char *text;
    rt_s16      textlen;        /* -1 = NUL-terminated */
} RtDrawReq;

/* Draws the request. Returns 1 if the theme defines the part (it was drawn),
 * 0 if not (the caller lets USER draw it). */
int rt_render(const RtTheme *t, RtSurface *s, const RtDrawReq *req);

/* If (part, state) is drawn by a plain Fill op, stores its colour in *out
 * and returns 1 (the engine uses this for brushes it hands to Windows,
 * e.g. the dialog background). */
int rt_theme_fill_color(const RtTheme *t, int part, int state, RtColor *out);

/* Colours of the Text op that draws (part, state): *text, and *shadow (1
 * if the op draws a shadow / emboss copy first). Returns 0 if the part is
 * not drawn by a Text op. */
int rt_theme_text_colors(const RtTheme *t, int part, int state, RtColor *text, RtColor *shadow,
                         int *has_shadow);

/* Font handle a theme uses for a named font ("Caption", "Menu" ...), or 0. */
RtFontH rt_theme_font(const RtTheme *t, const char *name);

/* Draws one built-in glyph centred in r (render/glyphs.c). */
void rt_glyph_draw(RtSurface *s, int glyph, const RtRect *r, int w, int h, RtColor c1, RtColor c2);
/* Natural size of a glyph for a box of the given size. */
void rt_glyph_size(int glyph, const RtRect *r, int *w, int *h);

#endif
