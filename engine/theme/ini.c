/* ini.c - minimal in-place INI parser (see rt_ini.h). */
#include "rt_ini.h"

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static char *trim(char *s, char *end)
{
    while (s < end && is_space(*s))
        s++;
    while (end > s && is_space(end[-1]))
        end--;
    *end = 0;
    return s;
}

int rt_ini_parse(RtIni *ini, char *buf, rt_u32 len)
{
    rt_u32 i;
    rt_u16 lines = 1;
    rt_u16 ns = 0, nk = 0;
    char *p, *end;

    ini->buf = buf;
    ini->secs = 0;
    ini->keys = 0;
    ini->nsecs = ini->nkeys = 0;

    for (i = 0; i < len; i++)
        if (buf[i] == '\n' && lines < 0xFFFE)
            lines++;
    /* Upper bound: every line is a section or a key. */
    ini->secs = (RtIniSec *)rt_alloc((rt_u32)(lines + 1) * sizeof(RtIniSec));
    ini->keys = (RtIniKey *)rt_alloc((rt_u32)lines * sizeof(RtIniKey));
    if (!ini->secs || !ini->keys) {
        rt_ini_free(ini);
        return -1;
    }
    /* Keys before the first section go into an unnamed section 0. */
    ini->secs[0].name = "";
    ini->secs[0].first = 0;
    ns = 1;

    p = buf;
    end = buf + len;
    while (p < end) {
        char *line = p, *eol, *s, *e;
        while (p < end && *p != '\n')
            p++;
        eol = p;
        if (p < end)
            p++;
        s = line;
        while (s < eol && is_space(*s))
            s++;
        if (s >= eol || *s == ';' || *s == '#')
            continue;
        if (*s == '[') {
            e = s + 1;
            while (e < eol && *e != ']')
                e++;
            ini->secs[ns - 1].count = (rt_u16)(nk - ini->secs[ns - 1].first);
            ini->secs[ns].name = trim(s + 1, e);
            ini->secs[ns].first = nk;
            ns++;
            continue;
        }
        e = s;
        while (e < eol && *e != '=')
            e++;
        if (e >= eol)
            continue;   /* not a key line */
        {
            char *v = e + 1, *c = v;
            /* trailing comment: ';' after white space */
            while (c < eol) {
                if (*c == ';' && c > v && is_space(c[-1]))
                    break;
                c++;
            }
            ini->keys[nk].key = trim(s, e);
            ini->keys[nk].val = trim(v, c);
            nk++;
        }
    }
    ini->secs[ns - 1].count = (rt_u16)(nk - ini->secs[ns - 1].first);
    ini->nsecs = ns;
    ini->nkeys = nk;
    return 0;
}

int rt_ini_load(RtIni *ini, const char *path)
{
    rt_u32 len;
    char *buf = rt_file_load(path, &len);
    if (!buf) {
        ini->buf = 0;
        ini->secs = 0;
        ini->keys = 0;
        ini->nsecs = ini->nkeys = 0;
        return -1;
    }
    return rt_ini_parse(ini, buf, len);
}

void rt_ini_free(RtIni *ini)
{
    if (ini->buf)
        rt_free(ini->buf);
    if (ini->secs)
        rt_free(ini->secs);
    if (ini->keys)
        rt_free(ini->keys);
    ini->buf = 0;
    ini->secs = 0;
    ini->keys = 0;
    ini->nsecs = ini->nkeys = 0;
}

int rt_ini_find(const RtIni *ini, const char *section)
{
    int i;
    for (i = 1; i < ini->nsecs; i++)
        if (rt_stricmp(ini->secs[i].name, section) == 0)
            return i;
    return -1;
}

const char *rt_ini_get_in(const RtIni *ini, int sec, const char *key)
{
    const RtIniSec *s;
    int i;
    if (sec < 0 || sec >= ini->nsecs)
        return 0;
    s = &ini->secs[sec];
    for (i = 0; i < s->count; i++)
        if (rt_stricmp(ini->keys[s->first + i].key, key) == 0)
            return ini->keys[s->first + i].val;
    return 0;
}

const char *rt_ini_get(const RtIni *ini, const char *section, const char *key)
{
    return rt_ini_get_in(ini, rt_ini_find(ini, section), key);
}

int rt_parse_ints(const char *s, int *out, int max)
{
    int n = 0;
    const char *e;
    if (!s)
        return 0;
    while (n < max) {
        while (*s == ' ' || *s == '\t')
            s++;
        if (!((*s >= '0' && *s <= '9') || *s == '-' || *s == '+'))
            break;
        out[n++] = (int)rt_atol(s, &e);
        s = e;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s != ',')
            break;
        s++;
    }
    return n;
}
