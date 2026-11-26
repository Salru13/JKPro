/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* W7 Kitserver face fix ("fserv-E"), applied IN PLACE to the player's own
 * <PES13>/kitserver/fserv.dll. Kitserver 13's licence allows redistribution of
 * unmodified binaries only, so JKPro carries just its own 28 edited bytes
 * (engine/kitserver/variants/make_fserv_e.py) and never a modified fserv.dll.
 *
 * Effect: fservGetFaceBin sends face id 0 and 0x7D3..0x8D0 (placeholder or empty
 * face BINs in data pack 6) to the game's default face 0x3B4 instead of crashing.
 * Off by default; the player turns it on in SETTINGS (owner, 2026-11-26). */
#ifndef JKPRO_FSERVFIX_H
#define JKPRO_FSERVFIX_H

#define JKPRO_FSERV_ORIG_SHA  "24915aa2c63d4ec075ea44db1d6ac64ff0f55504999631875633aa262df514dd"
#define JKPRO_FSERV_FIXED_SHA "b81aef015fd6a963e4f2ba796a289b365af6f514f9c59c9ea454ca2e2ee0d8e5"
#define JKPRO_FSERV_BACKUP    "fserv.dll.jkpro-orig"

enum {
    JKPRO_FSERVFIX_PATCHED = 0,  /* changed now (apply: fixed; revert: original restored) */
    JKPRO_FSERVFIX_ALREADY = 1,  /* already in the wanted state, nothing written */
    JKPRO_FSERVFIX_OTHER   = 2,  /* not the known fserv.dll: left alone */
    JKPRO_FSERVFIX_NONE    = 3   /* no fserv.dll (no Kitserver) */
    /* < 0: i/o or verify error; fserv.dll is untouched */
};

typedef struct JkproFservEdit { long off; int n; const unsigned char *old_b, *new_b; } JkproFservEdit;
extern const JkproFservEdit jkpro_fserv_edits[];
extern const int jkpro_fserv_nedits;

/* Turn the fix on. Patches only the known original; keeps one backup
 * (an existing backup is never overwritten); writes via temp + rename and only
 * after the result's sha256 equals the fixed sha. */
int jkpro_fservfix_apply(const char *kitserver_dir);
/* Turn the fix off: restores the backup, only when fserv.dll is our fixed file
 * and the backup is the known original. */
int jkpro_fservfix_revert(const char *kitserver_dir);
/* State without changing anything: PATCHED = fix is on, ALREADY = original (off),
 * OTHER, NONE. */
int jkpro_fservfix_state(const char *kitserver_dir);

/* Tests: same, with explicit shas. */
int jkpro_fservfix_apply_with(const char *dir, const char *orig_sha, const char *fixed_sha);
int jkpro_fservfix_revert_with(const char *dir, const char *orig_sha, const char *fixed_sha);

#endif
