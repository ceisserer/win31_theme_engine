/*
 * wstate.c - per-window state, kept in a small table in DGROUP instead of
 * window properties (plan 4.2). Linear probing on the window handle.
 */
#include "rt16.h"

#define WS_SLOTS 512    /* power of two; every classified window has an entry */

static WState table[WS_SLOTS];

static int slot(HWND hwnd)
{
    return (int)(((UINT)hwnd >> 2) & (WS_SLOTS - 1));
}

WState FAR *ws_get(HWND hwnd, BOOL create)
{
    int i = slot(hwnd), n;
    int free_slot = -1;
    for (n = 0; n < WS_SLOTS; n++, i = (i + 1) & (WS_SLOTS - 1)) {
        if (table[i].hwnd == hwnd)
            return &table[i];
        if (!table[i].hwnd) {
            free_slot = i;
            break;
        }
    }
    if (!create)
        return 0;
    if (free_slot < 0) {
        /* full: drop entries of destroyed windows and retry once */
        for (n = 0; n < WS_SLOTS; n++)
            if (table[n].hwnd && !IsWindow(table[n].hwnd))
                ws_forget(table[n].hwnd);
        i = slot(hwnd);
        for (n = 0; n < WS_SLOTS; n++, i = (i + 1) & (WS_SLOTS - 1))
            if (!table[i].hwnd) {
                free_slot = i;
                break;
            }
        if (free_slot < 0)
            return 0;
    }
    _fmemset(&table[free_slot], 0, sizeof(WState));
    table[free_slot].hwnd = hwnd;
    return &table[free_slot];
}

void ws_forget(HWND hwnd)
{
    int i = slot(hwnd), n, j;
    for (n = 0; n < WS_SLOTS; n++, i = (i + 1) & (WS_SLOTS - 1)) {
        if (!table[i].hwnd)
            return;
        if (table[i].hwnd == hwnd)
            break;
    }
    if (n == WS_SLOTS)
        return;
    /* backward-shift deletion keeps probe chains intact */
    table[i].hwnd = 0;
    j = i;
    for (;;) {
        int k, home_between;
        j = (j + 1) & (WS_SLOTS - 1);
        if (!table[j].hwnd)
            return;
        k = slot(table[j].hwnd);
        /* entry j may stay if its home slot lies cyclically in (i, j] */
        home_between = (i <= j) ? (k > i && k <= j) : (k > i || k <= j);
        if (!home_between) {
            table[i] = table[j];
            table[j].hwnd = 0;
            i = j;
        }
    }
}

void ws_clear(void)
{
    _fmemset(table, 0, sizeof(table));
}
