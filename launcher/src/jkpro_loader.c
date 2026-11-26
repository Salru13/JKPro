/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* The JKPro start screen ("kick-off"). See jkpro_loader.h. */
#include "jkpro_loader.h"
#include "jkpro_art.h"
#include "jk_art.h"

#include <stdlib.h>
#include <string.h>

enum {
    LOGO_CAP = 190, LOGO_Y = 330,      /* the logo's letters */
    TOUCH_MS = 900,                    /* one juggle */
    TEXT_Y0 = 600, TEXT_Y1 = 850,      /* the text band */
    TEXT_Y = 630, BAR_Y = 760, BAR_W = 900, BAR_H = 14
};

static uint8_t *g_cache;               /* stadium + logo without its ball */
static JkProLogoBox g_box;
static unsigned g_gen;                 /* bumped at every rebuild */

typedef struct { const uint8_t *pixels; unsigned gen; } Slot;
static Slot g_slots[4];
static int g_slot_next;

static inline uint32_t *px(JkFrame *f, int x, int y) { return (uint32_t *)(f->pixels + (size_t)y * f->stride) + x; }
static inline uint32_t cpx(int x, int y) { return ((const uint32_t *)g_cache)[(size_t)y * JK_W + x]; }

static int build(void)
{
    if (g_cache) return 0;
    g_cache = malloc((size_t)JK_W * JK_H * 4);
    if (!g_cache) return -1;
    JkFrame c = { g_cache, JK_W, JK_H, JK_W * 4 };
    jkpro_stadium(&c);
    int w = jkpro_logo_width(LOGO_CAP);
    g_box = jkpro_logo(&c, (JK_W - w) / 2, LOGO_Y, LOGO_CAP, 0, 256);
    /* the 16 spin positions once now, so no frame pays for them later */
    int d = g_box.ball_d;
    JkFrame t = { malloc((size_t)d * d * 4), (uint32_t)d, (uint32_t)d, (uint32_t)d * 4 };
    if (t.pixels)
        for (int s = 0; s < 16; s++) jkpro_ball(&t, d / 2, d / 2, d, s);
    free(t.pixels);
    g_gen++;
    return 0;
}

void jkpro_loader_release(void)
{
    free(g_cache);
    g_cache = NULL;
    memset(g_slots, 0, sizeof g_slots);
    jkpro_art_release();
}

/* Copies the cache's rectangle back into f (clipped). */
static void restore(JkFrame *f, int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)f->width) x1 = (int)f->width;
    if (y1 > (int)f->height) y1 = (int)f->height;
    if (x1 > JK_W) x1 = JK_W;
    if (y1 > JK_H) y1 = JK_H;
    if (x1 <= x0) return;
    for (int y = y0; y < y1; y++)
        memcpy(px(f, x0, y), g_cache + ((size_t)y * JK_W + (size_t)x0) * 4, (size_t)(x1 - x0) * 4);
}

/* 1 when f already holds this cache generation outside the moving parts. */
static int known(JkFrame *f)
{
    if (f->width < JK_W || f->height < JK_H) return 0;
    static const int SX[4] = { 0, JK_W - 1, 40, JK_W - 41 }, SY[4] = { 0, JK_H - 1, JK_H - 40, 40 };
    for (int i = 0; i < 4; i++)
        if (g_slots[i].pixels == f->pixels && g_slots[i].gen == g_gen) {
            for (int k = 0; k < 4; k++)
                if (*px(f, SX[k], SY[k]) != cpx(SX[k], SY[k])) return 0;
            return 1;
        }
    return 0;
}

static void remember(JkFrame *f)
{
    for (int i = 0; i < 4; i++)
        if (g_slots[i].pixels == f->pixels) { g_slots[i].gen = g_gen; return; }
    g_slots[g_slot_next].pixels = f->pixels;
    g_slots[g_slot_next].gen = g_gen;
    g_slot_next = (g_slot_next + 1) % 4;
}

/* A thin ellipse outline on the grass, additive. */
static void touch_ring(JkFrame *f, int cx, int cy, int rx, int ry, int alpha)
{
    if (rx < 2 || ry < 1 || alpha <= 0) return;
    for (int x = -rx; x <= rx; x++) {
        /* y = ry * sqrt(1 - (x/rx)^2) in integers */
        long q = (long)ry * ry * ((long)rx * rx - (long)x * x) / ((long)rx * rx);
        int y = 0;
        while ((long)(y + 1) * (y + 1) <= q) y++;
        jk_fill_add(f, cx + x, cy - y, 1, 2, 0xFFD27A, alpha);
        jk_fill_add(f, cx + x, cy + y - 1, 1, 2, 0xFFD27A, alpha);
    }
}

/* The ball's soft shadow on the grass. */
static void shadow(JkFrame *f, int cx, int cy, int rx, int ry, int alpha)
{
    for (int y = -ry; y <= ry; y++) {
        long q = (long)rx * rx * ((long)ry * ry - (long)y * y) / ((long)ry * ry);
        int w = 0;
        while ((long)(w + 1) * (w + 1) <= q) w++;
        int a = alpha * (ry - (y < 0 ? -y : y)) / (ry ? ry : 1);
        jk_fill_alpha(f, cx - w, cy + y, 2 * w + 1, 1, 0x000000, a);
    }
}

static void draw(JkFrame *f, uint64_t t_ms, const char *text, int progress, unsigned done, unsigned total,
                 const char *status)
{
    if (!f || !f->pixels) return;
    if (build() != 0) {
        jk_fill(f, 0, 0, (int)f->width, (int)f->height, 0x050913);
        return;
    }
    if (!text) text = "Starting PES 2013…";
    int d = g_box.ball_d, cx = g_box.ball_cx, rest = g_box.ball_cy, ground = rest + d / 2;
    int hop = d * 55 / 100, top = rest - hop * 22 / 10 - d * 13 / 10;

    if (!known(f)) {
        restore(f, 0, 0, JK_W, JK_H);
        remember(f);
    } else {
        restore(f, cx - d * 17 / 10, top, cx + d * 17 / 10, ground + d / 2);
        restore(f, 0, TEXT_Y0, JK_W, TEXT_Y1);
    }

    /* the juggle: a hop per touch, every fourth a higher kick-off */
    uint64_t k = t_ms / TOUCH_MS;
    int p = (int)(t_ms % TOUCH_MS) * 1000 / TOUCH_MS;                 /* 0..999 */
    int big = (k % 4) == 3;
    int h = (int)((long)hop * (big ? 22 : 10) / 10 * 4 * p * (1000 - p) / 1000000);
    int hn = h * 256 / (hop * 22 / 10);                               /* 0..256: how high */
    shadow(f, cx, ground, d * 46 / 100 * (256 - hn * 4 / 10) / 256, d * 8 / 100, 150 * (256 - hn * 6 / 10) / 256);
    if (p < 260) {
        int e = p * 256 / 260;
        touch_ring(f, cx, ground, d / 2 + d * e / 256 * 6 / 10, (d / 2 + d * e / 256 * 6 / 10) * 18 / 100,
                   110 * (256 - e) / 256);
    }
    int by = rest - h;
    jkpro_ball_halo(f, cx, by, d, 256);
    int spin = (int)((t_ms / (big ? 28 : 45)) & 15);
    jkpro_ball(f, cx, by, d, spin);

    /* the text band */
    int tw = jk_text_width(JK_FONT_MENU, text);
    jk_text(f, JK_FONT_MENU, (JK_W - tw) / 2, TEXT_Y, text, 0xF4F7FA);
    if (progress && total) {
        int bx = (JK_W - BAR_W) / 2;
        unsigned dd = done > total ? total : done;
        jk_fill(f, bx, BAR_Y, BAR_W, BAR_H, 0x16283A);
        int fw = (int)((unsigned long long)BAR_W * dd / total);
        jk_fill(f, bx, BAR_Y, fw, BAR_H, 0xFFB82E);
        if (fw > 0) jk_glow(f, bx + fw, BAR_Y + BAR_H / 2, 40, 16, 0xFFD27A, 90);
    }
    if (progress && status && status[0]) {
        int sw = jk_text_width(JK_FONT_BODY, status);
        jk_text(f, JK_FONT_BODY, (JK_W - sw) / 2, BAR_Y + 34, status, 0xA3B6C4);
    }
}

void jkpro_loader_draw(JkFrame *f, uint64_t t_ms, const char *text) { draw(f, t_ms, text, 0, 0, 0, NULL); }

void jkpro_loader_draw_progress(JkFrame *f, uint64_t t_ms, const char *text, unsigned done, unsigned total,
                                const char *status)
{
    draw(f, t_ms, text, 1, done, total, status);
}
