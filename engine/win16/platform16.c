/* platform16.c - rt_types.h platform services for the Win16 runtime. */
#include "rt16.h"

/* Memory is GMEM_SHARE so it belongs to the DLL, not to whichever task
 * happens to be current (a task's memory is freed when the task exits). */
void *rt_alloc(rt_u32 size)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, size ? size : 1);
    if (!h)
        return 0;
    return GlobalLock(h);
}

void rt_free(void *p)
{
    HGLOBAL h;
    if (!p)
        return;
    h = (HGLOBAL)LOWORD(GlobalHandle(FP_SEG(p)));
    GlobalUnlock(h);
    GlobalFree(h);
}

/* Only valid in the task that loads the theme (the loader / Control Panel
 * applet): the file handle is opened and closed within this call. */
char *rt_file_load(const char *path, rt_u32 *len)
{
    HFILE f = _lopen(path, OF_READ | OF_SHARE_DENY_WRITE);
    LONG n;
    char *buf;
    if (f == HFILE_ERROR)
        return 0;
    n = _llseek(f, 0, 2);
    _llseek(f, 0, 0);
    if (n < 0 || n > 60000L) {      /* keep within one segment */
        _lclose(f);
        return 0;
    }
    buf = (char *)rt_alloc((rt_u32)n + 1);
    if (buf && _lread(f, buf, (UINT)n) != (UINT)n) {
        rt_free(buf);
        buf = 0;
    }
    _lclose(f);
    if (buf && len)
        *len = (rt_u32)n;
    return buf;
}

RtColor rt_syscolor(int index)
{
    /* Windows 3.1 has no COLOR_3DDKSHADOW / COLOR_3DLIGHT: use the Win95
     * defaults (black, and the button face). */
    if (index == RT_SYS_3DDKSHADOW)
        return RT_RGB(0, 0, 0);
    if (index == RT_SYS_3DLIGHT)
        return (RtColor)GetSysColor(COLOR_BTNFACE);
    return (RtColor)GetSysColor(index);
}
