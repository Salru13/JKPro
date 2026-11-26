/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* Host test of pw_hosts.h: cc -Wall -Wextra -Werror test_pw_hosts.c && ./a.out */
#include "pw_hosts.h"

#include <stdio.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static int hit(const char *table, const char *name, unsigned a, unsigned b, unsigned c, unsigned d)
{
    unsigned char ip[4] = {0};
    return pw_hosts_lookup(table, name, ip) == 1 && ip[0] == a && ip[1] == b && ip[2] == c && ip[3] == d;
}

static int miss(const char *table, const char *name)
{
    unsigned char ip[4] = {9, 9, 9, 9};
    return pw_hosts_lookup(table, name, ip) == 0 && ip[0] == 9;
}

int main(void)
{
    const char *t = "ntl.konamionline.com=95.214.55.111;pes13-pc-gate.winning-eleven.net=95.214.55.111;"
                    "resic06.konami=1.2.3.4";
    CHECK(hit(t, "ntl.konamionline.com", 95, 214, 55, 111));
    CHECK(hit(t, "resic06.konami", 1, 2, 3, 4));
    CHECK(miss(t, "www.konami.com"));
    CHECK(miss(t, "konamionline.com"));          /* no suffix match */
    CHECK(miss(t, "xntl.konamionline.com"));
    CHECK(hit(t, "NTL.KonamiOnline.COM", 95, 214, 55, 111));
    CHECK(hit(t, "ntl.konamionline.com.", 95, 214, 55, 111));
    CHECK(hit("a.b.=5.6.7.8", "a.b", 5, 6, 7, 8));
    /* broken entries are skipped, the others still work */
    CHECK(hit("bad;x=1.2.3;y=300.1.1.1;=1.1.1.1;z=1.1.1.1x;ok.example=10.0.0.1", "ok.example", 10, 0, 0, 1));
    CHECK(miss("x=1.2.3", "x"));
    CHECK(miss("y=300.1.1.1", "y"));
    CHECK(miss("z=1.1.1.1x", "z"));
    CHECK(miss("z=1.1.1.1.1", "z"));
    CHECK(miss("z=01234.1.1.1", "z"));
    CHECK(hit(";;a=1.1.1.1;;", "a", 1, 1, 1, 1));
    /* empty, NULL */
    CHECK(miss("", "a"));
    CHECK(miss(NULL, "a"));
    CHECK(miss(t, ""));
    CHECK(miss(t, NULL));
    /* more than 16 entries: the 17th is not read */
    char big[1024] = "";
    for (int i = 0; i < 17; i++) {
        char e[32];
        snprintf(e, sizeof e, "h%d=10.0.0.%d;", i, i);
        strcat(big, e);
    }
    CHECK(hit(big, "h15", 10, 0, 0, 15));
    CHECK(miss(big, "h16"));
    printf("test_pw_hosts: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
