#!/bin/bash
# W7 decision-point guard: static test + Wine behavioural harness (WSL).
# Usage: KSERV_ORIG=<player kserv.dll 06a6fea9> bash w7test.sh [CANDIDATE.dll]
#   no CANDIDATE: rebuild from KSERV_ORIG with make_w7guard.py and compare to ../kserv.dll
set -u
H=$(cd "$(dirname "$0")" && pwd)
K=${KSERV_ORIG:?set KSERV_ORIG to the player kserv.dll 06a6fea9}
B=${ORBIS_BOOTSTRAP:-$HOME/orbis-win-host-bootstrap}
export PATH=$B/private-bin:$B/deps/rootfs/usr/bin:$PATH
W=/tmp/w7t; mkdir -p $W
i686-w64-mingw32-gcc -O1 -o $W/w7harness.exe "$H/w7harness.c" || exit 3
if [ $# -ge 1 ]; then C=$1; else
  python3 -I "$H/make_w7guard.py" "$K" $W/kserv.w7guard.dll || exit 4
  C=$W/kserv.w7guard.dll
  cmp $C "$H/../kserv.dll" && echo "rebuilt == engine/kitserver/kserv.dll"
fi
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="winedbg.exe=d"
rc=0
echo "== static"; (cd /tmp && python3 -I "$H/test_w7guard.py" "$K" "$C") || rc=1
echo "== harness"; cp "$C" $W/k.dll
(cd $W && timeout 120 "$B/wine-host/bin/wine" w7harness.exe 'Z:\tmp\w7t\k.dll') || rc=1
echo "rc=$rc"; exit $rc
