#!/usr/bin/env python3
"""Static checks for the W7 decision-point guard. Usage: test_w7guard.py ORIG.dll CAND.dll"""
import hashlib, re, struct, subprocess, sys

ORIG_SHA = "06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123"
BASE = 0x10000000
def toff(va): return va - 0x10001000 + 0x400
RANGES = [(0x300C8, 0x300CC), (toff(0x1000CD13), toff(0x1000CD3B))]
fails = []
def check(c, m):
    print(("PASS " if c else "FAIL ") + m)
    if not c: fails.append(m)

def relocs(d):
    out = []; off, end = 0x44C00, 0x44C00 + 0x30A2
    while off < end:
        page, sz = struct.unpack_from("<II", d, off)
        if sz == 0: break
        for i in range((sz - 8) // 2):
            e = struct.unpack_from("<H", d, off + 8 + 2 * i)[0]
            if e >> 12: out.append(BASE + page + (e & 0xFFF))
        off += sz
    return out

def dis(path, a, b):
    r = subprocess.run(["objdump", "-d", "-M", "intel", "-b", "pei-i386", "--start-address=%#x" % a,
                        "--stop-address=%#x" % b, path], capture_output=True, text=True, check=True)
    return [l.split("\t")[-1].strip() for l in r.stdout.splitlines()
            if re.match(r"^\s*1000[0-9a-f]{4}:\t", l) and len(l.split("\t")) >= 3]

def pe_tables(path):
    t = subprocess.run(["objdump", "-p", path], capture_output=True, text=True, check=True).stdout
    return t[t.find("The Import Tables"):t.find("The Export Tables")], t[t.find("The Export Tables"):t.find("PE File Base Relocations")]

o = open(sys.argv[1], "rb").read(); c = open(sys.argv[2], "rb").read()
check(hashlib.sha256(o).hexdigest() == ORIG_SHA, "original is the player's kserv.dll")
check(len(o) == len(c), "same size")
diff = [i for i in range(len(o)) if o[i] != c[i]]
outside = [hex(i) for i in diff if not any(a <= i < b for a, b in RANGES)]
check(diff and not outside, "only the two patch ranges differ (outside: %s)" % outside[:5])
for row in range(6):
    ro = struct.unpack_from("<9I", o, 0x2FFF8 + 36 * row); rc = struct.unpack_from("<9I", c, 0x2FFF8 + 36 * row)
    if row == 5:
        check(rc[:7] == ro[:7] and rc[8] == ro[8] and ro[7] == 0 and rc[7] == 0x011E0EC6,
              "v1.04 code[C_READ_UNIFORM_PICK] 0 -> 0x011E0EC6, rest of row unchanged")
    else:
        check(rc == ro, "address row %d unchanged" % row)
init = dis(sys.argv[2], 0x10008783, 0x10008799)
check(init == ["mov    eax,ds:0x1007645c", "add    esp,0x14", "push   0x0", "push   0x1", "push   0x3",
               "push   0x1000cd10", "push   eax", "call   esi"],
      "4th HookCallPoint(code[7], 0x1000cd10, codeShift 3, numNops 1) unchanged: %s" % init)
th = dis(sys.argv[2], 0x1000CD13, 0x1000CD2E)
want = ["cmp    BYTE PTR [edx+0x78],0x0", "je     0x1000cd24", "test   BYTE PTR [ebp+0x58],0x2",
        "je     0x1000cd2d", "cmp    ebx,0x3", "jae    0x1000cd2d", "add    esp,0x4", "push   0x11e0f35",
        "ret", "ret"]
check(th == want, "thunk disassembles as designed: %s" % th)
check(all(b == 0xCC for b in c[toff(0x1000CD2E):toff(0x1000CD3B)]), "rest of the old body is int3")
ro_, rc_ = relocs(o), relocs(c)
check(ro_ == rc_, "relocation table unchanged (%d entries)" % len(rc_))
check(not [r for r in rc_ if 0x1000CD10 <= r < 0x1000CD40], "no relocation inside the thunk area")
check(not [r for r in rc_ if 0x100310C0 <= r < 0x100310D0], "no relocation on the patched table dword")
io, eo = pe_tables(sys.argv[1]); ic, ec = pe_tables(sys.argv[2])
check(io == ic and len(io) > 100, "imports identical")
check(eo == ec and len(eo) > 100, "exports identical")
print("\n%d failure(s)" % len(fails)); sys.exit(1 if fails else 0)
