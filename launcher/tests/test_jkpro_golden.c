/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * JKPro golden images: every screen and state is rendered and its pixel hash
 * compared with launcher/golden/jkpro-goldens.txt. Previews always go to
 * build/preview/jkpro-<name>.png. JK_UPDATE_GOLDEN=1 rewrites the list (after
 * a deliberate change only).
 */
#include "jkpro_ui.h"
#include "jkpro_loader.h"
#include "jk_png.h"
#include "jk_sha256.h"
#include "jk_test.h"

#include <stdlib.h>
#include <sys/stat.h>

#define GOLDENS JK_ROOT_DIR "/golden/jkpro-goldens.txt"
#define PREVIEW JK_ROOT_DIR "/build/preview"

static JpTile T(JpTileState s, int modded, const char *folder, const char *title)
{
    JpTile t;
    memset(&t, 0, sizeof t);
    t.state = s; t.modded = modded;
    snprintf(t.folder, sizeof t.folder, "%s", folder);
    snprintf(t.title, sizeof t.title, "%s", title);
    return t;
}

static JpModel base(void)
{
    JpModel m;
    memset(&m, 0, sizeof m);
    m.tiles[0] = T(JP_ST_READY, 0, "PES13", "PES 2013");
    m.ntiles = 1;
    m.game_tile = 0;
    snprintf(m.version, sizeof m.version, "1.4.0.0");
    m.quality = 2;
    m.res = 1;
    m.live_files = 3;
    m.backups = 4;
    snprintf(m.last_backup, sizeof m.last_backup, "2026-11-25_1802");
    m.can_bundle = 1;
    snprintf(m.build_id, sizeof m.build_id, "JKPro 1.0");
    snprintf(m.root_path, sizeof m.root_path, "/data/JkPro");
    snprintf(m.game_path, sizeof m.game_path, "/data/JkPro/PES13");
    snprintf(m.logs_path, sizeof m.logs_path, "/data/JkPro/Logs");
    snprintf(m.online_file, sizeof m.online_file, "/data/JkPro/System/online-account");
    snprintf(m.sav_drop, sizeof m.sav_drop, "/data/JkPro/Saves/PES13/Import/");
    snprintf(m.sav_live, sizeof m.sav_live, "/data/JkPro/Saves/PES13/KONAMI/Pro Evolution Soccer 2013/save/");
    snprintf(m.sav_bak, sizeof m.sav_bak, "/data/JkPro/System/backup/saves/PES13/");
    return m;
}

typedef struct { char name[48]; JpModel m; } Case;

static int cases(Case *c)
{
    int n = 0;
    JpModel m;

    m = base();
    snprintf(c[n].name, sizeof c[n].name, "shelf-ready"); c[n++].m = m;

    m = base();
    m.tiles[0].state = JP_ST_NEEDS_PATCH;
    m.backups = 0; m.last_backup[0] = 0;
    snprintf(c[n].name, sizeof c[n].name, "shelf-needs-patch"); c[n++].m = m;

    m = base();
    m.ntiles = 0; m.game_tile = -1;
    snprintf(c[n].name, sizeof c[n].name, "shelf-no-game"); c[n++].m = m;

    m = base();
    m.tiles[0].state = JP_ST_PATCH_AGAIN; m.tiles[0].modded = 1;
    snprintf(c[n].name, sizeof c[n].name, "shelf-patch-again"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_DATAPACKS;
    m.dp_on = 1; m.dp_have = 1; m.dp_size = 327155712;
    snprintf(m.dp_ver, sizeof m.dp_ver, "6.00");
    snprintf(m.dp_live, sizeof m.dp_live, "/data/JkPro/DataPacks/PES13/KONAMI/Pro Evolution Soccer 2013/download/dt0f.img");
    snprintf(m.dp_olddir, sizeof m.dp_olddir, "/data/JkPro/DataPacks/PES13/old/");
    m.dp_wait = 1; m.dp_wait_size = 340787200;
    snprintf(m.dp_wait_name, sizeof m.dp_wait_name, "dt0f.img");
    snprintf(m.dp_wait_ver, sizeof m.dp_wait_ver, "7.00");
    m.dp_nold = 1; m.dp_old[0].size = 314572800;
    snprintf(m.dp_old[0].name, sizeof m.dp_old[0].name, "dt0f-5.00-2026-11-25_1700.img");
    snprintf(m.dp_old[0].ver, sizeof m.dp_old[0].ver, "5.00");
    snprintf(c[n].name, sizeof c[n].name, "datapacks"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_MENU; m.menu_focus = JP_MENU_PLAY; m.dp_on = 1;
    snprintf(c[n].name, sizeof c[n].name, "menu"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_PATCH; m.phase = JP_PH_RUNNING;
    m.steps[0] = m.steps[1] = JP_SS_OK; m.steps[2] = JP_SS_NOW; m.step_now = 2;
    m.bytes_total = 4200000; m.bytes_done = 1900000;
    snprintf(c[n].name, sizeof c[n].name, "patch-running"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_PATCH; m.phase = JP_PH_FAILED;
    m.steps[0] = m.steps[1] = m.steps[2] = JP_SS_OK; m.steps[3] = JP_SS_FAIL; m.step_now = 3;
    m.bytes_total = m.bytes_done = 4200000;
    m.dialog = JP_DLG_ERROR;
    snprintf(m.dlg_code, sizeof m.dlg_code, "E210");
    snprintf(m.dlg_title, sizeof m.dlg_title, "Registry file needed");
    snprintf(m.dlg_body, sizeof m.dlg_body, "PES 2013 needs the registry file (.reg) from your PC. Without it, the game "
             "says it has not been installed. Put the .reg file in /data/JkPro/PES13/ and press PATCH again. Steps 1-3 "
             "are done and stay done.");
    snprintf(c[n].name, sizeof c[n].name, "patch-e210"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_PATCH; m.phase = JP_PH_DONE;
    for (int i = 0; i < JP_STEP_COUNT; i++) m.steps[i] = JP_SS_OK;
    m.step_now = JP_STEP_RECORD;
    m.bytes_total = m.bytes_done = 4200000;
    snprintf(m.reg_used, sizeof m.reg_used, "PES_2013_Registry_Fix.reg");
    snprintf(c[n].name, sizeof c[n].name, "patch-done"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_SETTINGS; m.set_focus = JP_SET_FPS; m.res = 1;
    snprintf(c[n].name, sizeof c[n].name, "settings"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_SETTINGS; m.set_focus = JP_SET_KITSERVER; m.res = 1;
    m.kit_avail = 1; m.kit_mode = m.kit_set = 1;    /* Without kits */
    m.face_avail = 1; m.face_state = 1; m.face_set = 0;   /* face fix off (the original) */
    snprintf(c[n].name, sizeof c[n].name, "settings-kitserver"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_SETTINGS; m.set_focus = JP_SET_FACEFIX; m.res = 1;
    m.kit_avail = 1; m.kit_mode = m.kit_set = 0;
    m.face_avail = 1; m.face_state = 0; m.face_set = 1;   /* face fix on */
    snprintf(c[n].name, sizeof c[n].name, "settings-facefix"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_ONLINE; m.online = 1; m.online_focus = JP_ONL_ID;
    snprintf(m.online_id, sizeof m.online_id, "Joker13");
    snprintf(m.online_pw, sizeof m.online_pw, "secret1");   /* registration code empty: "uses your ID" */
    snprintf(c[n].name, sizeof c[n].name, "online"); c[n++].m = m;

    m.kb_field = JP_ONL_PW;
    jk_keyboard_init(&m.kb, "PASSWORD", "secret1", 63, NULL);
    m.kb.mask = 1;
    snprintf(c[n].name, sizeof c[n].name, "online-password-keyboard"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_SAVES;
    m.import_files = 2;
    snprintf(m.import_list, sizeof m.import_list, "OPTION.bin · PES13_option.bin");
    snprintf(m.import_warn, sizeof m.import_warn,
             "PES13_option.bin: PES 2013 reads EDIT.bin (option file) and OPTION.bin (settings). Rename yours to one of "
             "these (it is copied with its own name).");
    snprintf(c[n].name, sizeof c[n].name, "saves-import-waiting"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_SAVES;
    m.dialog = JP_DLG_LOAD;
    snprintf(m.dlg_title, sizeof m.dlg_title, "Load these saves?");
    snprintf(m.dlg_body, sizeof m.dlg_body, "OPTION.bin · EDIT.bin will replace the saves PES 2013 uses now. Your "
             "current saves are backed up first; you can restore them at any time.");
    snprintf(c[n].name, sizeof c[n].name, "saves-load-dialog"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_SAVES; m.sav_focus = JP_SAV_EXPORT;
    snprintf(m.sav_msg, sizeof m.sav_msg, "Loaded OPTION.bin, EDIT.bin. Import is empty again. Backup of the saves you "
             "had: 2026-11-25_1810.");
    snprintf(c[n].name, sizeof c[n].name, "saves-exported"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_RESTORE;
    {
        static const char *N[5] = { "2026-11-25_1649-2", "2026-11-25_1649", "2026-11-25_1645-2", "2026-11-25_1645",
                                    "2026-11-25_1640" };
        static const char *R[5] = { "before restore", "before match", "before match", "before import", "before match" };
        static const char *F[5] = { "OPTION.bin \xC2\xB7 7.9 MB", "OPTION.bin \xC2\xB7 7.9 MB",
                                    "OPTION.bin \xC2\xB7 402 KB     EDIT.bin \xC2\xB7 1.2 MB", "OPTION.bin \xC2\xB7 402 KB",
                                    "OPTION.bin \xC2\xB7 398 KB" };
        for (int i = 0; i < 5; i++) {
            snprintf(m.bk[i].name, sizeof m.bk[i].name, "%s", N[i]);
            snprintf(m.bk[i].reason, sizeof m.bk[i].reason, "%s", R[i]);
            snprintf(m.bk[i].files, sizeof m.bk[i].files, "%s", F[i]);
        }
        m.nbk = 5;
        m.bk_focus = 3;
    }
    snprintf(c[n].name, sizeof c[n].name, "restore-list"); c[n++].m = m;
    m.dialog = JP_DLG_RESTORE;
    snprintf(m.dlg_title, sizeof m.dlg_title, "Restore this backup?");
    snprintf(m.dlg_body, sizeof m.dlg_body, "Your saves go back to 2026-11-25 16:45 (before import): OPTION.bin "
             "\xC2\xB7 402 KB. They replace what PES 2013 has now: OPTION.bin \xC2\xB7 7.9 MB. That is backed up first "
             "(before restore).");
    snprintf(c[n].name, sizeof c[n].name, "restore-confirm"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_HELP;
    m.dp_on = 1;
    snprintf(m.dp_live, sizeof m.dp_live, "/data/JkPro/DataPacks/PES13/KONAMI/Pro Evolution Soccer 2013/download/dt0f.img");
    snprintf(m.dp_olddir, sizeof m.dp_olddir, "/data/JkPro/DataPacks/PES13/old/");
    for (int pg = 0; pg < JP_HELP_COUNT; pg++) {
        m.help_page = pg;
        snprintf(c[n].name, sizeof c[n].name, "help-%d", pg + 1); c[n++].m = m;
    }

    m = base();
    m.screen = JP_SCR_LOGS;
    snprintf(c[n].name, sizeof c[n].name, "logs"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_FAILED;
    snprintf(m.failure, sizeof m.failure, "The game stopped while starting, 3 times in a row. If you added kitserver "
             "or another mod, start once without it to check.");
    snprintf(c[n].name, sizeof c[n].name, "didnt-start"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_LOGS; m.logs_focus = 1;
    snprintf(m.bundle_msg, sizeof m.bundle_msg, "Saved in /data/JkPro/Logs/support/2026-11-25_1812. It holds logs only: no "
             "saves, no registry file.");
    snprintf(c[n].name, sizeof c[n].name, "logs-bundle-saved"); c[n++].m = m;

    m = base();
    m.rel_state = 1;
    snprintf(m.rel_version, sizeof m.rel_version, "1.0.0");
    snprintf(m.rel_key_label, sizeof m.rel_key_label, "JKPro release key 2026");
    snprintf(m.rel_key_id, sizeof m.rel_key_id, "4A1F2B3C");
    snprintf(c[n].name, sizeof c[n].name, "shelf-official"); c[n++].m = m;
    m.screen = JP_SCR_ABOUT;
    snprintf(c[n].name, sizeof c[n].name, "about-official"); c[n++].m = m;
    m.rel_state = 2; m.rel_changed = 2;
    snprintf(c[n].name, sizeof c[n].name, "about-changed"); c[n++].m = m;

    m = base();
    m.screen = JP_SCR_ABOUT;
    snprintf(c[n].name, sizeof c[n].name, "about"); c[n++].m = m;

    return n;
}

typedef struct { char name[64]; char sha[65]; } Shot;

int main(void)
{
    const char *upd = getenv("JK_UPDATE_GOLDEN");
    int update = upd && !strcmp(upd, "1");
    mkdir(JK_ROOT_DIR "/build", 0755);
    mkdir(PREVIEW, 0755);
    static Shot old[64];
    int n_old = 0;
    FILE *g = fopen(GOLDENS, "r");
    if (g) {
        while (n_old < 64 && fscanf(g, "%63s %64s", old[n_old].name, old[n_old].sha) == 2) n_old++;
        fclose(g);
    }
    JkFrame f = { calloc((size_t)JK_W * JK_H, 4), JK_W, JK_H, JK_W * 4 };
    static Case c[40];
    int nc = cases(c);
    FILE *out = update ? fopen(GOLDENS, "w") : NULL;
    /* the start screen: a touch, a kick-off and the first-start progress */
    static const char *LN[3] = { "loader-touch", "loader-kickoff", "loader-progress" };
    for (int i = 0; i < nc + 3; i++) {
        if (i < nc) jkpro_draw(&f, &c[i].m);
        else if (i == nc) jkpro_loader_draw(&f, 0, NULL);
        else if (i == nc + 1) jkpro_loader_draw(&f, 3150, NULL);
        else jkpro_loader_draw_progress(&f, 1200, "Getting PES 2013 ready", 204, 330, "Setting up the Windows layer (first start only)");
        const char *cname = i < nc ? c[i].name : LN[i - nc];
        char sha[65], png[512];
        jk_sha256_hex(f.pixels, (size_t)JK_W * JK_H * 4, sha);
        snprintf(png, sizeof png, PREVIEW "/jkpro-%s.png", cname);
        jk_png_write(png, &f);
        if (out) fprintf(out, "%s %s\n", cname, sha);
        const char *want = NULL;
        for (int k = 0; k < n_old; k++) if (!strcmp(old[k].name, cname)) want = old[k].sha;
        if (!update) {
            jk_test_checks++;
            if (!want || strcmp(want, sha)) {
                jk_test_failures++;
                fprintf(stderr, "  FAIL golden %s: %s (want %s)\n", cname, sha, want ? want : "none");
            }
        }
    }
    if (out) { fclose(out); fprintf(stderr, "jkpro goldens rewritten: %d\n", nc); }
    free(f.pixels);
    jkpro_draw_release();
    jkpro_loader_release();
    return jk_test_report("test_jkpro_golden");
}
