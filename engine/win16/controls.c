/*
 * controls.c - Win95 look for standard controls in dialogs (plan phase 4).
 *
 * Buttons: while a message to a button is being handled, the GDI
 * interceptors see everything USER draws into the button's client DC:
 *   - push buttons: everything is dropped; afterwards the theme paints the
 *     whole button (face, edge, text, focus);
 *   - check boxes / radio buttons: only the blit of USER's box bitmap is
 *     dropped; the theme paints its box at the same place, USER's text and
 *     focus rectangle stay;
 *   - group boxes: frame lines are dropped, the text stays; the theme paints
 *     an etched frame around the text.
 * Dialogs: WM_CTLCOLOR answers get the button-face brush (grey dialogs),
 * controls using the System font get the theme's "Controls" font, and edit
 * and list boxes get a sunken 3D border drawn in the dialog (CTL3D-style).
 */
#include "rt16.h"

/* Win 3.1 button messages / styles (not all in the Watcom headers) */
#define BM_GETCHECK16   (WM_USER + 0)
#define BM_SETCHECK16   (WM_USER + 1)
#define BM_GETSTATE16   (WM_USER + 2)
#define BM_SETSTATE16   (WM_USER + 3)
#define BM_SETSTYLE16   (WM_USER + 4)
#define BST_PUSHED16    0x0004

enum { CK_NONE, CK_PUSH, CK_DEFPUSH, CK_CHECK, CK_3STATE, CK_RADIO, CK_GROUP, CK_STATIC, CK_EDIT, CK_COMBO };

typedef struct Ctx {
    HWND   hwnd;
    BYTE   kind;
    BYTE   depth;
    BYTE   dropped;         /* USER drew something we dropped: overpaint */
    BYTE   have_box;
    BYTE   have_text;
    RtRect box;             /* check/radio box, client coordinates */
    RtRect text;            /* group box caption; disabled text (GrayString output) */
} Ctx;

#define MAX_CTX 6

static Ctx    ctx[MAX_CTX];
static int    nctx;
static int    box_w, box_h;     /* USER's check box bitmap cell */
static HBRUSH face_brush;     /* dialog background handed out via WM_CTLCOLOR */
static COLORREF face_color;

/* The theme's Dialog part if it is a plain fill, else the button face. */
static void make_face_brush(void)
{
    RtColor c;
    if (face_brush)
        DeleteObject(face_brush);
    if (g_theme && rt_theme_fill_color(g_theme, RT_PART_DIALOG, RT_ST_NORMAL, &c))
        face_color = (COLORREF)c;
    else
        face_color = GetSysColor(COLOR_BTNFACE);
    face_brush = CreateSolidBrush(face_color);
}

HWND g_ctl_hwnd;

/* ---- classification ---- */

int ctl_class(HWND hwnd)
{
    char name[16];
    WState FAR *ws = ws_get(hwnd, TRUE);
    int cc;
    if (ws && ws->classcode)
        return ws->classcode;
    GetClassName(hwnd, name, sizeof(name));
    if (lstrcmpi(name, "Button") == 0)
        cc = CLS_BUTTON;
    else if (lstrcmpi(name, "Edit") == 0)
        cc = CLS_EDIT;
    else if (lstrcmpi(name, "ListBox") == 0)
        cc = CLS_LISTBOX;
    else if (lstrcmpi(name, "ComboBox") == 0)
        cc = CLS_COMBO;
    else if (lstrcmpi(name, "Static") == 0)
        cc = CLS_STATIC;
    else if (lstrcmpi(name, "#32770") == 0)
        cc = CLS_DIALOG;
    else if (lstrcmpi(name, "#32768") == 0)
        cc = CLS_MENU;
    else if (lstrcmpi(name, "ScrollBar") == 0)
        cc = CLS_SCROLLBAR;
    else
        cc = CLS_OTHER;
    if (ws)
        ws->classcode = (WORD)cc;
    return cc;
}

static int button_kind(HWND hwnd)
{
    switch ((UINT)GetWindowLong(hwnd, GWL_STYLE) & 0x0F) {
    case BS_PUSHBUTTON:     return CK_PUSH;
    case BS_DEFPUSHBUTTON:  return CK_DEFPUSH;
    case BS_CHECKBOX:
    case BS_AUTOCHECKBOX:   return CK_CHECK;
    case BS_3STATE:
    case BS_AUTO3STATE:     return CK_3STATE;
    case BS_RADIOBUTTON:
    case BS_AUTORADIOBUTTON: return CK_RADIO;
    case BS_GROUPBOX:       return CK_GROUP;
    }
    return CK_NONE;         /* owner-draw, user buttons */
}

static int control_kind(HWND hwnd)
{
    switch (ctl_class(hwnd)) {
    case CLS_BUTTON:
        return button_kind(hwnd);
    case CLS_STATIC:
        switch ((UINT)GetWindowLong(hwnd, GWL_STYLE) & 0x0F) {
        case SS_LEFT: case SS_CENTER: case SS_RIGHT: case SS_SIMPLE: case SS_LEFTNOWORDWRAP:
            return CK_STATIC;
        }
        return CK_NONE;
    case CLS_EDIT:
        return CK_EDIT;
    case CLS_COMBO:
        /* drop-down lists: the combo box draws its field and button itself */
        if (((UINT)GetWindowLong(hwnd, GWL_STYLE) & 3) == (UINT)CBS_DROPDOWNLIST)
            return CK_COMBO;
        return CK_NONE;
    }
    return CK_NONE;
}

/* Only controls of dialogs are themed for now. */
static BOOL in_dialog(HWND hwnd)
{
    HWND parent = GetParent(hwnd);
    return parent && ctl_class(parent) == CLS_DIALOG;
}

BOOL ctl_wants(HWND hwnd, UINT msg)
{
    int kind;
    if (!g_running || !g_theme)
        return FALSE;
    switch (msg) {
    case WM_PAINT: case WM_ENABLE: case WM_SETTEXT: case WM_SETFOCUS: case WM_KILLFOCUS:
        break;
    case WM_NCPAINT:
        if (ctl_class(hwnd) != CLS_EDIT)
            return FALSE;
        break;
    default:
        if (ctl_class(hwnd) == CLS_COMBO) {
            /* combo boxes also repaint on input and on their own messages */
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_MOUSEMOVE || msg == WM_KEYDOWN
                || msg == WM_CHAR || (msg >= WM_USER && msg < WM_USER + 0x40))
                break;
            return FALSE;
        }
        if (ctl_class(hwnd) != CLS_BUTTON)
            return FALSE;
    }
    switch (msg) {
    case WM_PAINT: case WM_ENABLE: case WM_SETFOCUS: case WM_KILLFOCUS:
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_LBUTTONUP: case WM_MOUSEMOVE:
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: case WM_CHAR: case WM_SETTEXT:
    case WM_CANCELMODE: case WM_SETFONT:
    case BM_SETCHECK16: case BM_SETSTATE16: case BM_SETSTYLE16:
        break;
    default:
        return FALSE;
    }
    kind = control_kind(hwnd);
    if (kind == CK_NONE)
        return FALSE;
    return in_dialog(hwnd);
}

/* ---- context ---- */

static void arm(BOOL on)
{
    static const int ids[] = { P_PATBLT, P_BITBLT, P_EXTTEXTOUT, P_TEXTOUT, P_STRETCHBLT,
                               P_RECTANGLE, P_LINETO, P_POLYLINE, P_SETPIXEL, P_FASTWINDOWFRAME };
    int i;
    for (i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); i++) {
        if (on)
            patch_arm(ids[i], ARM_CTL);
        else
            patch_disarm(ids[i], ARM_CTL);
    }
}

void ctl_begin(HWND hwnd)
{
    Ctx *c;
    if (nctx && ctx[nctx - 1].hwnd == hwnd) {
        ctx[nctx - 1].depth++;
        arm(TRUE);
        return;
    }
    if (nctx >= MAX_CTX) {
        arm(TRUE);          /* keep arm/disarm balanced */
        return;
    }
    c = &ctx[nctx++];
    _fmemset(c, 0, sizeof(*c));
    c->hwnd = hwnd;
    c->kind = (BYTE)control_kind(hwnd);
    if (g_trace)
        log_msg("ctl begin %04X kind %d", hwnd, c->kind);
    g_ctl_hwnd = hwnd;
    arm(TRUE);
}

static void paint_button(const Ctx *c);
static void field_border(HWND parent, HWND child, HDC hdc);
static void render(RtSurface FAR *s, int part, int state, const RtRect FAR *rc, const RtRect FAR *hole,
                   LPCSTR text, WORD flags);

void ctl_end(HWND hwnd)
{
    Ctx done;
    arm(FALSE);
    if (!nctx || ctx[nctx - 1].hwnd != hwnd)
        return;
    if (ctx[nctx - 1].depth) {
        ctx[nctx - 1].depth--;
        return;
    }
    done = ctx[--nctx];
    g_ctl_hwnd = nctx ? ctx[nctx - 1].hwnd : 0;
    if (done.dropped && IsWindow(hwnd) && IsWindowVisible(hwnd))
        paint_button(&done);
}

/* hdc is the client DC of hwnd if its origin is the client origin */
static BOOL client_dc(HDC hdc, HWND hwnd)
{
    POINT pt;
    DWORD org = GetDCOrg(hdc);
    pt.x = pt.y = 0;
    ClientToScreen(hwnd, &pt);
    return (int)LOWORD(org) == pt.x && (int)HIWORD(org) == pt.y;
}

/* Called by every GDI interceptor while armed. Returns TRUE if USER's call
 * is to be dropped. op: GOP_*; for text ops, str/len are the text. */
BOOL ctl_gdi(int op, HDC hdc, int x, int y, int w, int h, LPCSTR str, int len)
{
    Ctx *c;
    if (!nctx)
        return FALSE;
    c = &ctx[nctx - 1];
    if (!client_dc(hdc, c->hwnd))
        return FALSE;
    switch (c->kind) {
    case CK_COMBO: {
        /* Win95: one sunken field around the whole control, the button
         * inside it. USER's button (the rightmost SM_CXVSCROLL pixels) is
         * dropped and painted afterwards; its fills of the text area are
         * kept inside the field's 2-pixel border. */
        RECT cr;
        int bl;
        GetClientRect(c->hwnd, &cr);
        bl = cr.right - GetSystemMetrics(SM_CXVSCROLL);
        if (x >= bl) {
            c->dropped = 1;
            return TRUE;
        }
        if (op == GOP_FRAME) {
            if (x == 0 && y == 0) {
                RtSurface sf;
                RtRect rc, hole;
                rt_rect_set(&rc, 0, 0, cr.right, cr.bottom);
                hole = rc;
                rt_rect_inset(&hole, 2, 2, 2, 2);
                patch_suspend();
                gdi_surface(&sf, hdc);
                render(&sf, RT_PART_FIELD, IsWindowEnabled(c->hwnd) ? RT_ST_NORMAL : RT_ST_DISABLED,
                       &rc, &hole, 0, 0);
                patch_resume();
            }
            return TRUE;
        }
        if (op == GOP_PATBLT) {
            int l = x < 2 ? 2 : x, t = y < 2 ? 2 : y;
            int r = x + w > bl ? bl : x + w, b = y + h > cr.bottom - 2 ? cr.bottom - 2 : y + h;
            if (r > l && b > t) {
                patch_suspend();
                PatBlt(hdc, l, t, r - l, b - t, PATCOPY);
                patch_resume();
            }
            return TRUE;
        }
        return FALSE;
    }
    case CK_EDIT: {
        /* Win 3.1 edit controls take WS_BORDER out of their style and draw
         * the border with FastWindowFrame at the edge of their client area.
         * Let it draw, then put the 3D ring on top. */
        RECT cr;
        GetClientRect(c->hwnd, &cr);
        if (op == GOP_FRAME && x == 0 && y == 0 && w == cr.right && h == cr.bottom) {
            WState FAR *ws = ws_get(c->hwnd, TRUE);
            if (ws)
                ws->flags |= WS_F_FIELD;
            c->dropped = 1;
        }
        return FALSE;
    }
    case CK_PUSH:
    case CK_DEFPUSH:
        c->dropped = 1;
        return TRUE;
    case CK_CHECK:
    case CK_3STATE:
    case CK_RADIO:
        if ((op == GOP_BITBLT || op == GOP_MASKBLT || op == GOP_STRETCHBLT) && w == box_w && h == box_h) {
            rt_rect_set(&c->box, x, y, x + w, y + h);
            c->have_box = 1;
            c->dropped = 1;
            return TRUE;
        }
        /* fall through: disabled text */
    case CK_STATIC:
        /* Disabled text (GrayString output, or plain text in the grey text
         * colour): Win95 embosses instead. */
        if (!IsWindowEnabled(c->hwnd)) {
            if (op == GOP_MASKBLT) {
                rt_rect_set(&c->text, x, y, x + w, y + h);
            } else if (op == GOP_TEXT && len > 0) {
                DWORD ext = GetTextExtent(hdc, str, len);
                rt_rect_set(&c->text, x, y, x + LOWORD(ext), y + HIWORD(ext));
            } else {
                return FALSE;
            }
            c->have_text = 1;
            c->dropped = 1;
            return TRUE;
        }
        return FALSE;
    case CK_GROUP:
        if (op == GOP_TEXT) {
            DWORD ext = GetTextExtent(hdc, str, len);
            rt_rect_set(&c->text, x - 1, y, x + LOWORD(ext) + 1, y + HIWORD(ext));
            c->have_text = 1;
            c->dropped = 1;
            return FALSE;
        }
        if (op == GOP_LINE || op == GOP_PIXEL || op == GOP_FRAME || (op == GOP_PATBLT && (w <= 2 || h <= 2))) {
            c->dropped = 1;
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

/* ---- painting ---- */

static void render(RtSurface FAR *s, int part, int state, const RtRect FAR *rc, const RtRect FAR *hole,
                   LPCSTR text, WORD flags)
{
    RtDrawReq q;
    _fmemset(&q, 0, sizeof(q));
    q.part = (rt_u8)part;
    q.state = (rt_u8)state;
    q.rc = *rc;
    q.flags = flags;
    if (hole) {
        q.flags |= RT_RQ_HOLE;
        q.hole = *hole;
    }
    q.text = text;
    q.textlen = -1;
    rt_render(g_theme, s, &q);
}

static void paint_button(const Ctx *c)
{
    HWND hwnd = c->hwnd;
    HDC hdc;
    RtSurface s;
    RECT cr;
    RtRect rc;
    BOOL enabled = IsWindowEnabled(hwnd);
    /* BM_* are WM_USER messages: only buttons may receive them (WM_USER+2
     * is EM_GETRECT for an edit control) */
    BOOL pushed = c->kind >= CK_PUSH && c->kind <= CK_RADIO
               && (SendMessage(hwnd, BM_GETSTATE16, 0, 0) & BST_PUSHED16) != 0;
    char text[80];

    GetClientRect(hwnd, &cr);
    rt_rect_set(&rc, cr.left, cr.top, cr.right, cr.bottom);
    hdc = GetDC(hwnd);
    if (!hdc)
        return;
    patch_suspend();
    gdi_surface(&s, hdc);
    switch (c->kind) {
    case CK_PUSH:
    case CK_DEFPUSH: {
        int st = pushed ? RT_ST_PRESSED : c->kind == CK_DEFPUSH ? RT_ST_DEFAULT : RT_ST_NORMAL;
        int ts = !enabled ? RT_ST_DISABLED : pushed ? RT_ST_PRESSED : RT_ST_NORMAL;
        if (pushed && c->kind == CK_DEFPUSH)
            st = RT_ST_DEFAULTPRESSED;
        GetWindowText(hwnd, text, sizeof(text));
        render(&s, RT_PART_PUSHBUTTON, st, &rc, 0, 0, 0);
        render(&s, RT_PART_PUSHBUTTONTEXT, ts, &rc, 0, text, RT_RQ_PREFIX | RT_RQ_CENTER);
        if (GetFocus() == hwnd && enabled) {
            RtRect f = rc;
            rt_rect_inset(&f, 4, 4, 4, 4);
            render(&s, RT_PART_FOCUS, RT_ST_NORMAL, &f, 0, 0, 0);
        }
        break;
    }
    case CK_CHECK:
    case CK_3STATE:
    case CK_RADIO: {
        UINT check = (UINT)SendMessage(hwnd, BM_GETCHECK16, 0, 0);
        int st;
        if (check == 2 && c->kind == CK_3STATE)
            st = !enabled ? RT_ST_MIXEDDISABLED : pushed ? RT_ST_MIXEDPRESSED : RT_ST_MIXED;
        else if (check)
            st = !enabled ? RT_ST_CHECKEDDISABLED : pushed ? RT_ST_CHECKEDPRESSED : RT_ST_CHECKED;
        else
            st = !enabled ? RT_ST_DISABLED : pushed ? RT_ST_PRESSED : RT_ST_NORMAL;
        if (c->have_box)
            render(&s, c->kind == CK_RADIO ? RT_PART_RADIO : RT_PART_CHECKBOX, st, &c->box, 0, 0, 0);
        if (c->have_text) {
            GetWindowText(hwnd, text, sizeof(text));
            render(&s, RT_PART_STATICTEXT, RT_ST_DISABLED, &c->text, 0, text, RT_RQ_PREFIX | RT_RQ_LEFT);
        }
        break;
    }
    case CK_EDIT:
        patch_resume();
        ReleaseDC(hwnd, hdc);
        ctl_field_ncpaint(hwnd);
        return;
    case CK_COMBO: {
        int bw = GetSystemMetrics(SM_CXVSCROLL);
        RtRect b;
        BOOL dropped = SendMessage(hwnd, CB_GETDROPPEDSTATE, 0, 0) != 0;
        rt_rect_set(&b, rc.right - bw, 2, rc.right - 2, rc.bottom - 2);
        render(&s, RT_PART_COMBOBUTTON, !enabled ? RT_ST_DISABLED : dropped ? RT_ST_PRESSED : RT_ST_NORMAL,
               &b, 0, 0, 0);
        break;
    }
    case CK_STATIC:
        if (c->have_text) {
            UINT st = (UINT)GetWindowLong(hwnd, GWL_STYLE);
            GetWindowText(hwnd, text, sizeof(text));
            render(&s, RT_PART_STATICTEXT, RT_ST_DISABLED, &c->text, 0, text,
                   (WORD)(RT_RQ_LEFT | ((st & SS_NOPREFIX) ? 0 : RT_RQ_PREFIX)));
        }
        break;

    case CK_GROUP: {
        RtRect frame = rc;
        TEXTMETRIC tm;
        HFONT f = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0), old = 0;
        if (f)
            old = SelectObject(hdc, f);
        GetTextMetrics(hdc, &tm);
        if (f)
            SelectObject(hdc, old);
        frame.top = (rt_s16)(frame.top + tm.tmHeight / 2);
        render(&s, RT_PART_GROUPBOX, enabled ? RT_ST_NORMAL : RT_ST_DISABLED, &frame,
               c->have_text ? &c->text : 0, 0, 0);
        break;
    }
    }
    patch_resume();
    ReleaseDC(hwnd, hdc);
}

/* ---- dialogs ---- */

static BOOL default_brush(HBRUSH br)
{
    LOGBRUSH lb;
    if (!br)
        return TRUE;
    if (GetObject(br, sizeof(lb), &lb) != sizeof(lb))
        return FALSE;
    return lb.lbStyle == BS_SOLID && lb.lbColor == GetSysColor(COLOR_WINDOW);
}

/* WM_CTLCOLOR answered by the dialog: make the default answers grey. */
void ctl_ctlcolor(HWND dlg, WPARAM wp, LPARAM lp, LRESULT FAR *result)
{
    UINT type = HIWORD(lp);
    HDC hdc = (HDC)wp;
    (void)dlg;
    if (!face_brush)
        return;
    if (type != CTLCOLOR_MSGBOX && type != CTLCOLOR_BTN && type != CTLCOLOR_DLG && type != CTLCOLOR_STATIC)
        return;
    if (g_trace)
        log_msg("CTLCOLOR type %u brush %04X default %d", type, LOWORD(*result),
                default_brush((HBRUSH)LOWORD(*result)));
    if (!default_brush((HBRUSH)LOWORD(*result)))
        return;
    SetBkColor(hdc, face_color);
    SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
    *result = (LRESULT)(UINT)face_brush;
}

/* Swap the System font for the theme's "Controls" font. */
static void swap_font(HWND hwnd)
{
    HFONT f = (HFONT)rt_theme_font(g_theme, "Controls");
    WState FAR *ws;
    if (!f || SendMessage(hwnd, WM_GETFONT, 0, 0))
        return;
    ws = ws_get(hwnd, TRUE);
    if (!ws)
        return;
    ws->flags |= WS_F_FONTSWAP;
    SendMessage(hwnd, WM_SETFONT, (WPARAM)f, 0);
}

void ctl_dialog_init(HWND dlg)
{
    HWND c;
    for (c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        swap_font(c);
}

/* The control's own 1-pixel border is inside its window (list boxes:
 * WS_BORDER; edit controls: drawn in the client area). The 2-pixel 3D ring
 * covers that border plus one pixel of the dialog around it. */
static void field_border(HWND parent, HWND child, HDC hdc)
{
    RtSurface s;
    RECT r;
    POINT pt;
    RtRect rc, hole;
    int out = 1;
    GetWindowRect(child, &r);
    pt.x = r.left;
    pt.y = r.top;
    ScreenToClient(parent, &pt);
    rt_rect_set(&rc, pt.x - out, pt.y - out, pt.x + (r.right - r.left) + out,
                pt.y + (r.bottom - r.top) + out);
    hole = rc;
    rt_rect_inset(&hole, 2, 2, 2, 2);
    if (g_trace)
        log_msg("field border %04X (%d,%d,%d,%d)", child, rc.left, rc.top, rc.right, rc.bottom);
    gdi_surface(&s, hdc);
    render(&s, RT_PART_FIELD, IsWindowEnabled(child) ? RT_ST_NORMAL : RT_ST_DISABLED, &rc, &hole, 0, 0);
}

static BOOL wants_field(HWND child)
{
    int cc = ctl_class(child);
    if (!IsWindowVisible(child))
        return FALSE;
    if (cc == CLS_LISTBOX)
        return (GetWindowLong(child, GWL_STYLE) & WS_BORDER) != 0;
    if (cc == CLS_EDIT) {
        WState FAR *ws = ws_get(child, FALSE);
        return ws && (ws->flags & WS_F_FIELD);
    }
    return FALSE;
}

/* After the dialog painted: 3D borders around its edit and list boxes. */
void ctl_dialog_paint(HWND dlg)
{
    HWND c;
    HDC hdc;
    if (g_trace)
        log_msg("dialog paint %04X visible %d", dlg, IsWindowVisible(dlg));
    if (!IsWindow(dlg) || !IsWindowVisible(dlg))
        return;
    hdc = GetDC(dlg);
    if (!hdc)
        return;
    patch_suspend();
    for (c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        if (wants_field(c))
            field_border(dlg, c, hdc);
    patch_resume();
    ReleaseDC(dlg, hdc);
}

/* After a field painted its own (black) border. */
void ctl_field_ncpaint(HWND child)
{
    HWND dlg = GetParent(child);
    HDC hdc;
    if (!dlg || !wants_field(child) || ctl_class(dlg) != CLS_DIALOG)
        return;
    hdc = GetDC(dlg);
    if (!hdc)
        return;
    patch_suspend();
    field_border(dlg, child, hdc);
    patch_resume();
    ReleaseDC(dlg, hdc);
}

BOOL ctl_is_field(HWND hwnd)
{
    HWND parent = GetParent(hwnd);
    return parent && ctl_class(parent) == CLS_DIALOG && wants_field(hwnd);
}

/* ---- lifetime ---- */

void ctl_init(void)
{
    HBITMAP b = LoadBitmap(NULL, MAKEINTRESOURCE(OBM_CHECKBOXES));
    BITMAP bm;
    if (b && GetObject(b, sizeof(bm), &bm)) {
        box_w = bm.bmWidth / 4;
        box_h = bm.bmHeight / 3;
    }
    if (b)
        DeleteObject(b);
    make_face_brush();
    log_msg("controls: check box cell %dx%d", box_w, box_h);
}

void ctl_syscolor(void)
{
    make_face_brush();
}

static void restore_font(HWND hwnd)
{
    WState FAR *ws = ws_get(hwnd, FALSE);
    if (ws && (ws->flags & WS_F_FONTSWAP) && IsWindow(hwnd)) {
        SendMessage(hwnd, WM_SETFONT, 0, 1);
        ws->flags &= ~WS_F_FONTSWAP;
    }
}

BOOL CALLBACK __export ctl_restore_child(HWND hwnd, LPARAM lp)
{
    (void)lp;
    restore_font(hwnd);
    return TRUE;
}

BOOL CALLBACK __export ctl_restore_top(HWND hwnd, LPARAM lp)
{
    (void)lp;
    EnumChildWindows(hwnd, (WNDENUMPROC)ctl_restore_child, 0);
    return TRUE;
}

/* At unload: give swapped controls their System font back. */
void ctl_shutdown(void)
{
    EnumWindows((WNDENUMPROC)ctl_restore_top, 0);
    if (face_brush)
        DeleteObject(face_brush);
    face_brush = 0;
    nctx = 0;
    g_ctl_hwnd = 0;
}

BOOL CALLBACK __export ctl_init_child(HWND hwnd, LPARAM lp)
{
    (void)lp;
    if (ctl_class(GetParent(hwnd)) == CLS_DIALOG)
        swap_font(hwnd);
    return TRUE;
}

BOOL CALLBACK __export ctl_init_top(HWND hwnd, LPARAM lp)
{
    (void)lp;
    if (ctl_class(hwnd) == CLS_DIALOG)
        EnumChildWindows(hwnd, (WNDENUMPROC)ctl_init_child, 0);
    return TRUE;
}

/* At start: dialogs that are already open. */
void ctl_attach_existing(void)
{
    EnumWindows((WNDENUMPROC)ctl_init_top, 0);
}
