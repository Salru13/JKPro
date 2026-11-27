#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# Package JKPro (JKPR00001) on the JkFlow 1.00 runtime (WSL), after apply_v01.sh + build-launcher.sh.
#   bash launcher/jkflow/make_pkg_jkpro.sh
# Payload: JkFlow 1.00's merged package view (package-view-292: runtime, sce_module, win/, seed245)
# minus blur-files/, plus our title prx, the native engine (jkpro-files/d3d9.dll: client 08cab072,
# 28c101ff + the GetDisplayMode X8R8G8B8 fix; dxvknative.prx: the enc17 server 86e62a08, the fself of
# the bench's 771525a0, in sce_module/ and win/.../x86_64-unix/ instead of JkFlow's b2c117c8), the PE
# fallback client (jkpro-files/d3d9-pe.dll: c988 16ca3a54), our param.sfo and icons.
# No game, Konami, Blur or AMAX file.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
B="${JK_BOOTSTRAP:-$HOME/orbis-win-host-bootstrap}"
J="${J:-$B/jkpro-title}"
V="$B/blur-launcher-r4/package-view-292"
TITLE_ID="${TITLE_ID:-JKPR00001}"
VER="${JK_VERSION:-01.01}"
CONTENT_ID="IV0000-${TITLE_ID}_00-JKPRO00000000001"
TOOLS="$B/native298-package277/orbis/sdk/orbis-sdk-v1/sdk/bin/linux"
PKGTOOL="$TOOLS/PkgTool.Core"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
export LD_LIBRARY_PATH="$HOME/.cache/orbis-ports/openssl11/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
say() { printf '== %s\n' "$*"; }
die() { echo "make_pkg_jkpro: $*" >&2; exit 1; }
pin() { [ "$(sha256sum "$1" | cut -c1-64)" = "$2" ] || die "unexpected $3: $1 (want ${2:0:8})"; }
PRX="$J/reloc-build/prx/postlink/clean/relocated_wine_app.prx"
[ -f "$PRX" ] || die "no clean title prx: build first"
grep -a -q "JKPRO start" "$PRX" || die "the title prx has no JKPro code"
for want in PW_PS4_FORCE_EXEC "d3d9.deferSurfaceCreation = True" "d3d9.maxAvailableMemory = 512" "JKPRO documents=" \
            DXVK_PS4_NATIVE_FX_BRACKET_LITE dxn_arm.txt; do
    grep -a -q "$want" "$PRX" || die "the title prx lacks: $want"
done
NATIVE_CLIENT="${JK_NATIVE_CLIENT:-$here/../../engine/client/candidate/d3d9.dll}"
pin "$NATIVE_CLIENT" 08cab072514f9e481a56a16454f4d9aa675f2da793bfb190cf18830712281d7a "native client"
PE_CLIENT="${JK_PE_CLIENT:-$B/native288-frame260-racelock-2mode/d3d9.c988.dll}"
pin "$PE_CLIENT" 16ca3a5452026ead32fc45813b87a4cc601a4ec7b8ac009f4e502de20e12737d "PE client (c988)"
SERVER="${JK_SERVER_PRX:-$HOME/nfsmw-bench/native/server-enc17/link/dxvknative.prx}"
pin "$SERVER" 86e62a08a2a88ee67faa20313fdf544bbdce5133cd67d86a8a496ab1322fbcf7 "native server (enc17)"
SRV_PATHS="sce_module/dxvknative.prx win/wine/lib/wine/x86_64-unix/dxvknative.prx"
# 1.01 online (ThirteenServer): JkFlow's ws2_32 + the JK_HOSTS table, engine/online/build_jkpro_ws2_32.py
WS2="${JK_WS2_PRX:-$B/jkpro-ws2_32-r1/ws2_32.prx}"
pin "$WS2" 34e15fdf20f4c0ced5ccb72c9e333f1cf2a95c2084e296a3fb921552f2f034b6 "ws2_32 (online)"
WS2_PATHS="sce_module/ws2_32.prx win/wine/lib/wine/x86_64-unix/ws2_32.prx"
# 1.01 online: Wine iphlpapi + a synthetic adapter (the console has no Nsi device), engine/online/build_jkpro_iphlpapi.sh
IPH="${JK_IPHLPAPI_DLL:-$B/jkpro-iphlpapi-r4/iphlpapi.dll}"
pin "$IPH" b21bd4d0e23758f830aab3164c3c6df2845ab9fbaec19959477fe47d0cbeac3a "iphlpapi (online)"
IPH_PATH="win/wine/lib/wine/i386-windows/iphlpapi.dll"
# 1.01 online: Wine ws2_32 (i386) where refused IPv4 multicast options report success (UPnP/SSDP), engine/online/build_jkpro_ws2_32_pe.sh
WSPE="${JK_WS2_PE_DLL:-$B/jkpro-ws2pe-r1/ws2_32.dll}"
pin "$WSPE" c39593fa953482ca2d4f43e8db0b5e86c48c7a71f3a12e6cb04ebc0c668f8b1c "ws2_32.dll (online)"
WSPE_PATH="win/wine/lib/wine/i386-windows/ws2_32.dll"

STAGE="$J/pkg-stage"; rm -rf "$STAGE" "$J/package"
mkdir -p "$STAGE/jkpro-files" "$STAGE/sce_sys" "$J/package"
cp "$NATIVE_CLIENT" "$STAGE/jkpro-files/d3d9.dll"
cp "$PE_CLIENT" "$STAGE/jkpro-files/d3d9-pe.dll"      # PATCH installs it when <System>/renderer-pe exists
mkdir -p "$STAGE/jkpro-files/about"                      # About > credits (as JK4SPEED's about/)
cp "$here/../assets/CREDITS-JKPro.txt" "$STAGE/jkpro-files/about/CREDITS.txt"
for rel in $SRV_PATHS; do
    [ -e "$V/$rel" ] || die "the view has no $rel"
    mkdir -p "$(dirname "$STAGE/$rel")"; cp "$SERVER" "$STAGE/$rel"
done
for rel in $WS2_PATHS; do
    [ -e "$V/$rel" ] || die "the view has no $rel"
    mkdir -p "$(dirname "$STAGE/$rel")"; cp "$WS2" "$STAGE/$rel"
done
[ -e "$V/$IPH_PATH" ] || die "the view has no $IPH_PATH"
mkdir -p "$(dirname "$STAGE/$IPH_PATH")"; cp "$IPH" "$STAGE/$IPH_PATH"
[ -e "$V/$WSPE_PATH" ] || die "the view has no $WSPE_PATH"
mkdir -p "$(dirname "$STAGE/$WSPE_PATH")"; cp "$WSPE" "$STAGE/$WSPE_PATH"
# 1.02: the first-start seed from apply_v01.sh (jkpro_seed.py): 1.00's own copies of the files above,
# whose END the title must carry (else every first start is E110)
SEED="$J/jkpro-seed/seed245"
[ -f "$SEED/blur-native248/seed.idx" ] || die "no jkpro-seed: run apply_v01.sh"
SEED_END=$(tail -1 "$SEED/blur-native248/seed.idx" | cut -c5-)
grep -a -q "$SEED_END" "$PRX" || die "the title prx does not carry the seed END ${SEED_END:0:8}"
cp -r "$SEED" "$STAGE/seed245"

say "release record (jkpro-files/release.txt; release.sig only from the owner)"
# The signature covers our code too, not only jkpro-files/: the title (eboot.bin + prx) and the native
# server at their installed paths (jk_release_verify hashes <app_root>/<path>).
python3 -I - "$STAGE/jkpro-files" \
    "eboot.bin=$J/reloc-build/eboot.bin" "sce_module/relocated_wine_app.prx=$PRX" \
    "sce_module/dxvknative.prx=$SERVER" "win/wine/lib/wine/x86_64-unix/dxvknative.prx=$SERVER" \
    "sce_module/ws2_32.prx=$WS2" "win/wine/lib/wine/x86_64-unix/ws2_32.prx=$WS2" "$IPH_PATH=$IPH" "$WSPE_PATH=$WSPE" <<'PYEOF'
import hashlib, os, sys
base = sys.argv[1]
lines = ['JKPRO-RELEASE 1', 'build JKPro ' + os.environ.get('JK_BUILD_NAME', '1.01')]
for spec in sys.argv[2:]:
    rel, src = spec.split('=', 1)
    lines.append('%s  %s' % (hashlib.sha256(open(src, 'rb').read()).hexdigest(), rel))
rels = []
for d, _, files in os.walk(base):
    for n in files:
        rels.append(os.path.relpath(os.path.join(d, n), base).replace(os.sep, '/'))
for rel in sorted(rels):
    if rel in ('release.txt', 'release.sig') or os.path.islink(os.path.join(base, rel)):
        continue
    h = hashlib.sha256(open(os.path.join(base, rel), 'rb').read()).hexdigest()
    lines.append('%s  jkpro-files/%s' % (h, rel))
open(os.path.join(base, 'release.txt'), 'w', newline='\n').write('\n'.join(lines) + '\n')
print('   release.txt: %d files (incl. title + server)' % (len(lines) - 2))
PYEOF
# The owner signs $STAGE/jkpro-files/release.txt (never this script) and puts the raw 64-byte
# Ed25519 signature at $J/release.sig; it is checked here with the public key only.
SIG="${JK_RELEASE_SIG:-$J/release.sig}"
if [ -f "$SIG" ]; then
    PKH="$here/../src/jk_release_key.h"
    hex=$(grep -o '0x[0-9a-fA-F][0-9a-fA-F]' "$PKH" | sed 's/0x//' | tr -d '\n')
    [ ${#hex} = 64 ] || die "no 32-byte public key in $PKH"
    PUB="$J/release-pub.pem"
    { printf '%s' "302a300506032b6570032100$hex" | xxd -r -p | base64 -w0 | sed 's/^/-----BEGIN PUBLIC KEY-----\n/;s/$/\n-----END PUBLIC KEY-----/'; echo; } > "$PUB"
    openssl pkeyutl -verify -pubin -inkey "$PUB" -rawin -in "$STAGE/jkpro-files/release.txt" -sigfile "$SIG" >/dev/null 2>&1 \
        || die "release.sig does not verify against jk_release_key.h for this release.txt: not packaged"
    cp "$SIG" "$STAGE/jkpro-files/release.sig"
    echo "   release.sig verified: official build"
else
    echo "   no release.sig: Unofficial build"
fi
python3 -I "$here/make_icons.py" "$STAGE/sce_sys" >/dev/null
cp -L "$V/sce_sys/keystone" "$STAGE/sce_sys/keystone"
SFO="$STAGE/sce_sys/param.sfo"
"$PKGTOOL" sfo_new "$SFO" >/dev/null
set_sfo() { "$PKGTOOL" sfo_setentry --value "$3" --type "$2" ${4:+--maxsize "$4"} "$SFO" "$1" >/dev/null; }
set_sfo APP_TYPE integer 1
set_sfo APP_VER utf8 "$VER" 8
set_sfo ATTRIBUTE integer 0x00010000
set_sfo CATEGORY utf8 gd 4
set_sfo CONTENT_ID utf8 "$CONTENT_ID" 48
set_sfo DOWNLOAD_DATA_SIZE integer 0
set_sfo PARENTAL_LEVEL integer 5
set_sfo PUBTOOLINFO utf8 "c_date=20261125,img0_l0_size=0,img0_l1_size=0,img0_sc_ksize=512,img0_pc_ksize=832" 512
set_sfo PUBTOOLVER integer 0x02890000
set_sfo SYSTEM_VER integer 0
set_sfo TITLE utf8 JKPro 128
set_sfo TITLE_ID utf8 "$TITLE_ID" 12
set_sfo VERSION utf8 "$VER" 8
"$PKGTOOL" sfo_listentries "$SFO" > "$J/param-sfo.txt"

say "file list"
LIST="$J/files.tsv"; : > "$LIST"
printf 'eboot.bin\t%s\n' "$J/reloc-build/eboot.bin" >> "$LIST"
printf 'sce_module/relocated_wine_app.prx\t%s\n' "$PRX" >> "$LIST"
(cd "$V" && find -L . -type f | sed 's#^\./##' | sort) | while read -r rel; do
    case "$rel" in
        blur-files/*|eboot.bin|sce_module/relocated_wine_app.prx|sce_sys/*|seed245/*) continue ;;
        sce_module/dxvknative.prx|win/wine/lib/wine/x86_64-unix/dxvknative.prx) continue ;;   # staged: enc17
        sce_module/ws2_32.prx|win/wine/lib/wine/x86_64-unix/ws2_32.prx) continue ;;   # staged: online ws2_32
        win/wine/lib/wine/i386-windows/iphlpapi.dll) continue ;;   # staged: online iphlpapi
        win/wine/lib/wine/i386-windows/ws2_32.dll) continue ;;   # staged: online ws2_32 (PE)
    esac
    printf '%s\t%s\n' "$rel" "$(readlink -f "$V/$rel")" >> "$LIST"
done
(cd "$STAGE" && find . -type f | sed 's#^\./##' | sort) | while read -r rel; do printf '%s\t%s\n' "$rel" "$STAGE/$rel" >> "$LIST"; done
if cut -f1 "$LIST" | grep -i -E 'blur\.exe|pes2013\.exe|amax|blur_12|\.afs$|\.img$|\.cpk$|settings\.exe'; then die "a game/AMAX file is in the list"; fi
wc -l < "$LIST" | sed 's/^/   files: /'
say "first-start check (every seed file, from the files this package carries)"
python3 -I - "$LIST" <<'PYEOF'
import hashlib, sys
files = dict(l.rstrip('\n').split('\t', 1) for l in open(sys.argv[1]))
idx = files['seed245/blur-native248/seed.idx']
bad, n = [], 0
for line in open(idx, encoding='utf-8'):
    f = line.rstrip('\n').split('\t')
    if f[0] != 'F' or len(f) != 5:
        continue
    rel = 'seed245/blur-native248/objects/' + f[2] if f[4] == '-' else f[4]
    data = open(files[rel], 'rb').read() if rel in files else None
    n += 1
    if data is None or len(data) != int(f[1]) or hashlib.sha256(data).hexdigest() != f[2]:
        bad.append('%s <- %s' % (f[3], rel))
if bad:
    sys.exit('make_pkg_jkpro: a fresh install would stop at E110:\n  ' + '\n  '.join(bad))
print('   %d seed files match' % n)
PYEOF

say "gp4 + pkg"
python3 -I "$here/../title/make_gp4.py" "$J/jkpro.gp4" "$CONTENT_ID" "$LIST"
"$PKGTOOL" pkg_build "$J/jkpro.gp4" "$J/package" > "$J/pkg-build.log" 2>&1 || { tail -20 "$J/pkg-build.log"; die "pkg_build failed"; }
PKG="$J/package/$CONTENT_ID.pkg"
[ -f "$PKG" ] || die "no package written"
"$PKGTOOL" pkg_validate "$PKG" > "$J/pkg-validate.log" 2>&1 || { tail -20 "$J/pkg-validate.log"; die "pkg_validate failed"; }
grep -q '\[FAIL\]\|\[ERROR\]' "$J/pkg-validate.log" && die "pkg_validate reported a failure"
say "package"
sha256sum "$PKG" | tee "$J/package.sha256"
stat -c '%s bytes' "$PKG"
cat "$J/param-sfo.txt" | grep -E "TITLE|CONTENT|VERSION|APP_VER"
