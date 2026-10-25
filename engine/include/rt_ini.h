/*
 * rt_ini.h - minimal INI parser.
 *
 * GetPrivateProfileString re-reads the file on every call, which is far too
 * slow for a theme with hundreds of keys, so the theme loader parses the file
 * once with this parser. The parser modifies the buffer in place (NUL
 * terminators) and keeps pointers into it.
 */
#ifndef RT_INI_H
#define RT_INI_H

#include "rt_types.h"

typedef struct RtIniKey {
    const char *key;
    const char *val;
} RtIniKey;

typedef struct RtIniSec {
    const char *name;
    rt_u16 first;           /* index of its first key in RtIni.keys */
    rt_u16 count;
} RtIniSec;

typedef struct RtIni {
    char     *buf;          /* owned, freed by rt_ini_free */
    RtIniSec *secs;
    RtIniKey *keys;
    rt_u16    nsecs;
    rt_u16    nkeys;
} RtIni;

/* Takes ownership of 'buf' (allocated with rt_alloc, NUL-terminated).
 * Returns 0 on success. Lines: "[Section]", "key=value", ";comment".
 * A ';' preceded by white space starts a trailing comment. */
int  rt_ini_parse(RtIni *ini, char *buf, rt_u32 len);
int  rt_ini_load(RtIni *ini, const char *path);
void rt_ini_free(RtIni *ini);

/* Section index or -1. Names are case-insensitive. */
int         rt_ini_find(const RtIni *ini, const char *section);
const char *rt_ini_get_in(const RtIni *ini, int sec, const char *key);
const char *rt_ini_get(const RtIni *ini, const char *section, const char *key);

/* Parses up to 'max' comma-separated integers; returns how many were read. */
int rt_parse_ints(const char *s, int *out, int max);

#endif
