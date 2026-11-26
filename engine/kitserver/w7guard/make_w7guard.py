#!/usr/bin/env python3
"""W7 decision-point guard: binary patch of the player's kserv.dll 13.4 (sha256 06a6fea9...).
Usage: make_w7guard.py IN.dll OUT.dll

Game (PES 2013 v1.04, decrypted): the texture-package loader 011E0E70 decides at
  011E0EC6  cmp byte [model+78h],0 ; je 011E0F35
whether the model's first two textures come from the loaded BIN (then the next
entry index becomes 2, 011E0F1A) or from the game's defaults (011E0F35, index 0).
With flag 2 set, entry `index` is used later without a bounds check
(011E1013 -> call 011CA190 with NULL; 011E173B uses [model+50h]).
The guard takes the from-BIN path only if it can complete: [model+78h] != 0 and
(flag 2 clear or entry count >= 3). Otherwise it continues exactly where the
game goes for [model+78h] == 0.

kserv's unused 4th hook slot (C_READ_UNIFORM_PICK = 0 for v1.04, skipped by
HookCallPoint) is pointed at 011E0EC6 (6 bytes = call + 1 nop, numNops is
already 1). The thunk lives in the dead kservReadUniformPickCallPoint body
(hook target = func + codeShift 3 = 1000CD13). Only the v1.04 table row changes."""
import hashlib, sys

ORIG_SHA = "06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123"
def toff(va): return va - 0x10001000 + 0x400

THUNK = bytes.fromhex(
    "807a7800"      # 00 cmp byte [edx+78h],0       ; model+78h (edx = [this+5Ch], 011E0EC3)
    "740b"          # 04 je  alt                     ; original: je 011E0F35
    "f6455802"      # 06 test byte [ebp+58h],2       ; loader flag 2 (uses entry `index`)
    "740e"          # 0A jz  keep
    "83fb03"        # 0C cmp ebx,3                   ; ebx = entry count [bin+0] (011E0EAB/011E0E92)
    "7309"          # 0F jae keep                    ; entry 2 exists
    "83c404"        # 11 alt: add esp,4              ; drop return address 011E0ECB
    "68350f1e01"    # 14 push 011E0F35
    "c3"            # 19 ret                         ; -> game's [model+78h]==0 path
    "c3")           # 1A keep: ret                   ; -> 011E0ECB+1 = 011E0ECC, original fall-through
assert len(THUNK) == 27

EDITS = [
    (0x300C8, "00000000", "c60e1e01", "code[C_READ_UNIFORM_PICK] for PES2013 v1.04 = 0x011E0EC6"),
    (toff(0x1000CD13), "9c55505351525657" "8bc5" "0555010000" "50" "e818000000" "83c404"
                       "5f5e5a595b585d9d" "3e8a96e43e0000" "c3",
     THUNK.hex() + "cc" * 13, "thunk at hook target (dead uniform-pick callpoint body, 40 bytes)"),
]

d = bytearray(open(sys.argv[1], "rb").read())
assert hashlib.sha256(d).hexdigest() == ORIG_SHA, "not the expected kserv.dll"
for off, old, new, what in EDITS:
    o, n = bytes.fromhex(old), bytes.fromhex(new)
    assert len(o) == len(n), what
    assert d[off:off + len(o)] == o, "unexpected bytes: " + what
    d[off:off + len(n)] = n
open(sys.argv[2], "wb").write(d)
print("patched", sys.argv[2], hashlib.sha256(d).hexdigest())
