#!/usr/bin/env python3
"""Offline checks for kserv-D-missing-entry.dll (Windows or Linux, needs pefile + capstone).

Usage: python -I test_variant_d.py ORIG_kserv.dll kserv-D-missing-entry.dll \
           DECRYPTED_pes2013-image.bin [jkdiag-capture-dir ...]

1. PE: only the 3 edit windows differ; relocations, imports, exports identical;
   no relocation inside an edit.
2. Hook wiring: v1.04 code[7] = 0x011E1013, HookCallPoint(code[7], 0x1000CD10, 3, 3, 0)
   -> CALL 0x1000CD13 + 3 NOPs over 011E1013..011E101A (8 bytes).
3. Game image (decrypted, base 0x400000): the 8 replaced bytes are the expected
   `cmp edi,ebx; jb 011E1046; xor eax,eax; jmp 011E105A`, 011E101B (the 0x20 block's
   jne target) is outside the window, the 3 thunk targets start with the expected
   instructions, and no rel8/rel32/jcc/absolute reference anywhere in .text points into
   011E1014..011E101A.
4. Thunk: the patched bytes are executed by a small interpreter (only the 7 opcodes the
   thunk uses; anything else fails) over a matrix of index/count/[model+50h]; ESP is
   balanced, only EAX/EFLAGS change, and the target equals the original game's target
   whenever the original does not fault.
5. Replay: each capture's loader/model/BIN state is run through a path model of
   011E0E70 (written from the disassembly, addresses cited) with the original code and
   with D. Every dereference that faults in the original is an assertion in the model.
"""
import hashlib, os, struct, sys
import capstone, pefile
from capstone import x86

ORIG_SHA = "06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123"
D_SHA = "9cdd7ff302413d3ee21789e3dfcbd970b8520a957baa704261a0156152c7f95d"
ALLOWED = [(0x300C8, 0x300CC), (0x7B8D, 0x7B8F), (0xC113, 0xC13B)]
GAME_BASE = 0x400000
HOOK, HOOK_END = 0x011E1013, 0x011E101B          # 8 bytes replaced
T_BLANK, T_SKIP, T_OK = 0x011E0FD2, 0x011E11C2, 0x011E1046
fails = 0


def check(cond, msg):
    global fails
    print(("PASS " if cond else "FAIL ") + msg)
    fails += not cond


def relocs(pe):
    out = []
    for b in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", []):
        out += [e.rva for e in b.entries if e.type]
    return sorted(out)


def imports_exports(pe):
    imp = [(e.dll, [(i.name, i.address) for i in e.imports]) for e in pe.DIRECTORY_ENTRY_IMPORT]
    exp = [(s.name, s.address) for s in pe.DIRECTORY_ENTRY_EXPORT.symbols]
    return imp, exp


# ---------------------------------------------------------------- 1. PE
orig_b, d_b = open(sys.argv[1], "rb").read(), open(sys.argv[2], "rb").read()
check(hashlib.sha256(orig_b).hexdigest() == ORIG_SHA, "input is the player's kserv.dll 13.4")
check(hashlib.sha256(d_b).hexdigest() == D_SHA, "D sha256 %s" % hashlib.sha256(d_b).hexdigest())
check(len(orig_b) == len(d_b), "same size")
diff = [i for i in range(len(orig_b)) if orig_b[i] != d_b[i]]
check(diff and all(any(a <= i < b for a, b in ALLOWED) for i in diff),
      "%d changed bytes, all inside %s" % (len(diff), [(hex(a), hex(b)) for a, b in ALLOWED]))
po, pd = pefile.PE(data=orig_b), pefile.PE(data=d_b)
check(relocs(po) == relocs(pd), "relocations identical (%d)" % len(relocs(po)))
check(imports_exports(po) == imports_exports(pd), "imports and exports identical")
hit = [hex(r) for r in relocs(po) for k in range(4)
       if any(a <= po.get_offset_from_rva(r) + k < b for a, b in ALLOWED)]
check(not hit, "no relocation touches an edit %s" % hit)

# ---------------------------------------------------------------- 2. hook wiring
img = pd.get_memory_mapped_image()
KB = pd.OPTIONAL_HEADER.ImageBase
row = struct.unpack_from("<9I", d_b, 0x2FFF8 + 36 * 5)
check(row[7] == HOOK and row[8] == struct.unpack_from("<9I", orig_b, 0x2FFF8 + 36 * 5)[8],
      "v1.04 code row: code[7]=%#x, others unchanged" % row[7])
call = img[0x878B:0x8796]
check(call == bytes.fromhex("6a006a036a036810cd0010"), "HookCallPoint(code[7], 0x1000CD10, 3, 3, 0)")
thunk = img[0xCD13:0xCD3B]
check(thunk[37:] == b"\xcc" * 3, "thunk is 37 bytes + 3 x CC inside the dead callpoint body")
check(img[0xCD3B:0xCD40] == po.get_memory_mapped_image()[0xCD3B:0xCD40], "1000CD3B.. untouched")

# ---------------------------------------------------------------- 3. game image
g = open(sys.argv[3], "rb").read()
gb = lambda va, n: g[va - GAME_BASE: va - GAME_BASE + n]
check(gb(HOOK, 8) == bytes.fromhex("3bfb722f33c0eb3f"),
      "011E1013: cmp edi,ebx; jb 011E1046; xor eax,eax; jmp 011E105A (8 bytes)")
check(gb(0x011E0FE2, 2) == bytes.fromhex("7537") and 0x011E0FE4 + 0x37 == HOOK_END,
      "011E0FE2 jne 011E101B lands right after the window")
check(gb(T_BLANK, 4) == bytes.fromhex("6a106a70"), "011E0FD2: push 10h; push 70h (flag-0x20 block)")
check(gb(T_SKIP, 4) == bytes.fromhex("f6455808"), "011E11C2: test byte [ebp+58h],8 (no-flag-2 path)")
check(gb(T_OK, 4) == bytes.fromhex("8b4c2444"), "011E1046: mov ecx,[esp+44h] (entry read)")
check(gb(0x011E100D, 6) == bytes.fromhex("0f84af010000"), "011E100D je 011E11C2 (game's own skip)")
lo, hi, refs = HOOK + 1, HOOK_END - 1, []
ts, te = 0x401000 - GAME_BASE, 0x13F7D78 - GAME_BASE
for i in range(ts, te - 6):
    op = g[i]
    if op in (0xE8, 0xE9):
        t = GAME_BASE + i + 5 + struct.unpack_from("<i", g, i + 1)[0]
        if lo <= t <= hi: refs.append(hex(GAME_BASE + i))
    elif 0x70 <= op <= 0x7F or op == 0xEB:
        t = GAME_BASE + i + 2 + struct.unpack_from("<b", g, i + 1)[0]
        if lo <= t <= hi: refs.append(hex(GAME_BASE + i))
    elif op == 0x0F and 0x80 <= g[i + 1] <= 0x8F:
        t = GAME_BASE + i + 6 + struct.unpack_from("<i", g, i + 2)[0]
        if lo <= t <= hi: refs.append(hex(GAME_BASE + i))
for v in range(lo, hi + 1):
    if g.find(struct.pack("<I", v)) != -1: refs.append("abs %#x" % v)
check(not refs, "nothing in .text targets 011E1014..011E101A (byte-wise scan) %s" % refs[:5])

# ---------------------------------------------------------------- 4. thunk interpreter
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True
REGS = {x86.X86_REG_EAX: "eax", x86.X86_REG_EBX: "ebx", x86.X86_REG_ECX: "ecx",
        x86.X86_REG_EDX: "edx", x86.X86_REG_ESI: "esi", x86.X86_REG_EDI: "edi",
        x86.X86_REG_EBP: "ebp", x86.X86_REG_ESP: "esp"}


def run_thunk(regs, mem):
    """Execute the patched thunk as the CALL at 011E1013 enters it. Returns (target, regs)."""
    r = dict(regs)
    r["esp"] -= 4
    mem[r["esp"]] = HOOK + 5                     # return address pushed by the CALL
    esp0 = r["esp"]
    code = bytes(thunk)
    pc, cf, zf, steps = 0x1000CD13, None, None, 0
    insns = {i.address: i for i in md.disasm(code, 0x1000CD13)}
    while True:
        steps += 1
        assert steps < 20, "loop"
        i = insns[pc]
        nxt = pc + i.size
        ops = i.operands
        if i.mnemonic == "cmp" and ops[0].type == x86.X86_OP_REG and ops[1].type == x86.X86_OP_REG:
            a, b = r[REGS[ops[0].reg]], r[REGS[ops[1].reg]]
            cf, zf = a < b, a == b
        elif i.mnemonic == "cmp" and ops[0].type == x86.X86_OP_MEM:
            a = mem[r[REGS[ops[0].mem.base]] + ops[0].mem.disp]
            cf, zf = a < ops[1].imm, a == ops[1].imm
        elif i.mnemonic == "jb":
            nxt = ops[0].imm if cf else nxt
        elif i.mnemonic == "jne":
            nxt = ops[0].imm if not zf else nxt
        elif i.mnemonic == "mov" and ops[0].type == x86.X86_OP_REG:
            r[REGS[ops[0].reg]] = mem[r[REGS[ops[1].mem.base]] + ops[1].mem.disp]
        elif i.mnemonic == "mov" and ops[0].type == x86.X86_OP_MEM:
            assert ops[0].mem.base == x86.X86_REG_ESP and ops[0].mem.disp == 0
            mem[r["esp"]] = ops[1].imm & 0xFFFFFFFF
        elif i.mnemonic == "ret" and not ops:
            assert r["esp"] == esp0, "stack not balanced"
            target = mem[r["esp"]]
            r["esp"] += 4
            return target, r
        else:
            raise AssertionError("unexpected instruction %s %s" % (i.mnemonic, i.op_str))
        pc = nxt


bad = 0
for idx in (0, 1, 2, 3, 5):
    for cnt in (0, 1, 2, 3, 4, 0x81460001):
        for m50 in (0, 0x20BE35D0):
            regs = dict(eax=0x11111111, ebx=cnt, ecx=0x33333333, edx=0x44444444,
                        esi=0x55555555, edi=idx, ebp=0x20B99030, esp=0x0D63FE80)
            mem = {0x20B99030 + 0x5C: 0x20772D20, 0x20772D20 + 0x50: m50}
            tgt, out = run_thunk(regs, mem)
            orig_tgt = T_OK if idx < cnt else None          # None = original calls 011CA190(NULL)
            want = orig_tgt if orig_tgt else (T_SKIP if m50 else T_BLANK)
            same = all(out[k] == regs[k] for k in regs if k != "eax")
            if tgt != want or not same:
                bad += 1
                print("  mismatch idx=%d cnt=%#x m50=%#x -> %#x" % (idx, cnt, m50, tgt))
check(bad == 0, "thunk matrix 60 cases: target = original when entry present, "
      "011E0FD2/011E11C2 when missing; ESP balanced; only EAX changes")


# ---------------------------------------------------------------- 5. capture replay
class Fault(Exception):
    pass


def path_011E0E70(st, patched):
    """Path model of loader method 011E0E70 for one call. st: flags, type78, count, bin,
    m50, entries (list of entry offsets). Returns the list of steps; raises Fault."""
    steps = []
    f, n = st["flags"], (st["count"] if st["bin"] else 0)  # 011E0E8E: BIN NULL -> count 0
    idx, m50 = 0, st["m50"]
    if f & 1:                                             # 011E0EB9
        if st["type78"]:                                  # 011E0EC6 cmp byte [model+78h],0
            # entry 0 read only if count>0 (011E0ECC ja), entry 1 only if count>1 (011E0EF9 ja)
            # 01151430 returns 0 for a NULL entry (01151454 test edi,edi)
            steps.append("+48/+4C from entries 0/1 (count %d)" % n)
            idx = 2                                       # 011E0F1A
        else:
            steps.append("011E0F35 path")
    if f & 0x20:
        m50 = "blank"; steps.append("0x20 blank -> +50")
    if f & 2:                                             # 011E1009
        while True:
            if not patched:
                if idx < n and st["entries"][idx]:
                    target = T_OK
                else:
                    raise Fault("011CA197 [NULL+7] via 011E105A (idx %d, count %d)" % (idx, n))
            else:
                regs = dict(eax=0, ebx=n, ecx=0, edx=0, esi=0, edi=idx, ebp=0x1000, esp=0x2000)
                mem = {0x1000 + 0x5C: 0x3000, 0x3000 + 0x50: 0 if m50 in (0, None) else 1}
                target, _ = run_thunk(regs, mem)
            if target == T_OK:
                if not st["entries"][idx]:
                    raise Fault("011CA197 entry offset 0")
                steps.append("flag2 entry %d" % idx)
                m50 = m50 or "tex"; idx += 1                # 011E1067..011E11BD
                break
            if target == T_BLANK:                         # 011E0FD2..011E1005 then 011E1009 again
                m50 = "blank"; steps.append("D: 0x20 block -> +50 blank"); continue
            assert target == T_SKIP
            steps.append("D: 011E11C2 (idx %d)" % idx); break
    if f & 8:
        raise AssertionError("flag 8 not modelled (0x47 loaders do not set it)")
    if f & 0x40:                                          # 011E15BA cmp edi,ebx; jge (signed)
        i = idx
        while i < n:
            if not st["entries"][i]:
                raise Fault("011CA197 in 0x40 loop")
            i += 1
        steps.append("0x40 loop %d..%d" % (idx, max(idx, n)))
    if f & 4:                                             # 011E173B mov eax,[model+50h]
        if not m50:
            raise Fault("011E1743 [NULL+10h]")
        steps.append("flag4 on +50 (%s)" % m50)
    steps.append("tail 011E17B6 free BIN" if st["bin"] else "tail 011E17B6 (no BIN)")
    return steps


def dw(blob, off):
    return struct.unpack_from("<I", blob, off)[0]


def capture_state(cdir):
    txt = open(os.path.join(cdir, "crash.txt")).read()
    reg = dict(x.split("=") for x in txt.split("\n")[4].split())
    ebp = int(reg["EBP"], 16)
    hp = lambda a: os.path.join(cdir, "heap-%08X.bin" % a)
    st = {"name": os.path.basename(cdir.rstrip("/\\"))}
    if os.path.exists(hp(ebp)):
        L = open(hp(ebp), "rb").read()
        st["flags"], model, st["bin"] = dw(L, 0x58) & 0xFFFF, dw(L, 0x5C), dw(L, 0x34)
        st["key"] = dw(L, 0x2C)
        if os.path.exists(hp(model)):
            M = open(hp(model), "rb").read()
            st["type78"], st["m50"] = M[0x78], dw(M, 0x50)
        else:
            st["type78"], st["m50"] = 1, None
    else:                                                 # capture 1: registers only
        line = [l for l in txt.split("\n") if l.startswith("  %08X:" % (ebp + 0x20))][0]
        b = bytes.fromhex("".join(line.split(":")[1].split()))
        st["key"], st["flags"], st["type78"], st["m50"] = dw(b, 0xC), 0x47, 1, None
        line = [l for l in txt.split("\n") if l.startswith("  %08X:" % (ebp + 0x30))][0]
        st["bin"] = dw(bytes.fromhex("".join(line.split(":")[1].split())), 4)
    st["count"], st["entries"] = int(reg["EBX"], 16), [0x20] * 8
    if st["bin"] and os.path.exists(hp(st["bin"])):
        Bn = open(hp(st["bin"]), "rb").read()
        st["count"] = dw(Bn, 0)
        tab = dw(Bn, 4)
        st["entries"] = [dw(Bn, tab + 12 * i) for i in range(st["count"])] + [0] * 8
    if not st["bin"]:
        st["count"] = 0
    return st


rows = []
for cdir in sys.argv[4:]:
    st = capture_state(cdir)
    for m50 in ([st["m50"]] if st["m50"] is not None else [0, 1]):
        s = dict(st, m50=m50)
        try:
            path_011E0E70(s, False); o = "no fault"
        except Fault as e:
            o = "FAULT " + str(e)
        try:
            p = path_011E0E70(s, True); d = "safe: " + "; ".join(p)
        except Fault as e:
            d = "FAULT " + str(e)
        rows.append((s["name"], "%08X" % s["key"], "%#x" % s["flags"], s["type78"], s["count"],
                     "%08X" % s["bin"], m50, o, d))
        check(o.startswith("FAULT") and d.startswith("safe"),
              "%s key=%08X flags=%#x count=%d bin=%08X m50=%s: original %s | D %s"
              % (s["name"], s["key"], s["flags"], s["count"], s["bin"], m50, o, d))
for n, bins in ((0, 0), (0, 1), (1, 1), (2, 1), (3, 1)):
    s = dict(flags=0x47, type78=1, count=n, bin=bins, m50=0, entries=[0x20] * n + [0] * 8)
    try:
        path_011E0E70(s, False); o = "no fault"
    except Fault:
        o = "fault"
    p = path_011E0E70(s, True)
    want_same = (o == "no fault")
    check(True, "synthetic bin=%s count=%d: original %s | D %s" % ("set" if bins else "NULL", n, o, "; ".join(p)))
    if want_same:
        check(p == path_011E0E70(s, False), "count %d: D path identical to original" % n)

print("fails=%d" % fails)
sys.exit(1 if fails else 0)
