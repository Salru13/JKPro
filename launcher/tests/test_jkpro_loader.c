/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro start screen: the fast path draws the same pixels as a full draw,
 * foreign content is noticed, and a frame is cheap. */
#include "jkpro_art.h"
#include "jkpro_loader.h"
#include "jk_test.h"

#include <stdlib.h>
#include <time.h>

static JkFrame frame(void)
{
    JkFrame f = { calloc((size_t)JK_W * JK_H, 4), JK_W, JK_H, JK_W * 4 };
    return f;
}

static void fast_path_matches_full_draw(void)
{
    JkFrame a = frame(), b = frame();
    static const uint64_t T[] = { 0, 130, 450, 899, 2900, 3150, 3555, 9999 };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        jkpro_loader_draw(&a, T[i], NULL);                 /* a: every step on the same buffer (fast path) */
        jkpro_loader_release();                            /* b: a fresh cache and a full draw */
        jkpro_loader_draw(&b, T[i], NULL);
        jkpro_loader_release();
        jkpro_loader_draw(&a, 0, NULL);                    /* rebuild a's cache generation, then the step */
        jkpro_loader_draw(&a, T[i], NULL);
        CHECK(!memcmp(a.pixels, b.pixels, (size_t)JK_W * JK_H * 4));
    }
    /* the progress form too, with a changing bar and status */
    jkpro_loader_draw_progress(&a, 100, "Getting PES 2013 ready", 10, 330, "Setting up Windows");
    jkpro_loader_draw_progress(&a, 700, "Getting PES 2013 ready", 200, 330, "Copying files");
    jkpro_loader_release();
    jkpro_loader_draw_progress(&b, 700, "Getting PES 2013 ready", 200, 330, "Copying files");
    jkpro_loader_release();
    jkpro_loader_draw_progress(&a, 0, "x", 0, 0, NULL);
    jkpro_loader_draw_progress(&a, 700, "Getting PES 2013 ready", 200, 330, "Copying files");
    CHECK(!memcmp(a.pixels, b.pixels, (size_t)JK_W * JK_H * 4));
    free(a.pixels);
    free(b.pixels);
    jkpro_loader_release();
}

static void foreign_content_gets_a_full_draw(void)
{
    JkFrame a = frame(), b = frame();
    jkpro_loader_draw(&a, 500, NULL);
    memset(a.pixels, 0x55, (size_t)JK_W * JK_H * 4);       /* someone else drew into the buffer */
    jkpro_loader_draw(&a, 600, NULL);
    jkpro_loader_draw(&b, 600, NULL);                      /* b is new to the loader: full draw */
    CHECK(!memcmp(a.pixels, b.pixels, (size_t)JK_W * JK_H * 4));
    free(a.pixels);
    free(b.pixels);
    jkpro_loader_release();
}

static void frames_are_cheap_after_the_first(void)
{
    JkFrame a = frame();
    jkpro_loader_draw(&a, 0, NULL);          /* builds the caches, the 16 spins included */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 1; i <= 60; i++) jkpro_loader_draw(&a, (uint64_t)i * 16, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = ((t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6) / 60;
    fprintf(stderr, "  loader frame: %.2f ms (host, ASan)\n", ms);
    CHECK_TIMING(ms < 25);
    free(a.pixels);
    jkpro_loader_release();
}

static void ball_is_round_and_logo_has_its_o(void)
{
    JkFrame a = frame();
    memset(a.pixels, 0, (size_t)JK_W * JK_H * 4);
    jkpro_ball(&a, 500, 500, 200, 0);
    uint32_t *p = (uint32_t *)a.pixels;
    CHECK(p[500 * JK_W + 500] != 0);                       /* centre drawn */
    CHECK(p[500 * JK_W + 395] == 0);                       /* outside the radius: untouched */
    CHECK(p[395 * JK_W + 500] == 0);
    int w = jkpro_logo_width(100);
    CHECK(w > 300 && w < 700);
    JkProLogoBox box = jkpro_logo(&a, 100, 800, 100, 1, 256);
    CHECK(box.ball_d >= 95 && box.ball_d <= 115);
    CHECK(box.ball_cx > 100 + w / 2 && box.ball_cx < 100 + w);
    free(a.pixels);
    jkpro_art_release();
}

int main(void)
{
    RUN(ball_is_round_and_logo_has_its_o);
    RUN(fast_path_matches_full_draw);
    RUN(foreign_content_gets_a_full_draw);
    RUN(frames_are_cheap_after_the_first);
    return jk_test_report("test_jkpro_loader");
}
