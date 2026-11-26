/* JKPro: the SETTINGS > Kitserver switch (owner, plan C, 2026-11-25).
 *
 * Kitserver 13 reads <game>/kitserver/config.txt; under [kload] each
 * "dll = <module>" line loads one module (kserv = kits). JKPro only changes the
 * comment state of those dll lines, nothing else in the file and no other
 * kitserver file:
 *   On            every dll line active/inactive as in the original file
 *   Without kits  as On, but "dll = kserv" commented out with a leading "; "
 *   Off           every dll line under [kload] commented out (kload.dll loads
 *                 no modules: the game runs without Kitserver modules)
 * Every other byte stays as it is (UTF-8 BOM, CRLF, unknown lines, comments).
 * The first time JKPro changes the file, the original is kept once, never
 * overwritten: <system>/backup/kitserver/config.txt.original. */
#ifndef JKPRO_KITCFG_H
#define JKPRO_KITCFG_H

#include <stddef.h>

typedef enum { JK_KIT_ON = 0, JK_KIT_NOKITS, JK_KIT_OFF, JK_KIT_COUNT } JkKitMode;

/* settings.ini words: "on", "nokits", "off"; the screen's words: "On", "Without kits", "Off". */
const char *jkpro_kit_word(JkKitMode m);
const char *jkpro_kit_text(JkKitMode m);
JkKitMode jkpro_kit_from_word(const char *w);   /* unknown or NULL: JK_KIT_ON */

/* Text level. cur/orig are the file bytes (orig NULL: cur is the original).
 * Returns a malloc'd new text (*outn its length), NULL on no memory. */
char *jkpro_kitcfg_apply(const char *cur, size_t n, const char *orig, size_t on, JkKitMode mode, size_t *outn);
/* The mode the text is in now (the first of On, Without kits, Off that would
 * change nothing); -1 if it matches none of them (the player's own mix). */
int jkpro_kitcfg_state(const char *cur, size_t n, const char *orig, size_t on);

/* File level. <game_dir>/kitserver/config.txt exists? */
int jkpro_kitcfg_present(const char *game_dir);
/* Applies mode to the game's config.txt (writes only when something changes,
 * backs up the original first). 0 ok or no config.txt, -1 failed (nothing changed). */
int jkpro_kitcfg_sync(const char *game_dir, const char *system_dir, JkKitMode mode);
/* The current state read from config.txt; -1 no file or the player's own mix. */
int jkpro_kitcfg_read_state(const char *game_dir, const char *system_dir);

#endif
