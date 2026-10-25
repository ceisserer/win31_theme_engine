/* theme.c - THEME.INI loader (format: docs/THEME_FORMAT.md). */
#include "rt_theme.h"
#include "rt_ini.h"

#ifdef RT_WIN16
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

static const char *const sys_names[RT_SYS_COUNT] = {
    "Scrollbar", "Background", "ActiveCaption", "InactiveCaption", "Menu",
    "Window", "WindowFrame", "MenuText", "WindowText", "CaptionText",
    "ActiveBorder", "InactiveBorder", "AppWorkspace", "Highlight",
    "HighlightText", "BtnFace", "BtnShadow", "GrayText", "BtnText",
    "InactiveCaptionText", "BtnHighlight", "3DDkShadow", "3DLight"
};

static const char *const op_names[] = {
    "None", "Fill", "Dither", "Gradient", "Edge", "Border", "Glyph", "Bitmap",
    "Text", "Focus", "Layers"
};

static const char *const edge_names[RT_EDGE_COUNT] = {
    "Raised", "RaisedSoft", "Sunken", "SunkenSoft", "Etched", "Bump",
    "RaisedOuter", "RaisedInner", "SunkenOuter", "SunkenInner", "Flat"
};

/* Default edge colours per preset: outer TL, outer BR, inner TL, inner BR,
 * as COLOR_* indices (0xFF = none). */
static const rt_u8 edge_colors[RT_EDGE_COUNT][4] = {
    { RT_SYS_3DLIGHT, RT_SYS_3DDKSHADOW, RT_SYS_BTNHIGHLIGHT, RT_SYS_BTNSHADOW },
    { RT_SYS_BTNHIGHLIGHT, RT_SYS_3DDKSHADOW, RT_SYS_3DLIGHT, RT_SYS_BTNSHADOW },
    { RT_SYS_BTNSHADOW, RT_SYS_BTNHIGHLIGHT, RT_SYS_3DDKSHADOW, RT_SYS_3DLIGHT },
    { RT_SYS_3DDKSHADOW, RT_SYS_BTNHIGHLIGHT, RT_SYS_BTNSHADOW, RT_SYS_3DLIGHT },
    { RT_SYS_BTNSHADOW, RT_SYS_BTNHIGHLIGHT, RT_SYS_BTNHIGHLIGHT, RT_SYS_BTNSHADOW },
    { RT_SYS_3DLIGHT, RT_SYS_3DDKSHADOW, RT_SYS_BTNSHADOW, RT_SYS_BTNHIGHLIGHT },
    { RT_SYS_3DLIGHT, RT_SYS_3DDKSHADOW, 0xFF, 0xFF },
    { RT_SYS_BTNHIGHLIGHT, RT_SYS_BTNSHADOW, 0xFF, 0xFF },
    { RT_SYS_BTNSHADOW, RT_SYS_BTNHIGHLIGHT, 0xFF, 0xFF },
    { RT_SYS_3DDKSHADOW, RT_SYS_3DLIGHT, 0xFF, 0xFF },
    { RT_SYS_BTNSHADOW, RT_SYS_BTNSHADOW, 0xFF, 0xFF }
};

static char search_dir[80];

void rt_theme_set_search_dir(const char *dir)
{
    rt_strlcpy(search_dir, dir ? dir : "", sizeof(search_dir));
}

typedef struct Loader {
    RtTheme *t;
    RtIni    ini;
    rt_u16  *secop;     /* per section: op index + 1; 0xFFFF = being parsed */
} Loader;

static void warn(RtTheme *t, const char *a, const char *b)
{
    int n = rt_strlen(t->log);
    t->nwarn++;
    if (n + rt_strlen(a) + rt_strlen(b) + 3 >= RT_LOG_SIZE)
        return;
    rt_strlcpy(t->log + n, a, RT_LOG_SIZE - n);
    n += rt_strlen(a);
    if (b && *b) {
        t->log[n++] = ' ';
        rt_strlcpy(t->log + n, b, RT_LOG_SIZE - n);
        n += rt_strlen(b);
    }
    t->log[n++] = '\n';
    t->log[n] = 0;
}

static int find_name(const char *const *names, int count, const char *s)
{
    int i;
    for (i = 0; i < count; i++)
        if (rt_stricmp(names[i], s) == 0)
            return i;
    return -1;
}

/* ---- colours ---- */

static int add_color(RtTheme *t, const char *name, int kind, int sys, RtColor rgb)
{
    int i;
    RtColorDef *c;
    if (!name || !*name) {
        /* deduplicate anonymous entries */
        for (i = 0; i < t->ncolors; i++) {
            c = &t->colors[i];
            if (!c->name[0] && c->kind == kind && c->sys == sys && c->rgb == rgb)
                return i;
        }
    }
    if (t->ncolors >= RT_MAX_COLORS) {
        warn(t, "too many colours", name);
        return RT_NOCOLOR;
    }
    c = &t->colors[t->ncolors];
    rt_strlcpy(c->name, name ? name : "", sizeof(c->name));
    c->kind = (rt_u8)kind;
    c->sys = (rt_u8)sys;
    c->rgb = rgb;
    return t->ncolors++;
}

static int find_color(const RtTheme *t, const char *name)
{
    int i;
    for (i = 0; i < t->ncolors; i++)
        if (t->colors[i].name[0] && rt_stricmp(t->colors[i].name, name) == 0)
            return i;
    return -1;
}

static int parse_rgb(const char *s, RtColor *out)
{
    int v[3];
    if (rt_parse_ints(s, v, 3) != 3)
        return 0;
    *out = RT_RGB(v[0], v[1], v[2]);
    return 1;
}

/* Turns a colour reference ("Sys:Name", "r,g,b", "None", or a [Colors]
 * name) into a colour-table index. */
static int color_ref(RtTheme *t, const char *s)
{
    RtColor rgb;
    int i;
    if (!s || !*s || rt_stricmp(s, "None") == 0)
        return RT_NOCOLOR;
    if (rt_strnicmp(s, "Sys:", 4) == 0) {
        i = find_name(sys_names, RT_SYS_COUNT, s + 4);
        if (i < 0) {
            warn(t, "unknown system colour", s);
            return RT_NOCOLOR;
        }
        return add_color(t, 0, RT_COL_SYS, i, 0);
    }
    if (parse_rgb(s, &rgb))
        return add_color(t, 0, RT_COL_RGB, 0, rgb);
    i = find_color(t, s);
    if (i < 0) {
        warn(t, "unknown colour", s);
        return RT_NOCOLOR;
    }
    return i;
}

static void resolve_colors(RtTheme *t)
{
    int i, depth;
    for (i = 0; i < t->ncolors; i++) {
        const RtColorDef *c = &t->colors[i];
        for (depth = 0; c->kind == RT_COL_ALIAS && depth < 8; depth++)
            c = &t->colors[c->alias];
        if (c->kind == RT_COL_SYS)
            t->rgb[i] = rt_syscolor(c->sys);
        else
            t->rgb[i] = c->rgb;
    }
}

static void load_colors(RtTheme *t, const RtIni *ini)
{
    int sec = rt_ini_find(ini, "Colors");
    int i, first;
    if (sec < 0)
        return;
    first = t->ncolors;
    /* pass 1: declare names so values may reference later entries */
    for (i = 0; i < ini->secs[sec].count; i++)
        add_color(t, ini->keys[ini->secs[sec].first + i].key, RT_COL_RGB, 0, 0);
    /* pass 2: values */
    for (i = 0; i < ini->secs[sec].count; i++) {
        const RtIniKey *k = &ini->keys[ini->secs[sec].first + i];
        int idx = first + i, ref;
        if (idx >= t->ncolors)
            break;
        ref = color_ref(t, k->val);
        if (ref == RT_NOCOLOR || ref == idx) {
            warn(t, "bad colour value for", k->key);
            continue;
        }
        t->colors[idx].kind = RT_COL_ALIAS;
        t->colors[idx].alias = (rt_u8)ref;
    }
}

/* [SysColors]: <system colour name>=r,g,b */
static void load_syscolors(RtTheme *t, const RtIni *ini)
{
    int sec = rt_ini_find(ini, "SysColors");
    int i, n;
    RtColor rgb;
    if (sec < 0)
        return;
    for (i = 0; i < ini->secs[sec].count && t->nsys < RT_SYS_COUNT; i++) {
        const RtIniKey *k = &ini->keys[ini->secs[sec].first + i];
        n = find_name(sys_names, RT_SYS_COUNT, k->key);
        if (n < 0 || !parse_rgb(k->val, &rgb)) {
            warn(t, "bad system colour", k->key);
            continue;
        }
        t->sys_idx[t->nsys] = (rt_u8)n;
        t->sys_rgb[t->nsys] = rgb;
        t->nsys++;
    }
}

/* ---- fonts ---- */

static void load_fonts(RtTheme *t, const RtIni *ini)
{
    int sec = rt_ini_find(ini, "Fonts");
    int i;
    if (sec < 0)
        return;
    for (i = 0; i < ini->secs[sec].count && t->nfonts < RT_MAX_FONTS; i++) {
        const RtIniKey *k = &ini->keys[ini->secs[sec].first + i];
        RtFontDef *f = &t->fonts[t->nfonts];
        const char *p = k->val;
        int n = 0, v[3];
        while (p[n] && p[n] != ',' && n < (int)sizeof(f->spec.face) - 1)
            n++;
        rt_strlcpy(f->spec.face, p, n + 1);
        p += n;
        if (*p == ',')
            p++;
        v[0] = 8;
        v[1] = 400;
        v[2] = 0;
        rt_parse_ints(p, v, 3);
        f->spec.points = (rt_s16)v[0];
        f->spec.weight = (rt_s16)v[1];
        f->spec.italic = (rt_u8)(v[2] != 0);
        rt_strlcpy(f->name, k->key, sizeof(f->name));
        f->handle = bk_font_create(&f->spec);
        t->nfonts++;
    }
}

static int font_ref(RtTheme *t, const char *s)
{
    int i;
    if (!s)
        return RT_NOFONT;
    for (i = 0; i < t->nfonts; i++)
        if (rt_stricmp(t->fonts[i].name, s) == 0)
            return i;
    warn(t, "unknown font", s);
    return RT_NOFONT;
}

RtFontH rt_theme_font(const RtTheme *t, const char *name)
{
    int i;
    for (; t; t = t->base)
        for (i = 0; i < t->nfonts; i++)
            if (rt_stricmp(t->fonts[i].name, name) == 0)
                return t->fonts[i].handle;
    return 0;
}

/* ---- images ---- */

static rt_u32 le32(const rt_u8 *p)
{
    return (rt_u32)p[0] | ((rt_u32)p[1] << 8) | ((rt_u32)p[2] << 16) | ((rt_u32)p[3] << 24);
}

static rt_u16 le16(const rt_u8 *p)
{
    return (rt_u16)(p[0] | (p[1] << 8));
}

/* Parses a .BMP file buffer into a DIB description (no copy). */
static int parse_bmp(rt_u8 *buf, rt_u32 len, RtDib *d)
{
    rt_u32 hsize, off;
    rt_s32 h;
    if (len < 54 || buf[0] != 'B' || buf[1] != 'M')
        return 0;
    off = le32(buf + 10);
    hsize = le32(buf + 14);
    if (hsize < 40 || le32(buf + 30) != 0)  /* BITMAPINFOHEADER, BI_RGB only */
        return 0;
    d->info = buf + 14;
    d->w = (rt_s16)le32(buf + 18);
    h = (rt_s32)le32(buf + 22);
    if (h < 0)
        return 0;   /* top-down DIBs are not valid on Win 3.x */
    d->h = (rt_s16)h;
    d->bpp = le16(buf + 28);
    if (d->bpp != 1 && d->bpp != 4 && d->bpp != 8 && d->bpp != 24)
        return 0;
    d->ncolors = (rt_u16)le32(buf + 46);
    if (d->ncolors == 0 && d->bpp <= 8)
        d->ncolors = (rt_u16)(1 << d->bpp);
    d->palette = buf + 14 + hsize;
    d->stride = (((rt_u32)d->w * d->bpp + 31) / 32) * 4;
    if (off + d->stride * (rt_u32)d->h > len)
        return 0;
    d->bits = buf + off;
    return 1;
}

static void recolor_dib(const RtTheme *t, RtDib *d)
{
    int i, j;
    if (d->bpp <= 8) {
        for (i = 0; i < d->ncolors; i++) {
            rt_u8 *q = d->palette + i * 4;
            RtColor c = RT_RGB(q[2], q[1], q[0]);
            for (j = 0; j < t->nrecolor; j++) {
                if (c == t->recolor_from[j] && t->recolor_to[j] != RT_NOCOLOR) {
                    RtColor n = t->rgb[t->recolor_to[j]];
                    q[0] = RT_B(n);
                    q[1] = RT_G(n);
                    q[2] = RT_R(n);
                    break;
                }
            }
        }
    } else {
        rt_s16 x, y;
        for (y = 0; y < d->h; y++) {
            rt_u8 *q = d->bits + (rt_u32)y * d->stride;
            for (x = 0; x < d->w; x++, q += 3) {
                RtColor c = RT_RGB(q[2], q[1], q[0]);
                for (j = 0; j < t->nrecolor; j++) {
                    if (c == t->recolor_from[j] && t->recolor_to[j] != RT_NOCOLOR) {
                        RtColor n = t->rgb[t->recolor_to[j]];
                        q[0] = RT_B(n);
                        q[1] = RT_G(n);
                        q[2] = RT_R(n);
                        break;
                    }
                }
            }
        }
    }
}

static void path_join(char *out, int size, const char *dir, const char *file)
{
    int n;
    rt_strlcpy(out, dir, size);
    n = rt_strlen(out);
    if (n > 0 && out[n - 1] != PATHSEP && n < size - 1)
        out[n++] = PATHSEP;
    rt_strlcpy(out + n, file, size - n);
}

static int build_image(RtTheme *t, RtImageDef *im)
{
    char path[128];
    rt_u32 len;
    rt_u8 *buf;
    RtDib d;
    if (im->handle) {
        bk_image_destroy(im->handle);
        im->handle = 0;
    }
    path_join(path, sizeof(path), t->dir, im->file);
    buf = (rt_u8 *)rt_file_load(path, &len);
    if (!buf) {
        warn(t, "cannot read image", im->file);
        return 0;
    }
    if (!parse_bmp(buf, len, &d)) {
        warn(t, "unsupported bitmap", im->file);
        rt_free(buf);
        return 0;
    }
    if (im->recolor)
        recolor_dib(t, &d);
    im->w = d.w;
    im->h = d.h;
    im->handle = bk_image_create(&d, im->keyed, im->key);
    rt_free(buf);
    return im->handle != 0;
}

static int image_ref(RtTheme *t, const char *file, int keyed, RtColor key, int recolor)
{
    int i;
    RtImageDef *im;
    for (i = 0; i < t->nimages; i++) {
        im = &t->images[i];
        if (rt_stricmp(im->file, file) == 0 && im->keyed == keyed && (!keyed || im->key == key)
            && im->recolor == recolor)
            return i;
    }
    if (t->nimages >= RT_MAX_IMAGES) {
        warn(t, "too many images", file);
        return -1;
    }
    im = &t->images[t->nimages];
    rt_strlcpy(im->file, file, sizeof(im->file));
    im->keyed = (rt_u8)keyed;
    im->key = key;
    im->recolor = (rt_u8)recolor;
    if (!build_image(t, im))
        return -1;
    return t->nimages++;
}

static void load_recolor(RtTheme *t, const RtIni *ini)
{
    int sec = rt_ini_find(ini, "Recolor");
    int i;
    if (sec < 0)
        return;
    for (i = 0; i < ini->secs[sec].count && t->nrecolor < RT_MAX_RECOLOR; i++) {
        const RtIniKey *k = &ini->keys[ini->secs[sec].first + i];
        RtColor from;
        if (!parse_rgb(k->key, &from)) {
            warn(t, "bad [Recolor] key", k->key);
            continue;
        }
        t->recolor_from[t->nrecolor] = from;
        t->recolor_to[t->nrecolor] = (rt_u8)color_ref(t, k->val);
        t->nrecolor++;
    }
}

/* ---- metrics ---- */

static void load_metrics(RtTheme *t, const RtIni *ini)
{
    static const char *const toks[] = { "", "Icon", "Title", "Min", "Max", "Close", "Gap" };
    RtMetrics *m = &t->metrics;
    int sec = rt_ini_find(ini, "Metrics");
    const char *s;
    int v[1], n = 0;

    /* Win95 defaults */
    m->btn_w = 16;
    m->btn_h = 14;
    m->btn_gap = 2;
    m->icon = 16;
    m->caption_h = 0;
    m->border = 0;
    m->text_pad = 2;
    m->layout[0] = RT_LAY_ICON;
    m->layout[1] = RT_LAY_TITLE;
    m->layout[2] = RT_LAY_MIN;
    m->layout[3] = RT_LAY_MAX;
    m->layout[4] = RT_LAY_GAP;
    m->layout[5] = RT_LAY_CLOSE;
    m->layout[6] = RT_LAY_END;
    if (sec < 0)
        return;
#define METRIC(key, field) \
    if (rt_parse_ints(rt_ini_get_in(ini, sec, key), v, 1) == 1) m->field = (rt_s16)v[0]
    METRIC("CaptionButtonWidth", btn_w);
    METRIC("CaptionButtonHeight", btn_h);
    METRIC("CaptionButtonGap", btn_gap);
    METRIC("CaptionIconSize", icon);
    METRIC("CaptionHeight", caption_h);
    METRIC("BorderWidth", border);
    METRIC("TitlePadding", text_pad);
#undef METRIC
    s = rt_ini_get_in(ini, sec, "Layout");
    if (s) {
        while (*s && n < 7) {
            char tok[12];
            int len = 0, i;
            while (*s == ' ' || *s == ',')
                s++;
            while (s[len] && s[len] != ',' && s[len] != ' ' && len < 11)
                len++;
            if (!len)
                break;
            rt_strlcpy(tok, s, len + 1);
            s += len;
            i = find_name(toks, 7, tok);
            if (i <= 0)
                warn(t, "unknown layout token", tok);
            else
                m->layout[n++] = (rt_u8)i;
        }
        m->layout[n] = RT_LAY_END;
    }
}

/* ---- ops ---- */

static int parse_op(Loader *L, int sec);

static void parse_inset(const char *s, rt_s16 *dst)
{
    int v[4], n = rt_parse_ints(s, v, 4), i;
    if (n == 1)
        v[1] = v[2] = v[3] = v[0];
    else if (n != 4)
        return;
    for (i = 0; i < 4; i++)
        dst[i] = (rt_s16)v[i];
}

static int parse_op(Loader *L, int sec)
{
    RtTheme *t = L->t;
    const RtIni *ini = &L->ini;
    const char *s;
    RtOp *op;
    int idx, type, v[4], n;

    if (L->secop[sec] == 0xFFFF) {
        warn(t, "recursive Layers in", ini->secs[sec].name);
        return -1;
    }
    if (L->secop[sec])
        return L->secop[sec] - 1;
    if (t->nops >= RT_MAX_OPS) {
        warn(t, "too many ops at", ini->secs[sec].name);
        return -1;
    }
    L->secop[sec] = 0xFFFF;

    s = rt_ini_get_in(ini, sec, "Op");
    if (s)
        type = find_name(op_names, sizeof(op_names) / sizeof(op_names[0]), s);
    else
        type = rt_ini_get_in(ini, sec, "Layers") ? RT_OP_LAYERS : -1;
    if (type < 0 || (type == RT_OP_NONE && !s)) {
        warn(t, "missing or unknown Op in", ini->secs[sec].name);
        L->secop[sec] = 0;
        return -1;
    }

    /* Layers may add ops (recursion), so reserve our slot first. */
    idx = t->nops++;
    op = &t->ops[idx];
    op->type = (rt_u8)type;
    op->font = RT_NOFONT;
    for (n = 0; n < 5; n++)
        op->c[n] = RT_NOCOLOR;
    op->flags = RT_OPF_VCENTER;
    parse_inset(rt_ini_get_in(ini, sec, "Inset"), op->inset);

    switch (type) {
    case RT_OP_FILL:
        op->c[0] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "Color"));
        break;
    case RT_OP_DITHER:
        op->c[0] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "Color"));
        op->c[1] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "Color2"));
        break;
    case RT_OP_GRADIENT:
        op->c[0] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "From"));
        op->c[1] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "To"));
        s = rt_ini_get_in(ini, sec, "Direction");
        op->style = RT_GRAD_H;
        if (s && rt_stricmp(s, "Vertical") == 0)
            op->style = RT_GRAD_V;
        else if (s && rt_stricmp(s, "Concentric") == 0)
            op->style = RT_GRAD_CONCENTRIC;
        op->m[0] = 32;
        if (rt_parse_ints(rt_ini_get_in(ini, sec, "Bands"), v, 1) == 1)
            op->m[0] = (rt_s16)v[0];
        break;
    case RT_OP_EDGE: {
        int e = 0, i;
        s = rt_ini_get_in(ini, sec, "Edge");
        if (s) {
            e = find_name(edge_names, RT_EDGE_COUNT, s);
            if (e < 0) {
                warn(t, "unknown edge", s);
                e = 0;
            }
        }
        op->style = (rt_u8)e;
        for (i = 0; i < 4; i++)
            op->c[i] = edge_colors[e][i] == 0xFF ? RT_NOCOLOR
                     : (rt_u8)add_color(t, 0, RT_COL_SYS, edge_colors[e][i], 0);
        /* Outer=a,b / Inner=c,d override the preset colours */
        {
            static const char *const keys[2] = { "Outer", "Inner" };
            int k;
            for (k = 0; k < 2; k++) {
                char buf[48];
                const char *val = rt_ini_get_in(ini, sec, keys[k]);
                int comma = 0;
                if (!val)
                    continue;
                rt_strlcpy(buf, val, sizeof(buf));
                while (buf[comma] && buf[comma] != ';' && buf[comma] != '/')
                    comma++;
                /* colours are separated by '/' because they may contain commas */
                if (buf[comma] == '/') {
                    buf[comma] = 0;
                    op->c[k * 2] = (rt_u8)color_ref(t, buf);
                    op->c[k * 2 + 1] = (rt_u8)color_ref(t, buf + comma + 1);
                } else {
                    warn(t, "Outer/Inner need two colours separated by '/' in", ini->secs[sec].name);
                }
            }
        }
        s = rt_ini_get_in(ini, sec, "Fill");
        if (s)
            op->c[4] = (rt_u8)color_ref(t, s);
        break;
    }
    case RT_OP_BORDER:
        op->c[0] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "Color"));
        op->m[0] = 1;
        if (rt_parse_ints(rt_ini_get_in(ini, sec, "Width"), v, 1) == 1)
            op->m[0] = (rt_s16)v[0];
        break;
    case RT_OP_GLYPH:
        s = rt_ini_get_in(ini, sec, "Glyph");
        op->style = (rt_u8)rt_glyph_from_name(s ? s : "");
        if (!op->style)
            warn(t, "unknown glyph in", ini->secs[sec].name);
        op->c[0] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "Color"));
        s = rt_ini_get_in(ini, sec, "Color2");
        if (s)
            op->c[1] = (rt_u8)color_ref(t, s);
        s = rt_ini_get_in(ini, sec, "Shadow");
        if (s) {
            op->c[2] = (rt_u8)color_ref(t, s);
            op->flags |= RT_OPF_SHADOW;
        }
        n = rt_parse_ints(rt_ini_get_in(ini, sec, "Size"), v, 2);
        if (n == 2) {
            op->m[0] = (rt_s16)v[0];
            op->m[1] = (rt_s16)v[1];
        }
        n = rt_parse_ints(rt_ini_get_in(ini, sec, "Offset"), v, 2);
        if (n == 2) {
            op->r[0] = (rt_s16)v[0];
            op->r[1] = (rt_s16)v[1];
        }
        break;
    case RT_OP_BITMAP: {
        static const char *const sizing[] = { "Stretch", "Tile", "NineGrid", "Center" };
        RtColor key = 0;
        int keyed, recolor, img;
        s = rt_ini_get_in(ini, sec, "Image");
        if (!s) {
            warn(t, "Bitmap without Image in", ini->secs[sec].name);
            op->type = RT_OP_NONE;
            break;
        }
        keyed = parse_rgb(rt_ini_get_in(ini, sec, "Transparent") ? rt_ini_get_in(ini, sec, "Transparent") : "", &key);
        recolor = rt_parse_ints(rt_ini_get_in(ini, sec, "Recolor"), v, 1) == 1 && v[0];
        img = image_ref(t, s, keyed, key, recolor);
        if (img < 0) {
            op->type = RT_OP_NONE;
            break;
        }
        op->image = (rt_u8)img;
        if (keyed)
            op->flags |= RT_OPF_KEYED;
        if (rt_parse_ints(rt_ini_get_in(ini, sec, "Source"), v, 4) == 4) {
            op->r[0] = (rt_s16)v[0];
            op->r[1] = (rt_s16)v[1];
            op->r[2] = (rt_s16)v[2];
            op->r[3] = (rt_s16)v[3];
        } else {
            op->r[2] = t->images[img].w;
            op->r[3] = t->images[img].h;
        }
        s = rt_ini_get_in(ini, sec, "Sizing");
        op->style = RT_SIZE_STRETCH;
        if (s) {
            n = find_name(sizing, 4, s);
            if (n < 0)
                warn(t, "unknown Sizing", s);
            else
                op->style = (rt_u8)n;
        }
        parse_inset(rt_ini_get_in(ini, sec, "Margins"), op->m);
        break;
    }
    case RT_OP_TEXT:
        s = rt_ini_get_in(ini, sec, "Font");
        op->font = (rt_u8)(s ? font_ref(t, s) : RT_NOFONT);
        op->c[0] = (rt_u8)color_ref(t, rt_ini_get_in(ini, sec, "Color"));
        s = rt_ini_get_in(ini, sec, "Shadow");
        if (s) {
            op->c[2] = (rt_u8)color_ref(t, s);
            op->flags |= RT_OPF_SHADOW;
        }
        s = rt_ini_get_in(ini, sec, "Align");
        op->style = RT_ALIGN_AUTO;
        if (s) {
            static const char *const al[] = { "Auto", "Left", "Center", "Right" };
            n = find_name(al, 4, s);
            if (n < 0)
                warn(t, "unknown Align", s);
            else
                op->style = (rt_u8)n;
        }
        n = rt_parse_ints(rt_ini_get_in(ini, sec, "Offset"), v, 2);
        if (n == 2) {
            op->r[0] = (rt_s16)v[0];
            op->r[1] = (rt_s16)v[1];
        }
        break;
    case RT_OP_FOCUS:
        break;
    case RT_OP_LAYERS: {
        const char *p = rt_ini_get_in(ini, sec, "Layers");
        while (p && *p && op->nlayers < RT_MAX_LAYERS) {
            char name[40];
            int len = 0, ls, li;
            while (*p == ' ' || *p == ',')
                p++;
            while (p[len] && p[len] != ',' && len < 39)
                len++;
            if (!len)
                break;
            rt_strlcpy(name, p, len + 1);
            while (len > 0 && name[len - 1] == ' ')
                name[--len] = 0;
            p += len;
            while (*p && *p != ',')
                p++;
            ls = rt_ini_find(ini, name);
            if (ls < 0) {
                warn(t, "unknown layer", name);
                continue;
            }
            li = parse_op(L, ls);
            /* 'op' may not move: t->ops is a fixed array */
            if (li >= 0)
                op->layers[op->nlayers++] = (rt_u16)li;
        }
        break;
    }
    }
    L->secop[sec] = (rt_u16)(idx + 1);
    return idx;
}

static int is_reserved(const char *name)
{
    static const char *const r[] = { "Theme", "Metrics", "Colors", "Fonts", "Recolor", "SysColors" };
    return find_name(r, 6, name) >= 0;
}

static void load_parts(Loader *L)
{
    const RtIni *ini = &L->ini;
    int i;
    for (i = 1; i < ini->nsecs; i++) {
        const char *name = ini->secs[i].name;
        int dot = 0, part, state = RT_ST_NORMAL, op;
        if (is_reserved(name))
            continue;
        while (name[dot] && name[dot] != '.')
            dot++;
        part = rt_part_from_name(name, dot);
        if (part < 0)
            continue;   /* helper section, parsed when referenced */
        if (name[dot] == '.') {
            state = rt_state_from_name(name + dot + 1);
            if (state < 0) {
                warn(L->t, "unknown state in", name);
                continue;
            }
        }
        op = parse_op(L, i);
        if (op >= 0)
            L->t->map[part][state] = (rt_u16)(op + 1);
    }
}

RtTheme *rt_theme_load(const char *dir)
{
    Loader L;
    RtTheme *t;
    char path[128];
    const char *s;
    int v[1];

    t = (RtTheme *)rt_alloc(sizeof(RtTheme));
    if (!t)
        return 0;
    rt_strlcpy(t->dir, dir, sizeof(t->dir));
    path_join(path, sizeof(path), dir, "THEME.INI");
    if (rt_ini_load(&L.ini, path) != 0) {
        rt_free(t);
        return 0;
    }
    if (rt_parse_ints(rt_ini_get(&L.ini, "Theme", "Format"), v, 1) == 1 && v[0] > RT_THEME_FORMAT) {
        rt_ini_free(&L.ini);
        rt_free(t);
        return 0;
    }
    L.t = t;
    L.secop = (rt_u16 *)rt_alloc((rt_u32)(L.ini.nsecs + 1) * sizeof(rt_u16));
    if (!L.secop) {
        rt_ini_free(&L.ini);
        rt_free(t);
        return 0;
    }
    s = rt_ini_get(&L.ini, "Theme", "Name");
    rt_strlcpy(t->name, s ? s : dir, sizeof(t->name));
    s = rt_ini_get(&L.ini, "Theme", "ColorMode");
    t->sys_colors = !(s && rt_stricmp(s, "Fixed") == 0);

    load_metrics(t, &L.ini);
    load_syscolors(t, &L.ini);
    load_colors(t, &L.ini);
    resolve_colors(t);          /* recolouring of images needs resolved colours */
    load_fonts(t, &L.ini);
    load_recolor(t, &L.ini);
    resolve_colors(t);
    load_parts(&L);
    resolve_colors(t);          /* ops may have added anonymous colours */

    s = rt_ini_get(&L.ini, "Theme", "Base");
    if (s && *s) {
        char bdir[96];
        int n = rt_strlen(t->dir);
        /* Base is a sibling directory */
        while (n > 0 && t->dir[n - 1] != PATHSEP)
            n--;
        rt_strlcpy(bdir, t->dir, n + 1);
        rt_strlcpy(bdir + n, s, sizeof(bdir) - n);
        t->base = rt_theme_load(bdir);
        if (!t->base && search_dir[0]) {
            path_join(bdir, sizeof(bdir), search_dir, s);
            t->base = rt_theme_load(bdir);
        }
        if (!t->base)
            warn(t, "cannot load base theme", s);
    }
    rt_free(L.secop);
    rt_ini_free(&L.ini);
    return t;
}

void rt_theme_free(RtTheme *t)
{
    int i;
    if (!t)
        return;
    if (t->base)
        rt_theme_free(t->base);
    for (i = 0; i < t->nfonts; i++)
        if (t->fonts[i].handle)
            bk_font_destroy(t->fonts[i].handle);
    for (i = 0; i < t->nimages; i++)
        if (t->images[i].handle)
            bk_image_destroy(t->images[i].handle);
    rt_free(t);
}

void rt_theme_syscolor_changed(RtTheme *t)
{
    int i;
    for (; t; t = t->base) {
        resolve_colors(t);
        for (i = 0; i < t->nimages; i++)
            if (t->images[i].recolor)
                build_image(t, &t->images[i]);
    }
}

int rt_theme_syscolors(const RtTheme *t, rt_u8 *idx, RtColor *rgb)
{
    int n = 0, i, j;
    if (!t)
        return 0;
    n = rt_theme_syscolors(t->base, idx, rgb);
    for (i = 0; i < t->nsys; i++) {
        for (j = 0; j < n && idx[j] != t->sys_idx[i]; j++)
            ;
        idx[j] = t->sys_idx[i];
        rgb[j] = t->sys_rgb[i];
        if (j == n)
            n++;
    }
    return n;
}

const RtOp *rt_theme_find(const RtTheme *t, int part, int state, const RtTheme **owner)
{
    if (part < 0 || part >= RT_PART_COUNT)
        return 0;
    for (; t; t = t->base) {
        int st = state;
        for (;;) {
            if (st >= 0 && st < RT_ST_COUNT && t->map[part][st]) {
                const RtOp *op = &t->ops[t->map[part][st] - 1];
                if (op->type == RT_OP_NONE)
                    return 0;   /* explicitly disabled: let USER draw */
                if (owner)
                    *owner = t;
                return op;
            }
            if (st == RT_ST_NORMAL)
                break;
            st = rt_state_fallback(st);
        }
    }
    return 0;
}

int rt_theme_fill_color(const RtTheme *t, int part, int state, RtColor *out)
{
    const RtTheme *owner = 0;
    const RtOp *op = rt_theme_find(t, part, state, &owner);
    if (!op || op->type != RT_OP_FILL || op->c[0] == RT_NOCOLOR || op->c[0] >= owner->ncolors)
        return 0;
    *out = owner->rgb[op->c[0]];
    return 1;
}

int rt_theme_text_colors(const RtTheme *t, int part, int state, RtColor *text, RtColor *shadow,
                         int *has_shadow)
{
    const RtTheme *owner = 0;
    const RtOp *op = rt_theme_find(t, part, state, &owner);
    if (!op || op->type != RT_OP_TEXT || op->c[0] == RT_NOCOLOR || op->c[0] >= owner->ncolors)
        return 0;
    *text = owner->rgb[op->c[0]];
    *has_shadow = (op->flags & RT_OPF_SHADOW) && op->c[2] != RT_NOCOLOR && op->c[2] < owner->ncolors;
    if (*has_shadow)
        *shadow = owner->rgb[op->c[2]];
    return 1;
}
