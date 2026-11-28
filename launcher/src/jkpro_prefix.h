/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * The title's edits to the Wine prefix before PES 2013 starts (jkpro_prepare.inc).
 * Run while Wine is not running (the registry files are only read at its start).
 */
#ifndef JKPRO_PREFIX_H
#define JKPRO_PREFIX_H

/* My Documents to win_dir (a Windows path, "Z:\data\JKPro\Saves\PES13"): in
 * user_reg, the value "Personal" (and "My Documents" when present) of
 * HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders and of
 * ...\User Shell Folders (keeping its str(2) type). Both "Personal" values must
 * be there; otherwise nothing is written. Written atomically, only on a change.
 * 1 changed, 0 already so, -1 not done (file missing or not a Wine registry,
 * a "Personal" missing, I/O). */
int jkpro_redirect_documents(const char *user_reg, const char *win_dir);

/* Common AppData to win_dir (PES 2013 reads its data pack from
 * <Common AppData>\KONAMI\Pro Evolution Soccer 2013\download): in system_reg, the
 * value "Common AppData" of HKLM\Software\Microsoft\Windows\CurrentVersion\Explorer\
 * Shell Folders and ...\User Shell Folders (keeping its str(2) type), and of the
 * same two keys under Software\Wow6432Node (pes2013.exe is 32-bit). All four
 * must be there; otherwise nothing is written. Written atomically, only on a
 * change. 1 changed, 0 already so, -1 not done. */
int jkpro_redirect_common_appdata(const char *system_reg, const char *win_dir);

/* PES 2013's start-up check (its "minimum system requirements" warning, which Kitserver's lodmixer
 * otherwise skips): sets, in system_reg, HKLM\Hardware\Description\System\CentralProcessor\0
 * "~MHz"=3500 plus the processor's identity (wineboot, which normally makes that key, never runs
 * on the console), and in user_reg, HKCU\Software\Wine\Direct3D "VideoMemorySize"="2048" and
 * "renderer"="no3d" (the video memory DirectDraw reports through wined3d; the console has no GL).
 * A missing key is added at the end of the file; in a present key only these values change.
 * Both files must be Wine registries, else nothing is written. Written atomically, only on a
 * change. 1 changed, 0 already so, -1 not done. */
int jkpro_pes_check_values(const char *system_reg, const char *user_reg);

/* <prefix_dir>/system.reg missing or not a Wine registry, and good_dir (the
 * launcher's backup/registry) has no good system.reg: removes the seed's
 * completion marker (<prefix_dir>/.n245-complete), so JkFlow's seed import
 * (update mode) puts the seed's registry files back. 1 armed, 0 nothing to do
 * (no prefix yet, registry fine, or a good copy the launcher restores), -1 the
 * marker could not be removed. */
int jkpro_reseed_arm(const char *prefix_dir, const char *good_dir);

/* "YYYY-MM-DD_HHMM" (UTC) for unix time t, by plain arithmetic: no strftime /
 * gmtime (libc's strftime faulted in its locale TLS in the title, rc3). 0, or
 * -1 for t < 0 or cap < 16. */
#include <stddef.h>
int jkpro_wall_stamp(long long t, char *out, size_t cap);

/* dir and everything under it: directories 0777, regular files 0666 (the console's
 * FTP server is another user; the player adds option files and copies saves
 * there). Symlinks are not followed. Number of entries changed, or -1 when dir
 * is not a directory. */
int jkpro_open_perms(const char *dir);

#endif
