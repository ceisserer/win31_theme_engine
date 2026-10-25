/*
 * rtdll.c - RETHEME.DLL entry points.
 *
 * Exports (called by RETHEME.EXE or a control panel applet, in their task):
 *   RtStart(themeDir)   load the theme, install patches and hooks
 *   RtStop()            remove everything, repaint
 *   RtDrainLog(buf, n)  copy diagnostics out of the ring buffer
 */
#include "rt16.h"

HINSTANCE g_hInst;
RtTheme  *g_theme;
BOOL      g_running;
WORD      g_winver;

int hooks_pending(void);

int CALLBACK LibMain(HINSTANCE hInst, WORD ds, WORD heap, LPSTR cmd)
{
    (void)ds;
    (void)heap;
    (void)cmd;
    g_hInst = hInst;
    log_init();
    return 1;
}

BOOL CALLBACK __export repaint_one(HWND hwnd, LPARAM lp)
{
    (void)lp;
    if (IsWindowVisible(hwnd))
        RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    return TRUE;
}

static void repaint_all(void)
{
    EnumWindows((WNDENUMPROC)repaint_one, 0);
}

/* The theme's [SysColors], set while the engine runs; the old values come
 * back at the stop unless the colour was changed meanwhile. */
static int      nsys;
static int      sys_idx[RT_SYS_COUNT];
static COLORREF sys_old[RT_SYS_COUNT], sys_new[RT_SYS_COUNT];

static void syscolors_apply(void)
{
    rt_u8 idx[RT_SYS_COUNT];
    RtColor rgb[RT_SYS_COUNT];
    int i, n = rt_theme_syscolors(g_theme, idx, rgb);
    HDC hdc = GetDC(NULL);
    /* with 16 colours Windows would dither most of them: use the nearest
     * solid colours instead */
    BOOL few = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES) < 8;
    for (i = 0; i < n && few; i++)
        rgb[i] = (RtColor)GetNearestColor(hdc, (COLORREF)rgb[i]);
    ReleaseDC(NULL, hdc);
    nsys = 0;
    for (i = 0; i < n; i++) {
        if (idx[i] == RT_SYS_3DDKSHADOW || idx[i] == RT_SYS_3DLIGHT) {
            rt16_set_win95_color(idx[i], rgb[i], TRUE);
            continue;
        }
        sys_idx[nsys] = idx[i];
        sys_old[nsys] = GetSysColor(idx[i]);
        sys_new[nsys] = (COLORREF)rgb[i];
        nsys++;
    }
    if (nsys)
        SetSysColors(nsys, sys_idx, sys_new);
    if (n) {
        log_msg("theme sets %d system colour(s)%s", n, (LPSTR)(few ? ", nearest solid colours" : ""));
        rt_theme_syscolor_changed(g_theme);
    }
}

static void syscolors_restore(void)
{
    int idx[RT_SYS_COUNT], i, n = 0;
    COLORREF old[RT_SYS_COUNT];
    for (i = 0; i < nsys; i++)
        if (GetSysColor(sys_idx[i]) == sys_new[i]) {
            idx[n] = sys_idx[i];
            old[n] = sys_old[i];
            n++;
        }
    if (n)
        SetSysColors(n, idx, old);
    nsys = 0;
    rt16_set_win95_color(RT_SYS_3DDKSHADOW, 0, FALSE);
    rt16_set_win95_color(RT_SYS_3DLIGHT, 0, FALSE);
}

int FAR PASCAL __export __loadds RtStart(LPCSTR theme_dir)
{
    WORD v = LOWORD(GetVersion());
    if (g_running)
        return 1;
    g_winver = (WORD)((LOBYTE(v) << 8) | HIBYTE(v));
    if (g_winver < 0x030A || g_winver >= 0x035F) {
        log_msg("unsupported Windows version %d.%02d", HIBYTE(g_winver), LOBYTE(g_winver));
        return -1;
    }
    if (!(GetWinFlags() & WF_PMODE)) {
        log_msg("real mode is not supported");
        return -2;
    }
    gdi_init();
    g_theme = rt_theme_load(theme_dir);
    if (!g_theme) {
        log_msg("cannot load theme %s", theme_dir);
        gdi_shutdown();
        return -3;
    }
    if (g_theme->log[0])
        log_msg("theme warnings: %s", (LPSTR)g_theme->log);
    log_msg("theme: %s", (LPSTR)g_theme->name);
    syscolors_apply();
    log_msg("metrics: frame %d/%d dlgframe %d/%d border %d/%d size %d/%d caption %d menu %d vscroll %d",
            GetSystemMetrics(SM_CXFRAME), GetSystemMetrics(SM_CYFRAME),
            GetSystemMetrics(SM_CXDLGFRAME), GetSystemMetrics(SM_CYDLGFRAME),
            GetSystemMetrics(SM_CXBORDER), GetSystemMetrics(SM_CYBORDER),
            GetSystemMetrics(SM_CXSIZE), GetSystemMetrics(SM_CYSIZE),
            GetSystemMetrics(SM_CYCAPTION), GetSystemMetrics(SM_CYMENU),
            GetSystemMetrics(SM_CXVSCROLL));
    patch_init();
    patch_arm(P_SYSERRORBOX, ARM_PERM);
    ctl_init();
    menu_init();
    scroll_init();
    if (!hooks_install()) {
        log_msg("cannot install hooks");
        patch_shutdown();
        rt_theme_free(g_theme);
        g_theme = 0;
        gdi_shutdown();
        syscolors_restore();
        return -4;
    }
    g_running = TRUE;
    ctl_attach_existing();
    menu_relayout_all();
    repaint_all();
    return 1;
}

/* Diagnostics: log every intercepted GDI call made in a paint context. */
void FAR PASCAL __export __loadds RtSetTrace(int level)
{
    static const int ids[] = { P_PATBLT, P_BITBLT, P_EXTTEXTOUT, P_TEXTOUT, P_RECTANGLE,
                               P_LINETO, P_POLYLINE, P_FASTWINDOWFRAME, P_STRETCHBLT };
    int i;
    if (level > 1 && g_trace <= 1)
        for (i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); i++)
            patch_arm(ids[i], ARM_PERM);
    else if (level <= 1 && g_trace > 1)
        for (i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); i++)
            patch_disarm(ids[i], ARM_PERM);
    g_trace = level;
}

int FAR PASCAL __export __loadds RtStop(void)
{
    /* g_running may already be FALSE after the SysErrorBox emergency stop;
     * hooks, patches and fonts still have to be cleaned up then. */
    if (!g_theme)
        return 1;
    if (hooks_pending()) {
        log_msg("stop refused: %d message(s) in progress", hooks_pending());
        return 0;
    }
    g_running = FALSE;
    hooks_remove();
    patch_shutdown();
    menu_shutdown();
    menu_relayout_all();        /* back to System font metrics */
    ctl_shutdown();
    ws_clear();
    rt_theme_free(g_theme);
    g_theme = 0;
    gdi_shutdown();
    syscolors_restore();
    repaint_all();
    return 1;
}

int FAR PASCAL __export __loadds RtDrainLog(LPSTR buf, int size)
{
    return log_drain(buf, size);
}

int FAR PASCAL __export WEP(int exit_type)
{
    (void)exit_type;
    /* both are no-ops if already done */
    g_running = FALSE;
    hooks_remove();
    patch_shutdown();
    return 1;
}
