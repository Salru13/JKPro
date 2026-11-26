/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * JKPro 1.0 on the JkFlow 1.00 title (blur-launcher-r4 wine64_main.c): replaces
 * blur_launcher_screen. Seeds the Wine prefix, runs the JKPro launcher UI
 * (launcher/src/jkpro_ui.c: PLAY · PATCH · SETTINGS · SAVE GAMES; it writes
 * d3d9.dll, the player's .reg and settings.dat), then composes the PES 2013
 * profile/env/dxvk.conf into JkFlow's BlDecision and starts through JkFlow's own
 * start path.
 *
 *   /data/JKPro (any letter case)/PES13 (any letter case)   the player's game
 *   <JKPro>/System/prefix, /cache/dxvk.conf, /logs-on, /renderer-pe, /start.txt
 *
 * After the game ends the title restarts itself ("cycle=N" argument): the UI
 * opens again. An exit within 45 s of the start is a start-up crash (kitserver 13's
 * kload.dll sometimes faults while it hooks D3D9): started again directly, at most
 * 3 times, then the UI.
 */
#include "jk_fs.h"
#include <sys/stat.h>
#include "jk_pes13set.h"
#include "jk_release.h"
#include "jk_release_key.h"
#include "jkpro_loader.h"
#include "jkpro_prefix.h"
#include "jkpro_ui.h"

/* The restart counter from the title's own arguments ("cycle=N", restart_title):
 * set by main before this runs (launch.cycle is parsed only later, by direct_init). */
static unsigned jkpro_cycle;
/* Set when the game stopped while starting 3 times in a row: the UI shows E101. */
static const char *jkpro_failure;

static int jkpro_ui_present(void *ctx, const JkFrame *f)
{
    BlFrame b = { f->pixels, f->width, f->height, f->stride };
    return blur_host_present(ctx, &b);
}
static int jkpro_ui_pad(void *ctx, uint32_t *held, uint32_t *pressed) { return blur_host_pad(ctx, held, pressed); }
static uint64_t jkpro_ui_now(void *ctx) { return blur_host_now(ctx); }
static void jkpro_ui_sleep(void *ctx, uint32_t us) { blur_host_sleep(ctx, us); }
static void jkpro_ui_log(void *ctx, const char *line) { (void)ctx; PS5LOG_LOG("JKPRO ui %s", line); }
static int jkpro_ui_wall(void *ctx, char *out, size_t cap)
{
    (void)ctx;
    /* no strftime here: it faulted in libc's locale TLS (SIGBUS, rc3 backup/support) */
    return jkpro_wall_stamp((long long)time(NULL), out, cap);
}

/* name (letter case ignored) inside dir: real name into out. 1 found. */
static int jkpro_find_ci(const char *dir, const char *name, char *out, size_t cap)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int hit = 0;
    while (!hit && (e = readdir(d)))
        if (!strcasecmp(e->d_name, name) && strlen(e->d_name) < cap) { snprintf(out, cap, "%s", e->d_name); hit = 1; }
    closedir(d);
    return hit;
}

/* Start record: "<unix time> <tries>". Returns 1 when this open is a start-up crash
 * retry (fills *tries), 0 for the UI. */
static int jkpro_retry_check(const char *st_path, int *tries_out)
{
    long long started = 0;
    int tries = 0;
    size_t len = 0;
    char *t = jk_fs_read(st_path, 256, &len);
    if (t) { (void)sscanf(t, "%lld %d", &started, &tries); free(t); }
    long long now = (long long)time(NULL);
    int early = started > 0 && now - started >= 0 && now - started < 45;
    int restart = jkpro_cycle > 0 || early;
    *tries_out = 0;
    if (!restart) return 0;
    if (early && tries < 3) {
        *tries_out = tries + 1;
        PS5LOG_LOG("JKPRO start-up crash after %llds: try %d of 3", now - started, tries + 1);
        return 1;
    }
    if (early) jkpro_failure = "The game stopped while starting, 3 times in a row.";
    PS5LOG_LOG("JKPRO game ended (cycle=%u after %llds, tries=%d): launcher", jkpro_cycle, now - started, tries);
    (void)unlink(st_path);
    return 0;
}

static void jkpro_launcher_screen(void)
{
    static BlurHost host;
    static char files_dir[200], prefix_dir[160], game_dir[256], other[256], exe[64], win_dir[300],
                root[160], saves_drop[240], st_path[200], docs_dir[240], win_docs[260],
                datapacks_dir[240], win_datapacks[260];
    static JkProDecision decision;
    if (blur_migrate_blocked) {
        blur_stop_screen(blur_migrate_text, "blur-migrate-blocked", 1);
        leave_to_home("blur-migrate-blocked");
    }
    snprintf(prefix_dir, sizeof(prefix_dir), "%s/prefix", library_root);
    snprintf(st_path, sizeof st_path, "%s/start.txt", library_root);
    snprintf(root, sizeof root, "%s", library_root);
    char *slash = strrchr(root, '/');
    if (slash) *slash = 0;                                   /* <JKPro> */
    /* Everything we and Wine create under <JKPro> stays writable for the console's
     * FTP server (another user): option files and saves are copied in by hand. */
    umask(0);
    {
        static const char *const open_dirs[] = { "Saves", "DataPacks", "System/backup", "Logs" };
        for (size_t i = 0; i < sizeof(open_dirs) / sizeof(open_dirs[0]); i++) {
            char od[240];
            snprintf(od, sizeof od, "%s/%s", root, open_dirs[i]);
            int n = jkpro_open_perms(od);
            if (n > 0) PS5LOG_LOG("JKPRO perms opened %s entries=%d", od, n);
        }
    }
    char flag[220];
    snprintf(flag, sizeof flag, "%s/renderer-pe", library_root);
    decision.pe_renderer = access(flag, F_OK) == 0;
    /* jkpro-files/: d3d9.dll (native client, the default) and d3d9-pe.dll (the PE fallback client);
     * the launcher's PATCH installs the one <System>/renderer-pe selects. */
    for (size_t i = 0; i < sizeof(runtime_roots) / sizeof(runtime_roots[0]) && !files_dir[0]; i++) {
        char probe[240];
        snprintf(probe, sizeof(probe), "%s/jkpro-files/d3d9.dll", runtime_roots[i]);
        if (access(probe, R_OK) == 0) snprintf(files_dir, sizeof(files_dir), "%s/jkpro-files", runtime_roots[i]);
    }
    int found = bl_game_dir_find("/data", game_dir, sizeof(game_dir), other, sizeof(other)) == 1 &&
                jkpro_find_ci(game_dir, "pes2013.exe", exe, sizeof(exe));
    /* The game's Documents: <JKPro>/Saves/<folder> (My Documents is redirected there before each start) */
    snprintf(docs_dir, sizeof docs_dir, "%s/Saves/%s", root,
             found && strrchr(game_dir, '/') ? strrchr(game_dir, '/') + 1 : "PES13");
    snprintf(saves_drop, sizeof saves_drop, "%s/Import", docs_dir);
    /* The data packs: <JKPro>/DataPacks/<folder> (Common AppData is redirected there before each start;
     * PES 2013 reads <Common AppData>/KONAMI/Pro Evolution Soccer 2013/download/dt0f.img) */
    snprintf(datapacks_dir, sizeof datapacks_dir, "%s/DataPacks/%s", root,
             found && strrchr(game_dir, '/') ? strrchr(game_dir, '/') + 1 : "PES13");

    int tries = 0;
    int retry = found && jkpro_retry_check(st_path, &tries);
    snprintf(flag, sizeof flag, "%s/logs-on", library_root);
    decision.logs = access(flag, F_OK) == 0;
    decision.show_fps = 0;

    int video_status = pw_videoout_ps5_open(&host.video);
    host.ops_ok = pw_pad_ps5_platform_ops(&host.pad_ops) == PW_OK;
    host.pad_status = host.ops_ok ? pw_pad_ps5_open(&host.pad, &host.pad_ops, pad_open_map,
                                                    sizeof(pad_open_map) / sizeof(pad_open_map[0]))
                                  : PW_ERR_STATE;
    host.pad_tried = now_ns();
    PS5LOG_LOG("JKPRO open video=%s pad=%s files=%s game=%s found=%d retry=%d", pw_result_name(video_status),
               pw_result_name(host.pad_status), files_dir[0] ? files_dir : "-", game_dir, found, retry);
    {   /* system.reg damaged by a crash and no good copy for the launcher to put back (its E611):
         * JkFlow's own seed import (update mode) restores the seed's registry files below. */
        char good[220];
        snprintf(good, sizeof good, "%s/backup/registry", library_root);
        int arm = jkpro_reseed_arm(prefix_dir, good);
        if (arm) PS5LOG_LOG("JKPRO registry re-seed armed=%d (system.reg damaged, no good copy)", arm);
    }
    if (blur_prepare_prefix(&host, video_status == PW_OK, prefix_dir) != 0)
        leave_to_home("jkpro-seed-failed");

    if (!retry) {
        JkProServices s;
        memset(&s, 0, sizeof(s));
        s.game_dir = found ? game_dir : "";
        s.prefix_dir = prefix_dir;
        s.system_dir = library_root;
        s.files_dir = files_dir[0] ? files_dir : "/app0/jkpro-files";
        s.saves_drop = saves_drop;
        s.usb_dir = access("/mnt/usb0", F_OK) == 0 ? "/mnt/usb0" : (access("/mnt/usb1", F_OK) == 0 ? "/mnt/usb1" : NULL);
        s.ctx = &host;
        s.present = jkpro_ui_present;
        s.pad = jkpro_ui_pad;
        s.now_ns = jkpro_ui_now;
        s.sleep_us = jkpro_ui_sleep;
        s.log = jkpro_ui_log;
        s.wall_time = jkpro_ui_wall;
        s.start_failure = jkpro_failure;
        s.logs_dir = logs_root;
        s.docs_dir = docs_dir;
        s.datapacks_dir = datapacks_dir;
        {   /* the Official Build badge: jkpro-files/release.txt + release.sig under the runtime root */
            static JkRelease rel;
            char app_root[200];
            snprintf(app_root, sizeof app_root, "%s", s.files_dir);
            char *cut = strrchr(app_root, '/');
            if (cut) *cut = 0;
            (void)jk_release_verify_in(app_root, "jkpro-files", "JKPRO-RELEASE 1", JK_RELEASE_PK, &rel);
            PS5LOG_LOG("JKPRO release state=%d changed=%d root=%s", (int)rel.state, rel.changed, app_root);
            s.release = &rel;
            s.build_id = "JKPro 1.0";
        }
        int rc = video_status == PW_OK ? jkpro_ui_run(&s, &decision) : -1;
        PS5LOG_LOG("JKPRO ui rc=%d action=%d logs=%d pe=%d", rc, (int)decision.action, decision.logs,
                   decision.pe_renderer);
        if (rc != 0 || decision.action != JKPRO_START) {
            if (host.pad_status == PW_OK) (void)pw_pad_ps5_close(&host.pad);
            if (video_status == PW_OK) (void)pw_videoout_ps5_close(&host.video);
            leave_to_home(rc != 0 ? "jkpro-ui-no-display" : "jkpro-ui-close");
        }
    }
    if (host.pad_status == PW_OK) (void)pw_pad_ps5_close(&host.pad);
    if (video_status == PW_OK) (void)pw_videoout_ps5_close(&host.video);
    if (!found) leave_to_home("jkpro-no-game");

    {   /* every start (crash retries too): My Documents -> docs_dir, settings.dat locked to 1080p High */
        snprintf(win_docs, sizeof win_docs, "Z:%s", docs_dir);
        for (char *c = win_docs; *c; c++) if (*c == '/') *c = '\\';
        char user_reg[200], sdat_dir[260];
        snprintf(user_reg, sizeof user_reg, "%s/user.reg", prefix_dir);
        int mk = jk_fs_mkdir_p(docs_dir);
        int redir = jkpro_redirect_documents(user_reg, win_docs);
        /* not redirected: the game reads the prefix's own Documents, so the lock goes there */
        if (redir < 0) snprintf(sdat_dir, sizeof sdat_dir, "%s/drive_c/users/BLUR/Documents", prefix_dir);
        else snprintf(sdat_dir, sizeof sdat_dir, "%s", docs_dir);
        int locked = jk_pes13set_write_locked(sdat_dir);
        PS5LOG_LOG("JKPRO documents=%s mkdir=%d redirect=%d settings.dat 1920x1080 high fs=%d (%s)", win_docs, mk,
                   redir, locked, sdat_dir);
    }
    {   /* every start: Common AppData -> datapacks_dir (system.reg, both registry views) */
        snprintf(win_datapacks, sizeof win_datapacks, "Z:%s", datapacks_dir);
        for (char *c = win_datapacks; *c; c++) if (*c == '/') *c = '\\';
        char system_reg[200], dl[320];
        snprintf(system_reg, sizeof system_reg, "%s/system.reg", prefix_dir);
        snprintf(dl, sizeof dl, "%s/KONAMI/Pro Evolution Soccer 2013/download", datapacks_dir);
        int mk = jk_fs_mkdir_p(dl);
        int redir = jkpro_redirect_common_appdata(system_reg, win_datapacks);
        PS5LOG_LOG("JKPRO datapacks=%s mkdir=%d redirect=%d", win_datapacks, mk, redir);
    }

    {   /* start record for the crash retry */
        char line[64];
        int n = snprintf(line, sizeof line, "%lld %d\n", (long long)time(NULL), retry ? tries : 0);
        (void)jk_fs_write_atomic(st_path, line, (size_t)n);
    }
    /* Z:\data\JKPRO\PES13 (the spelling on disk) */
    snprintf(win_dir, sizeof(win_dir), "Z:%s", game_dir);
    for (char *c = win_dir; *c; c++) if (*c == '/') *c = '\\';

    /* Native renderer by default (bench: owner match at 1080p High held 60, 198 MB free, enc17 server
     * 86e62a08 in sce_module); <System>/renderer-pe is the PE fallback (c988 client). */
    int native = !decision.pe_renderer, logs = decision.logs;
    if (native) {   /* the client's oneshot arm, before every native start */
        char arm[300];
        snprintf(arm, sizeof arm, "%s/dxn_arm.txt", game_dir);
        int armed = jk_fs_write_atomic(arm, "oneshot", 7);
        PS5LOG_LOG("JKPRO native arm %s rc=%d", arm, armed);
    }
    memset(&blur_decision, 0, sizeof(blur_decision));
    blur_decision.action = BL_START_GAME;
    snprintf(blur_decision.profile_text, sizeof(blur_decision.profile_text),
             "[application]\nid = pes13\nname = PES 2013\nexecutable = %s\\%s\nworking_directory = %s\n"
             "dll_overrides = d3d9=n\nruntime = wine-wow64\narchitecture = pe32\ngraphics = dxvk\n"
             "[display]\ndesktop = 1920x1080\nscaling = fit\nshow_fps = %s\n"
             "[input]\nmode = xinput\n"
             "[runtime]\nfast_clock = true\ncpu = native\n",
             win_dir, exe, win_dir, decision.show_fps ? "true" : "false");
    snprintf(blur_decision.dxvk_conf, sizeof(blur_decision.dxvk_conf),
             "d3d9.maxFrameLatency = 1\nd3d9.maxFrameRate = 60\nd3d9.presentInterval = 1\n"
             "d3d9.maxAvailableMemory = 512\nd3d9.memoryTrackTest = True\nd3d9.evictManagedOnUnlock = True\n"
             /* kitserver 13's kload.dll: CreateDevice no longer fails on the first swapchain */
             "d3d9.deferSurfaceCreation = True\n");
    unsigned n = 0;
#define JKPRO_ENV(k, v) do { snprintf(blur_decision.env[n].name, sizeof(blur_decision.env[n].name), "%s", k); \
        snprintf(blur_decision.env[n].value, sizeof(blur_decision.env[n].value), "%s", v); n++; } while (0)
    JKPRO_ENV("WINEDEBUG", logs ? "+tid,err+all,+loaddll,+msgbox" : "-all");
    if (logs) JKPRO_ENV("BLUR_LOGS", "1");
    /* Production: with Logs off nothing diagnostic is written to the game folder.
     * DXVK_LOG_LEVEL none = no <exe>_d3d9.log; DXVK_PS4_LOGS 0 = the engine's own log switch off
     * (checked before any switch file). Logs on (SETTINGS) keeps the support logs. */
    JKPRO_ENV("DXVK_LOG_LEVEL", logs ? "info" : "none");
    JKPRO_ENV("DXVK_PS4_LOGS", logs ? "1" : "0");
    /* No DXVK_PS4_FRAME_LOG: with it kitserver 13's kload.dll faulted on every start (v0.4). */
    JKPRO_ENV("PW_NATIVE_ALERT_UMTX", "1");
    JKPRO_ENV("PW_SERVER_PRIO", "699");
    JKPRO_ENV("DXVK_PS4_RACE_LOCK", "0");
    /* No forced-exec migration of every mapping (agent A: the kernel-panic window and a flexible-memory
     * consumer; bench T1x 60 -> 227 MB free). Read by JkFlow's ntdll (virtual_set_force_exec). */
    JKPRO_ENV("PW_PS4_FORCE_EXEC", "0");
    {   /* SETTINGS > Online server (ThirteenServer): <System>/online-hosts holds the table for the
         * Wine resolver (engine/online/pw_hosts.h); absent = off, the game as in 1.00. */
        char hosts_path[240], hosts[sizeof(blur_decision.env[0].value)];
        snprintf(hosts_path, sizeof hosts_path, "%s/online-hosts", library_root);
        FILE *hf = fopen(hosts_path, "rb");
        size_t hn = hf ? fread(hosts, 1, sizeof hosts - 1, hf) : 0;
        if (hf) fclose(hf);
        while (hn && (hosts[hn - 1] == '\n' || hosts[hn - 1] == '\r' || hosts[hn - 1] == ' ')) hn--;
        hosts[hn] = 0;
        if (hn) JKPRO_ENV("JK_HOSTS", hosts);
        /* Wine on the console has no Nsi device: GetAdaptersInfo says "no adapter" and the game reports
         * its network settings. Our client's net cache mode 3 serves one synthetic Ethernet adapter with
         * the console's real IPv4 (as JkFlow/Blur online, DXVK_PS4_NET_CACHE=3). */
        if (hn) JKPRO_ENV("DXVK_PS4_NET_CACHE", "3");
        /* online + Logs: trace the network libraries the game uses (support logs only) */
        if (hn && logs)
            for (unsigned i = 0; i < n; i++)
                if (!strcmp(blur_decision.env[i].name, "WINEDEBUG"))
                    snprintf(blur_decision.env[i].value, sizeof(blur_decision.env[i].value), "%s",
                             "+tid,err+all,+loaddll,+msgbox,+winsock,+iphlpapi,+nsi,+wininet,+winhttp,+dnsapi,+netapi32");
        PS5LOG_LOG("JKPRO online=%s", hn ? "on" : "off");
    }
    if (native) {   /* bench-verified set; no DXVK_PS4_D3D9_THREAD* (presents stopped after ~20 s) */
        JKPRO_ENV("DXVK_PS4_NATIVE", "menu");
        JKPRO_ENV("DXVK_PS4_NATIVE_ENC_WORKER", "1");
        JKPRO_ENV("DXVK_PS4_NATIVE_FX_WORKER", "1");
        JKPRO_ENV("DXVK_PS4_NATIVE_PRESENT_ASYNC", "1");
        JKPRO_ENV("DXVK_PS4_NATIVE_ENC_SPLIT", "2");
        JKPRO_ENV("DXVK_PS4_NATIVE_FX_DEDUPE", "1");
        JKPRO_ENV("DXVK_PS4_NATIVE_FX_BATCH", "1");
        JKPRO_ENV("DXVK_PS4_NATIVE_PIN_STAMP", "1");
        JKPRO_ENV("DXVK_PS4_NATIVE_FX_LITE", "1");
        JKPRO_ENV("DXVK_PS4_NATIVE_FX_BRACKET_LITE", "1");
    }
#undef JKPRO_ENV
    blur_decision.env_count = n;
    blur_write_dxvk_conf();
    PS5LOG_LOG("JKPRO start exe=%s\\%s native=%d logs=%d retry=%d env=%u", win_dir, exe, native, logs, retry, n);
}
