/*
 * rt16.h - internal interfaces of the Win16 runtime (RETHEME.DLL).
 *
 * Everything in engine/win16 runs inside other applications' tasks: on
 * their stacks (SS != DS), called from USER and GDI. Rules:
 *   - hook procedures and patch handlers are FAR PASCAL __loadds and live
 *     in fixed code segments (see win16.mk);
 *   - keep stack use small (< 512 bytes per handler), no recursion;
 *   - never open files outside the loader task (log.c buffers instead).
 */
#ifndef RT16_H
#define RT16_H

#define OEMRESOURCE         /* OBM_* bitmap IDs */
#include <windows.h>
#include <dos.h>
#include <string.h>
#include "rt_theme.h"
#include "rt_geom.h"

#define RT_HANDLER  FAR PASCAL __loadds

/* Win16 CWPSTRUCT (not in the Open Watcom headers). */
typedef struct tagCWPSTRUCT16 {
    LPARAM lParam;
    WPARAM wParam;
    UINT   message;
    HWND   hwnd;
} CWPSTRUCT16;
typedef CWPSTRUCT16 FAR *LPCWPSTRUCT16;

/* ---- engine state (rtdll.c) ---- */

extern HINSTANCE g_hInst;
extern RtTheme  *g_theme;
extern BOOL      g_running;
extern WORD      g_winver;          /* 0x030A = 3.10 */

/* ---- patch engine (patch.c) ---- */

enum {
    P_PATBLT,
    P_BITBLT,
    P_EXTTEXTOUT,
    P_TEXTOUT,
    P_PTINRECT,
    P_STRETCHBLT,
    P_RECTANGLE,
    P_LINETO,
    P_POLYLINE,
    P_SETPIXEL,
    P_SYSERRORBOX,          /* permanent: emergency removal of all patches */
    P_FASTWINDOWFRAME,
    P_GETTEXTEXTENT,
    P_GETTEXTEXTENTPOINT,
    P_SETSCROLLPOS,         /* permanent: USER repaints scroll bars inside these */
    P_SETSCROLLRANGE,
    P_ENABLESCROLLBAR,
    P_COUNT
};

/* Why a patch is armed; each reason is reference-counted separately. */
#define ARM_NC      0x01        /* non-client painting of a themed window */
#define ARM_TRACK   0x02        /* USER's min/max button tracking loop */
#define ARM_CTL     0x04        /* a control is being painted */
#define ARM_PERM    0x08        /* permanent patch */
#define ARM_MENU    0x10        /* menu bar / popup menus are drawn or measured */
#define ARM_SCROLL  0x20        /* scroll bars are drawn outside WM_NCPAINT */
#define ARM_REASONS 6

/* Original entry point of a patched function (to call it while the patch
 * is suspended or removed). */
FARPROC patch_target(int id);

BOOL patch_init(void);              /* resolves targets, creates aliases */
void patch_shutdown(void);          /* removes every patch */
void patch_arm(int id, WORD reason);
void patch_disarm(int id, WORD reason);
WORD patch_reasons(int id);
/* Temporarily restores the original bytes of every armed patch (while the
 * engine itself draws or calls the original function). Nests. */
void patch_suspend(void);
void patch_resume(void);
/* Restores only one patch's original bytes (to call that function while
 * the other patches stay active, e.g. so its own drawing is seen). */
void patch_unhook(int id);
void patch_rehook(int id);
/* Emergency path (SysErrorBox, unload): restore everything now. */
void patch_remove_all(void);

/* ---- hooks and dispatcher (hooks.c) ---- */

BOOL hooks_install(void);
void hooks_remove(void);

/* ---- per-window state (wstate.c) ---- */

#define WS_F_THEMED     0x0001      /* frame and caption are themed */
#define WS_F_EXCLUDED   0x0002
#define WS_F_ACTIVE     0x0004      /* caption drawn active */
#define WS_F_FONTSWAP   0x0008      /* System font replaced by the theme font */
#define WS_F_FIELD      0x0010      /* edit control with a border: gets the 3D ring */

/* WState.classcode (0 = not classified yet) */
enum { CLS_NONE, CLS_OTHER, CLS_BUTTON, CLS_EDIT, CLS_LISTBOX, CLS_COMBO, CLS_STATIC, CLS_DIALOG, CLS_MENU,
       CLS_SCROLLBAR };

typedef struct WState {
    HWND   hwnd;
    WORD   flags;
    WORD   classcode;
    RtNcIn nc;                      /* last computed geometry input */
} WState;

WState FAR *ws_get(HWND hwnd, BOOL create);
void        ws_forget(HWND hwnd);
void        ws_clear(void);

/* ---- non-client theming (ncpaint.c) ---- */

BOOL nc_wants(HWND hwnd);                       /* themed top-level window? */
void nc_fill_input(HWND hwnd, RtNcIn FAR *in);  /* geometry from USER */
void nc_begin(HWND hwnd, BOOL active);          /* USER starts NC painting */
void nc_end(HWND hwnd);                         /* USER finished: paint ours */
void nc_paint(HWND hwnd, HDC hdc, BOOL active);
/* Rewrites a screen point for WM_NCHITTEST; returns the theme hit code. */
int  nc_map_hittest(HWND hwnd, LPARAM FAR *lParam);
/* WH_GETMESSAGE side of the close button; returns TRUE if the message was
 * consumed (and rewritten to WM_NULL). */
BOOL nc_close_filter(MSG FAR *msg);
/* USER's min/max tracking loop (armed with ARM_TRACK). */
void nc_track_paint(HWND hwnd, BOOL pressed);
BOOL nc_track_ptinrect(const RECT FAR *rc, POINT pt, BOOL FAR *handled);
BOOL nc_sysbox_invert(HWND hwnd, HDC hdc, int x, int y, int w, int h);

/* Paint context: which window USER is painting right now. */
extern HWND g_nc_hwnd;
extern BOOL g_nc_active;
extern RtRect g_nc_userbar;         /* USER's caption bar, window coords */
extern int  g_track_hit;            /* RT_HIT_MIN / RT_HIT_MAX while USER tracks */
extern HWND g_track_hwnd;
extern BOOL g_track_down;           /* last PtInRect answer during tracking */

/* ---- controls (controls.c) ---- */

/* GDI operation classes seen by ctl_gdi */
enum { GOP_PATBLT, GOP_BITBLT, GOP_MASKBLT, GOP_STRETCHBLT, GOP_TEXT, GOP_LINE, GOP_PIXEL, GOP_FRAME };
#define ROP_MASKBLT 0x00B8074AL     /* PSDPxax: USER's transparent blits (check boxes, GrayString) */

extern HWND g_ctl_hwnd;             /* innermost control being painted */

int  ctl_class(HWND hwnd);
BOOL ctl_wants(HWND hwnd, UINT msg);
void ctl_begin(HWND hwnd);
void ctl_end(HWND hwnd);
BOOL ctl_gdi(int op, HDC hdc, int x, int y, int w, int h, LPCSTR str, int len);
void ctl_ctlcolor(HWND dlg, WPARAM wp, LPARAM lp, LRESULT FAR *result);
void ctl_dialog_init(HWND dlg);
void ctl_dialog_paint(HWND dlg);
BOOL ctl_is_field(HWND hwnd);
void ctl_field_ncpaint(HWND child);
void ctl_init(void);
void ctl_syscolor(void);
void ctl_shutdown(void);
void ctl_attach_existing(void);

/* ---- scroll bars (scroll.c) ---- */

extern HWND g_scroll_hwnd;          /* window or scroll bar control being drawn */
void scroll_begin(HWND hwnd);
void scroll_end(HWND hwnd);
BOOL scroll_wants(HWND hwnd, UINT msg, WPARAM wp);  /* message needs a scroll context */
BOOL scroll_gdi(int op, HDC hdc, int x, int y, int w, int h, HDC src, int sx, int sy, DWORD extra);
void scroll_init(void);

/* ---- menus (menus.c) ---- */

extern HWND g_menu_owner;           /* window whose menu loop is running */
#define MENU_POSSIBLE() (g_menu_owner || g_nc_hwnd)

void  menu_init(void);
void  menu_shutdown(void);
void  menu_message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);   /* from the dispatcher */
void  menu_measure_begin(HWND hwnd);    /* USER may lay out hwnd's menu bar */
void  menu_measure_end(HWND hwnd);
BOOL  menu_has_bar(HWND hwnd);
/* GDI call on a menu DC? Returns TRUE if handled (USER's call dropped). */
BOOL  menu_gdi(int op, HDC hdc, int x, int y, int w, int h, LPCSTR str, int len, DWORD extra);
/* While menus are measured: the font USER's DC should measure with, or 0. */
HFONT menu_measure_font(HDC hdc);
void  menu_relayout_all(void);          /* re-measure menu bars (start/unload) */

/* ---- GDI interceptors (intercept.c) ---- */

BOOL intercept_init(void);          /* registers handlers with the patch engine */
FARPROC intercept_handler(int id);
extern BOOL g_trace;                /* log every intercepted call in a paint context */

/* ---- GDI backend (gdi16.c) ---- */

struct RtSurface {
    HDC hdc;
    int depth;
};

void gdi_init(void);
void gdi_shutdown(void);
void gdi_surface(RtSurface FAR *s, HDC hdc);

/* ---- log (log.c) ---- */

void log_init(void);
void log_msg(const char FAR *fmt, ...);
/* Copies buffered text out (loader task only); returns bytes copied. */
int  log_drain(char FAR *buf, int size);

#endif
