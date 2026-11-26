#!/bin/bash
# Host-Wine test: RED with the do-nothing stub, GREEN with jkdiag.dll.
# Layout mirrors the console: PES13/kitserver/jkdiag.dll -> PES13/jkdiag/
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
B=${ORBIS_BOOTSTRAP:-$HOME/orbis-win-host-bootstrap}
WINE=$B/wine-host/bin/wine
OUT=/tmp/jkdiag-build
bash "$HERE/build.sh" "$OUT" || exit 1
export WINEPREFIX=/tmp/jkdiag-wine/prefix WINEDEBUG=-all WINEDLLOVERRIDES="winedbg.exe=d"
mkdir -p /tmp/jkdiag-wine

run_case() { # $1 = dll to install as jkdiag.dll ; prints checker result
    local T=/tmp/jkdiag-test
    rm -rf "$T"; mkdir -p "$T/PES13/kitserver"
    cp "$OUT/crashtest.exe" "$T/PES13/"; cp "$1" "$T/PES13/kitserver/jkdiag.dll"; cp "$OUT/kserv.dll" "$T/PES13/kitserver/"
    (cd "$T/PES13" && timeout 120 "$WINE" crashtest.exe 'Z:\tmp\jkdiag-test\PES13\kitserver\jkdiag' \
        'Z:\tmp\jkdiag-test\PES13\kitserver\kserv.dll' > "$T/stdout.txt" 2>&1)
    echo "crashtest exit=$?"
    ls -la "$T/PES13/jkdiag" 2>/dev/null | head -20
    python3 -I "$HERE/test/check_capture.py" "$T/PES13/jkdiag" "$OUT/crashtest.exe" "$OUT/kserv.dll" "$T/stdout.txt"
}
echo "===== RED: stub (must fail)"
run_case "$OUT/stub-jkdiag.dll"; red=$?
echo "===== GREEN: jkdiag.dll (must pass)"
DLL=${JKDIAG_DLL:-$OUT/jkdiag.dll}   # JKDIAG_DLL=<path> tests a delivered binary
sha256sum "$DLL"
run_case "$DLL"; green=$?
echo "red_exit=$red green_exit=$green"
[ $red -ne 0 ] && [ $green -eq 0 ]
