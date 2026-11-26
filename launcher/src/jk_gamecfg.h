/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * JKPro per-folder paths and settings.
 *
 * One game folder = one profile (design/SETTINGS-MODEL.md):
 *   <root>/<folder>                       the player's game folder
 *   <root>/System/games/<folder>/settings.ini   player changes only
 *   <root>/System/prefix/<folder>         its own Wine prefix
 *   <root>/System/cache/<folder>          composed dxvk.conf, shader cache
 *   <root>/Saves/<folder>                 its own Documents (saves, settings.dat)
 *
 * Defaults and allowed values come from the game definition ([defaults] key =
 * value, [allowed] key = a|b|c); settings.ini holds only what the player
 * changed, under [settings]. Unknown keys are kept. A value not allowed falls
 * back to the default and counts as one warning (W501).
 */
#ifndef JK_GAMECFG_H
#define JK_GAMECFG_H

#include "jk_games.h"

#define JK_GAMECFG_MAX 32

typedef struct {
    char game_dir[192], games_dir[192], prefix_dir[192], cache_dir[192], saves_dir[192];
} JkGamePaths;

typedef struct {
    char key[32], value[64];
    int is_default;   /* 1: from the definition, not written */
    int known;        /* 1: the definition has this key */
} JkGameKV;

typedef struct {
    JkGameKV kv[JK_GAMECFG_MAX];
    int n;
    const JkGameDef *def;
} JkGameSettings;

/* root = the product root (/data/JKPro). folder must be one path component
 * without "..". 0 ok, -1 refused or too long. */
int jk_game_paths(const char *root, const char *folder, JkGamePaths *out);

/* Adds a default and its allowed values ("a|b|c"; NULL = any) to a definition.
 * Used by the .game parser ([defaults] / [allowed]) and by tests. */
int jk_gamecfg_def_default(JkGameDef *d, const char *key, const char *value, const char *allowed);

/* Defaults, then <games_dir>/settings.ini on top. Returns the number of
 * warnings (values not allowed), 0 when clean. */
int jk_game_settings_load(const JkGamePaths *p, const JkGameDef *d, JkGameSettings *out);

/* The current value, or NULL for an unknown key. */
const char *jk_game_setting(const JkGameSettings *s, const char *key);

/* Changes a known key. -1 when the value is not allowed or the key unknown. */
int jk_game_settings_set(JkGameSettings *s, const char *key, const char *value);

/* Writes only non-default values (and unknown keys) atomically; the previous
 * file is kept as settings.ini.bak. 0 ok, -1 failed. */
int jk_game_settings_save(const JkGamePaths *p, const JkGameSettings *s);

#endif
