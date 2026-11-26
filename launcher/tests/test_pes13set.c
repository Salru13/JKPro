/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* PES 2013 settings.dat writer (jk_pes13set): byte-equal to Konami's settings.exe output. */
#include "jk_pes13set.h"
#include "jk_test.h"
#include "jk_tmp.h"

/* settings.exe samples (owner's PC, not committed): checked when present. */
#define EV JK_ROOT_DIR "/../evidence/pes13/settings/"

static int load(const char *name, unsigned char *d)
{
    FILE *f = fopen(name, "rb");
    if (!f) return -1;
    size_t n = fread(d, 1, JK_PES13SET_SIZE + 1, f);
    fclose(f);
    return n == JK_PES13SET_SIZE ? 0 : -1;
}

static void default_crc_known(void)
{
    unsigned char d[JK_PES13SET_SIZE];
    JkPes13Settings s = { 1280, 720, 1, 2, 0, 1 };
    CHECK_EQ(jk_pes13set_build(&s, d), 0);
    CHECK_EQ(jk_pes13set_crc(d), 0x3d6e);                 /* settings.exe default */
    CHECK_EQ(d[0x0C] | (d[0x0D] << 8), 0x3d6e);
    CHECK(!memcmp(d, "WECF", 4));
}

static void final_1080_xinput_crc_known(void)
{
    unsigned char d[JK_PES13SET_SIZE];
    JkPes13Settings s = { 1920, 1080, 1, 2, 1, 1 };
    CHECK_EQ(jk_pes13set_build(&s, d), 0);
    CHECK_EQ(d[0x0C] | (d[0x0D] << 8), 0x3b3c);          /* settings.exe 1080p fs high XInput */
    CHECK_EQ(d[0x0E], 0xcb);
    CHECK_EQ(d[0x10] | (d[0x11] << 8), 1920);
    CHECK_EQ(d[0x14] | (d[0x15] << 8), 1080);
}

static void equals_settings_exe_when_samples_present(void)
{
    unsigned char want[JK_PES13SET_SIZE], d[JK_PES13SET_SIZE];
    if (load(EV "H-final-1080-fs-high-vsync-xinput.dat", want)) { fprintf(stderr, "  (samples absent: skipped)\n"); return; }
    JkPes13Settings s = { 1920, 1080, 1, 2, 1, 1 };
    jk_pes13set_build(&s, d);
    CHECK(!memcmp(d, want, sizeof d));
}

static void bad_values_refused(void)
{
    unsigned char d[JK_PES13SET_SIZE];
    JkPes13Settings s = { 100, 100, 1, 2, 1, 1 };
    CHECK_EQ(jk_pes13set_build(&s, d), -1);
    JkPes13Settings q = { 1280, 720, 1, 3, 1, 1 };
    CHECK_EQ(jk_pes13set_build(&q, d), -1);
}

static void write_makes_folders(void)
{
    char dir[256], path[400];
    tmp_dir(dir, sizeof dir);
    snprintf(path, sizeof path, "%s/KONAMI/Pro Evolution Soccer 2013/settings.dat", dir);
    JkPes13Settings s = { 1280, 720, 1, 1, 1, 1 };
    CHECK_EQ(jk_pes13set_write(path, &s), 0);
    unsigned char d[JK_PES13SET_SIZE];
    CHECK_EQ(load(path, d), 0);
    CHECK_EQ(d[0x1C], 1);
    tmp_rm_rf(dir);
}

/* The title's lock at every start (owner 2026-11-25): 1920x1080, full screen, High,
 * XInput, vsync, written under the game's Documents whatever was there before. */
static void settings_dat_forced_1080_fullscreen(void)
{
    char dir[256], path[400];
    tmp_dir(dir, sizeof dir);
    snprintf(path, sizeof path, "%s/KONAMI/Pro Evolution Soccer 2013/settings.dat", dir);
    JkPes13Settings low = { 1280, 720, 0, 0, 0, 0 };
    CHECK_EQ(jk_pes13set_write(path, &low), 0);            /* the player chose 720p windowed Low */
    CHECK_EQ(jk_pes13set_write_locked(dir), 0);
    unsigned char d[JK_PES13SET_SIZE];
    CHECK_EQ(load(path, d), 0);
    CHECK_EQ(d[0x10] | (d[0x11] << 8), 1920);
    CHECK_EQ(d[0x14] | (d[0x15] << 8), 1080);
    CHECK_EQ(d[0x1C], 2);                                  /* High */
    CHECK_EQ(d[0x20], 1);                                  /* full screen */
    CHECK_EQ(d[0x0E] & 0x09, 0x09);                        /* vsync + XInput */
    CHECK_EQ(d[0x0C] | (d[0x0D] << 8), jk_pes13set_crc(d));
    CHECK_EQ(d[0x0C] | (d[0x0D] << 8), 0x3b3c);           /* settings.exe 1080p fs high XInput */
    CHECK_EQ(jk_pes13set_write_locked(NULL), -1);
    tmp_rm_rf(dir);
}

int main(void)
{
    RUN(default_crc_known);
    RUN(final_1080_xinput_crc_known);
    RUN(equals_settings_exe_when_samples_present);
    RUN(bad_values_refused);
    RUN(write_makes_folders);
    RUN(settings_dat_forced_1080_fullscreen);
    return jk_test_report("test_pes13set");
}
