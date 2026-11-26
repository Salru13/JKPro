/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * PES 2013 Data Packs (lead RE, 2026-11-25). A pack is one file, dt0f.img: an
 * AFS archive ("AFS\0", u32 entry count, then u32 offset/size pairs, every
 * entry inside the file). Its last entries are small records starting
 * 00 01 00 "WESYS" with an ASCII version at +0x30 (the base game's own file
 * reads "0.00"; a DLC e.g. "6.00"). The launcher decides by this header,
 * never by the file name.
 */
#ifndef JKPRO_DATAPACK_H
#define JKPRO_DATAPACK_H

#include <stddef.h>

typedef struct {
    char version[16];     /* "6.00", or "" when the version record cannot be read */
    long long size;       /* bytes */
    unsigned entries;
} JkProPackInfo;

/* 1: a PES 2013 data pack; 0: not one (or unreadable). out may be NULL. */
int jkpro_pack_check(const char *path, JkProPackInfo *out);

/* 1 when the file starts with "AFS\0" (cheap: 4 bytes). */
int jkpro_pack_magic(const char *path);

#endif
