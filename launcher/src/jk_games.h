/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * JKPro game catalog and folder detection.
 *
 * The package ships one "<id>.game" INI per supported game. The launcher lists
 * the folders under /data/JKPro (reserved names skipped) and matches each one
 * by content, never by folder name and never by a whole-file exe hash (patches
 * change the exe):
 *   1. the exe from [identity] exe, letter case ignored;
 *   2. every [identity] marker file (relative path, case ignored) and every
 *      marker_ext "dir/.ext" (at least one file with that extension in dir);
 *   3. the PE section table: a ".securom" section = NEEDS_DISC;
 *   4. a version label: "pe" = VS_FIXEDFILEINFO file version a.b.c.d in .rsrc,
 *      "guid" = the [guid] probes "<offset> = <text> <label>";
 *   5. [foreign] loader_dir / modded_file present = MODDED.
 *
 * [identity] keys: id, name, exe, marker (repeatable), marker_ext (repeatable),
 * version (pe | guid | none). [guid]: "0x77dca8 = {GUID} 1.00". [foreign]:
 * loader_dir, modded_file (repeatable). [defaults] key = value and
 * [allowed] key = a|b|c: the per-game settings (jk_gamecfg.h).
 */
#ifndef JK_GAMES_H
#define JK_GAMES_H

#include <stddef.h>

#define JK_GAME_MAX_MARKERS 8
#define JK_GAME_MAX_GUIDS 8
#define JK_GAME_MAX_FOREIGN 8
#define JK_GAME_MAX_DEFAULTS 32

typedef enum { JK_GV_READY, JK_GV_MODDED, JK_GV_NEEDS_DISC, JK_GV_MISSING, JK_GV_UNKNOWN } JkGameVerdict;

typedef struct {
    long offset;
    char text[48];
    char label[16];
} JkGuidProbe;

typedef struct {
    char key[32];
    char value[64];
    char allowed[160];  /* "a|b|c"; "" = any value */
} JkGameDefault;

typedef struct {
    char id[16];
    char name[64];
    char exe[64];
    char markers[JK_GAME_MAX_MARKERS][96];
    int nmarkers;
    char marker_ext[JK_GAME_MAX_MARKERS][64]; /* "dat/.afs" */
    int nmarker_ext;
    char version_kind[8];                     /* "pe", "guid", "none" */
    JkGuidProbe guids[JK_GAME_MAX_GUIDS];
    int nguids;
    char foreign[JK_GAME_MAX_FOREIGN][64];    /* loader dirs and modded files */
    int nforeign;
    JkGameDefault defaults[JK_GAME_MAX_DEFAULTS]; /* [defaults] key = value, [allowed] key = a|b */
    int ndefaults;
} JkGameDef;

typedef struct {
    char folder[64];      /* the folder name under the root, as spelled on disk */
    char game_id[16];     /* "" when no definition matched */
    char exe[64];         /* the exe as spelled on disk */
    char version[24];     /* "1.4.0.0", "1.00", or "" */
    JkGameVerdict verdict;
    char missing[96];     /* the first missing marker (JK_GV_MISSING) */
} JkGameFound;

/* Reads every *.game file in defs_dir (sorted by file name). Returns the count
 * read (at most max); a file without id or exe is skipped. -1: no folder. */
int jk_games_load_defs(const char *defs_dir, JkGameDef *defs, int max);

/* Lists root's folders (sorted case-insensitively, reserved names System,
 * Saves, Logs, Mods, devtools, Export skipped in any case, files and dot entries
 * skipped) and detects each. Returns the count written (at most max). */
int jk_games_scan(const char *root, const JkGameDef *defs, int ndefs, JkGameFound *out, int max);

/* 1 when name is reserved under the JKPro root. */
int jk_games_reserved(const char *name);

const char *jk_games_verdict_name(JkGameVerdict v);

#endif
