#!/usr/bin/env python3
"""Apply the InitSlotMap-synchronous patch to the player's kserv.dll (13.4).
Usage: make_patch.py IN.dll OUT.dll"""
import hashlib, sys

ORIG_SHA = "06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123"
OFF = 0x1000904D - 0x10001000 + 0x400   # file offset 0x844D
OLD = bytes.fromhex("8d4dfc 51 6a00 50 6880930010 6a00 6a00 ff153c600210".replace(" ", ""))
NEW = bytes.fromhex(
    "50"            # push eax            ; teamKitInfo (already computed above)
    "660f1f440000"  # nop (6 bytes)
    "6880930010"    # push 0x10009380     ; &InitSlotMap (reloc @+1 kept)
    "59"            # pop ecx
    "ffd1"          # call ecx            ; InitSlotMap(teamKitInfo), stdcall ret 4
    "6690"          # nop (2 bytes)
    "b93c600210"    # mov ecx,0x1002603c  ; filler keeping reloc @0x1000905f valid
)
assert len(OLD) == len(NEW) == 0x16, (len(OLD), len(NEW))
d = bytearray(open(sys.argv[1], "rb").read())
assert hashlib.sha256(d).hexdigest() == ORIG_SHA, "not the expected kserv.dll"
assert d[OFF:OFF + len(OLD)] == OLD, "unexpected bytes at patch site"
d[OFF:OFF + len(NEW)] = NEW
open(sys.argv[2], "wb").write(d)
print("patched", sys.argv[2], hashlib.sha256(d).hexdigest())
