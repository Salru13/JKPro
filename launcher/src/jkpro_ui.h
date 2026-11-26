/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * The JKPro launcher UI (design/FEATURES.md rev 2, owner 2026-11-25).
 *
 * Home is a shelf of game tiles, so later releases add games without a
 * redesign. JKPro 1.0 is PES 2013 only (owner): the shelf holds the game_dir
 * folder's tile and nothing else; other folders are not shown. On the tile:
 *   ✕  play (runs PATCH first when the game needs it)
 *   △  the game menu: PLAY · PATCH · SETTINGS · SAVE GAMES
 *   □  Help
 * PATCH: check folder, Windows layer, d3d9.dll, the player's .reg, settings.dat,
 * record. Settings live in <JKPro>/System/games/<folder>/settings.ini.
 * Saves: an import drop folder, automatic backups (the last 10) before every
 * PLAY and every import, restore, export.
 *
 * The UI calls no PS4 API: the title fills JkProServices and acts on the
 * decision. jkpro_ui_run never starts the game by itself: it returns
 * JKPRO_START only after the player pressed PLAY.
 */
#ifndef JKPRO_UI_H
#define JKPRO_UI_H

#include <stddef.h>
#include <stdint.h>

#include "jk_pes13set.h"
#include "jk_ui.h"
#include "jk_keyboard.h"

struct JkRelease;   /* jk_screens.h: the signed-release verdict (jk_release_verify) */

/* The official home of JKPro (one constant; the owner confirms the repository name). */
#define JKPRO_OFFICIAL_URL "github.com/Salru13/JKPro"
#define JKPRO_AUTHOR "XxXJoker13XxX"

/* ---- the seam (docs/launcher/HANDOFF-UI-BUILD.md) ---- */

typedef enum { JKPRO_START = 1, JKPRO_LEAVE } JkProAction;

typedef struct {
    JkProAction action;
    JkPes13Settings settings;   /* already written to settings.dat when action = START */
    int logs;                   /* <system_dir>/logs-on */
    int pe_renderer;            /* <system_dir>/renderer-pe */
    int show_fps;               /* Settings > Show FPS (the title maps it to DXVK_PS4_FPS_HUD) */
} JkProDecision;

typedef struct {
    const char *game_dir;     /* e.g. /data/JKPRO/PES13 ("" when none found) */
    const char *prefix_dir;   /* <JKPro>/System/prefix (seeded by the title before the UI) */
    const char *system_dir;   /* <JKPro>/System (settings.ini, state) */
    const char *files_dir;    /* /app0/jkpro-files (d3d9.dll) */
    const char *saves_drop;   /* <JKPro>/Saves/PES13/Import */
    const char *usb_dir;      /* USB root or NULL */
    /* the JkServices callbacks */
    void *ctx;
    int (*present)(void *ctx, const JkFrame *frame);            /* required; <0: display lost */
    int (*pad)(void *ctx, uint32_t *held, uint32_t *pressed);   /* <0: no pad yet; may be NULL */
    int (*should_quit)(void *ctx);                              /* may be NULL */
    uint64_t (*now_ns)(void *ctx);                              /* required */
    void (*sleep_us)(void *ctx, uint32_t us);                   /* required */
    void (*log)(void *ctx, const char *line);                   /* may be NULL */
    int (*wall_time)(void *ctx, char *out, size_t cap);         /* "YYYY-MM-DD_HHMM"; may be NULL */
    /* Set by the title when the previous start died early (within 45 s, after
     * its own retries): one plain sentence for the player, never a path. NULL
     * or "": the run opens on the shelf. Otherwise it opens on "PES 2013
     * didn't start" (E101). */
    const char *start_failure;
    /* <JKPro>/Logs: session logs; support bundles go to <logs_dir>/support/<stamp>
     * (and USB JKPro-support/). NULL: no support bundle row. */
    const char *logs_dir;
    /* The game's Documents (lead E3, 2026-11-25): <JKPro>/Saves/<folder>, which
     * the title redirects My Documents to before each start. The live saves
     * and settings.dat are then under <docs_dir>/KONAMI/Pro Evolution Soccer
     * 2013/. NULL: the prefix's own Documents (v1). With docs_dir set, saves
     * found only in the prefix's Documents are copied over once (never moved). */
    const char *docs_dir;
    /* Release badge and About (owner, as in JK4SPEED): the title fills release
     * from jk_release_verify over jkpro-files/release.txt + release.sig. NULL:
     * "Unofficial build". No state ever changes PLAY, PATCH or a setting. */
    const struct JkRelease *release;
    const char *build_id;       /* e.g. "JKPro 1.0"; may be NULL */
    /* Data packs (lead, 2026-11-25): <JKPro>/DataPacks/PES13, where the title
     * redirects Common AppData. The live pack is <datapacks_dir>/KONAMI/Pro
     * Evolution Soccer 2013/download/dt0f.img; older ones in <datapacks_dir>/old/.
     * NULL: no data-pack feature. */
    const char *datapacks_dir;
} JkProServices;

/* 0 ok (*out holds the decision), <0 bad arguments or the display was lost. */
int jkpro_ui_run(const JkProServices *s, JkProDecision *out);

/* ---- the model and state machine, exposed for host tests ---- */

/* Pad bits as scePadRead reports them (the same values as jk_frontend.h). */
enum {
    JKPRO_PAD_OPTIONS = 0x8u,   /* the keyboard's OK */
    JKPRO_PAD_UP = 0x10u, JKPRO_PAD_RIGHT = 0x20u, JKPRO_PAD_DOWN = 0x40u, JKPRO_PAD_LEFT = 0x80u,
    JKPRO_PAD_L1 = 0x400u, JKPRO_PAD_R1 = 0x800u,
    JKPRO_PAD_TRIANGLE = 0x1000u, JKPRO_PAD_CIRCLE = 0x2000u, JKPRO_PAD_CROSS = 0x4000u, JKPRO_PAD_SQUARE = 0x8000u
};

typedef enum {
    JP_SCR_SHELF = 0, JP_SCR_MENU, JP_SCR_PATCH, JP_SCR_SETTINGS, JP_SCR_SAVES, JP_SCR_HELP, JP_SCR_LOGS, JP_SCR_ABOUT,
    JP_SCR_FAILED,    /* "PES 2013 didn't start" (start_failure) */
    JP_SCR_RESTORE,   /* SAVE GAMES > Restore a backup: every kept backup, pick any */
    JP_SCR_DATAPACKS, /* the official Data Pack (dt0f.img): install, remove, use an older one */
    JP_SCR_ONLINE     /* SETTINGS > Online: ThirteenServer on/off, the account (1.01) */
} JpScreen;

/* A tile's state. */
typedef enum {
    JP_ST_READY = 0,       /* patched and current */
    JP_ST_NEEDS_PATCH,     /* never patched */
    JP_ST_PATCH_AGAIN,     /* patched once, our files changed since */
    JP_ST_MISSING,         /* required game files missing */
    JP_ST_UNSUPPORTED      /* an exe JKPro cannot start (jk_games NEEDS_DISC): "Unsupported version" */
} JpTileState;

#define JP_MAX_TILES 8
typedef struct {
    JpTileState state;
    int modded;            /* logged only: the owner does not want the word on screen */
    char folder[64];
    char title[32];        /* the game's name from its definition: "PES 2013" */
} JpTile;

/* Game menu rows */
typedef enum { JP_MENU_PLAY = 0, JP_MENU_PATCH, JP_MENU_SETTINGS, JP_MENU_SAVES, JP_MENU_DATAPACKS, JP_MENU_COUNT } JpMenuItem;
/* Settings rows (locked rows are shown, never focused) */
typedef enum {
    JP_SET_RESOLUTION = 0, JP_SET_QUALITY, JP_SET_DISPLAY, JP_SET_FRAMERATE, JP_SET_CONTROLLER, JP_SET_FPS,
    JP_SET_KITSERVER,           /* only with <game>/kitserver/config.txt (owner, plan C) */
    JP_SET_FACEFIX,             /* only with <game>/kitserver/fserv.dll (owner, W7 face fix, off by default) */
    JP_SET_ONLINE,              /* always: ThirteenServer on/off; Cross opens the Online page (1.01) */
    JP_SET_LOGS, JP_SET_ABOUT, JP_SET_COUNT
} JpSetItem;
/* Online page rows (JP_ONL_ID / _PW / _CODE are also the open keyboard's field) */
typedef enum { JP_ONL_SERVER = 0, JP_ONL_ID, JP_ONL_PW, JP_ONL_CODE, JP_ONL_COUNT } JpOnlineRow;
/* Save games rows */
typedef enum { JP_SAV_LOAD = 0, JP_SAV_RESTORE, JP_SAV_EXPORT, JP_SAV_COUNT } JpSavItem;
/* Help pages */
/* Help (owner, rc7: a real guide like JkFlow's): one section per page */
typedef enum {
    JP_HELP_GAME = 0, JP_HELP_REG, JP_HELP_SAVES, JP_HELP_PACKS, JP_HELP_USB, JP_HELP_ONLINE, JP_HELP_SUPPORT,
    JP_HELP_COUNT
} JpHelpPage;
#define JP_HELP_GAMES JP_HELP_GAME
#define JP_HELP_PATCH JP_HELP_REG
#define JP_HELP_TROUBLE JP_HELP_SUPPORT

/* PATCH steps */
typedef enum {
    JP_STEP_CHECK = 0, JP_STEP_WINDOWS, JP_STEP_FILES, JP_STEP_REGISTRY, JP_STEP_SETTINGS, JP_STEP_RECORD,
    JP_STEP_COUNT   /* PATCH never touches Kitserver files (owner, 2026-11-26) */
} JpStep;
typedef enum { JP_SS_WAIT = 0, JP_SS_NOW, JP_SS_OK, JP_SS_FAIL, JP_SS_HIDDEN } JpStepState;   /* HIDDEN: not shown */
typedef enum { JP_PH_IDLE = 0, JP_PH_RUNNING, JP_PH_DONE, JP_PH_FAILED } JpPhase;

/* Dialogs */
typedef enum {
    JP_DLG_NONE = 0,
    JP_DLG_ERROR,         /* code + text, ✕ OK */
    JP_DLG_INFO,          /* text, ✕ OK */
    JP_DLG_LOAD,          /* Load saves? ✕ Load ○ Cancel */
    JP_DLG_RESTORE,       /* Restore the latest backup? */
    JP_DLG_NO_BACKUP      /* E401 before PLAY: ✕ Play without backup ○ Back */
} JpDialog;

#define JP_IMPORT_MAX 16
#define JKPRO_PACKS_OLD 3          /* older data packs kept in old/ */
#define JKPRO_BACKUPS_LIST 12      /* JKPRO_BACKUPS_KEPT + 1 (a backup being restored is never pruned) */

typedef struct {
    JpScreen screen;
    JpTile tiles[JP_MAX_TILES];
    int ntiles;
    int tile_focus;
    int game_tile;              /* index of the playable tile, -1 none */
    int menu_focus;             /* JpMenuItem */
    char version[24];
    char missing_file[96];      /* JP_ST_MISSING: the first required file not found ("pes2013.exe", "img/dt01.img") */

    /* PATCH */
    JpPhase phase;
    JpStepState steps[JP_STEP_COUNT];
    int step_now;
    long long bytes_done, bytes_total;
    char reg_used[96];          /* the .reg file name step 4 used */
    int play_after_patch;       /* ✕ on an unpatched tile: offer PLAY when done */

    /* Settings (current values) */
    int set_focus;              /* JpSetItem */
    int res;                    /* 0: 1280x720, 1: 1920x1080 */
    int quality;                /* 0 low, 1 medium, 2 high */
    int show_fps, logs;
    int online;                 /* <system_dir>/online-hosts (SETTINGS > Online server, ThirteenServer) */
    /* the Online page (1.01, owner: entered here as in Blur): <system_dir>/online-account,
     * line 1 the ID, line 2 the password; never logged (lengths only) */
    char online_id[64], online_pw[64];
    char online_code[64];       /* line 3, optional: PES's registration code (empty: the title types the ID) */
    int online_focus;           /* JpOnlineRow */
    int kb_field;               /* the keyboard is open for JP_ONL_ID / JP_ONL_PW; 0 closed */
    JkKeyboard kb;
    char online_msg[160];
    char online_file[240];      /* <system_dir>/online-account (Help, the page) */
    int kit_avail;              /* <game>/kitserver/config.txt exists: the Kitserver row is shown */
    int kit_mode;               /* JkKitMode shown: the state read from config.txt */
    int kit_set;                /* JkKitMode saved in settings.ini (applied before PLAY) */
    int face_avail;             /* <game>/kitserver/fserv.dll exists: the face fix row is shown */
    int face_state;             /* jkpro_fservfix_state(): 0 on, 1 off (the original), 2 another version */
    int face_set;               /* settings.ini fservfix: 1 on (applied before PLAY), 0 off */
    char set_msg[120];

    /* Save games */
    int sav_focus;
    int live_files;             /* files in the game's save folder */
    int import_files;           /* files waiting in the drop folder (and USB) */
    char import_list[160];      /* "OPTION.bin · EDIT.bin · 2 more" */
    int import_usb;             /* some of them are on the USB drive */
    int backups;                /* backups kept */
    char last_backup[32];       /* its stamp */
    /* Restore a backup (owner, 2026-11-25): every kept backup, newest first */
    struct {
        char name[48];          /* its folder: the date and time it was taken */
        char reason[24];        /* "before match" / "before import" / "before restore" / "older backup" */
        char files[160];        /* "OPTION.bin 402 KB · EDIT.bin 1.2 MB" */
    } bk[JKPRO_BACKUPS_LIST];
    int nbk, bk_focus, bk_top;

    /* Data packs (only with datapacks_dir) */
    int dp_on;
    int dp_have;                /* a pack is active */
    char dp_ver[16];            /* its version ("" unknown) */
    long long dp_size;
    char dp_live[300];          /* .../KONAMI/Pro Evolution Soccer 2013/download/dt0f.img */
    char dp_olddir[240];        /* <datapacks_dir>/old/ */
    int dp_nold;
    struct { char name[96]; char ver[16]; long long size; } dp_old[JKPRO_PACKS_OLD];
    int dp_wait;                /* a valid pack waiting in Import */
    char dp_wait_name[96], dp_wait_ver[16];
    long long dp_wait_size;
    char dp_bad[200];           /* "x.img: not a PES 2013 data pack; it stays in Import." */
    int dp_focus;
    char dp_msg[300];
    int dp_msg_bad;
    char sav_msg[400];
    int sav_msg_bad;
    /* owner UX (2026-11-25): SAVE GAMES says where everything is, with the real paths */
    char sav_drop[200];         /* where to put files: <saves_drop>/ (the console's letter case) */
    char sav_live[240];         /* the live saves: <Documents>/KONAMI/Pro Evolution Soccer 2013/save/ */
    char sav_bak[220];          /* the backups: <system_dir>/backup/saves/<folder>/<date_time>/ */
    char import_warn[200];      /* a .bin in the drop folder that is not OPTION.bin / EDIT.bin */
    char import_old[200];       /* a non-empty Import/done-<stamp>/ left by an older version */
    int usb;

    int help_page;
    int logs_focus;
    /* the real folders, in the console's letter case (Help, empty shelf, Logs) */
    char root_path[200];        /* /data/JkPro */
    char game_path[240];        /* /data/JkPro/PES13 (or <root>/PES13 when none is found) */
    char logs_path[240];        /* /data/JkPro/Logs */
    char failure[240];          /* JP_SCR_FAILED: the title's sentence */
    int can_bundle;             /* logs_dir is set */
    int pe_renderer;            /* <system_dir>/renderer-pe: the support fallback (owner: native for 1.0) */
    /* the release verdict, copied from JkProServices.release at open */
    int rel_state;              /* JkReleaseState: 0 unofficial, 1 official, 2 changed files */
    int rel_changed;            /* changed: how many files differ */
    char rel_version[24], rel_key_label[64], rel_key_id[24], rel_reason[160];
    char build_id[48];
    char bundle_msg[300];       /* where the last support bundle went, or why not */
    int bundle_bad;

    /* Dialog on top of the screen */
    JpDialog dialog;
    char dlg_code[8];
    char dlg_title[64];
    char dlg_body[400];
    int dlg_nothing_changed;    /* add "Nothing was changed." */
} JpModel;

/* Everything the state machine needs besides the model. */
typedef struct {
    JpModel m;
    const JkProServices *s;
    JkFrame *frame;             /* NULL: no drawing (tests) */
    char root[256];             /* the JKPro root (parent of system_dir) */
    char folder[64];            /* the game folder's name */
    int done;
    JkProDecision decision;
} JpApp;

/* Reads the folders and opens the shelf. 0 ok, <0 bad arguments. */
int jkpro_app_init(JpApp *a, const JkProServices *s, JkFrame *frame);
/* Handles one poll's newly pressed buttons (one action at most). 1 when the
 * model changed. */
int jkpro_app_press(JpApp *a, uint32_t pressed);
/* Re-reads the game state from disk (tiles, patch state, saves). */
void jkpro_app_refresh(JpApp *a);

/* The badge, top right of every screen: "Official Build by XxXJoker13XxX ·
 * <build_id> · key <id>" only for an official release, else "Unofficial build". */
void jkpro_badge_text(const JpModel *m, char *out, size_t cap);

/* DATA PACKS rows: kinds 0 install, 1 remove, 2 an old pack (idx into dp_old). Returns the count. */
int jkpro_dp_rows(const JpModel *m, int kinds[], int idx[], int max);

/* A Help page as plain text (title, then one line per entry); returns the entries. For tests. */
int jkpro_help_lines(const JpModel *m, int page, char *out, size_t cap);

/* Drawing (jkpro_draw.c): pure, from the model. */
void jkpro_draw(JkFrame *f, const JpModel *m);
void jkpro_draw_release(void);
/* Words used by the screens and tests. */
const char *jkpro_state_text(JpTileState st);
const char *jkpro_res_text(int res);
const char *jkpro_quality_text(int q);

/* Tests only: stand-ins for jkpro_fservfix_apply/revert/state (NULL: the real ones). */
void jkpro_fservfix_fns_for_tests(int (*apply)(const char *), int (*revert)(const char *), int (*state)(const char *));

/* Paths inside the prefix (the handoff's layout). */
#define JKPRO_DOCS_REL "drive_c/users/BLUR/Documents/KONAMI/Pro Evolution Soccer 2013"
#define JKPRO_DOCS_SUB "KONAMI/Pro Evolution Soccer 2013"
#define JKPRO_BACKUPS_KEPT 10      /* the newest 10, whatever made them (owner, 2026-11-25) */

#endif
