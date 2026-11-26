/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro data packs: header check, version, install / remove / use an older one. */
#include "jkpro_datapack.h"
#include "jkpro_ui.h"
#include "jk_fs.h"
#include "jk_test.h"
#include "jk_tmp.h"

#include <dirent.h>
#include <stdint.h>

static const char *STORE =
    "WINE REGISTRY Version 2\n;; All keys relative to \\\\Machine\n\n#arch=win64\n\n";

static void put32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }

/* A small AFS file: count entries; the last two are WESYS records with version ver
 * (NULL: no version records). broken: an entry points past the end. */
static void make_pack(const char *dir, const char *rel, uint32_t count, const char *ver, int broken)
{
    uint32_t hdr = 8 + count * 8, rec = 0x40, data = 64;
    size_t total = hdr + (size_t)(count - 2) * data + 2 * rec;
    unsigned char *b = calloc(1, total);
    memcpy(b, "AFS\0", 4);
    put32(b + 4, count);
    uint32_t off = hdr;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t sz = i + 2 >= count ? rec : data;
        put32(b + 8 + i * 8, off);
        put32(b + 12 + i * 8, broken && i == 1 ? 0x7fffffff : sz);
        if (i + 2 >= count && ver) {
            b[off] = 0; b[off + 1] = 1; b[off + 2] = 0;
            memcpy(b + off + 3, "WESYS", 5);
            memcpy(b + off + 0x30, ver, strlen(ver));
        }
        off += sz;
    }
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", dir, rel);
    tmp_put(dir, rel, "");
    FILE *f = fopen(p, "wb");
    fwrite(b, 1, total, f);
    fclose(f);
    free(b);
}

static void check_header(void)
{
    char d[256], p[512];
    tmp_dir(d, sizeof d);
    JkProPackInfo in;
    make_pack(d, "a.img", 20, "6.00", 0);
    snprintf(p, sizeof p, "%s/a.img", d);
    CHECK_EQ(jkpro_pack_check(p, &in), 1);
    CHECK(!strcmp(in.version, "6.00"));
    CHECK_EQ(in.entries, 20);
    CHECK(jkpro_pack_magic(p));
    make_pack(d, "nover.img", 5, NULL, 0);
    snprintf(p, sizeof p, "%s/nover.img", d);
    CHECK_EQ(jkpro_pack_check(p, &in), 1);
    CHECK(in.version[0] == 0);                      /* the size is shown instead */
    make_pack(d, "broken.img", 5, "6.00", 1);
    snprintf(p, sizeof p, "%s/broken.img", d);
    CHECK_EQ(jkpro_pack_check(p, &in), 0);          /* an entry outside the file */
    tmp_put(d, "fake.img", "not an archive at all");
    snprintf(p, sizeof p, "%s/fake.img", d);
    CHECK_EQ(jkpro_pack_check(p, &in), 0);
    CHECK(!jkpro_pack_magic(p));
    tmp_rm_rf(d);
}

typedef struct {
    char root[256], game[300], sys[300], prefix[300], files[300], drop[300], dp[300];
    JkProServices s;
    JpApp a;
} Fx;

static int fx_wall_n;
static int fx_wall(void *c, char *out, size_t cap) { (void)c; snprintf(out, cap, "2026-11-25_%04d", ++fx_wall_n); return 0; }
static uint64_t fx_now(void *c) { (void)c; return 0; }
static void fx_sleep(void *c, uint32_t us) { (void)c; (void)us; }

static void fx_open(Fx *x)
{
    memset(x, 0, sizeof *x);
    tmp_dir(x->root, sizeof x->root);
    snprintf(x->game, sizeof x->game, "%s/PES13", x->root);
    snprintf(x->sys, sizeof x->sys, "%s/System", x->root);
    snprintf(x->prefix, sizeof x->prefix, "%s/System/prefix", x->root);
    snprintf(x->files, sizeof x->files, "%s/System/jkpro-files", x->root);
    snprintf(x->drop, sizeof x->drop, "%s/Saves/PES13/Import", x->root);
    snprintf(x->dp, sizeof x->dp, "%s/DataPacks/PES13", x->root);
    tmp_put(x->root, "System/prefix/system.reg", STORE);
    tmp_put(x->root, "System/jkpro-files/d3d9.dll", "OUR");
    tmp_put(x->root, "PES13/pes2013.exe", "MZ");
    tmp_put(x->root, "PES13/settings.exe", "MZ");
    tmp_put(x->root, "PES13/img/dt01.img", "img");
    tmp_put(x->root, "Saves/PES13/Import/.keep", "");
    x->s = (JkProServices){ x->game, x->prefix, x->sys, x->files, x->drop, NULL, x, NULL, NULL, NULL, fx_now, fx_sleep,
                            NULL, fx_wall, NULL, NULL, NULL, NULL, NULL, x->dp };
}

static void fx_init(Fx *x) { CHECK_EQ(jkpro_app_init(&x->a, &x->s, NULL), 0); }
static void press(Fx *x, uint32_t b) { jkpro_app_press(&x->a, b); }

static int exists(const char *root, const char *rel)
{
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    return jk_fs_exists(p);
}

static int count_old(const Fx *x)
{
    char d[512];
    snprintf(d, sizeof d, "%s/old", x->dp);
    DIR *dd = opendir(d);
    if (!dd) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(dd))) if (!strncmp(e->d_name, "dt0f-", 5)) n++;
    closedir(dd);
    return n;
}

#define LIVE "DataPacks/PES13/KONAMI/Pro Evolution Soccer 2013/download/dt0f.img"

static void open_datapacks(Fx *x)
{
    press(x, JKPRO_PAD_TRIANGLE);
    x->a.m.menu_focus = JP_MENU_DATAPACKS;
    press(x, JKPRO_PAD_CROSS);
}

static void install_switch_remove_use_old(void)
{
    Fx x;
    fx_open(&x);
    make_pack(x.root, "Saves/PES13/Import/dt0f.img", 30, "6.00", 0);
    tmp_put(x.root, "Saves/PES13/Import/OPTION.bin", "teams");
    fx_init(&x);
    JpModel *m = &x.a.m;
    CHECK_EQ(m->dp_on, 1);
    CHECK_EQ(m->dp_wait, 1);
    CHECK(!strcmp(m->dp_wait_ver, "6.00"));
    CHECK_EQ(m->import_files, 1);            /* the pack is not a save: only OPTION.bin */
    open_datapacks(&x);
    CHECK_EQ(m->screen, JP_SCR_DATAPACKS);
    press(&x, JKPRO_PAD_CROSS);              /* Install */
    CHECK_EQ(m->dp_msg_bad, 0);
    CHECK(exists(x.root, LIVE));
    CHECK(!exists(x.root, "Saves/PES13/Import/dt0f.img"));   /* moved, not copied */
    CHECK_EQ(m->dp_have, 1);
    CHECK(!strcmp(m->dp_ver, "6.00"));
    /* a newer pack under another name: installed, the 6.00 goes to old/ */
    make_pack(x.root, "Saves/PES13/Import/DLC7.img", 30, "7.00", 0);
    fx_init(&x);
    open_datapacks(&x);
    CHECK_EQ(m->dp_focus, 0);
    press(&x, JKPRO_PAD_CROSS);
    CHECK(!strcmp(m->dp_ver, "7.00"));
    CHECK_EQ(count_old(&x), 1);
    CHECK(!strcmp(m->dp_old[0].ver, "6.00"));
    /* use the old 6.00 again: 7.00 goes to old/ */
    int kinds[8], idx[8], n = jkpro_dp_rows(m, kinds, idx, 8);
    CHECK_EQ(n, 2);                          /* remove, use 6.00 */
    m->dp_focus = 1;
    press(&x, JKPRO_PAD_CROSS);
    CHECK(!strcmp(m->dp_ver, "6.00"));
    CHECK_EQ(count_old(&x), 1);
    CHECK(!strcmp(m->dp_old[0].ver, "7.00"));
    /* remove: back to the base data, the pack kept in old/ */
    m->dp_focus = 0;
    press(&x, JKPRO_PAD_CROSS);
    CHECK_EQ(m->dp_have, 0);
    CHECK(!exists(x.root, LIVE));
    CHECK_EQ(count_old(&x), 2);
    CHECK(exists(x.root, "PES13/img/dt01.img"));   /* the game folder is never touched */
    tmp_rm_rf(x.root);
}

static void old_keeps_three_and_a_fake_stays_in_import(void)
{
    Fx x;
    fx_open(&x);
    fx_init(&x);
    for (int i = 0; i < 5; i++) {
        char v[16], rel[64];
        snprintf(v, sizeof v, "%d.00", i + 1);
        snprintf(rel, sizeof rel, "Saves/PES13/Import/pack%d.img", i);
        make_pack(x.root, rel, 12, v, 0);
        fx_init(&x);
        open_datapacks(&x);
        press(&x, JKPRO_PAD_CROSS);
    }
    CHECK(!strcmp(x.a.m.dp_ver, "5.00"));
    CHECK_EQ(count_old(&x), JKPRO_PACKS_OLD);
    CHECK(!strcmp(x.a.m.dp_old[0].ver, "4.00"));  /* newest first */
    /* not a pack: stays, with a note, and is never taken as a save either */
    tmp_put(x.root, "Saves/PES13/Import/dt0f.img", "garbage");
    fx_init(&x);
    CHECK_EQ(x.a.m.dp_wait, 0);
    CHECK(strstr(x.a.m.dp_bad, "not a PES 2013 data pack") != NULL);
    CHECK_EQ(x.a.m.import_files, 0);
    CHECK(exists(x.root, "Saves/PES13/Import/dt0f.img"));
    /* without datapacks_dir there is no DATA PACKS row */
    x.s.datapacks_dir = NULL;
    fx_init(&x);
    CHECK_EQ(x.a.m.dp_on, 0);
    press(&x, JKPRO_PAD_TRIANGLE);
    for (int i = 0; i < 8; i++) press(&x, JKPRO_PAD_DOWN);
    CHECK_EQ(x.a.m.menu_focus, JP_MENU_SAVES);
    tmp_rm_rf(x.root);
}

int main(void)
{
    RUN(check_header);
    RUN(install_switch_remove_use_old);
    RUN(old_keeps_three_and_a_fake_stays_in_import);
    return jk_test_report("test_jkpro_datapack");
}
