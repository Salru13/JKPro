/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro per-folder paths and settings. See jk_gamecfg.h. */
#include "jk_gamecfg.h"
#include "jk_fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int put(char *dst, size_t cap, const char *a, const char *b, const char *c)
{
    int n = snprintf(dst, cap, "%s/%s%s%s", a, b, c ? "/" : "", c ? c : "");
    return n < 0 || (size_t)n >= cap ? -1 : 0;
}

int jk_game_paths(const char *root, const char *folder, JkGamePaths *out)
{
    memset(out, 0, sizeof *out);
    if (!root || !folder || !folder[0] || strchr(folder, '/') || !strcmp(folder, ".") || !strcmp(folder, "..")
        || strstr(folder, ".."))
        return -1;
    char sys[160];
    if (snprintf(sys, sizeof sys, "%s/System", root) >= (int)sizeof sys) return -1;
    int bad = put(out->game_dir, sizeof out->game_dir, root, folder, NULL)
            | put(out->games_dir, sizeof out->games_dir, sys, "games", folder)
            | put(out->prefix_dir, sizeof out->prefix_dir, sys, "prefix", folder)
            | put(out->cache_dir, sizeof out->cache_dir, sys, "cache", folder)
            | put(out->saves_dir, sizeof out->saves_dir, root, "Saves", folder);
    if (bad) { memset(out, 0, sizeof *out); return -1; }
    return 0;
}

int jk_gamecfg_def_default(JkGameDef *d, const char *key, const char *value, const char *allowed)
{
    if (d->ndefaults >= JK_GAME_MAX_DEFAULTS) return -1;
    JkGameDefault *g = &d->defaults[d->ndefaults++];
    memset(g, 0, sizeof *g);
    snprintf(g->key, sizeof g->key, "%s", key);
    snprintf(g->value, sizeof g->value, "%s", value);
    snprintf(g->allowed, sizeof g->allowed, "%s", allowed ? allowed : "");
    return 0;
}

static const JkGameDefault *find_def(const JkGameDef *d, const char *key)
{
    for (int i = 0; d && i < d->ndefaults; i++)
        if (!strcmp(d->defaults[i].key, key)) return &d->defaults[i];
    return NULL;
}

static int allowed(const JkGameDefault *g, const char *value)
{
    if (!g->allowed[0]) return 1;
    const char *p = g->allowed;
    size_t vl = strlen(value);
    while (*p) {
        size_t k = strcspn(p, "|");
        if (k == vl && !strncmp(p, value, k)) return 1;
        p += k;
        if (*p == '|') p++;
    }
    return 0;
}

static JkGameKV *slot(JkGameSettings *s, const char *key)
{
    for (int i = 0; i < s->n; i++)
        if (!strcmp(s->kv[i].key, key)) return &s->kv[i];
    if (s->n >= JK_GAMECFG_MAX) return NULL;
    JkGameKV *kv = &s->kv[s->n++];
    memset(kv, 0, sizeof *kv);
    snprintf(kv->key, sizeof kv->key, "%.31s", key);
    return kv;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, n - i + 1);
}

int jk_game_settings_load(const JkGamePaths *p, const JkGameDef *d, JkGameSettings *out)
{
    memset(out, 0, sizeof *out);
    out->def = d;
    for (int i = 0; d && i < d->ndefaults; i++) {
        JkGameKV *kv = slot(out, d->defaults[i].key);
        if (!kv) break;
        snprintf(kv->value, sizeof kv->value, "%s", d->defaults[i].value);
        kv->is_default = kv->known = 1;
    }
    char path[256];
    if (snprintf(path, sizeof path, "%s/settings.ini", p->games_dir) >= (int)sizeof path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int warnings = 0;
    char line[256], section[32] = "";
    while (fgets(line, sizeof line, f)) {
        trim(line);
        if (!line[0] || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            char *e = strchr(line, ']');
            if (e) { *e = 0; snprintf(section, sizeof section, "%.31s", line + 1); }
            continue;
        }
        if (strcmp(section, "settings")) continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line, *val = eq + 1;
        trim(key);
        trim(val);
        const JkGameDefault *g = find_def(d, key);
        if (g && !allowed(g, val)) { warnings++; continue; }   /* W501: keep the default */
        JkGameKV *kv = slot(out, key);
        if (!kv) continue;
        snprintf(kv->value, sizeof kv->value, "%.63s", val);
        kv->known = g != NULL;
        kv->is_default = g && !strcmp(g->value, val);
    }
    fclose(f);
    return warnings;
}

const char *jk_game_setting(const JkGameSettings *s, const char *key)
{
    for (int i = 0; i < s->n; i++)
        if (!strcmp(s->kv[i].key, key)) return s->kv[i].value;
    return NULL;
}

int jk_game_settings_set(JkGameSettings *s, const char *key, const char *value)
{
    const JkGameDefault *g = find_def(s->def, key);
    if (!g || !allowed(g, value) || strlen(value) >= sizeof s->kv[0].value) return -1;
    JkGameKV *kv = slot(s, key);
    if (!kv) return -1;
    snprintf(kv->value, sizeof kv->value, "%s", value);
    kv->known = 1;
    kv->is_default = !strcmp(g->value, value);
    return 0;
}

int jk_game_settings_save(const JkGamePaths *p, const JkGameSettings *s)
{
    char text[4096];
    size_t n = (size_t)snprintf(text, sizeof text, "[jkpro]\nschema = 1\ngame = %s\n\n[settings]\n",
                                s->def ? s->def->id : "");
    for (int i = 0; i < s->n && n < sizeof text; i++) {
        if (s->kv[i].is_default) continue;
        n += (size_t)snprintf(text + n, sizeof text - n, "%s = %s\n", s->kv[i].key, s->kv[i].value);
    }
    if (n >= sizeof text) return -1;
    if (jk_fs_mkdir_p(p->games_dir)) return -1;
    char path[256], bak[264];
    if (snprintf(path, sizeof path, "%s/settings.ini", p->games_dir) >= (int)sizeof path) return -1;
    snprintf(bak, sizeof bak, "%s.bak", path);
    if (jk_fs_exists(path)) {
        size_t len = 0;
        char *old = jk_fs_read(path, 1 << 16, &len);
        if (old) { jk_fs_write_atomic(bak, old, len); free(old); }
    }
    return jk_fs_write_atomic(path, text, n) ? -1 : 0;
}
