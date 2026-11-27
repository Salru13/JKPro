#!/usr/bin/env bash
# JKPro v0.1 on the JkFlow 1.00 title tree. Run in WSL on a fresh copy of blur-launcher-r4:
#   J=~/orbis-win-host-bootstrap/jkpro-title  bash apply_v01.sh
# Copies our launcher modules in, points the paths at /data/JkPro/PES13 (any letter case is found; JkPro is the default, like JkFlow/Jk4Speed) and replaces the
# Blur launcher call with jkpro_launcher_screen (jkpro_prepare.inc). Idempotent.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
L="${JKPRO_LAUNCHER:-$(cd "$here/.." && pwd)}"   # a launcher/ snapshot (git archive) for reproducible builds
J="${J:-$HOME/orbis-win-host-bootstrap/jkpro-title}"
W="$J/prospero/native/wine64_main.c"
O="${O:-$HOME/orbis-win-host-bootstrap/blur-launcher-r4/prospero}"
# always start from JkFlow 1.00's own files (idempotent re-apply)
cp "$O/native/wine64_main.c" "$W"; cp "$O/src/bl_paths.h" "$J/prospero/src/bl_paths.h"; cp "$O/src/bl_strings.c" "$J/prospero/src/bl_strings.c"
P="$J/prospero/src/bl_paths.h"

for f in jk_fs.c jk_fs.h jk_sha256.c jk_sha256.h jk_regimport.c jk_regimport.h jk_pes13set.c jk_pes13set.h jk_pes13set_base.inc \
         jk_games.c jk_games.h jk_gamecfg.c jk_gamecfg.h jk_ui.c jk_ui.h jk_art.c jk_art.h jkpro_ui.c jkpro_ui.h jkpro_draw.c jk_support.c jk_support.h \
         jk_keyboard.c jk_frontend.h jk_config.h jkpro_art.c jkpro_art.h jkpro_loader.c jkpro_loader.h jkpro_prefix.c jkpro_prefix.h jkpro_datapack.c jkpro_datapack.h jkpro_kitcfg.c jkpro_kitcfg.h jkpro_fservfix.c jkpro_fservfix.h jkpro_online.c jkpro_online.h \
         jk_release.c jk_release.h jk_release_key.h jk_ed25519.c jk_ed25519.h \
         jk_screens.h jk_modset.h jk_pace.h jk_patcher.h jk_saves.h jk_keyboard.h jk_controls.h; do
    cp "$L/src/$f" "$J/prospero/src/$f"
done
# the release badge's Ed25519 verify (jk_ed25519.c includes ../third_party/tweetnacl/tweetnacl.c)
mkdir -p "$J/prospero/third_party/tweetnacl"
cp "$L/third_party/tweetnacl/tweetnacl.c" "$L/third_party/tweetnacl/tweetnacl.h" "$J/prospero/third_party/tweetnacl/"
# the launcher font (Ubuntu Sans, baked by the launcher's own tool)
GEN="$J/jk-gen"; mkdir -p "$GEN"
rm -f "$GEN/jk_font.h" "$GEN/jk_font_data.c"     # always regenerated (the logo's ExtraBold Italic face)
bash "$L/tools/bake_assets.sh" "$GEN" >/dev/null
cp "$GEN/jk_font.h" "$GEN/jk_font_data.c" "$J/prospero/src/"
cp "$here/jkpro_prepare.inc" "$J/prospero/native/jkpro_prepare.inc"

sed -i -e 's#^\#define BL_DATA_ROOT .*#\#define BL_DATA_ROOT "/data/JkPro/System"#' \
       -e 's#^\#define BL_LOGS_ROOT .*#\#define BL_LOGS_ROOT "/data/JkPro/Logs"#' \
       -e 's#^\#define BL_WINE_GAME_DIR .*#\#define BL_WINE_GAME_DIR "Z:\\\\data\\\\JkPro\\\\PES13"#' \
       -e 's#^\#define BL_GAME_PARENT .*#\#define BL_GAME_PARENT "JkPro"#' \
       -e 's#^\#define BL_GAME_LEAF .*#\#define BL_GAME_LEAF "PES13"#' "$P"

# The first-start seed with 1.00's own copies of the files 1.01 replaced (fresh installs got E110):
# its new END hash goes into the title (build-launcher.sh N245_SEED_END); make_pkg stages the same seed.
V="${V:-$(dirname "$O")/package-view-292}"
SEED_END=$(python3 -I "$here/jkpro_seed.py" "$V" "$J/jkpro-seed")
[[ $SEED_END =~ ^[0-9a-f]{64}$ ]] || { echo "jkpro_seed.py gave no END" >&2; exit 1; }
sed -i -E "s/N245_SEED_END=[0-9a-f]{64}/N245_SEED_END=$SEED_END/" "$J/build-launcher.sh"
grep -q "N245_SEED_END=$SEED_END" "$J/build-launcher.sh" || { echo "build-launcher.sh END not set" >&2; exit 1; }
echo "seed END $SEED_END"

S="$J/prospero/src/bl_strings.c"
sed -i -e 's#"Starting Blur\\xE2\\x80\\xA6"#"Starting PES 2013\\xE2\\x80\\xA6"#' \
       -e 's#"Preparing BLUR environment"#"Preparing JKPro (first start only)"#' \
       -e 's#Your old JkFlow data in %s could not be moved to %s. Nothing was deleted. Free some space and start JkFlow again.#Your old JKPro data in %s could not be moved to %s. Nothing was deleted. Free some space and start JKPro again.#' "$S"
grep -n 'BL_S_LOADER\]\|BL_S_LOADER_PREPARE\]' "$S"

python3 - "$W" <<'EOF'
import sys, re
p = sys.argv[1]; s = open(p).read()
s = s.replace('"id = blur\\n"', '"id = pes13\\n"').replace('"name = Blur\\n"', '"name = PES 2013\\n"')
s = s.replace('"executable = Z:\\\\data\\\\JkFlow\\\\Blur\\\\Blur.exe\\n"', '"executable = Z:\\\\data\\\\JkPro\\\\PES13\\\\pes2013.exe\\n"')
s = s.replace('"working_directory = Z:\\\\data\\\\JkFlow\\\\Blur\\n"', '"working_directory = Z:\\\\data\\\\JkPro\\\\PES13\\n"')
s = s.replace('    "env = PW_SERVER_PRIO=699\\n"\n    "env = DXVK_PS4_NET_CACHE=3\\n";', '    "env = PW_SERVER_PRIO=699\\n";')
s = s.replace('#define PW_WINE64_GAME_DIR "/data/JkFlow/Blur"', '#define PW_WINE64_GAME_DIR "/data/JkPro/PES13"')
if 'jkpro_prepare.inc' not in s:
    s = s.replace('static void blur_launcher_screen(void)\n{', '__attribute__((unused)) static void blur_launcher_screen(void)\n{', 1)
    s = s.replace('/* Audit finding 7: extra[] holds', '#include "jkpro_prepare.inc"\n\n/* Audit finding 7: extra[] holds', 1)
    n = s.count('    blur_launcher_screen();')
    assert n == 1, n
    s = s.replace('    blur_launcher_screen();', '''    for (int ai = 1; ai < argc; ai++) {
        const char *cy = argv[ai] ? strstr(argv[ai], "cycle=") : NULL;
        if (cy) jkpro_cycle = (unsigned)strtoul(cy + 6, NULL, 10);
        PS5LOG_LOG("JKPRO arg %d=%s", ai, argv[ai] ? argv[ai] : "-");
    }
    jkpro_launcher_screen();''')
# JKPro's kick-off loader (launcher/src/jkpro_loader.c) in place of JkFlow's bl_loader: the
# first-start setup, its E110 screen and "Starting PES 2013..." until the game's first frame.
if 'jkpro_bl_draw' not in s:
    s = s.replace('#include "bl_loader.h"\n', '''#include "bl_loader.h"
#include "jkpro_loader.h"
static void jkpro_bl_draw(BlFrame *f, uint64_t t_ms)
{
    JkFrame j = { f->pixels, f->width, f->height, f->stride };
    jkpro_loader_draw(&j, t_ms, NULL);
}
static void jkpro_bl_progress(BlFrame *f, uint64_t t_ms, const char *big, unsigned done, unsigned total,
                              const char *status)
{
    JkFrame j = { f->pixels, f->width, f->height, f->stride };
    jkpro_loader_draw_progress(&j, t_ms, big, done, total, status);
}
''', 1)
    for old, new, want in (('bl_loader_draw(', 'jkpro_bl_draw(', 1), ('bl_loader_draw_progress(', 'jkpro_bl_progress(', 2),
                           ('    bl_loader_release();', '    jkpro_loader_release();', 1)):
        assert s.count(old) == want, (old, s.count(old))
        s = s.replace(old, new)
# Online account (1.01, owner: "mata in dem i förväg" as in Blur): the launcher keeps the player's
# ThirteenServer login in <System>/online-account (line 1 the ID, line 2 the password). In the game,
# L1 + touchpad click types the ID and R1 + touchpad click the password into the focused field,
# through the on-screen keyboard's own typing path (pw_osk_finished: the keys, then Enter).
if 'jkpro_account_load' not in s:
    s = s.replace('#include "jkpro_loader.h"\n', '''#include "jkpro_loader.h"
#define JKPRO_ACCOUNT_FILE "/data/JkPro/System/online-account"
static char jkpro_account_id[64], jkpro_account_pw[64], jkpro_account_code[64];
static int jkpro_account_ok;
static void jkpro_account_line(FILE *f, char *out, size_t cap)
{
    if (!fgets(out, (int)cap, f)) { out[0] = 0; return; }
    size_t n = strlen(out);
    while (n && (out[n - 1] == '\\n' || out[n - 1] == '\\r')) out[--n] = 0;
}
static void jkpro_account_load(void)
{
    FILE *f = fopen(JKPRO_ACCOUNT_FILE, "rb");
    if (!f) f = fopen(JKPRO_ACCOUNT_FILE ".txt", "rb");   /* Notepad's default name */
    if (!f) { PS5LOG_LOG("JKPRO account none"); return; }
    jkpro_account_line(f, jkpro_account_id, sizeof jkpro_account_id);
    jkpro_account_line(f, jkpro_account_pw, sizeof jkpro_account_pw);
    jkpro_account_line(f, jkpro_account_code, sizeof jkpro_account_code);   /* optional line 3: the registration code */
    fclose(f);
    jkpro_account_ok = jkpro_account_id[0] && jkpro_account_pw[0];
    PS5LOG_LOG("JKPRO account %s id_len=%u code=%s", jkpro_account_ok ? "ready" : "incomplete", (unsigned)strlen(jkpro_account_id),
               jkpro_account_code[0] ? "own" : "id");
}
''', 1)
    a = '''            osk_status = pw_osk_ps5_open(&osk_ps5, pw_osk_ps5_system_ops(osk_log), user);
            game_menu.keyboard = osk_status == PW_OK;
'''
    assert s.count(a) == 1, 'osk open'
    s = s.replace(a, a + '            jkpro_account_load();\n')
    a = '''            if (osk_status == PW_OK) {
                PwOskView view;
'''
    assert s.count(a) == 1, 'osk tick'
    s = s.replace(a, '''            if (osk_status == PW_OK && jkpro_account_ok && (pressed & PW_OSK_TOUCHPAD) &&
                (held & (0x400u | 0x800u)) && !osk_ps5.showing) {
                /* L1: the ID, R1: the password, L1+R1: the registration code (line 3; the server
                   accepts any text, so without one the ID is typed) */
                const int l1 = (held & 0x400u) != 0, r1 = (held & 0x800u) != 0;
                const char *what = l1 && r1 ? "code" : l1 ? "id" : "password";
                const char *text = l1 && r1 ? (jkpro_account_code[0] ? jkpro_account_code : jkpro_account_id)
                                 : l1 ? jkpro_account_id : jkpro_account_pw;
                pw_osk_open(&osk);
                pw_osk_opened(&osk, 1);
                pw_osk_finished(&osk, text);
                pressed &= ~PW_OSK_TOUCHPAD;
                PS5LOG_LOG("JKPRO account typed %s", what);
            }
''' + a)
# E110 (1.02, three player reports with no reason): the seed import's status and detail lived only in the
# session log, which opens after the launcher, so a failed first start left nothing. Now the reason is
# drawn on the E110 screen and appended to <System>/state/launcher.log (also on a later Retry), and the
# texts say JKPro.
if 'jkpro_seed_reason' not in s:
    s = s.replace('#define BLUR_HOME_NOTE "The PS4 may then show \'Something went wrong\'. That is expected here; start JkFlow again."',
                  '#define BLUR_HOME_NOTE "The PS4 may then show \'Something went wrong\'. That is expected here; start JKPro again."')
    old = '''        jkpro_bl_progress(&draw.frame, (now_ns() - draw.started) / 1000000ull, bl_str(BL_S_LOADER_PREPARE), 0, 0,
                                "E110 JkFlow could not prepare its files. Cross: try again   Circle: close");
        blur_home_note(&draw.frame, 0);       /* Circle: leave_to_home */
'''
    assert s.count(old) == 1, 'E110 screen'
    s = s.replace(old, '''        char why[160];
        jkpro_seed_reason(st, &seed, attempt, why, sizeof why);
        jkpro_bl_progress(&draw.frame, (now_ns() - draw.started) / 1000000ull, bl_str(BL_S_LOADER_PREPARE), 0, 0,
                                "E110 JKPro could not prepare its files. Cross: try again   Circle: close");
        (void)bl_text(&draw.frame, BL_FONT_NOTE, ((int)draw.frame.width - bl_text_width(BL_FONT_NOTE, why)) / 2,
                      (int)draw.frame.height * 80 / 100, why, 0xFFD27A);
        blur_home_note(&draw.frame, 0);       /* Circle: leave_to_home */
''')
    old = 'static int blur_prepare_prefix(BlurHost *host, int video_ok, const char *prefix)\n{'
    assert s.count(old) == 1, 'prepare_prefix'
    s = s.replace(old, '''/* JKPro: one line for the player under E110, and the full reason in <System>/state/launcher.log. */
static void jkpro_seed_reason(N245Status st, const N245Result *seed, unsigned attempt, char *why, size_t cap)
{
    const char *d = seed->detail[0] ? seed->detail : "-";
    if (st == N245_ERR_IO && strstr(d, "errno=28"))
        snprintf(why, cap, "The PS4 storage is full. Free some space, then press Cross. (%s)", n245_status_name(st));
    else if (st == N245_ERR_IO)
        snprintf(why, cap, "Could not write to /data/JkPro/System. Free some space, restart the PS4, start JKPro. (%s)",
                 n245_status_name(st));
    else if (st == N245_ERR_SEED)
        snprintf(why, cap, "JKPro's own files are damaged. Install the JKPro pkg again. (%s)", n245_status_name(st));
    else
        snprintf(why, cap, "Delete /data/JkPro/System/prefix by FTP, then press Cross. (%s)", n245_status_name(st));
    char dir[200], path[220];
    snprintf(dir, sizeof dir, "%s/state", library_root);
    (void)mkdir(dir, 0777);
    snprintf(path, sizeof path, "%s/launcher.log", dir);
    FILE *f = fopen(path, "ab");
    if (f) {
        fprintf(f, "E110 seed status=%s attempt=%u files=%u copied=%u kept=%u detail=%s\\n", n245_status_name(st),
                attempt, seed->files_total, seed->copied, seed->kept, d);
        fclose(f);
    }
}

''' + old)
open(p, 'w').write(s)
print('wine64_main.c patched')
EOF
grep -n 'id = pes13\|PES13\|jkpro_launcher_screen\|jkpro_prepare.inc\|NET_CACHE' "$W" | head
! grep -n 'bl_loader_draw(\|bl_loader_draw_progress(\|bl_loader_release()' "$W" || { echo "bl_loader still drawn" >&2; exit 1; }
grep -n 'BL_GAME_PARENT\|BL_GAME_LEAF\|BL_DATA_ROOT\|BL_WINE_GAME_DIR' "$P"
