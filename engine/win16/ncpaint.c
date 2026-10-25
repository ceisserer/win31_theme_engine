/*
 * ncpaint.c - themed frames, captions and caption buttons (plan phase 3).
 *
 * Strategy of this first version ("suppress and overpaint"):
 *   1. while USER paints the non-client area of a themed window, the GDI
 *      interceptors drop everything USER draws inside its caption bar;
 *   2. when USER is done (one-shot subclass, hooks.c), we paint the theme's
 *      frame, caption, icon, title and buttons over the whole non-client
 *      area.
 * Hit-testing is remapped from the theme layout to USER's layout, so USER's
 * own logic (move, size, system menu, min/max tracking) keeps working. The
 * close button has no USER equivalent and is tracked here.
 *
 * The USER 3.1 geometry used here (rt_user31_nc) is a first model and has
 * to be verified against real USER (calibration, plan 4.3).
 */
#include "rt16.h"

HWND   g_nc_hwnd;
BOOL   g_nc_active;
RtRect g_nc_userbar;
int    g_track_hit;
HWND   g_track_hwnd;
BOOL   g_track_down;

static HWND close_hwnd;         /* close button being tracked */
static BOOL close_down;

BOOL nc_wants(HWND hwnd)
{
    DWORD style;
    WState FAR *ws;
    if (!g_running || !g_theme || !IsWindow(hwnd))
        return FALSE;
    style = GetWindowLong(hwnd, GWL_STYLE);
    if (style & WS_CHILD)
        return FALSE;           /* MDI children: later */
    if ((style & WS_CAPTION) != WS_CAPTION)
        return FALSE;
    if (IsIconic(hwnd))
        return FALSE;
    ws = ws_get(hwnd, FALSE);
    if (ws && (ws->flags & WS_F_EXCLUDED))
        return FALSE;
    return TRUE;
}

void nc_fill_input(HWND hwnd, RtNcIn FAR *in)
{
    RECT wr;
    POINT pt;
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    DWORD ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    int fx, fy, client_top, menu = 0;

    GetWindowRect(hwnd, &wr);
    in->cx = (rt_s16)(wr.right - wr.left);
    in->cy = (rt_s16)(wr.bottom - wr.top);
    if (style & WS_THICKFRAME) {
        fx = GetSystemMetrics(SM_CXFRAME);
        fy = GetSystemMetrics(SM_CYFRAME);
    } else if ((style & WS_DLGFRAME) || (ex & WS_EX_DLGMODALFRAME)) {
        /* measured: USER draws a 1-pixel border outside the dialog frame
         * when the window also has WS_BORDER (every captioned dialog) */
        fx = GetSystemMetrics(SM_CXDLGFRAME);
        fy = GetSystemMetrics(SM_CYDLGFRAME);
        if ((style & WS_BORDER) && (ex & WS_EX_DLGMODALFRAME)) {
            fx += GetSystemMetrics(SM_CXBORDER);
            fy += GetSystemMetrics(SM_CYBORDER);
        }
    } else {
        fx = GetSystemMetrics(SM_CXBORDER);
        fy = GetSystemMetrics(SM_CYBORDER);
    }
    in->frame_x = (rt_s16)fx;
    in->frame_y = (rt_s16)fy;
    pt.x = pt.y = 0;
    ClientToScreen(hwnd, &pt);
    client_top = pt.y - wr.top;
    if (GetMenu(hwnd))
        menu = GetSystemMetrics(SM_CYMENU) + GetSystemMetrics(SM_CYBORDER);  /* + line below */
    /* The caption band runs from the frame to the menu bar / client area.
     * Multi-line menu bars make the menu taller than SM_CYMENU; the band is
     * capped at SM_CYCAPTION for that reason. */
    in->caption = (rt_s16)(client_top - fy - menu);
    if (in->caption > GetSystemMetrics(SM_CYCAPTION))
        in->caption = (rt_s16)GetSystemMetrics(SM_CYCAPTION);
    in->menu = (rt_s16)(client_top - fy - in->caption);
    in->cxsize = (rt_s16)GetSystemMetrics(SM_CXSIZE);
    in->cysize = (rt_s16)GetSystemMetrics(SM_CYSIZE);
    in->flags = 0;
    if (style & WS_SYSMENU)
        in->flags |= RT_NC_SYSMENU;
    if (style & WS_MINIMIZEBOX)
        in->flags |= RT_NC_MINBOX;
    if (style & WS_MAXIMIZEBOX)
        in->flags |= RT_NC_MAXBOX;
    if (IsZoomed(hwnd))
        in->flags |= RT_NC_MAXIMIZED;
    if (style & WS_THICKFRAME)
        in->flags |= RT_NC_THICKFRAME;
    if (ex & WS_EX_DLGMODALFRAME)
        in->flags |= RT_NC_NOICON;
}

static void layouts(HWND hwnd, RtNcIn FAR *in, RtNcLayout FAR *l, RtUserNc FAR *u)
{
    nc_fill_input(hwnd, in);
    rt_nc_layout(&g_theme->metrics, in, l);
    rt_user31_nc(in, u);
}

void nc_begin(HWND hwnd, BOOL active)
{
    RtNcIn in;
    RtUserNc u;
    nc_fill_input(hwnd, &in);
    rt_user31_nc(&in, &u);
    g_nc_hwnd = hwnd;
    g_nc_active = active;
    g_nc_userbar = u.caption;
    patch_arm(P_PATBLT, ARM_NC);
    patch_arm(P_BITBLT, ARM_NC);
    patch_arm(P_EXTTEXTOUT, ARM_NC);
    patch_arm(P_TEXTOUT, ARM_NC);
    patch_arm(P_STRETCHBLT, ARM_NC);       /* scroll bar arrows */
    patch_arm(P_FASTWINDOWFRAME, ARM_NC);  /* scroll bar and thumb outlines */
    if (menu_has_bar(hwnd))
        menu_measure_begin(hwnd);
}

void nc_end(HWND hwnd)
{
    HDC hdc;
    BOOL active = g_nc_active;
    patch_disarm(P_PATBLT, ARM_NC);
    patch_disarm(P_BITBLT, ARM_NC);
    patch_disarm(P_EXTTEXTOUT, ARM_NC);
    patch_disarm(P_TEXTOUT, ARM_NC);
    patch_disarm(P_STRETCHBLT, ARM_NC);
    patch_disarm(P_FASTWINDOWFRAME, ARM_NC);
    if (menu_has_bar(hwnd))
        menu_measure_end(hwnd);
    g_nc_hwnd = 0;
    if (!IsWindow(hwnd) || !nc_wants(hwnd))
        return;
    hdc = GetWindowDC(hwnd);
    if (hdc) {
        nc_paint(hwnd, hdc, active);
        ReleaseDC(hwnd, hdc);
    }
}

/* Is a system-menu command disabled? (Win95 greys the matching button.) */
static BOOL cmd_disabled(HWND hwnd, UINT cmd)
{
    HMENU m = GetSystemMenu(hwnd, FALSE);
    UINT st;
    if (!m)
        return TRUE;
    st = GetMenuState(m, cmd, MF_BYCOMMAND);
    return st == (UINT)-1 || (st & (MF_GRAYED | MF_DISABLED)) != 0;
}

static void draw_icon(HWND hwnd, HDC hdc, const RtRect FAR *r, BOOL active)
{
    HICON icon = (HICON)GetClassWord(hwnd, GCW_HICON);
    int cx, cy;
    HDC mem;
    HBITMAP bmp, old;
    HBRUSH br;
    RECT rc;
    if (rt_rect_empty(r))
        return;
    if (!icon)
        icon = LoadIcon(NULL, IDI_APPLICATION);
    cx = GetSystemMetrics(SM_CXICON);
    cy = GetSystemMetrics(SM_CYICON);
    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, cx, cy);
    if (!mem || !bmp) {
        if (mem)
            DeleteDC(mem);
        if (bmp)
            DeleteObject(bmp);
        return;
    }
    old = SelectObject(mem, bmp);
    /* background: the caption colour under the icon */
    br = CreateSolidBrush(GetSysColor(active ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION));
    SetRect(&rc, 0, 0, cx, cy);
    FillRect(mem, &rc, br);
    DeleteObject(br);
    DrawIcon(mem, 0, 0, icon);
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, r->left, r->top, RT_RECT_W(r), RT_RECT_H(r), mem, 0, 0, cx, cy, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void draw_button(RtSurface FAR *s, HWND hwnd, const RtNcIn FAR *in, const RtNcLayout FAR *l,
                        int b, BOOL pressed)
{
    RtDrawReq q;
    _fmemset(&q, 0, sizeof(q));
    if (rt_rect_empty(&l->btn[b]))
        return;
    q.rc = l->btn[b];
    q.state = pressed ? RT_ST_PRESSED : RT_ST_NORMAL;
    switch (b) {
    case RT_NCB_CLOSE:
        q.part = RT_PART_BTNCLOSE;
        if (cmd_disabled(hwnd, SC_CLOSE))
            q.state = RT_ST_DISABLED;
        break;
    case RT_NCB_MIN:
        q.part = RT_PART_BTNMIN;
        if (!(in->flags & RT_NC_MINBOX) || cmd_disabled(hwnd, SC_MINIMIZE))
            q.state = RT_ST_DISABLED;
        break;
    default:
        q.part = (in->flags & RT_NC_MAXIMIZED) ? RT_PART_BTNRESTORE : RT_PART_BTNMAX;
        if (!(in->flags & RT_NC_MAXBOX)
            || cmd_disabled(hwnd, (in->flags & RT_NC_MAXIMIZED) ? SC_RESTORE : SC_MAXIMIZE))
            q.state = RT_ST_DISABLED;
        break;
    }
    rt_render(g_theme, s, &q);
}

void nc_paint(HWND hwnd, HDC hdc, BOOL active)
{
    RtNcIn in;
    RtNcLayout l;
    RtUserNc u;
    RtSurface s;
    RtDrawReq q;
    char title[96];
    int st = active ? RT_ST_ACTIVE : RT_ST_INACTIVE, b;
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);

    layouts(hwnd, &in, &l, &u);
    patch_suspend();
    gdi_surface(&s, hdc);

    _fmemset(&q, 0, sizeof(q));
    q.part = (style & WS_THICKFRAME) ? RT_PART_FRAME : RT_PART_DLGFRAME;
    q.state = (rt_u8)st;
    q.rc = l.window;
    q.flags = RT_RQ_HOLE;
    q.hole = l.inner;
    rt_render(g_theme, &s, &q);

    if (!rt_rect_empty(&l.caption)) {
        _fmemset(&q, 0, sizeof(q));
        q.part = RT_PART_CAPTION;
        q.state = (rt_u8)st;
        q.rc = l.caption;
        rt_render(g_theme, &s, &q);

        draw_icon(hwnd, hdc, &l.icon, active);

        GetWindowText(hwnd, title, sizeof(title));
        _fmemset(&q, 0, sizeof(q));
        q.part = RT_PART_CAPTIONTEXT;
        q.state = (rt_u8)st;
        q.rc = l.title;
        q.text = title;
        q.textlen = -1;
        rt_render(g_theme, &s, &q);

        for (b = 0; b < RT_NCB_COUNT; b++) {
            BOOL pressed = (b == RT_NCB_CLOSE && close_hwnd == hwnd && close_down);
            draw_button(&s, hwnd, &in, &l, b, pressed);
        }
    }
    patch_resume();
    {
        WState FAR *ws = ws_get(hwnd, TRUE);
        if (ws) {
            ws->flags |= WS_F_THEMED;
            if (active)
                ws->flags |= WS_F_ACTIVE;
            else
                ws->flags &= ~WS_F_ACTIVE;
            ws->nc = in;
        }
    }
}

/* Draws one caption button (USER's tracking loop, close tracking). */
static void paint_button(HWND hwnd, int b, BOOL pressed)
{
    RtNcIn in;
    RtNcLayout l;
    RtUserNc u;
    RtSurface s;
    HDC hdc = GetWindowDC(hwnd);
    if (!hdc)
        return;
    layouts(hwnd, &in, &l, &u);
    patch_suspend();
    gdi_surface(&s, hdc);
    draw_button(&s, hwnd, &in, &l, b, pressed);
    patch_resume();
    ReleaseDC(hwnd, hdc);
}

void nc_track_paint(HWND hwnd, BOOL pressed)
{
    paint_button(hwnd, g_track_hit == RT_HIT_MIN ? RT_NCB_MIN : RT_NCB_MAX, pressed);
}

/* PtInRect during USER's min/max tracking: answer with the theme button.
 * USER tests a rectangle slightly larger than the box it drew, in window or
 * screen coordinates; it is recognised by its centre lying in our model of
 * the box. */
BOOL nc_track_ptinrect(const RECT FAR *rc, POINT pt, BOOL FAR *handled)
{
    RtNcIn in;
    RtNcLayout l;
    RtUserNc u;
    RECT wr;
    const RtRect FAR *ub, FAR *tb;
    int space;
    *handled = FALSE;
    if (!g_track_hwnd || !IsWindow(g_track_hwnd))
        return FALSE;
    layouts(g_track_hwnd, &in, &l, &u);
    GetWindowRect(g_track_hwnd, &wr);
    ub = g_track_hit == RT_HIT_MIN ? &u.minbox : &u.maxbox;
    tb = g_track_hit == RT_HIT_MIN ? &l.btn[RT_NCB_MIN] : &l.btn[RT_NCB_MAX];
    for (space = 0; space < 2; space++) {
        int ox = space ? wr.left : 0, oy = space ? wr.top : 0;
        int cx = (rc->left + rc->right) / 2 - ox, cy = (rc->top + rc->bottom) / 2 - oy;
        if (!rt_rect_contains(ub, cx, cy))
            continue;
        *handled = TRUE;
        g_track_down = rt_rect_contains(tb, pt.x - ox, pt.y - oy);
        return g_track_down;
    }
    return FALSE;
}

int nc_map_hittest(HWND hwnd, LPARAM FAR *lParam)
{
    RtNcIn in;
    RtNcLayout l;
    RtUserNc u;
    RECT wr;
    int x, y, ux, uy, hit;
    if (!nc_wants(hwnd))
        return RT_HIT_NONE;
    GetWindowRect(hwnd, &wr);
    layouts(hwnd, &in, &l, &u);
    x = (int)(short)LOWORD(*lParam) - wr.left;
    y = (int)(short)HIWORD(*lParam) - wr.top;
    hit = rt_nc_map_point(&l, &u, x, y, &ux, &uy);
    if (ux != x || uy != y)
        *lParam = MAKELPARAM(ux + wr.left, uy + wr.top);
    return hit;
}

/* ---- the close button: tracked by the engine (WH_GETMESSAGE) ---- */

static BOOL in_close(HWND hwnd, POINT pt, BOOL screen)
{
    RtNcIn in;
    RtNcLayout l;
    RtUserNc u;
    RECT wr;
    GetWindowRect(hwnd, &wr);
    if (!screen)
        ClientToScreen(hwnd, &pt);
    layouts(hwnd, &in, &l, &u);
    return rt_rect_contains(&l.btn[RT_NCB_CLOSE], pt.x - wr.left, pt.y - wr.top);
}

BOOL nc_close_filter(MSG FAR *msg)
{
    POINT pt;
    if (close_hwnd) {
        if (msg->hwnd != close_hwnd)
            return FALSE;
        switch (msg->message) {
        case WM_MOUSEMOVE:
            pt.x = (int)(short)LOWORD(msg->lParam);
            pt.y = (int)(short)HIWORD(msg->lParam);
            if (in_close(close_hwnd, pt, FALSE) != close_down) {
                close_down = !close_down;
                paint_button(close_hwnd, RT_NCB_CLOSE, close_down);
            }
            msg->message = WM_NULL;
            return TRUE;
        case WM_LBUTTONUP: {
            HWND h = close_hwnd;
            BOOL fire = close_down;
            close_hwnd = 0;
            close_down = FALSE;
            ReleaseCapture();
            paint_button(h, RT_NCB_CLOSE, FALSE);
            msg->message = WM_NULL;
            if (fire)
                PostMessage(h, WM_SYSCOMMAND, SC_CLOSE, 0L);
            return TRUE;
        }
        }
        return FALSE;
    }
    if ((msg->message == WM_NCLBUTTONDOWN || msg->message == WM_NCLBUTTONDBLCLK)
        && nc_wants(msg->hwnd)) {
        pt.x = (int)(short)LOWORD(msg->lParam);
        pt.y = (int)(short)HIWORD(msg->lParam);
        if (in_close(msg->hwnd, pt, TRUE)) {
            msg->message = WM_NULL;
            if (cmd_disabled(msg->hwnd, SC_CLOSE))
                return TRUE;
            close_hwnd = msg->hwnd;
            close_down = TRUE;
            SetCapture(close_hwnd);
            paint_button(close_hwnd, RT_NCB_CLOSE, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}

/* USER inverts its system-menu box while the system menu is open; the theme
 * draws an icon there instead, so the inversion is dropped. */
BOOL nc_sysbox_invert(HWND hwnd, HDC hdc, int x, int y, int w, int h)
{
    RtNcIn in;
    RtUserNc u;
    RECT wr;
    DWORD org = GetDCOrg(hdc);
    if (!nc_wants(hwnd))
        return FALSE;
    GetWindowRect(hwnd, &wr);
    if ((int)LOWORD(org) != wr.left || (int)HIWORD(org) != wr.top)
        return FALSE;
    nc_fill_input(hwnd, &in);
    rt_user31_nc(&in, &u);
    return x >= u.sysbox.left - 1 && y >= u.sysbox.top - 1 && x + w <= u.sysbox.right + 1
        && y + h <= u.sysbox.bottom + 1;
}
