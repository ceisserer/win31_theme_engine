/*
 * preview.c - renders preview scenes of a theme to PNG files.
 *
 *   preview <theme-dir> <out-dir> [scene ...]
 *
 * Without scene names all scenes are rendered. Theme warnings are printed
 * to stderr; the exit code is 1 if the theme cannot be loaded.
 */
#include <stdio.h>
#include <string.h>
#include "rt_theme.h"
#include "backend_sw.h"
#include "scenes.h"

static int render(const RtTheme *t, const char *out, int scene)
{
    char path[512];
    RtSurface *s = scene_render(t, scene);
    int rc;
    snprintf(path, sizeof(path), "%s/%s.png", out, scene_name(scene));
    rc = sw_write_png(s, path);
    sw_surface_free(s);
    if (rc == 0)
        printf("%s\n", path);
    else
        fprintf(stderr, "cannot write %s\n", path);
    return rc;
}

int main(int argc, char **argv)
{
    RtTheme *t;
    int i, rc = 0;
    if (argc < 3) {
        fprintf(stderr, "usage: %s <theme-dir> <out-dir> [scene ...]\n", argv[0]);
        return 2;
    }
    t = rt_theme_load(argv[1]);
    if (!t) {
        fprintf(stderr, "cannot load theme %s\n", argv[1]);
        return 1;
    }
    if (t->log[0])
        fprintf(stderr, "%s: %u warning(s):\n%s", argv[1], t->nwarn, t->log);
    {
        /* the system colours the engine sets for the theme */
        rt_u8 idx[RT_SYS_COUNT];
        RtColor rgb[RT_SYS_COUNT];
        int n = rt_theme_syscolors(t, idx, rgb);
        for (i = 0; i < n; i++)
            host_set_syscolor(idx[i], rgb[i]);
        if (n)
            rt_theme_syscolor_changed(t);
    }
    if (argc == 3) {
        for (i = 0; i < SCENE_COUNT; i++)
            rc |= render(t, argv[2], i);
    } else {
        for (i = 3; i < argc; i++) {
            int sc = scene_from_name(argv[i]);
            if (sc < 0) {
                fprintf(stderr, "unknown scene %s\n", argv[i]);
                rc = 1;
                continue;
            }
            rc |= render(t, argv[2], sc);
        }
    }
    rt_theme_free(t);
    return rc ? 1 : 0;
}
