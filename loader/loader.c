/*
 * loader.c - RETHEME.EXE: loads RETHEME.DLL, keeps it resident, writes the
 * engine's log to RETHEME.LOG.
 *
 *   RETHEME [theme-directory]
 *
 * Without an argument the theme comes from RETHEME.INI ([Engine] Theme=),
 * default THEMES\WIN95 next to the executable. The program shows as an
 * icon; closing it unloads the engine.
 */
#include <windows.h>
#include <string.h>

typedef int (FAR PASCAL *START_FN)(LPCSTR);
typedef int (FAR PASCAL *STOP_FN)(void);
typedef int (FAR PASCAL *DRAIN_FN)(LPSTR, int);

static HINSTANCE dll;
static START_FN  rt_start;
static STOP_FN   rt_stop;
static DRAIN_FN  rt_drain;
static char      home[144];        /* directory of RETHEME.EXE, with '\' */
static char      log_path[160];

static void flush_log(void)
{
    char buf[512];
    int n;
    HFILE f;
    if (!rt_drain)
        return;
    n = rt_drain(buf, sizeof(buf));
    if (n <= 0)
        return;
    f = _lopen(log_path, OF_WRITE | OF_SHARE_DENY_WRITE);
    if (f == HFILE_ERROR)
        f = _lcreat(log_path, 0);
    if (f == HFILE_ERROR)
        return;
    _llseek(f, 0, 2);
    do {
        _lwrite(f, buf, n);
        n = rt_drain(buf, sizeof(buf));
    } while (n > 0);
    _lclose(f);
}

static void say(LPCSTR text)
{
    HFILE f = _lopen(log_path, OF_WRITE | OF_SHARE_DENY_WRITE);
    if (f == HFILE_ERROR)
        f = _lcreat(log_path, 0);
    if (f == HFILE_ERROR)
        return;
    _llseek(f, 0, 2);
    _lwrite(f, text, lstrlen(text));
    _lwrite(f, "\r\n", 2);
    _lclose(f);
}

LRESULT CALLBACK __export WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        flush_log();
        return 0;
    case WM_QUERYOPEN:
        return 0;       /* stay an icon */
    case WM_CLOSE:
        if (rt_stop && !rt_stop()) {
            MessageBox(hwnd, "The theme engine is busy. Try again in a moment.", "Retheme16",
                       MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        rt_stop = 0;
        flush_log();
        DestroyWindow(hwnd);
        return 0;
    case WM_ENDSESSION:
        if (wp && rt_stop)
            rt_stop();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;
    char theme[160], path[160], ini[160], *p;
    int rc;

    (void)show;
    if (prev)
        return 0;       /* one instance */
    GetModuleFileName(inst, home, sizeof(home));
    p = strrchr(home, '\\');
    if (p)
        p[1] = 0;
    lstrcpy(log_path, home);
    lstrcat(log_path, "RETHEME.LOG");
    lstrcpy(ini, home);
    lstrcat(ini, "RETHEME.INI");

    if (cmd && *cmd) {
        lstrcpy(theme, cmd);
    } else {
        char def[160];
        lstrcpy(def, home);
        lstrcat(def, "THEMES\\WIN95");
        GetPrivateProfileString("Engine", "Theme", def, theme, sizeof(theme), ini);
    }

    lstrcpy(path, home);
    lstrcat(path, "RETHEME.DLL");
    dll = LoadLibrary(path);
    if ((UINT)dll < 32) {
        char m[80];
        wsprintf(m, "Cannot load RETHEME.DLL (error %u).", (UINT)dll);
        say(m);
        MessageBox(NULL, m, "Retheme16", MB_OK | MB_ICONSTOP);
        return 1;
    }
    rt_start = (START_FN)GetProcAddress(dll, "RTSTART");
    rt_stop = (STOP_FN)GetProcAddress(dll, "RTSTOP");
    rt_drain = (DRAIN_FN)GetProcAddress(dll, "RTDRAINLOG");
    if (!rt_start || !rt_stop || !rt_drain) {
        MessageBox(NULL, "RETHEME.DLL is incomplete.", "Retheme16", MB_OK | MB_ICONSTOP);
        FreeLibrary(dll);
        return 1;
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIcon(inst, "RETHEME");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "Retheme16";
    RegisterClass(&wc);
    hwnd = CreateWindow("Retheme16", "Retheme16", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
                        200, 100, NULL, NULL, inst, NULL);

    say("--- RETHEME start");
    rc = rt_start(theme);
    if (rc == 1 && GetPrivateProfileInt("Engine", "Trace", 0, ini)) {
        typedef void (FAR PASCAL *TRACE_FN)(int);
        TRACE_FN tr = (TRACE_FN)GetProcAddress(dll, "RTSETTRACE");
        if (tr)
            tr(GetPrivateProfileInt("Engine", "Trace", 0, ini));
    }
    flush_log();
    if (rc != 1) {
        char m[200];
        wsprintf(m, "The theme engine could not start (error %d). See RETHEME.LOG.", rc);
        MessageBox(hwnd, m, "Retheme16", MB_OK | MB_ICONSTOP);
        DestroyWindow(hwnd);
        FreeLibrary(dll);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
    SetTimer(hwnd, 1, 100, NULL);
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (rt_stop)
        rt_stop();
    flush_log();
    FreeLibrary(dll);
    return 0;
}
