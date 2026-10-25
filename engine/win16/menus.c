/*
 * menus.c - Win95 look for menu bars and popup menus (plan phase 5).
 *
 * USER 3.1 draws menus with a small set of GDI calls (measured with the
 * trace, see docs/STATUS.md):
 *   - background / item fills: PatBlt(PATCOPY) with the COLOR_MENU brush,
 *     selection with the COLOR_HIGHLIGHT brush;
 *   - item text: TextOut with the '&' removed, the mnemonic underline as a
 *     separate 1-pixel PatBlt in the text colour;
 *   - check marks and sub-menu arrows: transparent blits (ROP_MASKBLT) from
 *     USER's bitmap strip; a disabled item that is selected is drawn into a
 *     memory DC with GrayString and blitted the same way;
 *   - the popup border: FastWindowFrame; the menu bar ends with a 1-pixel
 *     black line.
 * Each of these is replaced by the theme's part on menu DCs (the menu bar
 * area of the window being painted, or a visible popup menu window).
 *
 * Text is drawn with the theme's "Menu" font. While USER lays out menus
 * the GetTextExtent patches make it measure with that font too, so item
 * widths and underline positions fit.
 */
#include "rt16.h"

#define MAX_POPUPS 8

static HWND  popups[MAX_POPUPS];
HWND         g_menu_owner;      /* window whose menu loop is running */
static int   measure_depth;
#define MAX_MEASURE 4
static HWND  measure_hwnd[MAX_MEASURE];   /* menu_measure_begin windows */
static int   check_w, check_h, arrow_w, arrow_h;
static int   font_h, font_ascent;   /* theme menu font */

/* GrayString text captured from the memory DC during menu mode */
static struct {
    char text[64];
    int  len;
    int  x, y;
} gray;

/* the last item text drawn: where the mnemonic underline belongs */
static struct {
    HDC hdc;
    int x, y, top;
    int part, state;
} last;

static const int menu_patches[] = { P_PATBLT, P_BITBLT, P_TEXTOUT, P_EXTTEXTOUT, P_FASTWINDOWFRAME,
                                    P_GETTEXTEXTENT, P_GETTEXTEXTENTPOINT };

static void arm(BOOL on)
{
    int i;
    for (i = 0; i < (int)(sizeof(menu_patches) / sizeof(menu_patches[0])); i++) {
        if (on)
            patch_arm(menu_patches[i], ARM_MENU);
        else
            patch_disarm(menu_patches[i], ARM_MENU);
    }
}

static void arm_measure(BOOL on)
{
    if (on) {
        patch_arm(P_GETTEXTEXTENT, ARM_MENU);
        patch_arm(P_GETTEXTEXTENTPOINT, ARM_MENU);
    } else {
        patch_disarm(P_GETTEXTEXTENT, ARM_MENU);
        patch_disarm(P_GETTEXTEXTENTPOINT, ARM_MENU);
    }
}

static HFONT theme_font(void)
{
    return g_theme ? (HFONT)rt_theme_font(g_theme, "Menu") : 0;
}

BOOL menu_has_bar(HWND hwnd)
{
    return !(GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) && GetMenu(hwnd) != 0;
}

/* ---- menu mode and measuring ---- */

static void remember_popup(HWND hwnd)
{
    int i, free_slot = -1;
    for (i = 0; i < MAX_POPUPS; i++) {
        if (popups[i] == hwnd)
            return;
        if (!popups[i] || !IsWindow(popups[i]))
            free_slot = i;
    }
    if (free_slot >= 0)
        popups[free_slot] = hwnd;
}

void menu_message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)wp;
    if (ctl_class(hwnd) == CLS_MENU) {
        if (msg == WM_NCDESTROY) {
            int i;
            for (i = 0; i < MAX_POPUPS; i++)
                if (popups[i] == hwnd)
                    popups[i] = 0;
            return;
        }
        remember_popup(hwnd);
    }
    switch (msg) {
    case WM_INITMENU:
    case WM_INITMENUPOPUP:
        if (!g_menu_owner && !(GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD)) {
            g_menu_owner = hwnd;
            gray.len = 0;
            arm(TRUE);
        }
        break;
    case WM_MENUSELECT:
        /* flags 0xFFFF and no menu: the menu loop is over */
        if (g_menu_owner && LOWORD(lp) == 0xFFFF && HIWORD(lp) == 0) {
            g_menu_owner = 0;
            arm(FALSE);
        }
        break;
    }
}

void menu_measure_begin(HWND hwnd)
{
    if (measure_depth < MAX_MEASURE)
        measure_hwnd[measure_depth] = hwnd;
    measure_depth++;
    arm_measure(TRUE);
}

void menu_measure_end(HWND hwnd)
{
    (void)hwnd;
    if (measure_depth > 0) {
        measure_depth--;
        arm_measure(FALSE);
    }
}

/* Whether hdc is a DC of a window whose menu bar is laid out (or a screen
 * DC). Other windows can paint meanwhile: the MDI frame's WM_NCACTIVATE
 * repaints the active MDI child's caption, which must keep its widths. */
static BOOL measured_dc(HDC hdc)
{
    DWORD org = GetDCOrg(hdc);
    int i, n = measure_depth < MAX_MEASURE ? measure_depth : MAX_MEASURE;
    RECT r;
    if (org == 0)
        return TRUE;
    for (i = 0; i < n; i++) {
        if (!measure_hwnd[i] || !IsWindow(measure_hwnd[i]))
            continue;
        GetWindowRect(measure_hwnd[i], &r);
        if ((int)LOWORD(org) == r.left && (int)HIWORD(org) == r.top)
            return TRUE;
    }
    return FALSE;
}

HFONT menu_measure_font(HDC hdc)
{
    HFONT cur, sys;
    if (!g_menu_owner && (!measure_depth || !measured_dc(hdc)))
        return 0;
    sys = GetStockObject(SYSTEM_FONT);
    cur = SelectObject(hdc, sys);
    SelectObject(hdc, cur);
    return cur == sys ? theme_font() : 0;
}

/* ---- which DC ---- */

enum { MD_NONE, MD_BAR, MD_POPUP };

/* Classifies hdc. *area receives the menu bar or popup rectangle in DC
 * coordinates. */
static int menu_dc(HDC hdc, RECT FAR *area)
{
    DWORD org = GetDCOrg(hdc);
    int ox = (int)LOWORD(org), oy = (int)HIWORD(org), i;
    HWND owner;
    RECT wr;
    for (i = 0; i < MAX_POPUPS; i++) {
        POINT pt;
        if (!popups[i] || !IsWindow(popups[i]) || !IsWindowVisible(popups[i]))
            continue;
        GetWindowRect(popups[i], &wr);
        pt.x = pt.y = 0;
        ClientToScreen(popups[i], &pt);
        if (ox == wr.left && oy == wr.top) {
            SetRect(area, 0, 0, wr.right - wr.left, wr.bottom - wr.top);
            return MD_POPUP;
        }
        if (ox == pt.x && oy == pt.y) {
            SetRect(area, wr.left - pt.x, wr.top - pt.y, wr.right - pt.x, wr.bottom - pt.y);
            return MD_POPUP;
        }
    }
    owner = g_menu_owner ? g_menu_owner : g_nc_hwnd;
    if (owner && menu_has_bar(owner)) {
        RtNcIn in;
        GetWindowRect(owner, &wr);
        if (ox != wr.left || oy != wr.top)
            return MD_NONE;
        nc_fill_input(owner, &in);
        if (in.menu <= 0)
            return MD_NONE;
        SetRect(area, in.frame_x, in.frame_y + in.caption, in.cx - in.frame_x,
                in.frame_y + in.caption + in.menu);
        return MD_BAR;
    }
    return MD_NONE;
}

/* ---- drawing ---- */

static void render(HDC hdc, int part, int state, int l, int t, int r, int b, const RECT FAR *hole,
                   LPCSTR text, int len, WORD flags)
{
    RtSurface s;
    RtDrawReq q;
    if (r <= l || b <= t)
        return;
    _fmemset(&q, 0, sizeof(q));
    q.part = (rt_u8)part;
    q.state = (rt_u8)state;
    rt_rect_set(&q.rc, l, t, r, b);
    q.flags = flags;
    if (hole) {
        q.flags |= RT_RQ_HOLE;
        rt_rect_set(&q.hole, hole->left, hole->top, hole->right, hole->bottom);
    }
    q.text = text;
    q.textlen = (rt_s16)len;
    gdi_surface(&s, hdc);
    rt_render(g_theme, &s, &q);
}

static int text_state(HDC hdc)
{
    COLORREF c = GetTextColor(hdc);
    if (c == GetNearestColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT)))
        return RT_ST_SELECTED;
    if (c == GetNearestColor(hdc, GetSysColor(COLOR_GRAYTEXT)))
        return RT_ST_DISABLED;
    return RT_ST_NORMAL;
}

static BOOL is_color(HDC hdc, COLORREF brush, int sys)
{
    return brush == GetNearestColor(hdc, GetSysColor(sys)) || brush == GetSysColor(sys);
}

static void fill(HDC hdc, int l, int t, int r, int b, COLORREF c)
{
    HBRUSH br = CreateSolidBrush(c), old = SelectObject(hdc, br);
    PatBlt(hdc, l, t, r - l, b - t, PATCOPY);
    SelectObject(hdc, old);
    DeleteObject(br);
}

static void underline(HDC hdc, int x, int w)
{
    RtColor c, sh;
    int has_shadow = 0, y;
    if (!rt_theme_text_colors(g_theme, last.part, last.state, &c, &sh, &has_shadow))
        return;
    y = last.top + font_ascent + 1;
    if (has_shadow)
        fill(hdc, x + 1, y + 1, x + w + 1, y + 2, (COLORREF)sh);
    fill(hdc, x, y, x + w, y + 1, (COLORREF)c);
}

static void draw_text(HDC hdc, int kind, int x, int y, LPCSTR str, int len, int state)
{
    TEXTMETRIC tm;
    HFONT f = theme_font(), old;
    DWORD ext;
    int part = kind == MD_BAR ? RT_PART_MENUBARTEXT : RT_PART_MENUTEXT;
    GetTextMetrics(hdc, &tm);           /* USER's font: the item height it laid out */
    old = f ? SelectObject(hdc, f) : 0;
    ext = GetTextExtent(hdc, str, len);
    if (f)
        SelectObject(hdc, old);
    render(hdc, part, state, x, y, x + LOWORD(ext), y + tm.tmHeight, 0, str, len, RT_RQ_LEFT);
    last.hdc = hdc;
    last.x = x;
    last.y = y;
    last.top = y + (tm.tmHeight - font_h) / 2;
    last.part = part;
    last.state = state;
}

BOOL menu_gdi(int op, HDC hdc, int x, int y, int w, int h, LPCSTR str, int len, DWORD extra)
{
    RECT a;
    int kind;
    BOOL done = FALSE;

    if (!g_theme)
        return FALSE;
    kind = menu_dc(hdc, &a);
    if (kind == MD_NONE) {
        /* GrayString draws a disabled, selected item into a memory DC */
        if (g_menu_owner && op == GOP_TEXT && GetDCOrg(hdc) == 0 && len > 0) {
            gray.len = len < (int)sizeof(gray.text) ? len : (int)sizeof(gray.text) - 1;
            _fmemcpy(gray.text, str, gray.len);
            gray.text[gray.len] = 0;
            gray.x = x;
            gray.y = y;
        }
        return FALSE;
    }
    if (kind == MD_BAR && (x < a.left || y < a.top || x >= a.right || y >= a.bottom))
        return FALSE;

    patch_suspend();
    switch (op) {
    case GOP_PATBLT: {
        COLORREF brush = (COLORREF)extra;
        if (h == 1 && w > 0) {
            if (kind == MD_BAR && w >= (a.right - a.left) - 2) {
                /* the line below the menu bar: Win95 has none */
                render(hdc, RT_PART_MENUBAR, RT_ST_NORMAL, x, y, x + w, y + 1, 0, 0, 0, 0);
            } else if (kind == MD_POPUP && w >= (a.right - a.left) - 8) {
                render(hdc, RT_PART_MENUSEPARATOR, RT_ST_NORMAL, x + 1, y, x + w - 1, y + 2, 0, 0, 0, 0);
            } else if (last.hdc == hdc) {
                underline(hdc, x, w);
            }
            done = TRUE;
            break;
        }
        if (is_color(hdc, brush, COLOR_MENU) || is_color(hdc, brush, COLOR_HIGHLIGHT)) {
            BOOL sel = !is_color(hdc, brush, COLOR_MENU);
            if (kind == MD_POPUP) {
                if (!sel && w >= (a.right - a.left) - 1 && h >= (a.bottom - a.top) - 1) {
                    render(hdc, RT_PART_MENU, RT_ST_NORMAL, a.left, a.top, a.right, a.bottom, 0, 0, 0, 0);
                } else {
                    /* keep items off the 2-pixel border */
                    int l = x < a.left + 2 ? a.left + 2 : x;
                    int r = x + w > a.right - 2 ? a.right - 2 : x + w;
                    render(hdc, RT_PART_MENUITEM, sel ? RT_ST_SELECTED : RT_ST_NORMAL, l, y, r, y + h,
                           0, 0, 0, 0);
                }
            } else if (!sel && w >= (a.right - a.left) - 2) {
                render(hdc, RT_PART_MENUBAR, RT_ST_NORMAL, x, y, x + w, y + h, 0, 0, 0, 0);
            } else {
                render(hdc, RT_PART_MENUBARITEM, sel ? RT_ST_SELECTED : RT_ST_NORMAL, x, y, x + w, y + h,
                       0, 0, 0, 0);
            }
            done = TRUE;
        }
        break;
    }
    case GOP_TEXT:
        if (len > 0)
            draw_text(hdc, kind, x, y, str, len, text_state(hdc));
        done = TRUE;
        break;
    case GOP_MASKBLT: {
        /* the transparent blit paints with the selected brush, not the
         * text colour */
        COLORREF brush = (COLORREF)extra;
        int st = is_color(hdc, brush, COLOR_HIGHLIGHTTEXT) ? RT_ST_SELECTED
               : is_color(hdc, brush, COLOR_GRAYTEXT) ? RT_ST_DISABLED : RT_ST_NORMAL;
        if (w == check_w && h == check_h) {
            render(hdc, RT_PART_MENUCHECK, st, x, y, x + w, y + h, 0, 0, 0, 0);
            done = TRUE;
        } else if (w == arrow_w && h == arrow_h) {
            render(hdc, RT_PART_MENUSUBARROW, st, x, y, x + w, y + h, 0, 0, 0, 0);
            done = TRUE;
        } else if (kind == MD_POPUP && gray.len) {
            draw_text(hdc, kind, x + gray.x, y + gray.y, gray.text, gray.len, RT_ST_DISABLED);
            gray.len = 0;
            done = TRUE;
        }
        break;
    }
    case GOP_FRAME:
        if (kind == MD_POPUP) {
            RECT hole = a;
            InflateRect(&hole, -2, -2);
            render(hdc, RT_PART_MENU, RT_ST_NORMAL, a.left, a.top, a.right, a.bottom, &hole, 0, 0, 0);
            done = TRUE;
        }
        break;
    }
    patch_resume();
    return done;
}

/* ---- lifetime ---- */

static void bitmap_size(int obm, int *w, int *h)
{
    HBITMAP b = LoadBitmap(NULL, MAKEINTRESOURCE(obm));
    BITMAP bm;
    *w = *h = 0;
    if (b && GetObject(b, sizeof(bm), &bm)) {
        *w = bm.bmWidth;
        *h = bm.bmHeight;
    }
    if (b)
        DeleteObject(b);
}

void menu_init(void)
{
    HDC dc = GetDC(NULL);
    HFONT f = theme_font(), old;
    TEXTMETRIC tm;
    bitmap_size(OBM_CHECK, &check_w, &check_h);
    bitmap_size(OBM_MNARROW, &arrow_w, &arrow_h);
    old = f ? SelectObject(dc, f) : 0;
    GetTextMetrics(dc, &tm);
    if (f)
        SelectObject(dc, old);
    ReleaseDC(NULL, dc);
    font_h = tm.tmHeight;
    font_ascent = tm.tmAscent;
    log_msg("menus: check %dx%d, arrow %dx%d, font height %d", check_w, check_h, arrow_w, arrow_h, font_h);
}

void menu_shutdown(void)
{
    if (g_menu_owner)
        arm(FALSE);
    while (measure_depth > 0)
        menu_measure_end(0);
    g_menu_owner = 0;
    _fmemset(popups, 0, sizeof(popups));
}

BOOL CALLBACK __export menu_relayout_one(HWND hwnd, LPARAM lp)
{
    (void)lp;
    if (IsWindowVisible(hwnd) && menu_has_bar(hwnd))
        SetMenu(hwnd, GetMenu(hwnd));       /* recalculates the menu bar */
    return TRUE;
}

void menu_relayout_all(void)
{
    EnumWindows((WNDENUMPROC)menu_relayout_one, 0);
}
