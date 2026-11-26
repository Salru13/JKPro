/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro SETTINGS > Kitserver: config.txt dll lines (On / Without kits / Off). */
#include "jkpro_kitcfg.h"
#include "jk_fs.h"
#include "jk_test.h"
#include "jk_tmp.h"

/* Kitserver 13 style: UTF-8 BOM, CRLF, a commented module, an unknown option,
 * a dll line in another section (never touched). */
static const char *ORIG =
    "\xEF\xBB\xBF[kload]\r\n"
    "dll = kserv\r\n"
    "dll = afs2fs.dll\r\n"
    "; dll = speeder\r\n"
    "unknown.option = 1\r\n"
    "\r\n"
    "[kserv]\r\n"
    "dll = notme\r\n"
    "enable = 1\r\n";

static const char *NOKITS =
    "\xEF\xBB\xBF[kload]\r\n"
    "; dll = kserv\r\n"
    "dll = afs2fs.dll\r\n"
    "; dll = speeder\r\n"
    "unknown.option = 1\r\n"
    "\r\n"
    "[kserv]\r\n"
    "dll = notme\r\n"
    "enable = 1\r\n";

static const char *OFF =
    "\xEF\xBB\xBF[kload]\r\n"
    "; dll = kserv\r\n"
    "; dll = afs2fs.dll\r\n"
    "; dll = speeder\r\n"
    "unknown.option = 1\r\n"
    "\r\n"
    "[kserv]\r\n"
    "dll = notme\r\n"
    "enable = 1\r\n";

static int apply_is(const char *cur, const char *orig, JkKitMode mode, const char *want)
{
    size_t k;
    char *t = jkpro_kitcfg_apply(cur, strlen(cur), orig, orig ? strlen(orig) : 0, mode, &k);
    int ok = t && k == strlen(want) && !memcmp(t, want, k);
    if (!ok && t) fprintf(stderr, "  got:\n%.*s\n", (int)k, t);
    free(t);
    return ok;
}

static void three_states_byte_exact(void)
{
    CHECK(apply_is(ORIG, NULL, JK_KIT_ON, ORIG));          /* On on the original: nothing changes */
    CHECK(apply_is(ORIG, NULL, JK_KIT_NOKITS, NOKITS));    /* only the kserv line, "; " in front */
    CHECK(apply_is(ORIG, NULL, JK_KIT_OFF, OFF));          /* every dll line under [kload] */
    CHECK(apply_is(NOKITS, ORIG, JK_KIT_OFF, OFF));
    CHECK(apply_is(OFF, ORIG, JK_KIT_NOKITS, NOKITS));
}

static void round_trip_to_the_original(void)
{
    CHECK(apply_is(NOKITS, ORIG, JK_KIT_ON, ORIG));
    CHECK(apply_is(OFF, ORIG, JK_KIT_ON, ORIG));           /* speeder stays commented: it was in the original */
    /* LF only, no BOM, kserv the first line after [kload], spaces before dll */
    const char *lf = "[kload]\n  dll = kserv.dll\n#dll=lodmixer\n";
    size_t k1, k2;
    char *off = jkpro_kitcfg_apply(lf, strlen(lf), NULL, 0, JK_KIT_OFF, &k1);
    CHECK(off && !strcmp(off, "[kload]\n;   dll = kserv.dll\n#dll=lodmixer\n"));
    char *on = jkpro_kitcfg_apply(off, k1, lf, strlen(lf), JK_KIT_ON, &k2);
    CHECK(on && k2 == strlen(lf) && !memcmp(on, lf, k2));
    free(off);
    free(on);
    /* the last line without a newline */
    CHECK(apply_is("[kload]\r\ndll = kserv", NULL, JK_KIT_NOKITS, "[kload]\r\n; dll = kserv"));
}

static void later_edits_and_unknown_lines_kept(void)
{
    /* the player changed an option and added a module after JKPro's backup */
    const char *edited =
        "\xEF\xBB\xBF[kload]\r\n"
        "; dll = kserv\r\n"
        "dll = afs2fs.dll\r\n"
        "; dll = speeder\r\n"
        "dll = newmod\r\n"
        "unknown.option = 2\r\n"
        "\r\n"
        "[kserv]\r\n"
        "dll = notme\r\n"
        "enable = 0\r\n";
    const char *on =
        "\xEF\xBB\xBF[kload]\r\n"
        "dll = kserv\r\n"
        "dll = afs2fs.dll\r\n"
        "; dll = speeder\r\n"
        "dll = newmod\r\n"
        "unknown.option = 2\r\n"
        "\r\n"
        "[kserv]\r\n"
        "dll = notme\r\n"
        "enable = 0\r\n";
    CHECK(apply_is(edited, ORIG, JK_KIT_ON, on));           /* only kserv's comment state changes */
    CHECK_EQ(jkpro_kitcfg_state(edited, strlen(edited), ORIG, strlen(ORIG)), JK_KIT_NOKITS);
}

static void state_read_back(void)
{
    CHECK_EQ(jkpro_kitcfg_state(ORIG, strlen(ORIG), NULL, 0), JK_KIT_ON);
    CHECK_EQ(jkpro_kitcfg_state(NOKITS, strlen(NOKITS), ORIG, strlen(ORIG)), JK_KIT_NOKITS);
    CHECK_EQ(jkpro_kitcfg_state(OFF, strlen(OFF), ORIG, strlen(ORIG)), JK_KIT_OFF);
    /* the player's own mix (afs2fs off, kserv on): none of the three */
    const char *mix =
        "\xEF\xBB\xBF[kload]\r\ndll = kserv\r\n; dll = afs2fs.dll\r\n; dll = speeder\r\nunknown.option = 1\r\n\r\n"
        "[kserv]\r\ndll = notme\r\nenable = 1\r\n";
    CHECK_EQ(jkpro_kitcfg_state(mix, strlen(mix), ORIG, strlen(ORIG)), -1);
    CHECK(!strcmp(jkpro_kit_word(JK_KIT_NOKITS), "nokits"));
    CHECK(!strcmp(jkpro_kit_text(JK_KIT_NOKITS), "Without kits"));
    CHECK_EQ(jkpro_kit_from_word("off"), JK_KIT_OFF);
    CHECK_EQ(jkpro_kit_from_word("bogus"), JK_KIT_ON);
    CHECK_EQ(jkpro_kit_from_word(NULL), JK_KIT_ON);
}

static int file_is(const char *dir, const char *rel, const char *want)
{
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", dir, rel);
    size_t n;
    char *t = jk_fs_read(p, 1 << 20, &n);
    int ok = t && n == strlen(want) && !memcmp(t, want, n);
    free(t);
    return ok;
}

static void files_and_backup_once(void)
{
    char d[256], game[300], sys[300];
    tmp_dir(d, sizeof d);
    snprintf(game, sizeof game, "%s/PES13", d);
    snprintf(sys, sizeof sys, "%s/System", d);
    /* no config.txt: nothing to do, nothing made */
    CHECK(!jkpro_kitcfg_present(game));
    CHECK_EQ(jkpro_kitcfg_sync(game, sys, JK_KIT_OFF), 0);
    CHECK_EQ(jkpro_kitcfg_read_state(game, sys), -1);
    CHECK(!tmp_exists(d, "System/backup/kitserver"));
    tmp_put(d, "PES13/kitserver/config.txt", ORIG);
    tmp_put(d, "PES13/kitserver/kserv.dll", "KSERV");
    CHECK(jkpro_kitcfg_present(game));
    /* On with no change: no write, no backup */
    CHECK_EQ(jkpro_kitcfg_sync(game, sys, JK_KIT_ON), 0);
    CHECK(!tmp_exists(d, "System/backup/kitserver/config.txt.original"));
    CHECK_EQ(jkpro_kitcfg_read_state(game, sys), JK_KIT_ON);
    /* Without kits: the original kept, config.txt changed */
    CHECK_EQ(jkpro_kitcfg_sync(game, sys, JK_KIT_NOKITS), 0);
    CHECK(file_is(d, "System/backup/kitserver/config.txt.original", ORIG));
    CHECK(file_is(d, "PES13/kitserver/config.txt", NOKITS));
    CHECK_EQ(jkpro_kitcfg_read_state(game, sys), JK_KIT_NOKITS);
    /* Off, then On: back to the original bytes; the backup never overwritten */
    CHECK_EQ(jkpro_kitcfg_sync(game, sys, JK_KIT_OFF), 0);
    CHECK(file_is(d, "PES13/kitserver/config.txt", OFF));
    CHECK(file_is(d, "System/backup/kitserver/config.txt.original", ORIG));
    CHECK_EQ(jkpro_kitcfg_sync(game, sys, JK_KIT_ON), 0);
    CHECK(file_is(d, "PES13/kitserver/config.txt", ORIG));
    /* a re-copied folder (the original again) follows the setting at the next sync */
    CHECK_EQ(jkpro_kitcfg_sync(game, sys, JK_KIT_NOKITS), 0);
    tmp_put(d, "PES13/kitserver/config.txt", ORIG);
    CHECK_EQ(jkpro_kitcfg_read_state(game, sys), JK_KIT_ON);
    CHECK_EQ(jkpro_kitcfg_sync(game, sys, JK_KIT_NOKITS), 0);
    CHECK(file_is(d, "PES13/kitserver/config.txt", NOKITS));
    CHECK(file_is(d, "System/backup/kitserver/config.txt.original", ORIG));
    /* no other kitserver file touched */
    CHECK(file_is(d, "PES13/kitserver/kserv.dll", "KSERV"));
    tmp_rm_rf(d);
}

int main(void)
{
    RUN(three_states_byte_exact);
    RUN(round_trip_to_the_original);
    RUN(later_edits_and_unknown_lines_kept);
    RUN(state_read_back);
    RUN(files_and_backup_once);
    return jk_test_report("test_jkpro_kitcfg");
}
