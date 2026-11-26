#!/usr/bin/env python3
"""Offline checks for variant E (make_fserv_e.py). Usage: test_fserv_e.py ORIG.dll E.dll
1. ORIG is the player's fserv.dll; only the three edit windows differ.
2. Headers, .reloc, imports and exports are byte-identical; no relocation lands in an edit.
3. Nothing in .text branches/calls into the two stub windows except the edited jb.
4. fservGetFaceBin is executed by a small interpreter for every faceId 0..2999 in both
   DLLs: ORIG returns faceId; E returns 0x3B4 for 0 and 0x7D3..0x8D0, else faceId; ebp,
   ebx and esp are balanced. For faceId >= 3000 the code from 10003EAD on is unchanged."""
import os, struct, sys, capstone
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_fserv_e import EDITS, foff

orig, new = open(sys.argv[1], "rb").read(), open(sys.argv[2], "rb").read()
fails = []
def check(c, msg):
    if not c: fails.append(msg)

check(len(orig) == len(new), "size")
win = set()
for off, old, n in EDITS: win.update(range(off, off + len(bytes.fromhex(n))))
diff = [i for i in range(len(orig)) if orig[i] != new[i]]
check(all(i in win for i in diff), "byte outside edit windows")
check(len(diff) > 0, "no change")

pe = struct.unpack_from('<I', orig, 0x3c)[0]
ib = struct.unpack_from('<I', orig, pe + 0x34)[0]
rr, rs = struct.unpack_from('<II', orig, pe + 0x78 + 8 * 5)
def r2o(r): return r - 0x55000 + 0x20600 if r >= 0x55000 else r - 0x1000 + 0x400
relocs = []
p, end = r2o(rr), r2o(rr) + rs
while p < end:
    page, size = struct.unpack_from('<II', orig, p)
    for k in range((size - 8) // 2):
        e = struct.unpack_from('<H', orig, p + 8 + 2 * k)[0]
        if e >> 12 == 3: relocs.append(page + (e & 0xfff))
    p += size
check(orig[r2o(rr):end] == new[r2o(rr):end], ".reloc changed")
check(orig[:0x400] == new[:0x400], "headers changed")
for r in relocs:
    o = r - 0x1000 + 0x400
    if any(i in win for i in range(o, o + 4)): fails.append("reloc in edit %x" % r)

# 3. references into the stub windows
TS, TE = 0x400, 0x400 + 90553
stubs = set(range(0x10003ED1, 0x10003EE0)) | set(range(0x10003F11, 0x10003F20))
for i in range(TS, TE - 5):
    va = ib + 0x1000 + i - 0x400
    b = orig[i]
    if b in (0xE8, 0xE9):
        t = va + 5 + struct.unpack_from('<i', orig, i + 1)[0]
        if t in stubs: fails.append("rel32 ref %x -> %x" % (va, t))
    if b == 0x0F and 0x80 <= orig[i + 1] <= 0x8F:
        t = va + 6 + struct.unpack_from('<i', orig, i + 2)[0]
        if t in stubs: fails.append("jcc32 ref %x -> %x" % (va, t))
for t in stubs:
    if orig.find(struct.pack('<I', t)) != -1: fails.append("abs ref to %x" % t)

# 4. interpreter for the face-id paths below 3000
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
def run(img, face):
    st = [face, 0xDEAD0000]          # list top = [esp] = retaddr, [esp+4] = faceId
    reg = {"eax": 0x11111111, "ebx": 0x22222222, "ecx": 0x33333333, "ebp": 0x44444444}
    esp0 = len(st); flags = {}
    ip = 0x10003EA0
    for _ in range(64):
        o = foff(ip)
        ins = next(md.disasm(img[o:o + 16], ip))
        m, ops = ins.mnemonic, ins.op_str
        nxt = ip + ins.size
        if m == "push": st.append(reg[ops])
        elif m == "pop": reg[ops] = st.pop()
        elif m == "mov" and ops == "ebp, esp": reg["ebp"] = ("esp", len(st))
        elif m == "mov" and ops == "eax, dword ptr [ebp + 8]":
            base = reg["ebp"][1]; reg["eax"] = st[base - 3]   # saved ebp, ret, arg
        elif m == "mov" and ops.startswith("eax, 0x"): reg["eax"] = int(ops.split(", ")[1], 16)
        elif m in ("cmp", "test"):
            a = reg["eax"]
            b = a if m == "test" else int(ops.split(", ")[1], 16)
            r = (a - b) & 0xffffffff if m == "cmp" else a & b
            flags = {"z": r == 0, "c": (a < b) if m == "cmp" else False}
        elif m in ("jb", "jae", "je", "jmp"):
            t = int(ops, 16)
            take = {"jb": flags.get("c"), "jae": not flags.get("c"),
                    "je": flags.get("z"), "jmp": True}[m]
            if take: nxt = t
        elif m == "ret":
            ok = len(st) == esp0 and st[-1] == 0xDEAD0000 and reg["ebp"] == 0x44444444 \
                 and reg["ebx"] == 0x22222222
            return reg["eax"], ok
        else:
            raise RuntimeError("unexpected %x %s %s (face %d)" % (ip, m, ops, face))
        ip = nxt
    raise RuntimeError("no ret")
bad = 0
for f in range(3000):
    ro, oko = run(orig, f); rn, okn = run(new, f)
    want = 0x3B4 if (f == 0 or 0x7D3 <= f <= 0x8D0) else f
    if ro != f or rn != want or not (oko and okn):
        bad += 1
        if bad < 5: fails.append("face %d: orig %x new %x want %x" % (f, ro, rn, want))
print("faceId 0..2999 checked, mismatches:", bad)
check(orig[foff(0x10003EAD):foff(0x10003ED1)] == new[foff(0x10003EAD):foff(0x10003ED1)],
      "x-face path changed")
print("relocs:", len(relocs), "changed bytes:", len(diff))
print("fails=%d" % len(fails))
for f in fails: print("  FAIL", f)
sys.exit(1 if fails else 0)
