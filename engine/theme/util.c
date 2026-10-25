/* util.c - rectangle and string helpers for the portable core. */
#include "rt_types.h"

void rt_rect_set(RtRect *r, int l, int t, int rr, int b)
{
    r->left = (rt_s16)l;
    r->top = (rt_s16)t;
    r->right = (rt_s16)rr;
    r->bottom = (rt_s16)b;
}

void rt_rect_inset(RtRect *r, int l, int t, int rr, int b)
{
    r->left = (rt_s16)(r->left + l);
    r->top = (rt_s16)(r->top + t);
    r->right = (rt_s16)(r->right - rr);
    r->bottom = (rt_s16)(r->bottom - b);
}

int rt_rect_empty(const RtRect *r)
{
    return r->right <= r->left || r->bottom <= r->top;
}

int rt_rect_equal(const RtRect *a, const RtRect *b)
{
    return a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom;
}

int rt_rect_contains(const RtRect *r, int x, int y)
{
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

static int lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

int rt_stricmp(const char *a, const char *b)
{
    for (;;) {
        int ca = lower((unsigned char)*a++);
        int cb = lower((unsigned char)*b++);
        if (ca != cb)
            return ca - cb;
        if (ca == 0)
            return 0;
    }
}

int rt_strnicmp(const char *a, const char *b, int n)
{
    while (n-- > 0) {
        int ca = lower((unsigned char)*a++);
        int cb = lower((unsigned char)*b++);
        if (ca != cb)
            return ca - cb;
        if (ca == 0)
            return 0;
    }
    return 0;
}

void rt_strlcpy(char *dst, const char *src, int size)
{
    if (size <= 0)
        return;
    while (--size > 0 && *src)
        *dst++ = *src++;
    *dst = 0;
}

int rt_strlen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

long rt_atol(const char *s, const char **end)
{
    long v = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-' || *s == '+')
        neg = (*s++ == '-');
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    if (end)
        *end = s;
    return neg ? -v : v;
}
