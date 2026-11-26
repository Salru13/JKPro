/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* SETTINGS > Online server (ThirteenServer): the table written for JK_HOSTS. */
#include "jkpro_online.h"
#include "jk_test.h"
#include "jk_tmp.h"
#include "jk_fs.h"

static const char *NAMES[] = { "ntl.konamionline.com=95.214.55.111", "info.service.konamionline.com=95.214.55.111",
                               "pes13-pc-gate.winning-eleven.net=95.214.55.111",
                               "pes13pcstun-e.winning-eleven.net=95.214.55.111", "resic06.konami=95.214.55.111" };

static void builtin(void)
{
    char t[JKPRO_ONLINE_CAP];
    CHECK_EQ(jkpro_online_table(NULL, t, sizeof t), 0);
    for (int i = 0; i < 5; i++) CHECK(strstr(t, NAMES[i]) != NULL);
    CHECK(strlen(t) < 300);
    CHECK_EQ(jkpro_online_table("/nonexistent/online-server.txt", t, sizeof t), 0);
    CHECK(strstr(t, NAMES[0]) != NULL);
    CHECK_EQ(jkpro_online_table(NULL, t, 20), -1);
    CHECK_EQ(jkpro_online_table(NULL, NULL, 0), -1);
}

static void override_file(void)
{
    char d[512], p[600], t[JKPRO_ONLINE_CAP];
    tmp_dir(d, sizeof d);
    snprintf(p, sizeof p, "%s/%s", d, JKPRO_ONLINE_OVERRIDE);
    tmp_put(d, JKPRO_ONLINE_OVERRIDE, "# ThirteenServer\r\n1.2.3.4  ntl.konamionline.com\n"
                                       "\t5.6.7.8 resic06.konami # new\nbad line\n300.1.1.1 x.y\n");
    CHECK_EQ(jkpro_online_table(p, t, sizeof t), 1);
    CHECK(!strcmp(t, "ntl.konamionline.com=1.2.3.4;resic06.konami=5.6.7.8"));
    /* nothing valid: the built-in table */
    tmp_put(d, JKPRO_ONLINE_OVERRIDE, "# empty\nnot an entry\n");
    CHECK_EQ(jkpro_online_table(p, t, sizeof t), 0);
    CHECK(strstr(t, NAMES[4]) != NULL);
    /* more than 16 lines: 16 are used */
    char big[2048] = "";
    for (int i = 0; i < 20; i++) {
        char l[64];
        snprintf(l, sizeof l, "10.0.0.%d h%d.example\n", i, i);
        strcat(big, l);
    }
    tmp_put(d, JKPRO_ONLINE_OVERRIDE, big);
    CHECK_EQ(jkpro_online_table(p, t, sizeof t), 1);
    CHECK(strstr(t, "h15.example=10.0.0.15") != NULL);
    CHECK(strstr(t, "h16.example") == NULL);
    tmp_rm_rf(d);
}

static void set_and_state(void)
{
    char d[512], sys[600], buf[JKPRO_ONLINE_CAP];
    tmp_dir(d, sizeof d);
    snprintf(sys, sizeof sys, "%s/System", d);
    CHECK_EQ(jkpro_online_is_on(sys), 0);
    CHECK_EQ(jkpro_online_set(sys, 0), 0);               /* off when already off */
    CHECK_EQ(jkpro_online_set(sys, 1), 0);
    CHECK_EQ(jkpro_online_is_on(sys), 1);
    CHECK(tmp_get(sys, JKPRO_ONLINE_FLAG, buf, sizeof buf) > 0);
    CHECK(strstr(buf, NAMES[2]) != NULL);
    tmp_put(sys, JKPRO_ONLINE_OVERRIDE, "9.9.9.9 ntl.konamionline.com\n");
    CHECK_EQ(jkpro_online_set(sys, 1), 0);               /* re-written with the player's file */
    CHECK(tmp_get(sys, JKPRO_ONLINE_FLAG, buf, sizeof buf) > 0);
    CHECK(!strcmp(buf, "ntl.konamionline.com=9.9.9.9"));
    CHECK_EQ(jkpro_online_set(sys, 0), 0);
    CHECK_EQ(jkpro_online_is_on(sys), 0);
    CHECK_EQ(jkpro_online_set(NULL, 1), -1);
    tmp_rm_rf(d);
}

int main(void)
{
    RUN(builtin);
    RUN(override_file);
    RUN(set_and_state);
    return jk_test_report("test_jkpro_online");
}
