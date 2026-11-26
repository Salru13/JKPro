/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * PES 2013 settings.dat writer (Documents\KONAMI\Pro Evolution Soccer 2013\settings.dat).
 * Same bytes as Konami's settings.exe for the same choices (research/pes13-settings-format.md):
 * "WECF" v2, 852 bytes, ~CRC16-CCITT(0x1021, init 0) at 0x0C over the file with 0x0C..0x0D zero.
 */
#ifndef JK_PES13SET_H
#define JK_PES13SET_H

#include <stddef.h>

#define JK_PES13SET_SIZE 852

typedef struct {
    unsigned width, height;
    int fullscreen;   /* 1 full screen, 0 window */
    int quality;      /* 0 low, 1 medium, 2 high */
    int xinput;       /* 1: controller 1 in XInput mode */
    int vsync;        /* 1: Wait for Vsync */
} JkPes13Settings;

/* The checksum of an 852-byte buffer (bytes 0x0C..0x0D taken as zero). */
unsigned jk_pes13set_crc(const unsigned char *d);

/* Builds the file into out (JK_PES13SET_SIZE bytes). 0 ok, -1 bad values. */
int jk_pes13set_build(const JkPes13Settings *s, unsigned char out[JK_PES13SET_SIZE]);

/* Builds and writes it atomically to path, making the parent folders. 0 ok. */
int jk_pes13set_write(const char *path, const JkPes13Settings *s);

/* JKPro's lock (owner 2026-11-25), written by the title before every start:
 * 1920x1080, full screen, High, XInput, vsync, into
 * <docs_dir>/KONAMI/Pro Evolution Soccer 2013/settings.dat (the base file's
 * other blocks; the old file is replaced). 0 ok, -1 not written. */
int jk_pes13set_write_locked(const char *docs_dir);

#endif
