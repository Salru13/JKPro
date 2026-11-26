/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * Imports the player's own .reg file (as PC guides ship it, e.g. a PES 2013
 * "Registry Fix") into the game's offline Wine system.reg, before the first start.
 *
 * Only the one key the game definition names is taken (key = "Software\KONAMI\
 * PES2013", stored under Software\Wow6432Node\ for a 32-bit game, whichever view
 * the file used); every other key in the file is ignored. installdir is always
 * written as installdir_win (the PS4 folder), whatever the file says. The
 * key's block in system.reg is replaced, so importing twice changes nothing.
 * Values are never logged or returned.
 */
#ifndef JK_REGIMPORT_H
#define JK_REGIMPORT_H

#include <stddef.h>

/* reg_path NULL or unreadable: only installdir is written. required: a value
 * name the game needs ("code"); when it ends up absent, missing gets its name
 * (else ""). Returns the number of values written to the key, -1 when
 * system.reg is not a Wine registry or cannot be written. */
int jk_regimport(const char *reg_path, const char *system_reg, const char *key,
                 const char *installdir_win, const char *required, char *missing, size_t mcap);

#endif
