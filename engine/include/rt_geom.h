/*
 * rt_geom.h - non-client geometry: the theme layout (where the theme draws)
 * and the USER layout (what USER 3.1 computes), and the mapping between the
 * two (plan section 4.4).
 */
#ifndef RT_GEOM_H
#define RT_GEOM_H

#include "rt_types.h"
#include "rt_theme.h"

/* RtNcIn.flags */
#define RT_NC_SYSMENU    0x0001
#define RT_NC_MINBOX     0x0002
#define RT_NC_MAXBOX     0x0004
#define RT_NC_MAXIMIZED  0x0008
#define RT_NC_NOICON     0x0010     /* dialogs: Win95 shows no caption icon */
#define RT_NC_THICKFRAME 0x0020

typedef struct RtNcIn {
    rt_s16 cx, cy;          /* window size */
    rt_s16 frame_x, frame_y;/* frame thickness (USER's), each side */
    rt_s16 caption;         /* caption band height incl. the separator row; 0 = no caption */
    rt_s16 menu;            /* menu bar height, 0 = none */
    rt_s16 cxsize;          /* USER's caption button bitmap width (SM_CXSIZE) */
    rt_s16 cysize;          /* USER's caption bar height (SM_CYSIZE); 0 = caption - 1 */
    rt_u16 flags;
} RtNcIn;

enum RtNcButton { RT_NCB_CLOSE, RT_NCB_MIN, RT_NCB_MAX, RT_NCB_COUNT };

enum RtNcHit {
    RT_HIT_NONE, RT_HIT_CAPTION, RT_HIT_SYSMENU, RT_HIT_MIN, RT_HIT_MAX, RT_HIT_CLOSE
};

typedef struct RtNcLayout {
    RtRect window;          /* 0, 0, cx, cy */
    RtRect inner;           /* window minus frame: hole of the frame part */
    RtRect caption;         /* caption band incl. separator row */
    RtRect icon;            /* empty if none */
    RtRect title;
    RtRect btn[RT_NCB_COUNT];   /* empty if absent */
    RtRect menubar;
    RtRect client;
} RtNcLayout;

/* Theme layout of a window (Win95 rules: min and max always appear
 * together, close needs a system menu, dialogs have no icon). */
void rt_nc_layout(const RtMetrics *m, const RtNcIn *in, RtNcLayout *out);
int  rt_nc_hit(const RtNcLayout *l, int x, int y);

/* USER 3.1 layout of the same window, measured on Windows 3.1x: the caption
 * bar is SM_CYSIZE high at the top of the area inside the frame; the
 * system-menu box on the left and the min/max boxes on the right are each
 * SM_CXSIZE + 1 wide (bitmap plus its separator line); the title is centred
 * in between. */
typedef struct RtUserNc {
    RtRect caption;         /* caption bar without the separator row */
    RtRect sysbox;
    RtRect minbox;
    RtRect maxbox;
} RtUserNc;

void rt_user31_nc(const RtNcIn *in, RtUserNc *out);

/* Maps a window-relative point from the theme layout into the USER layout,
 * so that USER's own hit-testing gives the answer the theme layout implies.
 * Returns the theme hit code; *ux, *uy receive the point to hand to USER.
 * RT_HIT_CLOSE has no USER equivalent: the engine handles it itself. */
int rt_nc_map_point(const RtNcLayout *theme, const RtUserNc *user, int x, int y, int *ux, int *uy);

#endif
