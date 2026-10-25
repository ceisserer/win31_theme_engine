/*
 * patch.c - in-memory code patches on GDI/USER functions (plan 4.1).
 *
 * A patch overwrites the first 5 bytes of the target with JMP FAR handler.
 * Targets are written through a data alias of their code segment
 * (AllocSelector + PrestoChangoSelector); the segment is page-locked so it
 * can neither move nor be discarded (a reload from disk would drop the JMP).
 *
 * Patches are "armed" only while USER paints something we restyle. Each
 * arming reason has its own reference count; the JMP is written while any
 * reason is active and the engine is not suspended (patch_suspend), which is
 * how handlers and the renderer call the original functions.
 */
#include "rt16.h"
#include <string.h>

typedef struct Patch {
    FARPROC target;
    FARPROC handler;
    UINT    alias;          /* writable selector aliasing the target segment */
    HGLOBAL hseg;           /* code segment handle, page-locked */
    BYTE    saved[5];
    BYTE    jmp[5];
    BYTE    ok;             /* resolved and ready */
    BYTE    written;        /* JMP currently in place */
    BYTE    count[ARM_REASONS]; /* per reason bit: ARM_NC, ARM_TRACK, ARM_CTL, ARM_PERM ... */
    BYTE    held;           /* patch_unhook nesting: keep the original bytes */
} Patch;

static Patch patches[P_COUNT];
static int   suspended;

/* module, ordinal (from Wine's gdi.exe16.spec / user.exe16.spec) */
static const struct { const char *module; WORD ordinal; } targets[P_COUNT] = {
    { "GDI", 29 },      /* PatBlt */
    { "GDI", 34 },      /* BitBlt */
    { "GDI", 351 },     /* ExtTextOut */
    { "GDI", 33 },      /* TextOut */
    { "USER", 76 },     /* PtInRect */
    { "GDI", 35 },      /* StretchBlt */
    { "GDI", 27 },      /* Rectangle */
    { "GDI", 19 },      /* LineTo */
    { "GDI", 37 },      /* Polyline */
    { "GDI", 31 },      /* SetPixel */
    { "USER", 320 },    /* SysErrorBox */
    { "GDI", 400 },     /* FastWindowFrame */
    { "GDI", 91 },      /* GetTextExtent */
    { "GDI", 471 },     /* GetTextExtentPoint */
    { "USER", 62 },     /* SetScrollPos */
    { "USER", 64 },     /* SetScrollRange */
    { "USER", 482 }     /* EnableScrollBar */
};

static int reason_slot(WORD reason)
{
    int i;
    for (i = 0; i < ARM_REASONS; i++)
        if (reason == (WORD)(1 << i))
            return i;
    return 0;
}

FARPROC patch_target(int id)
{
    return patches[id].ok ? patches[id].target : 0;
}

static BYTE FAR *target_bytes(const Patch *p)
{
    return (BYTE FAR *)MK_FP(p->alias, FP_OFF(p->target));
}

static void write_jmp(Patch *p)
{
    if (!p->ok || p->written)
        return;
    _fmemcpy(target_bytes(p), p->jmp, 5);
    p->written = 1;
}

static void write_orig(Patch *p)
{
    BYTE FAR *b;
    if (!p->ok || !p->written)
        return;
    b = target_bytes(p);
    /* Only restore if the JMP is still ours: someone may have patched on
     * top of us, and writing our saved bytes would break their patch. */
    if (_fmemcmp(b, p->jmp, 5) == 0)
        _fmemcpy(b, p->saved, 5);
    else
        log_msg("patch %d: foreign code on target, left in place", (int)(p - patches));
    p->written = 0;
}

static BOOL armed(const Patch *p)
{
    int i;
    for (i = 0; i < ARM_REASONS; i++)
        if (p->count[i])
            return TRUE;
    return FALSE;
}

static void sync(Patch *p)
{
    if (armed(p) && !suspended && !p->held)
        write_jmp(p);
    else
        write_orig(p);
}

BOOL patch_init(void)
{
    int i;
    for (i = 0; i < P_COUNT; i++) {
        Patch *p = &patches[i];
        HINSTANCE mod = GetModuleHandle(targets[i].module);
        FARPROC h = intercept_handler(i);
        BYTE FAR *src;
        memset(p, 0, sizeof(*p));
        if (!mod || !h)
            continue;
        p->target = GetProcAddress(mod, MAKEINTRESOURCE(targets[i].ordinal));
        if (!p->target) {
            log_msg("patch %d: %s.%u not found", i, targets[i].module, targets[i].ordinal);
            continue;
        }
        p->hseg = GetCodeHandle(p->target);     /* loads the segment if needed */
        if (!p->hseg || !GlobalPageLock(p->hseg)) {
            log_msg("patch %d: cannot lock segment", i);
            continue;
        }
        p->alias = AllocSelector(FP_SEG(p->target));
        if (!p->alias) {
            GlobalPageUnlock(p->hseg);
            continue;
        }
        PrestoChangoSelector(FP_SEG(p->target), p->alias);
        src = target_bytes(p);
        _fmemcpy(p->saved, src, 5);
        p->handler = h;
        p->jmp[0] = 0xEA;                       /* JMP FAR ptr16:16 */
        p->jmp[1] = (BYTE)FP_OFF(h);
        p->jmp[2] = (BYTE)(FP_OFF(h) >> 8);
        p->jmp[3] = (BYTE)FP_SEG(h);
        p->jmp[4] = (BYTE)(FP_SEG(h) >> 8);
        p->ok = 1;
        log_msg("patch %d: %s.%u at %04X:%04X", i, targets[i].module, targets[i].ordinal,
                FP_SEG(p->target), FP_OFF(p->target));
    }
    return TRUE;
}

void patch_remove_all(void)
{
    int i;
    for (i = 0; i < P_COUNT; i++) {
        _fmemset(patches[i].count, 0, sizeof(patches[i].count));
        write_orig(&patches[i]);
    }
}

void patch_shutdown(void)
{
    int i;
    patch_remove_all();
    for (i = 0; i < P_COUNT; i++) {
        Patch *p = &patches[i];
        if (!p->ok)
            continue;
        FreeSelector(p->alias);
        GlobalPageUnlock(p->hseg);
        p->ok = 0;
    }
}

void patch_arm(int id, WORD reason)
{
    Patch *p = &patches[id];
    int r = reason_slot(reason);
    if (p->count[r] < 255)
        p->count[r]++;
    sync(p);
}

void patch_disarm(int id, WORD reason)
{
    Patch *p = &patches[id];
    int r = reason_slot(reason);
    if (p->count[r])
        p->count[r]--;
    sync(p);
}

WORD patch_reasons(int id)
{
    const Patch *p = &patches[id];
    WORD r = 0;
    int i;
    for (i = 0; i < ARM_REASONS; i++)
        if (p->count[i])
            r |= (WORD)(1 << i);
    return r;
}

void patch_suspend(void)
{
    int i;
    if (suspended++ == 0)
        for (i = 0; i < P_COUNT; i++)
            write_orig(&patches[i]);
}

void patch_resume(void)
{
    int i;
    if (suspended == 0 || --suspended != 0)
        return;
    for (i = 0; i < P_COUNT; i++)
        sync(&patches[i]);
}

void patch_unhook(int id)
{
    Patch *p = &patches[id];
    p->held++;
    write_orig(p);
}

void patch_rehook(int id)
{
    Patch *p = &patches[id];
    if (p->held)
        p->held--;
    sync(p);
}
