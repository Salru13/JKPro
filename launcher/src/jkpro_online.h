/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * Online play through ThirteenServer (thirteenserver.online, a community
 * server for PES 2013). On a PC the server is reached by hosts-file lines;
 * Wine on the console reads the same table from JK_HOSTS (engine/online).
 *
 * SETTINGS > Online server writes <system_dir>/online-hosts (on) or removes
 * it (off); the title sets JK_HOSTS to its text before the game starts.
 * The text is "name=a.b.c.d;..." from the player's own
 * <system_dir>/online-server.txt when it holds at least one valid line in
 * hosts-file form ("95.214.55.111 ntl.konamionline.com", # comments), else
 * the built-in ThirteenServer table. JKPro never stores a password: the
 * player logs in inside the game.
 */
#ifndef JKPRO_ONLINE_H
#define JKPRO_ONLINE_H

#include <stddef.h>

#define JKPRO_ONLINE_FLAG "online-hosts"
#define JKPRO_ONLINE_OVERRIDE "online-server.txt"
#define JKPRO_ONLINE_MAX 16      /* entries, as pw_hosts.h reads */
#define JKPRO_ONLINE_CAP 512     /* the title's env value buffer */

/* The table into out. 1: from override_path, 0: built-in, -1: out too small.
 * override_path may be NULL or a missing file. */
int jkpro_online_table(const char *override_path, char *out, size_t cap);

/* on: write <system_dir>/online-hosts (the table now in force); off: remove it. 0 or -1. */
int jkpro_online_set(const char *system_dir, int on);

/* 1 when <system_dir>/online-hosts exists. */
int jkpro_online_is_on(const char *system_dir);

#endif
