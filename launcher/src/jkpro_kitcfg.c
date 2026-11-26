/* JKPro: the SETTINGS > Kitserver switch. See jkpro_kitcfg.h. */
#include "jkpro_kitcfg.h"
#include "jk_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#define KIT_MAX_FILE (1024 * 1024)
#define KIT_MAX_DLLS 64

static const char *WORD[JK_KIT_COUNT] = { "on", "nokits", "off" };
static const char *TEXT[JK_KIT_COUNT] = { "On", "Without kits", "Off" };

const char *jkpro_kit_word(JkKitMode m) { return (unsigned)m < JK_KIT_COUNT ? WORD[m] : WORD[0]; }
const char *jkpro_kit_text(JkKitMode m) { return (unsigned)m < JK_KIT_COUNT ? TEXT[m] : TEXT[0]; }

JkKitMode jkpro_kit_from_word(const char *w)
{
    for (int i = 0; w && i < JK_KIT_COUNT; i++)
        if (!strcmp(w, WORD[i])) return (JkKitMode)i;
    return JK_KIT_ON;
}

/* ---------------------------------------------------------------- one line */

typedef struct {
    size_t start, end;       /* the line, end after its '\n' (if any) */
    size_t p;                /* after a UTF-8 BOM (first line only) */
    size_t q;                /* after leading spaces/tabs */
    int dll;                 /* a "dll = x" line under [kload] (active or commented) */
    int active;
    char name[48];           /* the module, lowercase, without ".dll" */
} KitLine;

static int is_ws(char c) { return c == ' ' || c == '\t'; }
static int is_cmt(char c) { return c == ';' || c == '#'; }

/* Reads the next line at *pos; tracks [kload]. 0 at the end. */
static int next_line(const char *s, size_t n, size_t *pos, int *in_kload, KitLine *l)
{
    if (*pos >= n) return 0;
    memset(l, 0, sizeof *l);
    l->start = *pos;
    size_t e = *pos;
    while (e < n && s[e] != '\n') e++;
    size_t content_end = e;                    /* before '\n' */
    if (content_end > l->start && s[content_end - 1] == '\r') content_end--;
    l->end = e < n ? e + 1 : e;
    *pos = l->end;

    size_t p = l->start;
    if (p == 0 && n >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        p = 3;
    l->p = p;
    size_t q = p;
    while (q < content_end && is_ws(s[q])) q++;
    l->q = q;
    if (q < content_end && s[q] == '[') {
        size_t a = q + 1, b = a;
        while (b < content_end && s[b] != ']') b++;
        while (a < b && is_ws(s[a])) a++;
        size_t z = b;
        while (z > a && is_ws(s[z - 1])) z--;
        *in_kload = z - a == 5 && !strncasecmp(s + a, "kload", 5);
        return 1;
    }
    if (!*in_kload) return 1;

    size_t r = q;
    int commented = 0;
    while (r < content_end && is_cmt(s[r])) { commented = 1; r++; }
    while (r < content_end && is_ws(s[r])) r++;
    if (content_end - r < 3 || strncasecmp(s + r, "dll", 3) != 0) return 1;
    r += 3;
    while (r < content_end && is_ws(s[r])) r++;
    if (r >= content_end || s[r] != '=') return 1;
    r++;
    while (r < content_end && (is_ws(s[r]) || s[r] == '"')) r++;
    size_t v = r;
    while (r < content_end && !is_ws(s[r]) && s[r] != '"' && !is_cmt(s[r])) r++;
    size_t len = r - v;
    if (len == 0) return 1;
    if (len >= sizeof l->name) len = sizeof l->name - 1;
    for (size_t i = 0; i < len; i++) l->name[i] = (char)tolower((unsigned char)s[v + i]);
    l->name[len] = 0;
    size_t nl = strlen(l->name);
    if (nl > 4 && !strcmp(l->name + nl - 4, ".dll")) l->name[nl - 4] = 0;
    l->dll = 1;
    l->active = !commented;
    return 1;
}

/* ---------------------------------------------------------------- text level */

typedef struct { char name[48]; int active; } KitDll;

static int orig_dlls(const char *s, size_t n, KitDll *out, int max)
{
    size_t pos = 0;
    int in_k = 0, k = 0;
    KitLine l;
    while (next_line(s, n, &pos, &in_k, &l))
        if (l.dll && k < max) { memcpy(out[k].name, l.name, sizeof out[k].name); out[k].active = l.active; k++; }
    return k;
}

char *jkpro_kitcfg_apply(const char *cur, size_t n, const char *orig, size_t on, JkKitMode mode, size_t *outn)
{
    static KitDll od[KIT_MAX_DLLS];
    if (!orig) { orig = cur; on = n; }
    int nod = orig_dlls(orig, on, od, KIT_MAX_DLLS);
    /* at most 2 bytes more per line */
    size_t lines = 1;
    for (size_t i = 0; i < n; i++) lines += cur[i] == '\n';
    char *o = malloc(n + 2 * lines + 1);
    if (!o) return NULL;
    size_t w = 0, pos = 0;
    int in_k = 0;
    KitLine l;
    while (next_line(cur, n, &pos, &in_k, &l)) {
        int want = l.active;
        if (l.dll) {
            for (int i = 0; i < nod; i++)
                if (!strcmp(od[i].name, l.name)) { want = od[i].active; break; }   /* not in the original: as it is */
            if (mode == JK_KIT_NOKITS && !strcmp(l.name, "kserv")) want = 0;
            if (mode == JK_KIT_OFF) want = 0;
        }
        if (!l.dll || want == l.active) {
            memcpy(o + w, cur + l.start, l.end - l.start);
            w += l.end - l.start;
        } else if (!want) {                       /* comment out: "; " in front, after a BOM */
            memcpy(o + w, cur + l.start, l.p - l.start); w += l.p - l.start;
            o[w++] = ';'; o[w++] = ' ';
            memcpy(o + w, cur + l.p, l.end - l.p); w += l.end - l.p;
        } else {                                  /* uncomment: drop the ';' run and one space after it */
            memcpy(o + w, cur + l.start, l.q - l.start); w += l.q - l.start;
            size_t r = l.q;
            while (r < l.end && is_cmt(cur[r])) r++;
            if (r < l.end && cur[r] == ' ') r++;
            memcpy(o + w, cur + r, l.end - r); w += l.end - r;
        }
    }
    o[w] = 0;
    *outn = w;
    return o;
}

int jkpro_kitcfg_state(const char *cur, size_t n, const char *orig, size_t on)
{
    for (int m = 0; m < JK_KIT_COUNT; m++) {
        size_t k;
        char *t = jkpro_kitcfg_apply(cur, n, orig, on, (JkKitMode)m, &k);
        if (!t) return -1;
        int same = k == n && !memcmp(t, cur, n);
        free(t);
        if (same) return m;
    }
    return -1;
}

/* ---------------------------------------------------------------- file level */

static void cfg_path(const char *game_dir, char *out, size_t cap)
{
    snprintf(out, cap, "%s/kitserver/config.txt", game_dir);
}

static void orig_path(const char *system_dir, char *out, size_t cap)
{
    snprintf(out, cap, "%s/backup/kitserver/config.txt.original", system_dir);
}

int jkpro_kitcfg_present(const char *game_dir)
{
    char p[1024];
    cfg_path(game_dir, p, sizeof p);
    return jk_fs_exists(p) && !jk_fs_is_dir(p);
}

/* cur and (if kept) the original; 0 ok, 1 no config.txt, -1 failed */
static int load_both(const char *game_dir, const char *system_dir, char **cur, size_t *n, char **orig, size_t *on)
{
    char p[1024], b[1024];
    *cur = *orig = NULL;
    *n = *on = 0;
    if (!jkpro_kitcfg_present(game_dir)) return 1;
    cfg_path(game_dir, p, sizeof p);
    orig_path(system_dir, b, sizeof b);
    *cur = jk_fs_read(p, KIT_MAX_FILE, n);
    if (!*cur) return -1;
    if (jk_fs_exists(b)) {
        *orig = jk_fs_read(b, KIT_MAX_FILE, on);
        if (!*orig) { free(*cur); *cur = NULL; return -1; }
    }
    return 0;
}

int jkpro_kitcfg_sync(const char *game_dir, const char *system_dir, JkKitMode mode)
{
    char *cur, *orig;
    size_t n, on;
    int r = load_both(game_dir, system_dir, &cur, &n, &orig, &on);
    if (r != 0) return r > 0 ? 0 : -1;
    size_t k;
    char *t = jkpro_kitcfg_apply(cur, n, orig, on, mode, &k);
    int rc = 0;
    if (!t) rc = -1;
    else if (k != n || memcmp(t, cur, n) != 0) {
        char p[1024], b[1024], bdir[1024];
        cfg_path(game_dir, p, sizeof p);
        orig_path(system_dir, b, sizeof b);
        snprintf(bdir, sizeof bdir, "%s/backup/kitserver", system_dir);
        if (!orig && (jk_fs_mkdir_p(bdir) != 0 || jk_fs_write_atomic(b, cur, n) != 0)) rc = -1;   /* kept once */
        else if (jk_fs_write_atomic(p, t, k) != 0) rc = -1;
    }
    free(t);
    free(cur);
    free(orig);
    return rc;
}

int jkpro_kitcfg_read_state(const char *game_dir, const char *system_dir)
{
    char *cur, *orig;
    size_t n, on;
    if (load_both(game_dir, system_dir, &cur, &n, &orig, &on) != 0) return -1;
    int st = jkpro_kitcfg_state(cur, n, orig, on);
    free(cur);
    free(orig);
    return st;
}
