/*
 * scroll.c - Win95 look for scroll bars: window scroll bars (non-client)
 * and scroll bar controls.
 *
 * USER 3.1 draws a scroll bar with (measured with the trace):
 *   - FastWindowFrame around the whole bar,
 *   - StretchBlt of each arrow from its bitmap strip (17x17),
 *   - PatBlt(PATCOPY) of the track with the COLOR_SCROLLBAR brush,
 *   - FastWindowFrame around the thumb, then PatBlt lines for its 3D look.
 * On a scroll bar DC these become the theme's track, arrow buttons and
 * thumb. Besides WM_NCPAINT / WM_PAINT, USER repaints scroll bars inside
 * SetScrollPos, SetScrollRange, EnableScrollBar and its tracking loop; each
 * of those runs in a scroll context (scroll_begin / scroll_end).
 */
#include "rt16.h"

#define MAX_DEPTH 4
#define SBS_VERT16 0x0001

HWND g_scroll_hwnd;

static HWND stack[MAX_DEPTH];
static int  depth;
static RECT thumb;          /* thumb drawn last (its 3D lines are dropped) */
static HDC  thumb_dc;

static const int scroll_patches[] = { P_PATBLT, P_BITBLT, P_STRETCHBLT, P_FASTWINDOWFRAME };

static void arm(BOOL on)
{
    int i;
    for (i = 0; i < (int)(sizeof(scroll_patches) / sizeof(scroll_patches[0])); i++) {
        if (on)
            patch_arm(scroll_patches[i], ARM_SCROLL);
        else
            patch_disarm(scroll_patches[i], ARM_SCROLL);
    }
}

void scroll_begin(HWND hwnd)
{
    if (depth < MAX_DEPTH)
        stack[depth] = hwnd;
    depth++;
    g_scroll_hwnd = hwnd;
    arm(TRUE);
}

void scroll_end(HWND hwnd)
{
    (void)hwnd;
    arm(FALSE);
    if (depth > 0)
        depth--;
    g_scroll_hwnd = depth > 0 && depth <= MAX_DEPTH ? stack[depth - 1] : 0;
    if (!depth)
        thumb_dc = 0;
}

static BOOL themed_control(HWND hwnd)
{
    HWND parent;
    if (ctl_class(hwnd) != CLS_SCROLLBAR)
        return FALSE;
    parent = GetParent(hwnd);
    return parent && ctl_class(parent) == CLS_DIALOG;
}

/* Windows whose own scroll bars are themed: themed top-level windows, and
 * child windows in dialogs (list boxes, multi-line edit controls). */
static BOOL window_themed(HWND hwnd)
{
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    HWND parent;
    if (!(style & (WS_VSCROLL | WS_HSCROLL)))
        return FALSE;
    if (!(style & WS_CHILD))
        return nc_wants(hwnd);
    parent = GetParent(hwnd);
    return parent && ctl_class(parent) == CLS_DIALOG;
}

BOOL scroll_wants(HWND hwnd, UINT msg, WPARAM wp)
{
    if (!g_running || !g_theme)
        return FALSE;
    if (msg == WM_NCLBUTTONDOWN || msg == WM_NCLBUTTONDBLCLK)
        return (wp == HTVSCROLL || wp == HTHSCROLL) && window_themed(hwnd);
    if (msg == WM_NCPAINT)
        return (GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) && window_themed(hwnd);
    switch (msg) {
    case WM_PAINT: case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_ENABLE:
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_KEYDOWN:
    /* the control tracks the mouse in its own messages after the click */
    case WM_LBUTTONUP: case WM_MOUSEMOVE: case WM_TIMER: case WM_KEYUP: case WM_CANCELMODE:
        return themed_control(hwnd);
    }
    return FALSE;
}

/* Window scroll bars in window coordinates (measured: the bars overlap
 * the client edge by one pixel). */
static void nc_bars(HWND hwnd, RECT FAR *v, RECT FAR *h)
{
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    RECT wr, cr;
    POINT pt;
    int l, t, r, b;
    GetWindowRect(hwnd, &wr);
    GetClientRect(hwnd, &cr);
    pt.x = pt.y = 0;
    ClientToScreen(hwnd, &pt);
    l = pt.x - wr.left;
    t = pt.y - wr.top;
    r = l + cr.right;
    b = t + cr.bottom;
    SetRectEmpty(v);
    SetRectEmpty(h);
    if (style & WS_VSCROLL)
        SetRect(v, r, t - 1, r + GetSystemMetrics(SM_CXVSCROLL), b + 1);
    if (style & WS_HSCROLL)
        SetRect(h, l - 1, b, r + 1, b + GetSystemMetrics(SM_CYHSCROLL));
}

static BOOL inside(const RECT FAR *r, int x, int y, int w, int h)
{
    return !IsRectEmpty(r) && x >= r->left && y >= r->top && x + w <= r->right && y + h <= r->bottom;
}

/* Finds the scroll bar the call draws into, in DC coordinates. */
static BOOL find_bar(HDC hdc, int x, int y, int w, int h, RECT FAR *bar, BOOL FAR *vert)
{
    HWND cand[2];
    DWORD org = GetDCOrg(hdc);
    int i;
    cand[0] = g_scroll_hwnd;
    cand[1] = g_nc_hwnd;
    for (i = 0; i < 2; i++) {
        HWND hwnd = cand[i];
        if (!hwnd || !IsWindow(hwnd))
            continue;
        if (ctl_class(hwnd) == CLS_SCROLLBAR) {
            POINT pt;
            pt.x = pt.y = 0;
            ClientToScreen(hwnd, &pt);
            if ((int)LOWORD(org) != pt.x || (int)HIWORD(org) != pt.y)
                continue;
            GetClientRect(hwnd, bar);
            *vert = (GetWindowLong(hwnd, GWL_STYLE) & SBS_VERT16) != 0;
            if (inside(bar, x, y, w, h))
                return TRUE;
        } else {
            RECT wr, v, hz;
            GetWindowRect(hwnd, &wr);
            if ((int)LOWORD(org) != wr.left || (int)HIWORD(org) != wr.top)
                continue;
            nc_bars(hwnd, &v, &hz);
            if (inside(&v, x, y, w, h)) {
                *bar = v;
                *vert = TRUE;
                return TRUE;
            }
            if (inside(&hz, x, y, w, h)) {
                *bar = hz;
                *vert = FALSE;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void render(HDC hdc, int part, int state, int l, int t, int r, int b)
{
    RtSurface s;
    RtDrawReq q;
    _fmemset(&q, 0, sizeof(q));
    q.part = (rt_u8)part;
    q.state = (rt_u8)state;
    rt_rect_set(&q.rc, l, t, r, b);
    gdi_surface(&s, hdc);
    rt_render(g_theme, &s, &q);
}

static BOOL near_color(HDC dc, COLORREF c, int sys)
{
    return c == GetNearestColor(dc, GetSysColor(sys)) || c == GetSysColor(sys);
}

BOOL scroll_gdi(int op, HDC hdc, int x, int y, int w, int h, HDC src, int sx, int sy, DWORD extra)
{
    RECT bar;
    BOOL vert, done = FALSE;
    if (!g_theme || w <= 0 || h <= 0 || !find_bar(hdc, x, y, w, h, &bar, &vert))
        return FALSE;
    if (g_trace)
        log_msg("scroll op %d %d,%d %dx%d bar %d,%d,%d,%d vert %d", op, x, y, w, h, bar.left, bar.top,
                bar.right, bar.bottom, vert);
    patch_suspend();
    switch (op) {
    case GOP_FRAME:
        if (x == bar.left && y == bar.top && x + w == bar.right && y + h == bar.bottom) {
            /* The whole bar (drawn after the arrows): Win95 has no black
             * outline, so the track covers it between the arrow buttons. */
            int a = vert ? bar.right - bar.left : bar.bottom - bar.top;
            if (vert)
                render(hdc, RT_PART_SCROLLTRACKV, RT_ST_NORMAL, bar.left, bar.top + a, bar.right, bar.bottom - a);
            else
                render(hdc, RT_PART_SCROLLTRACKH, RT_ST_NORMAL, bar.left + a, bar.top, bar.right - a, bar.bottom);
            thumb_dc = 0;
        } else if (vert ? (y == bar.top || y + h == bar.bottom) : (x == bar.left || x + w == bar.right)) {
            /* outline of an arrow button: the arrow is already drawn */
        } else {
            render(hdc, vert ? RT_PART_SCROLLTHUMBV : RT_PART_SCROLLTHUMBH, RT_ST_NORMAL, x, y, x + w, y + h);
            SetRect(&thumb, x, y, x + w, y + h);
            thumb_dc = hdc;
        }
        done = TRUE;
        break;
    case GOP_PATBLT:
        if (thumb_dc == hdc && inside(&thumb, x, y, w, h)) {
            done = TRUE;            /* USER's 3D lines of the thumb */
        } else if (near_color(hdc, (COLORREF)extra, COLOR_SCROLLBAR) || w == 1 || h == 1) {
            /* track, or a piece of the bar's outline USER redraws when the
             * thumb moves (Win95 has no outline). USER's track fills leave
             * out the outline rows; ours cover the full bar thickness, or
             * the edges of a previous thumb would stay behind. */
            if (vert)
                render(hdc, RT_PART_SCROLLTRACKV, RT_ST_NORMAL, bar.left, y, bar.right, y + h);
            else
                render(hdc, RT_PART_SCROLLTRACKH, RT_ST_NORMAL, x, bar.top, x + w, bar.bottom);
            done = TRUE;
        }
        break;
    case GOP_BITBLT:
    case GOP_STRETCHBLT: {
        int part, st = RT_ST_NORMAL;
        COLORREF tl, mid;
        if (vert)
            part = y == bar.top ? RT_PART_SCROLLUP : RT_PART_SCROLLDOWN;
        else
            part = x == bar.left ? RT_PART_SCROLLLEFT : RT_PART_SCROLLRIGHT;
        if (vert ? (y != bar.top && y + h != bar.bottom) : (x != bar.left && x + w != bar.right))
            break;
        /* state from USER's arrow bitmap: a raised arrow has a white
         * top-left highlight; a disabled one has a grey glyph */
        tl = GetPixel(src, sx + 1, sy + 1);
        mid = GetPixel(src, sx + 8, sy + 8);
        if (!near_color(src, tl, COLOR_BTNHIGHLIGHT))
            st = RT_ST_PRESSED;
        else if (near_color(src, mid, COLOR_BTNSHADOW) || near_color(src, mid, COLOR_GRAYTEXT))
            st = RT_ST_DISABLED;
        render(hdc, part, st, x, y, x + w, y + h);
        done = TRUE;
        break;
    }
    }
    patch_resume();
    return done;
}

/* ---- scroll APIs: USER repaints the bar inside them ---- */

static BOOL api_themed(HWND hwnd, int bar)
{
    if (!g_running || !g_theme || !IsWindow(hwnd))
        return FALSE;
    return bar == SB_CTL ? themed_control(hwnd) : window_themed(hwnd);
}

int RT_HANDLER h_SetScrollPos(HWND hwnd, int bar, int pos, BOOL redraw)
{
    int r;
    BOOL ctx = redraw && api_themed(hwnd, bar);
    if (ctx)
        scroll_begin(hwnd);
    patch_unhook(P_SETSCROLLPOS);
    r = SetScrollPos(hwnd, bar, pos, redraw);
    patch_rehook(P_SETSCROLLPOS);
    if (ctx)
        scroll_end(hwnd);
    return r;
}

void RT_HANDLER h_SetScrollRange(HWND hwnd, int bar, int lo, int hi, BOOL redraw)
{
    BOOL ctx = redraw && api_themed(hwnd, bar);
    if (ctx)
        scroll_begin(hwnd);
    patch_unhook(P_SETSCROLLRANGE);
    SetScrollRange(hwnd, bar, lo, hi, redraw);
    patch_rehook(P_SETSCROLLRANGE);
    if (ctx)
        scroll_end(hwnd);
}

BOOL RT_HANDLER h_EnableScrollBar(HWND hwnd, UINT bar, UINT arrows)
{
    BOOL r;
    BOOL ctx = api_themed(hwnd, (int)bar);
    if (ctx)
        scroll_begin(hwnd);
    patch_unhook(P_ENABLESCROLLBAR);
    r = EnableScrollBar(hwnd, bar, arrows);
    patch_rehook(P_ENABLESCROLLBAR);
    if (ctx)
        scroll_end(hwnd);
    return r;
}

void scroll_init(void)
{
    patch_arm(P_SETSCROLLPOS, ARM_PERM);
    patch_arm(P_SETSCROLLRANGE, ARM_PERM);
    patch_arm(P_ENABLESCROLLBAR, ARM_PERM);
}
