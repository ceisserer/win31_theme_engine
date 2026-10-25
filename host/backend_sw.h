/* backend_sw.h - host software surface (see backend_sw.c). */
#ifndef BACKEND_SW_H
#define BACKEND_SW_H

#include "rt_backend.h"

#define SW_MAX_CLIP 16

typedef struct SwClip {
    int    has_clip, has_excl;
    RtRect clip, excl;          /* surface coordinates */
} SwClip;

struct RtSurface {
    int     w, h;
    rt_u32 *px;                 /* 0x00RRGGBB, top-down */
    int     ox, oy;             /* origin: drawing at (0,0) lands on (ox, oy) */
    int     depth;
    SwClip  clips[SW_MAX_CLIP];
};

RtSurface *sw_surface_create(int w, int h, RtColor bg);
void       sw_surface_free(RtSurface *s);
void       sw_set_origin(RtSurface *s, int x, int y);
int        sw_write_png(const RtSurface *s, const char *path);
/* PNG file image in a malloc'd buffer (deterministic output). */
unsigned char *sw_encode_png(const RtSurface *s, size_t *len);

/* platform_host.c: override a system colour (COLOR_* index). */
void host_set_syscolor(int index, RtColor c);
void host_reset_syscolors(void);

#endif
