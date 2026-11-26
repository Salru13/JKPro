/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* The JKPro launcher UI: state machine, PATCH, settings, saves. See jkpro_ui.h. */
#include "jkpro_ui.h"

/* Paths and UI texts are cut to their buffers on purpose (a path longer than
 * PATH_MAX_JP is refused by the file calls that follow); GCC's truncation
 * guesses are not errors here. */
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "jk_fs.h"
#include "jk_gamecfg.h"
#include "jk_games.h"
#include "jk_regimport.h"
#include "jk_support.h"
#include "jk_screens.h"
#include "jkpro_datapack.h"
#include "jkpro_kitcfg.h"
#include "jkpro_fservfix.h"
#include "jkpro_online.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define REG_KEY "Software\\KONAMI\\PES2013"
#define PATH_MAX_JP 1024

/* ---------------------------------------------------------------- small helpers */

static void cpy(char *out, size_t cap, const char *s)
{
    if (cap) snprintf(out, cap, "%s", s ? s : "");
}

static void jp_log(JpApp *a, const char *fmt, const char *x)
{
    if (!a->s || !a->s->log) return;
    char line[600];
    snprintf(line, sizeof line, fmt, x ? x : "");
    a->s->log(a->s->ctx, line);
}

static const char *base_name(const char *p)
{
    const char *b = strrchr(p, '/');
    return b ? b + 1 : p;
}

/* A fresh stamp for a backup or export folder: the host's local time, else a
 * counter. The caller makes it unique in its folder. */
static void stamp(JpApp *a, char *out, size_t cap)
{
    if (a->s->wall_time && a->s->wall_time(a->s->ctx, out, cap) == 0 && out[0]) return;
    static unsigned n;
    snprintf(out, cap, "auto-%06u", ++n);
}

/* <dir>/<name>, then <dir>/<name>-2, -3 ... until it does not exist. */
static int unique_dir(const char *dir, const char *name, char *out, size_t cap)
{
    for (int i = 1; i < 100; i++) {
        if (i == 1) snprintf(out, cap, "%s/%s", dir, name);
        else snprintf(out, cap, "%s/%s-%d", dir, name, i);
        if (!jk_fs_exists(out)) return 0;
    }
    return -1;
}

typedef struct { char name[128]; int is_dir; } Ent;

/* Sorted entries of dir (no dot entries). Returns the count (at most max). */
static int list(const char *dir, Ent *out, int max)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof out[0].name) continue;
        char p[PATH_MAX_JP];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) continue;
        cpy(out[n].name, sizeof out[n].name, e->d_name);
        out[n].is_dir = S_ISDIR(st.st_mode);
        n++;
    }
    closedir(d);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcmp(out[j - 1].name, out[j].name) > 0; j--) {
            Ent t = out[j]; out[j] = out[j - 1]; out[j - 1] = t;
        }
    return n;
}

static int count_files(const char *dir)
{
    static Ent e[256];
    int n = list(dir, e, 256), c = 0;
    for (int i = 0; i < n; i++) {
        if (e[i].is_dir) {
            char p[PATH_MAX_JP];
            snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
            Ent sub[64];
            int k = list(p, sub, 64);
            for (int j = 0; j < k; j++) c += !sub[j].is_dir;
        } else c++;
    }
    return c;
}

/* Copies the files of src into dst (one level of subfolders kept). 0 ok. */
static int copy_tree(const char *src, const char *dst, int depth)
{
    if (jk_fs_mkdir_p(dst) != 0) return -1;
    Ent *e = calloc(256, sizeof *e);
    if (!e) return -1;
    int n = list(src, e, 256), rc = 0;
    for (int i = 0; i < n && rc == 0; i++) {
        char s[PATH_MAX_JP], d[PATH_MAX_JP];
        snprintf(s, sizeof s, "%s/%s", src, e[i].name);
        snprintf(d, sizeof d, "%s/%s", dst, e[i].name);
        if (e[i].is_dir) rc = depth < 3 ? copy_tree(s, d, depth + 1) : 0;
        else if (strcmp(e[i].name, "backup.txt") != 0) rc = jk_fs_copy_checked(s, d, NULL);
    }
    free(e);
    return rc;
}

static int remove_tree(const char *path)
{
    if (!jk_fs_is_dir(path)) return jk_fs_remove(path);
    Ent *e = calloc(256, sizeof *e);
    if (!e) return -1;
    int n = list(path, e, 256), rc = 0;
    for (int i = 0; i < n; i++) {
        char p[PATH_MAX_JP];
        snprintf(p, sizeof p, "%s/%s", path, e[i].name);
        if (remove_tree(p) != 0) rc = -1;
    }
    free(e);
    if (rmdir(path) != 0) rc = -1;
    return rc;
}

/* Removes the files (and subfolders) inside dir, keeping dir. */
static int empty_dir(const char *dir)
{
    Ent *e = calloc(256, sizeof *e);
    if (!e) return -1;
    int n = list(dir, e, 256), rc = 0;
    for (int i = 0; i < n; i++) {
        char p[PATH_MAX_JP];
        snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
        if (remove_tree(p) != 0) rc = -1;
    }
    free(e);
    return rc;
}

/* ---------------------------------------------------------------- paths */

/* The game's Documents\KONAMI\Pro Evolution Soccer 2013 folder. */
static void p_game_docs(const JpApp *a, char *out, size_t cap)
{
    if (a->s->docs_dir && a->s->docs_dir[0]) snprintf(out, cap, "%s/" JKPRO_DOCS_SUB, a->s->docs_dir);
    else snprintf(out, cap, "%s/" JKPRO_DOCS_REL, a->s->prefix_dir);
}
static void p_live(const JpApp *a, char *out, size_t cap)
{
    char d[PATH_MAX_JP];
    p_game_docs(a, d, sizeof d);
    snprintf(out, cap, "%s/save", d);
}
static void p_sdat(const JpApp *a, char *out, size_t cap)
{
    char d[PATH_MAX_JP];
    p_game_docs(a, d, sizeof d);
    snprintf(out, cap, "%s/settings.dat", d);
}
static void p_backups(const JpApp *a, char *out, size_t cap)
{
    snprintf(out, cap, "%s/backup/saves/%s", a->s->system_dir, a->folder);
}
static void p_record(const JpApp *a, char *out, size_t cap)
{
    snprintf(out, cap, "%s/games/%s/patch.txt", a->s->system_dir, a->folder);
}
static void p_flag(const JpApp *a, const char *name, char *out, size_t cap)
{
    snprintf(out, cap, "%s/%s", a->s->system_dir, name);
}
static int have_game(const JpApp *a) { return a->m.game_tile >= 0; }
static JpTile *game_tile(JpApp *a) { return have_game(a) ? &a->m.tiles[a->m.game_tile] : NULL; }

/* ---------------------------------------------------------------- game definition */

static void builtin_def(JkGameDef *d)
{
    memset(d, 0, sizeof *d);
    cpy(d->id, sizeof d->id, "pes2013");
    cpy(d->name, sizeof d->name, "PES 2013");
    cpy(d->exe, sizeof d->exe, "pes2013.exe");
    cpy(d->markers[d->nmarkers++], sizeof d->markers[0], "settings.exe");
    cpy(d->markers[d->nmarkers++], sizeof d->markers[0], "img/dt01.img");
    cpy(d->version_kind, sizeof d->version_kind, "pe");
    cpy(d->foreign[d->nforeign++], sizeof d->foreign[0], "kitserver");
    cpy(d->foreign[d->nforeign++], sizeof d->foreign[0], "PESEDIT.exe");
    cpy(d->foreign[d->nforeign++], sizeof d->foreign[0], "pes2013_100.exe");
    /* provisional (kernel-panic case open): 1280x720 Medium; 1920x1080 is the target */
    /* lead G4: 1920x1080 is the default; 1280x720 stays, marked "not recommended" */
    jk_gamecfg_def_default(d, "resolution", "1920x1080", "1920x1080");
    jk_gamecfg_def_default(d, "quality", "high", "high");
    jk_gamecfg_def_default(d, "show_fps", "0", "0|1");
    jk_gamecfg_def_default(d, "kitserver", "on", "on|nokits|off");
    jk_gamecfg_def_default(d, "fservfix", "off", "on|off");
}


const char *jkpro_res_text(int res) { return res == 1 ? "1920 × 1080" : "1280 × 720"; }
const char *jkpro_quality_text(int q) { return q == 0 ? "Low" : q == 2 ? "High" : "Medium"; }

const char *jkpro_state_text(JpTileState st)
{
    switch (st) {
    case JP_ST_READY: return "Ready";
    case JP_ST_NEEDS_PATCH: return "PATCH needed";
    case JP_ST_PATCH_AGAIN: return "PATCH needed";
    default: return "Check game files";   /* owner: missing files and an unsupported exe are one state for the player */
    }
}

/* ---------------------------------------------------------------- Kitserver face fix (fserv.dll) */

static int (*ff_apply)(const char *) = jkpro_fservfix_apply;
static int (*ff_revert)(const char *) = jkpro_fservfix_revert;
static int (*ff_state)(const char *) = jkpro_fservfix_state;

void jkpro_fservfix_fns_for_tests(int (*apply)(const char *), int (*revert)(const char *), int (*state)(const char *))
{
    ff_apply = apply ? apply : jkpro_fservfix_apply;
    ff_revert = revert ? revert : jkpro_fservfix_revert;
    ff_state = state ? state : jkpro_fservfix_state;
}

static void kit_dir(const JpApp *a, char *out, size_t cap) { snprintf(out, cap, "%s/kitserver", a->s->game_dir); }

static int face_present(const JpApp *a)
{
    char p[PATH_MAX_JP];
    snprintf(p, sizeof p, "%s/kitserver/fserv.dll", a->s->game_dir);
    return jk_fs_exists(p) && !jk_fs_is_dir(p);
}

/* The saved choice on fserv.dll (on: apply, off: revert); returns the call's result. */
static int face_sync(JpApp *a, int on)
{
    char d[PATH_MAX_JP];
    kit_dir(a, d, sizeof d);
    int r = on ? ff_apply(d) : ff_revert(d);
    if (r < 0) jp_log(a, "JKPRO face fix: could not turn it %s, fserv.dll unchanged", on ? "on" : "off");
    else if (r == JKPRO_FSERVFIX_PATCHED) jp_log(a, "JKPRO face fix turned %s", on ? "on" : "off");
    else if (r == JKPRO_FSERVFIX_OTHER) jp_log(a, "JKPRO face fix: %s", "another fserv.dll version, left alone");
    return r;
}

/* ---------------------------------------------------------------- online account */

#define JKPRO_ACCOUNT "online-account"

static void account_line(FILE *f, char *out, size_t cap)
{
    if (!fgets(out, (int)cap, f)) { out[0] = 0; return; }
    size_t n = strlen(out);
    while (n && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = 0;
}

/* <system_dir>/online-account, else online-account.txt (Notepad's name), as the title reads it */
static void account_load(JpApp *a)
{
    JpModel *m = &a->m;
    m->online_id[0] = m->online_pw[0] = m->online_code[0] = 0;
    char p[PATH_MAX_JP];
    snprintf(p, sizeof p, "%s/" JKPRO_ACCOUNT, a->s->system_dir);
    FILE *f = fopen(p, "rb");
    if (!f) {
        snprintf(p, sizeof p, "%s/" JKPRO_ACCOUNT ".txt", a->s->system_dir);
        f = fopen(p, "rb");
    }
    if (!f) return;
    account_line(f, m->online_id, sizeof m->online_id);
    account_line(f, m->online_pw, sizeof m->online_pw);
    account_line(f, m->online_code, sizeof m->online_code);
    fclose(f);
}

/* Writes both lines to <system_dir>/online-account (and drops a .txt: one file);
 * both empty: no file. Logs lengths only. 0 ok, -1 failed. */
static int account_save(JpApp *a)
{
    JpModel *m = &a->m;
    char p[PATH_MAX_JP], t[PATH_MAX_JP], body[224], ln[80];
    snprintf(p, sizeof p, "%s/" JKPRO_ACCOUNT, a->s->system_dir);
    snprintf(t, sizeof t, "%s/" JKPRO_ACCOUNT ".txt", a->s->system_dir);
    int rc;
    if (!m->online_id[0] && !m->online_pw[0] && !m->online_code[0]) {
        rc = jk_fs_exists(p) && jk_fs_remove(p) != 0 ? -1 : 0;
    } else {
        int n = m->online_code[0] ? snprintf(body, sizeof body, "%s\n%s\n%s\n", m->online_id, m->online_pw, m->online_code)
                                  : snprintf(body, sizeof body, "%s\n%s\n", m->online_id, m->online_pw);
        rc = jk_fs_mkdir_p(a->s->system_dir) == 0 && jk_fs_write_atomic(p, body, (size_t)n) == 0 ? 0 : -1;
        memset(body, 0, sizeof body);
    }
    if (rc == 0 && jk_fs_exists(t)) jk_fs_remove(t);
    snprintf(ln, sizeof ln, "id %zu, password %zu, code %zu", strlen(m->online_id), strlen(m->online_pw),
             strlen(m->online_code));
    jp_log(a, rc == 0 ? "JKPRO online account saved (%s characters)" : "JKPRO online account NOT saved (%s characters)", ln);
    return rc;
}

static void online_toggle(JpApp *a)
{
    JpModel *m = &a->m;
    if (jkpro_online_set(a->s->system_dir, !m->online) == 0) {
        m->online = !m->online;
        jp_log(a, "JKPRO online %s", m->online ? "ThirteenServer" : "off");
    } else {
        jp_log(a, "JKPRO online: could not change %s", JKPRO_ONLINE_FLAG);
        cpy(m->online_msg, sizeof m->online_msg, "Not changed: JKPro could not write in its System folder.");
    }
}

/* ---------------------------------------------------------------- settings */

static void settings_load(JpApp *a)
{
    JpModel *m = &a->m;
    m->res = 1; m->quality = 2; m->show_fps = 0;
    m->kit_avail = 0; m->kit_mode = m->kit_set = JK_KIT_ON;
    m->face_avail = 0; m->face_set = 0; m->face_state = JKPRO_FSERVFIX_NONE;
    char p[PATH_MAX_JP];
    p_flag(a, "logs-on", p, sizeof p);
    m->logs = jk_fs_exists(p);
    m->online = jkpro_online_is_on(a->s->system_dir);
    account_load(a);
    if (!have_game(a)) return;
    JkGameDef d;
    builtin_def(&d);
    JkGamePaths gp;
    if (jk_game_paths(a->root, a->folder, &gp) != 0) return;
    static JkGameSettings s;
    jk_game_settings_load(&gp, &d, &s);
    /* owner (2026-11-25): locked to 1920x1080, 60 FPS, High, full screen; an
     * older settings.ini with other values is ignored */
    const char *f = jk_game_setting(&s, "show_fps");
    m->show_fps = f && !strcmp(f, "1");
    /* Kitserver (owner, plan C): the saved choice, shown as config.txt is now */
    m->kit_set = m->kit_mode = jkpro_kit_from_word(jk_game_setting(&s, "kitserver"));
    m->kit_avail = jkpro_kitcfg_present(a->s->game_dir);
    if (m->kit_avail) {
        int st = jkpro_kitcfg_read_state(a->s->game_dir, a->s->system_dir);
        if (st >= 0) m->kit_mode = st;
    }
    /* the face fix (owner, 2026-11-26): off by default, shown as fserv.dll is now */
    const char *ff = jk_game_setting(&s, "fservfix");
    m->face_set = ff && !strcmp(ff, "on");
    m->face_avail = face_present(a);
    if (m->face_avail) {
        char d[PATH_MAX_JP];
        kit_dir(a, d, sizeof d);
        m->face_state = ff_state(d);
    }
}

static int settings_save(JpApp *a)
{
    JpModel *m = &a->m;
    JkGameDef d;
    builtin_def(&d);
    JkGamePaths gp;
    if (jk_game_paths(a->root, a->folder, &gp) != 0) return -1;
    static JkGameSettings s;
    jk_game_settings_load(&gp, &d, &s);
    jk_game_settings_set(&s, "show_fps", m->show_fps ? "1" : "0");
    jk_game_settings_set(&s, "kitserver", jkpro_kit_word((JkKitMode)m->kit_set));
    jk_game_settings_set(&s, "fservfix", m->face_set ? "on" : "off");
    if (jk_fs_mkdir_p(gp.games_dir) != 0) return -1;
    return jk_game_settings_save(&gp, &s);
}

static JkPes13Settings settings_dat(const JpModel *m)
{
    JkPes13Settings s = { m->res ? 1920u : 1280u, m->res ? 1080u : 720u, 1, m->quality, 1, 1 };
    return s;
}

/* ---------------------------------------------------------------- the game's state */

/* The client PATCH installs as the game's d3d9.dll (lead, 2026-11-25): the
 * native client, or the PE fallback when <system_dir>/renderer-pe exists. */
static const char *client_name(const JpApp *a)
{
    char flag[PATH_MAX_JP];
    snprintf(flag, sizeof flag, "%s/renderer-pe", a->s->system_dir);
    return jk_fs_exists(flag) ? "d3d9-pe.dll" : "d3d9.dll";
}

/* 1 when dst is one of our two clients (switching between them keeps no backup). */
static int is_our_client(const JpApp *a, const char *dst)
{
    static const char *const NAMES[2] = { "d3d9.dll", "d3d9-pe.dll" };
    char b[65], c[65], src[PATH_MAX_JP];
    long long sb, sc;
    if (jk_fs_sha256(dst, b, &sb) != 0) return 0;
    for (int i = 0; i < 2; i++) {
        snprintf(src, sizeof src, "%s/%s", a->s->files_dir, NAMES[i]);
        if (jk_fs_sha256(src, c, &sc) == 0 && sb == sc && !strcmp(b, c)) return 1;
    }
    return 0;
}

static int our_d3d9_in_place(const JpApp *a, char sha_out[65])
{
    char src[PATH_MAX_JP], dst[PATH_MAX_JP], b[65];
    long long sa, sb;
    snprintf(src, sizeof src, "%s/%s", a->s->files_dir, client_name(a));
    snprintf(dst, sizeof dst, "%s/d3d9.dll", a->s->game_dir);
    if (jk_fs_sha256(src, sha_out, &sa) != 0) return 0;
    return jk_fs_sha256(dst, b, &sb) == 0 && sa == sb && !strcmp(sha_out, b);
}

static JpTileState patch_state(const JpApp *a)
{
    char rec[PATH_MAX_JP], sdat[PATH_MAX_JP], sha[65];
    p_record(a, rec, sizeof rec);
    p_sdat(a, sdat, sizeof sdat);
    if (!jk_fs_exists(rec)) return JP_ST_NEEDS_PATCH;
    if (!our_d3d9_in_place(a, sha) || !jk_fs_exists(sdat)) return JP_ST_PATCH_AGAIN;
    size_t len = 0;
    char *t = jk_fs_read(rec, 4096, &len);
    int same = t && strstr(t, sha) != NULL;
    free(t);
    return same ? JP_ST_READY : JP_ST_PATCH_AGAIN;
}

static void saves_refresh(JpApp *a);
static void dp_refresh(JpApp *a);

static void tiles_refresh(JpApp *a)
{
    JpModel *m = &a->m;
    int keep_folder_focus = m->ntiles > 0;
    char focused[64] = "";
    if (keep_folder_focus && m->tile_focus < m->ntiles) cpy(focused, sizeof focused, m->tiles[m->tile_focus].folder);
    m->ntiles = 0;
    m->game_tile = -1;
    m->version[0] = 0;
    m->missing_file[0] = 0;

    static JkGameDef defs[8];
    int nd = 0;
    char gdir[PATH_MAX_JP];
    snprintf(gdir, sizeof gdir, "%s/games", a->s->files_dir);
    nd = jk_games_load_defs(gdir, defs, 7);
    if (nd < 0) nd = 0;
    int has_pes13 = 0;
    for (int i = 0; i < nd; i++) has_pes13 |= !strcmp(defs[i].id, "pes2013");
    if (!has_pes13) builtin_def(&defs[nd++]);

    static JkGameFound found[JP_MAX_TILES];
    int nf = jk_games_scan(a->root, defs, nd, found, JP_MAX_TILES - 1);
    /* JKPro 1.0 is PES 2013 only (owner, 2026-11-25): only the game folder gets a
     * tile. Other folders are left alone and not shown; a later release lists its
     * games here, each with its own definition and PATCH. */
    for (int i = 0; i < nf && m->ntiles < JP_MAX_TILES; i++) {
        if (!a->folder[0] || strcasecmp(found[i].folder, a->folder) != 0) continue;
        JpTile *t = &m->tiles[m->ntiles];
        memset(t, 0, sizeof *t);
        cpy(t->folder, sizeof t->folder, found[i].folder);
        const char *title = found[i].folder;
        for (int k = 0; k < nd; k++)
            if (found[i].game_id[0] && !strcmp(defs[k].id, found[i].game_id)) title = defs[k].name;
        cpy(t->title, sizeof t->title, title);
        t->modded = found[i].verdict == JK_GV_MODDED;
        m->game_tile = m->ntiles;
        cpy(m->version, sizeof m->version, found[i].version);
        if (strcmp(found[i].game_id, "pes2013") != 0) {
            t->state = JP_ST_MISSING;
            cpy(t->title, sizeof t->title, "PES 2013");
            cpy(m->missing_file, sizeof m->missing_file, found[i].missing[0] ? found[i].missing : "pes2013.exe");
        } else if (found[i].verdict == JK_GV_MISSING) {
            t->state = JP_ST_MISSING;
            cpy(m->missing_file, sizeof m->missing_file, found[i].missing[0] ? found[i].missing : "a game file");
        }
        else if (found[i].verdict == JK_GV_NEEDS_DISC) t->state = JP_ST_UNSUPPORTED;
        else t->state = JP_ST_NEEDS_PATCH;   /* set below */
        m->ntiles++;
    }

    JpTile *g = game_tile(a);
    if (g && g->state == JP_ST_NEEDS_PATCH) g->state = patch_state(a);

    m->tile_focus = have_game(a) ? m->game_tile : 0;
    if (focused[0])
        for (int i = 0; i < m->ntiles; i++)
            if (!strcmp(m->tiles[i].folder, focused)) m->tile_focus = i;
}

void jkpro_app_refresh(JpApp *a)
{
    tiles_refresh(a);
    saves_refresh(a);
}

/* ---------------------------------------------------------------- dialogs */

static void dialog(JpApp *a, JpDialog kind, const char *code, const char *title, const char *body, int nothing)
{
    JpModel *m = &a->m;
    m->dialog = kind;
    cpy(m->dlg_code, sizeof m->dlg_code, code);
    cpy(m->dlg_title, sizeof m->dlg_title, title);
    cpy(m->dlg_body, sizeof m->dlg_body, body);
    m->dlg_nothing_changed = nothing;
    if (code && code[0]) {
        char line[600];
        snprintf(line, sizeof line, "JKPRO %s %s: %s", code, title, body);
        if (a->s->log) a->s->log(a->s->ctx, line);
    }
}

/* ---------------------------------------------------------------- drawing hook */

static void show(JpApp *a)
{
    if (!a->frame || !a->s->present) return;
    jkpro_draw(a->frame, &a->m);
    a->s->present(a->s->ctx, a->frame);
}

/* ---------------------------------------------------------------- the Windows registry */

/* Wine's registry save is not crash-safe (lead, 2026-11-25): a console panic
 * can leave system.reg, user.reg or userdef.reg at 0 bytes, and the game then
 * no longer starts. JKPro keeps the last good copy of each in
 * <system_dir>/backup/registry/ and puts it back before a start. */
static const char *const REG_FILES[3] = { "system.reg", "user.reg", "userdef.reg" };

static int reg_valid(const char *path)
{
    static const char HEAD[] = "WINE REGISTRY Version 2";
    char head[sizeof HEAD];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t len = fread(head, 1, sizeof HEAD - 1, f);
    fclose(f);
    return len == sizeof HEAD - 1 && !memcmp(head, HEAD, sizeof HEAD - 1);
}

/* Puts a damaged registry file back from the last good copy. *restored counts
 * the files put back. Returns the number of damaged files that could not be
 * put back (0: the registry is fine now). A file that does not exist is left
 * to the title (it seeds the prefix); only a damaged one counts. */
static int registry_check(JpApp *a, int *restored)
{
    int bad = 0;
    *restored = 0;
    for (int i = 0; i < 3; i++) {
        char live[PATH_MAX_JP], keep[PATH_MAX_JP];
        snprintf(live, sizeof live, "%s/%s", a->s->prefix_dir, REG_FILES[i]);
        snprintf(keep, sizeof keep, "%s/backup/registry/%s", a->s->system_dir, REG_FILES[i]);
        if (!jk_fs_exists(live) || reg_valid(live)) continue;
        if (reg_valid(keep) && jk_fs_copy_checked(keep, live, NULL) == 0 && reg_valid(live)) {
            (*restored)++;
            jp_log(a, "JKPRO E610 registry restored: %s", REG_FILES[i]);
        } else {
            bad++;
            jp_log(a, "JKPRO E611 registry damaged, no good copy: %s", REG_FILES[i]);
        }
    }
    return bad;
}

/* Keeps the current registry as the last good copy (only files that are whole). */
static void registry_keep(JpApp *a)
{
    char dir[PATH_MAX_JP];
    snprintf(dir, sizeof dir, "%s/backup/registry", a->s->system_dir);
    if (jk_fs_mkdir_p(dir) != 0) return;
    for (int i = 0; i < 3; i++) {
        char live[PATH_MAX_JP], keep[PATH_MAX_JP];
        snprintf(live, sizeof live, "%s/%s", a->s->prefix_dir, REG_FILES[i]);
        snprintf(keep, sizeof keep, "%s/%s", dir, REG_FILES[i]);
        if (reg_valid(live)) jk_fs_copy_checked(live, keep, NULL);
    }
}

static void registry_damaged_dialog(JpApp *a)
{
    dialog(a, JP_DLG_ERROR, "E611", "Windows settings damaged",
           "A crash damaged the Windows settings PES 2013 needs, and there is no good copy to put back. Close JKPro and "
           "open it again so the Windows layer is set up, then run PATCH. Your saves are not affected.", 1);
}

/* "Check game files" (owner): one tile state, the dialog says exactly what is wrong.
 * Returns 202 (a file is missing) or 205 (an unsupported pes2013.exe). */
static int check_files_text(const JpApp *a, char *out, size_t cap)
{
    const JpTile *g = a->m.game_tile >= 0 ? &a->m.tiles[a->m.game_tile] : NULL;
    if (g && g->state == JP_ST_UNSUPPORTED) {
        snprintf(out, cap, "PES 2013 can't start: this pes2013.exe version is not supported. Use the PC version 1.04 exe.");
        return 205;
    }
    snprintf(out, cap, "PES 2013 can't start: %s is missing in %s. Copy the whole PC game folder again.",
             a->m.missing_file[0] ? a->m.missing_file : "a game file", a->s->game_dir);
    return 202;
}

/* ---------------------------------------------------------------- PATCH */

static int reg_names_pes13(const char *path)
{
    size_t len = 0;
    char *t = jk_fs_read(path, 1 << 20, &len);
    if (!t) return 0;
    int hit = 0;
    static const char U16[] = "P\0E\0S\0" "2\0" "0\0" "1\0" "3\0";
    for (size_t i = 0; i + 7 <= len && !hit; i++)
        if (!memcmp(t + i, "PES2013", 7) || (i + 14 <= len && !memcmp(t + i, U16, 14))) hit = 1;
    free(t);
    return hit;
}

/* The first *.reg naming KONAMI\PES2013 in dir (top level only). */
static int find_reg_in(const char *dir, char *out, size_t cap)
{
    if (!dir || !dir[0]) return 0;
    Ent *e = calloc(128, sizeof *e);
    if (!e) return 0;
    int n = list(dir, e, 128), hit = 0;
    for (int i = 0; i < n && !hit; i++) {
        size_t l = strlen(e[i].name);
        if (e[i].is_dir || l < 5 || strcasecmp(e[i].name + l - 4, ".reg")) continue;
        char p[PATH_MAX_JP];
        snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
        if (reg_names_pes13(p)) { cpy(out, cap, p); hit = 1; }
    }
    free(e);
    return hit;
}

/* Lead Q1: the game folder, then the JKPro root, then USB JKPro/. */
static int find_reg(const JpApp *a, char *out, size_t cap)
{
    char usb[PATH_MAX_JP] = "";
    if (a->s->usb_dir && a->s->usb_dir[0]) snprintf(usb, sizeof usb, "%s/JKPro", a->s->usb_dir);
    return find_reg_in(a->s->game_dir, out, cap) || find_reg_in(a->root, out, cap) || find_reg_in(usb, out, cap);
}

static void step(JpApp *a, int i, JpStepState st)
{
    a->m.steps[i] = st;
    if (st == JP_SS_NOW) a->m.step_now = i;
    show(a);
}

static void patch_fail(JpApp *a, int i, const char *code, const char *title, const char *body, int nothing)
{
    step(a, i, JP_SS_FAIL);
    a->m.phase = JP_PH_FAILED;
    a->m.play_after_patch = 0;
    dialog(a, JP_DLG_ERROR, code, title, body, nothing);
}

static int write_settings_dat(JpApp *a)
{
    char p[PATH_MAX_JP];
    p_sdat(a, p, sizeof p);
    JkPes13Settings s = settings_dat(&a->m);
    return jk_pes13set_write(p, &s);
}

static void patch_run(JpApp *a)
{
    JpModel *m = &a->m;
    char body[400];
    for (int i = 0; i < JP_STEP_COUNT; i++) m->steps[i] = JP_SS_WAIT;
    m->phase = JP_PH_RUNNING;
    m->bytes_done = m->bytes_total = 0;
    m->reg_used[0] = 0;
    m->screen = JP_SCR_PATCH;
    jp_log(a, "JKPRO patch start %s", a->s->game_dir);

    /* 1 check the game folder */
    step(a, JP_STEP_CHECK, JP_SS_NOW);
    tiles_refresh(a);
    JpTile *g = game_tile(a);
    if (!g) {
        snprintf(body, sizeof body, "Put your PES 2013 folder (with pes2013.exe) in %s, for example %s/PES13.",
                 a->root, a->root);
        patch_fail(a, JP_STEP_CHECK, "E201", "PES 2013 not found", body, 1);
        return;
    }
    if (g->state == JP_ST_MISSING || g->state == JP_ST_UNSUPPORTED) {
        int code = check_files_text(a, body, sizeof body);
        patch_fail(a, JP_STEP_CHECK, code == 202 ? "E202" : "E205", "Check game files", body, 1);
        return;
    }
    step(a, JP_STEP_CHECK, JP_SS_OK);

    /* 2 the Windows layer (the title seeds it before the UI) */
    step(a, JP_STEP_WINDOWS, JP_SS_NOW);
    char sysreg[PATH_MAX_JP];
    snprintf(sysreg, sizeof sysreg, "%s/system.reg", a->s->prefix_dir);
    int fixed = 0;
    if (registry_check(a, &fixed) > 0 || !reg_valid(sysreg)) {
        patch_fail(a, JP_STEP_WINDOWS, jk_fs_exists(sysreg) ? "E611" : "E301", "PATCH stopped at step 2",
                   "The Windows layer is not ready or was damaged by a crash. Close JKPro and open it again; it is set "
                   "up at the start.", 1);
        return;
    }
    step(a, JP_STEP_WINDOWS, JP_SS_OK);

    /* 3 our d3d9.dll into the game folder (anything else is kept first) */
    step(a, JP_STEP_FILES, JP_SS_NOW);
    char src[PATH_MAX_JP], dst[PATH_MAX_JP], sha[65];
    const char *client = client_name(a);
    snprintf(src, sizeof src, "%s/%s", a->s->files_dir, client);
    snprintf(dst, sizeof dst, "%s/d3d9.dll", a->s->game_dir);
    long long size = jk_fs_size(src);
    if (size <= 0) {
        snprintf(body, sizeof body, "The JKPro package is missing %s. Reinstall the JKPro package.", client);
        patch_fail(a, JP_STEP_FILES, "E301", "PATCH stopped at step 3", body, 1);
        return;
    }
    m->bytes_total = size;
    if (!our_d3d9_in_place(a, sha)) {
        if (jk_fs_exists(dst) && !is_our_client(a, dst)) {
            char st[48], bdir[PATH_MAX_JP], parent[PATH_MAX_JP], keep[PATH_MAX_JP];
            stamp(a, st, sizeof st);
            snprintf(parent, sizeof parent, "%s/backup/%s", a->s->system_dir, a->folder);
            if (unique_dir(parent, st, bdir, sizeof bdir) != 0 || jk_fs_mkdir_p(bdir) != 0) {
                patch_fail(a, JP_STEP_FILES, "E301", "PATCH stopped at step 3",
                           "JKPro could not make a backup of the d3d9.dll in your game folder.", 1);
                return;
            }
            snprintf(keep, sizeof keep, "%s/d3d9.dll", bdir);
            if (jk_fs_copy_checked(dst, keep, NULL) != 0) {
                patch_fail(a, JP_STEP_FILES, "E301", "PATCH stopped at step 3",
                           "JKPro could not make a backup of the d3d9.dll in your game folder.", 1);
                return;
            }
        }
        if (jk_fs_copy_checked(src, dst, sha) != 0) {
            patch_fail(a, JP_STEP_FILES, "E301", "PATCH stopped at step 3",
                       "JKPro could not copy d3d9.dll into your game folder. Check that the PS4 has free space.", 1);
            return;
        }
    }
    m->bytes_done = size;
    step(a, JP_STEP_FILES, JP_SS_OK);

    /* 4 the player's .reg */
    step(a, JP_STEP_REGISTRY, JP_SS_NOW);
    char reg[PATH_MAX_JP];
    if (!find_reg(a, reg, sizeof reg)) {
        snprintf(body, sizeof body,
                 "PES 2013 needs the registry file (.reg) from your PC. Without it, the game says it has not been "
                 "installed. Put the .reg file in %s/ and press PATCH again. Steps 1-3 are done and stay done.",
                 a->s->game_dir);
        patch_fail(a, JP_STEP_REGISTRY, "E210", "Registry file needed", body, 0);
        return;
    }
    cpy(m->reg_used, sizeof m->reg_used, base_name(reg));
    char win[PATH_MAX_JP], missing[32] = "";
    snprintf(win, sizeof win, "Z:%s\\", a->s->game_dir);
    for (char *c = win; *c; c++) if (*c == '/') *c = '\\';
    int nreg = jk_regimport(reg, sysreg, REG_KEY, win, "code", missing, sizeof missing);
    if (nreg < 0) {
        patch_fail(a, JP_STEP_REGISTRY, "E301", "PATCH stopped at step 4",
                   "JKPro could not write the Windows registry. Close JKPro and open it again.", 0);
        return;
    }
    if (missing[0]) {
        snprintf(body, sizeof body,
                 "The registry file %s has no \"code\" value, so PES 2013 would say it has not been installed. Use the "
                 ".reg file from your own PC install and press PATCH again.", m->reg_used);
        patch_fail(a, JP_STEP_REGISTRY, "E211", "The .reg file has no code", body, 0);
        return;
    }
    jp_log(a, "JKPRO patch reg %s", m->reg_used);
    step(a, JP_STEP_REGISTRY, JP_SS_OK);

    /* 5 settings.dat */
    step(a, JP_STEP_SETTINGS, JP_SS_NOW);
    if (write_settings_dat(a) != 0) {
        patch_fail(a, JP_STEP_SETTINGS, "E501", "Settings not written",
                   "JKPro could not write the game's settings file. Check that the PS4 has free space, then press PATCH again.",
                   0);
        return;
    }
    step(a, JP_STEP_SETTINGS, JP_SS_OK);

    /* record */
    step(a, JP_STEP_RECORD, JP_SS_NOW);
    char rec[PATH_MAX_JP], text[300];
    p_record(a, rec, sizeof rec);
    snprintf(text, sizeof text, "d3d9 %s\nreg %s\n", sha, m->reg_used);
    if (jk_fs_mkdir_parent(rec) != 0 || jk_fs_write_atomic(rec, text, strlen(text)) != 0) {
        patch_fail(a, JP_STEP_RECORD, "E301", "PATCH stopped at step 6",
                   "JKPro could not save that PATCH is done. Check that the PS4 has free space.", 0);
        return;
    }
    step(a, JP_STEP_RECORD, JP_SS_OK);
    registry_keep(a);
    m->phase = JP_PH_DONE;
    tiles_refresh(a);
    jp_log(a, "JKPRO patch done %s", a->s->game_dir);
}

/* ---------------------------------------------------------------- saves */

static int backups_list(const JpApp *a, Ent *e, int max)
{
    char dir[PATH_MAX_JP];
    p_backups(a, dir, sizeof dir);
    int n = list(dir, e, max), k = 0;
    for (int i = 0; i < n; i++) if (e[i].is_dir) e[k++] = e[i];
    return k;
}

/* "402 KB", "7.9 MB", "512 B" (integers only: no libc locale). */
static void human_size(long long n, char *out, size_t cap)
{
    if (n < 1024) snprintf(out, cap, "%lld B", n);
    else if (n < 1024 * 1024) snprintf(out, cap, "%lld KB", (n + 512) / 1024);
    else {
        long long t = (n * 10 + 524288) / 1048576;
        snprintf(out, cap, "%lld.%lld MB", t / 10, t % 10);
    }
}

/* One line per save file (top level and one subfolder; backup.txt left out):
 * "<name> <size> <sha256>\n", sorted. With sums 0 the sha is left out. */
static void saves_digest(const char *dir, char *out, size_t cap, int sums)
{
    out[0] = 0;
    Ent *e = calloc(128, sizeof *e);
    if (!e) return;
    int n = list(dir, e, 128);
    for (int i = 0; i < n; i++) {
        char p[PATH_MAX_JP];
        snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
        int k = 1;
        Ent sub[32];
        const Ent *items = &e[i];
        char prefix[140] = "";
        if (e[i].is_dir) {
            k = list(p, sub, 32);
            items = sub;
            snprintf(prefix, sizeof prefix, "%s/", e[i].name);
        }
        for (int j = 0; j < k; j++) {
            if (items[j].is_dir || !strcmp(items[j].name, "backup.txt")) continue;
            char f[PATH_MAX_JP], sha[65] = "-";
            long long size = 0;
            if (e[i].is_dir) snprintf(f, sizeof f, "%s/%s", p, items[j].name);
            else cpy(f, sizeof f, p);
            if (sums) jk_fs_sha256(f, sha, &size);
            else size = jk_fs_size(f);
            size_t l = strlen(out);
            snprintf(out + l, cap - l, "%s%s %lld %s\n", prefix, items[j].name, size, sums ? sha : "-");
        }
    }
    free(e);
}

/* "OPTION.bin 402 KB · EDIT.bin 1.2 MB" from a digest. */
static void digest_summary(const char *dg, char *out, size_t cap)
{
    out[0] = 0;
    int shown = 0, total = 0;
    const char *p = dg;
    while (*p) {
        char name[160] = "";
        long long size = 0;
        const char *eol = strchr(p, '\n');
        if (sscanf(p, "%159s %lld", name, &size) == 2) {
            total++;
            if (shown < 3) {
                char hs[32];
                human_size(size, hs, sizeof hs);
                size_t l = strlen(out);
                snprintf(out + l, cap - l, "%s%s \xC2\xB7 %s", shown ? "     " : "", name, hs);
                shown++;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    if (total > shown) {
        size_t l = strlen(out);
        snprintf(out + l, cap - l, "     and %d more", total - shown);
    }
}

/* The reason written in a backup's backup.txt; "" for an older backup without one. */
static void backup_reason(const char *bdir, char *out, size_t cap)
{
    char p[PATH_MAX_JP];
    snprintf(p, sizeof p, "%s/backup.txt", bdir);
    out[0] = 0;
    size_t len = 0;
    char *t = jk_fs_read(p, 1 << 16, &len);
    if (!t) return;
    char *r = strstr(t, "reason ");
    if (r) {
        r += 7;
        size_t k = strcspn(r, "\r\n");
        snprintf(out, cap, "%.*s", (int)k, r);
    }
    free(t);
}

/* Keeps the newest JKPRO_BACKUPS_KEPT backups (owner: 10, whatever made them);
 * never removes keep (the backup being restored). */
static void prune_backups(JpApp *a, const char *keep)
{
    char dir[PATH_MAX_JP];
    p_backups(a, dir, sizeof dir);
    Ent *e = calloc(256, sizeof *e);
    if (!e) return;
    int n = backups_list(a, e, 256), kept = 0;
    for (int i = n - 1; i >= 0; i--) {                  /* newest first */
        if (keep && !strcmp(keep, e[i].name)) continue;
        if (++kept > JKPRO_BACKUPS_KEPT) {
            char b[PATH_MAX_JP];
            snprintf(b, sizeof b, "%s/%s", dir, e[i].name);
            remove_tree(b);
        }
    }
    free(e);
}

/* Takes a backup of the live saves, with a backup.txt (reason, time, files and
 * sizes). Before a match, a backup is taken only when the saves changed since
 * the newest backup, so repeated PLAY never churns the history. 1 taken, 0
 * nothing to back up (or unchanged), -1 failed. */
static int backup_saves(JpApp *a, const char *reason, const char *keep, char *name_out, size_t cap)
{
    char live[PATH_MAX_JP], dir[PATH_MAX_JP], st[48], dst[PATH_MAX_JP];
    p_live(a, live, sizeof live);
    if (count_files(live) == 0) return 0;
    p_backups(a, dir, sizeof dir);
    static char now_dg[8192], old_dg[8192];
    saves_digest(live, now_dg, sizeof now_dg, 1);
    if (!strcmp(reason, "before match")) {
        Ent *e = calloc(256, sizeof *e);
        int n = e ? backups_list(a, e, 256) : 0;
        if (n > 0) {
            char newest[PATH_MAX_JP];
            snprintf(newest, sizeof newest, "%s/%s", dir, e[n - 1].name);
            saves_digest(newest, old_dg, sizeof old_dg, 1);
            if (!strcmp(now_dg, old_dg)) { free(e); return 0; }       /* unchanged: the newest backup holds it */
        }
        free(e);
    }
    stamp(a, st, sizeof st);
    if (jk_fs_mkdir_p(dir) != 0 || unique_dir(dir, st, dst, sizeof dst) != 0) return -1;
    if (copy_tree(live, dst, 0) != 0) { remove_tree(dst); return -1; }
    /* backup.txt: readable over FTP too */
    static char txt[9000];
    snprintf(txt, sizeof txt, "JKPro save backup\nreason %s\ntime %s\ngame folder %s\nfiles (name, bytes, sha256):\n%s",
             reason, st, a->folder, now_dg);
    char bt[PATH_MAX_JP];
    snprintf(bt, sizeof bt, "%s/backup.txt", dst);
    jk_fs_write_atomic(bt, txt, strlen(txt));
    if (name_out) cpy(name_out, cap, base_name(dst));
    prune_backups(a, keep);
    return 1;
}

static char restore_target[128];      /* the backup a Restore dialog names */

typedef struct { char path[PATH_MAX_JP]; char name[128]; int usb; } ImportFile;

/* The files waiting to be imported: the drop folder, its save/ subfolder, then USB JKPro/saves. */
static int import_scan(const JpApp *a, ImportFile *out, int max)
{
    int n = 0;
    char roots[2][PATH_MAX_JP] = { "", "" };
    cpy(roots[0], sizeof roots[0], a->s->saves_drop);
    if (a->s->usb_dir && a->s->usb_dir[0]) snprintf(roots[1], sizeof roots[1], "%s/JKPro/saves", a->s->usb_dir);
    for (int r = 0; r < 2; r++) {
        if (!roots[r][0]) continue;
        for (int sub = 0; sub < 2; sub++) {
            char dir[PATH_MAX_JP];
            if (sub) snprintf(dir, sizeof dir, "%s/save", roots[r]);
            else cpy(dir, sizeof dir, roots[r]);
            Ent e[64];
            int k = list(dir, e, 64);
            for (int i = 0; i < k && n < max; i++) {
                if (e[i].is_dir) continue;
                snprintf(out[n].path, sizeof out[n].path, "%s/%s", dir, e[i].name);
                /* data packs (dt*.img, AFS) are not saves: DATA PACKS takes them */
                size_t l = strlen(e[i].name);
                if ((l > 4 && !strcasecmp(e[i].name + l - 4, ".img")) || jkpro_pack_magic(out[n].path)) continue;
                cpy(out[n].name, sizeof out[n].name, e[i].name);
                out[n].usb = r == 1;
                n++;
            }
        }
    }
    return n;
}

static void saves_refresh(JpApp *a)
{
    JpModel *m = &a->m;
    char live[PATH_MAX_JP];
    p_live(a, live, sizeof live);
    m->live_files = count_files(live);
    m->usb = a->s->usb_dir && a->s->usb_dir[0] && jk_fs_is_dir(a->s->usb_dir);
    static ImportFile imp[JP_IMPORT_MAX];
    int n = import_scan(a, imp, JP_IMPORT_MAX);
    m->import_files = n;
    m->import_usb = 0;
    m->import_list[0] = 0;
    for (int i = 0; i < n; i++) {
        m->import_usb |= imp[i].usb;
        if (i < 3) {
            size_t l = strlen(m->import_list);
            snprintf(m->import_list + l, sizeof m->import_list - l, "%s%s", i ? " · " : "", imp[i].name);
        }
    }
    if (n > 3) {
        size_t l = strlen(m->import_list);
        snprintf(m->import_list + l, sizeof m->import_list - l, " · %d more", n - 3);
    }
    Ent *e = calloc(256, sizeof *e);
    m->backups = 0;
    m->last_backup[0] = 0;
    m->nbk = 0;
    if (e) {
        m->backups = backups_list(a, e, 256);
        if (m->backups) cpy(m->last_backup, sizeof m->last_backup, e[m->backups - 1].name);
        char dir[PATH_MAX_JP];
        p_backups(a, dir, sizeof dir);
        static char dg[8192];
        for (int i = m->backups - 1; i >= 0 && m->nbk < JKPRO_BACKUPS_LIST; i--) {   /* newest first */
            char b[PATH_MAX_JP], why[24];
            snprintf(b, sizeof b, "%s/%s", dir, e[i].name);
            cpy(m->bk[m->nbk].name, sizeof m->bk[0].name, e[i].name);
            backup_reason(b, why, sizeof why);
            cpy(m->bk[m->nbk].reason, sizeof m->bk[0].reason, why[0] ? why : "older backup");
            saves_digest(b, dg, sizeof dg, 0);
            digest_summary(dg, m->bk[m->nbk].files, sizeof m->bk[0].files);
            m->nbk++;
        }
        free(e);
    }
    if (m->bk_focus >= m->nbk) m->bk_focus = m->nbk ? m->nbk - 1 : 0;
    /* the real paths, in the console's own letter case */
    char bak[PATH_MAX_JP];
    p_backups(a, bak, sizeof bak);
    snprintf(m->sav_drop, sizeof m->sav_drop, "%s/", a->s->saves_drop);
    snprintf(m->sav_live, sizeof m->sav_live, "%s/", live);
    snprintf(m->sav_bak, sizeof m->sav_bak, "%s/", bak);
    /* older versions left Import/done-<stamp>/ folders: remove the empty ones, name the rest */
    m->import_old[0] = 0;
    {
        Ent d[64];
        int k = list(a->s->saves_drop, d, 64);
        for (int i = 0; i < k; i++) {
            if (!d[i].is_dir || strncmp(d[i].name, "done-", 5)) continue;
            char p[PATH_MAX_JP];
            snprintf(p, sizeof p, "%s/%s", a->s->saves_drop, d[i].name);
            if (rmdir(p) != 0 && !m->import_old[0])
                snprintf(m->import_old, sizeof m->import_old,
                         "Old import copies are in Import/%s/: you can delete them (the files first, then the folder).",
                         d[i].name);
        }
    }
    /* PES 2013 PC (lead, rc6): the option file (teams, names, kits) is EDIT.bin; OPTION.bin
     * holds the game settings (about 400 KB). Hints only: every file is still copied
     * with its own name. */
    m->import_warn[0] = 0;
    for (int i = 0; i < n && !m->import_warn[0]; i++) {
        size_t l = strlen(imp[i].name);
        if (!strcasecmp(imp[i].name, "OPTION.bin") && jk_fs_size(imp[i].path) > 2 * 1024 * 1024) {
            char hs[32];
            human_size(jk_fs_size(imp[i].path), hs, sizeof hs);
            snprintf(m->import_warn, sizeof m->import_warn,
                     "OPTION.bin (%s) looks like an option file: name it EDIT.bin. OPTION.bin only holds the game "
                     "settings.", hs);
        } else if (l > 4 && !strcasecmp(imp[i].name + l - 4, ".bin") && strcasecmp(imp[i].name, "OPTION.bin") &&
                   strcasecmp(imp[i].name, "EDIT.bin")) {
            snprintf(m->import_warn, sizeof m->import_warn,
                     "%s: PES 2013 reads EDIT.bin (option file) and OPTION.bin (settings). Rename yours to one of "
                     "these (it is copied with its own name).", imp[i].name);
        }
    }
}

static void sav_msg(JpApp *a, int bad, const char *text)
{
    a->m.sav_msg_bad = bad;
    cpy(a->m.sav_msg, sizeof a->m.sav_msg, text);
}

static void saves_load(JpApp *a)
{
    JpModel *m = &a->m;
    static ImportFile imp[JP_IMPORT_MAX];
    int n = import_scan(a, imp, JP_IMPORT_MAX);
    char taken[48] = "", live[PATH_MAX_JP], body[400];
    p_live(a, live, sizeof live);
    int b = backup_saves(a, "before import", NULL, taken, sizeof taken);
    if (b < 0) {
        dialog(a, JP_DLG_ERROR, "E402", "Saves not loaded",
               "JKPro could not back up your current saves, so it loaded nothing. Check that the PS4 has free space.", 1);
        return;
    }
    if (jk_fs_mkdir_p(live) != 0) {
        dialog(a, JP_DLG_ERROR, "E402", "Saves not loaded", "JKPro could not open the game's save folder.", 1);
        return;
    }
    for (int i = 0; i < n; i++) {
        char dst[PATH_MAX_JP];
        snprintf(dst, sizeof dst, "%s/%s", live, imp[i].name);
        if (jk_fs_copy_checked(imp[i].path, dst, NULL) != 0) {
            snprintf(body, sizeof body,
                     "JKPro could not copy %s. Your previous saves are in the backup %s: restore it under Restore a backup.",
                     imp[i].name, taken[0] ? taken : "(none: there were no saves)");
            dialog(a, JP_DLG_ERROR, "E402", "Saves not loaded", body, 0);
            saves_refresh(a);
            return;
        }
    }
    /* W1 (owner, rc6): the console's FTP cannot remove a folder that is not empty, so
     * Import keeps no done-<stamp>/ copies. Every file was copied and checked (sha256)
     * into the save folder, and the backup taken first holds the saves from before:
     * the dropped files are deleted, and Import is empty again. USB stays as it is. */
    for (int i = 0; i < n; i++)
        if (!imp[i].usb) jk_fs_remove(imp[i].path);
    char sub[PATH_MAX_JP];
    snprintf(sub, sizeof sub, "%s/save", a->s->saves_drop);
    rmdir(sub);
    char names[200] = "";
    for (int i = 0; i < n; i++) {
        size_t l = strlen(names);
        if (i < 4) snprintf(names + l, sizeof names - l, "%s%s", i ? ", " : "", imp[i].name);
        else { snprintf(names + l, sizeof names - l, " and %d more", n - 4); break; }
    }
    char warn[200];
    cpy(warn, sizeof warn, m->import_warn);
    int local = 0;
    for (int i = 0; i < n; i++) local |= !imp[i].usb;
    char moved[120] = "";
    if (local) snprintf(moved, sizeof moved, " Import is empty again.");
    if (taken[0]) snprintf(body, sizeof body, "Loaded %s.%s Backup of the saves you had: %s.%s%s", names, moved, taken,
                           warn[0] ? " " : "", warn);
    else snprintf(body, sizeof body, "Loaded %s.%s There were no saves to back up.%s%s", names, moved, warn[0] ? " " : "",
                  warn);
    saves_refresh(a);
    sav_msg(a, 0, body);
    (void)m;
    jp_log(a, "JKPRO saves loaded %s", taken);
}


static void saves_restore(JpApp *a)
{
    char dir[PATH_MAX_JP], src[PATH_MAX_JP], live[PATH_MAX_JP], body[400];
    p_backups(a, dir, sizeof dir);
    snprintf(src, sizeof src, "%s/%s", dir, restore_target);
    p_live(a, live, sizeof live);
    if (!restore_target[0] || !jk_fs_is_dir(src)) {
        dialog(a, JP_DLG_ERROR, "E402", "Backup not restored", "That backup is gone.", 1);
        return;
    }
    char taken[48] = "";
    if (backup_saves(a, "before restore", restore_target, taken, sizeof taken) < 0) {
        dialog(a, JP_DLG_ERROR, "E402", "Backup not restored",
               "JKPro could not back up your current saves first, so it restored nothing.", 1);
        return;
    }
    /* the backup being restored is never pruned by that new backup (keep) */
    if (jk_fs_mkdir_p(live) != 0 || empty_dir(live) != 0 || copy_tree(src, live, 0) != 0) {
        snprintf(body, sizeof body, "The restore stopped. Your saves from just before are in the backup %s.",
                 taken[0] ? taken : "-");
        dialog(a, JP_DLG_ERROR, "E402", "Backup not restored", body, 0);
        saves_refresh(a);
        return;
    }
    saves_refresh(a);
    a->m.screen = JP_SCR_SAVES;
    if (taken[0]) snprintf(body, sizeof body, "Restored the backup from %s. The saves you had are in the backup %s "
                           "(before restore).", restore_target, taken);
    else snprintf(body, sizeof body, "Restored the backup from %s.", restore_target);
    sav_msg(a, 0, body);
}

static void saves_export(JpApp *a)
{
    char live[PATH_MAX_JP], parent[PATH_MAX_JP], st[48], name[128], dst[PATH_MAX_JP], body[400];
    p_live(a, live, sizeof live);
    if (count_files(live) == 0) {
        dialog(a, JP_DLG_INFO, "", "No saves yet", "PES 2013 has no saves on this PS4 yet. Play once, then export.", 1);
        return;
    }
    if (a->m.usb) snprintf(parent, sizeof parent, "%s/JKPro/export", a->s->usb_dir);
    else snprintf(parent, sizeof parent, "%s/Export", a->root);
    stamp(a, st, sizeof st);
    snprintf(name, sizeof name, "%s-%s", a->folder, st);
    if (jk_fs_mkdir_p(parent) != 0 || unique_dir(parent, name, dst, sizeof dst) != 0 || copy_tree(live, dst, 0) != 0) {
        snprintf(body, sizeof body, "JKPro could not write to %s.", parent);
        sav_msg(a, 1, body);
        return;
    }
    snprintf(body, sizeof body, "Exported to %s", dst);
    sav_msg(a, 0, body);
}

/* ---------------------------------------------------------------- PLAY */

static void play_now(JpApp *a, int with_backup)
{
    JpModel *m = &a->m;
    int fixed = 0;
    if (registry_check(a, &fixed) > 0) {
        registry_damaged_dialog(a);
        return;
    }
    registry_keep(a);                    /* the registry is whole now: it is the good copy */
    if (write_settings_dat(a) != 0) {
        dialog(a, JP_DLG_ERROR, "E501", "Settings not written",
               "JKPro could not write the game's settings file. Check that the PS4 has free space.", 1);
        return;
    }
    if (with_backup && backup_saves(a, "before match", NULL, NULL, 0) < 0) {
        dialog(a, JP_DLG_NO_BACKUP, "E401", "Saves not backed up",
               "JKPro could not back up your saves before the match. You can play without a backup, or go back.", 1);
        return;
    }
    if (m->kit_avail || jkpro_kitcfg_present(a->s->game_dir)) {
        if (jkpro_kitcfg_sync(a->s->game_dir, a->s->system_dir, (JkKitMode)m->kit_set) != 0)
            jp_log(a, "JKPRO kitserver: could not set config.txt to %s, played as it is", jkpro_kit_word(m->kit_set));
        else
            jp_log(a, "JKPRO kitserver %s", jkpro_kit_word(m->kit_set));
    }
    if (face_present(a)) face_sync(a, m->face_set);   /* errors are logged; the game still starts */
    if (jkpro_online_is_on(a->s->system_dir) && jkpro_online_set(a->s->system_dir, 1) != 0)   /* picks up online-server.txt */
        jp_log(a, "JKPRO online: could not refresh %s", JKPRO_ONLINE_FLAG);
    char p[PATH_MAX_JP];
    memset(&a->decision, 0, sizeof a->decision);
    a->decision.action = JKPRO_START;
    a->decision.settings = settings_dat(m);
    a->decision.logs = m->logs;
    a->decision.show_fps = m->show_fps;
    p_flag(a, "renderer-pe", p, sizeof p);
    a->decision.pe_renderer = jk_fs_exists(p);
    a->done = 1;
    jp_log(a, "JKPRO play %s", a->s->game_dir);
}

/* ✕ on the game: play when ready, else PATCH first. */
static void play(JpApp *a)
{
    JpTile *g = game_tile(a);
    if (!g) return;
    char body[400];
    switch (g->state) {
    case JP_ST_READY: play_now(a, 1); return;
    case JP_ST_MISSING:
    case JP_ST_UNSUPPORTED: {
        int code = check_files_text(a, body, sizeof body);
        dialog(a, JP_DLG_ERROR, code == 202 ? "E202" : "E205", "Check game files", body, 1);
        return;
    }
    default:
        a->m.play_after_patch = 1;
        patch_run(a);
        return;
    }
}

/* ---------------------------------------------------------------- data packs */

#define DP_SUB "KONAMI/Pro Evolution Soccer 2013/download"

static void dp_live_path(const JpApp *a, char *out, size_t cap)
{
    snprintf(out, cap, "%s/" DP_SUB "/dt0f.img", a->s->datapacks_dir);
}

/* "6.00" -> "6.00"; anything but [0-9A-Za-z.] becomes '_'; "" -> "unknown". */
static void dp_ver_tag(const char *v, char *out, size_t cap)
{
    size_t k = 0;
    for (; v && *v && k + 1 < cap; v++)
        out[k++] = ((*v >= '0' && *v <= '9') || (*v >= 'a' && *v <= 'z') || (*v >= 'A' && *v <= 'Z') || *v == '.') ? *v : '_';
    out[k] = 0;
    if (!k) snprintf(out, cap, "unknown");
}

/* Old packs are named dt0f-<version>-<date_time>.img: the stamp part sorts them. */
static const char *dp_old_stamp(const char *name)
{
    const char *p = strchr(name + 5, '-');
    return p ? p + 1 : name;
}

/* The old/ packs, newest first (at most max). */
static int dp_old_list(const JpApp *a, Ent *e, int max)
{
    char dir[PATH_MAX_JP];
    snprintf(dir, sizeof dir, "%s/old", a->s->datapacks_dir);
    int n = list(dir, e, max), k = 0;
    for (int i = 0; i < n; i++)
        if (!e[i].is_dir && !strncasecmp(e[i].name, "dt0f-", 5)) e[k++] = e[i];
    for (int i = 1; i < k; i++)
        for (int j = i; j > 0 && strcmp(dp_old_stamp(e[j - 1].name), dp_old_stamp(e[j].name)) < 0; j--) {
            Ent t = e[j]; e[j] = e[j - 1]; e[j - 1] = t;
        }
    return k;
}

static void dp_prune_old(const JpApp *a)
{
    Ent e[64];
    int n = dp_old_list(a, e, 64);
    for (int i = JKPRO_PACKS_OLD; i < n; i++) {
        char p[PATH_MAX_JP];
        snprintf(p, sizeof p, "%s/old/%s", a->s->datapacks_dir, e[i].name);
        jk_fs_remove(p);
    }
}

/* The first valid pack waiting in Import (or Import/save); bad gets the first .img that is not one. */
static int dp_find_waiting(const JpApp *a, char *path, size_t cap, JkProPackInfo *info, char *bad, size_t bcap)
{
    if (bad) bad[0] = 0;
    for (int sub = 0; sub < 2; sub++) {
        char dir[PATH_MAX_JP];
        if (sub) snprintf(dir, sizeof dir, "%s/save", a->s->saves_drop);
        else cpy(dir, sizeof dir, a->s->saves_drop);
        Ent e[64];
        int k = list(dir, e, 64);
        for (int i = 0; i < k; i++) {
            if (e[i].is_dir) continue;
            char p[PATH_MAX_JP];
            snprintf(p, sizeof p, "%s/%s", dir, e[i].name);
            size_t l = strlen(e[i].name);
            int img = l > 4 && !strcasecmp(e[i].name + l - 4, ".img");
            if (!img && !jkpro_pack_magic(p)) continue;
            if (jkpro_pack_check(p, info)) { cpy(path, cap, p); return 1; }
            if (bad && !bad[0]) snprintf(bad, bcap, "%s: not a PES 2013 data pack; it stays in Import.", e[i].name);
        }
    }
    return 0;
}

static void dp_refresh(JpApp *a)
{
    JpModel *m = &a->m;
    m->dp_have = 0; m->dp_nold = 0; m->dp_wait = 0;
    m->dp_ver[0] = m->dp_wait_name[0] = m->dp_wait_ver[0] = m->dp_bad[0] = 0;
    m->dp_size = m->dp_wait_size = 0;
    if (!m->dp_on) return;
    char live[PATH_MAX_JP];
    dp_live_path(a, live, sizeof live);
    cpy(m->dp_live, sizeof m->dp_live, live);
    snprintf(m->dp_olddir, sizeof m->dp_olddir, "%s/old/", a->s->datapacks_dir);
    JkProPackInfo in;
    if (jk_fs_exists(live) && jkpro_pack_check(live, &in)) {
        m->dp_have = 1;
        cpy(m->dp_ver, sizeof m->dp_ver, in.version);
        m->dp_size = in.size;
    }
    Ent e[64];
    int n = dp_old_list(a, e, 64);
    for (int i = 0; i < n && m->dp_nold < JKPRO_PACKS_OLD; i++) {
        char p[PATH_MAX_JP];
        snprintf(p, sizeof p, "%s/old/%s", a->s->datapacks_dir, e[i].name);
        cpy(m->dp_old[m->dp_nold].name, sizeof m->dp_old[0].name, e[i].name);
        /* the version from the name: dt0f-<version>-<stamp>.img */
        const char *v = e[i].name + 5, *d = strchr(v, '-');
        snprintf(m->dp_old[m->dp_nold].ver, sizeof m->dp_old[0].ver, "%.*s", d ? (int)(d - v) : 0, v);
        m->dp_old[m->dp_nold].size = jk_fs_size(p);
        m->dp_nold++;
    }
    char wp[PATH_MAX_JP];
    if (dp_find_waiting(a, wp, sizeof wp, &in, m->dp_bad, sizeof m->dp_bad)) {
        m->dp_wait = 1;
        cpy(m->dp_wait_name, sizeof m->dp_wait_name, base_name(wp));
        cpy(m->dp_wait_ver, sizeof m->dp_wait_ver, in.version);
        m->dp_wait_size = in.size;
    }
}

/* Moves the active pack to old/dt0f-<version>-<date_time>.img. 0 ok (or none), -1 failed. */
static int dp_retire_active(JpApp *a, char *moved_as, size_t cap)
{
    char live[PATH_MAX_JP], dir[PATH_MAX_JP], tag[24], st[48], name[128], to[PATH_MAX_JP];
    dp_live_path(a, live, sizeof live);
    if (moved_as) moved_as[0] = 0;
    if (!jk_fs_exists(live)) return 0;
    JkProPackInfo in;
    jkpro_pack_check(live, &in);
    dp_ver_tag(in.version, tag, sizeof tag);
    stamp(a, st, sizeof st);
    snprintf(dir, sizeof dir, "%s/old", a->s->datapacks_dir);
    if (jk_fs_mkdir_p(dir) != 0) return -1;
    snprintf(name, sizeof name, "dt0f-%s-%s.img", tag, st);
    snprintf(to, sizeof to, "%s/%s", dir, name);
    for (int k = 2; jk_fs_exists(to) && k < 100; k++) {
        snprintf(name, sizeof name, "dt0f-%s-%s-%d.img", tag, st, k);
        snprintf(to, sizeof to, "%s/%s", dir, name);
    }
    if (jk_fs_rename(live, to) != 0) return -1;
    if (moved_as) cpy(moved_as, cap, name);
    return 0;
}

/* Puts src in place as the active pack (a move, same filesystem). */
static int dp_activate(JpApp *a, const char *src)
{
    char live[PATH_MAX_JP];
    dp_live_path(a, live, sizeof live);
    if (jk_fs_mkdir_parent(live) != 0 && jk_fs_mkdir_p(a->s->datapacks_dir) != 0) return -1;
    char dir[PATH_MAX_JP];
    snprintf(dir, sizeof dir, "%s/" DP_SUB, a->s->datapacks_dir);
    if (jk_fs_mkdir_p(dir) != 0) return -1;
    return jk_fs_rename(src, live);
}

static void dp_say(JpApp *a, int bad, const char *text)
{
    a->m.dp_msg_bad = bad;
    cpy(a->m.dp_msg, sizeof a->m.dp_msg, text);
}

static void dp_install(JpApp *a)
{
    char wp[PATH_MAX_JP], old[128], body[300];
    JkProPackInfo in;
    if (!dp_find_waiting(a, wp, sizeof wp, &in, NULL, 0)) { dp_say(a, 1, "No data pack is waiting in Import."); return; }
    if (dp_retire_active(a, old, sizeof old) != 0) {
        dp_say(a, 1, "E302: JKPro could not move the active data pack to old/. Nothing was changed.");
        return;
    }
    if (dp_activate(a, wp) != 0) {
        dp_say(a, 1, "E302: JKPro could not move the data pack into place. It is still in Import.");
        return;
    }
    dp_prune_old(a);
    if (old[0]) snprintf(body, sizeof body, "Installed data pack %s. The one before is in old/%s. The game uses it from its "
                         "next start.", in.version[0] ? in.version : "(version unknown)", old);
    else snprintf(body, sizeof body, "Installed data pack %s. The game uses it from its next start.",
                  in.version[0] ? in.version : "(version unknown)");
    dp_say(a, 0, body);
    jp_log(a, "JKPRO data pack installed %s", in.version);
    dp_refresh(a);
}

static void dp_remove(JpApp *a)
{
    char old[128], body[300];
    if (dp_retire_active(a, old, sizeof old) != 0 || !old[0]) {
        dp_say(a, 1, "E302: JKPro could not move the data pack to old/. Nothing was changed.");
        return;
    }
    dp_prune_old(a);
    snprintf(body, sizeof body, "Removed: the pack is in old/%s. The game uses its own base data from its next start.", old);
    dp_say(a, 0, body);
    dp_refresh(a);
}

static void dp_use_old(JpApp *a, int i)
{
    char src[PATH_MAX_JP], tmp[PATH_MAX_JP], old[128], body[300], name[96];
    if (i < 0 || i >= a->m.dp_nold) return;
    cpy(name, sizeof name, a->m.dp_old[i].name);
    snprintf(src, sizeof src, "%s/old/%s", a->s->datapacks_dir, name);
    /* take the chosen one out of old/ first, so retiring the active one never prunes it */
    snprintf(tmp, sizeof tmp, "%s/old/.using-%s", a->s->datapacks_dir, name);
    if (jk_fs_rename(src, tmp) != 0) { dp_say(a, 1, "E302: JKPro could not use that data pack. Nothing was changed."); return; }
    if (dp_retire_active(a, old, sizeof old) != 0 || dp_activate(a, tmp) != 0) {
        jk_fs_rename(tmp, src);
        dp_say(a, 1, "E302: JKPro could not switch the data pack. Nothing was changed.");
        return;
    }
    dp_prune_old(a);
    snprintf(body, sizeof body, "Now using %s.%s%s The game uses it from its next start.", name, old[0] ? " The one before is "
             "in old/" : "", old);
    dp_say(a, 0, body);
    dp_refresh(a);
}

/* The rows of DATA PACKS: install (when waiting), remove (when active), then each old pack. */
typedef enum { DP_ROW_INSTALL = 0, DP_ROW_REMOVE, DP_ROW_OLD } DpRowKind;
static int dp_rows(const JpModel *m, int kinds[], int idx[], int max)
{
    int n = 0;
    if (m->dp_wait && n < max) { kinds[n] = DP_ROW_INSTALL; idx[n++] = 0; }
    if (m->dp_have && n < max) { kinds[n] = DP_ROW_REMOVE; idx[n++] = 0; }
    for (int i = 0; i < m->dp_nold && n < max; i++) { kinds[n] = DP_ROW_OLD; idx[n++] = i; }
    return n;
}

int jkpro_dp_rows(const JpModel *m, int kinds[], int idx[], int max) { return dp_rows(m, kinds, idx, max); }

static int press_datapacks(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    int kinds[8], idx[8], n = dp_rows(m, kinds, idx, 8);
    if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_MENU; return 1; }
    if (p & JKPRO_PAD_UP) { if (m->dp_focus > 0) m->dp_focus--; return 1; }
    if (p & JKPRO_PAD_DOWN) { if (m->dp_focus + 1 < n) m->dp_focus++; return 1; }
    if (!(p & JKPRO_PAD_CROSS) || m->dp_focus >= n) return 0;
    switch (kinds[m->dp_focus]) {
    case DP_ROW_INSTALL: dp_install(a); break;
    case DP_ROW_REMOVE: dp_remove(a); break;
    default: dp_use_old(a, idx[m->dp_focus]); break;
    }
    int n2 = dp_rows(m, kinds, idx, 8);
    if (m->dp_focus >= n2) m->dp_focus = n2 ? n2 - 1 : 0;
    return 1;
}

/* ---------------------------------------------------------------- input */

static JpScreen patch_back = JP_SCR_SHELF, help_back = JP_SCR_SHELF;

static int set_row_locked(const JpModel *m, int row)
{
    return row == JP_SET_RESOLUTION || row == JP_SET_QUALITY || row == JP_SET_DISPLAY || row == JP_SET_FRAMERATE ||
           row == JP_SET_CONTROLLER || (row == JP_SET_KITSERVER && !m->kit_avail) ||
           (row == JP_SET_FACEFIX && !m->face_avail);
}

static void set_changed(JpApp *a)
{
    if (settings_save(a) != 0) cpy(a->m.set_msg, sizeof a->m.set_msg, "Not saved: JKPro could not write settings.ini.");
    else cpy(a->m.set_msg, sizeof a->m.set_msg, "Saved. Used from the next PLAY.");
}

static void open_menu(JpApp *a)
{
    JpTile *g = game_tile(a);
    a->m.screen = JP_SCR_MENU;
    a->m.menu_focus = g && g->state == JP_ST_READY ? JP_MENU_PLAY : JP_MENU_PATCH;
}

static int press_dialog(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (!(p & (JKPRO_PAD_CROSS | JKPRO_PAD_CIRCLE))) return 0;
    JpDialog d = m->dialog;
    m->dialog = JP_DLG_NONE;
    if (!(p & JKPRO_PAD_CROSS)) return 1;
    if (d == JP_DLG_LOAD) saves_load(a);
    else if (d == JP_DLG_RESTORE) saves_restore(a);
    else if (d == JP_DLG_NO_BACKUP) play_now(a, 0);
    return 1;
}

static int press_shelf(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (p & JKPRO_PAD_LEFT) { if (m->tile_focus > 0) m->tile_focus--; return 1; }
    if (p & JKPRO_PAD_RIGHT) { if (m->tile_focus + 1 < m->ntiles) m->tile_focus++; return 1; }
    if (p & JKPRO_PAD_SQUARE) { help_back = JP_SCR_SHELF; m->screen = JP_SCR_HELP; return 1; }
    char body[400];
    if (p & (JKPRO_PAD_CROSS | JKPRO_PAD_TRIANGLE)) {
        if (m->ntiles == 0) {
            snprintf(body, sizeof body,
                     "Copy your whole PES 2013 folder (with pes2013.exe) to %s over FTP, for example %s/PES13. Then "
                     "close JKPro and open it again.", a->root, a->root);
            dialog(a, JP_DLG_ERROR, "E201", "PES 2013 not found", body, 1);
            return 1;
        }
        patch_back = JP_SCR_SHELF;
        if (p & JKPRO_PAD_CROSS) play(a);
        else open_menu(a);
        return 1;
    }
    return 0;
}

static int press_menu(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (p & JKPRO_PAD_UP) { if (m->menu_focus > 0) m->menu_focus--; return 1; }
    if (p & JKPRO_PAD_DOWN) { if (m->menu_focus + 1 < (m->dp_on ? JP_MENU_COUNT : JP_MENU_DATAPACKS)) m->menu_focus++; return 1; }
    if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_SHELF; return 1; }
    if (!(p & JKPRO_PAD_CROSS)) return 0;
    patch_back = JP_SCR_MENU;
    switch (m->menu_focus) {
    case JP_MENU_PLAY: play(a); break;
    case JP_MENU_PATCH: m->play_after_patch = 0; patch_run(a); break;
    case JP_MENU_SETTINGS: m->screen = JP_SCR_SETTINGS; m->set_focus = JP_SET_FPS; m->set_msg[0] = 0; break;
    case JP_MENU_DATAPACKS: m->screen = JP_SCR_DATAPACKS; m->dp_focus = 0; m->dp_msg[0] = 0; dp_refresh(a); break;
    default:
        m->screen = JP_SCR_SAVES; m->sav_focus = JP_SAV_LOAD; m->sav_msg[0] = 0;
        saves_refresh(a);
        break;
    }
    return 1;
}

static int press_patch(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (m->phase == JP_PH_RUNNING) return 0;
    if (p & JKPRO_PAD_CIRCLE) { m->screen = patch_back; m->phase = JP_PH_IDLE; return 1; }
    if (p & JKPRO_PAD_SQUARE) { help_back = JP_SCR_PATCH; m->help_page = JP_HELP_PATCH; m->screen = JP_SCR_HELP; return 1; }
    if (!(p & JKPRO_PAD_CROSS)) return 0;
    if (m->phase == JP_PH_DONE) play_now(a, 1);
    else patch_run(a);
    return 1;
}

static int press_settings(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_MENU; return 1; }
    if (p & (JKPRO_PAD_UP | JKPRO_PAD_DOWN)) {
        int d = (p & JKPRO_PAD_UP) ? -1 : 1, f = m->set_focus;
        do f += d; while (f >= 0 && f < JP_SET_COUNT && set_row_locked(m, f));
        if (f >= 0 && f < JP_SET_COUNT) m->set_focus = f;
        return 1;
    }
    int lr = (p & JKPRO_PAD_RIGHT) ? 1 : (p & JKPRO_PAD_LEFT) ? -1 : 0;
    switch (m->set_focus) {
    case JP_SET_FPS:
        if (lr || (p & JKPRO_PAD_CROSS)) { m->show_fps = !m->show_fps; set_changed(a); return 1; }
        return 0;
    case JP_SET_KITSERVER:
        if (!m->kit_avail || !(lr || (p & JKPRO_PAD_CROSS))) return 0;
        m->kit_set = (m->kit_mode + (lr < 0 ? JK_KIT_COUNT - 1 : 1)) % JK_KIT_COUNT;
        if (settings_save(a) != 0) {
            m->kit_set = m->kit_mode;
            cpy(m->set_msg, sizeof m->set_msg, "Not saved: JKPro could not write settings.ini.");
        } else if (jkpro_kitcfg_sync(a->s->game_dir, a->s->system_dir, (JkKitMode)m->kit_set) != 0) {
            cpy(m->set_msg, sizeof m->set_msg, "Saved, but kitserver/config.txt could not be changed now: JKPro tries again at PLAY.");
            m->kit_mode = m->kit_set;
        } else {
            m->kit_mode = m->kit_set;
            cpy(m->set_msg, sizeof m->set_msg, "Saved. Kitserver's config.txt is changed (your original is in the backup).");
            jp_log(a, "JKPRO kitserver set to %s", jkpro_kit_word(m->kit_set));
        }
        return 1;
    case JP_SET_FACEFIX: {
        if (!m->face_avail || !(lr || (p & JKPRO_PAD_CROSS))) return 0;
        int was = m->face_set;
        m->face_set = m->face_state != JKPRO_FSERVFIX_PATCHED;   /* flip what the file is now */
        if (settings_save(a) != 0) {
            m->face_set = was;
            cpy(m->set_msg, sizeof m->set_msg, "Not saved: JKPro could not write settings.ini.");
            return 1;
        }
        int r = face_sync(a, m->face_set);
        char d[PATH_MAX_JP];
        kit_dir(a, d, sizeof d);
        m->face_state = ff_state(d);
        if (r == JKPRO_FSERVFIX_OTHER)
            cpy(m->set_msg, sizeof m->set_msg, "Your fserv.dll is not the version JKPro knows: left alone.");
        else if (r < 0)
            cpy(m->set_msg, sizeof m->set_msg, "Saved, but fserv.dll could not be changed now (it is unchanged): JKPro tries again at PLAY.");
        else if (m->face_set)
            cpy(m->set_msg, sizeof m->set_msg, "Face fix on. Your original fserv.dll is kept next to it (" JKPRO_FSERV_BACKUP ").");
        else
            cpy(m->set_msg, sizeof m->set_msg, "Face fix off. Your original fserv.dll is back.");
        return 1;
    }
    case JP_SET_ONLINE:
        if (lr) { online_toggle(a); return 1; }
        if (p & JKPRO_PAD_CROSS) { m->screen = JP_SCR_ONLINE; m->online_focus = JP_ONL_SERVER; m->online_msg[0] = 0; return 1; }
        return 0;
    case JP_SET_LOGS:
        if (p & JKPRO_PAD_CROSS) { m->screen = JP_SCR_LOGS; m->logs_focus = 0; m->bundle_msg[0] = 0; return 1; }
        return 0;
    case JP_SET_ABOUT:
        if (p & JKPRO_PAD_CROSS) { m->screen = JP_SCR_ABOUT; return 1; }
        return 0;
    default: return 0;
    }
}

static int press_saves(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_MENU; return 1; }
    if (p & JKPRO_PAD_UP) { if (m->sav_focus > 0) m->sav_focus--; return 1; }
    if (p & JKPRO_PAD_DOWN) { if (m->sav_focus + 1 < JP_SAV_COUNT) m->sav_focus++; return 1; }
    if (p & JKPRO_PAD_SQUARE) { help_back = JP_SCR_SAVES; m->help_page = JP_HELP_SAVES; m->screen = JP_SCR_HELP; return 1; }
    if (!(p & JKPRO_PAD_CROSS)) return 0;
    char body[400];
    saves_refresh(a);
    m->sav_msg[0] = 0;
    switch (m->sav_focus) {
    case JP_SAV_LOAD:
        if (m->import_files == 0) {
            snprintf(body, sizeof body,
                     "Put the files in %s over FTP (or on a USB drive in JKPro/saves/), then choose Load saves. The "
                     "option file (teams, names, kits) is EDIT.bin, the game settings OPTION.bin; a whole save/ folder works "
                     "too.", m->sav_drop);
            dialog(a, JP_DLG_INFO, "", "Nothing to load", body, 0);
        } else {
            snprintf(body, sizeof body, "%s go into the game's save folder. Your current saves are backed up first, to "
                     "System/backup/saves/%s/; restore them at any time.%s%s", m->import_list, a->folder,
                     m->import_warn[0] ? " " : "", m->import_warn);
            dialog(a, JP_DLG_LOAD, "", "Load these saves?", body, 0);
        }
        return 1;
    case JP_SAV_RESTORE:
        if (m->backups == 0) {
            dialog(a, JP_DLG_INFO, "", "No backups yet",
                   "JKPro backs up your saves before every match and every import. There is none yet.", 0);
            return 1;
        }
        m->screen = JP_SCR_RESTORE;
        m->bk_focus = 0;
        m->bk_top = 0;
        return 1;
    default:
        saves_export(a);
        return 1;
    }
}

/* The support bundle: logs only (never saves, never .reg files). */
static void support_bundle(JpApp *a)
{
    JpModel *m = &a->m;
    if (!m->can_bundle) return;
    char st[48], rec[PATH_MAX_JP], ini[PATH_MAX_JP], glog[PATH_MAX_JP], out[PATH_MAX_JP];
    stamp(a, st, sizeof st);
    p_record(a, rec, sizeof rec);
    snprintf(ini, sizeof ini, "%s/games/%s/settings.ini", a->s->system_dir, a->folder);
    snprintf(glog, sizeof glog, "%s/pes2013_d3d9.log", a->s->game_dir ? a->s->game_dir : "");
    /* the title's game log (in the game folder), our settings and PATCH record */
    const char *files[] = { glog, ini, rec, NULL };
    if (!have_game(a)) files[0] = ini, files[1] = rec, files[2] = NULL;
    JkSupportIn in;
    memset(&in, 0, sizeof in);
    in.data_root = a->s->system_dir;
    in.game_dir = have_game(a) ? a->s->game_dir : NULL;
    in.files_dir = a->s->files_dir;
    in.logs_dir = a->s->logs_dir;
    in.usb_dir = a->s->usb_dir;
    in.stamp = st;
    in.log_files = files;
    in.state = have_game(a) ? jkpro_state_text(game_tile(a)->state) : "no game";
    in.product = "JKPro";
    in.exe_name = "pes2013.exe";
    int usb = 0;
    if (jk_fs_mkdir_p(a->s->system_dir) != 0 || jk_support_bundle(&in, out, sizeof out, &usb) != 0) {
        m->bundle_bad = 1;
        snprintf(m->bundle_msg, sizeof m->bundle_msg, "E603: the support bundle could not be saved in %s.", a->s->logs_dir);
        jp_log(a, "JKPRO E603 support bundle failed: %s", a->s->logs_dir);
        return;
    }
    m->bundle_bad = 0;
    snprintf(m->bundle_msg, sizeof m->bundle_msg, "Saved in %s%s. It holds logs only: no saves, no registry file.", out,
             usb ? " and on the USB drive (JKPro-support)" : "");
    jp_log(a, "JKPRO support bundle %s", out);
}

static int press_restore(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_SAVES; return 1; }
    if (p & JKPRO_PAD_UP) { if (m->bk_focus > 0) m->bk_focus--; }
    else if (p & JKPRO_PAD_DOWN) { if (m->bk_focus + 1 < m->nbk) m->bk_focus++; }
    else if (p & JKPRO_PAD_CROSS) {
        if (m->bk_focus >= m->nbk) return 0;
        char now[8192], cur[160], body[400], when[48];
        char live[PATH_MAX_JP];
        p_live(a, live, sizeof live);
        saves_digest(live, now, sizeof now, 0);
        digest_summary(now, cur, sizeof cur);
        const char *nm = m->bk[m->bk_focus].name;
        cpy(restore_target, sizeof restore_target, nm);
        if (strlen(nm) >= 15 && nm[10] == '_') snprintf(when, sizeof when, "%.10s %.2s:%.2s", nm, nm + 11, nm + 13);
        else cpy(when, sizeof when, nm);
        snprintf(body, sizeof body, "Your saves go back to %s (%s): %s. They replace what PES 2013 has now: %s. That is "
                 "backed up first (before restore).", when, m->bk[m->bk_focus].reason, m->bk[m->bk_focus].files,
                 cur[0] ? cur : "no saves");
        dialog(a, JP_DLG_RESTORE, "", "Restore this backup?", body, 0);
        return 1;
    } else return 0;
    if (m->bk_focus < m->bk_top) m->bk_top = m->bk_focus;
    if (m->bk_focus >= m->bk_top + 7) m->bk_top = m->bk_focus - 6;
    return 1;
}

static int press_logs(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_SETTINGS; return 1; }
    int rows = m->can_bundle ? 2 : 1;
    if (p & JKPRO_PAD_UP) { if (m->logs_focus > 0) m->logs_focus--; return 1; }
    if (p & JKPRO_PAD_DOWN) { if (m->logs_focus + 1 < rows) m->logs_focus++; return 1; }
    if (m->logs_focus == 1) {
        if (p & JKPRO_PAD_CROSS) { support_bundle(a); return 1; }
        return 0;
    }
    if (!(p & (JKPRO_PAD_CROSS | JKPRO_PAD_LEFT | JKPRO_PAD_RIGHT))) return 0;
    char flag[PATH_MAX_JP];
    p_flag(a, "logs-on", flag, sizeof flag);
    if (m->logs) {
        if (jk_fs_remove(flag) == 0) m->logs = 0;
    } else if (jk_fs_mkdir_p(a->s->system_dir) == 0 && jk_fs_write_atomic(flag, "1\n", 2) == 0) {
        m->logs = 1;
    }
    return 1;
}

static int press_online(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (m->kb_field) {
        int r = jk_keyboard_input(&m->kb, p);
        if (r == JK_KB_EDITING) return 1;
        int field = m->kb_field;
        m->kb_field = 0;
        if (r != JK_KB_DONE) return 1;
        char *dst = field == JP_ONL_ID ? m->online_id : field == JP_ONL_PW ? m->online_pw : m->online_code;
        snprintf(dst, sizeof m->online_id, "%s", jk_keyboard_text(&m->kb));
        memset(&m->kb.text, 0, sizeof m->kb.text);
        memset(&m->kb.initial, 0, sizeof m->kb.initial);
        if (account_save(a) != 0)
            cpy(m->online_msg, sizeof m->online_msg, "Not saved: JKPro could not write in its System folder.");
        else if (field == JP_ONL_PW && m->online_pw[0] && (strlen(m->online_pw) < 6 || strlen(m->online_pw) > 20))
            cpy(m->online_msg, sizeof m->online_msg,
                "Password saved. ThirteenServer passwords are 6 to 20 characters: check yours.");
        else
            cpy(m->online_msg, sizeof m->online_msg, field == JP_ONL_ID ? "Online ID saved."
                                                   : field == JP_ONL_PW ? "Password saved." : "Registration code saved.");
        return 1;
    }
    if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_SETTINGS; return 1; }
    if (p & JKPRO_PAD_UP) { if (m->online_focus > 0) m->online_focus--; return 1; }
    if (p & JKPRO_PAD_DOWN) { if (m->online_focus + 1 < JP_ONL_COUNT) m->online_focus++; return 1; }
    int lr = p & (JKPRO_PAD_LEFT | JKPRO_PAD_RIGHT);
    if (m->online_focus == JP_ONL_SERVER) {
        if (lr || (p & JKPRO_PAD_CROSS)) { m->online_msg[0] = 0; online_toggle(a); return 1; }
        return 0;
    }
    if (!(p & JKPRO_PAD_CROSS)) return 0;
    m->online_msg[0] = 0;
    m->kb_field = m->online_focus;
    if (m->online_focus == JP_ONL_ID) jk_keyboard_init(&m->kb, "ONLINE ID", m->online_id, 63, NULL);
    else if (m->online_focus == JP_ONL_CODE) jk_keyboard_init(&m->kb, "REGISTRATION CODE", m->online_code, 63, NULL);
    else {
        jk_keyboard_init(&m->kb, "PASSWORD", m->online_pw, 63, NULL);
        m->kb.mask = 1;
    }
    return 1;
}

int jkpro_app_press(JpApp *a, uint32_t p)
{
    JpModel *m = &a->m;
    if (!p || a->done) return 0;
    if (m->dialog != JP_DLG_NONE) return press_dialog(a, p);
    switch (m->screen) {
    case JP_SCR_SHELF: return press_shelf(a, p);
    case JP_SCR_MENU: return press_menu(a, p);
    case JP_SCR_PATCH: return press_patch(a, p);
    case JP_SCR_SETTINGS: return press_settings(a, p);
    case JP_SCR_SAVES: return press_saves(a, p);
    case JP_SCR_LOGS: return press_logs(a, p);
    case JP_SCR_ONLINE: return press_online(a, p);
    case JP_SCR_RESTORE: return press_restore(a, p);
    case JP_SCR_DATAPACKS: return press_datapacks(a, p);
    case JP_SCR_HELP:
        if (p & (JKPRO_PAD_LEFT | JKPRO_PAD_UP)) { m->help_page = (m->help_page + JP_HELP_COUNT - 1) % JP_HELP_COUNT; return 1; }
        if (p & (JKPRO_PAD_RIGHT | JKPRO_PAD_DOWN)) { m->help_page = (m->help_page + 1) % JP_HELP_COUNT; return 1; }
        if (p & (JKPRO_PAD_CIRCLE | JKPRO_PAD_SQUARE)) { m->screen = help_back; return 1; }
        return 0;
    case JP_SCR_ABOUT:
        if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_SETTINGS; return 1; }
        return 0;
    case JP_SCR_FAILED:
        if (p & JKPRO_PAD_CIRCLE) { m->screen = JP_SCR_SHELF; return 1; }
        if (p & JKPRO_PAD_SQUARE) {
            if (m->can_bundle) { support_bundle(a); return 1; }
            help_back = JP_SCR_FAILED; m->help_page = JP_HELP_TROUBLE; m->screen = JP_SCR_HELP;
            return 1;
        }
        if (p & JKPRO_PAD_CROSS) {
            m->screen = JP_SCR_SHELF;
            patch_back = JP_SCR_SHELF;
            if (have_game(a)) play(a);
            return 1;
        }
        return 0;
    }
    return 0;
}

/* ---------------------------------------------------------------- init and run */

int jkpro_app_init(JpApp *a, const JkProServices *s, JkFrame *frame)
{
    if (!a || !s || !s->prefix_dir || !s->system_dir || !s->files_dir || !s->saves_drop) return -1;
    memset(a, 0, sizeof *a);
    a->s = s;
    a->frame = frame;
    patch_back = help_back = JP_SCR_SHELF;
    const char *game = s->game_dir ? s->game_dir : "";
    if (game[0]) cpy(a->folder, sizeof a->folder, base_name(game));
    /* the JKPro root: the game folder's parent, else system_dir's */
    cpy(a->root, sizeof a->root, game[0] ? game : s->system_dir);
    char *slash = strrchr(a->root, '/');
    if (slash && slash != a->root) *slash = 0;
    a->m.screen = JP_SCR_SHELF;
    a->m.ntiles = 0;
    tiles_refresh(a);
    settings_load(a);
    saves_refresh(a);
    a->m.can_bundle = s->logs_dir && s->logs_dir[0];
    /* the release verdict: information only, it never changes what JKPro does */
    if (s->release) {
        const JkRelease *r = s->release;
        a->m.rel_state = (int)r->state;
        a->m.rel_changed = r->changed;
        cpy(a->m.rel_version, sizeof a->m.rel_version, r->version);
        cpy(a->m.rel_key_label, sizeof a->m.rel_key_label, r->key_label);
        cpy(a->m.rel_key_id, sizeof a->m.rel_key_id, r->key_id);
        cpy(a->m.rel_reason, sizeof a->m.rel_reason, r->reason);
    }
    cpy(a->m.build_id, sizeof a->m.build_id, s->build_id);
    cpy(a->m.root_path, sizeof a->m.root_path, a->root);
    if (game[0]) cpy(a->m.game_path, sizeof a->m.game_path, game);
    else snprintf(a->m.game_path, sizeof a->m.game_path, "%s/PES13", a->root);
    snprintf(a->m.online_file, sizeof a->m.online_file, "%s/" JKPRO_ACCOUNT, s->system_dir ? s->system_dir : "");
    if (s->logs_dir && s->logs_dir[0]) cpy(a->m.logs_path, sizeof a->m.logs_path, s->logs_dir);
    else snprintf(a->m.logs_path, sizeof a->m.logs_path, "%s/Logs", a->root);
    a->m.dp_on = s->datapacks_dir && s->datapacks_dir[0];
    dp_refresh(a);
    {
        char pe[PATH_MAX_JP];
        p_flag(a, "renderer-pe", pe, sizeof pe);
        a->m.pe_renderer = jk_fs_exists(pe);
    }
    /* Documents moved out of the prefix (docs_dir): copy saves found only in the old place, once */
    int moved = 0;
    if (s->docs_dir && s->docs_dir[0]) {
        char now[PATH_MAX_JP], old[PATH_MAX_JP];
        p_live(a, now, sizeof now);
        snprintf(old, sizeof old, "%s/" JKPRO_DOCS_REL "/save", s->prefix_dir);
        if (count_files(now) == 0 && count_files(old) > 0) {
            if (copy_tree(old, now, 0) == 0) {
                moved = 1;
                jp_log(a, "JKPRO saves copied to %s", now);
                saves_refresh(a);
            } else {
                jp_log(a, "JKPRO E402 saves not copied to %s", now);
            }
        }
    }
    /* a crash may have emptied the registry: put it back now and say so */
    int fixed = 0, bad = registry_check(a, &fixed);
    if (bad > 0) registry_damaged_dialog(a);
    else if (fixed > 0)
        dialog(a, JP_DLG_INFO, "E610", "Windows settings restored",
               "The last crash damaged the Windows settings PES 2013 needs. JKPro put back the last good copy, so the "
               "game can start as before. Your saves and settings are not affected.", 0);
    else if (moved) {
        char body[400], now[PATH_MAX_JP];
        snprintf(now, sizeof now, "%s", s->docs_dir);
        snprintf(body, sizeof body, "Your PES 2013 saves now live in %s, where you can copy them over FTP. JKPro "
                 "copied them there; the old copy is kept.", now);
        dialog(a, JP_DLG_INFO, "", "Saves have a new home", body, 0);
    }
    if (s->start_failure && s->start_failure[0]) {
        a->m.screen = JP_SCR_FAILED;
        cpy(a->m.failure, sizeof a->m.failure, s->start_failure);
        jp_log(a, "JKPRO E101 didn't start: %s", s->start_failure);
    }
    jp_log(a, "JKPRO ui open game=%s", game);
    return 0;
}

int jkpro_ui_run(const JkProServices *s, JkProDecision *out)
{
    if (!s || !out || !s->present || !s->now_ns || !s->sleep_us) return -1;
    JkFrame f = { calloc((size_t)JK_W * JK_H, 4), JK_W, JK_H, JK_W * 4 };
    if (!f.pixels) return -1;
    static JpApp a;
    if (jkpro_app_init(&a, s, &f) != 0) { free(f.pixels); return -1; }
    int rc = 0, dirty = 1;
    uint64_t last_present = 0;
    while (!a.done) {
        if (s->should_quit && s->should_quit(s->ctx)) {
            memset(&a.decision, 0, sizeof a.decision);
            a.decision.action = JKPRO_LEAVE;
            break;
        }
        uint32_t held = 0, pressed = 0;
        if (s->pad && s->pad(s->ctx, &held, &pressed) >= 0 && pressed)
            dirty |= jkpro_app_press(&a, pressed);
        if (a.done) break;
        uint64_t now = s->now_ns(s->ctx);
        if (dirty || now - last_present > 250000000ull) {
            if (dirty) jkpro_draw(&f, &a.m);
            if (s->present(s->ctx, &f) < 0) { rc = -1; break; }
            last_present = now;
            dirty = 0;
        }
        s->sleep_us(s->ctx, 16000);
    }
    *out = a.decision;
    if (rc == 0 && !a.done && out->action != JKPRO_LEAVE) out->action = JKPRO_LEAVE;
    free(f.pixels);
    jkpro_draw_release();
    return rc;
}
