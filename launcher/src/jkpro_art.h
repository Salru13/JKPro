/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * JKPro's own art, drawn in code (owner, 2026-11-25: "the same feel as JkFlow
 * and Jk4Speed; the O in PRO a real football"): the JKPRO logo (white
 * ExtraBold Italic letters with a pitch-green to floodlight-gold glow, the O a
 * shaded football), the football itself, and a night-stadium background with
 * floodlights and light streaks. No image files, no game or publisher art.
 * Everything is built once into caches and then copied or blended.
 */
#ifndef JKPRO_ART_H
#define JKPRO_ART_H

#include "jk_ui.h"

/* Where the logo landed: the letters' box (cap height cap, top-left x, y)
 * and the football standing in for the O. */
typedef struct {
    int x, y, w, h;            /* the letters + ball, without the glow */
    int ball_cx, ball_cy, ball_d;
} JkProLogoBox;

/* The width of the logo (letters + ball, no glow) at cap height cap. */
int jkpro_logo_width(int cap);
/* Draws the logo with its letters' top-left at (x, y), cap height cap (16..
 * 240 px). with_ball 0 leaves the O's place empty (the loader bounces its
 * own ball there). glow 0..256 scales the glow. */
JkProLogoBox jkpro_logo(JkFrame *f, int x, int y, int cap, int with_ball, int glow);

/* A football of diameter d (8..400) centred on (cx, cy), anti-aliased, lit
 * from the top left; spin 0..15 turns it about a tilted axis. */
void jkpro_ball(JkFrame *f, int cx, int cy, int d, int spin);

/* The ball's floodlight-gold halo (draw it before the ball); glow 0..256. */
void jkpro_ball_halo(JkFrame *f, int cx, int cy, int d, int glow);

/* The night stadium: floodlights, haze, light streaks and the pitch with its
 * centre circle (whole frame, cached). */
void jkpro_stadium(JkFrame *f);

/* Frees every cache. */
void jkpro_art_release(void);

#endif
