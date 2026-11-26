#!/usr/bin/env python3
"""Offline checks for the kserv.dll InitSlotMap-synchronous patch.

Usage: test_kserv_patch.py ORIGINAL.dll CANDIDATE.dll
Exit 0 = all checks pass. Uses only the stdlib + GNU objdump (pei-i386).
"""
import hashlib, struct, subprocess, sys

ORIG_SHA = "06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123"
BASE = 0x10000000
TEXT_VA, TEXT_OFF = 0x10001000, 0x400
PATCH_VA0, PATCH_VA1 = 0x1000904D, 0x10009063          # [start, end)
CREATE_THREAD_IAT = 0x1002603C
INIT_SLOT_MAP = 0x10009380
RELOCS_IN_RANGE = {0x10009055, 0x1000905F}

fails = []
def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond:
        fails.append(msg)

def va2off(va):
    return va - TEXT_VA + TEXT_OFF

def pe_checksum(data, csum_off):
    s = 0
    n = len(data)
    buf = data + (b"\0" if n % 2 else b"")
    for i in range(0, len(buf), 2):
        if i == csum_off or i == csum_off + 2:
            continue
        s += struct.unpack_from("<H", buf, i)[0]
        s = (s & 0xFFFF) + (s >> 16)
    s = (s & 0xFFFF) + (s >> 16)
    return (s + n) & 0xFFFFFFFF

def relocs(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    rrva, rsize = struct.unpack_from("<II", d, opt + 96 + 5 * 8)
    secs = []
    so = opt + struct.unpack_from("<H", d, pe + 20)[0]
    for i in range(nsec):
        _, vs, va, rs, ro = struct.unpack_from("<8sIIII", d, so + 40 * i)
        secs.append((va, max(vs, rs), ro))
    def r2o(rva):
        for va, sz, ro in secs:
            if va <= rva < va + sz:
                return rva - va + ro
        raise ValueError(hex(rva))
    out = []
    off, end = r2o(rrva), r2o(rrva) + rsize
    while off < end:
        page, sz = struct.unpack_from("<II", d, off)
        if sz == 0:
            break
        for i in range((sz - 8) // 2):
            e = struct.unpack_from("<H", d, off + 8 + 2 * i)[0]
            if e >> 12:
                out.append((BASE + page + (e & 0xFFF), e >> 12))
        off += sz
    return out

def disasm(path, a, b):
    r = subprocess.run(["objdump", "-d", "-M", "intel", "-b", "pei-i386",
                        "--start-address=%#x" % a, "--stop-address=%#x" % b, path],
                       capture_output=True, text=True, check=True)
    import re
    return [l.strip() for l in r.stdout.splitlines() if re.match(r"^\s*1000[0-9a-f]{4}:\t", l)]

def exports_text(path):
    r = subprocess.run(["objdump", "-p", path], capture_output=True, text=True, check=True)
    t = r.stdout
    i = t.find("The Export Tables")
    j = t.find("The Function Table") if "The Function Table" in t else t.find("PE File Base Relocations")
    imp = t[t.find("The Import Tables"):t.find("The Export Tables")]
    return t[i:j], imp

orig_p, cand_p = sys.argv[1], sys.argv[2]
o = open(orig_p, "rb").read()
c = open(cand_p, "rb").read()
check(hashlib.sha256(o).hexdigest() == ORIG_SHA, "original is the player's kserv.dll (sha256 06a6fea9...)")
check(len(o) == len(c), "same file size")

pe = struct.unpack_from("<I", c, 0x3C)[0]
csum_off = pe + 24 + 64
diff = [i for i in range(min(len(o), len(c))) if o[i] != c[i]]
outside = [i for i in diff if not (va2off(PATCH_VA0) <= i < va2off(PATCH_VA1))
           and not (csum_off <= i < csum_off + 4)]
check(diff != [], "candidate differs from original")
check(outside == [], "only patch range + PE checksum changed (outside diffs: %s)" % [hex(i) for i in outside[:8]])

stored = struct.unpack_from("<I", c, csum_off)[0]
ostored = struct.unpack_from("<I", o, csum_off)[0]
if ostored == 0:
    check(stored == 0, "PE checksum left at 0 like the original (not enforced for user DLLs)")
else:
    check(stored == pe_checksum(c, csum_off), "PE checksum field valid (%#x)" % stored)

lines = disasm(cand_p, PATCH_VA0, PATCH_VA1)
body = "\n".join(lines)
print(body)
check("call   DWORD PTR ds:0x1002603c" not in body, "no CreateThread call in patched range")
check(any("push   0x10009380" in l for l in lines), "InitSlotMap address still pushed (reloc slot kept)")
check(any("call   ecx" in l for l in lines), "InitSlotMap called directly (call ecx)")
seq = [l.split("\t")[-1].strip() for l in lines]
try:
    k = seq.index("push   eax")
    check(seq[k + 1:k + 5] == ["nop    WORD PTR [eax+eax*1+0x0]", "push   0x10009380", "pop    ecx", "call   ecx"]
          or seq[k + 1:k + 4] == ["push   0x10009380", "pop    ecx", "call   ecx"],
          "teamKitInfo (eax) is the single stdcall argument to InitSlotMap")
except ValueError:
    check(False, "push eax (teamKitInfo argument) present")
# tail must still fall into the original epilogue
tail = disasm(cand_p, PATCH_VA1, PATCH_VA1 + 4)
check(tail and tail[0].split("\t")[-1].strip() == "mov    esp,ebp", "falls through to original epilogue (mov esp,ebp; pop ebp; ret)")
# instruction boundaries: no instruction may straddle PATCH_VA1
last = lines[-1]
la = int(last.split(":")[0], 16); ln = len(last.split("\t")[1].split())
check(la + ln == PATCH_VA1, "last patched instruction ends exactly at %#x" % PATCH_VA1)

ro, rc = relocs(o), relocs(c)
check(ro == rc, "base relocation table unchanged (%d entries)" % len(rc))
# every reloc in the patched range must land on the imm32 of an instruction we emitted
starts = {}
for l in lines:
    a = int(l.split(":")[0], 16); n = len(l.split("\t")[1].split())
    starts[a] = n
for r, t in rc:
    if PATCH_VA0 <= r < PATCH_VA1:
        ok = t == 3 and any(a < r and r + 4 == a + n for a, n in starts.items())
        check(ok, "reloc %#x is the trailing imm32 of a patched instruction" % r)
        val = struct.unpack_from("<I", c, va2off(r))[0]
        check(BASE <= val < BASE + 0x9C000, "reloc %#x target %#x is inside the image" % (r, val))

eo, io = exports_text(orig_p)
ec, ic = exports_text(cand_p)
check(eo == ec and len(eo) > 100, "export table identical")
check(io == ic and len(io) > 100, "import table identical")

print("\n%d failure(s)" % len(fails))
sys.exit(1 if fails else 0)
