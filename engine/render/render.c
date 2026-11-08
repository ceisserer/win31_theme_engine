/* render.c - executes theme ops through the backend interface. */
#include "rt_theme.h"

#define MAX_DEPTH 4
#define MAX_TEXT  96      /* stack: this runs on the application's stack */

static int get_color(const RtTheme *t, rt_u8 idx, RtColor *out)
{
    if (idx == RT_NOCOLOR || idx >= t->ncolors)
        return 0;
    *out = t->rgb[idx];
    return 1;
}

static RtColor lerp(RtColor a, RtColor b, int i, int n)
{
    long r, g, bl;
    if (n <= 1)
        return a;
    r = RT_R(a) + ((long)RT_R(b) - RT_R(a)) * i / (n - 1);
    g = RT_G(a) + ((long)RT_G(b) - RT_G(a)) * i / (n - 1);
    bl = RT_B(a) + ((long)RT_B(b) - RT_B(a)) * i / (n - 1);
    return RT_RGB(r, g, bl);
}

static void fill(RtSurface *s, int l, int t, int r, int b, RtColor c)
{
    RtRect rc;
    if (r <= l || b <= t)
        return;
    rt_rect_set(&rc, l, t, r, b);
    bk_fill(s, &rc, c);
}

static void draw_gradient(RtSurface *s, const RtRect *r, RtColor c0, RtColor c1, int dir, int bands)
{
    int w = RT_RECT_W(r), h = RT_RECT_H(r), i;
    if (c0 == c1 || bands <= 1) {
        bk_fill(s, r, c0);
        return;
    }
    if (dir == RT_GRAD_H) {
        if (bands > w)
            bands = w;
        for (i = 0; i < bands; i++)
            fill(s, r->left + (int)((long)w * i / bands), r->top,
                 r->left + (int)((long)w * (i + 1) / bands), r->bottom, lerp(c0, c1, i, bands));
    } else if (dir == RT_GRAD_V) {
        if (bands > h)
            bands = h;
        for (i = 0; i < bands; i++)
            fill(s, r->left, r->top + (int)((long)h * i / bands),
                 r->right, r->top + (int)((long)h * (i + 1) / bands), lerp(c0, c1, i, bands));
    } else {
        /* Nested rectangles from the outside in; each one paints over the
         * centre of the previous one. */
        for (i = 0; i < bands; i++) {
            int dx = (int)((long)w * i / (2 * bands));
            int dy = (int)((long)h * i / (2 * bands));
            fill(s, r->left + dx, r->top + dy, r->right - dx, r->bottom - dy, lerp(c0, c1, i, bands));
        }
    }
}

/* One 1-pixel ring: top/left in tl, bottom/right in br (br owns the
 * top-right and bottom-left corner pixels, like Win95 DrawEdge). */
static void ring(RtSurface *s, const RtRect *r, const RtTheme *t, rt_u8 tl, rt_u8 br)
{
    RtColor c;
    if (get_color(t, tl, &c)) {
        fill(s, r->left, r->top, r->right - 1, r->top + 1, c);
        fill(s, r->left, r->top, r->left + 1, r->bottom - 1, c);
    }
    if (get_color(t, br, &c)) {
        fill(s, r->left, r->bottom - 1, r->right, r->bottom, c);
        fill(s, r->right - 1, r->top, r->right, r->bottom, c);
    }
}

static void draw_edge(RtSurface *s, const RtRect *r0, const RtTheme *t, const RtOp *op)
{
    RtRect r = *r0;
    RtColor c;
    ring(s, &r, t, op->c[0], op->c[1]);
    rt_rect_inset(&r, 1, 1, 1, 1);
    if (op->c[2] != RT_NOCOLOR || op->c[3] != RT_NOCOLOR) {
        ring(s, &r, t, op->c[2], op->c[3]);
        rt_rect_inset(&r, 1, 1, 1, 1);
    }
    if (get_color(t, op->c[4], &c) && !rt_rect_empty(&r))
        bk_fill(s, &r, c);
}

static void draw_border(RtSurface *s, const RtRect *r, RtColor c, int w)
{
    fill(s, r->left, r->top, r->right, r->top + w, c);
    fill(s, r->left, r->bottom - w, r->right, r->bottom, c);
    fill(s, r->left, r->top + w, r->left + w, r->bottom - w, c);
    fill(s, r->right - w, r->top + w, r->right, r->bottom - w, c);
}

static void blit(RtSurface *s, RtImageH img, int sx, int sy, int sw, int sh,
                 int dl, int dt, int dr, int db)
{
    RtRect src, dst;
    if (sw <= 0 || sh <= 0 || dr <= dl || db <= dt)
        return;
    rt_rect_set(&src, sx, sy, sx + sw, sy + sh);
    rt_rect_set(&dst, dl, dt, dr, db);
    bk_image_draw(s, img, &src, &dst);
}

static void draw_bitmap(RtSurface *s, const RtRect *r, const RtTheme *t, const RtOp *op)
{
    RtImageH img = t->images[op->image].handle;
    int sx = op->r[0], sy = op->r[1], sw = op->r[2], sh = op->r[3];
    if (!img)
        return;
    switch (op->style) {
    case RT_SIZE_STRETCH:
        blit(s, img, sx, sy, sw, sh, r->left, r->top, r->right, r->bottom);
        break;
    case RT_SIZE_CENTER: {
        int x = r->left + (RT_RECT_W(r) - sw) / 2;
        int y = r->top + (RT_RECT_H(r) - sh) / 2;
        blit(s, img, sx, sy, sw, sh, x, y, x + sw, y + sh);
        break;
    }
    case RT_SIZE_TILE: {
        int x, y;
        for (y = r->top; y < r->bottom; y += sh) {
            int h = r->bottom - y < sh ? r->bottom - y : sh;
            for (x = r->left; x < r->right; x += sw) {
                int w = r->right - x < sw ? r->right - x : sw;
                blit(s, img, sx, sy, w, h, x, y, x + w, y + h);
            }
        }
        break;
    }
    case RT_SIZE_NINEGRID: {
        int ml = op->m[0], mt = op->m[1], mr = op->m[2], mb = op->m[3];
        int l = r->left, tp = r->top, rr = r->right, b = r->bottom;
        int cw = sw - ml - mr, ch = sh - mt - mb;
        /* shrink margins if the target is smaller than the fixed parts */
        if (ml + mr > rr - l) {
            ml = (rr - l) * ml / (ml + mr ? ml + mr : 1);
            mr = (rr - l) - ml;
        }
        if (mt + mb > b - tp) {
            mt = (b - tp) * mt / (mt + mb ? mt + mb : 1);
            mb = (b - tp) - mt;
        }
        /* corners */
        blit(s, img, sx, sy, ml, mt, l, tp, l + ml, tp + mt);
        blit(s, img, sx + sw - mr, sy, mr, mt, rr - mr, tp, rr, tp + mt);
        blit(s, img, sx, sy + sh - mb, ml, mb, l, b - mb, l + ml, b);
        blit(s, img, sx + sw - mr, sy + sh - mb, mr, mb, rr - mr, b - mb, rr, b);
        /* edges */
        blit(s, img, sx + op->m[0], sy, cw, mt, l + ml, tp, rr - mr, tp + mt);
        blit(s, img, sx + op->m[0], sy + sh - mb, cw, mb, l + ml, b - mb, rr - mr, b);
        blit(s, img, sx, sy + op->m[1], ml, ch, l, tp + mt, l + ml, b - mb);
        blit(s, img, sx + sw - mr, sy + op->m[1], mr, ch, rr - mr, tp + mt, rr, b - mb);
        /* centre */
        blit(s, img, sx + op->m[0], sy + op->m[1], cw, ch, l + ml, tp + mt, rr - mr, b - mb);
        break;
    }
    }
}

/* Where a Text op puts the text of req into r: the label without '&'
 * markers in buf (n characters, mnemonic at index *ul or -1), its font and
 * its box (x, y, w, h). Returns 0 if there is no text. */
typedef struct TextLayout {
    char    buf[MAX_TEXT];
    int     n, ul, x, y, w, h;
    RtFontH f;
} TextLayout;

static int text_layout(RtSurface *s, const RtRect *r, const RtTheme *t, const RtOp *op,
                       const RtDrawReq *req, TextLayout *tl)
{
    int len, i, align;
    const char *text = req->text;

    if (!text)
        return 0;
    tl->n = 0;
    tl->ul = -1;
    len = req->textlen < 0 ? rt_strlen(text) : req->textlen;
    for (i = 0; i < len && tl->n < MAX_TEXT; i++) {
        if ((req->flags & RT_RQ_PREFIX) && text[i] == '&') {
            if (i + 1 < len && text[i + 1] == '&') {
                tl->buf[tl->n++] = '&';
                i++;
            } else if (i + 1 < len) {
                tl->ul = tl->n;
            }
            continue;
        }
        tl->buf[tl->n++] = text[i];
    }
    tl->f = op->font != RT_NOFONT ? t->fonts[op->font].handle : (t->nfonts ? t->fonts[0].handle : 0);
    tl->w = bk_text_width(s, tl->f, tl->buf, tl->n);
    tl->h = bk_text_height(s, tl->f);
    align = op->style;
    if (align == RT_ALIGN_AUTO)
        align = (req->flags & RT_RQ_CENTER) ? RT_ALIGN_CENTER
              : (req->flags & RT_RQ_RIGHT) ? RT_ALIGN_RIGHT : RT_ALIGN_LEFT;
    if (align == RT_ALIGN_CENTER)
        tl->x = r->left + (RT_RECT_W(r) - tl->w) / 2;
    else if (align == RT_ALIGN_RIGHT)
        tl->x = r->right - tl->w;
    else
        tl->x = r->left;
    tl->y = (op->flags & RT_OPF_VCENTER) ? r->top + (RT_RECT_H(r) - tl->h) / 2 : r->top;
    tl->x += op->r[0];
    tl->y += op->r[1];
    return 1;
}

static void draw_text(RtSurface *s, const RtRect *r, const RtTheme *t, const RtOp *op,
                      const RtDrawReq *req)
{
    TextLayout tl;
    RtColor c, sc;
    int x, y, h;

    if (!get_color(t, op->c[0], &c) || !text_layout(s, r, t, op, req, &tl))
        return;
    x = tl.x;
    y = tl.y;
    h = tl.h;
    if ((op->flags & RT_OPF_BACK) && tl.n > 0 && get_color(t, op->c[3], &sc)) {
        int bl = x - op->m[0], br = x + tl.w + op->m[0];
        if (bl < r->left) bl = r->left;
        if (br > r->right) br = r->right;
        if (bl < br)
            fill(s, bl, r->top, br, r->bottom, sc);
    }
    if ((op->flags & RT_OPF_SHADOW) && get_color(t, op->c[2], &sc)) {
        bk_text(s, tl.f, x + 1, y + 1, r, tl.buf, tl.n, sc);
        if (tl.ul >= 0) {
            int ux = x + 1 + bk_text_width(s, tl.f, tl.buf, tl.ul);
            fill(s, ux, y + h, ux + bk_text_width(s, tl.f, tl.buf + tl.ul, 1), y + h + 1, sc);
        }
    }
    bk_text(s, tl.f, x, y, r, tl.buf, tl.n, c);
    if (tl.ul >= 0) {
        int ux = x + bk_text_width(s, tl.f, tl.buf, tl.ul);
        fill(s, ux, y + h - 1, ux + bk_text_width(s, tl.f, tl.buf + tl.ul, 1), y + h, c);
    }
}

static void draw_glyph(RtSurface *s, const RtRect *r0, const RtTheme *t, const RtOp *op)
{
    RtColor c1, c2 = 0, sc;
    RtRect r = *r0;
    int w = op->m[0], h = op->m[1];
    if (!get_color(t, op->c[0], &c1))
        return;
    get_color(t, op->c[1], &c2);
    if (!w || !h)
        rt_glyph_size(op->style, &r, &w, &h);
    r.left = (rt_s16)(r.left + op->r[0]);
    r.right = (rt_s16)(r.right + op->r[0]);
    r.top = (rt_s16)(r.top + op->r[1]);
    r.bottom = (rt_s16)(r.bottom + op->r[1]);
    if ((op->flags & RT_OPF_SHADOW) && get_color(t, op->c[2], &sc)) {
        RtRect sr = r;
        rt_rect_inset(&sr, 1, 1, -1, -1);
        rt_glyph_draw(s, op->style, &sr, w, h, sc, sc);
    }
    rt_glyph_draw(s, op->style, &r, w, h, c1, op->c[1] == RT_NOCOLOR ? c1 : c2);
}

static void render_op(const RtTheme *t, RtSurface *s, const RtOp *op, const RtRect *rc,
                      const RtDrawReq *req, int depth)
{
    RtRect r = *rc;
    RtColor c, c2;
    int i;

    rt_rect_inset(&r, op->inset[0], op->inset[1], op->inset[2], op->inset[3]);
    if (rt_rect_empty(&r))
        return;
    switch (op->type) {
    case RT_OP_FILL:
        if (get_color(t, op->c[0], &c))
            bk_fill(s, &r, c);
        break;
    case RT_OP_DITHER:
        if (get_color(t, op->c[0], &c) && get_color(t, op->c[1], &c2))
            bk_dither(s, &r, c, c2);
        break;
    case RT_OP_GRADIENT:
        if (get_color(t, op->c[0], &c)) {
            if (!get_color(t, op->c[1], &c2))
                c2 = c;
            draw_gradient(s, &r, c, c2, op->style, op->m[0]);
        }
        break;
    case RT_OP_EDGE:
        draw_edge(s, &r, t, op);
        break;
    case RT_OP_BORDER:
        if (get_color(t, op->c[0], &c))
            draw_border(s, &r, c, op->m[0]);
        break;
    case RT_OP_GLYPH:
        draw_glyph(s, &r, t, op);
        break;
    case RT_OP_BITMAP:
        draw_bitmap(s, &r, t, op);
        break;
    case RT_OP_TEXT:
        draw_text(s, &r, t, op, req);
        break;
    case RT_OP_FOCUS:
        /* never through the label: grow to enclose req.hole, within rc */
        if (req->flags & RT_RQ_ENCLOSE) {
            if (r.left > req->hole.left) r.left = req->hole.left;
            if (r.top > req->hole.top) r.top = req->hole.top;
            if (r.right < req->hole.right) r.right = req->hole.right;
            if (r.bottom < req->hole.bottom) r.bottom = req->hole.bottom;
            if (r.left < rc->left) r.left = rc->left;
            if (r.top < rc->top) r.top = rc->top;
            if (r.right > rc->right) r.right = rc->right;
            if (r.bottom > rc->bottom) r.bottom = rc->bottom;
        }
        bk_focus(s, &r);
        break;
    case RT_OP_LAYERS:
        if (depth >= MAX_DEPTH)
            break;
        for (i = 0; i < op->nlayers; i++)
            render_op(t, s, &t->ops[op->layers[i]], &r, req, depth + 1);
        break;
    }
}

/* Finds the first Text op of op (following Layers), with the rectangle it
 * gets from rc after the insets on the way. */
static const RtOp *find_text(const RtTheme *t, const RtOp *op, RtRect *r, int depth)
{
    int i;
    rt_rect_inset(r, op->inset[0], op->inset[1], op->inset[2], op->inset[3]);
    if (op->type == RT_OP_TEXT)
        return op;
    if (op->type != RT_OP_LAYERS || depth >= MAX_DEPTH)
        return 0;
    for (i = 0; i < op->nlayers; i++) {
        RtRect lr = *r;
        const RtOp *f = find_text(t, &t->ops[op->layers[i]], &lr, depth + 1);
        if (f) {
            *r = lr;
            return f;
        }
    }
    return 0;
}

int rt_text_box(const RtTheme *t, RtSurface *s, const RtDrawReq *req, RtRect *out)
{
    const RtTheme *owner = 0;
    const RtOp *op = rt_theme_find(t, req->part, req->state, &owner);
    RtRect r = req->rc;
    TextLayout tl;
    if (!op || !(op = find_text(owner, op, &r, 0)) || !text_layout(s, &r, owner, op, req, &tl))
        return 0;
    rt_rect_set(out, tl.x, tl.y, tl.x + tl.w, tl.y + tl.h);
    return 1;
}

int rt_render(const RtTheme *t, RtSurface *s, const RtDrawReq *req)
{
    const RtTheme *owner = 0;
    const RtOp *op = rt_theme_find(t, req->part, req->state, &owner);
    if (!op)
        return 0;
    bk_clip_push(s, &req->rc, (req->flags & (RT_RQ_HOLE | RT_RQ_ENCLOSE)) == RT_RQ_HOLE ? &req->hole : 0);
    render_op(owner, s, op, &req->rc, req, 0);
    bk_clip_pop(s);
    return 1;
}
