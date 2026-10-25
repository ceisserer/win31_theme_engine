/*
 * rt_types.h - basic types shared by the portable core.
 *
 * The portable core (theme, render, geom, recog) is plain C89. It is built
 * twice: as part of RETHEME.DLL (Open Watcom, 16-bit large model) and on the
 * host (gcc) for tests and previews. "int" is 16 bits on Win16, so anything
 * whose size matters uses the fixed-size types below.
 */
#ifndef RT_TYPES_H
#define RT_TYPES_H

#include <stddef.h>

#if defined(__WATCOMC__) && defined(_M_I86)
#define RT_WIN16 1
typedef unsigned char  rt_u8;
typedef signed char    rt_s8;
typedef unsigned short rt_u16;
typedef short          rt_s16;
typedef unsigned long  rt_u32;
typedef long           rt_s32;
#else
#include <stdint.h>
typedef uint8_t  rt_u8;
typedef int8_t   rt_s8;
typedef uint16_t rt_u16;
typedef int16_t  rt_s16;
typedef uint32_t rt_u32;
typedef int32_t  rt_s32;
#endif

/* Same layout as the Win16 RECT and POINT (16-bit fields). */
typedef struct RtRect  { rt_s16 left, top, right, bottom; } RtRect;
typedef struct RtPoint { rt_s16 x, y; } RtPoint;

/* 0x00BBGGRR, same as COLORREF. */
typedef rt_u32 RtColor;
#define RT_RGB(r, g, b) ((RtColor)(((rt_u32)(rt_u8)(r)) | ((rt_u32)(rt_u8)(g) << 8) | ((rt_u32)(rt_u8)(b) << 16)))
#define RT_R(c) ((rt_u8)((c) & 0xFF))
#define RT_G(c) ((rt_u8)(((c) >> 8) & 0xFF))
#define RT_B(c) ((rt_u8)(((c) >> 16) & 0xFF))

#define RT_RECT_W(r) ((r)->right - (r)->left)
#define RT_RECT_H(r) ((r)->bottom - (r)->top)

void rt_rect_set(RtRect *r, int l, int t, int rr, int b);
void rt_rect_inset(RtRect *r, int l, int t, int rr, int b);
int  rt_rect_empty(const RtRect *r);
int  rt_rect_equal(const RtRect *a, const RtRect *b);
int  rt_rect_contains(const RtRect *r, int x, int y);

/* Platform services, implemented once per platform (platform_host.c,
 * platform_win16.c). */
void *rt_alloc(rt_u32 size);           /* zero-filled; NULL on failure */
void  rt_free(void *p);
/* Reads a whole file into memory (allocated with rt_alloc, plus a trailing
 * NUL byte). Returns NULL on failure. */
char *rt_file_load(const char *path, rt_u32 *len);
/* Current colour of a COLOR_* system colour index. */
RtColor rt_syscolor(int index);

/* Small string helpers (no locale, ASCII only). */
int   rt_stricmp(const char *a, const char *b);
int   rt_strnicmp(const char *a, const char *b, int n);
void  rt_strlcpy(char *dst, const char *src, int size);
int   rt_strlen(const char *s);
/* Parses a decimal integer; *end receives the first unparsed char. */
long  rt_atol(const char *s, const char **end);

#endif
