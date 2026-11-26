/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro: the player's .reg into the game's Wine prefix. See jk_regimport.h. */
#include "jk_regimport.h"
#include "jk_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define MAX_VALUES 32

typedef struct {
    char name[64];    /* with quotes: "code" */
    char data[512];   /* everything after '=' */
} RegValue;

/* The .reg file as ASCII lines: UTF-16LE (BOM) is narrowed (non-ASCII -> '?'). */
static char *read_reg_text(const char *path)
{
    size_t len = 0;
    char *raw = jk_fs_read(path, 1 << 20, &len);
    if (!raw) return NULL;
    if (len >= 2 && (unsigned char)raw[0] == 0xff && (unsigned char)raw[1] == 0xfe) {
        char *t = malloc(len / 2 + 1);
        if (!t) { free(raw); return NULL; }
        size_t k = 0;
        for (size_t i = 2; i + 1 < len; i += 2) {
            unsigned c = (unsigned char)raw[i] | ((unsigned char)raw[i + 1] << 8);
            t[k++] = c < 0x80 ? (char)c : '?';
        }
        t[k] = 0;
        free(raw);
        return t;
    }
    return raw;
}

/* "SOFTWARE\Wow6432Node\KONAMI\PES2013" and "SOFTWARE\KONAMI\PES2013" both match
 * key "Software\KONAMI\PES2013" (case ignored). */
static int key_matches(const char *k, const char *want)
{
    if (!strcasecmp(k, want)) return 1;
    const char *slash = strchr(want, '\\');
    if (!slash) return 0;
    char w[256];
    snprintf(w, sizeof w, "%.*s\\Wow6432Node%s", (int)(slash - want), want, slash);
    return !strcasecmp(k, w);
}

static int parse_values(char *text, const char *key, RegValue *v, int max)
{
    int n = 0, in = 0;
    for (char *line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '[') {
            in = 0;
            const char *pre = "[HKEY_LOCAL_MACHINE\\";
            char *end = strrchr(line, ']');
            if (!strncasecmp(line, pre, strlen(pre)) && end) {
                *end = 0;
                in = key_matches(line + strlen(pre), key);
            }
            continue;
        }
        if (!in || line[0] != '"' || n >= max) continue;
        char *q = strchr(line + 1, '"');
        if (!q || q[1] != '=') continue;
        size_t nl = (size_t)(q - line + 1);
        if (nl >= sizeof v[n].name || strlen(q + 2) >= sizeof v[n].data) continue;
        memcpy(v[n].name, line, nl);
        v[n].name[nl] = 0;
        snprintf(v[n].data, sizeof v[n].data, "%s", q + 2);
        n++;
    }
    return n;
}

/* key "Software\KONAMI\PES2013" -> "Software\\\\Wow6432Node\\\\KONAMI\\\\PES2013" (Wine store). */
static void store_key(const char *key, char *out, size_t cap)
{
    char tmp[256];
    const char *slash = strchr(key, '\\');
    if (slash && strncasecmp(slash + 1, "Wow6432Node\\", 12))
        snprintf(tmp, sizeof tmp, "%.*s\\Wow6432Node%s", (int)(slash - key), key, slash);
    else
        snprintf(tmp, sizeof tmp, "%s", key);
    size_t k = 0;
    for (const char *c = tmp; *c && k + 2 < cap; c++) {
        out[k++] = *c;
        if (*c == '\\') out[k++] = '\\';
    }
    out[k] = 0;
}

static void escape_value(const char *in, char *out, size_t cap)
{
    size_t k = 0;
    for (const char *c = in; *c && k + 2 < cap; c++) {
        if (*c == '\\' || *c == '"') out[k++] = '\\';
        out[k++] = *c;
    }
    out[k] = 0;
}

int jk_regimport(const char *reg_path, const char *system_reg, const char *key,
                 const char *installdir_win, const char *required, char *missing, size_t mcap)
{
    if (missing && mcap) missing[0] = 0;
    size_t slen = 0;
    char *store = jk_fs_read(system_reg, 64u << 20, &slen);
    if (!store) return -1;
    if (strncmp(store, "WINE REGISTRY Version 2", 23)) { free(store); return -1; }

    RegValue v[MAX_VALUES];
    int n = 0;
    char *text = reg_path ? read_reg_text(reg_path) : NULL;
    if (text) { n = parse_values(text, key, v, MAX_VALUES - 1); free(text); }

    /* installdir: replaced or added */
    char esc[480];
    escape_value(installdir_win, esc, sizeof esc);
    int k;
    for (k = 0; k < n && strcasecmp(v[k].name, "\"installdir\""); k++) {}
    if (k == n) n++;
    snprintf(v[k].name, sizeof v[k].name, "\"installdir\"");
    snprintf(v[k].data, sizeof v[k].data, "\"%s\"", esc);

    if (required && required[0] && missing) {
        char want[80];
        snprintf(want, sizeof want, "\"%s\"", required);
        int have = 0;
        for (int i = 0; i < n; i++) have |= !strcasecmp(v[i].name, want);
        if (!have) snprintf(missing, mcap, "%s", required);
    }

    char wkey[300], header[340];
    store_key(key, wkey, sizeof wkey);
    snprintf(header, sizeof header, "\n[%s]", wkey);

    /* drop an existing block for this key */
    char *at = NULL;
    for (char *p = strstr(store, header); p; p = strstr(p + 1, header)) {
        char c = p[strlen(header)];
        if (c == ' ' || c == '\n') { at = p; break; }
    }
    size_t cut_from = slen, cut_to = slen;
    if (at) {
        cut_from = (size_t)(at - store) + 1;
        char *end = strstr(at + 1, "\n\n");
        cut_to = end ? (size_t)(end - store) + 2 : slen;
    }

    size_t cap = slen + 64 + (size_t)n * 600 + strlen(header);
    char *out = malloc(cap);
    if (!out) { free(store); return -1; }
    size_t o = 0;
    memcpy(out, store, cut_from);
    o = cut_from;
    memmove(out + o, store + cut_to, slen - cut_to);
    o += slen - cut_to;
    while (o && out[o - 1] == '\n') o--;
    o += (size_t)snprintf(out + o, cap - o, "\n\n[%s] %lld\n", wkey, (long long)time(NULL));
    for (int i = 0; i < n; i++) o += (size_t)snprintf(out + o, cap - o, "%s=%s\n", v[i].name, v[i].data);
    int rc = jk_fs_write_atomic(system_reg, out, o) ? -1 : n;
    free(out);
    free(store);
    return rc;
}
