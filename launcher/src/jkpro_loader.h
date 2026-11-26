/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * The JKPro start screen ("kick-off"), shown by the title from PLAY until the
 * game takes the screen, and during the first-start Windows setup.
 *
 * The night stadium with the JKPRO logo; the football that is the logo's O
 * juggles in its place (hop, spin, a shadow and a light ring on the grass at
 * each touch), and every fourth touch is a kick-off: a higher kick. Under it
 * the big line ("Starting PES 2013…") and, in the progress form, a bar and a
 * status line.
 *
 * Cheap at 60 Hz: the still parts are built once into a cache; per frame only
 * the ball's area and the text band are copied back and redrawn. A buffer the
 * loader has not drawn into before (or that someone else drew into) gets one
 * full copy first. Pure drawing on JkFrame: no I/O.
 */
#ifndef JKPRO_LOADER_H
#define JKPRO_LOADER_H

#include <stdint.h>

#include "jk_ui.h"

/* One frame, t_ms since the loader started. text NULL: "Starting PES 2013…". */
void jkpro_loader_draw(JkFrame *f, uint64_t t_ms, const char *text);

/* The same with a progress bar (total 0: none) and a status line under the
 * text (NULL: none): the title's first-start setup. */
void jkpro_loader_draw_progress(JkFrame *f, uint64_t t_ms, const char *text, unsigned done, unsigned total,
                                const char *status);

/* Frees the caches (call at the game's first frame). The next draw rebuilds. */
void jkpro_loader_release(void);

#endif
