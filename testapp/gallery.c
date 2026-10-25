/*
 * gallery.c - GALLERY.EXE: every window style and standard control the
 * engine themes, in fixed positions, for manual tests and emulator
 * screenshots (plan 6.2).
 *
 *   GALLERY [dialog] [styles] [msgbox] [syserror]
 *
 * The optional words open those scenes at start-up.
 */
#include <windows.h>
#include <string.h>
#include "gallery.h"

static HINSTANCE inst;
static HWND main_wnd;

BOOL CALLBACK __export ControlsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    static const char *const items[] = { "Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot" };
    int i;
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemText(dlg, IDC_EDIT, "Edit control");
        for (i = 0; i < 6; i++) {
            SendDlgItemMessage(dlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)(LPCSTR)items[i]);
            SendDlgItemMessage(dlg, IDC_COMBO, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)items[i]);
        }
        SendDlgItemMessage(dlg, IDC_LIST, LB_SETCURSEL, 1, 0);
        SendDlgItemMessage(dlg, IDC_COMBO, CB_SETCURSEL, 0, 0);
        CheckDlgButton(dlg, IDC_CHECK2, 1);
        CheckDlgButton(dlg, IDC_CHECK3, 2);
        CheckRadioButton(dlg, IDC_RADIO1, IDC_RADIO3, IDC_RADIO1);
        SetScrollRange(GetDlgItem(dlg, IDC_SCROLL), SB_CTL, 0, 100, FALSE);
        SetScrollPos(GetDlgItem(dlg, IDC_SCROLL), SB_CTL, 30, FALSE);
        return TRUE;
    case WM_HSCROLL: {
        HWND sb = (HWND)HIWORD(lp);
        int pos = GetScrollPos(sb, SB_CTL);
        switch (wp) {
        case SB_LINELEFT:     pos -= 1; break;
        case SB_LINERIGHT:    pos += 1; break;
        case SB_PAGELEFT:     pos -= 10; break;
        case SB_PAGERIGHT:    pos += 10; break;
        case SB_THUMBPOSITION: pos = LOWORD(lp); break;
        }
        SetScrollPos(sb, SB_CTL, pos < 0 ? 0 : pos > 100 ? 100 : pos, TRUE);
        return TRUE;
    }
    case WM_COMMAND:
        if (wp == IDOK || wp == IDCANCEL) {
            EndDialog(dlg, wp);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

LRESULT CALLBACK __export PlainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* One window per caption style the engine distinguishes. */
static void open_styles(void)
{
    static const struct { const char *title; DWORD style; DWORD ex; } st[] = {
        { "Sizable, min+max",     WS_OVERLAPPEDWINDOW, 0 },
        { "Max box only",         WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX, 0 },
        { "No min/max",           WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, 0 },
        { "Dialog frame",         WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_DLGFRAME, WS_EX_DLGMODALFRAME },
        { "No system menu",       WS_CAPTION | WS_THICKFRAME, 0 },
        { "Thin border",          WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER, 0 }
    };
    int i;
    for (i = 0; i < 6; i++) {
        HWND w = CreateWindowEx(st[i].ex, "GalleryPlain", st[i].title, st[i].style,
                                40 + (i % 3) * 190, 220 + (i / 3) * 110, 180, 100,
                                main_wnd, NULL, inst, NULL);
        ShowWindow(w, SW_SHOWNOACTIVATE);
    }
}

static void controls_dialog(void)
{
    FARPROC p = MakeProcInstance((FARPROC)ControlsProc, inst);
    DialogBox(inst, MAKEINTRESOURCE(IDD_CONTROLS), main_wnd, (DLGPROC)p);
    FreeProcInstance(p);
}

LRESULT CALLBACK __export MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND:
        switch (wp) {
        case CM_DIALOG:  controls_dialog(); return 0;
        case CM_MSGBOX:
            MessageBox(hwnd, "This is a message box.", "Gallery", MB_OKCANCEL | MB_ICONINFORMATION);
            return 0;
        case CM_STYLES:  open_styles(); return 0;
        case CM_EXIT:    DestroyWindow(hwnd); return 0;
        case CM_UNLOAD: {
            /* closing RETHEME.EXE's window unloads the engine */
            HWND w = FindWindow("Retheme16", NULL);
            if (w)
                PostMessage(w, WM_CLOSE, 0, 0);
            return 0;
        }
        case CM_SYSERROR: {
            /* reading an empty floppy drive makes Windows show its
             * "Cannot read from drive A" system error box (SysErrorBox) */
            OFSTRUCT of;
            HFILE f = OpenFile("A:\\GALLERY.TST", &of, OF_READ);
            if (f != HFILE_ERROR)
                _lclose(f);
            return 0;
        }
        case CM_ABOUT:
            MessageBox(hwnd, "Gallery: test scenes for the Retheme16 engine.", "About", MB_OK);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        TextOut(dc, 8, 8, "Hello from Windows 3.1!", 23);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    MSG m;
    inst = hInst;
    if (!prev) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = MainProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszMenuName = MAKEINTRESOURCE(IDM_MAIN);
        wc.lpszClassName = "Gallery";
        RegisterClass(&wc);
        wc.lpfnWndProc = PlainProc;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = "GalleryPlain";
        RegisterClass(&wc);
    }
    main_wnd = CreateWindow("Gallery", "Gallery", WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
                            20, 20, 360, 190, NULL, NULL, hInst, NULL);
    ShowWindow(main_wnd, show);
    UpdateWindow(main_wnd);
    if (cmd && _fstrstr(cmd, "styles"))
        open_styles();
    if (cmd && _fstrstr(cmd, "msgbox"))
        PostMessage(main_wnd, WM_COMMAND, CM_MSGBOX, 0);
    if (cmd && _fstrstr(cmd, "dialog"))
        PostMessage(main_wnd, WM_COMMAND, CM_DIALOG, 0);
    if (cmd && _fstrstr(cmd, "syserror"))
        PostMessage(main_wnd, WM_COMMAND, CM_SYSERROR, 0);
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return m.wParam;
}
