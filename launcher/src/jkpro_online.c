/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
#include "jkpro_online.h"
#include "jk_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* thirteenserver.online/connect (read 2026-11-26): the PC setup.bat lines. */
static const char BUILTIN[] =
    "ntl.konamionline.com=95.214.55.111;"
    "info.service.konamionline.com=95.214.55.111;"
    "pes13-pc-gate.winning-eleven.net=95.214.55.111;"
    "pes13pcstun-e.winning-eleven.net=95.214.55.111;"
    "resic06.konami=95.214.55.111";

static int ipv4_ok(const char *s)
{
    int parts = 0;
    while (*s) {
        int v = 0, d = 0;
        while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; if (++d > 3 || v > 255) return 0; }
        if (!d) return 0;
        parts++;
        if (*s == '.') { s++; if (!*s) return 0; }
        else if (*s) return 0;
    }
    return parts == 4;
}

static int name_ok(const char *s)
{
    size_t n = strlen(s);
    if (!n || n > 127) return 0;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '-' || *s == '.'))
            return 0;
    return 1;
}

/* hosts-file text -> "name=ip;...". Entries found, or -1 when cap is too small. */
static int from_hosts(char *text, char *out, size_t cap)
{
    size_t used = 0;
    int n = 0;
    out[0] = 0;
    for (char *line = strtok(text, "\r\n"); line && n < JKPRO_ONLINE_MAX; line = strtok(NULL, "\r\n")) {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char ip[64], name[160];
        if (sscanf(line, " %63s %159s", ip, name) != 2 || !ipv4_ok(ip) || !name_ok(name)) continue;
        int w = snprintf(out + used, cap - used, "%s%s=%s", n ? ";" : "", name, ip);
        if (w < 0 || (size_t)w >= cap - used) { out[0] = 0; return -1; }
        used += (size_t)w;
        n++;
    }
    return n;
}

int jkpro_online_table(const char *override_path, char *out, size_t cap)
{
    if (!out || !cap) return -1;
    out[0] = 0;
    if (override_path && jk_fs_exists(override_path)) {
        size_t len = 0;
        char *text = jk_fs_read(override_path, 16384, &len);
        if (text) {
            int n = from_hosts(text, out, cap);
            free(text);
            if (n > 0) return 1;
            if (n < 0) return -1;
        }
    }
    if (sizeof BUILTIN > cap) return -1;
    memcpy(out, BUILTIN, sizeof BUILTIN);
    return 0;
}

int jkpro_online_set(const char *system_dir, int on)
{
    char flag[1024], over[1024];
    if (!system_dir || snprintf(flag, sizeof flag, "%s/%s", system_dir, JKPRO_ONLINE_FLAG) >= (int)sizeof flag ||
        snprintf(over, sizeof over, "%s/%s", system_dir, JKPRO_ONLINE_OVERRIDE) >= (int)sizeof over)
        return -1;
    if (!on) return jk_fs_exists(flag) ? (jk_fs_remove(flag) == 0 ? 0 : -1) : 0;
    char table[JKPRO_ONLINE_CAP];
    if (jkpro_online_table(over, table, sizeof table) < 0) return -1;
    if (jk_fs_mkdir_p(system_dir) != 0) return -1;
    return jk_fs_write_atomic(flag, table, strlen(table)) == 0 ? 0 : -1;
}

int jkpro_online_is_on(const char *system_dir)
{
    char flag[1024];
    if (!system_dir || snprintf(flag, sizeof flag, "%s/%s", system_dir, JKPRO_ONLINE_FLAG) >= (int)sizeof flag) return 0;
    return jk_fs_exists(flag);
}
