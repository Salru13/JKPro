/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro game catalog and folder detection. See jk_games.h. */
#include "jk_games.h"

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char *const RESERVED[] = { "System", "Saves", "Logs", "Mods", "devtools", "Export" };

int jk_games_reserved(const char *name)
{
    for (size_t i = 0; i < sizeof RESERVED / sizeof *RESERVED; i++)
        if (!strcasecmp(name, RESERVED[i])) return 1;
    return 0;
}

const char *jk_games_verdict_name(JkGameVerdict v)
{
    switch (v) {
    case JK_GV_READY: return "ready";
    case JK_GV_MODDED: return "modded";
    case JK_GV_NEEDS_DISC: return "needs-disc";
    case JK_GV_MISSING: return "missing";
    default: return "unknown";
    }
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, n - i + 1);
}

static void copy(char *dst, size_t cap, const char *src)
{
    snprintf(dst, cap, "%s", src);
}

static void def_key(JkGameDef *d, const char *section, const char *key, char *val)
{
    if (!strcmp(section, "identity")) {
        if (!strcmp(key, "id")) copy(d->id, sizeof d->id, val);
        else if (!strcmp(key, "name")) copy(d->name, sizeof d->name, val);
        else if (!strcmp(key, "exe")) copy(d->exe, sizeof d->exe, val);
        else if (!strcmp(key, "version")) copy(d->version_kind, sizeof d->version_kind, val);
        else if (!strcmp(key, "marker") && d->nmarkers < JK_GAME_MAX_MARKERS)
            copy(d->markers[d->nmarkers++], sizeof d->markers[0], val);
        else if (!strcmp(key, "marker_ext") && d->nmarker_ext < JK_GAME_MAX_MARKERS)
            copy(d->marker_ext[d->nmarker_ext++], sizeof d->marker_ext[0], val);
    } else if (!strcmp(section, "guid") && d->nguids < JK_GAME_MAX_GUIDS) {
        JkGuidProbe *g = &d->guids[d->nguids];
        char text[64] = "", label[32] = "";
        if (sscanf(val, "%63s %31s", text, label) == 2) {
            g->offset = strtol(key, NULL, 0);
            copy(g->text, sizeof g->text, text);
            copy(g->label, sizeof g->label, label);
            if (g->offset > 0) d->nguids++;
        }
    } else if (!strcmp(section, "defaults") || !strcmp(section, "allowed")) {
        int k = 0;
        while (k < d->ndefaults && strcmp(d->defaults[k].key, key)) k++;
        if (k == d->ndefaults) {
            if (k >= JK_GAME_MAX_DEFAULTS) return;
            memset(&d->defaults[k], 0, sizeof d->defaults[k]);
            copy(d->defaults[k].key, sizeof d->defaults[k].key, key);
            d->ndefaults++;
        }
        if (!strcmp(section, "defaults")) copy(d->defaults[k].value, sizeof d->defaults[k].value, val);
        else copy(d->defaults[k].allowed, sizeof d->defaults[k].allowed, val);
    } else if (!strcmp(section, "foreign") && (!strcmp(key, "loader_dir") || !strcmp(key, "modded_file"))
               && d->nforeign < JK_GAME_MAX_FOREIGN) {
        copy(d->foreign[d->nforeign++], sizeof d->foreign[0], val);
    }
}

static int load_def(const char *path, JkGameDef *d)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    memset(d, 0, sizeof *d);
    char line[512], section[32] = "";
    while (fgets(line, sizeof line, f)) {
        trim(line);
        if (!line[0] || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            char *e = strchr(line, ']');
            if (e) { *e = 0; copy(section, sizeof section, line + 1); }
            continue;
        }
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line, *val = eq + 1;
        trim(key);
        trim(val);
        def_key(d, section, key, val);
    }
    fclose(f);
    if (!d->version_kind[0]) copy(d->version_kind, sizeof d->version_kind, "none");
    return d->id[0] && d->exe[0] ? 0 : -1;
}

static int cmp_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int cmp_name_ci(const void *a, const void *b)
{
    int c = strcasecmp(*(char *const *)a, *(char *const *)b);
    return c ? c : strcmp(*(char *const *)a, *(char *const *)b);
}

/* Entry names of dir (files and/or folders), sorted; caller frees. */
static int list_dir(const char *dir, char ***out, int want_dirs, int ci)
{
    *out = NULL;
    DIR *d = opendir(dir);
    if (!d) return -1;
    int n = 0, cap = 0;
    char **v = NULL;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st)) continue;
        if (want_dirs != !!S_ISDIR(st.st_mode)) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            char **nv = realloc(v, (size_t)cap * sizeof *v);
            if (!nv) break;
            v = nv;
        }
        v[n++] = strdup(e->d_name);
    }
    closedir(d);
    if (n) qsort(v, (size_t)n, sizeof *v, ci ? cmp_name_ci : cmp_name);
    *out = v;
    return n;
}

static void free_list(char **v, int n)
{
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
}

int jk_games_load_defs(const char *defs_dir, JkGameDef *defs, int max)
{
    char **names;
    int n = list_dir(defs_dir, &names, 0, 0);
    if (n < 0) return -1;
    int got = 0;
    for (int i = 0; i < n && got < max; i++) {
        size_t len = strlen(names[i]);
        if (len < 6 || strcmp(names[i] + len - 5, ".game")) continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", defs_dir, names[i]);
        if (!load_def(p, &defs[got])) got++;
    }
    free_list(names, n);
    return got;
}

/* Resolves a relative path below dir, each component matched ignoring case.
 * out gets the real path; returns 1 found, 0 not. */
static int find_ci_path(const char *dir, const char *rel, char *out, size_t cap)
{
    char cur[1024], part[256];
    copy(cur, sizeof cur, dir);
    const char *p = rel;
    while (*p) {
        size_t k = strcspn(p, "/");
        if (k == 0 || k >= sizeof part) return 0;
        memcpy(part, p, k);
        part[k] = 0;
        p += k;
        if (*p == '/') p++;
        DIR *d = opendir(cur);
        if (!d) return 0;
        struct dirent *e;
        char hit[256] = "";
        while ((e = readdir(d)))
            if (!strcasecmp(e->d_name, part) && (!hit[0] || strcmp(e->d_name, hit) < 0)) copy(hit, sizeof hit, e->d_name);
        closedir(d);
        if (!hit[0]) return 0;
        size_t used = strlen(cur);
        if (used + 1 + strlen(hit) + 1 > sizeof cur) return 0;
        cur[used] = '/';
        strcpy(cur + used + 1, hit);
    }
    copy(out, cap, cur);
    return 1;
}

static int has_ext_in(const char *dir, const char *spec)
{
    const char *slash = strrchr(spec, '/');
    if (!slash) return 0;
    char sub[128], real[1024];
    size_t k = (size_t)(slash - spec);
    if (k >= sizeof sub) return 0;
    memcpy(sub, spec, k);
    sub[k] = 0;
    const char *ext = slash + 1;
    if (!find_ci_path(dir, sub, real, sizeof real)) return 0;
    char **names;
    int n = list_dir(real, &names, 0, 0);
    int hit = 0;
    for (int i = 0; i < n && !hit; i++) {
        size_t l = strlen(names[i]), el = strlen(ext);
        hit = l > el && !strcasecmp(names[i] + l - el, ext);
    }
    if (n > 0) free_list(names, n);
    return hit;
}

typedef struct {
    int securom;
    uint32_t rsrc_raw, rsrc_size;
} PeInfo;

static int pe_info(FILE *f, PeInfo *pi)
{
    memset(pi, 0, sizeof *pi);
    unsigned char h[4096];
    size_t n = fread(h, 1, sizeof h, f);
    if (n < 0x40 || h[0] != 'M' || h[1] != 'Z') return -1;
    uint32_t pe;
    memcpy(&pe, h + 0x3c, 4);
    if (pe + 24 > n || memcmp(h + pe, "PE\0\0", 4)) return -1;
    uint16_t nsec, optsz;
    memcpy(&nsec, h + pe + 6, 2);
    memcpy(&optsz, h + pe + 20, 2);
    uint32_t sec = pe + 24 + optsz;
    for (uint32_t i = 0; i < nsec && sec + (i + 1) * 40 <= n; i++) {
        const unsigned char *s = h + sec + i * 40;
        char name[9] = { 0 };
        memcpy(name, s, 8);
        if (!strcasecmp(name, ".securom")) pi->securom = 1;
        if (!strcmp(name, ".rsrc")) {
            memcpy(&pi->rsrc_size, s + 16, 4);
            memcpy(&pi->rsrc_raw, s + 20, 4);
        }
    }
    return 0;
}

static void pe_version(FILE *f, const PeInfo *pi, char *out, size_t cap)
{
    out[0] = 0;
    if (!pi->rsrc_raw || !pi->rsrc_size || pi->rsrc_size > (64u << 20)) return;
    unsigned char *b = malloc(pi->rsrc_size);
    if (!b) return;
    if (!fseek(f, (long)pi->rsrc_raw, SEEK_SET)) {
        size_t n = fread(b, 1, pi->rsrc_size, f);
        for (size_t i = 0; i + 16 <= n; i += 4) {
            uint32_t sig, ms, ls;
            memcpy(&sig, b + i, 4);
            if (sig != 0xFEEF04BDu) continue;
            memcpy(&ms, b + i + 8, 4);
            memcpy(&ls, b + i + 12, 4);
            snprintf(out, cap, "%u.%u.%u.%u", ms >> 16, ms & 0xffff, ls >> 16, ls & 0xffff);
            break;
        }
    }
    free(b);
}

static void guid_version(FILE *f, const JkGameDef *d, char *out, size_t cap)
{
    out[0] = 0;
    for (int i = 0; i < d->nguids; i++) {
        char buf[64] = { 0 };
        size_t want = strlen(d->guids[i].text);
        if (want >= sizeof buf || fseek(f, d->guids[i].offset, SEEK_SET)) continue;
        if (fread(buf, 1, want, f) != want) continue;
        if (!memcmp(buf, d->guids[i].text, want)) { copy(out, cap, d->guids[i].label); return; }
    }
}

/* Tries one definition on one folder. 1 = this game (out filled), 0 = no. */
static int detect(const char *dir, const JkGameDef *d, JkGameFound *out)
{
    char exe[1024];
    if (!find_ci_path(dir, d->exe, exe, sizeof exe)) return 0;
    copy(out->game_id, sizeof out->game_id, d->id);
    copy(out->exe, sizeof out->exe, strrchr(exe, '/') + 1);
    out->verdict = JK_GV_READY;
    char tmp[1024];
    for (int i = 0; i < d->nmarkers; i++)
        if (!find_ci_path(dir, d->markers[i], tmp, sizeof tmp)) {
            out->verdict = JK_GV_MISSING;
            copy(out->missing, sizeof out->missing, d->markers[i]);
            return 1;
        }
    for (int i = 0; i < d->nmarker_ext; i++)
        if (!has_ext_in(dir, d->marker_ext[i])) {
            out->verdict = JK_GV_MISSING;
            copy(out->missing, sizeof out->missing, d->marker_ext[i]);
            return 1;
        }
    FILE *f = fopen(exe, "rb");
    if (f) {
        PeInfo pi;
        if (!pe_info(f, &pi)) {
            if (!strcmp(d->version_kind, "pe")) pe_version(f, &pi, out->version, sizeof out->version);
            else if (!strcmp(d->version_kind, "guid")) guid_version(f, d, out->version, sizeof out->version);
            if (pi.securom) out->verdict = JK_GV_NEEDS_DISC;
        }
        fclose(f);
    }
    if (out->verdict == JK_GV_READY)
        for (int i = 0; i < d->nforeign; i++)
            if (find_ci_path(dir, d->foreign[i], tmp, sizeof tmp)) { out->verdict = JK_GV_MODDED; break; }
    return 1;
}

int jk_games_scan(const char *root, const JkGameDef *defs, int ndefs, JkGameFound *out, int max)
{
    char **names;
    int n = list_dir(root, &names, 1, 1);
    if (n < 0) return 0;
    int got = 0;
    for (int i = 0; i < n && got < max; i++) {
        if (jk_games_reserved(names[i]) || strlen(names[i]) >= sizeof out[0].folder) continue;
        JkGameFound *g = &out[got];
        memset(g, 0, sizeof *g);
        copy(g->folder, sizeof g->folder, names[i]);
        g->verdict = JK_GV_UNKNOWN;
        char dir[1024];
        snprintf(dir, sizeof dir, "%s/%s", root, names[i]);
        for (int k = 0; k < ndefs; k++)
            if (detect(dir, &defs[k], g)) break;
        got++;
    }
    free_list(names, n);
    return got;
}
