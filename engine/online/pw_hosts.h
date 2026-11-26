/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * JKPro online: a small hosts table for the Wine ws2_32 resolver on the PS4.
 *
 * On a PC, a community server is reached by lines in the Windows hosts file.
 * Wine on the console has no hosts file: every name goes to the console's
 * resolver. The launcher therefore passes the table in the environment
 * variable JK_HOSTS, "name=a.b.c.d;name=a.b.c.d" (at most PW_HOSTS_MAX
 * entries, IPv4 only), and pw_console_resolve asks it first. Names match
 * without case and without a trailing dot; a broken entry is skipped; a name
 * that is not in the table goes to the console's resolver as before.
 *
 * Header-only, so the ws2_32 build and the host test compile the same code.
 */
#ifndef PW_HOSTS_H
#define PW_HOSTS_H

#include <stddef.h>
#include <string.h>

#define PW_HOSTS_MAX 16
#define PW_HOSTS_NAME_MAX 127

static int pw_hosts_lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* "a.b.c.d" of exactly len bytes -> four bytes in network order. */
static int pw_hosts_ipv4(const char *s, size_t len, unsigned char out[4])
{
    size_t i = 0;
    for (int part = 0; part < 4; part++) {
        unsigned v = 0, digits = 0;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + (unsigned)(s[i] - '0');
            if (++digits > 3 || v > 255)
                return 0;
            i++;
        }
        if (!digits)
            return 0;
        out[part] = (unsigned char)v;
        if (part < 3) {
            if (i >= len || s[i] != '.')
                return 0;
            i++;
        }
    }
    return i == len;
}

/* Same name: case-insensitive, a trailing dot on either side ignored. */
static int pw_hosts_same(const char *a, size_t alen, const char *b, size_t blen)
{
    if (alen && a[alen - 1] == '.')
        alen--;
    if (blen && b[blen - 1] == '.')
        blen--;
    if (!alen || alen != blen)
        return 0;
    for (size_t i = 0; i < alen; i++)
        if (pw_hosts_lower((unsigned char)a[i]) != pw_hosts_lower((unsigned char)b[i]))
            return 0;
    return 1;
}

/* 1 and the address in out when name is in table, else 0. */
static int pw_hosts_lookup(const char *table, const char *name, unsigned char out[4])
{
    if (!table || !name || !*name)
        return 0;
    size_t nlen = strlen(name);
    if (nlen > PW_HOSTS_NAME_MAX + 1)
        return 0;
    int entries = 0;
    const char *p = table;
    while (*p && entries < PW_HOSTS_MAX) {
        const char *end = strchr(p, ';');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        const char *eq = memchr(p, '=', len);
        if (len) {
            entries++;
            unsigned char ip[4];
            if (eq && eq > p && pw_hosts_ipv4(eq + 1, len - (size_t)(eq + 1 - p), ip) &&
                pw_hosts_same(p, (size_t)(eq - p), name, nlen)) {
                memcpy(out, ip, 4);
                return 1;
            }
        }
        if (!end)
            break;
        p = end + 1;
    }
    return 0;
}

#endif
