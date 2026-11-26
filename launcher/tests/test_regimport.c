/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro: the player's .reg file into the game's Wine prefix (jk_regimport). */
#include "jk_regimport.h"
#include "jk_test.h"
#include "jk_tmp.h"

static const char *STORE =
    "WINE REGISTRY Version 2\n;; All keys relative to \\\\Machine\n\n#arch=win64\n\n"
    "[Software\\\\Microsoft\\\\Windows] 1700000000\n#time=1\n\"A\"=\"b\"\n\n";

static void put_utf16(const char *dir, const char *rel, const char *ascii)
{
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", dir, rel);
    FILE *f = fopen(p, "wb");
    fputc(0xff, f); fputc(0xfe, f);
    for (const char *c = ascii; *c; c++) {
        if (*c == '\n') { fputc('\r', f); fputc(0, f); }
        fputc(*c, f); fputc(0, f);
    }
    fclose(f);
}

static void regimport_utf16_v5(void)
{
    char d[256], buf[8192], missing[64];
    tmp_dir(d, sizeof d);
    tmp_put(d, "system.reg", STORE);
    put_utf16(d, "fix.reg", "Windows Registry Editor Version 5.00\n\n"
              "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\KONAMI\\PES2013]\n"
              "\"code\"=\"AAAA-BBBB-CCCC-DDDD-EEEE\"\n\"installdir\"=\"..\\\\Pro Evolution Soccer 2013\\\\\"\n"
              "\"version\"=\"1.04.0000\"\n");
    char reg[300], store[300];
    snprintf(reg, sizeof reg, "%s/fix.reg", d);
    snprintf(store, sizeof store, "%s/system.reg", d);
    CHECK_EQ(jk_regimport(reg, store, "Software\\KONAMI\\PES2013", "Z:\\data\\JKPro\\PES13\\", "code", missing, sizeof missing), 3);
    CHECK(missing[0] == 0);
    tmp_get(d, "system.reg", buf, sizeof buf);
    CHECK(strstr(buf, "[Software\\\\Wow6432Node\\\\KONAMI\\\\PES2013]") != NULL);
    CHECK(strstr(buf, "\"installdir\"=\"Z:\\\\data\\\\JKPro\\\\PES13\\\\\"") != NULL);
    CHECK(strstr(buf, "\"code\"=\"AAAA-BBBB-CCCC-DDDD-EEEE\"") != NULL);
    CHECK(strstr(buf, "\"version\"=\"1.04.0000\"") != NULL);
    CHECK(strstr(buf, "Pro Evolution Soccer 2013") == NULL);
    CHECK(strstr(buf, "[Software\\\\Microsoft\\\\Windows]") != NULL);
    /* idempotent: a second import replaces the block, never duplicates it */
    CHECK_EQ(jk_regimport(reg, store, "Software\\KONAMI\\PES2013", "Z:\\data\\JKPro\\PES13\\", "code", missing, sizeof missing), 3);
    tmp_get(d, "system.reg", buf, sizeof buf);
    const char *first = strstr(buf, "KONAMI\\\\PES2013]");
    CHECK(first && !strstr(first + 1, "KONAMI\\\\PES2013]"));
    tmp_rm_rf(d);
}

static void regimport_no_file_seeds_installdir_and_reports_missing(void)
{
    char d[256], buf[8192], missing[64];
    tmp_dir(d, sizeof d);
    tmp_put(d, "system.reg", STORE);
    char store[300];
    snprintf(store, sizeof store, "%s/system.reg", d);
    CHECK_EQ(jk_regimport(NULL, store, "Software\\KONAMI\\PES2013", "Z:\\data\\JKPro\\PES13\\", "code", missing, sizeof missing), 1);
    CHECK(!strcmp(missing, "code"));
    tmp_get(d, "system.reg", buf, sizeof buf);
    CHECK(strstr(buf, "[Software\\\\Wow6432Node\\\\KONAMI\\\\PES2013]") != NULL);
    CHECK(strstr(buf, "\"installdir\"=\"Z:\\\\data\\\\JKPro\\\\PES13\\\\\"") != NULL);
    tmp_rm_rf(d);
}

static void regimport_32bit_export_gets_wow6432node(void)
{
    char d[256], buf[8192], missing[64];
    tmp_dir(d, sizeof d);
    tmp_put(d, "system.reg", STORE);
    tmp_put(d, "x.reg", "REGEDIT4\r\n\r\n[HKEY_LOCAL_MACHINE\\SOFTWARE\\KONAMI\\PES2013]\r\n\"code\"=\"X\"\r\n");
    char reg[300], store[300];
    snprintf(reg, sizeof reg, "%s/x.reg", d);
    snprintf(store, sizeof store, "%s/system.reg", d);
    CHECK_EQ(jk_regimport(reg, store, "Software\\KONAMI\\PES2013", "Z:\\g\\", "code", missing, sizeof missing), 2);
    tmp_get(d, "system.reg", buf, sizeof buf);
    CHECK(strstr(buf, "[Software\\\\Wow6432Node\\\\KONAMI\\\\PES2013]") != NULL);
    CHECK(strstr(buf, "[Software\\\\KONAMI\\\\PES2013]") == NULL);
    tmp_rm_rf(d);
}

static void regimport_ignores_other_keys_and_bad_store(void)
{
    char d[256], buf[8192], missing[64];
    tmp_dir(d, sizeof d);
    tmp_put(d, "system.reg", STORE);
    tmp_put(d, "x.reg", "REGEDIT4\n\n[HKEY_CURRENT_USER\\Software\\Evil]\n\"a\"=\"b\"\n"
                        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Other]\n\"a\"=\"b\"\n");
    char reg[300], store[300];
    snprintf(reg, sizeof reg, "%s/x.reg", d);
    snprintf(store, sizeof store, "%s/system.reg", d);
    CHECK_EQ(jk_regimport(reg, store, "Software\\KONAMI\\PES2013", "Z:\\g\\", "code", missing, sizeof missing), 1);
    tmp_get(d, "system.reg", buf, sizeof buf);
    CHECK(strstr(buf, "Evil") == NULL);
    CHECK(strstr(buf, "Other") == NULL);
    tmp_put(d, "bad.reg", "not a wine store\n");
    snprintf(store, sizeof store, "%s/bad.reg", d);
    CHECK_EQ(jk_regimport(NULL, store, "Software\\KONAMI\\PES2013", "Z:\\g\\", "code", missing, sizeof missing), -1);
    tmp_rm_rf(d);
}

int main(void)
{
    RUN(regimport_utf16_v5);
    RUN(regimport_no_file_seeds_installdir_and_reports_missing);
    RUN(regimport_32bit_export_gets_wow6432node);
    RUN(regimport_ignores_other_keys_and_bad_store);
    return jk_test_report("test_regimport");
}
