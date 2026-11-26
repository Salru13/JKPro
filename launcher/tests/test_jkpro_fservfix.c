/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* W7: the Kitserver 13 face guard, applied in place to the player's own fserv.dll
 * (jkpro_fservfix). Kitserver's licence allows redistribution of unmodified binaries
 * only, so JKPro ships the 28 edited bytes and never a modified fserv.dll. */
#include "jkpro_fservfix.h"
#include "jk_test.h"
#include "jk_tmp.h"
#include "jk_fs.h"
#include "jk_sha256.h"

#define FSIZE 141312

/* A stand-in fserv.dll: deterministic filler with the real "old" bytes at the edit offsets. */
static unsigned char *fake_orig(void)
{
    unsigned char *b = malloc(FSIZE);
    for (int i = 0; i < FSIZE; i++) b[i] = (unsigned char)(i * 7 + 3);
    for (int e = 0; e < jkpro_fserv_nedits; e++)
        memcpy(b + jkpro_fserv_edits[e].off, jkpro_fserv_edits[e].old_b, (size_t)jkpro_fserv_edits[e].n);
    return b;
}

static void fixed_of(const unsigned char *orig, unsigned char *out)
{
    memcpy(out, orig, FSIZE);
    for (int e = 0; e < jkpro_fserv_nedits; e++)
        memcpy(out + jkpro_fserv_edits[e].off, jkpro_fserv_edits[e].new_b, (size_t)jkpro_fserv_edits[e].n);
}

static void put_bin(const char *dir, const char *name, const void *d, size_t n)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "wb"); fwrite(d, 1, n, f); fclose(f);
}

static int file_eq(const char *dir, const char *name, const void *d, size_t n)
{
    char p[512]; size_t len = 0;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    char *got = jk_fs_read(p, 1 << 20, &len);
    int ok = got && len == n && !memcmp(got, d, n);
    free(got);
    return ok;
}

static int exists_in(const char *dir, const char *name)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    return jk_fs_exists(p);
}

static int count_entries(const char *dir)
{
    int n = 0; DIR *d = opendir(dir); struct dirent *e;
    while (d && (e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++;
    if (d) closedir(d);
    return n;
}

static void the_edit_table_is_the_verified_variant_e(void)
{
    CHECK(!strcmp(JKPRO_FSERV_ORIG_SHA, "24915aa2c63d4ec075ea44db1d6ac64ff0f55504999631875633aa262df514dd"));
    CHECK(!strcmp(JKPRO_FSERV_FIXED_SHA, "b81aef015fd6a963e4f2ba796a289b365af6f514f9c59c9ea454ca2e2ee0d8e5"));
    CHECK_EQ(jkpro_fserv_nedits, 3);
    int changed = 0;
    for (int e = 0; e < jkpro_fserv_nedits; e++) {
        const JkproFservEdit *x = &jkpro_fserv_edits[e];
        CHECK(x->off > 0 && x->off + x->n <= FSIZE);
        for (int i = 0; i < x->n; i++) changed += x->old_b[i] != x->new_b[i];
    }
    CHECK_EQ(changed, 28);                         /* make_fserv_e.py: 28 bytes */
    CHECK_EQ(jkpro_fserv_edits[0].off, 0x32AB);    /* jb 10003EAB: 72 22 -> 72 24 */
    CHECK_EQ(jkpro_fserv_edits[0].new_b[1], 0x24);
    CHECK_EQ(jkpro_fserv_edits[2].new_b[5], 0x73); /* stub 2: cmp eax,8D1h; jae */
}

static void patches_the_known_original_and_keeps_one_backup(void)
{
    char dir[256], osha[65], fsha[65];
    unsigned char *orig = fake_orig(), *want = malloc(FSIZE);
    fixed_of(orig, want);
    jk_sha256_hex(orig, FSIZE, osha); jk_sha256_hex(want, FSIZE, fsha);
    tmp_dir(dir, sizeof dir);
    put_bin(dir, "fserv.dll", orig, FSIZE);
    CHECK_EQ(jkpro_fservfix_apply_with(dir, osha, fsha), JKPRO_FSERVFIX_PATCHED);
    CHECK(file_eq(dir, "fserv.dll", want, FSIZE));
    CHECK(file_eq(dir, "fserv.dll.jkpro-orig", orig, FSIZE));
    CHECK_EQ(count_entries(dir), 2);               /* no temp file left */
    /* second run: already fixed, nothing written, backup kept */
    CHECK_EQ(jkpro_fservfix_apply_with(dir, osha, fsha), JKPRO_FSERVFIX_ALREADY);
    CHECK(file_eq(dir, "fserv.dll", want, FSIZE));
    CHECK(file_eq(dir, "fserv.dll.jkpro-orig", orig, FSIZE));
    tmp_rm_rf(dir); free(orig); free(want);
}

static void an_existing_backup_is_never_overwritten(void)
{
    char dir[256], osha[65], fsha[65];
    unsigned char *orig = fake_orig(), *want = malloc(FSIZE);
    fixed_of(orig, want);
    jk_sha256_hex(orig, FSIZE, osha); jk_sha256_hex(want, FSIZE, fsha);
    tmp_dir(dir, sizeof dir);
    put_bin(dir, "fserv.dll", orig, FSIZE);
    put_bin(dir, "fserv.dll.jkpro-orig", "OLDER BACKUP", 12);
    CHECK_EQ(jkpro_fservfix_apply_with(dir, osha, fsha), JKPRO_FSERVFIX_PATCHED);
    CHECK(file_eq(dir, "fserv.dll", want, FSIZE));
    CHECK(file_eq(dir, "fserv.dll.jkpro-orig", "OLDER BACKUP", 12));
    tmp_rm_rf(dir); free(orig); free(want);
}

static void another_fserv_version_is_left_alone(void)
{
    char dir[256], osha[65], fsha[65];
    unsigned char *orig = fake_orig(), *want = malloc(FSIZE), *other = malloc(FSIZE);
    fixed_of(orig, want);
    jk_sha256_hex(orig, FSIZE, osha); jk_sha256_hex(want, FSIZE, fsha);
    memcpy(other, orig, FSIZE); other[100] ^= 0x5A;
    tmp_dir(dir, sizeof dir);
    put_bin(dir, "fserv.dll", other, FSIZE);
    CHECK_EQ(jkpro_fservfix_apply_with(dir, osha, fsha), JKPRO_FSERVFIX_OTHER);
    CHECK(file_eq(dir, "fserv.dll", other, FSIZE));
    CHECK(!exists_in(dir, "fserv.dll.jkpro-orig"));
    CHECK_EQ(count_entries(dir), 1);
    tmp_rm_rf(dir); free(orig); free(want); free(other);
}

static void no_fserv_means_no_kitserver(void)
{
    char dir[256], nodir[300];
    tmp_dir(dir, sizeof dir);
    CHECK_EQ(jkpro_fservfix_apply(dir), JKPRO_FSERVFIX_NONE);
    snprintf(nodir, sizeof nodir, "%s/no-such-kitserver", dir);
    CHECK_EQ(jkpro_fservfix_apply(nodir), JKPRO_FSERVFIX_NONE);
    CHECK_EQ(count_entries(dir), 0);
    tmp_rm_rf(dir);
}

static void a_result_that_does_not_verify_is_never_written(void)
{
    char dir[256], osha[65], bad[65];
    unsigned char *orig = fake_orig();
    jk_sha256_hex(orig, FSIZE, osha);
    memset(bad, '0', 64); bad[64] = 0;
    tmp_dir(dir, sizeof dir);
    put_bin(dir, "fserv.dll", orig, FSIZE);
    CHECK(jkpro_fservfix_apply_with(dir, osha, bad) < 0);
    CHECK(file_eq(dir, "fserv.dll", orig, FSIZE));
    CHECK_EQ(count_entries(dir), 1);               /* no backup, no temp */
    tmp_rm_rf(dir); free(orig);
}

/* Settings "off": the player's original comes back from the backup, and only when
 * fserv.dll is our fixed file and the backup is the known original. */
static void revert_restores_the_original(void)
{
    char dir[256], osha[65], fsha[65];
    unsigned char *orig = fake_orig(), *want = malloc(FSIZE);
    fixed_of(orig, want);
    jk_sha256_hex(orig, FSIZE, osha); jk_sha256_hex(want, FSIZE, fsha);
    tmp_dir(dir, sizeof dir);
    put_bin(dir, "fserv.dll", orig, FSIZE);
    CHECK_EQ(jkpro_fservfix_revert_with(dir, osha, fsha), JKPRO_FSERVFIX_ALREADY); /* already original */
    CHECK_EQ(jkpro_fservfix_apply_with(dir, osha, fsha), JKPRO_FSERVFIX_PATCHED);
    CHECK_EQ(jkpro_fservfix_revert_with(dir, osha, fsha), JKPRO_FSERVFIX_PATCHED);
    CHECK(file_eq(dir, "fserv.dll", orig, FSIZE));
    CHECK(file_eq(dir, "fserv.dll.jkpro-orig", orig, FSIZE)); /* backup kept */
    CHECK_EQ(count_entries(dir), 2);
    /* on/off/on works */
    CHECK_EQ(jkpro_fservfix_apply_with(dir, osha, fsha), JKPRO_FSERVFIX_PATCHED);
    CHECK(file_eq(dir, "fserv.dll", want, FSIZE));
    tmp_rm_rf(dir); free(orig); free(want);
}

static void revert_never_touches_an_unknown_file(void)
{
    char dir[256], osha[65], fsha[65];
    unsigned char *orig = fake_orig(), *want = malloc(FSIZE), *other = malloc(FSIZE);
    fixed_of(orig, want);
    jk_sha256_hex(orig, FSIZE, osha); jk_sha256_hex(want, FSIZE, fsha);
    memcpy(other, orig, FSIZE); other[7] ^= 1;
    tmp_dir(dir, sizeof dir);
    /* someone else's fserv: left alone */
    put_bin(dir, "fserv.dll", other, FSIZE);
    put_bin(dir, "fserv.dll.jkpro-orig", orig, FSIZE);
    CHECK_EQ(jkpro_fservfix_revert_with(dir, osha, fsha), JKPRO_FSERVFIX_OTHER);
    CHECK(file_eq(dir, "fserv.dll", other, FSIZE));
    /* our fixed file but a damaged backup: left alone */
    put_bin(dir, "fserv.dll", want, FSIZE);
    put_bin(dir, "fserv.dll.jkpro-orig", other, FSIZE);
    CHECK(jkpro_fservfix_revert_with(dir, osha, fsha) < 0);
    CHECK(file_eq(dir, "fserv.dll", want, FSIZE));
    /* no fserv */
    tmp_rm_rf(dir); tmp_dir(dir, sizeof dir);
    CHECK_EQ(jkpro_fservfix_revert(dir), JKPRO_FSERVFIX_NONE);
    tmp_rm_rf(dir); free(orig); free(want); free(other);
}

static void the_real_file_gives_the_verified_sha(void)
{
    /* Optional: JKPRO_FSERV_REAL=<player's fserv.dll 24915aa2> (never in the repo). */
    const char *real = getenv("JKPRO_FSERV_REAL");
    if (!real || !*real) { fprintf(stderr, "  (skip: JKPRO_FSERV_REAL not set)\n"); return; }
    char dir[256], p[512], sha[65]; size_t len = 0;
    char *d = jk_fs_read(real, 1 << 20, &len);
    CHECK(d != NULL);
    if (!d) return;
    tmp_dir(dir, sizeof dir);
    put_bin(dir, "fserv.dll", d, len);
    CHECK_EQ(jkpro_fservfix_apply(dir), JKPRO_FSERVFIX_PATCHED);
    snprintf(p, sizeof p, "%s/fserv.dll", dir);
    CHECK_EQ(jk_fs_sha256(p, sha, NULL), 0);
    CHECK(!strcmp(sha, JKPRO_FSERV_FIXED_SHA));
    CHECK_EQ(jkpro_fservfix_apply(dir), JKPRO_FSERVFIX_ALREADY);
    snprintf(p, sizeof p, "%s/fserv.dll.jkpro-orig", dir);
    CHECK_EQ(jk_fs_sha256(p, sha, NULL), 0);
    CHECK(!strcmp(sha, JKPRO_FSERV_ORIG_SHA));
    tmp_rm_rf(dir); free(d);
}

int main(void)
{
    RUN(the_edit_table_is_the_verified_variant_e);
    RUN(patches_the_known_original_and_keeps_one_backup);
    RUN(an_existing_backup_is_never_overwritten);
    RUN(another_fserv_version_is_left_alone);
    RUN(no_fserv_means_no_kitserver);
    RUN(a_result_that_does_not_verify_is_never_written);
    RUN(revert_restores_the_original);
    RUN(revert_never_touches_an_unknown_file);
    RUN(the_real_file_gives_the_verified_sha);
    return jk_test_report("test_jkpro_fservfix");
}
