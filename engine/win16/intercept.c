/*
 * intercept.c - handlers for the patched GDI/USER functions.
 *
 * A handler runs instead of the original function while its patch is armed.
 * It either drops/replaces what USER draws or calls the original with the
 * patches suspended. Handlers run on the caller's stack: keep them small.
 */
#include "rt16.h"

/* Window-relative rectangle of a GDI call on hdc, if hdc is a window DC of
 * hwnd (its origin is the window's top-left corner). */
static BOOL window_dc(HDC hdc, HWND hwnd)
{
    RECT wr;
    DWORD org;
    if (!hwnd)
        return FALSE;
    org = GetDCOrg(hdc);
    GetWindowRect(hwnd, &wr);
    return (int)LOWORD(org) == wr.left && (int)HIWORD(org) == wr.top;
}

static BOOL rect_inside(const RtRect FAR *outer, int x, int y, int w, int h)
{
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }
    return x >= outer->left && y >= outer->top && x + w <= outer->right && y + h <= outer->bottom;
}

/* Something USER draws into its own caption bar while we theme it. */
static BOOL nc_drop(HDC hdc, int x, int y, int w, int h)
{
    if (!g_nc_hwnd || !window_dc(hdc, g_nc_hwnd))
        return FALSE;
    return rect_inside(&g_nc_userbar, x, y, w, h);
}

static BOOL nc_drop_point(HDC hdc, int x, int y)
{
    if (!g_nc_hwnd || !window_dc(hdc, g_nc_hwnd))
        return FALSE;
    return rt_rect_contains(&g_nc_userbar, x, y);
}

/* During USER's min/max tracking loop: is this blit the button glyph? */
static BOOL track_blit(HDC hdc, int x, int y, int w, int h)
{
    RtNcIn in;
    RtUserNc u;
    const RtRect FAR *box;
    if (!g_track_hwnd || !window_dc(hdc, g_track_hwnd))
        return FALSE;
    nc_fill_input(g_track_hwnd, &in);
    rt_user31_nc(&in, &u);
    box = g_track_hit == RT_HIT_MIN ? &u.minbox : &u.maxbox;
    return rect_inside(box, x, y, w, h);
}

BOOL g_trace;

/* Trace level 1: calls made in a paint context; level 2: every call while
 * the drawing patches are armed permanently. */
#define TRACE_ON (g_trace > 1 || (g_trace && (g_ctl_hwnd || g_nc_hwnd)))

static COLORREF brush_color(HDC hdc)
{
    LOGBRUSH lb;
    HBRUSH b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SelectObject(hdc, b);
    if (!b || GetObject(b, sizeof(lb), &lb) != sizeof(lb))
        return 0xFFFFFFFFL;
    return lb.lbColor;
}

/* Every handler: let non-client and control logic look at the call first,
 * then call the original with the patches suspended. */

BOOL RT_HANDLER h_PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop)
{
    BOOL r;
    if (TRACE_ON)
        log_msg("PatBlt %04X %d,%d %dx%d rop %08lX brush %06lX", hdc, x, y, w, h, rop, brush_color(hdc));
    if (nc_drop(hdc, x, y, w, h))
        return TRUE;
    if (g_ctl_hwnd && ctl_gdi(GOP_PATBLT, hdc, x, y, w, h, 0, 0))
        return TRUE;
    if ((g_scroll_hwnd || g_nc_hwnd) && rop == PATCOPY
        && scroll_gdi(GOP_PATBLT, hdc, x, y, w, h, 0, 0, 0, brush_color(hdc)))
        return TRUE;
    if (g_menu_owner && rop == DSTINVERT && nc_sysbox_invert(g_menu_owner, hdc, x, y, w, h))
        return TRUE;
    if (MENU_POSSIBLE() && rop == PATCOPY && menu_gdi(GOP_PATBLT, hdc, x, y, w, h, 0, 0, brush_color(hdc)))
        return TRUE;
    patch_suspend();
    r = PatBlt(hdc, x, y, w, h, rop);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_BitBlt(HDC hdc, int x, int y, int w, int h, HDC src, int sx, int sy, DWORD rop)
{
    BOOL r;
    if (TRACE_ON)
        log_msg("BitBlt %04X %d,%d %dx%d from %04X %d,%d rop %08lX", hdc, x, y, w, h, src, sx, sy, rop);
    if (nc_drop(hdc, x, y, w, h))
        return TRUE;
    if (track_blit(hdc, x, y, w, h)) {
        nc_track_paint(g_track_hwnd, g_track_down);
        return TRUE;
    }
    if (g_ctl_hwnd && ctl_gdi(rop == ROP_MASKBLT ? GOP_MASKBLT : GOP_BITBLT, hdc, x, y, w, h, 0, 0))
        return TRUE;
    if (MENU_POSSIBLE() && rop == ROP_MASKBLT && menu_gdi(GOP_MASKBLT, hdc, x, y, w, h, 0, 0, brush_color(hdc)))
        return TRUE;
    if ((g_scroll_hwnd || g_nc_hwnd) && rop == SRCCOPY
        && scroll_gdi(GOP_BITBLT, hdc, x, y, w, h, src, sx, sy, 0))
        return TRUE;
    patch_suspend();
    r = BitBlt(hdc, x, y, w, h, src, sx, sy, rop);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_StretchBlt(HDC hdc, int x, int y, int w, int h, HDC src, int sx, int sy,
                             int sw, int sh, DWORD rop)
{
    BOOL r;
    if (TRACE_ON)
        log_msg("StretchBlt %04X %d,%d %dx%d from %04X %d,%d %dx%d rop %08lX", hdc, x, y, w, h,
                src, sx, sy, sw, sh, rop);
    if (nc_drop(hdc, x, y, w, h))
        return TRUE;
    if (g_ctl_hwnd && ctl_gdi(GOP_STRETCHBLT, hdc, x, y, w, h, 0, 0))
        return TRUE;
    if ((g_scroll_hwnd || g_nc_hwnd) && rop == SRCCOPY
        && scroll_gdi(GOP_STRETCHBLT, hdc, x, y, w, h, src, sx, sy, 0))
        return TRUE;
    patch_suspend();
    r = StretchBlt(hdc, x, y, w, h, src, sx, sy, sw, sh, rop);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_ExtTextOut(HDC hdc, int x, int y, UINT opts, const RECT FAR *rc,
                             LPCSTR str, UINT len, int FAR *dx)
{
    BOOL r;
    if (TRACE_ON)
        log_msg("ExtTextOut %04X %d,%d opts %04X len %u text %04X", hdc, x, y, opts, len,
                GetTextColor(hdc));
    if (nc_drop_point(hdc, x, y))
        return TRUE;
    if (g_ctl_hwnd && ctl_gdi(GOP_TEXT, hdc, x, y, 0, 0, str, len))
        return TRUE;
    if (MENU_POSSIBLE() && !(opts & ETO_OPAQUE) && menu_gdi(GOP_TEXT, hdc, x, y, 0, 0, str, len, 0))
        return TRUE;
    patch_suspend();
    r = ExtTextOut(hdc, x, y, opts, rc, str, len, dx);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_TextOut(HDC hdc, int x, int y, LPCSTR str, int len)
{
    BOOL r;
    if (TRACE_ON)
        log_msg("TextOut %04X %d,%d len %d", hdc, x, y, len);
    if (nc_drop_point(hdc, x, y))
        return TRUE;
    if (g_ctl_hwnd && ctl_gdi(GOP_TEXT, hdc, x, y, 0, 0, str, len))
        return TRUE;
    if (MENU_POSSIBLE() && menu_gdi(GOP_TEXT, hdc, x, y, 0, 0, str, len, 0))
        return TRUE;
    patch_suspend();
    r = TextOut(hdc, x, y, str, len);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_Rectangle(HDC hdc, int l, int t, int rr, int b)
{
    BOOL r;
    if (TRACE_ON)
        log_msg("Rectangle %04X %d,%d,%d,%d brush %06lX", hdc, l, t, rr, b, brush_color(hdc));
    if (g_ctl_hwnd && ctl_gdi(GOP_LINE, hdc, l, t, rr - l, b - t, 0, 0))
        return TRUE;
    patch_suspend();
    r = Rectangle(hdc, l, t, rr, b);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_LineTo(HDC hdc, int x, int y)
{
    BOOL r;
    DWORD cp = GetCurrentPosition(hdc);
    int x0 = (int)(short)LOWORD(cp), y0 = (int)(short)HIWORD(cp);
    if (TRACE_ON)
        log_msg("LineTo %04X %d,%d -> %d,%d", hdc, x0, y0, x, y);
    if (g_ctl_hwnd && ctl_gdi(GOP_LINE, hdc, x0 < x ? x0 : x, y0 < y ? y0 : y,
                              (x0 < x ? x - x0 : x0 - x) + 1, (y0 < y ? y - y0 : y0 - y) + 1, 0, 0)) {
        MoveTo(hdc, x, y);      /* keep the current position consistent */
        return TRUE;
    }
    patch_suspend();
    r = LineTo(hdc, x, y);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_Polyline(HDC hdc, const POINT FAR *pts, int n)
{
    BOOL r;
    if (TRACE_ON)
        log_msg("Polyline %04X n=%d %d,%d", hdc, n, n ? pts[0].x : 0, n ? pts[0].y : 0);
    if (g_ctl_hwnd && n > 0) {
        int i, l = pts[0].x, t = pts[0].y, rr = l, b = t;
        for (i = 1; i < n; i++) {
            if (pts[i].x < l) l = pts[i].x;
            if (pts[i].x > rr) rr = pts[i].x;
            if (pts[i].y < t) t = pts[i].y;
            if (pts[i].y > b) b = pts[i].y;
        }
        if (ctl_gdi(GOP_LINE, hdc, l, t, rr - l + 1, b - t + 1, 0, 0))
            return TRUE;
    }
    patch_suspend();
    r = Polyline(hdc, pts, n);
    patch_resume();
    return r;
}

COLORREF RT_HANDLER h_SetPixel(HDC hdc, int x, int y, COLORREF c)
{
    COLORREF r;
    if (TRACE_ON)
        log_msg("SetPixel %04X %d,%d %06lX", hdc, x, y, c);
    if (g_ctl_hwnd && ctl_gdi(GOP_PIXEL, hdc, x, y, 1, 1, 0, 0))
        return c;
    patch_suspend();
    r = SetPixel(hdc, x, y, c);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_PtInRect(const RECT FAR *rc, POINT pt)
{
    BOOL r, handled;
    if (g_track_hwnd) {
        r = nc_track_ptinrect(rc, pt, &handled);
        if (handled)
            return r;
    }
    patch_suspend();
    r = PtInRect(rc, pt);
    patch_resume();
    return r;
}

/* Permanent: before USER shows a system error box (e.g. after a fault),
 * every patch is removed and the engine stops, so the error box and
 * whatever follows run on unmodified USER/GDI. */
typedef WORD (FAR PASCAL *SYSERRORBOXPROC)(LPCSTR, LPCSTR, WORD, WORD, WORD);

WORD RT_HANDLER h_SysErrorBox(LPCSTR text, LPCSTR caption, WORD b1, WORD b2, WORD b3)
{
    SYSERRORBOXPROC orig = (SYSERRORBOXPROC)patch_target(P_SYSERRORBOX);
    g_running = FALSE;
    patch_remove_all();
    log_msg("SysErrorBox: engine stopped, all patches removed");
    return orig(text, caption, b1, b2, b3);
}

/* USER draws window borders (WS_BORDER, frames) through this. */
typedef BOOL (FAR PASCAL *FASTWINDOWFRAMEPROC)(HDC, LPRECT, int, int, DWORD);

BOOL RT_HANDLER h_FastWindowFrame(HDC hdc, LPRECT rc, int xw, int yw, DWORD rop)
{
    BOOL r;
    FASTWINDOWFRAMEPROC orig = (FASTWINDOWFRAMEPROC)patch_target(P_FASTWINDOWFRAME);
    if (TRACE_ON)
        log_msg("FastWindowFrame %04X %d,%d,%d,%d %dx%d rop %08lX", hdc, rc->left, rc->top, rc->right,
                rc->bottom, xw, yw, rop);
    if (g_ctl_hwnd && ctl_gdi(GOP_FRAME, hdc, rc->left, rc->top, rc->right - rc->left,
                              rc->bottom - rc->top, 0, xw))
        return TRUE;
    if (MENU_POSSIBLE() && menu_gdi(GOP_FRAME, hdc, rc->left, rc->top, rc->right - rc->left,
                                    rc->bottom - rc->top, 0, 0, 0))
        return TRUE;
    if ((g_scroll_hwnd || g_nc_hwnd) && scroll_gdi(GOP_FRAME, hdc, rc->left, rc->top, rc->right - rc->left,
                                                   rc->bottom - rc->top, 0, 0, 0, rop))
        return TRUE;
    patch_suspend();
    r = orig(hdc, rc, xw, yw, rop);
    patch_resume();
    return r;
}

/* While USER lays out menus: measure with the theme's menu font. */
DWORD RT_HANDLER h_GetTextExtent(HDC hdc, LPCSTR str, int len)
{
    DWORD r;
    HFONT f = menu_measure_font(hdc), old = 0;
    patch_suspend();
    if (f)
        old = SelectObject(hdc, f);
    r = GetTextExtent(hdc, str, len);
    if (f)
        SelectObject(hdc, old);
    patch_resume();
    return r;
}

BOOL RT_HANDLER h_GetTextExtentPoint(HDC hdc, LPCSTR str, int len, SIZE FAR *sz)
{
    BOOL r;
    HFONT f = menu_measure_font(hdc), old = 0;
    patch_suspend();
    if (f)
        old = SelectObject(hdc, f);
    r = GetTextExtentPoint(hdc, str, len, sz);
    if (f)
        SelectObject(hdc, old);
    patch_resume();
    return r;
}

int  RT_HANDLER h_SetScrollPos(HWND hwnd, int bar, int pos, BOOL redraw);
void RT_HANDLER h_SetScrollRange(HWND hwnd, int bar, int lo, int hi, BOOL redraw);
BOOL RT_HANDLER h_EnableScrollBar(HWND hwnd, UINT bar, UINT arrows);

FARPROC intercept_handler(int id)
{
    switch (id) {
    case P_PATBLT:      return (FARPROC)h_PatBlt;
    case P_BITBLT:      return (FARPROC)h_BitBlt;
    case P_EXTTEXTOUT:  return (FARPROC)h_ExtTextOut;
    case P_TEXTOUT:     return (FARPROC)h_TextOut;
    case P_PTINRECT:    return (FARPROC)h_PtInRect;
    case P_STRETCHBLT:  return (FARPROC)h_StretchBlt;
    case P_RECTANGLE:   return (FARPROC)h_Rectangle;
    case P_LINETO:      return (FARPROC)h_LineTo;
    case P_POLYLINE:    return (FARPROC)h_Polyline;
    case P_SETPIXEL:    return (FARPROC)h_SetPixel;
    case P_SYSERRORBOX: return (FARPROC)h_SysErrorBox;
    case P_FASTWINDOWFRAME: return (FARPROC)h_FastWindowFrame;
    case P_GETTEXTEXTENT: return (FARPROC)h_GetTextExtent;
    case P_GETTEXTEXTENTPOINT: return (FARPROC)h_GetTextExtentPoint;
    case P_SETSCROLLPOS: return (FARPROC)h_SetScrollPos;
    case P_SETSCROLLRANGE: return (FARPROC)h_SetScrollRange;
    case P_ENABLESCROLLBAR: return (FARPROC)h_EnableScrollBar;
    }
    return 0;
}

BOOL intercept_init(void)
{
    return TRUE;
}
