#!/usr/bin/env python3
"""Variant E: face-id guard in the player's fserv.dll (Kitserver 13 faceserver).
Usage: make_fserv_e.py ORIG_fserv.dll OUT.dll

Root cause (W7, 2026-11-26): the crashing loader 011E0E70 is a FACE model (request type 1,
key built at 011E02F0 from the face id; fserv hooks it at 011E0343 = C_GET_FACE_BIN).
The owner's EDIT.bin has special-face players whose face id is 0 (key 0x0C000082 = dt0c
bin 130, a 1-entry placeholder) or 2048/2049 (keys 0x0F0000CB/CC = dt0f bins 203/204,
empty in data pack 6). The face loader reads entry 2 of that BIN without a bounds check.

fservGetFaceBin(faceId) (export, 10003EA0) returns faceId unchanged for faceId < 3000.
E makes it return 0x3B4 (face 948, the game's own default face, dt0c bin 1078, 3 entries;
the same value fserv already returns for a missing GDB face) when
    faceId == 0                      (never a real face)
    0x7D3 <= faceId <= 0x8D0         (data-pack face range minus the two faces DP6 ships,
                                      2001 = dt0f bin 156 and 2002 = dt0f bin 157)
Every other id, and the whole x-face path (>= 3000), is unchanged.

Edits (file offset = VA - 0x10001000 + 0x400):
  10003EAB  72 22 -> 72 24             jb ->10003ED1 instead of ->10003ECF
  10003ED1  int3 x15 -> stub 1 (13 bytes) + 2 int3
  10003F11  int3 x15 -> stub 2 (14 bytes) + 1 int3
No relocation touches these bytes (checked by test_fserv_e.py)."""
import hashlib, sys

ORIG_SHA = "24915aa2c63d4ec075ea44db1d6ac64ff0f55504999631875633aa262df514dd"
def foff(va): return va - 0x10001000 + 0x400

STUB1 = ("85c0"            # 10003ED1 test eax,eax
         "7443"            # 10003ED3 jz   10003F18 (bad)
         "3dd3070000"      # 10003ED5 cmp  eax,7D3h
         "72f3"            # 10003EDA jb   10003ECF (pop ebp; ret: unchanged id)
         "eb33")           # 10003EDC jmp  10003F11
STUB2 = ("3dd1080000"      # 10003F11 cmp  eax,8D1h
         "73b7"            # 10003F16 jae  10003ECF (unchanged id)
         "b8b4030000"      # 10003F18 bad: mov eax,3B4h
         "5dc3")           # 10003F1D pop ebp; ret
EDITS = [(foff(0x10003EAB), "7222", "7224"),
         (foff(0x10003ED1), "cc" * 15, STUB1 + "cc" * (15 - len(STUB1) // 2)),
         (foff(0x10003F11), "cc" * 15, STUB2 + "cc" * (15 - len(STUB2) // 2))]

if __name__ == "__main__":
    src = open(sys.argv[1], "rb").read()
    assert hashlib.sha256(src).hexdigest() == ORIG_SHA, "not the player's fserv.dll"
    d = bytearray(src)
    for off, old, new in EDITS:
        o, n = bytes.fromhex(old), bytes.fromhex(new)
        assert len(o) == len(n) and d[off:off + len(o)] == o, hex(off)
        d[off:off + len(n)] = n
    open(sys.argv[2], "wb").write(d)
    print(hashlib.sha256(d).hexdigest(), sys.argv[2], sum(a != b for a, b in zip(src, d)), "bytes")
