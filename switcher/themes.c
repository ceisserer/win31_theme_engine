/*
 * themes.c - THEMES.EXE: chooses the theme of the running engine.
 *
 * Lists the themes in THEMES\ next to the program (their THEME.INI names)
 * with the PREVIEW.BMP the installer made for them. Apply writes the theme
 * to RETHEME.INI ([Engine] Theme=), stops the running engine (WM_CLOSE to
 * the loader's window, then waits until RETHEME.EXE and RETHEME.DLL are
 * unloaded) and starts RETHEME.EXE again; no restart of Windows is needed.
 */
#include <windows.h>
#include <dos.h>
#include <string.h>
#include "themes.h"

#define MAX_THEMES  32
#define WAIT_MS     10000

static HINSTANCE inst;
static char home[144];              /* directory of THEMES.EXE, with '\' */
static char ini[160];               /* RETHEME.INI */
static char dirs[MAX_THEMES][13];   /* theme directory names */
static int  ndirs;

/* ---- preview -------------------------------------------------------- */

static HGLOBAL preview;              /* the BMP file of the selected theme */

static void load_preview(int i)
{
    char path[200];
    HFILE f;
    long size;
    if (preview)
        GlobalFree(preview);
    preview = 0;
    if (i < 0)
        return;
    wsprintf(path, "%sTHEMES\\%s\\PREVIEW.BMP", (LPSTR)home, (LPSTR)dirs[i]);
    f = _lopen(path, OF_READ | OF_SHARE_DENY_WRITE);
    if (f == HFILE_ERROR)
        return;
    size = _llseek(f, 0, 2);
    _llseek(f, 0, 0);
    if (size > (long)sizeof(BITMAPFILEHEADER) + (long)sizeof(BITMAPINFOHEADER)) {
        preview = GlobalAlloc(GMEM_MOVEABLE, size);
        if (preview) {
            BYTE _huge *p = (BYTE _huge *)GlobalLock(preview);
            if (_hread(f, p, size) != size || p[0] != 'B' || p[1] != 'M') {
                GlobalUnlock(preview);
                GlobalFree(preview);
                preview = 0;
            } else {
                GlobalUnlock(preview);
            }
        }
    }
    _lclose(f);
}

LRESULT CALLBACK __export PreviewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));
        if (preview) {
            BYTE _huge *p = (BYTE _huge *)GlobalLock(preview);
            BITMAPFILEHEADER FAR *fh = (BITMAPFILEHEADER FAR *)p;
            BITMAPINFO FAR *bi = (BITMAPINFO FAR *)(p + sizeof(BITMAPFILEHEADER));
            int w = (int)bi->bmiHeader.biWidth, h = (int)bi->bmiHeader.biHeight;
            int dw = rc.right, dh = rc.bottom;
            /* fit into the area, keep the aspect ratio, never enlarge */
            if (w > 0 && h > 0) {
                if (w <= dw && h <= dh) {
                    dw = w;
                    dh = h;
                } else if ((long)w * dh > (long)h * dw) {
                    dh = (int)((long)h * dw / w);
                } else {
                    dw = (int)((long)w * dh / h);
                }
                SetStretchBltMode(hdc, COLORONCOLOR);
                StretchDIBits(hdc, (rc.right - dw) / 2, (rc.bottom - dh) / 2, dw, dh, 0, 0, w, h,
                              p + fh->bfOffBits, bi, DIB_RGB_COLORS, SRCCOPY);
            }
            GlobalUnlock(preview);
        } else {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            DrawText(hdc, "No preview", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ---- themes --------------------------------------------------------- */

static void theme_path(int i, char *out)
{
    wsprintf(out, "%sTHEMES\\%s", (LPSTR)home, (LPSTR)dirs[i]);
}

/* Fills the list with the themes; returns the index of the current one. */
static int fill_list(HWND list)
{
    struct find_t ff;
    char pattern[160], path[200], name[64], cur[160], *base;
    int i, n, sel = -1;
    /* the loader's default: THEMES\WIN95 */
    GetPrivateProfileString("Engine", "Theme", "WIN95", cur, sizeof(cur), ini);
    base = strrchr(cur, '\\');
    base = base ? base + 1 : cur;
    wsprintf(pattern, "%sTHEMES\\*.*", (LPSTR)home);
    ndirs = 0;
    for (n = _dos_findfirst(pattern, _A_SUBDIR, &ff); !n && ndirs < MAX_THEMES; n = _dos_findnext(&ff)) {
        if (!(ff.attrib & _A_SUBDIR) || ff.name[0] == '.')
            continue;
        wsprintf(path, "%sTHEMES\\%s\\THEME.INI", (LPSTR)home, (LPSTR)ff.name);
        if (!GetPrivateProfileString("Theme", "Name", "", name, sizeof(name), path))
            continue;
        lstrcpy(dirs[ndirs], ff.name);
        i = (int)SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(LPSTR)name);
        SendMessage(list, LB_SETITEMDATA, i, ndirs);
        ndirs++;
    }
    /* sorted list: find the current theme after all are in */
    for (i = 0; i < ndirs; i++) {
        int d = (int)SendMessage(list, LB_GETITEMDATA, i, 0);
        if (lstrcmpi(dirs[d], base) == 0)
            sel = i;
    }
    return sel;
}

static int selected(HWND dlg)
{
    HWND list = GetDlgItem(dlg, IDC_LIST);
    int i = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
    return i == LB_ERR ? -1 : (int)SendMessage(list, LB_GETITEMDATA, i, 0);
}

static BOOL engine_loaded(void)
{
    return GetModuleHandle("RTLOADER") || GetModuleHandle("RETHEME");
}

static void show_state(HWND dlg)
{
    SetDlgItemText(dlg, IDC_STATE, FindWindow("Retheme16", NULL)
                   ? "The theme engine is running. Apply switches to the selected theme."
                   : "The theme engine is not running. Apply starts it with the selected theme.");
}

static void pump(void)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* Stops the engine if it runs; FALSE if it is still loaded. */
static BOOL stop_engine(void)
{
    HWND w = FindWindow("Retheme16", NULL);
    DWORD t0;
    if (w)
        SendMessage(w, WM_CLOSE, 0, 0);
    t0 = GetTickCount();
    while (engine_loaded() && GetTickCount() - t0 < WAIT_MS) {
        pump();
        Yield();
    }
    return !engine_loaded();
}

static void apply(HWND dlg)
{
    char path[200], exe[160];
    int i = selected(dlg);
    HCURSOR old;
    if (i < 0)
        return;
    theme_path(i, path);
    WritePrivateProfileString("Engine", "Theme", path, ini);
    WritePrivateProfileString(NULL, NULL, NULL, ini);     /* flush */
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
    old = SetCursor(LoadCursor(NULL, IDC_WAIT));
    if (!stop_engine()) {
        SetCursor(old);
        MessageBox(dlg, "The theme engine is busy and could not be stopped. Try again in a moment.",
                   "Retheme16 Themes", MB_OK | MB_ICONINFORMATION);
    } else {
        wsprintf(exe, "%sRETHEME.EXE", (LPSTR)home);
        if (WinExec(exe, SW_SHOWMINNOACTIVE) < 32) {
            SetCursor(old);
            MessageBox(dlg, "RETHEME.EXE could not be started.", "Retheme16 Themes", MB_OK | MB_ICONSTOP);
        }
        SetCursor(old);
    }
    EnableWindow(GetDlgItem(dlg, IDC_APPLY), TRUE);
    show_state(dlg);
}

BOOL CALLBACK __export ThemesProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        HWND list = GetDlgItem(dlg, IDC_LIST);
        int sel = fill_list(list);
        (void)lp;
        SendMessage(list, LB_SETCURSEL, sel < 0 ? 0 : sel, 0);
        load_preview(selected(dlg));
        show_state(dlg);
        if (!ndirs) {
            SetDlgItemText(dlg, IDC_STATE, "No themes found in the THEMES directory.");
            EnableWindow(GetDlgItem(dlg, IDC_APPLY), FALSE);
        }
        return TRUE;
    }
    case WM_QUERYDRAGICON:      /* the task button's icon (NewShell16) */
        return (BOOL)LoadIcon(inst, "RETHEME");
    case WM_COMMAND:
        switch (wp) {
        case IDC_LIST:
            if (HIWORD(lp) == LBN_SELCHANGE) {
                load_preview(selected(dlg));
                InvalidateRect(GetDlgItem(dlg, IDC_PREVIEW), NULL, FALSE);
            } else if (HIWORD(lp) == LBN_DBLCLK) {
                apply(dlg);
            }
            return TRUE;
        case IDC_APPLY:
            apply(dlg);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

int PASCAL WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    FARPROC proc;
    char *p;
    (void)cmd;
    (void)show;
    if (prev) {
        HWND w = FindWindow(NULL, "Retheme16 Themes");
        if (w)
            SetActiveWindow(w);
        return 0;
    }
    inst = hinst;
    GetModuleFileName(inst, home, sizeof(home));
    p = strrchr(home, '\\');
    if (p)
        p[1] = 0;
    wsprintf(ini, "%sRETHEME.INI", (LPSTR)home);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = PreviewProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "RtThemePreview";
    RegisterClass(&wc);

    proc = MakeProcInstance((FARPROC)ThemesProc, inst);
    DialogBox(inst, MAKEINTRESOURCE(IDD_THEMES), NULL, (DLGPROC)proc);
    FreeProcInstance(proc);
    if (preview)
        GlobalFree(preview);
    return 0;
}
