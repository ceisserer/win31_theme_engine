/*
 * test_main.c - host test suite for the portable core.
 *
 *   rttest                  run all tests
 *   rttest --update-golden  rewrite the golden PNGs in host/tests/golden from the renderer
 *
 * Run from the repository root (paths are relative to it).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rt_theme.h"
#include "rt_ini.h"
#include "rt_geom.h"
#include "backend_sw.h"
#include "scenes.h"

static int failures, checks;
static int update_golden;

#define CHECK(cond) check_((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) check_eq_((long)(a), (long)(b), #a, #b, __FILE__, __LINE__)

static void check_(int ok, const char *what, const char *file, int line)
{
    checks++;
    if (!ok) {
        failures++;
        fprintf(stderr, "%s:%d: FAILED: %s\n", file, line, what);
    }
}

static void check_eq_(long a, long b, const char *sa, const char *sb, const char *file, int line)
{
    checks++;
    if (a != b) {
        failures++;
        fprintf(stderr, "%s:%d: FAILED: %s == %s (%ld != %ld)\n", file, line, sa, sb, a, b);
    }
}

static int rect_is(const RtRect *r, int l, int t, int rr, int b)
{
    if (r->left == l && r->top == t && r->right == rr && r->bottom == b)
        return 1;
    fprintf(stderr, "  rect is (%d,%d,%d,%d), expected (%d,%d,%d,%d)\n",
            r->left, r->top, r->right, r->bottom, l, t, rr, b);
    return 0;
}

static char *dup_str(const char *s)
{
    char *p = (char *)rt_alloc((rt_u32)strlen(s) + 1);
    strcpy(p, s);
    return p;
}

/* ---- INI parser ---- */

static void test_ini(void)
{
    RtIni ini;
    char *buf = dup_str(
        "top=1\n"
        "[First]\r\n"
        "  key = value with spaces  \n"
        "; comment\n"
        "Other=a;b ; trailing comment\n"
        "[ second ]\n"
        "Nums=1, -2,3 ,4\n"
        "empty=\n"
        "noequals\n");
    int v[5];
    CHECK_EQ(rt_ini_parse(&ini, buf, (rt_u32)strlen(buf)), 0);
    CHECK_EQ(ini.nsecs, 3);
    CHECK(strcmp(rt_ini_get_in(&ini, 0, "top"), "1") == 0);
    CHECK(strcmp(rt_ini_get(&ini, "first", "KEY"), "value with spaces") == 0);
    CHECK(strcmp(rt_ini_get(&ini, "First", "Other"), "a;b") == 0);
    CHECK(rt_ini_get(&ini, "Second", "Nums") != 0);
    CHECK_EQ(rt_parse_ints(rt_ini_get(&ini, "Second", "Nums"), v, 5), 4);
    CHECK_EQ(v[1], -2);
    CHECK_EQ(v[3], 4);
    CHECK(strcmp(rt_ini_get(&ini, "Second", "empty"), "") == 0);
    CHECK(rt_ini_get(&ini, "Second", "noequals") == 0);
    CHECK(rt_ini_get(&ini, "Missing", "key") == 0);
    rt_ini_free(&ini);
}

/* ---- names and fallbacks ---- */

static void test_names(void)
{
    CHECK_EQ(rt_part_from_name("CaptionText", 11), RT_PART_CAPTIONTEXT);
    CHECK_EQ(rt_part_from_name("captiontext", 11), RT_PART_CAPTIONTEXT);
    CHECK_EQ(rt_part_from_name("Caption.Active", 7), RT_PART_CAPTION);
    CHECK_EQ(rt_part_from_name("Nope", 4), -1);
    CHECK_EQ(rt_state_from_name("checkedpressed"), RT_ST_CHECKEDPRESSED);
    CHECK_EQ(rt_glyph_from_name("Restore"), RT_GLYPH_RESTORE);
    CHECK_EQ(rt_state_fallback(RT_ST_CHECKEDPRESSED), RT_ST_CHECKED);
    CHECK_EQ(rt_state_fallback(RT_ST_CHECKED), RT_ST_NORMAL);
    CHECK_EQ(rt_state_fallback(RT_ST_MIXEDDISABLED), RT_ST_CHECKEDDISABLED);
}

/* ---- theme loading ---- */

static void test_theme_classic(void)
{
    RtTheme *t = rt_theme_load("themes/WIN95");
    const RtTheme *owner;
    int p;
    CHECK(t != 0);
    if (!t)
        return;
    if (t->log[0])
        fprintf(stderr, "WIN95 warnings:\n%s", t->log);
    CHECK_EQ(t->nwarn, 0);
    CHECK(strcmp(t->name, "Redmond 95") == 0);
    CHECK_EQ(t->metrics.btn_w, 16);
    CHECK_EQ(t->metrics.btn_h, 14);
    /* Classic95 must define every part (it is the root of all themes);
     * window parts have Active/Inactive instead of Normal. */
    for (p = 0; p < RT_PART_COUNT; p++) {
        int ok = rt_theme_find(t, p, RT_ST_NORMAL, &owner) != 0
              || (rt_theme_find(t, p, RT_ST_ACTIVE, &owner) != 0
                  && rt_theme_find(t, p, RT_ST_INACTIVE, &owner) != 0);
        if (!ok)
            fprintf(stderr, "  part %s is not defined\n", rt_part_names[p]);
        CHECK(ok);
    }
    /* state fallback: CheckedPressed is defined, MixedDisabled falls back */
    CHECK(rt_theme_find(t, RT_PART_CHECKBOX, RT_ST_MIXEDDISABLED, &owner)
          == rt_theme_find(t, RT_PART_CHECKBOX, RT_ST_CHECKEDDISABLED, &owner));
    /* system colours are live */
    host_set_syscolor(2, RT_RGB(1, 2, 3));
    rt_theme_syscolor_changed(t);
    {
        RtSurface *s = sw_surface_create(10, 10, 0);
        RtDrawReq q;
        memset(&q, 0, sizeof(q));
        q.part = RT_PART_CAPTION;
        q.state = RT_ST_ACTIVE;
        rt_rect_set(&q.rc, 0, 0, 10, 10);
        CHECK(rt_render(t, s, &q));
        CHECK_EQ(s->px[0], 0x010203);       /* bar */
        CHECK_EQ(s->px[9 * 10], 0xC0C0C0);  /* separator row */
        sw_surface_free(s);
    }
    host_reset_syscolors();
    rt_theme_free(t);
}

/* Every bundled theme must load without warnings. */
static void test_theme_bundled(void)
{
    static const char *const dirs[] = { "themes/WIN95", "themes/WIN98", "themes/WINME", "themes/SLATE" };
    int i;
    for (i = 0; i < (int)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        RtTheme *t = rt_theme_load(dirs[i]);
        CHECK(t != 0);
        if (!t)
            continue;
        if (t->log[0])
            fprintf(stderr, "%s warnings:\n%s", dirs[i], t->log);
        CHECK_EQ(t->nwarn, 0);
        CHECK(t->base != 0 || strcmp(dirs[i], "themes/WIN95") == 0);
        rt_theme_free(t);
    }
}

/* [SysColors]: the colours a theme sets while it runs */
static void test_theme_syscolors(void)
{
    rt_u8 idx[RT_SYS_COUNT];
    RtColor rgb[RT_SYS_COUNT];
    RtTheme *t = rt_theme_load("themes/WIN98");
    int i, n;
    CHECK(t != 0);
    if (t) {
        CHECK_EQ(rt_theme_syscolors(t, idx, rgb), 0);
        rt_theme_free(t);
    }
    t = rt_theme_load("themes/WINME");
    CHECK(t != 0);
    if (!t)
        return;
    n = rt_theme_syscolors(t, idx, rgb);
    CHECK_EQ(n, RT_SYS_COUNT);
    for (i = 0; i < n && idx[i] != 15; i++)     /* BtnFace */
        ;
    CHECK(i < n && rgb[i] == RT_RGB(212, 208, 200));
    rt_theme_free(t);
}

static void test_theme_errors(void)
{
    CHECK(rt_theme_load("host/tests/data/does-not-exist") == 0);
}

static void test_theme_bitmap(void)
{
    RtTheme *t;
    RtSurface *s;
    RtDrawReq q;
    rt_theme_set_search_dir("themes");
    t = rt_theme_load("host/tests/data/BMPTEST");
    rt_theme_set_search_dir(0);
    CHECK(t != 0);
    if (!t)
        return;
    if (t->log[0])
        fprintf(stderr, "BMPTEST warnings:\n%s", t->log);
    CHECK_EQ(t->nwarn, 0);
    CHECK(t->base != 0);
    /* nine-grid with colour key and recolouring */
    s = sw_surface_create(30, 20, RT_RGB(0, 0, 255));
    memset(&q, 0, sizeof(q));
    q.part = RT_PART_PUSHBUTTON;
    q.state = RT_ST_NORMAL;
    rt_rect_set(&q.rc, 0, 0, 30, 20);
    CHECK(rt_render(t, s, &q));
    CHECK_EQ(s->px[0], 0x0000FF);               /* keyed corner stays */
    CHECK_EQ(s->px[1], 0x000000);               /* border */
    CHECK_EQ(s->px[2 * 30 + 3], 0xC0C0C0);      /* yellow recoloured to BtnFace */
    CHECK_EQ(s->px[29], 0x0000FF);
    CHECK_EQ(s->px[19 * 30 + 29], 0x0000FF);
    CHECK_EQ(s->px[18 * 30 + 29], 0x000000);    /* right border, stretched */
    sw_surface_free(s);
    /* unrecoloured stretch: yellow stays */
    s = sw_surface_create(24, 24, 0);
    q.state = RT_ST_PRESSED;
    rt_rect_set(&q.rc, 0, 0, 24, 24);
    CHECK(rt_render(t, s, &q));
    CHECK_EQ(s->px[4 * 24 + 4], 0xFFFF00);
    CHECK_EQ(s->px[0], 0xFF00FF);               /* not keyed here */
    sw_surface_free(s);
    /* inherited part comes from the base theme */
    CHECK(rt_theme_find(t, RT_PART_BTNCLOSE, RT_ST_NORMAL, 0) != 0);
    rt_theme_free(t);
}

/* ---- geometry ---- */

static void test_layout(void)
{
    RtTheme *t = rt_theme_load("themes/WIN95");
    RtNcIn in;
    RtNcLayout l;
    RtUserNc u;
    int ux, uy, hit;
    if (!t) {
        CHECK(t != 0);
        return;
    }
    memset(&in, 0, sizeof(in));
    in.cx = 300;
    in.cy = 200;
    in.frame_x = in.frame_y = 4;
    in.caption = 19;
    in.menu = 19;
    in.cxsize = 18;
    in.flags = RT_NC_SYSMENU | RT_NC_MINBOX | RT_NC_MAXBOX;
    rt_nc_layout(&t->metrics, &in, &l);
    CHECK(rect_is(&l.caption, 4, 4, 296, 23));
    CHECK(rect_is(&l.btn[RT_NCB_CLOSE], 278, 6, 294, 20));
    CHECK(rect_is(&l.btn[RT_NCB_MAX], 260, 6, 276, 20));
    CHECK(rect_is(&l.btn[RT_NCB_MIN], 244, 6, 260, 20));
    CHECK(rect_is(&l.icon, 6, 5, 22, 21));
    CHECK(rect_is(&l.title, 24, 4, 242, 22));
    CHECK(rect_is(&l.menubar, 4, 23, 296, 42));
    CHECK(rect_is(&l.client, 4, 42, 296, 196));

    /* Win95 rule: only a max box still shows both buttons */
    in.flags = RT_NC_SYSMENU | RT_NC_MAXBOX;
    rt_nc_layout(&t->metrics, &in, &l);
    CHECK(!rt_rect_empty(&l.btn[RT_NCB_MIN]));
    /* dialog: close only, no icon */
    in.flags = RT_NC_SYSMENU | RT_NC_NOICON;
    rt_nc_layout(&t->metrics, &in, &l);
    CHECK(rt_rect_empty(&l.btn[RT_NCB_MIN]) && rt_rect_empty(&l.btn[RT_NCB_MAX]));
    CHECK(rt_rect_empty(&l.icon));
    CHECK(rect_is(&l.btn[RT_NCB_CLOSE], 278, 6, 294, 20));
    CHECK_EQ(l.title.left, 6);

    /* mapping theme points to USER 3.1 points */
    in.flags = RT_NC_SYSMENU | RT_NC_MINBOX | RT_NC_MAXBOX;
    rt_nc_layout(&t->metrics, &in, &l);
    in.cysize = 18;
    rt_user31_nc(&in, &u);
    /* measured on WfW 3.11 / VGA: SM_CXSIZE = SM_CYSIZE = 18, frame 4 */
    CHECK(rect_is(&u.caption, 4, 4, 296, 22));
    CHECK(rect_is(&u.sysbox, 4, 4, 23, 22));
    CHECK(rect_is(&u.maxbox, 277, 4, 296, 22));
    CHECK(rect_is(&u.minbox, 258, 4, 277, 22));
    hit = rt_nc_map_point(&l, &u, 250, 10, &ux, &uy);      /* theme min button */
    CHECK_EQ(hit, RT_HIT_MIN);
    CHECK(rt_rect_contains(&u.minbox, ux, uy));
    hit = rt_nc_map_point(&l, &u, 265, 10, &ux, &uy);      /* theme max button */
    CHECK_EQ(hit, RT_HIT_MAX);
    CHECK(rt_rect_contains(&u.maxbox, ux, uy));
    hit = rt_nc_map_point(&l, &u, 285, 10, &ux, &uy);      /* close: engine's own */
    CHECK_EQ(hit, RT_HIT_CLOSE);
    hit = rt_nc_map_point(&l, &u, 10, 10, &ux, &uy);       /* icon -> system box */
    CHECK_EQ(hit, RT_HIT_SYSMENU);
    CHECK(rt_rect_contains(&u.sysbox, ux, uy));
    hit = rt_nc_map_point(&l, &u, 262, 21, &ux, &uy);      /* caption below the buttons */
    CHECK_EQ(hit, RT_HIT_CAPTION);
    CHECK(!rt_rect_contains(&u.minbox, ux, uy) && !rt_rect_contains(&u.maxbox, ux, uy));
    CHECK(rt_rect_contains(&u.caption, ux, uy));
    hit = rt_nc_map_point(&l, &u, 100, 10, &ux, &uy);      /* plain caption stays */
    CHECK_EQ(hit, RT_HIT_CAPTION);
    CHECK(ux == 100 && uy == 10);
    rt_theme_free(t);
}

static void test_glyphs(void)
{
    RtRect r;
    int w, h;
    rt_rect_set(&r, 0, 0, 16, 16);
    rt_glyph_size(RT_GLYPH_ARROWUP, &r, &w, &h);
    CHECK(w == 7 && h == 4);
    rt_glyph_size(RT_GLYPH_ARROWLEFT, &r, &w, &h);
    CHECK(w == 4 && h == 7);
    rt_rect_set(&r, 0, 0, 17, 17);      /* Win 3.1 scroll bar */
    rt_glyph_size(RT_GLYPH_ARROWDOWN, &r, &w, &h);
    CHECK(w == 7 && h == 4);
    rt_glyph_size(RT_GLYPH_CLOSE, &r, &w, &h);
    CHECK(w == 8 && h == 7);
}

/* ---- golden images ---- */

static unsigned char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long n;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = 0;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static void golden(const char *theme_dir, const char *tag)
{
    RtTheme *t;
    int sc;
    rt_theme_set_search_dir("themes");
    t = rt_theme_load(theme_dir);
    rt_theme_set_search_dir(0);
    CHECK(t != 0);
    if (!t)
        return;
    for (sc = 0; sc < SCENE_COUNT; sc++) {
        char path[256];
        RtSurface *s = scene_render(t, sc);
        size_t len, glen;
        unsigned char *png = sw_encode_png(s, &len), *gold;
        snprintf(path, sizeof(path), "host/tests/golden/%s_%s.png", tag, scene_name(sc));
        if (update_golden) {
            CHECK_EQ(sw_write_png(s, path), 0);
        } else {
            gold = read_file(path, &glen);
            checks++;
            if (!gold || glen != len || memcmp(gold, png, len) != 0) {
                char out[256];
                failures++;
                snprintf(out, sizeof(out), "build/host/%s_%s.actual.png", tag, scene_name(sc));
                sw_write_png(s, out);
                fprintf(stderr, "golden mismatch: %s (actual image: %s)\n", path, out);
            }
            free(gold);
        }
        free(png);
        sw_surface_free(s);
    }
    rt_theme_free(t);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--update-golden") == 0)
        update_golden = 1;
    test_ini();
    test_names();
    test_theme_classic();
    test_theme_errors();
    test_theme_bundled();
    test_theme_syscolors();
    test_theme_bitmap();
    test_layout();
    test_glyphs();
    golden("themes/WIN95", "win95");
    golden("themes/WIN98", "win98");
    golden("themes/SLATE", "slate");
    golden("host/tests/data/BMPTEST", "bmptest");
    printf("%d checks, %d failures%s\n", checks, failures, update_golden ? " (golden images updated)" : "");
    return failures ? 1 : 0;
}
