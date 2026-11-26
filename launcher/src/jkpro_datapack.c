/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* PES 2013 Data Packs: recognise one by its AFS header. See jkpro_datapack.h. */
#include "jkpro_datapack.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MAX_ENTRIES 100000u

static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int jkpro_pack_magic(const char *path)
{
    unsigned char h[4];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(h, 1, 4, f);
    fclose(f);
    return n == 4 && !memcmp(h, "AFS\0", 4);
}

/* The version string of one AFS entry, when it is a 00 01 00 "WESYS" record. */
static int entry_version(FILE *f, uint32_t off, uint32_t size, char *out, size_t cap)
{
    unsigned char rec[0x50];
    if (size < 0x34) return 0;
    size_t want = size < sizeof rec ? size : sizeof rec;
    if (fseeko(f, (off_t)off, SEEK_SET) != 0 || fread(rec, 1, want, f) != want) return 0;
    if (!(rec[0] == 0 && rec[1] == 1 && rec[2] == 0 && !memcmp(rec + 3, "WESYS", 5))) return 0;
    size_t k = 0;
    for (size_t i = 0x30; i < want && k + 1 < cap; i++) {
        unsigned char c = rec[i];
        if (!c) break;
        if (c < 0x20 || c > 0x7e) return 0;
        out[k++] = (char)c;
    }
    out[k] = 0;
    return k > 0;
}

int jkpro_pack_check(const char *path, JkProPackInfo *out)
{
    JkProPackInfo info;
    memset(&info, 0, sizeof info);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int ok = 0;
    unsigned char h[8];
    if (fseeko(f, 0, SEEK_END) != 0) goto done;
    off_t total = ftello(f);
    if (total < 16 || fseeko(f, 0, SEEK_SET) != 0 || fread(h, 1, 8, f) != 8 || memcmp(h, "AFS\0", 4)) goto done;
    uint32_t count = le32(h + 4);
    if (count == 0 || count > MAX_ENTRIES || (off_t)(8 + (off_t)count * 8) > total) goto done;
    unsigned char *tab = malloc((size_t)count * 8);
    if (!tab) goto done;
    if (fread(tab, 8, count, f) != count) { free(tab); goto done; }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t o = le32(tab + i * 8), s = le32(tab + i * 8 + 4);
        if ((off_t)o + (off_t)s > total) { free(tab); goto done; }   /* every entry inside the file */
    }
    /* the version: the last entries first */
    for (uint32_t i = count; i-- > 0 && i + 4 > count && !info.version[0];)
        entry_version(f, le32(tab + i * 8), le32(tab + i * 8 + 4), info.version, sizeof info.version);
    free(tab);
    info.size = (long long)total;
    info.entries = count;
    ok = 1;
done:
    fclose(f);
    if (out) *out = info;
    return ok;
}
