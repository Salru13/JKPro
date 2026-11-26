#!/bin/bash
# JKPro client build: ~/nfsmw-bench/native/scripts/build-client-native6enc17i.sh (recipe 68cd73e0)
# with N=~/jkpro-engine-client and the source tree given as $1 (a copy; read only), output $2.
#   jk_build.sh <tree-name> <out-name>    e.g. release-28c101ff-src control / src6-17i fix
set -euo pipefail
N=$HOME/jkpro-engine-client
OLD_BASE=${JK_OLD_BASE:-$HOME/orbis-win-host-bootstrap}   # the build path baked into the tree
BASE=$HOME/nfsmw-bench/c988-nfs/base          # symlinks only, read
R=$N/${1:?tree}
MODE=${2:?out}
T=$N/work-$MODE; OUT=$N/out/$MODE
rm -rf "$T"; mkdir -p "$T" "$OUT"
cp -aL "$R/." "$T/"
NW=$T/native-wine; B=$NW/native288-claude/frame260build.py
sed -i "s|^BASE = Path('$OLD_BASE')|BASE = Path('$BASE')|" "$B"; grep -q "^BASE = Path('$BASE')" "$B"
sed -i "s|--out-implib=$OLD_BASE/native286-dxvk259/candidate/d3d9.dll.a|--out-implib=$OUT/d3d9.dll.a|" "$NW/native286-dxvk259-build.json"
sed -i "s|$OLD_BASE|$BASE|g" "$NW/native286-dxvk259-build.json"
grep -q -- "--out-implib=$OUT/d3d9.dll.a" "$NW/native286-dxvk259-build.json"
FRAME260_OUT=$OUT PYTHONDONTWRITEBYTECODE=1 nice -n 10 python3 "$B" > "$N/build-$MODE.log" 2>&1 || { tail -30 "$N/build-$MODE.log"; exit 1; }
tail -4 "$N/build-$MODE.log"
sha256sum "$OUT/d3d9.c988.dll"
rm -f "$OUT"/*.obj; rm -rf "$T"
