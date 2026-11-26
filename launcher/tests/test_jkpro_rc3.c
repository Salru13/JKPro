/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * rc3 (2026-11-25): the second PLAY crashed on the console once a real save
 * existed in docs_dir. Host reproduction of that state through the whole
 * jkpro_ui_run loop (open -> draw -> PLAY), also on small thread stacks.
 */
#include "jkpro_ui.h"
#include "jkpro_loader.h"
#include "jk_fs.h"
#include "jk_test.h"
#include "jk_tmp.h"

#include <pthread.h>

static const char *STORE =
    "WINE REGISTRY Version 2\n;; All keys relative to \\\\Machine\n\n#arch=win64\n\n"
    "[Software\\\\Microsoft\\\\Windows] 1700000000\n#time=1\n\"A\"=\"b\"\n\n";
static const char *REG_OK =
    "Windows Registry Editor Version 5.00\n\n[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\KONAMI\\PES2013]\n"
    "\"code\"=\"AAAA-BBBB-CCCC-DDDD-EEEE\"\n";

typedef struct {
    char root[256], game[300], sys[300], prefix[300], files[300], drop[300], docs[300], logs[300];
    JkProServices s;
    int frame, presents;
    uint64_t t;
} Host;

static int h_present(void *c, const JkFrame *f) { Host *h = c; (void)f; h->presents++; return 0; }
static int h_pad(void *c, uint32_t *held, uint32_t *pressed)
{
    Host *h = c;
    *held = 0;
    h->frame++;
    /* a few idle frames, then X (PLAY on the patched tile); X again in case a note is up */
    *pressed = (h->frame == 5 || h->frame == 9) ? JKPRO_PAD_CROSS : 0;
    return 0;
}
static uint64_t h_now(void *c) { Host *h = c; h->t += 16000000ull; return h->t; }
static void h_sleep(void *c, uint32_t us) { (void)c; (void)us; }
static int h_wall(void *c, char *out, size_t cap) { (void)c; snprintf(out, cap, "2026-11-25_1700"); return 0; }

static void put_random(const char *dir, const char *rel, size_t n)
{
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", dir, rel);
    tmp_put(dir, rel, "");
    FILE *f = fopen(p, "wb");
    uint32_t x = 12345;
    for (size_t i = 0; i < n; i++) { x = x * 1103515245u + 12345u; fputc((int)(x >> 16) & 255, f); }
    fclose(f);
}

static void host_open(Host *h, int real_save)
{
    memset(h, 0, sizeof *h);
    tmp_dir(h->root, sizeof h->root);
    snprintf(h->game, sizeof h->game, "%s/PES13", h->root);
    snprintf(h->sys, sizeof h->sys, "%s/System", h->root);
    snprintf(h->prefix, sizeof h->prefix, "%s/System/prefix", h->root);
    snprintf(h->files, sizeof h->files, "%s/System/jkpro-files", h->root);
    snprintf(h->docs, sizeof h->docs, "%s/Saves/PES13", h->root);
    snprintf(h->drop, sizeof h->drop, "%s/Saves/PES13/Import", h->root);
    snprintf(h->logs, sizeof h->logs, "%s/Logs", h->root);
    tmp_put(h->root, "System/prefix/system.reg", STORE);
    tmp_put(h->root, "System/prefix/user.reg", STORE);
    tmp_put(h->root, "System/prefix/userdef.reg", STORE);
    /* the old prefix Documents exists, with no save dir */
    tmp_put(h->root, "System/prefix/" JKPRO_DOCS_REL "/settings.dat", "old");
    tmp_put(h->root, "System/jkpro-files/d3d9.dll", "OUR-D3D9-CLIENT");
    tmp_put(h->root, "PES13/pes2013.exe", "MZ");
    tmp_put(h->root, "PES13/settings.exe", "MZ");
    tmp_put(h->root, "PES13/img/dt01.img", "img");
    tmp_put(h->root, "PES13/PES_2013_Registry_Fix.reg", REG_OK);
    h->s = (JkProServices){ h->game, h->prefix, h->sys, h->files, h->drop, NULL, h, h_present, h_pad, NULL, h_now,
                            h_sleep, NULL, h_wall, NULL, h->logs, h->docs, NULL, "JKPro 1.0", NULL };
    /* first run: PATCH (+ PLAY) the way the owner did it */
    static JpApp a;
    jkpro_app_init(&a, &h->s, NULL);
    jkpro_app_press(&a, JKPRO_PAD_CROSS);
    jkpro_app_press(&a, JKPRO_PAD_CROSS);
    if (real_save) put_random(h->docs, JKPRO_DOCS_SUB "/save/OPTION.bin", 402008);
}

static void *run(void *arg)
{
    Host *h = arg;
    JkProDecision d;
    int rc = jkpro_ui_run(&h->s, &d);
    return (void *)(intptr_t)(rc == 0 && d.action == JKPRO_START ? 1 : 0);
}

static int run_on_stack(Host *h, size_t stack)
{
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, stack);
    pthread_t t;
    if (pthread_create(&t, &at, run, h) != 0) return -1;
    void *r = NULL;
    pthread_join(t, &r);
    pthread_attr_destroy(&at);
    return (int)(intptr_t)r;
}

static void second_play_with_a_real_save(void)
{
    Host h;
    host_open(&h, 1);
    CHECK_EQ(run_on_stack(&h, 8u << 20), 1);       /* the cycle=1 open and PLAY */
    CHECK(h.presents > 0);
    char bdir[512];
    snprintf(bdir, sizeof bdir, "%s/backup/saves/PES13", h.sys);
    CHECK(jk_fs_is_dir(bdir));                     /* the save was backed up before PLAY */
    tmp_rm_rf(h.root);
}

static void second_play_on_small_stacks(void)
{
    static const size_t S[] = { 512u << 10, 256u << 10 };
    for (size_t i = 0; i < sizeof S / sizeof S[0]; i++) {
        Host h;
        host_open(&h, 1);
        fprintf(stderr, "  stack %zu KB\n", S[i] >> 10);
        CHECK_EQ(run_on_stack(&h, S[i]), 1);
        tmp_rm_rf(h.root);
    }
}

static void two_runs_in_one_process_with_the_loader_between(void)
{
    Host h;
    host_open(&h, 0);
    CHECK_EQ(run_on_stack(&h, 8u << 20), 1);       /* cycle 0: PLAY */
    JkFrame f = { calloc((size_t)JK_W * JK_H, 4), JK_W, JK_H, JK_W * 4 };
    for (int i = 0; i < 30; i++) jkpro_loader_draw(&f, (uint64_t)i * 16, NULL);
    jkpro_loader_release();                        /* the game's first frame */
    put_random(h.docs, JKPRO_DOCS_SUB "/save/OPTION.bin", 402008);   /* the game saved */
    h.frame = 0;
    CHECK_EQ(run_on_stack(&h, 8u << 20), 1);       /* cycle 1: the UI again, then PLAY */
    for (int i = 0; i < 30; i++) jkpro_loader_draw(&f, (uint64_t)i * 16, NULL);
    jkpro_loader_release();
    free(f.pixels);
    tmp_rm_rf(h.root);
}

int main(void)
{
    RUN(two_runs_in_one_process_with_the_loader_between);
    RUN(second_play_with_a_real_save);
    RUN(second_play_on_small_stacks);
    return jk_test_report("test_jkpro_rc3");
}
