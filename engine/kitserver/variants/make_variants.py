#!/usr/bin/env python3
"""Bisect variants of the player's kserv.dll 13.4 (sha256 06a6fea9...).
Usage: make_variants.py ORIG.dll OUTDIR

A  euro-kit rewrite off   : data row v1.04 EURO_TEAM_KIT_INFO_PTR 0x019E8288 -> 0
                             (InitEuroKitAttributes reads/writes the game's vector only
                              behind `if (p && *p)`: 1000986F/10009874 and 1000A042/1000A048)
B  num-slot hooks off      : code row v1.04 C_READ_NUM_SLOTS1..3 -> 0
                             (HookCallPoint skips addr 0; x-slot GDB kits then fall back)
C  decision-point guard    : ../w7guard/make_w7guard.py edits (code[7] -> 011E0EC6 + thunk)
AC A + C
D  missing-entry default   : code[7] -> 011E1013 (the flag-2 bounds check of loader 011E0E70),
                             numNops 1 -> 3 (8 bytes 011E1013..011E101A), dead callpoint body ->
                             37-byte thunk. Entry present: 011E1046 (original). Entry missing
                             (original always calls 011CA190(NULL)): if [model+50h]==0 run the
                             game's own flag-0x20 blank-texture block 011E0FD2 once, then the
                             game's own "no flag 2" continuation 011E11C2. See test_variant_d.py.
All edits are data dwords or the dead callpoint body; no relocation touches them."""
import hashlib, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "w7guard"))

ORIG_SHA = "06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123"
def toff(va): return va - 0x10001000 + 0x400
A = [(0x3015C, "88829e01", "00000000")]
B = [(0x300B4, "3e6a1d01f0741d01c0ca2b01", "000000000000000000000000")]
THUNK = ("807a7800740bf6455802740e83fb03730983c40468350f1e01c3c3")
C = [(0x300C8, "00000000", "c60e1e01"),
     (toff(0x1000CD13), "9c55505351525657" "8bc5" "0555010000" "50" "e818000000" "83c404"
                        "5f5e5a595b585d9d" "3e8a96e43e0000" "c3", THUNK + "cc" * 13)]
ORIG_CALLPOINT = ("9c55505351525657" "8bc5" "0555010000" "50" "e818000000" "83c404"
                  "5f5e5a595b585d9d" "3e8a96e43e0000" "c3")
THUNK_D = ("3bfb" "7219"                 # cmp edi,ebx ; jb ok
           "8b455c" "83785000" "7508"    # mov eax,[ebp+5Ch] ; cmp [eax+50h],0 ; jne skip
           "c70424d20f1e01" "c3"         # [esp]=011E0FD2 ; ret  (flag-0x20 blank texture)
           "c70424c2111e01" "c3"         # skip: [esp]=011E11C2 ; ret (no-flag-2 path)
           "c7042446101e01" "c3")        # ok:   [esp]=011E1046 ; ret (original entry read)
D = [(0x300C8, "00000000", "13101e01"),
     (toff(0x1000878D), "6a01", "6a03"),
     (toff(0x1000CD13), ORIG_CALLPOINT, THUNK_D + "cc" * 3)]
VARIANTS = {"kserv-A-noeuro.dll": A, "kserv-B-nonumslots.dll": B,
            "kserv-C-guard.dll": C, "kserv-AC-noeuro-guard.dll": A + C,
            "kserv-D-missing-entry.dll": D}

src = open(sys.argv[1], "rb").read()
assert hashlib.sha256(src).hexdigest() == ORIG_SHA
for name, edits in VARIANTS.items():
    d = bytearray(src)
    for off, old, new in edits:
        o, n = bytes.fromhex(old), bytes.fromhex(new)
        assert len(o) == len(n) and d[off:off + len(o)] == o, (name, hex(off))
        d[off:off + len(n)] = n
    open(os.path.join(sys.argv[2], name), "wb").write(d)
    print(hashlib.sha256(d).hexdigest(), name, sum(a != b for a, b in zip(src, d)), "bytes")
