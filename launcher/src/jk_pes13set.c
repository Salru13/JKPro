/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* PES 2013 settings.dat writer. See jk_pes13set.h. */
#include "jk_pes13set.h"
#include "jk_fs.h"

#include <stdio.h>
#include <string.h>

#include "jk_pes13set_base.inc"

static void put32(unsigned char *d, size_t off, unsigned v)
{
    d[off] = (unsigned char)v;
    d[off + 1] = (unsigned char)(v >> 8);
    d[off + 2] = (unsigned char)(v >> 16);
    d[off + 3] = (unsigned char)(v >> 24);
}

unsigned jk_pes13set_crc(const unsigned char *d)
{
    unsigned c = 0;
    for (size_t i = 0; i < JK_PES13SET_SIZE; i++) {
        unsigned b = (i == 0x0C || i == 0x0D) ? 0 : d[i];
        c ^= b << 8;
        for (int k = 0; k < 8; k++) c = (c & 0x8000) ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
    }
    return (~c) & 0xFFFF;
}

int jk_pes13set_build(const JkPes13Settings *s, unsigned char out[JK_PES13SET_SIZE])
{
    if (!s || !out || s->width < 640 || s->width > 7680 || s->height < 480 || s->height > 4320 ||
        s->quality < 0 || s->quality > 2)
        return -1;
    memcpy(out, jk_pes13set_base, JK_PES13SET_SIZE);
    put32(out, 0x10, s->width);
    put32(out, 0x14, s->height);
    put32(out, 0x18, s->width * 9 == s->height * 16 ? 1u : 0u);
    put32(out, 0x1C, (unsigned)s->quality);
    put32(out, 0x20, s->fullscreen ? 1u : 0u);
    unsigned char f = out[0x0E] & (unsigned char)~0x09u;
    out[0x0E] = (unsigned char)(f | (s->vsync ? 0x01 : 0) | (s->xinput ? 0x08 : 0));
    unsigned crc = jk_pes13set_crc(out);
    out[0x0C] = (unsigned char)crc;
    out[0x0D] = (unsigned char)(crc >> 8);
    return 0;
}

int jk_pes13set_write(const char *path, const JkPes13Settings *s)
{
    unsigned char d[JK_PES13SET_SIZE];
    if (jk_pes13set_build(s, d) || jk_fs_mkdir_parent(path)) return -1;
    return jk_fs_write_atomic(path, d, sizeof d) ? -1 : 0;
}

int jk_pes13set_write_locked(const char *docs_dir)
{
    static const JkPes13Settings LOCK = { 1920, 1080, 1, 2, 1, 1 };
    char path[1024];
    if (!docs_dir || !docs_dir[0] ||
        snprintf(path, sizeof path, "%s/KONAMI/Pro Evolution Soccer 2013/settings.dat", docs_dir) >= (int)sizeof path)
        return -1;
    return jk_pes13set_write(path, &LOCK);
}
