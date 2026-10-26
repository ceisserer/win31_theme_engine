/*
 * hooks.c - system-wide hooks, the message dispatcher and one-shot
 * subclassing (plan 4.2).
 *
 * Win16 has no "after the window procedure" hook. To act after USER has
 * handled a message, the dispatcher subclasses the target window for
 * exactly one message: the subclass procedure removes itself, calls the
 * original procedure, then post-processes. Because it is gone before the
 * original procedure runs, it never stays in another program's subclass
 * chain.
 */
#include "rt16.h"

static HHOOK hk_callwnd, hk_getmsg;

/* Post-processing jobs for one-shot subclassing. */
enum { JOB_NC, JOB_TRACK, JOB_CTL, JOB_CTLCOLOR, JOB_DLGINIT, JOB_DLGPAINT, JOB_FIELDNC, JOB_MEASURE,
       JOB_SCROLL };

#define MAX_ONESHOT 16

typedef struct OneShot {
    HWND    hwnd;
    WNDPROC orig;
    UINT    msg;
    WPARAM  wp;
    LPARAM  lp;
    BYTE    job;
    BYTE    called;         /* the subclass procedure has run */
} OneShot;

static OneShot oneshot[MAX_ONESHOT];
static int     noneshot;
static int     nactive;     /* OneShotProc frames on some task's stack */

LRESULT CALLBACK __export __loadds OneShotProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static BOOL oneshot_push(HWND hwnd, UINT msg, int job, WPARAM wp, LPARAM lp)
{
    OneShot *o;
    WNDPROC cur;
    if (noneshot >= MAX_ONESHOT) {
        log_msg("oneshot: table full (msg %04X)", msg);
        return FALSE;
    }
    cur = (WNDPROC)GetWindowLong(hwnd, GWL_WNDPROC);
    if (cur == (WNDPROC)OneShotProc) {
        if (g_trace)
            log_msg("oneshot: %04X busy, msg %04X job %d dropped", hwnd, msg, job);
        return FALSE;       /* already pending for this window */
    }
    o = &oneshot[noneshot++];
    o->hwnd = hwnd;
    o->orig = cur;
    o->msg = msg;
    o->wp = wp;
    o->lp = lp;
    o->job = (BYTE)job;
    o->called = 0;
    SetWindowLong(hwnd, GWL_WNDPROC, (LONG)(WNDPROC)OneShotProc);
    return TRUE;
}

static void post_process(const OneShot *o, LRESULT FAR *result)
{
    switch (o->job) {
    case JOB_CTL:
        ctl_end(o->hwnd);
        break;
    case JOB_CTLCOLOR:
        ctl_ctlcolor(o->hwnd, o->wp, o->lp, result);
        break;
    case JOB_DLGINIT:
        ctl_dialog_init(o->hwnd);
        break;
    case JOB_DLGPAINT:
        ctl_dialog_paint(o->hwnd);
        break;
    case JOB_FIELDNC:
        ctl_field_ncpaint(o->hwnd);
        break;
    case JOB_MEASURE:
        menu_measure_end(o->hwnd);
        break;
    case JOB_SCROLL:
        scroll_end(o->hwnd);
        if (o->msg == WM_NCPAINT && ctl_is_field(o->hwnd))
            ctl_field_ncpaint(o->hwnd);     /* list box: its 3D ring too */
        break;
    case JOB_NC:
        nc_end(o->hwnd);
        break;
    case JOB_TRACK:
        patch_disarm(P_BITBLT, ARM_TRACK);
        patch_disarm(P_PTINRECT, ARM_TRACK);
        g_track_hwnd = 0;
        g_track_hit = RT_HIT_NONE;
        /* USER repaints its own button state at the end; repaint ours */
        if (IsWindow(o->hwnd)) {
            HDC hdc = GetWindowDC(o->hwnd);
            if (hdc) {
                WState FAR *ws = ws_get(o->hwnd, FALSE);
                nc_paint(o->hwnd, hdc, ws ? (ws->flags & WS_F_ACTIVE) != 0 : TRUE);
                ReleaseDC(o->hwnd, hdc);
            }
        }
        break;
    }
}

LRESULT CALLBACK __export __loadds OneShotProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int i;
    OneShot o;
    LRESULT r;
    /* find the innermost pending entry for this window */
    for (i = noneshot - 1; i >= 0; i--)
        if (oneshot[i].hwnd == hwnd && !oneshot[i].called)
            break;
    if (i < 0)      /* cannot happen; fail safe */
        return DefWindowProc(hwnd, msg, wp, lp);
    oneshot[i].called = 1;
    o = oneshot[i];
    /* unsubclass before anything else runs */
    if ((WNDPROC)GetWindowLong(hwnd, GWL_WNDPROC) == (WNDPROC)OneShotProc)
        SetWindowLong(hwnd, GWL_WNDPROC, (LONG)o.orig);
    /* drop the entry: entries are only ever pushed/popped in nesting order */
    for (; i < noneshot - 1; i++)
        oneshot[i] = oneshot[i + 1];
    noneshot--;

    /* The original procedure may yield (a button's WM_COMMAND opening a
     * dialog, a scroll bar's tracking loop) while this frame waits to
     * return into the DLL: RtStop must not let the DLL go meanwhile. */
    nactive++;
    r = CallWindowProc((FARPROC)o.orig, hwnd, msg, wp, lp);
    /* Normally msg == o.msg. If another message got there first, the job
     * still has to end here if it armed patches. */
    if (msg == o.msg || o.job == JOB_NC || o.job == JOB_TRACK || o.job == JOB_CTL || o.job == JOB_MEASURE
        || o.job == JOB_SCROLL)
        post_process(&o, &r);
    if (msg == WM_NCDESTROY)
        ws_forget(hwnd);    /* post-processing may have created state again */
    nactive--;
    return r;
}

/* Messages that matter whether they are sent or posted. */
static void dispatch_any(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    menu_message(hwnd, msg, wp, lp);
    if (msg == WM_PAINT && ctl_class(hwnd) == CLS_DIALOG) {
        oneshot_push(hwnd, WM_PAINT, JOB_DLGPAINT, wp, lp);
        return;
    }
    /* before the field border: a list box's WM_NCPAINT also draws its
     * scroll bar (JOB_SCROLL then repaints the field border as well) */
    if (scroll_wants(hwnd, msg, wp)) {
        if (oneshot_push(hwnd, msg, JOB_SCROLL, wp, lp))
            scroll_begin(hwnd);
        return;
    }
    if (msg == WM_NCPAINT && ctl_is_field(hwnd) && ctl_class(hwnd) != CLS_EDIT) {
        oneshot_push(hwnd, WM_NCPAINT, JOB_FIELDNC, wp, lp);
        return;
    }
    if (ctl_wants(hwnd, msg)) {
        if (oneshot_push(hwnd, msg, JOB_CTL, wp, lp))
            ctl_begin(hwnd);
    }
}

/* Called for SendMessage traffic (WH_CALLWNDPROC). */
static void dispatch_sent(LPCWPSTRUCT16 cwp)
{
    HWND hwnd = cwp->hwnd;
    switch (cwp->message) {
    case WM_NCPAINT:
    case WM_NCACTIVATE:
    case WM_SETTEXT:
        if (!nc_wants(hwnd) || g_nc_hwnd)
            break;
        {
            BOOL active;
            if (cwp->message == WM_NCACTIVATE)
                active = cwp->wParam != 0;
            else {
                /* WS_F_ACTIVE is known once the caption was painted; until
                 * then (windows that were open when the engine started) ask */
                WState FAR *ws = ws_get(hwnd, FALSE);
                active = ws && (ws->flags & WS_F_THEMED) ? (ws->flags & WS_F_ACTIVE) != 0
                                                          : GetActiveWindow() == hwnd;
            }
            if (oneshot_push(hwnd, cwp->message, JOB_NC, cwp->wParam, cwp->lParam))
                nc_begin(hwnd, active);
        }
        break;
    case WM_NCHITTEST:
        nc_map_hittest(hwnd, &cwp->lParam);
        break;
    case WM_NCCALCSIZE:
        /* USER recalculates the menu bar here: let it measure with our font */
        if (menu_has_bar(hwnd) && oneshot_push(hwnd, WM_NCCALCSIZE, JOB_MEASURE, cwp->wParam, cwp->lParam))
            menu_measure_begin(hwnd);
        break;
    case WM_CTLCOLOR:
        if (ctl_class(hwnd) == CLS_DIALOG)
            oneshot_push(hwnd, WM_CTLCOLOR, JOB_CTLCOLOR, cwp->wParam, cwp->lParam);
        break;
    case WM_INITDIALOG:
        if (ctl_class(hwnd) == CLS_DIALOG)
            oneshot_push(hwnd, WM_INITDIALOG, JOB_DLGINIT, cwp->wParam, cwp->lParam);
        break;
    case WM_NCDESTROY:
        /* forget the window after everything that looks at it: its handle
         * is reused soon, and a new window must not inherit its state */
        menu_message(hwnd, cwp->message, cwp->wParam, cwp->lParam);
        ws_forget(hwnd);
        return;
    case WM_SYSCOLORCHANGE:
        if (g_theme)
            rt_theme_syscolor_changed(g_theme);
        ctl_syscolor();
        break;
    }
    dispatch_any(hwnd, cwp->message, cwp->wParam, cwp->lParam);
}

/* Called for posted messages about to be returned by GetMessage. */
static void dispatch_posted(MSG FAR *msg)
{
    if (nc_close_filter(msg))
        return;
    if (msg->message == WM_NCLBUTTONDOWN && (msg->wParam == HTREDUCE || msg->wParam == HTZOOM)
        && nc_wants(msg->hwnd) && !g_track_hwnd) {
        if (oneshot_push(msg->hwnd, WM_NCLBUTTONDOWN, JOB_TRACK, msg->wParam, msg->lParam)) {
            g_track_hwnd = msg->hwnd;
            g_track_hit = msg->wParam == HTREDUCE ? RT_HIT_MIN : RT_HIT_MAX;
            g_track_down = TRUE;
            patch_arm(P_BITBLT, ARM_TRACK);
            patch_arm(P_PTINRECT, ARM_TRACK);
        }
        return;
    }
    if (msg->hwnd)
        dispatch_any(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

LRESULT CALLBACK __export __loadds CallWndHook(int code, WPARAM wp, LPARAM lp)
{
    if (code >= 0 && g_running)
        dispatch_sent((LPCWPSTRUCT16)lp);
    return CallNextHookEx(hk_callwnd, code, wp, lp);
}

LRESULT CALLBACK __export __loadds GetMsgHook(int code, WPARAM wp, LPARAM lp)
{
    if (code >= 0 && g_running)
        dispatch_posted((MSG FAR *)lp);
    return CallNextHookEx(hk_getmsg, code, wp, lp);
}

BOOL hooks_install(void)
{
    hk_callwnd = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)CallWndHook, g_hInst, NULL);
    hk_getmsg = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)GetMsgHook, g_hInst, NULL);
    if (!hk_callwnd || !hk_getmsg) {
        hooks_remove();
        return FALSE;
    }
    return TRUE;
}

void hooks_remove(void)
{
    if (hk_callwnd)
        UnhookWindowsHookEx(hk_callwnd);
    if (hk_getmsg)
        UnhookWindowsHookEx(hk_getmsg);
    hk_callwnd = hk_getmsg = 0;
}

/* One-shot subclasses not yet run or still running: while there are any,
 * the DLL must stay loaded. */
int hooks_pending(void)
{
    return noneshot + nactive;
}
