/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro launcher UI: shelf, game menu, PATCH, settings, save games, PLAY. */
#include "jkpro_ui.h"
#include "jkpro_kitcfg.h"
#include "jkpro_fservfix.h"
#include "jk_fs.h"
#include "jk_pes13set.h"
#include "jk_test.h"
#include "jk_tmp.h"
#include "jk_screens.h"

#include <dirent.h>

static const char *STORE =
    "WINE REGISTRY Version 2\n;; All keys relative to \\\\Machine\n\n#arch=win64\n\n"
    "[Software\\\\Microsoft\\\\Windows] 1700000000\n#time=1\n\"A\"=\"b\"\n\n";
static const char *REG_OK =
    "Windows Registry Editor Version 5.00\n\n[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\KONAMI\\PES2013]\n"
    "\"code\"=\"AAAA-BBBB-CCCC-DDDD-EEEE\"\n\"version\"=\"1.04.0000\"\n";
static const char *REG_NO_CODE =
    "Windows Registry Editor Version 5.00\n\n[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\KONAMI\\PES2013]\n"
    "\"version\"=\"1.04.0000\"\n";

typedef struct {
    char root[256], game[300], sys[300], prefix[300], files[300], drop[300], usb[300];
    JkProServices s;
    JpApp a;
    int stamp;
} Fx;

static uint64_t fx_now(void *c) { (void)c; return 0; }
static void fx_sleep(void *c, uint32_t us) { (void)c; (void)us; }
static int fx_wall(void *c, char *out, size_t cap)
{
    Fx *x = c;
    snprintf(out, cap, "2026-11-25_%04d", ++x->stamp);
    return 0;
}

static void fx_open(Fx *x, int with_game)
{
    memset(x, 0, sizeof *x);
    tmp_dir(x->root, sizeof x->root);
    snprintf(x->game, sizeof x->game, "%s/PES13", x->root);
    snprintf(x->sys, sizeof x->sys, "%s/System", x->root);
    snprintf(x->prefix, sizeof x->prefix, "%s/System/prefix", x->root);
    snprintf(x->files, sizeof x->files, "%s/System/jkpro-files", x->root);
    snprintf(x->drop, sizeof x->drop, "%s/Saves/PES13/Import", x->root);
    tmp_put(x->root, "System/prefix/system.reg", STORE);
    tmp_put(x->root, "System/jkpro-files/d3d9.dll", "OUR-D3D9-CLIENT");
    if (with_game) {
        tmp_put(x->root, "PES13/pes2013.exe", "MZ-not-really");
        tmp_put(x->root, "PES13/settings.exe", "MZ");
        tmp_put(x->root, "PES13/img/dt01.img", "img");
    }
    x->s = (JkProServices){ with_game ? x->game : "", x->prefix, x->sys, x->files, x->drop, NULL,
                            x, NULL, NULL, NULL, fx_now, fx_sleep, NULL, fx_wall, NULL, NULL, NULL, NULL, NULL, NULL };
}

static void fx_init(Fx *x) { CHECK_EQ(jkpro_app_init(&x->a, &x->s, NULL), 0); }
static void fx_close(Fx *x) { tmp_rm_rf(x->root); }
static void press(Fx *x, uint32_t b) { jkpro_app_press(&x->a, b); }

static int exists(const char *root, const char *rel)
{
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    return jk_fs_exists(p);
}

static int count_dirs(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) if (e->d_name[0] != '.') n++;
    closedir(d);
    return n;
}

#define LIVE "System/prefix/" JKPRO_DOCS_REL "/save"
#define SDAT "System/prefix/" JKPRO_DOCS_REL "/settings.dat"

static void shelf_shows_only_the_pes13_folder(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES6/PES6.exe", "MZ");          /* another game: 1.0 leaves it alone */
    tmp_put(x.root, "Backup-old/readme.txt", "x");
    tmp_put(x.root, "Saves/PES13/Import/.keep", "");
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK_EQ(m->screen, JP_SCR_SHELF);
    CHECK_EQ(m->ntiles, 1);
    CHECK_EQ(m->game_tile, 0);
    CHECK_EQ(m->tile_focus, 0);
    CHECK(!strcmp(m->tiles[0].title, "PES 2013"));
    CHECK(!strcmp(m->tiles[0].folder, "PES13"));
    CHECK_EQ(m->tiles[0].state, JP_ST_NEEDS_PATCH);
    press(&x, JKPRO_PAD_RIGHT);                      /* nothing to move to */
    CHECK_EQ(m->tile_focus, 0);
    CHECK(!exists(x.root, "PES6/d3d9.dll"));
    fx_close(&x);
}

static void no_game_shows_where_to_put_it(void)
{
    Fx x;
    fx_open(&x, 0);
    fx_init(&x);
    CHECK_EQ(x.a.m.ntiles, 0);
    CHECK_EQ(x.a.m.game_tile, -1);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.dialog, JP_DLG_ERROR);
    CHECK(!strcmp(x.a.m.dlg_code, "E201"));
    CHECK(!x.a.done);
    fx_close(&x);
}

static void cross_on_unpatched_runs_patch_and_stops_at_missing_reg(void)
{
    Fx x;
    fx_open(&x, 1);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);
    JpModel *m = &x.a.m;
    CHECK_EQ(m->screen, JP_SCR_PATCH);
    CHECK_EQ(m->phase, JP_PH_FAILED);
    CHECK_EQ(m->steps[JP_STEP_CHECK], JP_SS_OK);
    CHECK_EQ(m->steps[JP_STEP_WINDOWS], JP_SS_OK);
    CHECK_EQ(m->steps[JP_STEP_FILES], JP_SS_OK);
    CHECK_EQ(m->steps[JP_STEP_REGISTRY], JP_SS_FAIL);
    CHECK_EQ(m->steps[JP_STEP_SETTINGS], JP_SS_WAIT);
    CHECK_EQ(m->dialog, JP_DLG_ERROR);
    CHECK(!strcmp(m->dlg_code, "E210"));
    CHECK(strstr(m->dlg_body, x.game) != NULL);   /* says where to put the file */
    CHECK(exists(x.root, "PES13/d3d9.dll"));
    CHECK(!x.a.done);
    fx_close(&x);
}

static void reg_without_code_is_e211(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/PES_2013_Registry_Fix.reg", REG_NO_CODE);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.phase, JP_PH_FAILED);
    CHECK(!strcmp(x.a.m.dlg_code, "E211"));
    fx_close(&x);
}

static void patch_completes_then_cross_plays_with_settings(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/PES_2013_Registry_Fix.reg", REG_OK);
    tmp_put(x.root, "PES13/d3d9.dll", "SOMEONE-ELSES-D3D9");
    fx_init(&x);
    JpModel *m = &x.a.m;
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->phase, JP_PH_DONE);
    for (int i = 0; i < JP_STEP_COUNT; i++)
        CHECK_EQ(m->steps[i], JP_SS_OK);
    CHECK(strstr(m->reg_used, "PES_2013_Registry_Fix.reg") != NULL);
    CHECK(exists(x.root, SDAT));
    char buf[4096];
    tmp_get(x.root, "PES13/d3d9.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "OUR-D3D9-CLIENT"));
    CHECK_EQ(count_dirs(x.sys) > 0, 1);
    /* the foreign d3d9.dll was kept */
    char bdir[400];
    snprintf(bdir, sizeof bdir, "%s/backup/PES13", x.sys);
    CHECK_EQ(count_dirs(bdir), 1);
    tmp_get(x.root, "System/prefix/system.reg", buf, sizeof buf);
    CHECK(strstr(buf, "\"code\"=") != NULL);
    /* settings.dat: defaults 1920x1080 High, full screen, XInput, vsync */
    unsigned char want[JK_PES13SET_SIZE];
    JkPes13Settings def = { 1920, 1080, 1, 2, 1, 1 };
    jk_pes13set_build(&def, want);
    char sdat[512];
    snprintf(sdat, sizeof sdat, "%s/" SDAT, x.root);
    size_t len = 0;
    char *got = jk_fs_read(sdat, 4096, &len);
    CHECK(got && len == JK_PES13SET_SIZE && !memcmp(got, want, len));
    free(got);
    /* done: ✕ = PLAY */
    tmp_put(x.root, LIVE "/OPTION.bin", "my teams");
    press(&x, JKPRO_PAD_CROSS);
    CHECK(x.a.done);
    CHECK_EQ(x.a.decision.action, JKPRO_START);
    CHECK_EQ(x.a.decision.settings.width, 1920);
    CHECK_EQ(x.a.decision.settings.quality, 2);
    CHECK_EQ(x.a.decision.settings.xinput, 1);
    CHECK_EQ(x.a.decision.settings.vsync, 1);
    CHECK_EQ(x.a.decision.settings.fullscreen, 1);
    CHECK_EQ(x.a.decision.logs, 0);
    CHECK_EQ(x.a.decision.show_fps, 0);
    /* a backup of the saves was taken before PLAY */
    snprintf(bdir, sizeof bdir, "%s/backup/saves/PES13", x.sys);
    CHECK_EQ(count_dirs(bdir), 1);
    fx_close(&x);
}

static void reopened_patched_game_is_ready_and_changed_d3d9_needs_patch_again(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.phase, JP_PH_DONE);
    fx_init(&x);
    CHECK_EQ(x.a.m.tiles[0].state, JP_ST_READY);
    tmp_put(x.root, "PES13/d3d9.dll", "A-PATCH-OVERWROTE-IT");
    tmp_put(x.root, "PES13/kitserver/readme.txt", "ks");
    fx_init(&x);
    CHECK_EQ(x.a.m.tiles[0].state, JP_ST_PATCH_AGAIN);
    CHECK_EQ(x.a.m.tiles[0].modded, 1);
    press(&x, JKPRO_PAD_CROSS);             /* ✕ patches again, never starts */
    CHECK_EQ(x.a.m.screen, JP_SCR_PATCH);
    CHECK_EQ(x.a.m.phase, JP_PH_DONE);
    CHECK(!x.a.done);
    fx_close(&x);
}

static void triangle_menu_and_settings_are_locked_to_1080_high(void)
{
    Fx x;
    fx_open(&x, 1);
    /* an older settings.ini with other values is ignored (owner: locked) */
    tmp_put(x.root, "System/games/PES13/settings.ini", "[settings]\nresolution = 1280x720\nquality = low\n");
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK_EQ(m->res, 1);
    CHECK_EQ(m->quality, 2);
    press(&x, JKPRO_PAD_TRIANGLE);
    CHECK_EQ(m->screen, JP_SCR_MENU);
    CHECK_EQ(m->menu_focus, JP_MENU_PATCH);  /* not patched: PATCH is focused */
    press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(m->menu_focus, JP_MENU_SETTINGS);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->screen, JP_SCR_SETTINGS);
    CHECK_EQ(m->set_focus, JP_SET_FPS);       /* the first row that can change */
    press(&x, JKPRO_PAD_UP);                  /* the locked rows above are never focused */
    CHECK_EQ(m->set_focus, JP_SET_FPS);
    press(&x, JKPRO_PAD_TRIANGLE);           /* no Defaults any more: nothing to reset */
    CHECK_EQ(m->res, 1);
    CHECK_EQ(m->quality, 2);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->show_fps, 1);
    char buf[2048];
    CHECK(tmp_get(x.root, "System/games/PES13/settings.ini", buf, sizeof buf) >= 0);
    CHECK(strstr(buf, "show_fps") != NULL);
    fx_init(&x);
    CHECK_EQ(x.a.m.show_fps, 1);
    /* PLAY: 1920x1080, High, full screen, vsync, XInput; Show FPS to the title */
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    press(&x, JKPRO_PAD_CROSS);              /* PATCH */
    press(&x, JKPRO_PAD_CROSS);              /* PLAY */
    CHECK(x.a.done);
    CHECK_EQ(x.a.decision.show_fps, 1);
    CHECK_EQ(x.a.decision.settings.width, 1920);
    CHECK_EQ(x.a.decision.settings.height, 1080);
    CHECK_EQ(x.a.decision.settings.quality, 2);
    CHECK_EQ(x.a.decision.settings.fullscreen, 1);
    CHECK_EQ(x.a.decision.settings.vsync, 1);
    /* ○ back to the menu, ○ back to the shelf */
    fx_init(&x);
    press(&x, JKPRO_PAD_TRIANGLE); press(&x, JKPRO_PAD_DOWN); press(&x, JKPRO_PAD_DOWN); press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.screen, JP_SCR_SETTINGS);
    press(&x, JKPRO_PAD_CIRCLE);
    CHECK_EQ(x.a.m.screen, JP_SCR_MENU);
    press(&x, JKPRO_PAD_CIRCLE);
    CHECK_EQ(x.a.m.screen, JP_SCR_SHELF);
    fx_close(&x);
}

static void logs_setting_writes_flag_file(void)
{
    Fx x;
    fx_open(&x, 1);
    fx_init(&x);
    press(&x, JKPRO_PAD_TRIANGLE); press(&x, JKPRO_PAD_DOWN); press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.screen, JP_SCR_SETTINGS);
    press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(x.a.m.set_focus, JP_SET_LOGS);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.screen, JP_SCR_LOGS);
    press(&x, JKPRO_PAD_CROSS);              /* logs on */
    CHECK_EQ(x.a.m.logs, 1);
    CHECK(exists(x.root, "System/logs-on"));
    press(&x, JKPRO_PAD_CROSS);              /* off again */
    CHECK(!exists(x.root, "System/logs-on"));
    CHECK_EQ(x.a.m.pe_renderer, 0);          /* native by default; the fallback is a support flag */
    tmp_put(x.root, "System/renderer-pe", "");
    fx_init(&x);
    CHECK_EQ(x.a.m.pe_renderer, 1);
    fx_close(&x);
}

static void import_loads_saves_with_backup_and_moves_drop(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, LIVE "/OPTION.bin", "old teams");
    tmp_put(x.root, "Saves/PES13/Import/OPTION.bin", "new teams");
    tmp_put(x.root, "Saves/PES13/Import/save/EDIT.bin", "edit");
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK_EQ(m->import_files, 2);
    CHECK_EQ(m->live_files, 1);
    press(&x, JKPRO_PAD_TRIANGLE);
    m->menu_focus = JP_MENU_SAVES;
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->screen, JP_SCR_SAVES);
    CHECK_EQ(m->sav_focus, JP_SAV_LOAD);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->dialog, JP_DLG_LOAD);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->dialog, JP_DLG_NONE);
    CHECK_EQ(m->sav_msg_bad, 0);
    char buf[256];
    tmp_get(x.root, LIVE "/OPTION.bin", buf, sizeof buf);
    CHECK(!strcmp(buf, "new teams"));
    CHECK(exists(x.root, LIVE "/EDIT.bin"));
    CHECK_EQ(m->import_files, 0);
    CHECK(!exists(x.root, "Saves/PES13/Import/OPTION.bin"));
    CHECK_EQ(m->backups, 1);
    char bdir[400];
    snprintf(bdir, sizeof bdir, "%s/backup/saves/PES13", x.sys);
    CHECK_EQ(count_dirs(bdir), 1);
    /* restore the backup: the old teams come back, and the current saves are backed up first */
    m->sav_focus = JP_SAV_RESTORE;
    press(&x, JKPRO_PAD_CROSS);              /* the list of every backup */
    CHECK_EQ(m->screen, JP_SCR_RESTORE);
    CHECK_EQ(m->nbk, 1);
    CHECK(!strcmp(m->bk[0].reason, "before import"));
    CHECK(strstr(m->bk[0].files, "OPTION.bin") != NULL);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->dialog, JP_DLG_RESTORE);
    CHECK(strstr(m->dlg_body, "before import") != NULL);     /* names what comes back ... */
    CHECK(strstr(m->dlg_body, "EDIT.bin") != NULL);          /* ... and what it replaces */
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->screen, JP_SCR_SAVES);
    tmp_get(x.root, LIVE "/OPTION.bin", buf, sizeof buf);
    CHECK(!strcmp(buf, "old teams"));
    CHECK(!exists(x.root, LIVE "/EDIT.bin"));
    CHECK(!exists(x.root, LIVE "/backup.txt"));              /* a backup's own note never lands in the saves */
    CHECK_EQ(m->backups, 2);
    CHECK(!strcmp(m->bk[0].reason, "before restore"));
    fx_close(&x);
}

static void restore_any_backup_and_no_duplicates(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);              /* PATCH */
    /* five matches with different saves, then three with the same: 5 backups, not 8 */
    for (int i = 0; i < 8; i++) {
        char v[32];
        snprintf(v, sizeof v, "teams v%d", i < 5 ? i : 4);
        tmp_put(x.root, LIVE "/OPTION.bin", v);
        fx_init(&x);
        press(&x, JKPRO_PAD_CROSS);          /* PLAY */
        CHECK(x.a.done);
    }
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK_EQ(m->backups, 5);
    CHECK_EQ(m->nbk, 5);
    for (int i = 0; i < m->nbk; i++) CHECK(!strcmp(m->bk[i].reason, "before match"));
    /* backup.txt: reason, time, files with sizes */
    char bt[600], buf[2048];
    snprintf(bt, sizeof bt, "System/backup/saves/PES13/%s/backup.txt", m->bk[0].name);
    CHECK(tmp_get(x.root, bt, buf, sizeof buf) >= 0);
    CHECK(strstr(buf, "reason before match") != NULL);
    CHECK(strstr(buf, "OPTION.bin 8 ") != NULL);        /* "teams v4": 8 bytes */
    /* pick the oldest (v0), restore it */
    m->screen = JP_SCR_SAVES;
    m->sav_focus = JP_SAV_RESTORE;
    press(&x, JKPRO_PAD_CROSS);
    for (int i = 0; i < 10; i++) press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(m->bk_focus, 4);
    press(&x, JKPRO_PAD_CROSS);
    press(&x, JKPRO_PAD_CROSS);
    tmp_get(x.root, LIVE "/OPTION.bin", buf, sizeof buf);
    CHECK(!strcmp(buf, "teams v0"));
    CHECK_EQ(m->backups, 6);                 /* + before restore */
    fx_close(&x);
}

static void saves_screen_names_the_real_paths_and_warns_on_a_wrong_name(void)
{
    Fx x;
    fx_open(&x, 1);
    char docs[300];
    snprintf(docs, sizeof docs, "%s/Saves/PES13", x.root);
    x.s.docs_dir = docs;
    tmp_put(x.root, "Saves/PES13/" JKPRO_DOCS_SUB "/save/OPTION.bin", "old teams");
    tmp_put(x.root, "Saves/PES13/Import/PES13_option_2026.bin", "new teams");
    fx_init(&x);
    JpModel *m = &x.a.m;
    char want[600];
    snprintf(want, sizeof want, "%s/", x.drop);
    CHECK(!strcmp(m->sav_drop, want));
    snprintf(want, sizeof want, "%s/Saves/PES13/" JKPRO_DOCS_SUB "/save/", x.root);
    CHECK(!strcmp(m->sav_live, want));
    snprintf(want, sizeof want, "%s/backup/saves/PES13/", x.sys);
    CHECK(!strcmp(m->sav_bak, want));
    CHECK(strstr(m->import_warn, "PES13_option_2026.bin") != NULL);
    CHECK(strstr(m->import_warn, "PES 2013 reads EDIT.bin (option file) and OPTION.bin (settings)") != NULL);
    /* only a warning: Load saves still copies it, and says where everything went */
    m->screen = JP_SCR_SAVES;
    m->sav_focus = JP_SAV_LOAD;
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->dialog, JP_DLG_LOAD);
    CHECK(strstr(m->dlg_body, "System/backup/saves/PES13/") != NULL);
    press(&x, JKPRO_PAD_CROSS);
    CHECK(exists(x.root, "Saves/PES13/" JKPRO_DOCS_SUB "/save/PES13_option_2026.bin"));
    CHECK(strstr(m->sav_msg, "Loaded PES13_option_2026.bin.") != NULL);
    CHECK(strstr(m->sav_msg, "Import is empty again.") != NULL);
    CHECK(!exists(x.root, "Saves/PES13/Import/PES13_option_2026.bin"));     /* deleted from Import ... */
    CHECK(exists(x.root, "Saves/PES13/" JKPRO_DOCS_SUB "/save/PES13_option_2026.bin"));   /* ... it is in the saves */
    {
        DIR *d = opendir(x.drop);
        struct dirent *e;
        int left = 0;
        while (d && (e = readdir(d))) if (e->d_name[0] != '.') left++;
        if (d) closedir(d);
        CHECK_EQ(left, 0);                   /* no done- folders: Import is empty */
    }
    CHECK(strstr(m->sav_msg, "Backup of the saves you had: 2026-11-25_") != NULL);
    CHECK(strstr(m->sav_msg, "Rename yours to one of these") != NULL);
    /* a big OPTION.bin is most likely an option file: the hint says EDIT.bin (and it is still copied) */
    {
        char p[600];
        snprintf(p, sizeof p, "%s/OPTION.bin", x.drop);
        tmp_put(x.root, "Saves/PES13/Import/OPTION.bin", "");
        FILE *f = fopen(p, "wb");
        for (int i = 0; i < 3 * 1024 * 1024; i++) fputc(i & 255, f);
        fclose(f);
        fx_init(&x);
        CHECK(strstr(x.a.m.import_warn, "looks like an option file: name it EDIT.bin") != NULL);
        remove(p);
    }
    /* OPTION.bin (small) and EDIT.bin are never warned about */
    tmp_put(x.root, "Saves/PES13/Import/OPTION.bin", "teams");
    tmp_put(x.root, "Saves/PES13/Import/EDIT.bin", "edit");
    fx_init(&x);
    CHECK(x.a.m.import_warn[0] == 0);
    /* nothing to load: the dialog names the drop path and the file name */
    fx_close(&x);
    fx_open(&x, 1);
    fx_init(&x);
    x.a.m.screen = JP_SCR_SAVES;
    x.a.m.sav_focus = JP_SAV_LOAD;
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.dialog, JP_DLG_INFO);
    CHECK(strstr(x.a.m.dlg_body, x.drop) != NULL);
    CHECK(strstr(x.a.m.dlg_body, "OPTION.bin") != NULL);
    fx_close(&x);
}

static void old_done_folders_are_cleaned_or_named(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "Saves/PES13/Import/done-2026-11-25_1600/.keep", "");   /* only a dot file: not empty for rmdir */
    char empty[400];
    snprintf(empty, sizeof empty, "%s/done-2026-11-25_1500", x.drop);
    mkdir(empty, 0777);
    tmp_put(x.root, "Saves/PES13/Import/done-2026-11-25_1700/OPTION.bin", "copy");
    fx_init(&x);
    CHECK(!exists(x.root, "Saves/PES13/Import/done-2026-11-25_1500"));      /* empty: removed */
    CHECK(exists(x.root, "Saves/PES13/Import/done-2026-11-25_1700/OPTION.bin"));   /* never deleted with content */
    CHECK(strstr(x.a.m.import_old, "Import/done-") != NULL);
    CHECK_EQ(x.a.m.import_files, 0);         /* files in done- folders are not imported again */
    fx_close(&x);
}

static void backups_keep_last_ten(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);              /* PATCH */
    for (int i = 0; i < 13; i++) {
        char v[32];
        snprintf(v, sizeof v, "teams %d", i);   /* the saves change every match */
        tmp_put(x.root, LIVE "/OPTION.bin", v);
        fx_init(&x);
        press(&x, JKPRO_PAD_CROSS);          /* PLAY */
        CHECK(x.a.done);
    }
    char bdir[400];
    snprintf(bdir, sizeof bdir, "%s/backup/saves/PES13", x.sys);
    CHECK_EQ(count_dirs(bdir), JKPRO_BACKUPS_KEPT);
    fx_close(&x);
}

static void export_goes_to_root_export_without_usb(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, LIVE "/OPTION.bin", "teams");
    fx_init(&x);
    x.a.m.screen = JP_SCR_SAVES;
    x.a.m.sav_focus = JP_SAV_EXPORT;
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.sav_msg_bad, 0);
    char edir[400];
    snprintf(edir, sizeof edir, "%s/Export", x.root);
    CHECK_EQ(count_dirs(edir), 1);
    press(&x, JKPRO_PAD_CROSS);              /* never overwrites: a second folder */
    CHECK_EQ(count_dirs(edir), 2);
    fx_close(&x);
}

static void help_and_about_and_circle_on_shelf_does_nothing(void)
{
    Fx x;
    fx_open(&x, 1);
    fx_init(&x);
    press(&x, JKPRO_PAD_CIRCLE);
    CHECK_EQ(x.a.m.screen, JP_SCR_SHELF);
    CHECK(!x.a.done);
    press(&x, JKPRO_PAD_SQUARE);
    CHECK_EQ(x.a.m.screen, JP_SCR_HELP);
    press(&x, JKPRO_PAD_RIGHT);
    CHECK_EQ(x.a.m.help_page, 1);
    press(&x, JKPRO_PAD_LEFT); press(&x, JKPRO_PAD_LEFT);
    CHECK_EQ(x.a.m.help_page, JP_HELP_COUNT - 1);
    press(&x, JKPRO_PAD_CIRCLE);
    CHECK_EQ(x.a.m.screen, JP_SCR_SHELF);
    fx_close(&x);
}

static void start_failure_opens_didnt_start_then_try_again(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);              /* PATCH, done */
    CHECK_EQ(x.a.m.phase, JP_PH_DONE);
    x.s.start_failure = "The game stopped while starting, 3 times in a row.";
    fx_init(&x);
    CHECK_EQ(x.a.m.screen, JP_SCR_FAILED);
    CHECK(strstr(x.a.m.failure, "3 times") != NULL);
    CHECK(!x.a.done);                        /* never starts by itself */
    press(&x, JKPRO_PAD_SQUARE);
    CHECK_EQ(x.a.m.screen, JP_SCR_HELP);
    CHECK_EQ(x.a.m.help_page, JP_HELP_TROUBLE);
    press(&x, JKPRO_PAD_CIRCLE);
    CHECK_EQ(x.a.m.screen, JP_SCR_FAILED);
    press(&x, JKPRO_PAD_CROSS);              /* Try again = PLAY */
    CHECK(x.a.done);
    CHECK_EQ(x.a.decision.action, JKPRO_START);
    fx_init(&x);
    press(&x, JKPRO_PAD_CIRCLE);             /* ○ = the shelf, nothing starts */
    CHECK_EQ(x.a.m.screen, JP_SCR_SHELF);
    CHECK(!x.a.done);
    x.s.start_failure = "";
    fx_init(&x);
    CHECK_EQ(x.a.m.screen, JP_SCR_SHELF);
    fx_close(&x);
}

static void support_bundle_has_logs_but_no_saves_or_reg(void)
{
    Fx x;
    fx_open(&x, 1);
    char logs[300];
    snprintf(logs, sizeof logs, "%s/Logs", x.root);
    tmp_put(x.root, "Logs/session-1.log", "wine says hello");
    tmp_put(x.root, "PES13/pes2013_d3d9.log", "client log");
    tmp_put(x.root, "PES13/PES_2013_Registry_Fix.reg", REG_OK);
    tmp_put(x.root, LIVE "/OPTION.bin", "teams");
    x.s.logs_dir = logs;
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK_EQ(m->can_bundle, 1);
    press(&x, JKPRO_PAD_CROSS);              /* PATCH, so patch.txt exists */
    press(&x, JKPRO_PAD_CIRCLE);
    press(&x, JKPRO_PAD_TRIANGLE); m->menu_focus = JP_MENU_SETTINGS; press(&x, JKPRO_PAD_CROSS);
    m->set_focus = JP_SET_LOGS; press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->screen, JP_SCR_LOGS);
    press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(m->logs_focus, 1);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->bundle_bad, 0);
    CHECK(strstr(m->bundle_msg, "/Logs/support/") != NULL);
    char sup[400], buf[4096];
    snprintf(sup, sizeof sup, "%s/Logs/support", x.root);
    CHECK_EQ(count_dirs(sup), 1);
    buf[0] = 0;
    DIR *d = opendir(sup);
    struct dirent *e;
    while (d && (e = readdir(d)))
        if (e->d_name[0] != '.') {
            char rel[300];
            snprintf(rel, sizeof rel, "Logs/support/%s/MANIFEST.txt", e->d_name);
            CHECK(tmp_get(x.root, rel, buf, sizeof buf) >= 0);
        }
    if (d) closedir(d);
    CHECK(strstr(buf, "JKPro support bundle") != NULL);
    CHECK(strstr(buf, "session-1.log") != NULL);
    CHECK(strstr(buf, "patch.txt") != NULL);
    CHECK(strstr(buf, "pes2013_d3d9.log") != NULL);
    CHECK(strstr(buf, "OPTION.bin") == NULL);
    CHECK(strstr(buf, "Registry_Fix") == NULL);
    /* the didn't-start screen: □ saves a bundle too */
    x.s.start_failure = "The game stopped while starting.";
    fx_init(&x);
    press(&x, JKPRO_PAD_SQUARE);
    CHECK_EQ(x.a.m.screen, JP_SCR_FAILED);
    CHECK_EQ(x.a.m.bundle_bad, 0);
    CHECK_EQ(count_dirs(sup), 2);
    fx_close(&x);
}

static void crash_emptied_registry_is_restored(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    tmp_put(x.root, "System/prefix/user.reg", STORE);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);              /* PATCH: keeps the good copy */
    CHECK_EQ(x.a.m.phase, JP_PH_DONE);
    CHECK(exists(x.root, "System/backup/registry/system.reg"));
    CHECK(exists(x.root, "System/backup/registry/user.reg"));
    /* a panic leaves both at 0 bytes */
    tmp_put(x.root, "System/prefix/system.reg", "");
    tmp_put(x.root, "System/prefix/user.reg", "");
    fx_init(&x);
    CHECK_EQ(x.a.m.dialog, JP_DLG_INFO);
    CHECK(!strcmp(x.a.m.dlg_code, "E610"));
    char buf[8192];
    tmp_get(x.root, "System/prefix/system.reg", buf, sizeof buf);
    CHECK(strstr(buf, "WINE REGISTRY Version 2") == buf);
    CHECK(strstr(buf, "\"code\"=") != NULL);   /* the imported key came back with it */
    press(&x, JKPRO_PAD_CROSS);              /* close the note */
    press(&x, JKPRO_PAD_CROSS);              /* PLAY works */
    CHECK(x.a.done);
    /* damaged with no good copy: the note at open, and PLAY is blocked with E611 */
    tmp_put(x.root, "System/prefix/user.reg", "");
    char keep[400];
    snprintf(keep, sizeof keep, "%s/System/backup/registry/user.reg", x.root);
    remove(keep);
    fx_init(&x);
    CHECK(!strcmp(x.a.m.dlg_code, "E611"));
    press(&x, JKPRO_PAD_CROSS);
    press(&x, JKPRO_PAD_CROSS);              /* PLAY */
    CHECK(!x.a.done);
    CHECK(!strcmp(x.a.m.dlg_code, "E611"));
    fx_close(&x);
}

static void docs_dir_holds_saves_and_old_saves_are_copied_once(void)
{
    Fx x;
    fx_open(&x, 1);
    char docs[300];
    snprintf(docs, sizeof docs, "%s/Saves/PES13", x.root);
    tmp_put(x.root, LIVE "/OPTION.bin", "teams from the prefix");
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    x.s.docs_dir = docs;
    fx_init(&x);
    CHECK_EQ(x.a.m.dialog, JP_DLG_INFO);     /* "Saves have a new home" */
    char buf[256];
    CHECK(tmp_get(x.root, "Saves/PES13/" JKPRO_DOCS_SUB "/save/OPTION.bin", buf, sizeof buf) >= 0);
    CHECK(!strcmp(buf, "teams from the prefix"));
    CHECK(exists(x.root, LIVE "/OPTION.bin"));          /* the old copy is kept */
    CHECK_EQ(x.a.m.live_files, 1);
    press(&x, JKPRO_PAD_CROSS);              /* close the note */
    press(&x, JKPRO_PAD_CROSS);              /* PATCH: settings.dat goes to the new Documents */
    CHECK_EQ(x.a.m.phase, JP_PH_DONE);
    CHECK(exists(x.root, "Saves/PES13/" JKPRO_DOCS_SUB "/settings.dat"));
    /* the next open copies nothing again */
    tmp_put(x.root, "Saves/PES13/" JKPRO_DOCS_SUB "/save/OPTION.bin", "newer teams");
    fx_init(&x);
    CHECK_EQ(x.a.m.dialog, JP_DLG_NONE);
    tmp_get(x.root, "Saves/PES13/" JKPRO_DOCS_SUB "/save/OPTION.bin", buf, sizeof buf);
    CHECK(!strcmp(buf, "newer teams"));
    /* the drop folder sits beside the KONAMI tree and is not a save */
    tmp_put(x.root, "Saves/PES13/Import/EDIT.bin", "edit");
    fx_init(&x);
    CHECK_EQ(x.a.m.import_files, 1);
    CHECK_EQ(x.a.m.live_files, 1);
    fx_close(&x);
}

static void pe_flag_switches_the_client_and_asks_for_patch_again(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    tmp_put(x.root, "System/jkpro-files/d3d9-pe.dll", "OUR-PE-CLIENT");
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);              /* PATCH: native */
    CHECK_EQ(x.a.m.phase, JP_PH_DONE);
    char buf[256];
    tmp_get(x.root, "PES13/d3d9.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "OUR-D3D9-CLIENT"));
    /* support turns the PE fallback on: PATCH again, then the PE client is d3d9.dll */
    tmp_put(x.root, "System/renderer-pe", "");
    fx_init(&x);
    CHECK_EQ(x.a.m.tiles[0].state, JP_ST_PATCH_AGAIN);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.phase, JP_PH_DONE);
    tmp_get(x.root, "PES13/d3d9.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "OUR-PE-CLIENT"));
    char bdir[400];
    snprintf(bdir, sizeof bdir, "%s/backup/PES13", x.sys);
    CHECK_EQ(count_dirs(bdir), 0);           /* our own client is never "backed up" */
    /* and back to native */
    char flag[400];
    snprintf(flag, sizeof flag, "%s/renderer-pe", x.sys);
    remove(flag);
    fx_init(&x);
    CHECK_EQ(x.a.m.tiles[0].state, JP_ST_PATCH_AGAIN);
    /* the flag set but the package has no PE client: E301 naming the file */
    tmp_put(x.root, "System/renderer-pe", "");
    snprintf(flag, sizeof flag, "%s/jkpro-files/d3d9-pe.dll", x.sys);
    remove(flag);
    fx_init(&x);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.phase, JP_PH_FAILED);
    CHECK(!strcmp(x.a.m.dlg_code, "E301"));
    CHECK(strstr(x.a.m.dlg_body, "d3d9-pe.dll") != NULL);
    fx_close(&x);
}

static void release_state_never_blocks_play(void)
{
    static JkRelease rel[3];
    rel[0] = (JkRelease){ JK_REL_UNOFFICIAL, NULL, NULL, NULL, "The signature does not match.", 0 };
    rel[1] = (JkRelease){ JK_REL_OFFICIAL, "1.0.0", "JKPro release key 2026", "4A1F2B3C", NULL, 0 };
    rel[2] = (JkRelease){ JK_REL_CHANGED, "1.0.0", "JKPro release key 2026", "4A1F2B3C", NULL, 2 };
    for (int k = -1; k < 3; k++) {
        Fx x;
        fx_open(&x, 1);
        tmp_put(x.root, "PES13/fix.reg", REG_OK);
        x.s.release = k < 0 ? NULL : &rel[k];
        x.s.build_id = "JKPro 1.0";
        fx_init(&x);
        char badge[160];
        jkpro_badge_text(&x.a.m, badge, sizeof badge);
        if (k == 1) CHECK(!strcmp(badge, "Official Build by XxXJoker13XxX  \xC2\xB7  JKPro 1.0  \xC2\xB7  key 4A1F2B3C"));
        else CHECK(!strcmp(badge, "Unofficial build"));
        /* settings change, PATCH and PLAY work the same in every state */
        press(&x, JKPRO_PAD_TRIANGLE); press(&x, JKPRO_PAD_DOWN); press(&x, JKPRO_PAD_CROSS);
        CHECK_EQ(x.a.m.screen, JP_SCR_SETTINGS);
        press(&x, JKPRO_PAD_CROSS);
        CHECK_EQ(x.a.m.show_fps, 1);
        press(&x, JKPRO_PAD_CIRCLE); press(&x, JKPRO_PAD_CIRCLE);
        press(&x, JKPRO_PAD_CROSS);          /* PATCH */
        CHECK_EQ(x.a.m.phase, JP_PH_DONE);
        press(&x, JKPRO_PAD_CROSS);          /* PLAY */
        CHECK(x.a.done);
        CHECK_EQ(x.a.decision.action, JKPRO_START);
        fx_close(&x);
    }
}

static void help_names_the_real_paths(void)
{
    Fx x;
    fx_open(&x, 1);
    fx_init(&x);
    static char t[8192];
    jkpro_help_lines(&x.a.m, JP_HELP_GAME, t, sizeof t);
    char want[600];
    snprintf(want, sizeof want, "%s/pes2013.exe", x.game);
    CHECK(strstr(t, want) != NULL);
    CHECK(strstr(t, "settings.exe") != NULL && strstr(t, "/img/") != NULL && strstr(t, "Kitserver is optional") != NULL);
    jkpro_help_lines(&x.a.m, JP_HELP_REG, t, sizeof t);
    snprintf(want, sizeof want, "%s/PES_2013_Registry_Fix.reg", x.game);
    CHECK(strstr(t, want) != NULL);
    CHECK(strstr(t, "not been installed") != NULL && strstr(t, "Any name ending in .reg") != NULL);
    jkpro_help_lines(&x.a.m, JP_HELP_SAVES, t, sizeof t);
    CHECK(strstr(t, x.drop) != NULL && strstr(t, "EDIT.bin = option file") != NULL);
    jkpro_help_lines(&x.a.m, JP_HELP_PACKS, t, sizeof t);
    CHECK(strstr(t, "dt0f.img") != NULL);
    jkpro_help_lines(&x.a.m, JP_HELP_USB, t, sizeof t);
    CHECK(strstr(t, "USB:/JKPro/saves/") != NULL);
    jkpro_help_lines(&x.a.m, JP_HELP_SUPPORT, t, sizeof t);
    snprintf(want, sizeof want, "%s/Logs/support/", x.root);
    CHECK(strstr(t, want) != NULL);
    /* no page names a data folder that is not this console's own */
    for (int p = 0; p < JP_HELP_COUNT; p++) {
        jkpro_help_lines(&x.a.m, p, t, sizeof t);
        CHECK(strstr(t, "/data/JKPRO") == NULL && strstr(t, "/data/JKPro") == NULL);
    }
    fx_close(&x);
}

static void check_game_files_is_one_state_with_exact_dialogs(void)
{
    Fx x;
    fx_open(&x, 1);
    char p[400];
    snprintf(p, sizeof p, "%s/img/dt01.img", x.game);
    remove(p);
    fx_init(&x);
    CHECK_EQ(x.a.m.tiles[0].state, JP_ST_MISSING);
    CHECK(!strcmp(jkpro_state_text(x.a.m.tiles[0].state), "Check game files"));
    CHECK(!strcmp(jkpro_state_text(JP_ST_UNSUPPORTED), "Check game files"));
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(x.a.m.dialog, JP_DLG_ERROR);
    CHECK(!strcmp(x.a.m.dlg_code, "E202"));
    CHECK(strstr(x.a.m.dlg_body, "dt01.img is missing in") != NULL);
    CHECK(strstr(x.a.m.dlg_body, x.game) != NULL);
    CHECK(strstr(x.a.m.dlg_body, "Copy the whole PC game folder again") != NULL);
    /* an unsupported exe: the dialog says so, E205 */
    x.a.m.dialog = JP_DLG_NONE;
    x.a.m.tiles[0].state = JP_ST_UNSUPPORTED;
    press(&x, JKPRO_PAD_CROSS);
    CHECK(!strcmp(x.a.m.dlg_code, "E205"));
    CHECK(strstr(x.a.m.dlg_body, "this pes2013.exe version is not supported. Use the PC version 1.04 exe.") != NULL);
    /* Help page 1 explains the red tile */
    static char t[8192];
    jkpro_help_lines(&x.a.m, JP_HELP_GAME, t, sizeof t);
    CHECK(strstr(t, "Check game files") != NULL && strstr(t, "version 1.04") != NULL);
    fx_close(&x);
}

/* A stand-in for jkpro_fservfix (its own tests use the real fserv.dll): the
 * file holds "ORIG" (the known original), "FIXED" or anything else. */
static int ff_err;
static int ff_read(const char *dir, char *buf, size_t cap)
{
    char p[600];
    snprintf(p, sizeof p, "%s/fserv.dll", dir);
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, cap - 1, f);
    buf[n] = 0;
    fclose(f);
    return 0;
}
static void ff_write(const char *dir, const char *name, const char *text)
{
    char p[600];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "wb");
    fputs(text, f);
    fclose(f);
}
static int ff_state(const char *dir)
{
    char b[64];
    if (ff_read(dir, b, sizeof b) != 0) return JKPRO_FSERVFIX_NONE;
    return !strcmp(b, "FIXED") ? JKPRO_FSERVFIX_PATCHED : !strcmp(b, "ORIG") ? JKPRO_FSERVFIX_ALREADY : JKPRO_FSERVFIX_OTHER;
}
static int ff_apply(const char *dir)
{
    if (ff_err) return -5;
    int st = ff_state(dir);
    if (st == JKPRO_FSERVFIX_PATCHED) return JKPRO_FSERVFIX_ALREADY;
    if (st != JKPRO_FSERVFIX_ALREADY) return st;
    char bk[600];
    snprintf(bk, sizeof bk, "%s/" JKPRO_FSERV_BACKUP, dir);
    if (!jk_fs_exists(bk)) ff_write(dir, JKPRO_FSERV_BACKUP, "ORIG");
    ff_write(dir, "fserv.dll", "FIXED");
    return JKPRO_FSERVFIX_PATCHED;
}
static int ff_revert(const char *dir)
{
    if (ff_err) return -5;
    int st = ff_state(dir);
    if (st == JKPRO_FSERVFIX_ALREADY) return JKPRO_FSERVFIX_ALREADY;
    if (st != JKPRO_FSERVFIX_PATCHED) return st;
    ff_write(dir, "fserv.dll", "ORIG");
    return JKPRO_FSERVFIX_PATCHED;
}

static void kitserver_face_fix_in_settings(void)
{
    jkpro_fservfix_fns_for_tests(ff_apply, ff_revert, ff_state);
    ff_err = 0;
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    tmp_put(x.root, "PES13/kitserver/kserv.dll", "PLAYERS-KSERV");
    /* no fserv.dll: no row */
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK(!m->face_avail);
    m->screen = JP_SCR_SETTINGS; m->set_focus = JP_SET_FPS;
    press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(m->set_focus, JP_SET_LOGS);
    /* the player's original fserv.dll: the row says Off (the default) */
    tmp_put(x.root, "PES13/kitserver/fserv.dll", "ORIG");
    fx_init(&x);
    CHECK(m->face_avail);
    CHECK_EQ(m->face_state, JKPRO_FSERVFIX_ALREADY);
    CHECK_EQ(m->face_set, 0);
    char buf[2048];
    /* PATCH leaves every Kitserver file alone */
    press(&x, JKPRO_PAD_TRIANGLE);
    m->menu_focus = JP_MENU_PATCH;
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->phase, JP_PH_DONE);
    tmp_get(x.root, "PES13/kitserver/fserv.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "ORIG"));
    tmp_get(x.root, "PES13/kitserver/kserv.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "PLAYERS-KSERV"));
    CHECK(!exists(x.root, "PES13/kitserver/" JKPRO_FSERV_BACKUP));
    /* turn it on: patched in place, the original kept next to it, saved */
    m->screen = JP_SCR_SETTINGS; m->set_focus = JP_SET_FPS;
    press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(m->set_focus, JP_SET_FACEFIX);
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->face_set, 1);
    CHECK_EQ(m->face_state, JKPRO_FSERVFIX_PATCHED);
    tmp_get(x.root, "PES13/kitserver/fserv.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "FIXED"));
    tmp_get(x.root, "PES13/kitserver/" JKPRO_FSERV_BACKUP, buf, sizeof buf);
    CHECK(!strcmp(buf, "ORIG"));
    CHECK(strstr(m->set_msg, "Face fix on") != NULL);
    CHECK(tmp_get(x.root, "System/games/PES13/settings.ini", buf, sizeof buf) >= 0);
    CHECK(strstr(buf, "fservfix = on") != NULL || strstr(buf, "fservfix=on") != NULL);
    /* the player copies the original back: the row shows Off, PLAY turns it on again */
    tmp_put(x.root, "PES13/kitserver/fserv.dll", "ORIG");
    fx_init(&x);
    CHECK_EQ(m->face_state, JKPRO_FSERVFIX_ALREADY);
    CHECK_EQ(m->face_set, 1);
    press(&x, JKPRO_PAD_CROSS);                       /* PLAY (ready) */
    CHECK(x.a.done);
    tmp_get(x.root, "PES13/kitserver/fserv.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "FIXED"));
    /* turn it off: the original back */
    fx_init(&x);
    CHECK_EQ(m->face_state, JKPRO_FSERVFIX_PATCHED);
    m->screen = JP_SCR_SETTINGS; m->set_focus = JP_SET_FACEFIX;
    press(&x, JKPRO_PAD_LEFT);
    CHECK_EQ(m->face_set, 0);
    CHECK_EQ(m->face_state, JKPRO_FSERVFIX_ALREADY);
    tmp_get(x.root, "PES13/kitserver/fserv.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "ORIG"));
    CHECK(strstr(m->set_msg, "Face fix off") != NULL);
    /* another fserv.dll version: left alone, the row says so */
    tmp_put(x.root, "PES13/kitserver/fserv.dll", "SOMEONE-ELSES");
    fx_init(&x);
    CHECK_EQ(m->face_state, JKPRO_FSERVFIX_OTHER);
    m->screen = JP_SCR_SETTINGS; m->set_focus = JP_SET_FACEFIX;
    press(&x, JKPRO_PAD_CROSS);
    tmp_get(x.root, "PES13/kitserver/fserv.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "SOMEONE-ELSES"));
    CHECK(strstr(m->set_msg, "not the version JKPro knows") != NULL);
    /* an error before PLAY: logged, the game still starts, the file untouched */
    tmp_put(x.root, "PES13/kitserver/fserv.dll", "ORIG");
    fx_init(&x);
    m->face_set = 1;
    ff_err = 1;
    press(&x, JKPRO_PAD_CROSS);
    CHECK(x.a.done);
    tmp_get(x.root, "PES13/kitserver/fserv.dll", buf, sizeof buf);
    CHECK(!strcmp(buf, "ORIG"));
    ff_err = 0;
    /* Help page 1 points to the switch */
    static char t[8192];
    jkpro_help_lines(m, JP_HELP_GAME, t, sizeof t);
    CHECK(strstr(t, "SETTINGS > Kitserver face fix") != NULL);
    jkpro_fservfix_fns_for_tests(NULL, NULL, NULL);
    fx_close(&x);
}

static void kitserver_switch_in_settings(void)
{
    static const char *CFG = "\xEF\xBB\xBF[kload]\r\ndll = kserv\r\ndll = afs2fs\r\n";
    static const char *NOK = "\xEF\xBB\xBF[kload]\r\n; dll = kserv\r\ndll = afs2fs\r\n";
    static const char *OFFC = "\xEF\xBB\xBF[kload]\r\n; dll = kserv\r\n; dll = afs2fs\r\n";
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES13/fix.reg", REG_OK);
    /* no kitserver/config.txt: no row (DOWN from Show FPS goes to Logs) */
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK(!m->kit_avail);
    m->screen = JP_SCR_SETTINGS; m->set_focus = JP_SET_FPS;
    press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(m->set_focus, JP_SET_LOGS);
    /* with it: On, the row between Show FPS and Logs */
    tmp_put(x.root, "PES13/kitserver/config.txt", CFG);
    fx_init(&x);
    CHECK(m->kit_avail);
    CHECK_EQ(m->kit_mode, JK_KIT_ON);
    m->screen = JP_SCR_SETTINGS; m->set_focus = JP_SET_FPS;
    press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(m->set_focus, JP_SET_KITSERVER);
    press(&x, JKPRO_PAD_RIGHT);                     /* Without kits */
    CHECK_EQ(m->kit_mode, JK_KIT_NOKITS);
    char buf[2048];
    CHECK(tmp_get(x.root, "System/games/PES13/settings.ini", buf, sizeof buf) >= 0);
    CHECK(strstr(buf, "kitserver = nokits") != NULL || strstr(buf, "kitserver=nokits") != NULL);
    CHECK(tmp_get(x.root, "PES13/kitserver/config.txt", buf, sizeof buf) >= 0 && !strcmp(buf, NOK));
    CHECK(tmp_get(x.root, "System/backup/kitserver/config.txt.original", buf, sizeof buf) >= 0 && !strcmp(buf, CFG));
    CHECK(strstr(m->set_msg, "Saved") != NULL);
    press(&x, JKPRO_PAD_CROSS);                     /* Off */
    CHECK_EQ(m->kit_mode, JK_KIT_OFF);
    CHECK(tmp_get(x.root, "PES13/kitserver/config.txt", buf, sizeof buf) >= 0 && !strcmp(buf, OFFC));
    press(&x, JKPRO_PAD_RIGHT);                     /* back to On: the original bytes */
    CHECK_EQ(m->kit_mode, JK_KIT_ON);
    CHECK(tmp_get(x.root, "PES13/kitserver/config.txt", buf, sizeof buf) >= 0 && !strcmp(buf, CFG));
    press(&x, JKPRO_PAD_LEFT);                      /* LEFT: On -> Off */
    CHECK_EQ(m->kit_mode, JK_KIT_OFF);
    press(&x, JKPRO_PAD_LEFT);                      /* Off -> Without kits */
    CHECK_EQ(m->kit_mode, JK_KIT_NOKITS);
    /* the player copies the kitserver folder again: SETTINGS shows what the file is now, PLAY applies the setting */
    tmp_put(x.root, "PES13/kitserver/config.txt", CFG);
    fx_init(&x);
    CHECK_EQ(m->kit_mode, JK_KIT_ON);
    CHECK_EQ(m->kit_set, JK_KIT_NOKITS);
    press(&x, JKPRO_PAD_CROSS);                     /* PATCH */
    press(&x, JKPRO_PAD_CROSS);                     /* PLAY */
    CHECK(x.a.done);
    CHECK(tmp_get(x.root, "PES13/kitserver/config.txt", buf, sizeof buf) >= 0 && !strcmp(buf, NOK));
    CHECK(tmp_get(x.root, "System/backup/kitserver/config.txt.original", buf, sizeof buf) >= 0 && !strcmp(buf, CFG));
    /* Help page 1 says where the switch is */
    static char t[8192];
    jkpro_help_lines(m, JP_HELP_GAME, t, sizeof t);
    CHECK(strstr(t, "Without kits or Kitserver Off are other ways out.") != NULL);   /* whole: not cut */
    fx_close(&x);
}

static void every_screen_draws(void)
{
    Fx x;
    fx_open(&x, 1);
    tmp_put(x.root, "PES6/PES6.exe", "MZ");
    fx_init(&x);
    JkFrame f = { calloc((size_t)JK_W * JK_H, 4), JK_W, JK_H, JK_W * 4 };
    JpModel m = x.a.m;
    snprintf(m.failure, sizeof m.failure, "The game stopped while starting.");
    for (int s = JP_SCR_SHELF; s <= JP_SCR_FAILED; s++) {
        m.screen = (JpScreen)s;
        for (int d = JP_DLG_NONE; d <= JP_DLG_NO_BACKUP; d++) {
            m.dialog = (JpDialog)d;
            jkpro_draw(&f, &m);
        }
    }
    /* something was drawn in the safe area */
    uint32_t px = ((uint32_t *)f.pixels)[540 * JK_W + 960];
    CHECK(px != 0);
    free(f.pixels);
    jkpro_draw_release();
    fx_close(&x);
}

int main(void)
{
    RUN(shelf_shows_only_the_pes13_folder);
    RUN(no_game_shows_where_to_put_it);
    RUN(cross_on_unpatched_runs_patch_and_stops_at_missing_reg);
    RUN(reg_without_code_is_e211);
    RUN(patch_completes_then_cross_plays_with_settings);
    RUN(reopened_patched_game_is_ready_and_changed_d3d9_needs_patch_again);
    RUN(triangle_menu_and_settings_are_locked_to_1080_high);
    RUN(logs_setting_writes_flag_file);
    RUN(import_loads_saves_with_backup_and_moves_drop);
    RUN(saves_screen_names_the_real_paths_and_warns_on_a_wrong_name);
    RUN(old_done_folders_are_cleaned_or_named);
    RUN(backups_keep_last_ten);
    RUN(restore_any_backup_and_no_duplicates);
    RUN(export_goes_to_root_export_without_usb);
    RUN(help_and_about_and_circle_on_shelf_does_nothing);
    RUN(start_failure_opens_didnt_start_then_try_again);
    RUN(support_bundle_has_logs_but_no_saves_or_reg);
    RUN(crash_emptied_registry_is_restored);
    RUN(docs_dir_holds_saves_and_old_saves_are_copied_once);
    RUN(pe_flag_switches_the_client_and_asks_for_patch_again);
    RUN(release_state_never_blocks_play);
    RUN(help_names_the_real_paths);
    RUN(check_game_files_is_one_state_with_exact_dialogs);
    RUN(kitserver_face_fix_in_settings);
    RUN(kitserver_switch_in_settings);
    RUN(every_screen_draws);
    return jk_test_report("test_jkpro_ui");
}
