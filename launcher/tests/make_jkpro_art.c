/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro package art and start-screen previews, drawn with JKPro's own art:
 *   build/sce_sys-jkpro/icon0.png (512x512), pic1.png (1920x1080), and
 *   build/preview/jkpro-loader-*.png.
 * Run: launcher/tests/run.sh make_jkpro_art */
#include "jkpro_art.h"
#include "jkpro_loader.h"
#include "jkpro_ui.h"
#include "jk_png.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(void)
{
    mkdir(JK_ROOT_DIR "/build", 0755);
    mkdir(JK_ROOT_DIR "/build/sce_sys-jkpro", 0755);
    mkdir(JK_ROOT_DIR "/build/preview", 0755);
    JkFrame f = { calloc((size_t)JK_W * JK_H, 4), JK_W, JK_H, JK_W * 4 };
    int rc = 0;

    /* pic1: the stadium with the logo large, "PS4" under it */
    jkpro_stadium(&f);
    int cap = 200, w = jkpro_logo_width(cap);
    jkpro_logo(&f, (JK_W - w) / 2, 330, cap, 1, 256);
    const char *sub = "P  S  4";
    int sw = jk_text_width(JK_FONT_HEAD, sub);
    jk_text(&f, JK_FONT_HEAD, (JK_W - sw) / 2, 330 + cap + 70, sub, 0xDDE6EE);
    rc |= jk_png_write(JK_ROOT_DIR "/build/sce_sys-jkpro/pic1.png", &f);

    /* icon0: a 512x512 window on the stadium around a floodlight, the logo in its middle */
    JkFrame ic = { calloc(512 * 512, 4), 512, 512, 512 * 4 };
    jkpro_stadium(&f);
    for (int y = 0; y < 512; y++)
        memcpy(ic.pixels + (size_t)y * ic.stride, f.pixels + (size_t)(y + 300) * f.stride + (size_t)704 * 4, 512 * 4);
    int icap = 92, iw = jkpro_logo_width(icap);
    jkpro_logo(&ic, (512 - iw) / 2, 196, icap, 1, 256);
    const char *isub = "P  S  4";
    int isw = jk_text_width(JK_FONT_BODY, isub);
    jk_text(&ic, JK_FONT_BODY, (512 - isw) / 2, 196 + icap + 46, isub, 0xDDE6EE);
    rc |= jk_png_write(JK_ROOT_DIR "/build/sce_sys-jkpro/icon0.png", &ic);
    free(ic.pixels);

    /* start-screen previews */
    static const struct { uint64_t t; const char *name; } T[] = {
        { 0, "jkpro-loader-touch" }, { 450, "jkpro-loader-hop" }, { 3150, "jkpro-loader-kickoff" } };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        char p[512];
        jkpro_loader_draw(&f, T[i].t, NULL);
        snprintf(p, sizeof p, JK_ROOT_DIR "/build/preview/%s.png", T[i].name);
        rc |= jk_png_write(p, &f);
    }
    jkpro_loader_draw_progress(&f, 1200, "Getting PES 2013 ready", 204, 330, "Setting up the Windows layer (first start only)");
    rc |= jk_png_write(JK_ROOT_DIR "/build/preview/jkpro-loader-progress.png", &f);
    /* design preview (design/EXPANSION.md): a later release's shelf with three games */
    {
        JpModel m;
        memset(&m, 0, sizeof m);
        static const struct { JpTileState st; int mod; const char *folder, *title; } G[3] = {
            { JP_ST_READY, 0, "PES13", "PES 2013" }, { JP_ST_NEEDS_PATCH, 0, "PES6", "PES 6" },
            { JP_ST_READY, 1, "PES12-Patch", "PES 2012" } };
        for (int i = 0; i < 3; i++) {
            m.tiles[i].state = G[i].st;
            m.tiles[i].modded = G[i].mod;
            snprintf(m.tiles[i].folder, sizeof m.tiles[i].folder, "%s", G[i].folder);
            snprintf(m.tiles[i].title, sizeof m.tiles[i].title, "%s", G[i].title);
        }
        m.ntiles = 3;
        m.game_tile = 1;
        m.tile_focus = 1;
        m.quality = 1;
        jkpro_draw(&f, &m);
        rc |= jk_png_write(JK_ROOT_DIR "/build/preview/jkpro-expansion-shelf.png", &f);
        jkpro_draw_release();
    }
    free(f.pixels);
    jkpro_loader_release();
    printf("jkpro art in build/sce_sys-jkpro and build/preview (rc=%d)\n", rc);
    return rc ? 1 : 0;
}
