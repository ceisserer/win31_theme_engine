/* platform_host.c - rt_types.h platform services for host builds. */
#include <stdio.h>
#include <stdlib.h>
#include "rt_types.h"
#include "backend_sw.h"

void *rt_alloc(rt_u32 size)
{
    return calloc(1, size ? size : 1);
}

void rt_free(void *p)
{
    free(p);
}

char *rt_file_load(const char *path, rt_u32 *len)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) {
        /* theme files use 8.3 upper-case names; also try lower case */
        char lower[256];
        int i;
        for (i = 0; path[i] && i < 255; i++)
            lower[i] = (path[i] >= 'A' && path[i] <= 'Z') ? (char)(path[i] + 32) : path[i];
        lower[i] = 0;
        f = fopen(lower, "rb");
        if (!f)
            return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)rt_alloc((rt_u32)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        rt_free(buf);
        buf = 0;
    }
    fclose(f);
    if (buf && len)
        *len = (rt_u32)n;
    return buf;
}

/* Windows 95 default colour scheme; indices 21/22 are COLOR_3DDKSHADOW and
 * COLOR_3DLIGHT. */
static const RtColor win95_colors[23] = {
    RT_RGB(192, 192, 192), RT_RGB(0, 128, 128), RT_RGB(0, 0, 128), RT_RGB(128, 128, 128),
    RT_RGB(192, 192, 192), RT_RGB(255, 255, 255), RT_RGB(0, 0, 0), RT_RGB(0, 0, 0),
    RT_RGB(0, 0, 0), RT_RGB(255, 255, 255), RT_RGB(192, 192, 192), RT_RGB(192, 192, 192),
    RT_RGB(128, 128, 128), RT_RGB(0, 0, 128), RT_RGB(255, 255, 255), RT_RGB(192, 192, 192),
    RT_RGB(128, 128, 128), RT_RGB(128, 128, 128), RT_RGB(0, 0, 0), RT_RGB(192, 192, 192),
    RT_RGB(255, 255, 255), RT_RGB(0, 0, 0), RT_RGB(192, 192, 192)
};

static RtColor colors[23];
static int colors_init;

void host_reset_syscolors(void)
{
    int i;
    for (i = 0; i < 23; i++)
        colors[i] = win95_colors[i];
    colors_init = 1;
}

void host_set_syscolor(int index, RtColor c)
{
    if (!colors_init)
        host_reset_syscolors();
    if (index >= 0 && index < 23)
        colors[index] = c;
}

RtColor rt_syscolor(int index)
{
    if (!colors_init)
        host_reset_syscolors();
    if (index < 0 || index >= 23)
        return 0;
    return colors[index];
}
