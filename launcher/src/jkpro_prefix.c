/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* The title's prefix edits before a start. See jkpro_prefix.h. */
#include "jkpro_prefix.h"
#include "jk_fs.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char REG_HEAD[] = "WINE REGISTRY Version 2\n";
#define EXPLORER "Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Explorer\\\\"
#define EXPLORER32 "Software\\\\Wow6432Node\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Explorer\\\\"
static const char *const USER_SECTIONS[] = {
    "[" EXPLORER "Shell Folders]",
    "[" EXPLORER "User Shell Folders]",
};
static const char *const SYSTEM_SECTIONS[] = {
    "[" EXPLORER "Shell Folders]",
    "[" EXPLORER "User Shell Folders]",
    "[" EXPLORER32 "Shell Folders]",
    "[" EXPLORER32 "User Shell Folders]",
};
#define MAX_SECTIONS 4

static int reg_valid(const char *path)
{
    char head[sizeof REG_HEAD];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(head, 1, sizeof REG_HEAD - 1, f);
    fclose(f);
    return n == sizeof REG_HEAD - 1 && !memcmp(head, REG_HEAD, n);
}

/* The section a "[...]" line opens: its index in sections, -1 another key. */
static int section_of(const char *line, size_t len, const char *const *sections, int nsec)
{
    for (int i = 0; i < nsec; i++) {
        size_t n = strlen(sections[i]);
        if (len >= n && !memcmp(line, sections[i], n) && (len == n || line[n] == ' ')) return i;
    }
    return -1;
}

/* Sets the values names[] ("\"Personal\"=" form) to win_dir in each of sections[]
 * of the registry file reg, keeping a str(2): type. names[0] must be there exactly
 * once in every section, or nothing is written; the others are set when present.
 * Written atomically, only on a change. 1 changed, 0 already so, -1 not done. */
static int redirect_values(const char *reg, const char *win_dir, const char *const *sections, int nsec,
                           const char *const *names, int nnames)
{
    if (!reg || !win_dir || !win_dir[0] || strchr(win_dir, '"') || nsec > MAX_SECTIONS) return -1;
    size_t len = 0;
    char *t = jk_fs_read(reg, 64u << 20, &len);
    if (!t) return -1;
    if (len < sizeof REG_HEAD - 1 || memcmp(t, REG_HEAD, sizeof REG_HEAD - 1)) { free(t); return -1; }
    /* the value as the registry file spells it: backslashes doubled */
    char value[1024];
    size_t vn = 0;
    for (const char *c = win_dir; *c && vn + 3 < sizeof value; c++) {
        if (*c == '\\') value[vn++] = '\\';
        value[vn++] = *c;
    }
    value[vn] = 0;
    size_t cap = len + 4 * (vn + 32) + 1;
    char *out = malloc(cap);
    if (!out) { free(t); return -1; }
    size_t on = 0;
    int section = -1, required[MAX_SECTIONS] = { 0 }, changed = 0;
    for (size_t pos = 0; pos < len;) {
        char *nl = memchr(t + pos, '\n', len - pos);
        size_t ll = nl ? (size_t)(nl - (t + pos)) : len - pos;
        const char *line = t + pos;
        const char *name = NULL;
        int which = -1;
        if (ll && line[0] == '[') section = section_of(line, ll, sections, nsec);
        else if (section >= 0) {
            for (int i = 0; i < nnames && !name; i++) {
                size_t nn = strlen(names[i]);
                if (ll > nn && !memcmp(line, names[i], nn)) { name = names[i]; which = i; }
            }
        }
        if (on + ll + vn + 32 > cap) {               /* room for this line, replaced or not */
            char *grown = realloc(out, cap * 2 + ll + vn + 32);
            if (!grown) { free(out); free(t); return -1; }
            out = grown;
            cap = cap * 2 + ll + vn + 32;
        }
        if (name) {
            size_t nn = strlen(name);
            int expand = ll > nn + 7 && !memcmp(line + nn, "str(2):", 7);
            on += (size_t)snprintf(out + on, cap - on, "%s%s\"%s\"", name, expand ? "str(2):" : "", value);
            size_t newl = strlen(name) + (expand ? 7 : 0) + vn + 2;
            if (newl != ll || memcmp(out + on - newl, line, ll)) changed = 1;
            if (which == 0) required[section]++;
        } else {
            memcpy(out + on, line, ll);
            on += ll;
        }
        if (nl) out[on++] = '\n';
        pos += ll + (nl ? 1 : 0);
    }
    free(t);
    int rc = changed ? 1 : 0;
    for (int i = 0; i < nsec; i++)
        if (required[i] != 1) rc = -1;
    if (rc == 1) rc = jk_fs_write_atomic(reg, out, on) == 0 ? 1 : -1;
    free(out);
    return rc;
}

int jkpro_redirect_documents(const char *user_reg, const char *win_dir)
{
    static const char *const names[] = { "\"Personal\"=", "\"My Documents\"=" };
    return redirect_values(user_reg, win_dir, USER_SECTIONS, 2, names, 2);
}

int jkpro_redirect_common_appdata(const char *system_reg, const char *win_dir)
{
    static const char *const names[] = { "\"Common AppData\"=" };
    return redirect_values(system_reg, win_dir, SYSTEM_SECTIONS, 4, names, 1);
}

int jkpro_reseed_arm(const char *prefix_dir, const char *good_dir)
{
    if (!prefix_dir || !good_dir) return -1;
    char p[1024];
    if (!jk_fs_is_dir(prefix_dir)) return 0;                    /* first start: the import builds it */
    snprintf(p, sizeof p, "%s/system.reg", prefix_dir);
    if (reg_valid(p)) return 0;
    snprintf(p, sizeof p, "%s/system.reg", good_dir);
    if (reg_valid(p)) return 0;                                 /* the launcher puts it back (E610) */
    snprintf(p, sizeof p, "%s/.n245-complete", prefix_dir);
    if (!jk_fs_exists(p)) return 1;                             /* already armed */
    return unlink(p) == 0 ? 1 : -1;
}

int jkpro_wall_stamp(long long t, char *out, size_t cap)
{
    if (t < 0 || !out || cap < 16) return -1;
    long long days = t / 86400, sec = t % 86400;
    /* civil date from days since 1970-01-01 (Howard Hinnant's algorithm) */
    long long z = days + 719468, era = z / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1;
    long long m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y++;
    int n = snprintf(out, cap, "%04lld-%02lld-%02lld_%02lld%02lld", y, m, d, sec / 3600, sec % 3600 / 60);
    return n == 15 ? 0 : -1;
}

static int open_perms_walk(const char *dir, int depth)
{
    int changed = 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[1024];
        if (snprintf(p, sizeof p, "%s/%s", dir, e->d_name) >= (int)sizeof p) continue;
        struct stat st;
        if (lstat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if ((st.st_mode & 07777) != 0777 && chmod(p, 0777) == 0) changed++;
            if (depth < 16) changed += open_perms_walk(p, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            if ((st.st_mode & 07777) != 0666 && chmod(p, 0666) == 0) changed++;
        }
    }
    closedir(d);
    return changed;
}

int jkpro_open_perms(const char *dir)
{
    struct stat st;
    if (!dir || stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    int changed = 0;
    if ((st.st_mode & 07777) != 0777 && chmod(dir, 0777) == 0) changed++;
    return changed + open_perms_walk(dir, 0);
}
