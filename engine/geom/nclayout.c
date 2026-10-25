/* nclayout.c - theme and USER 3.1 non-client layouts and point mapping. */
#include "rt_geom.h"

static void empty(RtRect *r)
{
    rt_rect_set(r, 0, 0, 0, 0);
}

void rt_nc_layout(const RtMetrics *m, const RtNcIn *in, RtNcLayout *out)
{
    int bar_h, by, left, right, i, has_btns, y;
    int show[RT_NCB_COUNT];

    rt_rect_set(&out->window, 0, 0, in->cx, in->cy);
    out->inner = out->window;
    rt_rect_inset(&out->inner, in->frame_x, in->frame_y, in->frame_x, in->frame_y);
    empty(&out->icon);
    empty(&out->title);
    for (i = 0; i < RT_NCB_COUNT; i++)
        empty(&out->btn[i]);

    y = out->inner.top;
    if (in->caption > 0) {
        rt_rect_set(&out->caption, out->inner.left, y, out->inner.right, y + in->caption);
        y += in->caption;
    } else {
        empty(&out->caption);
    }
    if (in->menu > 0) {
        rt_rect_set(&out->menubar, out->inner.left, y, out->inner.right, y + in->menu);
        y += in->menu;
    } else {
        empty(&out->menubar);
    }
    rt_rect_set(&out->client, out->inner.left, y, out->inner.right, out->inner.bottom);
    if (in->caption <= 0)
        return;

    bar_h = in->caption - 1;    /* without the separator row */
    by = out->caption.top + (bar_h - m->btn_h) / 2;
    has_btns = (in->flags & (RT_NC_MINBOX | RT_NC_MAXBOX)) != 0;
    show[RT_NCB_CLOSE] = (in->flags & RT_NC_SYSMENU) != 0;
    show[RT_NCB_MIN] = has_btns;
    show[RT_NCB_MAX] = has_btns;

    left = out->caption.left + 2;
    right = out->caption.right - 2;

    /* tokens before Title go left to right */
    for (i = 0; i < 8 && m->layout[i] != RT_LAY_END && m->layout[i] != RT_LAY_TITLE; i++) {
        int tok = m->layout[i], b = -1;
        if (tok == RT_LAY_ICON) {
            if ((in->flags & RT_NC_SYSMENU) && !(in->flags & RT_NC_NOICON)) {
                int iy = out->caption.top + (bar_h - m->icon) / 2;
                rt_rect_set(&out->icon, left, iy, left + m->icon, iy + m->icon);
                left += m->icon;
            }
            continue;
        }
        if (tok == RT_LAY_GAP) {
            left += m->btn_gap;
            continue;
        }
        b = tok == RT_LAY_CLOSE ? RT_NCB_CLOSE : tok == RT_LAY_MIN ? RT_NCB_MIN : RT_NCB_MAX;
        if (show[b]) {
            rt_rect_set(&out->btn[b], left, by, left + m->btn_w, by + m->btn_h);
            left += m->btn_w;
        }
    }
    /* tokens after Title go right to left, starting from the last one */
    if (i < 8 && m->layout[i] == RT_LAY_TITLE) {
        int first = i + 1, last = first;
        while (last < 8 && m->layout[last] != RT_LAY_END)
            last++;
        for (i = last - 1; i >= first; i--) {
            int tok = m->layout[i], b;
            if (tok == RT_LAY_ICON) {
                if ((in->flags & RT_NC_SYSMENU) && !(in->flags & RT_NC_NOICON)) {
                    int iy = out->caption.top + (bar_h - m->icon) / 2;
                    rt_rect_set(&out->icon, right - m->icon, iy, right, iy + m->icon);
                    right -= m->icon;
                }
                continue;
            }
            if (tok == RT_LAY_GAP) {
                /* a gap only separates buttons that are actually shown */
                if (right != out->caption.right - 2)
                    right -= m->btn_gap;
                continue;
            }
            b = tok == RT_LAY_CLOSE ? RT_NCB_CLOSE : tok == RT_LAY_MIN ? RT_NCB_MIN : RT_NCB_MAX;
            if (show[b]) {
                rt_rect_set(&out->btn[b], right - m->btn_w, by, right, by + m->btn_h);
                right -= m->btn_w;
            }
        }
    }
    if (left > out->caption.left + 2)
        left += m->text_pad;
    if (right < out->caption.right - 2)
        right -= m->text_pad;
    rt_rect_set(&out->title, left, out->caption.top, right, out->caption.top + bar_h);
}

int rt_nc_hit(const RtNcLayout *l, int x, int y)
{
    if (rt_rect_contains(&l->btn[RT_NCB_CLOSE], x, y))
        return RT_HIT_CLOSE;
    if (rt_rect_contains(&l->btn[RT_NCB_MIN], x, y))
        return RT_HIT_MIN;
    if (rt_rect_contains(&l->btn[RT_NCB_MAX], x, y))
        return RT_HIT_MAX;
    if (rt_rect_contains(&l->icon, x, y))
        return RT_HIT_SYSMENU;
    if (rt_rect_contains(&l->caption, x, y))
        return RT_HIT_CAPTION;
    return RT_HIT_NONE;
}

void rt_user31_nc(const RtNcIn *in, RtUserNc *out)
{
    int l = in->frame_x, r = in->cx - in->frame_x, t = in->frame_y;
    int h = in->cysize > 0 ? in->cysize : in->caption - 1;
    int bw = in->cxsize + 1;
    rt_rect_set(&out->caption, l, t, r, t + h);
    if (in->flags & RT_NC_SYSMENU) {
        rt_rect_set(&out->sysbox, l, t, l + bw, t + h);
    } else {
        empty(&out->sysbox);
    }
    empty(&out->minbox);
    empty(&out->maxbox);
    if (in->flags & RT_NC_MAXBOX) {
        rt_rect_set(&out->maxbox, r - bw, t, r, t + h);
        r -= bw;
    }
    if (in->flags & RT_NC_MINBOX)
        rt_rect_set(&out->minbox, r - bw, t, r, t + h);
}

static void center(const RtRect *r, int *x, int *y)
{
    *x = (r->left + r->right) / 2;
    *y = (r->top + r->bottom) / 2;
}

int rt_nc_map_point(const RtNcLayout *theme, const RtUserNc *user, int x, int y, int *ux, int *uy)
{
    int hit = rt_nc_hit(theme, x, y);
    *ux = x;
    *uy = y;
    switch (hit) {
    case RT_HIT_MIN:
        if (!rt_rect_empty(&user->minbox))
            center(&user->minbox, ux, uy);
        else
            hit = RT_HIT_NONE;      /* disabled button: USER has no box */
        break;
    case RT_HIT_MAX:
        if (!rt_rect_empty(&user->maxbox))
            center(&user->maxbox, ux, uy);
        else
            hit = RT_HIT_NONE;
        break;
    case RT_HIT_SYSMENU:
        center(&user->sysbox, ux, uy);
        break;
    case RT_HIT_CAPTION:
    case RT_HIT_CLOSE:
        /* Inside the theme caption but over one of USER's boxes: move the
         * point onto plain caption so USER does not start tracking a box
         * the user cannot see. */
        if (rt_rect_contains(&user->sysbox, x, y) || rt_rect_contains(&user->minbox, x, y)
            || rt_rect_contains(&user->maxbox, x, y)) {
            int l = user->sysbox.right > user->caption.left ? user->sysbox.right : user->caption.left;
            int r = !rt_rect_empty(&user->minbox) ? user->minbox.left
                  : !rt_rect_empty(&user->maxbox) ? user->maxbox.left : user->caption.right;
            *ux = (l + r) / 2;
            *uy = (user->caption.top + user->caption.bottom) / 2;
        }
        break;
    }
    return hit;
}
