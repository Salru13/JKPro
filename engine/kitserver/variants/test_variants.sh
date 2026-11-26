#!/bin/bash
# Build + verify the bisect variants (WSL). Usage: KSERV_ORIG=<player kserv.dll> bash test_variants.sh [OUTDIR]
set -u
H=$(cd "$(dirname "$0")" && pwd)
K=${KSERV_ORIG:?set KSERV_ORIG}
OUT=${1:-$H}
B=${ORBIS_BOOTSTRAP:-$HOME/orbis-win-host-bootstrap}
export PATH=$B/private-bin:$B/deps/rootfs/usr/bin:$PATH
W=/tmp/kvar; mkdir -p $W
python3 -I "$H/make_variants.py" "$K" "$OUT" || exit 2
rc=0
python3 -I - "$K" "$OUT" <<'PY' || rc=1
import struct, sys, os, subprocess
o = open(sys.argv[1], "rb").read()
def relocs(d):
    out = []; off, end = 0x44C00, 0x44C00 + 0x30A2
    while off < end:
        page, sz = struct.unpack_from("<II", d, off)
        if sz == 0: break
        out += [page + (e & 0xFFF) for e in struct.unpack_from("<%dH" % ((sz - 8) // 2), d, off + 8) if e >> 12]
        off += sz
    return out
def exp(p):
    t = subprocess.run(["objdump", "-p", p], capture_output=True, text=True).stdout
    return t[t.find("The Import Tables"):t.find("PE File Base Relocations")]
allowed = {"A": [(0x3015C, 0x30160)], "B": [(0x300B4, 0x300C0)],
           "C": [(0x300C8, 0x300CC), (0xC113, 0xC13B)]}
allowed["AC"] = allowed["A"] + allowed["C"]
allowed["D"] = [(0x300C8, 0x300CC), (0x7B8D, 0x7B8F), (0xC113, 0xC13B)]
fails = 0
ro = relocs(o); eo = exp(sys.argv[1])
for f in sorted(os.listdir(sys.argv[2])):
    if not f.startswith("kserv-") or not f.endswith(".dll"): continue
    tag = f.split("-")[1]
    c = open(os.path.join(sys.argv[2], f), "rb").read()
    diff = [i for i in range(len(o)) if o[i] != c[i]]
    bad = [hex(i) for i in diff if not any(a <= i < b for a, b in allowed[tag])]
    rel_hit = [hex(r) for r in ro if any(a <= (r - 0x30000 + 0x2F000 if r >= 0x30000 else r - 0x1000 + 0x400) < b for a, b in allowed[tag])]
    ok = len(c) == len(o) and diff and not bad and relocs(c) == ro and exp(os.path.join(sys.argv[2], f)) == eo and not rel_hit
    print("%s %s: %d bytes changed, outside=%s, relocs/imports/exports unchanged=%s, relocs on edits=%s"
          % ("PASS" if ok else "FAIL", f, len(diff), bad, relocs(c) == ro and exp(os.path.join(sys.argv[2], f)) == eo, rel_hit))
    fails += not ok
    row = struct.unpack_from("<9I", c, 0x2FFF8 + 36 * 5); drow = struct.unpack_from("<6I", c, 0x300D0 + 24 * 5)
    print("     v1.04 code row %s" % [hex(x) for x in row]); print("     v1.04 data row %s" % [hex(x) for x in drow])
sys.exit(1 if fails else 0)
PY
i686-w64-mingw32-gcc -O1 -o $W/w7harness.exe "$H/../w7guard/w7harness.c" || exit 3
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="winedbg.exe=d"
for v in kserv-C-guard.dll kserv-AC-noeuro-guard.dll; do
  cp "$OUT/$v" $W/k.dll
  echo "== harness $v"; (cd $W && timeout 120 "$B/wine-host/bin/wine" w7harness.exe 'Z:\tmp\kvar\k.dll' | grep -E "PASS|FAIL|matrix") || rc=1
done
sha256sum "$OUT"/kserv-*.dll
echo "rc=$rc"; exit $rc
