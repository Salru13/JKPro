/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro per-folder paths and settings (jk_gamecfg). */
#include "jk_gamecfg.h"
#include "jk_test.h"
#include "jk_tmp.h"

static void def13(JkGameDef *d)
{
    memset(d, 0, sizeof *d);
    snprintf(d->id, sizeof d->id, "pes2013");
    snprintf(d->exe, sizeof d->exe, "pes2013.exe");
    jk_gamecfg_def_default(d, "resolution", "1920x1080", "1920x1080|1280x720");
    jk_gamecfg_def_default(d, "frame_rate", "60", "60");
    jk_gamecfg_def_default(d, "quality", "high", "low|medium|high");
    jk_gamecfg_def_default(d, "show_fps", "0", "0|1");
}

static void paths_per_folder(void)
{
    JkGamePaths p;
    CHECK_EQ(jk_game_paths("/data/JKPro", "PES13", &p), 0);
    CHECK(!strcmp(p.game_dir, "/data/JKPro/PES13"));
    CHECK(!strcmp(p.games_dir, "/data/JKPro/System/games/PES13"));
    CHECK(!strcmp(p.prefix_dir, "/data/JKPro/System/prefix/PES13"));
    CHECK(!strcmp(p.cache_dir, "/data/JKPro/System/cache/PES13"));
    CHECK(!strcmp(p.saves_dir, "/data/JKPro/Saves/PES13"));
    CHECK_EQ(jk_game_paths("/data/JKPro", "../x", &p), -1);
    CHECK_EQ(jk_game_paths("/data/JKPro", "", &p), -1);
}

static void defaults_when_no_file(void)
{
    char root[256];
    tmp_dir(root, sizeof root);
    JkGamePaths p;
    jk_game_paths(root, "PES13", &p);
    JkGameDef d;
    def13(&d);
    JkGameSettings s;
    CHECK_EQ(jk_game_settings_load(&p, &d, &s), 0);
    CHECK(!strcmp(jk_game_setting(&s, "resolution"), "1920x1080"));
    CHECK(!strcmp(jk_game_setting(&s, "frame_rate"), "60"));
    CHECK(jk_game_setting(&s, "nope") == NULL);
    tmp_rm_rf(root);
}

static void defaults_not_written(void)
{
    char root[256], buf[4096];
    tmp_dir(root, sizeof root);
    JkGamePaths p;
    jk_game_paths(root, "PES13", &p);
    JkGameDef d;
    def13(&d);
    JkGameSettings s;
    jk_game_settings_load(&p, &d, &s);
    CHECK_EQ(jk_game_settings_save(&p, &s), 0);
    CHECK(tmp_get(root, "System/games/PES13/settings.ini", buf, sizeof buf) >= 0);
    CHECK(strstr(buf, "resolution") == NULL);
    CHECK(strstr(buf, "[jkpro]") != NULL);
    tmp_rm_rf(root);
}

static void two_folders_isolated(void)
{
    char root[256], a[4096], b[4096], b2[4096];
    tmp_dir(root, sizeof root);
    JkGameDef d;
    def13(&d);
    JkGamePaths p1, p2;
    jk_game_paths(root, "PES13", &p1);
    jk_game_paths(root, "PES13-B", &p2);
    JkGameSettings s1, s2;
    jk_game_settings_load(&p2, &d, &s2);
    CHECK_EQ(jk_game_settings_set(&s2, "quality", "low"), 0);
    jk_game_settings_save(&p2, &s2);
    CHECK(tmp_get(root, "System/games/PES13-B/settings.ini", b, sizeof b) >= 0);
    jk_game_settings_load(&p1, &d, &s1);
    CHECK_EQ(jk_game_settings_set(&s1, "resolution", "1280x720"), 0);
    jk_game_settings_save(&p1, &s1);
    CHECK(tmp_get(root, "System/games/PES13/settings.ini", a, sizeof a) >= 0);
    CHECK(tmp_get(root, "System/games/PES13-B/settings.ini", b2, sizeof b2) >= 0);
    CHECK(!strcmp(b, b2));
    CHECK(strstr(a, "resolution = 1280x720") != NULL);
    CHECK(strstr(a, "quality") == NULL);
    jk_game_settings_load(&p2, &d, &s2);
    CHECK(!strcmp(jk_game_setting(&s2, "resolution"), "1920x1080"));
    CHECK(!strcmp(jk_game_setting(&s2, "quality"), "low"));
    tmp_rm_rf(root);
}

static void bad_value_falls_back_with_warning(void)
{
    char root[256];
    tmp_dir(root, sizeof root);
    tmp_put(root, "System/games/PES13/settings.ini",
            "[jkpro]\nschema = 1\n[settings]\nframe_rate = 30\nquality = ultra\nmy_note = kept\n");
    JkGamePaths p;
    jk_game_paths(root, "PES13", &p);
    JkGameDef d;
    def13(&d);
    JkGameSettings s;
    CHECK_EQ(jk_game_settings_load(&p, &d, &s), 2); /* two warnings (W501) */
    CHECK(!strcmp(jk_game_setting(&s, "frame_rate"), "60"));
    CHECK(!strcmp(jk_game_setting(&s, "quality"), "high"));
    CHECK_EQ(jk_game_settings_set(&s, "frame_rate", "30"), -1);
    char buf[4096];
    jk_game_settings_save(&p, &s);
    tmp_get(root, "System/games/PES13/settings.ini", buf, sizeof buf);
    CHECK(strstr(buf, "my_note = kept") != NULL);
    CHECK(tmp_exists(root, "System/games/PES13/settings.ini.bak"));
    tmp_rm_rf(root);
}

int main(void)
{
    RUN(paths_per_folder);
    RUN(defaults_when_no_file);
    RUN(defaults_not_written);
    RUN(two_folders_isolated);
    RUN(bad_value_falls_back_with_warning);
    return jk_test_report("test_gamecfg");
}
