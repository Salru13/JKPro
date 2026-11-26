/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro game catalog and folder detection (jk_games). */
#include "jk_games.h"
#include "jk_test.h"
#include "jk_tmp.h"

#include <stdint.h>

/* A minimal PE32 file: MZ, PE header, section table with the given names.
 * A ".rsrc" section carries a VS_FIXEDFILEINFO with file version ms.ls. Extra
 * bytes (e.g. a GUID string) can be placed at a file offset. */
static void put_pe(const char *dir, const char *rel, const char *const *sections, int nsec,
                   uint32_t ver_ms, uint32_t ver_ls, long extra_off, const char *extra)
{
    size_t size = 0x80000;
    if (extra_off > 0 && (size_t)extra_off + 64 > size) size = (size_t)extra_off + 64;
    unsigned char *b = calloc(1, size);
    b[0] = 'M'; b[1] = 'Z';
    uint32_t pe = 0x80;
    memcpy(b + 0x3c, &pe, 4);
    memcpy(b + pe, "PE\0\0", 4);
    uint16_t machine = 0x14c, n = (uint16_t)nsec, optsz = 0xe0;
    memcpy(b + pe + 4, &machine, 2);
    memcpy(b + pe + 6, &n, 2);
    memcpy(b + pe + 20, &optsz, 2);
    uint32_t sec = pe + 24 + optsz;
    for (int i = 0; i < nsec; i++) {
        unsigned char *s = b + sec + (size_t)i * 40;
        strncpy((char *)s, sections[i], 8);
        uint32_t va = 0x1000u * (uint32_t)(i + 1), vs = 0x1000, raw = 0x1000u * (uint32_t)(i + 1), rs = 0x1000;
        memcpy(s + 8, &vs, 4); memcpy(s + 12, &va, 4); memcpy(s + 16, &rs, 4); memcpy(s + 20, &raw, 4);
        if (!strcmp(sections[i], ".rsrc")) {
            uint32_t sig = 0xFEEF04BD, sv = 0x10000;
            memcpy(b + raw + 0x100, &sig, 4);
            memcpy(b + raw + 0x104, &sv, 4);
            memcpy(b + raw + 0x108, &ver_ms, 4);
            memcpy(b + raw + 0x10c, &ver_ls, 4);
        }
    }
    if (extra_off > 0 && extra) memcpy(b + extra_off, extra, strlen(extra));
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", dir, rel);
    for (char *c = p + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(p, 0755); *c = '/'; }
    FILE *f = fopen(p, "wb");
    fwrite(b, 1, size, f);
    fclose(f);
    free(b);
}

static const char *PES13_GAME =
    "[identity]\nid = pes2013\nname = Pro Evolution Soccer 2013\nexe = pes2013.exe\n"
    "marker = img/dt01.img\nmarker = settings.exe\nversion = pe\n"
    "[foreign]\nloader_dir = kitserver\nmodded_file = PESEDIT.exe\n";
static const char *PES6_GAME =
    "[identity]\nid = pes6\nname = Pro Evolution Soccer 6\nexe = PES6.exe\n"
    "marker = settings.exe\nmarker_ext = dat/.afs\nversion = guid\n"
    "[guid]\n0x77dca8 = {20418978-752D-491c-AF1E-A2EDE086E000} 1.00\n"
    "0x77eca8 = {20418978-752D-491c-AF1E-A2EDE086E000} 1.10\n"
    "0x77eca8 = {EEF9975D-2F76-42c6-AC44-8F84C6B4E68D} WE2007\n"
    "[foreign]\nloader_dir = kitserver\n";

static const char *const PLAIN[] = { ".text", ".rdata", ".data", ".rsrc" };
static const char *const SECUROM[] = { ".text", ".rdata", ".data", ".rsrc", ".securom" };

static char base[200];

/* base/defs holds the catalog, root = base/JKPro the scanned folder. */
static int setup(char *root, char *defs, JkGameDef *d)
{
    tmp_dir(base, sizeof base);
    snprintf(root, 256, "%s/JKPro", base);
    mkdir(root, 0755);
    snprintf(defs, 300, "%s/defs", base);
    tmp_put(defs, "pes2013.game", PES13_GAME);
    tmp_put(defs, "pes6.game", PES6_GAME);
    tmp_put(defs, "notes.txt", "not a game file");
    return jk_games_load_defs(defs, d, 8);
}

static void pes13_folder(const char *root, const char *folder)
{
    char g[512];
    snprintf(g, sizeof g, "%s/%s", root, folder);
    put_pe(g, "pes2013.exe", PLAIN, 4, 0x00010004, 0x00000000, 0, NULL);
    tmp_put(g, "settings.exe", "x");
    tmp_put(g, "img/dt01.img", "x");
}

static void load_defs_reads_both(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    CHECK_EQ(setup(root, defs, d), 2);
    int i13 = !strcmp(d[0].id, "pes2013") ? 0 : 1;
    CHECK(!strcmp(d[i13].exe, "pes2013.exe"));
    CHECK_EQ(d[i13].nmarkers, 2);
    CHECK_EQ(d[1 - i13].nguids, 3);
    tmp_rm_rf(base);
}

static void scan_finds_pes13_in_any_case(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    pes13_folder(root, "pes2013 patch");
    pes13_folder(root, "PES13");
    JkGameFound f[8];
    CHECK_EQ(jk_games_scan(root, d, n, f, 8), 2);
    CHECK(!strcmp(f[0].folder, "PES13"));
    CHECK(!strcmp(f[1].folder, "pes2013 patch"));
    CHECK(!strcmp(f[0].game_id, "pes2013"));
    CHECK_EQ(f[0].verdict, JK_GV_READY);
    CHECK(!strcmp(f[0].version, "1.4.0.0"));
    tmp_rm_rf(base);
}

static void scan_exe_name_case_ignored(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    char g[512];
    snprintf(g, sizeof g, "%s/X", root);
    put_pe(g, "PES2013.EXE", PLAIN, 4, 0x00010004, 0x00000000, 0, NULL);
    tmp_put(g, "Settings.exe", "x");
    tmp_put(g, "IMG/DT01.IMG", "x");
    JkGameFound f[4];
    CHECK_EQ(jk_games_scan(root, d, n, f, 4), 1);
    CHECK_EQ(f[0].verdict, JK_GV_READY);
    CHECK(!strcmp(f[0].exe, "PES2013.EXE"));
    tmp_rm_rf(base);
}

static void scan_kitserver_is_modded(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    pes13_folder(root, "PES13");
    char g[512];
    snprintf(g, sizeof g, "%s/PES13", root);
    tmp_put(g, "kitserver/config.txt", "x");
    JkGameFound f[4];
    CHECK_EQ(jk_games_scan(root, d, n, f, 4), 1);
    CHECK_EQ(f[0].verdict, JK_GV_MODDED);
    tmp_rm_rf(base);
}

static void scan_securom_is_needs_disc(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    char g[512];
    snprintf(g, sizeof g, "%s/PES6", root);
    put_pe(g, "PES6.exe", SECUROM, 5, 0, 0, 0x77dca8, "{20418978-752D-491c-AF1E-A2EDE086E000}");
    tmp_put(g, "settings.exe", "x");
    tmp_put(g, "dat/0_text.afs", "x");
    JkGameFound f[4];
    CHECK_EQ(jk_games_scan(root, d, n, f, 4), 1);
    CHECK(!strcmp(f[0].game_id, "pes6"));
    CHECK_EQ(f[0].verdict, JK_GV_NEEDS_DISC);
    CHECK(!strcmp(f[0].version, "1.00"));
    tmp_rm_rf(base);
}

static void pes6_guid_probe_labels_110_and_we2007(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    char g[512];
    snprintf(g, sizeof g, "%s/A", root);
    put_pe(g, "PES6.exe", PLAIN, 4, 0, 0, 0x77eca8, "{20418978-752D-491c-AF1E-A2EDE086E000}");
    tmp_put(g, "settings.exe", "x");
    tmp_put(g, "dat/e_text.afs", "x");
    snprintf(g, sizeof g, "%s/B", root);
    put_pe(g, "PES6.exe", PLAIN, 4, 0, 0, 0x77eca8, "{EEF9975D-2F76-42c6-AC44-8F84C6B4E68D}");
    tmp_put(g, "settings.exe", "x");
    tmp_put(g, "dat/e_text.afs", "x");
    JkGameFound f[4];
    CHECK_EQ(jk_games_scan(root, d, n, f, 4), 2);
    CHECK(!strcmp(f[0].version, "1.10"));
    CHECK(!strcmp(f[1].version, "WE2007"));
    CHECK_EQ(f[0].verdict, JK_GV_READY);
    tmp_rm_rf(base);
}

static void scan_missing_marker_reports_file(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    char g[512];
    snprintf(g, sizeof g, "%s/PES13", root);
    put_pe(g, "pes2013.exe", PLAIN, 4, 0x00010004, 0x00000000, 0, NULL);
    tmp_put(g, "settings.exe", "x");
    JkGameFound f[4];
    CHECK_EQ(jk_games_scan(root, d, n, f, 4), 1);
    CHECK_EQ(f[0].verdict, JK_GV_MISSING);
    CHECK(!strcmp(f[0].missing, "img/dt01.img"));
    tmp_rm_rf(base);
}

static void scan_unknown_folder_and_reserved_skipped(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    tmp_put(root, "Holiday photos/a.jpg", "x");
    tmp_put(root, "System/state/x", "x");
    tmp_put(root, "saves/x", "x");
    tmp_put(root, "Logs/x", "x");
    tmp_put(root, "Mods/x", "x");
    tmp_put(root, "a-file.txt", "x");
    JkGameFound f[8];
    CHECK_EQ(jk_games_scan(root, d, n, f, 8), 1); /* only "Holiday photos" */
    int unknown = 0;
    for (int i = 0; i < 1; i++) unknown += f[i].verdict == JK_GV_UNKNOWN && !f[i].game_id[0];
    CHECK_EQ(unknown, 1);
    tmp_rm_rf(base);
}

static void scan_respects_max(void)
{
    char root[256], defs[300];
    JkGameDef d[8];
    int n = setup(root, defs, d);
    pes13_folder(root, "A");
    pes13_folder(root, "B");
    pes13_folder(root, "C");
    JkGameFound f[2];
    CHECK_EQ(jk_games_scan(root, d, n, f, 2), 2);
    tmp_rm_rf(base);
}

int main(void)
{
    RUN(load_defs_reads_both);
    RUN(scan_finds_pes13_in_any_case);
    RUN(scan_exe_name_case_ignored);
    RUN(scan_kitserver_is_modded);
    RUN(scan_securom_is_needs_disc);
    RUN(pes6_guid_probe_labels_110_and_we2007);
    RUN(scan_missing_marker_reports_file);
    RUN(scan_unknown_folder_and_reserved_skipped);
    RUN(scan_respects_max);
    return jk_test_report("test_games");
}
