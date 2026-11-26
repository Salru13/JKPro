/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
#include "jkpro_fservfix.h"
#include "jk_fs.h"
#include "jk_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* make_fserv_e.py, file offsets (VA - 0x10001000 + 0x400). Our own bytes only. */
static const unsigned char e0_old[] = { 0x72, 0x22 };
static const unsigned char e0_new[] = { 0x72, 0x24 };                     /* jb 10003ED1 */
static const unsigned char e1_old[] = { 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc };
static const unsigned char e1_new[] = { 0x85, 0xc0,                       /* test eax,eax */
                                        0x74, 0x43,                       /* jz   10003F18 */
                                        0x3d, 0xd3, 0x07, 0x00, 0x00,     /* cmp  eax,7D3h */
                                        0x72, 0xf3,                       /* jb   10003ECF */
                                        0xeb, 0x33 };                     /* jmp  10003F11 */
static const unsigned char e2_old[] = { 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc };
static const unsigned char e2_new[] = { 0x3d, 0xd1, 0x08, 0x00, 0x00,     /* cmp  eax,8D1h */
                                        0x73, 0xb7,                       /* jae  10003ECF */
                                        0xb8, 0xb4, 0x03, 0x00, 0x00,     /* mov  eax,3B4h */
                                        0x5d, 0xc3 };                     /* pop ebp; ret */

const JkproFservEdit jkpro_fserv_edits[] = {
    { 0x32AB, 2, e0_old, e0_new },
    { 0x32D1, 13, e1_old, e1_new },
    { 0x3311, 14, e2_old, e2_new },
};
const int jkpro_fserv_nedits = 3;

#define MAXF (1 << 20)

static int paths(const char *dir, char *dll, char *bak, size_t cap)
{
    if (!dir || jk_fs_join(dll, cap, dir, "fserv.dll") || jk_fs_join(bak, cap, dir, JKPRO_FSERV_BACKUP)) return -1;
    return 0;
}

int jkpro_fservfix_apply_with(const char *dir, const char *orig_sha, const char *fixed_sha)
{
    char dll[1024], bak[1024], sha[65];
    if (paths(dir, dll, bak, sizeof dll)) return -1;
    if (!jk_fs_exists(dll)) return JKPRO_FSERVFIX_NONE;
    if (jk_fs_sha256(dll, sha, NULL)) return -2;
    if (!strcmp(sha, fixed_sha)) return JKPRO_FSERVFIX_ALREADY;
    if (strcmp(sha, orig_sha)) return JKPRO_FSERVFIX_OTHER;

    size_t len = 0;
    unsigned char *d = (unsigned char *)jk_fs_read(dll, MAXF, &len);
    if (!d) return -3;
    for (int e = 0; e < jkpro_fserv_nedits; e++) {
        const JkproFservEdit *x = &jkpro_fserv_edits[e];
        if (x->off + x->n > (long)len || memcmp(d + x->off, x->old_b, (size_t)x->n)) { free(d); return JKPRO_FSERVFIX_OTHER; }
        memcpy(d + x->off, x->new_b, (size_t)x->n);
    }
    jk_sha256_hex(d, len, sha);
    if (strcmp(sha, fixed_sha)) { free(d); return -4; }
    if (!jk_fs_exists(bak) && jk_fs_copy_checked(dll, bak, orig_sha)) { free(d); return -5; }
    int rc = jk_fs_write_atomic(dll, d, len);
    free(d);
    return rc ? -6 : JKPRO_FSERVFIX_PATCHED;
}

int jkpro_fservfix_revert_with(const char *dir, const char *orig_sha, const char *fixed_sha)
{
    char dll[1024], bak[1024], sha[65];
    if (paths(dir, dll, bak, sizeof dll)) return -1;
    if (!jk_fs_exists(dll)) return JKPRO_FSERVFIX_NONE;
    if (jk_fs_sha256(dll, sha, NULL)) return -2;
    if (!strcmp(sha, orig_sha)) return JKPRO_FSERVFIX_ALREADY;
    if (strcmp(sha, fixed_sha)) return JKPRO_FSERVFIX_OTHER;
    if (!jk_fs_exists(bak) || jk_fs_sha256(bak, sha, NULL) || strcmp(sha, orig_sha)) return -7;
    size_t len = 0;
    char *d = jk_fs_read(bak, MAXF, &len);
    if (!d) return -3;
    jk_sha256_hex(d, len, sha);
    int rc = strcmp(sha, orig_sha) ? -7 : jk_fs_write_atomic(dll, d, len) ? -6 : JKPRO_FSERVFIX_PATCHED;
    free(d);
    return rc;
}

int jkpro_fservfix_apply(const char *kitserver_dir)
{
    return jkpro_fservfix_apply_with(kitserver_dir, JKPRO_FSERV_ORIG_SHA, JKPRO_FSERV_FIXED_SHA);
}

int jkpro_fservfix_revert(const char *kitserver_dir)
{
    return jkpro_fservfix_revert_with(kitserver_dir, JKPRO_FSERV_ORIG_SHA, JKPRO_FSERV_FIXED_SHA);
}

int jkpro_fservfix_state(const char *kitserver_dir)
{
    char dll[1024], bak[1024], sha[65];
    if (paths(kitserver_dir, dll, bak, sizeof dll)) return -1;
    if (!jk_fs_exists(dll)) return JKPRO_FSERVFIX_NONE;
    if (jk_fs_sha256(dll, sha, NULL)) return -2;
    if (!strcmp(sha, JKPRO_FSERV_FIXED_SHA)) return JKPRO_FSERVFIX_PATCHED;
    if (!strcmp(sha, JKPRO_FSERV_ORIG_SHA)) return JKPRO_FSERVFIX_ALREADY;
    return JKPRO_FSERVFIX_OTHER;
}
