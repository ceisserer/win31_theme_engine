/*
 * scenes.c - mock windows, dialogs and menus drawn through the renderer.
 *
 * Shared by the previewer (preview.c) and the golden-image tests. The
 * scene layouts follow Win95's metrics at 96 dpi (frame 4, caption 18+1,
 * menu bar 19, scroll bar 16, check box 13, radio 12).
 */
#include <string.h>
#include "rt_theme.h"
#include "rt_geom.h"
#include "backend_sw.h"
#include "scenes.h"

static void req_init(RtDrawReq *q, int part, int state, int l, int t, int r, int b)
{
    memset(q, 0, sizeof(*q));
    q->part = (rt_u8)part;
    q->state = (rt_u8)state;
    rt_rect_set(&q->rc, l, t, r, b);
    q->textlen = -1;
}

static void part(const RtTheme *th, RtSurface *s, int p, int st, int l, int t, int r, int b)
{
    RtDrawReq q;
    req_init(&q, p, st, l, t, r, b);
    rt_render(th, s, &q);
}

static void part_rc(const RtTheme *th, RtSurface *s, int p, int st, const RtRect *rc)
{
    part(th, s, p, st, rc->left, rc->top, rc->right, rc->bottom);
}

static void text(const RtTheme *th, RtSurface *s, int p, int st, const RtRect *rc,
                 const char *str, int flags)
{
    RtDrawReq q;
    req_init(&q, p, st, rc->left, rc->top, rc->right, rc->bottom);
    q.text = str;
    q.flags = (rt_u16)flags;
    rt_render(th, s, &q);
}

static int text_w(const RtTheme *th, RtSurface *s, const char *font, const char *str)
{
    char buf[128];
    int n = 0;
    for (; *str && n < 127; str++)
        if (*str != '&')
            buf[n++] = *str;
    return bk_text_width(s, rt_theme_font(th, font), buf, n);
}

/* A generic 16x16 application icon (no artwork needed). */
static void draw_icon(RtSurface *s, const RtRect *r)
{
    RtRect a;
    if (rt_rect_empty(r))
        return;
    a = *r;
    rt_rect_inset(&a, 1, 2, 1, 2);
    bk_fill(s, &a, RT_RGB(0, 0, 0));
    rt_rect_inset(&a, 1, 1, 1, 1);
    bk_fill(s, &a, RT_RGB(255, 255, 255));
    a.bottom = (rt_s16)(a.top + 2);
    bk_fill(s, &a, RT_RGB(0, 0, 128));
}

/* Draws the non-client area of a window at (x, y) and returns its layout. */
static void window_frame(const RtTheme *th, RtSurface *s, int x, int y, const RtNcIn *in,
                         int active, int dialog, const char *title, RtNcLayout *l,
                         const int *btn_states)
{
    RtDrawReq q;
    int st = active ? RT_ST_ACTIVE : RT_ST_INACTIVE, i;
    static const int btn_parts[RT_NCB_COUNT] = { RT_PART_BTNCLOSE, RT_PART_BTNMIN, RT_PART_BTNMAX };

    sw_set_origin(s, x, y);
    rt_nc_layout(&th->metrics, in, l);
    req_init(&q, dialog ? RT_PART_DLGFRAME : RT_PART_FRAME, st, 0, 0, in->cx, in->cy);
    q.flags = RT_RQ_HOLE;
    q.hole = l->inner;
    rt_render(th, s, &q);
    part_rc(th, s, RT_PART_CAPTION, st, &l->caption);
    draw_icon(s, &l->icon);
    text(th, s, RT_PART_CAPTIONTEXT, st, &l->title, title, 0);
    for (i = 0; i < RT_NCB_COUNT; i++) {
        int p = btn_parts[i];
        if (rt_rect_empty(&l->btn[i]))
            continue;
        if (i == RT_NCB_MAX && (in->flags & RT_NC_MAXIMIZED))
            p = RT_PART_BTNRESTORE;
        part_rc(th, s, p, btn_states ? btn_states[i] : RT_ST_NORMAL, &l->btn[i]);
    }
}

static void menubar(const RtTheme *th, RtSurface *s, const RtRect *bar, const char *const *items,
                    int n, int selected)
{
    int i, x = bar->left;
    part_rc(th, s, RT_PART_MENUBAR, RT_ST_NORMAL, bar);
    for (i = 0; i < n; i++) {
        RtRect r;
        int w = text_w(th, s, "Menu", items[i]) + 12;
        int st = i == selected ? RT_ST_SELECTED : RT_ST_NORMAL;
        rt_rect_set(&r, x, bar->top + 1, x + w, bar->bottom - 1);
        part_rc(th, s, RT_PART_MENUBARITEM, st, &r);
        text(th, s, RT_PART_MENUBARTEXT, st, &r, items[i], RT_RQ_PREFIX);
        x += w;
    }
}

static void vscroll(const RtTheme *th, RtSurface *s, int l, int t, int b, int thumb_pos)
{
    part(th, s, RT_PART_SCROLLUP, RT_ST_NORMAL, l, t, l + 16, t + 16);
    part(th, s, RT_PART_SCROLLTRACKV, RT_ST_NORMAL, l, t + 16, l + 16, b - 16);
    part(th, s, RT_PART_SCROLLTHUMBV, RT_ST_NORMAL, l, t + 16 + thumb_pos, l + 16, t + 16 + thumb_pos + 24);
    part(th, s, RT_PART_SCROLLDOWN, RT_ST_NORMAL, l, b - 16, l + 16, b);
}

static void scene_window(const RtTheme *th, RtSurface *s, int x, int y, int active)
{
    static const char *const items[] = { "&File", "&Edit", "&Search", "&Help" };
    RtNcIn in;
    RtNcLayout l;
    RtRect c, sb;

    memset(&in, 0, sizeof(in));
    in.cx = 300;
    in.cy = 180;
    in.frame_x = in.frame_y = 4;
    in.caption = 19;
    in.menu = 19;
    in.cxsize = 18;
    in.flags = RT_NC_SYSMENU | RT_NC_MINBOX | RT_NC_MAXBOX | RT_NC_THICKFRAME;
    window_frame(th, s, x, y, &in, active, 0, active ? "Untitled - Notepad" : "Program Manager", &l, 0);
    menubar(th, s, &l.menubar, items, 4, active ? 1 : -1);

    /* client: sunken edit field with a vertical scroll bar */
    c = l.client;
    {
        RtDrawReq q;
        req_init(&q, RT_PART_FIELD, RT_ST_NORMAL, c.left, c.top, c.right, c.bottom);
        q.flags = RT_RQ_HOLE;
        q.hole = c;
        rt_rect_inset(&q.hole, 2, 2, 2, 2);
        rt_render(th, s, &q);
    }
    rt_rect_inset(&c, 2, 2, 2, 2);
    sb = c;
    sb.left = (rt_s16)(sb.right - 16);
    c.right = sb.left;
    bk_fill(s, &c, rt_syscolor(5));
    {
        RtRect tr = c;
        rt_rect_inset(&tr, 3, 2, 3, 0);
        tr.bottom = (rt_s16)(tr.top + 14);
        bk_text(s, rt_theme_font(th, "Controls"), tr.left, tr.top, &tr,
                "Hello from Windows 3.1!", 23, rt_syscolor(8));
    }
    vscroll(th, s, sb.left, sb.top, sb.bottom, 10);
    sw_set_origin(s, 0, 0);
}

static void label(const RtTheme *th, RtSurface *s, int l, int t, int r, int b, const char *str,
                  int disabled)
{
    RtRect rc;
    rt_rect_set(&rc, l, t, r, b);
    text(th, s, RT_PART_STATICTEXT, disabled ? RT_ST_DISABLED : RT_ST_NORMAL, &rc, str,
         RT_RQ_PREFIX | RT_RQ_LEFT);
}

static void button(const RtTheme *th, RtSurface *s, int l, int t, const char *str, int st,
                   int text_st, int focus)
{
    RtRect r;
    rt_rect_set(&r, l, t, l + 75, t + 23);
    part_rc(th, s, RT_PART_PUSHBUTTON, st, &r);
    text(th, s, RT_PART_PUSHBUTTONTEXT, text_st, &r, str, RT_RQ_PREFIX);
    if (focus) {
        rt_rect_inset(&r, 4, 4, 4, 4);
        part_rc(th, s, RT_PART_FOCUS, RT_ST_NORMAL, &r);
    }
}

static void check(const RtTheme *th, RtSurface *s, int p, int l, int t, int st, const char *str,
                  int disabled)
{
    int box = p == RT_PART_RADIO ? 12 : 13;
    part(th, s, p, st, l, t + (16 - box) / 2, l + box, t + (16 - box) / 2 + box);
    label(th, s, l + box + 4, t, l + 150, t + 16, str, disabled);
}

static void scene_dialog(const RtTheme *th, RtSurface *s, int x, int y)
{
    RtNcIn in;
    RtNcLayout l;
    RtRect c, g;
    int cx, cy;

    memset(&in, 0, sizeof(in));
    in.cx = 380;
    in.cy = 270;
    in.frame_x = in.frame_y = 3;
    in.caption = 19;
    in.cxsize = 18;
    in.flags = RT_NC_SYSMENU | RT_NC_NOICON;
    window_frame(th, s, x, y, &in, 1, 1, "Display Properties", &l, 0);
    c = l.client;
    part_rc(th, s, RT_PART_DIALOG, RT_ST_NORMAL, &c);
    cx = c.left;
    cy = c.top;

    /* group box with check boxes */
    rt_rect_set(&g, cx + 8, cy + 14, cx + 180, cy + 150);
    part_rc(th, s, RT_PART_GROUPBOX, RT_ST_NORMAL, &g);
    {
        RtRect lr;
        rt_rect_set(&lr, cx + 14, cy + 7, cx + 14 + text_w(th, s, "Controls", "Options") + 4, cy + 21);
        part_rc(th, s, RT_PART_DIALOG, RT_ST_NORMAL, &lr);
        lr.left = (rt_s16)(lr.left + 2);
        text(th, s, RT_PART_STATICTEXT, RT_ST_NORMAL, &lr, "Options", 0);
    }
    check(th, s, RT_PART_CHECKBOX, cx + 18, cy + 26, RT_ST_NORMAL, "&Normal", 0);
    check(th, s, RT_PART_CHECKBOX, cx + 18, cy + 46, RT_ST_CHECKED, "&Checked", 0);
    check(th, s, RT_PART_CHECKBOX, cx + 18, cy + 66, RT_ST_CHECKEDPRESSED, "Checked, pressed", 0);
    check(th, s, RT_PART_CHECKBOX, cx + 18, cy + 86, RT_ST_MIXED, "Indeterminate", 0);
    check(th, s, RT_PART_CHECKBOX, cx + 18, cy + 106, RT_ST_DISABLED, "Disabled", 1);
    check(th, s, RT_PART_CHECKBOX, cx + 18, cy + 126, RT_ST_CHECKEDDISABLED, "Checked, disabled", 1);

    /* radio buttons */
    check(th, s, RT_PART_RADIO, cx + 196, cy + 14, RT_ST_CHECKED, "&Selected", 0);
    check(th, s, RT_PART_RADIO, cx + 196, cy + 34, RT_ST_NORMAL, "&Unselected", 0);
    check(th, s, RT_PART_RADIO, cx + 196, cy + 54, RT_ST_PRESSED, "Pressed", 0);
    check(th, s, RT_PART_RADIO, cx + 196, cy + 74, RT_ST_CHECKEDDISABLED, "Disabled", 1);

    /* edit field and combo box */
    {
        RtDrawReq q;
        RtRect f;
        rt_rect_set(&f, cx + 196, cy + 100, cx + 360, cy + 121);
        req_init(&q, RT_PART_FIELD, RT_ST_NORMAL, f.left, f.top, f.right, f.bottom);
        q.flags = RT_RQ_HOLE;
        q.hole = f;
        rt_rect_inset(&q.hole, 2, 2, 2, 2);
        rt_render(th, s, &q);
        rt_rect_inset(&f, 2, 2, 2, 2);
        bk_fill(s, &f, rt_syscolor(5));
        f.left = (rt_s16)(f.left + 2);
        bk_text(s, rt_theme_font(th, "Controls"), f.left, f.top + 2, &f, "Edit control", 12, rt_syscolor(8));

        rt_rect_set(&f, cx + 196, cy + 128, cx + 360, cy + 149);
        req_init(&q, RT_PART_FIELD, RT_ST_NORMAL, f.left, f.top, f.right, f.bottom);
        q.flags = RT_RQ_HOLE;
        q.hole = f;
        rt_rect_inset(&q.hole, 2, 2, 2, 2);
        rt_render(th, s, &q);
        rt_rect_inset(&f, 2, 2, 2, 2);
        bk_fill(s, &f, rt_syscolor(5));
        part(th, s, RT_PART_COMBOBUTTON, RT_ST_NORMAL, f.right - 16, f.top, f.right, f.bottom);
        f.right = (rt_s16)(f.right - 16);
        rt_rect_inset(&f, 1, 1, 1, 1);
        bk_fill(s, &f, rt_syscolor(13));
        f.left = (rt_s16)(f.left + 1);
        bk_text(s, rt_theme_font(th, "Controls"), f.left, f.top + 1, &f, "Combo box", 9, rt_syscolor(14));
    }
    label(th, s, cx + 196, cy + 156, cx + 360, cy + 172, "Disabled static text", 1);

    /* push buttons */
    button(th, s, cx + 8, cy + 190, "OK", RT_ST_DEFAULT, RT_ST_NORMAL, 1);
    button(th, s, cx + 92, cy + 190, "Cancel", RT_ST_NORMAL, RT_ST_NORMAL, 0);
    button(th, s, cx + 176, cy + 190, "Pressed", RT_ST_PRESSED, RT_ST_PRESSED, 0);
    button(th, s, cx + 260, cy + 190, "&Apply", RT_ST_NORMAL, RT_ST_DISABLED, 0);
    sw_set_origin(s, 0, 0);
}

static void scene_menu(const RtTheme *th, RtSurface *s, int x, int y)
{
    static const char *const items[] = {
        "&New", "&Open...", "&Save", "Save &As...", "-", "&Word Wrap", "&Print", "Recent &Files", "-", "E&xit"
    };
    static const int flags[] = { 0, 1, 0, 0, 0, 2, 4, 8, 0, 0 };    /* 1 sel, 2 check, 4 disabled, 8 sub */
    int n = 10, i, w = 150, h = 4, yy;
    RtDrawReq q;

    for (i = 0; i < n; i++)
        h += items[i][0] == '-' ? 9 : 18;
    sw_set_origin(s, x, y);
    req_init(&q, RT_PART_MENU, RT_ST_NORMAL, 0, 0, w, h);
    rt_render(th, s, &q);
    yy = 2;
    for (i = 0; i < n; i++) {
        RtRect r, tr;
        int st = (flags[i] & 1) ? RT_ST_SELECTED : (flags[i] & 4) ? RT_ST_DISABLED : RT_ST_NORMAL;
        if (items[i][0] == '-') {
            part(th, s, RT_PART_MENUSEPARATOR, RT_ST_NORMAL, 3, yy + 3, w - 3, yy + 5);
            yy += 9;
            continue;
        }
        rt_rect_set(&r, 3, yy, w - 3, yy + 18);
        part_rc(th, s, RT_PART_MENUITEM, st == RT_ST_DISABLED ? RT_ST_NORMAL : st, &r);
        if (flags[i] & 2)
            part(th, s, RT_PART_MENUCHECK, st, r.left, r.top, r.left + 18, r.bottom);
        if (flags[i] & 8)
            part(th, s, RT_PART_MENUSUBARROW, st, r.right - 16, r.top, r.right - 4, r.bottom);
        tr = r;
        tr.left = (rt_s16)(tr.left + 18);
        text(th, s, RT_PART_MENUTEXT, st, &tr, items[i], RT_RQ_PREFIX);
        yy += 18;
    }
    sw_set_origin(s, 0, 0);
}

static void scene_buttons(const RtTheme *th, RtSurface *s, int x, int y)
{
    static const int parts[] = { RT_PART_BTNCLOSE, RT_PART_BTNMIN, RT_PART_BTNMAX, RT_PART_BTNRESTORE };
    static const int states[] = { RT_ST_NORMAL, RT_ST_PRESSED, RT_ST_DISABLED };
    static const int sparts[] = { RT_PART_SCROLLUP, RT_PART_SCROLLDOWN, RT_PART_SCROLLLEFT, RT_PART_SCROLLRIGHT };
    int i, j;
    RtRect bg;
    sw_set_origin(s, x, y);
    rt_rect_set(&bg, 0, 0, 4 * 22 + 6, 3 * 20 + 3 * 20 + 6);
    part_rc(th, s, RT_PART_DIALOG, RT_ST_NORMAL, &bg);
    for (j = 0; j < 3; j++)
        for (i = 0; i < 4; i++)
            part(th, s, parts[i], states[j], 4 + i * 22, 4 + j * 20, 4 + i * 22 + 16, 4 + j * 20 + 14);
    for (j = 0; j < 3; j++)
        for (i = 0; i < 4; i++)
            part(th, s, sparts[i], states[j], 4 + i * 22, 64 + j * 20, 4 + i * 22 + 16, 64 + j * 20 + 16);
    sw_set_origin(s, 0, 0);
}

static const char *const scene_names[SCENE_COUNT] = { "window", "inactive", "dialog", "menu", "buttons" };

int scene_from_name(const char *name)
{
    int i;
    for (i = 0; i < SCENE_COUNT; i++)
        if (strcmp(scene_names[i], name) == 0)
            return i;
    return -1;
}

const char *scene_name(int scene)
{
    return scene >= 0 && scene < SCENE_COUNT ? scene_names[scene] : "?";
}

RtSurface *scene_render(const RtTheme *th, int scene)
{
    RtSurface *s;
    RtRect all;
    switch (scene) {
    case SCENE_WINDOW:
    case SCENE_INACTIVE:
        s = sw_surface_create(320, 200, rt_syscolor(1));
        scene_window(th, s, 10, 10, scene == SCENE_WINDOW);
        return s;
    case SCENE_DIALOG:
        s = sw_surface_create(400, 290, rt_syscolor(1));
        scene_dialog(th, s, 10, 10);
        return s;
    case SCENE_MENU:
        s = sw_surface_create(170, 200, rt_syscolor(1));
        scene_menu(th, s, 10, 10);
        return s;
    case SCENE_BUTTONS:
        s = sw_surface_create(110, 140, rt_syscolor(1));
        rt_rect_set(&all, 0, 0, 110, 140);
        scene_buttons(th, s, 6, 6);
        return s;
    }
    return 0;
}
