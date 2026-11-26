/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* The title's prefix edits before a start (jkpro_prefix): My Documents redirect, registry re-seed. */
#include "jkpro_prefix.h"
#include "jk_test.h"
#include "jk_tmp.h"
#include "jk_fs.h"
#include <sys/stat.h>

#define SF "[Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Explorer\\\\Shell Folders] 1794011076\n"
#define USF "[Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Explorer\\\\User Shell Folders] 1794011076\n"

static const char USER_REG[] =
    "WINE REGISTRY Version 2\n"
    ";; All keys relative to \\\\User\\\\S-1-5-21-0-0-0-1000\n\n"
    "[Software\\\\Other] 1794011076\n"
    "\"Personal\"=\"not a shell folder\"\n\n"
    SF "#time=1dd6e4e5ea96c2c\n"
    "\"AppData\"=\"C:\\\\users\\\\BLUR\\\\AppData\\\\Roaming\"\n"
    "\"My Documents\"=\"C:\\\\users\\\\BLUR\\\\Documents\"\n"
    "\"Personal\"=\"C:\\\\users\\\\BLUR\\\\Documents\"\n\n"
    USF "#time=1dd6e4e5ea95124\n"
    "\"AppData\"=str(2):\"%USERPROFILE%\\\\AppData\\\\Roaming\"\n"
    "\"Personal\"=str(2):\"%USERPROFILE%\\\\Documents\"\n\n";

#define WIN "Z:\\data\\JKPro\\Saves\\PES13"
#define WIN_REG "Z:\\\\data\\\\JKPro\\\\Saves\\\\PES13"

static void documents_redirected(void)
{
    char dir[256], path[400], *got = malloc(8192);
    tmp_dir(dir, sizeof dir);
    tmp_put(dir, "user.reg", USER_REG);
    snprintf(path, sizeof path, "%s/user.reg", dir);
    CHECK_EQ(jkpro_redirect_documents(path, WIN), 1);
    CHECK(tmp_get(dir, "user.reg", got, 8192) > 0);
    CHECK(strstr(got, SF "#time=1dd6e4e5ea96c2c\n\"AppData\"=\"C:\\\\users\\\\BLUR\\\\AppData\\\\Roaming\"\n"
                         "\"My Documents\"=\"" WIN_REG "\"\n\"Personal\"=\"" WIN_REG "\"\n\n") != NULL);
    CHECK(strstr(got, USF "#time=1dd6e4e5ea95124\n\"AppData\"=str(2):\"%USERPROFILE%\\\\AppData\\\\Roaming\"\n"
                          "\"Personal\"=str(2):\"" WIN_REG "\"\n\n") != NULL);
    CHECK(strstr(got, "[Software\\\\Other] 1794011076\n\"Personal\"=\"not a shell folder\"\n") != NULL);
    CHECK(!strncmp(got, "WINE REGISTRY Version 2\n", 24));
    /* already redirected: nothing written */
    CHECK_EQ(jkpro_redirect_documents(path, WIN), 0);
    free(got);
    tmp_rm_rf(dir);
}

static void documents_refused_without_personal(void)
{
    char dir[256], path[400], *got = malloc(8192);
    tmp_dir(dir, sizeof dir);
    tmp_put(dir, "user.reg", "WINE REGISTRY Version 2\n\n" SF "\"AppData\"=\"C:\\\\x\"\n\n");
    snprintf(path, sizeof path, "%s/user.reg", dir);
    CHECK_EQ(jkpro_redirect_documents(path, WIN), -1);
    CHECK(tmp_get(dir, "user.reg", got, 8192) > 0);
    CHECK(!strcmp(got, "WINE REGISTRY Version 2\n\n" SF "\"AppData\"=\"C:\\\\x\"\n\n"));   /* untouched */
    tmp_put(dir, "user.reg", "");                                                       /* 0 bytes */
    CHECK_EQ(jkpro_redirect_documents(path, WIN), -1);
    snprintf(path, sizeof path, "%s/missing.reg", dir);
    CHECK_EQ(jkpro_redirect_documents(path, WIN), -1);
    free(got);
    tmp_rm_rf(dir);
}

#define EXP "Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Explorer\\\\"
#define WOW "Software\\\\Wow6432Node\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Explorer\\\\"
#define SYS_SF "[" EXP "Shell Folders] 1794011076\n"
#define SYS_USF "[" EXP "User Shell Folders] 1794011076\n"
#define WOW_SF "[" WOW "Shell Folders] 1794011076\n"
#define WOW_USF "[" WOW "User Shell Folders] 1794011076\n"
#define CA_SF "\"Common AppData\"=\"C:\\\\ProgramData\"\n"
#define CA_USF "\"Common AppData\"=str(2):\"%ProgramData%\"\n"
/* "Common AppData" under keys that are not the four Explorer folders: untouched */
#define OTHER "[Software\\\\Other\\\\Explorer\\\\Shell Folders] 1794011076\n" CA_SF "\n" \
              "[" EXP "Shell Folders\\\\Sub] 1794011076\n" CA_USF "\n"
#define SYS_HEAD "WINE REGISTRY Version 2\n;; All keys relative to \\\\Machine\n\n"

static const char SYSTEM_REG[] =
    SYS_HEAD
    OTHER
    SYS_SF "#time=1dd6e4e5ea96c2c\n" CA_SF "\"Common Desktop\"=\"C:\\\\users\\\\Public\\\\Desktop\"\n\n"
    SYS_USF CA_USF "\"Common Desktop\"=str(2):\"%PUBLIC%\\\\Desktop\"\n\n"
    WOW_SF CA_SF "\n"
    WOW_USF CA_USF "\n";

#define DP "Z:\\data\\JKPro\\DataPacks\\PES13"
#define DP_REG "Z:\\\\data\\\\JKPro\\\\DataPacks\\\\PES13"

static void common_appdata_redirected(void)
{
    char dir[256], path[400], *got = malloc(8192);
    tmp_dir(dir, sizeof dir);
    tmp_put(dir, "system.reg", SYSTEM_REG);
    snprintf(path, sizeof path, "%s/system.reg", dir);
    CHECK_EQ(jkpro_redirect_common_appdata(path, DP), 1);
    CHECK(tmp_get(dir, "system.reg", got, 8192) > 0);
    CHECK(strstr(got, SYS_SF "#time=1dd6e4e5ea96c2c\n\"Common AppData\"=\"" DP_REG "\"\n"
                             "\"Common Desktop\"=\"C:\\\\users\\\\Public\\\\Desktop\"\n\n") != NULL);
    CHECK(strstr(got, SYS_USF "\"Common AppData\"=str(2):\"" DP_REG "\"\n"
                              "\"Common Desktop\"=str(2):\"%PUBLIC%\\\\Desktop\"\n\n") != NULL);
    CHECK(strstr(got, WOW_SF "\"Common AppData\"=\"" DP_REG "\"\n\n") != NULL);
    CHECK(strstr(got, WOW_USF "\"Common AppData\"=str(2):\"" DP_REG "\"\n") != NULL);
    CHECK(strstr(got, OTHER) != NULL);                                  /* other keys untouched */
    CHECK(!strncmp(got, SYS_HEAD OTHER, sizeof SYS_HEAD OTHER - 1));
    /* already redirected: nothing written */
    struct stat a, b;
    CHECK(stat(path, &a) == 0);
    CHECK_EQ(jkpro_redirect_common_appdata(path, DP), 0);
    CHECK(stat(path, &b) == 0 && a.st_ino == b.st_ino);
    free(got);
    tmp_rm_rf(dir);
}

static void common_appdata_refused_when_a_section_is_missing(void)
{
    static const char NO_WOW_USF[] =
        "WINE REGISTRY Version 2\n\n" OTHER
        SYS_SF CA_SF "\n" SYS_USF CA_USF "\n" WOW_SF CA_SF "\n";
    static const char NO_VALUE[] =
        "WINE REGISTRY Version 2\n\n"
        SYS_SF CA_SF "\n" SYS_USF CA_USF "\n" WOW_SF CA_SF "\n" WOW_USF "\"Common Desktop\"=\"x\"\n";
    char dir[256], path[400], *got = malloc(8192);
    tmp_dir(dir, sizeof dir);
    snprintf(path, sizeof path, "%s/system.reg", dir);
    tmp_put(dir, "system.reg", NO_WOW_USF);
    CHECK_EQ(jkpro_redirect_common_appdata(path, DP), -1);
    CHECK(tmp_get(dir, "system.reg", got, 8192) > 0 && !strcmp(got, NO_WOW_USF));   /* untouched */
    tmp_put(dir, "system.reg", NO_VALUE);
    CHECK_EQ(jkpro_redirect_common_appdata(path, DP), -1);
    CHECK(tmp_get(dir, "system.reg", got, 8192) > 0 && !strcmp(got, NO_VALUE));
    tmp_put(dir, "system.reg", "");
    CHECK_EQ(jkpro_redirect_common_appdata(path, DP), -1);
    snprintf(path, sizeof path, "%s/missing.reg", dir);
    CHECK_EQ(jkpro_redirect_common_appdata(path, DP), -1);
    free(got);
    tmp_rm_rf(dir);
}

#define MARKER ".n245-complete"

static void reseed_armed_when_system_reg_empty_and_no_good_copy(void)
{
    char prefix[256], good[300], buf[64];
    tmp_dir(prefix, sizeof prefix);
    snprintf(good, sizeof good, "%s/good", prefix);
    tmp_put(prefix, MARKER, "N245-COMPLETE abc\n");
    tmp_put(prefix, "system.reg", "");
    CHECK_EQ(jkpro_reseed_arm(prefix, good), 1);
    CHECK_EQ(tmp_get(prefix, MARKER, buf, sizeof buf), -1);            /* the seed import runs again */
    tmp_rm_rf(prefix);
}

static void reseed_not_armed_with_good_copy(void)
{
    char prefix[256], buf[64];
    tmp_dir(prefix, sizeof prefix);
    tmp_put(prefix, MARKER, "N245-COMPLETE abc\n");
    tmp_put(prefix, "system.reg", "");
    tmp_put(prefix, "good/system.reg", "WINE REGISTRY Version 2\n;; good\n");
    char good[300];
    snprintf(good, sizeof good, "%s/good", prefix);
    CHECK_EQ(jkpro_reseed_arm(prefix, good), 0);                       /* the launcher restores it (E610) */
    CHECK(tmp_get(prefix, MARKER, buf, sizeof buf) > 0);
    tmp_rm_rf(prefix);
}

static void reseed_not_armed_when_valid(void)
{
    char prefix[256], good[300], buf[64];
    tmp_dir(prefix, sizeof prefix);
    snprintf(good, sizeof good, "%s/good", prefix);
    tmp_put(prefix, MARKER, "N245-COMPLETE abc\n");
    tmp_put(prefix, "system.reg", "WINE REGISTRY Version 2\n;; fine\n");
    CHECK_EQ(jkpro_reseed_arm(prefix, good), 0);
    CHECK(tmp_get(prefix, MARKER, buf, sizeof buf) > 0);
    /* a headless system.reg (not a Wine registry) counts as damaged */
    tmp_put(prefix, "system.reg", "garbage");
    CHECK_EQ(jkpro_reseed_arm(prefix, good), 1);
    tmp_rm_rf(prefix);
}

static void reseed_not_armed_without_prefix(void)
{
    char dir[256], prefix[300], good[300];
    tmp_dir(dir, sizeof dir);
    snprintf(prefix, sizeof prefix, "%s/prefix", dir);                 /* first start: the import builds it */
    snprintf(good, sizeof good, "%s/good", dir);
    CHECK_EQ(jkpro_reseed_arm(prefix, good), 0);
    tmp_rm_rf(dir);
}

/* The backup/support stamp is computed without libc time formatting
 * (strftime crashed in the title: SIGBUS in its locale TLS, 2026-11-25 rc3). */
static void wall_stamp_is_utc_minutes(void)
{
    char out[32];
    CHECK(jkpro_wall_stamp(0, out, sizeof out) == 0 && !strcmp(out, "1970-01-01_0000"));
    CHECK(jkpro_wall_stamp(951782400LL, out, sizeof out) == 0 && !strcmp(out, "2000-02-29_0000"));
    CHECK(jkpro_wall_stamp(1795623750LL, out, sizeof out) == 0 && !strcmp(out, "2026-11-25_1622"));
    CHECK(jkpro_wall_stamp(4102444799LL, out, sizeof out) == 0 && !strcmp(out, "2099-12-31_2359"));
    CHECK(jkpro_wall_stamp(-1, out, sizeof out) == -1);
    CHECK(jkpro_wall_stamp(0, out, 15) == -1);
}

/* Everything under <JKPro> the player may touch over FTP (Saves, backups, logs)
 * gets rwx for all: the console's FTP server is another user (rc4: 0755 saves
 * could not be replaced or added to). */
static void open_perms_makes_tree_writable(void)
{
    char dir[256], p[400];
    struct stat st;
    tmp_dir(dir, sizeof dir);
    snprintf(p, sizeof p, "%s/Saves/PES13/save", dir);
    CHECK(jk_fs_mkdir_p(p) == 0);
    snprintf(p, sizeof p, "%s/Saves/PES13/save/OPTION.bin", dir);
    CHECK(jk_fs_write_atomic(p, "x", 1) == 0);
    chmod(p, 0644);
    snprintf(p, sizeof p, "%s/Saves/PES13", dir);
    chmod(p, 0755);
    snprintf(p, sizeof p, "%s/Saves", dir);
    CHECK(jkpro_open_perms(p) >= 2);
    snprintf(p, sizeof p, "%s/Saves/PES13", dir);
    CHECK(stat(p, &st) == 0 && (st.st_mode & 07777) == 0777);
    snprintf(p, sizeof p, "%s/Saves/PES13/save/OPTION.bin", dir);
    CHECK(stat(p, &st) == 0 && (st.st_mode & 07777) == 0666);
    CHECK(jkpro_open_perms(p) == -1);          /* not a directory */
    snprintf(p, sizeof p, "%s/none", dir);
    CHECK(jkpro_open_perms(p) == -1);
}

int main(void)
{
    RUN(wall_stamp_is_utc_minutes);
    RUN(open_perms_makes_tree_writable);
    RUN(documents_redirected);
    RUN(documents_refused_without_personal);
    RUN(common_appdata_redirected);
    RUN(common_appdata_refused_when_a_section_is_missing);
    RUN(reseed_armed_when_system_reg_empty_and_no_good_copy);
    RUN(reseed_not_armed_with_good_copy);
    RUN(reseed_not_armed_when_valid);
    RUN(reseed_not_armed_without_prefix);
    return jk_test_report("test_jkpro_prefix");
}
