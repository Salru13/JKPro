# W7 bisect variants and decision-point guard (2026-11-25, current)

All variants are binary patches of the player's kserv.dll 13.4.0.0, sha256
`06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123`. They are
built by `variants/make_variants.py` and checked by
`variants/test_variants.sh`. In every variant, relocations, imports and
exports are unchanged, and no relocation touches an edit.

| File | sha256 | Change |
|---|---|---|
| `variants/kserv-A-noeuro.dll` | `b2649adf24ee7e9cba73d4bad701f8eb714c104a8e611d6a9af084786ff7f183` | data 0x3015C `0x019E8288` → 0 (v1.04 EURO_TEAM_KIT_INFO_PTR) |
| `variants/kserv-B-nonumslots.dll` | `3fbd34701480ebf32532b4f524cf8c609ef846eb8d5d3ef29391823fb8acac6d` | data 0x300B4–0x300BF, the 3 num-slot hook addresses → 0 |
| `variants/kserv-C-guard.dll` (= `kserv.dll`) | `43159a566579bed6590610f9e25451ed697e6bf99130134bb671ad6efdf7f8a0` | decision-point guard (below) |
| `variants/kserv-AC-noeuro-guard.dll` | `820abb3db2720665ab6b506e968b45ed0b98be45d891392dbc7e4405a62c1443` | A + C |

**Variant A.**
- Observed: the 2013 build only reads or writes the game's euro vector
  behind `p && *p`: `1000986F mov eax,[dta5]; test; je` → `mov eax,[eax];
  test; je`, and the same at `1000A042..1000A050`. `dta[5]` (0x10076478) has
  no other reader.
- With 0, the original euro list is not enumerated and the game's
  begin/end/end_again are never replaced or freed.
- Effect: GDB euro-kits are not applied. Normal GDB kits and x-slots are
  unaffected.

**Variant B.**
- Observed: the player's kload `HookCallPoint` starts with
  `test esi(addr),esi; je skip`.
- Effect: the game keeps its own num-slot code. A GDB team relinked to
  x-slot 0x1000 then gets the game's fallback kit, so Arsenal will look
  wrong in B.

**Variant C, the decision-point guard.**
- The game (decrypted v1.04) decides at `011E0EC6 cmp byte [model+78h],0;
  je 011E0F35` whether the model's first textures come from the BIN. If they
  do, `011E0F1A` sets the entry index to 2.
- With loader flag 2, that entry is then used without a bounds check:
  - `011E1013` passes NULL to `011CA190`; that was the crash in captures 1
    and 2.
  - `011E173B` uses `[model+50h]`; that was the crash in capture 3, under
    the earlier call-site guard.
- The guard hooks the 6-byte decision (kserv's unused 4th hook slot,
  `code[7]` = 0x011E0EC6, numNops 1, thunk at `1000CD13`). It keeps the
  from-BIN path only if `[model+78h]` ≠ 0 and (flag 2 clear, or entry
  count ≥ 3). Otherwise it continues at `011E0F35`, the game's own
  `[model+78h]`=0 path.

Why the `011E0F35` path is consistent (observed in the decrypted image):
- It does not touch BIN entries and leaves the index at 0.
- Flag 2 then uses entry 0, guarded by `cmp edi,ebx` (count ≥ 1).
- That sets `[model+50h]`, so flag 4 (`011E173B`) is safe.
- The flag-40h loop is bounds-checked (`011E15BA`, and `011E167C` inside
  it).
- The function tail `011E17B6..011E17D3` does not use the index.

Ground truth from the captures:
- Captures 2 and 3 (heap dumps): flags 0x47, `[model+78h]`=1, count 1,
  entry 0 = {0x20, 0x55600} → the guard takes `011E0F35`.
- Capture 1: count 1 (EBX) and index 2 show the same decision, inferred
  from the execution.
- Harness: 120-case matrix, 0 failures. Every register and ESP are
  preserved, and the outcome equals the original game decision except
  where the original always faults.
- Nothing jumps into `011E0EC7..011E0ECB` (full `.text` scan).

**Console result, 2026-11-26: variant A REJECTED.**
- Loaded kserv image had the A patch (`.data` RVA 0x3115C = 0, from `kserv-image.bin`).
- It still crashed at `011CA197` (null read of `[0+7]`), the same as capture 1.
- Capture A heap: loader `20B6A4B0` key 0x0C000082, flags 0x47, model `20BB10C0` `[+78h]`=1, bin `2101E970` count 1, entry 0 = {0x20, 0x55600}.
- So the euro-list rewrite is not the cause. The state is identical to captures 2 and 3, and C's condition covers it.
- Next: C alone on the console.

**Console result, 2026-11-26: variant C REJECTED.**
- It crashed at `011CA197` via `011E105A` with eax=0, edi (index)=0 and ebx (count)=0.
- Loader `20B99030`: key 0x0F0000CB (dt0f), flags 0x47, `[+34h]` BIN pointer = **NULL**, `[+38h]` = 0x81460001.
- Model `20772D20`: `[+78h]`=1, `[+50h]`=0.
- With no BIN, the `011E0F35` path (count ≥ 1 assumed) also faults. C only covered count 1..2.
- Four fixes have now failed (sync InitSlotMap, call-site guard, A, C). Root-cause work: why does the loader reach the flag-2 block with no loaded BIN when kserv is on?

**Variant D, the missing-entry guard (`variants/kserv-D-missing-entry.dll`, sha256 `9cdd7ff302413d3ee21789e3dfcbd970b8520a957baa704261a0156152c7f95d`).**
- Mechanism (observed):
  - Loaders of type 1/3 (flags 0x47) always set the entry index to 2 (`011E0EC6`/`011E0F1A`).
  - There is no "BIN ready" state after decompression. `011356C0` ignores a failed decompress, so the BIN can be NULL.
  - The flag-2 block reads entry 2 without a bounds check.
  - The trigger with kserv on (why such models get the 1-entry BIN 0x0C000082, or an undecodable dt0f 0xCB) is still unknown.
- Hook at `011E1013`, code[7] with 3 NOPs. Thunk at `1000CD13`:
  - **Entry present:** `011E1046`, as the original.
  - **Entry missing:** if `[model+50h]`=0, it first runs the game's own blank-texture block `011E0FD2`. Then it continues at `011E11C2`, the game's own "no flag 2" path.
- Verified offline: `python -I test_variant_d.py ORIG D pes2013-image.bin <captures>` gives fails=0.
  - It replays captures 1, 2, 3, A and C. Every capture that faults in the original takes the safe path in D.
  - With count 3, D's path is identical to the original.
- Console test, 2026-11-26: **REJECTED.**
  - It crashed again at `011CA197`, but this time the fault address was 0x40CD7097 (jkdiag avlog), not NULL. jkdiag v2 only dumps null-page faults, so there is no full capture.
  - So the entry-2 pointer existed and pointed at garbage: the BIN content itself was invalid.

**Pattern after five failed fixes (Inferred, to be proven next):** the loader is handed the wrong data.
- Capture A: a kit model gets dt0c bin 130, a genuine 1-entry file.
- Capture C: dt0f 0xCB is a buffer with no valid header.
- Capture D: entry 2 holds a garbage pointer.
- One explanation fits all three, plus "kserv off = no crash": with kserv's callbacks registered, afsio's read redirection (event map without a lock, insert never overwrites, slot table replaced) serves some reads from the wrong file or offset when reads overlap during fast scrolling.
- Owner bisect, 2026-11-26, original kserv:
  - no data pack + no EDIT.bin: no crash;
  - data pack 6 alone: no crash;
  - data pack 6 + EDIT.bin: crash.
  - The EDIT.bin needs data pack 6 (with 1.00 it does not load).
  - So the trigger is the option file together with kserv.
  - The 2026-11-25 result "crashes without EDIT.bin" conflicts with this. A stray copy is suspected.
- Variant B with data pack 6 + EDIT.bin: **REJECTED**. Same null crash, same key 0x0C000082 (1-entry BIN) in capture B.
- Next: RE of EDIT.bin team kit fields vs kserv 13.4 slot ranges (kserv predates data pack 6).

**Root cause found (RE 2026-11-26, static): FACES, not kits.**
- The crashing type-1 model is a player face. Its key is built in fserv's hook at `011E0343` (call 79303BD3, fserv `C_GET_FACE_BIN`): `0x0C000082 + faceId`, or with a data pack `0x0EFFF8CB + id` for 0x7D1..0x8D0.
- Key 0x0C000082 = face id 0, dt0c bin 130, a 1-entry placeholder. Real faces have 3 entries.
- Key 0x0F0000CB = face id 2048, dt0f bin 203, empty in DP6.
- The face loader reads entry 2 without a bounds check.
- The owner's EDIT.bin (decrypted with the game's own LCG + CRC16 + CRC32) has 21 special-face players with face 0, 2048 or 2049 (Tomiyasu, Douglas Luiz, Tsimikas, Pedro Neto…). The flags were left by an earlier faceserver session.
- Fast scrolling only matters because it loads many squads.
- The hypothesis "kserv predates DP6" is refuted: the ranges line up.
- **Fix E: `variants/fserv-E-faceguard.dll`** (fserv input `24915aa2…`, output sha256 `b81aef015fd6a963e4f2ba796a289b365af6f514f9c59c9ea454ca2e2ee0d8e5`).
  - `fservGetFaceBin` returns the default face 0x3B4 for face 0 and for 0x7D3..0x8D0. Everything else is unchanged.
  - `test_fserv_e.py`: fails=0 over face ids 0..2999; the original fails as expected.
- Open: why kserv off seemed not to crash (2026-11-25, EDIT.bin state uncertain).
- Separate kit issue: 17 teams carry saved kserv x-slots and 25 slot values are shared. These can cause wrong kits, not this crash.
- Console test E, 2026-11-26, with original kserv + data pack 6 + EDIT.bin: **PASS**.
  - 6 sessions of fast Premier League scrolling plus a full Arsenal match: no crash.
  - jkdiag recorded no access violation at all (no `jkdiag/` dir created).
  - **Shipped fix for W7 = fserv-E.**
- Downstream guards cannot fix wrong data. Next is a diagnostic: log key + raw-buffer head at `011356C0` and compare with the real file on disk.

**Rejected, 2026-11-25: call-site guard** `efee54cc…` (hook at `011E105A`).
It moved the crash to `011E1743`, the `[model+50h]` use (capture 3). The
section below describes it and is kept as a record.

---

# kserv.dll W7 call-site guard (REJECTED: moved the crash)

| | |
|---|---|
| Applies to | the player's kserv.dll 13.4.0.0, sha256 `06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123` |
| Result | sha256 `efee54cc13d1ae09a514bc0588c9f0beeb32e39bcb544d2925ef969e8285f44e` (no longer shipped) |

**What it guards (observed in captures 1 and 2):**
- The game's kit-texture loader at `011E0E70` takes entry index 2 of a loaded
  BIN that has only 1 entry. The check at `011E1013 cmp edi,ebx` fails, so
  `xor eax,eax` sets eax to 0.
- `011E105A push 3; push eax; call 011CA190` then calls the function, which
  reads `[NULL+7]` without a NULL check (`011CA193/011CA197`).
- The same function does bounds-check the next block (`011E15BA cmp edi,ebx;
  jge`). Only the flag-2 block is missing the check.

**Patch:**

| File offset | Original | New | Effect |
|---|---|---|---|
| 0x300C8 | `00000000` | `5A101E01` | `code[C_READ_UNIFORM_PICK]` for PES 2013 v1.04 becomes 0x011E105A. The value was 0 before, so kserv skipped that hook. |
| 0x7B8D (VA 1000878D) | `6A 01` | `6A 03` | numNops for that hook goes from 1 to 3, so the whole 8-byte call is replaced. |
| 0xC113–0xC13A (VA 1000CD13) | dead uniform-pick callpoint body (40 bytes) | 33-byte thunk + `CC`×7 | see below |

The thunk:

```
test eax,eax; jz skip
push eax; mov dword [esp+4],3; push 011E1062h; push 011CA190h; ret   ; = the original call
skip: add esp,4; push 011E11C2h; ret                                   ; = the game's "no flag 2" path
```

At init, kserv's own `HookCallPoint` writes `E8 rel32 + 90 90 90` over
`011E105A..011E1061`.

**Why it is safe:**
- Observed, `test_w7guard.py`: everything else is byte-identical. The other
  address rows, imports, exports and all 3432 relocations are unchanged, and
  no relocation touches the patched bytes.
- Observed, harness on host Wine with kserv relocated:
  - Non-NULL eax: the call to 0x011CA190 gets the same arguments (eax, 3) and
    returns to 0x011E1062 with the same 8 argument bytes on the stack.
  - NULL eax: 0x011CA190 is not called, and execution continues at 0x011E11C2
    with a balanced stack.
- Observed, full `.text` scan of the decrypted image: no branch, call or
  pointer targets `011E105B..011E1061`.
- Observed: at `011E11C2` the registers the game uses (edi = index, ebx =
  count) are the same as on its own `je 011E11C2` path (`011E100D`), and the
  next index use is bounds-checked (`011E15BA`).
- Observed: the model's methods null-check `[model+50h]` at `011DFF7E`,
  `011E0682` and `011E0948`.
- Inferred, not verified: `011E0D2C` uses `[ebx+50h]` without a check, and
  its object type is unverified. If that object is this model, a later crash
  may appear there.
- The non-NULL path is identical to the original, so kits that load normally
  are unaffected (inferred from the harness result and the unchanged code).

**Root cause, still open:** why the game pairs a kit model
(`[model+78h]` = 1, key list = {0x0C000082}) with dt0c bin 130 (genuinely 1
entry), and how kserv makes this happen. This guard removes the crash at its
null dereference; it does not explain the cause.

---

# W7 analysis status (jkdiag capture 1, 2026-11-25): superseded by the guard above

Observed in the decrypted image (VAs in pes2013.exe v1.04):

1. The crash: `011CA190 push ecx/esi/edi; 011CA193 mov edi,[esp+10h]` (arg1)
   `; 011CA197 movzx ecx,byte [edi+7]`, with arg1 = 0.
2. The caller is `011E0E70`, vtable slot 1 of class `01527EF4`. This is a
   loader that processes a loaded BIN. Its flags `[this+58h]` = 0x47, set by
   the constructor at `011E1D45`.
3. How `011E0E70` gets there:
   - `eax=[this+34h]` is the BIN buffer. count = `[bin+0]`, table =
     `bin+[bin+4]`, entries are 12 bytes.
   - With flag 1 and `[model+78h]` != 0, it sets the entry index to 2
     (`011E0F1A`).
   - With flag 2: `011E1013 cmp edi,ebx` (index 2 against count 1). `jb` is
     not taken, so `011E1017 xor eax,eax`. Then `011E105A push 3; push eax;
     call 011CA190`, which receives NULL.
4. The capture values: this = 1D840130, `[this+2Ch]` = key 0x0C000082
   (dt0c bin 130), `[this+34h]` = 1DCF8530, `[this+38h]` = 0x55620,
   count = 1 (EBX), index = 2.
5. dt0c.img bin 130 on the console really has 1 entry (0x55600-byte texture,
   unpacked 0x55620). It is a static resource: the game creates it at
   `011E2D65` (`push 0C000082h`). So the data is correct; the loader type is
   wrong for this bin.
6. The kserv num-slot hooks at 011D6A3E, 011D74F0 and 012BCAC0 are patched
   correctly (call kserv+C703/+C733/+C763). For every non-x-slot index they
   return the game's own value 0x100. They are neutral unless a team is
   relinked to slot 0x1000.

Unknown: which kserv change makes the game build a type-0x47 (kit-model)
loader on bin 130 with `[model+78h]` != 0. The next capture uses jkdiag v2
heap dumps (`this`, `this+5Ch` model, `this+28h` resource).

---

# kserv.dll patch: InitSlotMap runs on the calling thread (W7 candidate)

Status: **REJECTED as a W7 fix.** Its DLL (sha256 3496a2b1…, rebuilt by
`make_patch.py`) is no longer in this folder; `kserv.dll` is now the W7
guard. Owner test on rc9: it still crashes on
fast scroll, and the original kserv with an empty GDB map also crashes. Keep
this file only as a record. The analysis continues with `engine/jkdiag`.
Original status: candidate, not yet tested in the game. It removes a real race in
kserv. The evidence does **not** yet prove that this race causes the W7
fast-scroll crash (see "Root cause status"). It still has to be tested on
the PS4.

| | |
|---|---|
| Applies to | player's `kitserver/kserv.dll`, "KSERV Module 13.4.0.0", 294400 bytes, sha256 `06a6fea90a0c4eedba6e2b3d205052f2f273cb9a7427fd01b66416efb4a31123` |
| Result | `kserv.dll`, 294400 bytes, sha256 `3496a2b1c04d33b4fa8e37e5a2cca7b9687b727fa09d8f980c92d81b00b10e15` |
| Changed | 22-byte window at file offset `0x844D`–`0x8462` (VA `0x1000904D`–`0x10009062`, image base `0x10000000`, `.text`). 12 bytes differ. Nothing else changes: headers, PE checksum (0, as in the original), exports, imports and `.reloc` are byte-identical. |
| Reproduce | `python3 -I make_patch.py kserv.orig.dll kserv.dll` (checks the input sha256 and the original bytes before it patches) |
| Verify | `python3 -I test_kserv_patch.py kserv.orig.dll kserv.dll` (needs GNU objdump with pei-i386) |

## What changed

`kservAfterReadNames` (exported, RVA `0x9020`) runs from kload's after-read-names
hook. In 13.4 it calls `InitSlotMapInThread()`, which starts
`CreateThread(InitSlotMap, teamKitInfo)` and returns at once. The leaked thread
then clears and rebuilds `_slotMaps` / `_reverseSlotMaps`, rewrites the game's
`TEAM_KIT_INFO` (slot relink to x-slot `0x1000`, kit attributes) and swaps the
game's euro-kit list (`begin`/`end` written separately, then `HeapFree` of the
previous list). The game thread keeps running while this happens. Only
`InitSlotMap` itself takes `_cs`. The readers (`GetTeamIndexesBySlot`,
`CreatePipeFor*Bin`, `_lastKit`) and the game do not.

The patch makes the after-read-names path call `InitSlotMap(teamKitInfo)`
directly, the way `kservReadEditData` already does at `0x1000A905`
(`push esi; call 0x10009380`). The rebuild then finishes before the hook
returns to the game. `InitSlotMap` itself is unchanged and keeps `_cs`.
`InitSlotMap` is `DWORD WINAPI` (`ret 4` at `0x1000983E`), so the stack is
balanced.

| VA | original | new |
|---|---|---|
| 1000904D | `8D 4D FC` lea ecx,[ebp-4] | `50` push eax (teamKitInfo) |
| 10009050 | `51` push ecx; `6A 00` push 0; `50` push eax | `66 0F 1F 44 00 00` 6-byte nop (1000904E) |
| 10009054 | `68 80 93 00 10` push 0x10009380 (reloc @55) | same bytes, kept so the reloc stays valid |
| 10009059 | `6A 00` push 0; `6A 00` push 0 | `59` pop ecx; `FF D1` call ecx (InitSlotMap); `66 90` nop |
| 1000905D | `FF 15 3C 60 02 10` call [CreateThread] (reloc @5F) | `B9 3C 60 02 10` mov ecx,0x1002603C at 1000905E (harmless filler that keeps the reloc @5F on an imm32) |
| 10009063 | `mov esp,ebp; pop ebp; ret` | unchanged |

Source-level equivalent (kitserver `src/kserv/kserv.cpp`, `kservAfterReadNames`):

```diff
     // initialize kit slots
-    InitSlotMapInThread();
+    InitSlotMap((TEAM_KIT_INFO*)(*(DWORD*)dta[PLAYERS_DATA]
+            + dta[TEAM_KIT_INFO_OFFSET]));
```

## Offline verification (done)

- `test_kserv_patch.py` was RED on the original (5 failures: CreateThread still
  called, no direct call, ...) and is GREEN on the patched DLL (0 failures).
  It checks: only the patch window differs; there is no CreateThread call; a
  single stdcall argument (eax = teamKitInfo); the code falls through to the
  original epilogue on an instruction boundary; the `.reloc` table is identical
  (3432 entries) and the two relocs in the window (`0x10009055`, `0x1000905F`)
  sit on imm32 operands of the new instructions; exports and imports are
  identical.
- Wine smoke test (`~/orbis-win-host-bootstrap/wine-host`, 32-bit loader built
  with its `i686-w64-mingw32-gcc`, throwaway prefix in /tmp): 0x10000000 was
  reserved first to force relocation, then each DLL was loaded with
  `LoadLibraryExA(..., DONT_RESOLVE_DLL_REFERENCES)`. Both load at 0x7B330000.
  All 6 exports resolve at the same RVAs (ApplyKitAttributes 0xa450,
  hookVtableFunction 0x8fe0, kservAfterReadNames 0x9020,
  kservCheckKitReloadFlag 0xcd70, kservReadNumSlots 0xc790,
  kservReadUniformPick 0xcd40). The patched operands relocate correctly
  (push → base+0x9380, mov → base+0x2603C). `kservReadNumSlots(0x1000)` runs
  and returns 0x1700.
- Not tested: DllMain/module init (it needs kload, afsio and the running game)
  and the game itself.

## Root cause status (read before shipping)

Observed: the crash is `movzx ecx, byte [edi+7]` with edi = 0 at game VA
`0x011CA197` (pc-bytes `0F B6 4F 07 83 C1 F8`). The return address on the stack
is `0x011E1062`, close to kserv's num-slot hooks (0x11D6A3E, 0x11D74F0). It
happens on thread 0x5c at t≈68 s (s0/s1/s2/s7 runs). `pes2013.exe` `.text` is
encrypted on disk (SecuROM), so the game function cannot be read statically.

Inferred: the threaded InitSlotMap runs once, at boot (after-read-names comes
right after the EDIT data load), and finishes in milliseconds. In the EDIT.bin
run, its writes to TEAM_KIT_INFO are no-ops: "storing original kit attributes"
is not logged the second time. A crash tens of seconds later, during
team-select scrolling, therefore does not obviously come from this race. The
only lasting effects are the euro-kit list swap/free, and (without EDIT.bin)
the slot-0x1000 relink being published by a second thread. This patch closes
both windows, but **it may not fix W7.**

Player's 13.4 build compared with upstream source: the address tables match
v1.04, and the build installs the same 3 num-slot hooks. Its kload
HookCallPoint signature is older (3, n, 0). The upload-uniform and
kit-reload-flag hooks are not installed (code[7] = 0, code[8] = 0xD6180C is
unused).

Discriminating tests for the lead (one PS4 run each):
1. Candidate DLL, same scroll. If there is no crash in 3 tries, adopt it.
2. If it still crashes: original DLL with `GDB/uni/map.txt` emptied (0 teams).
   - Crash → the num-slot hooks are the cause.
   - No crash → the x-slot relink / pipe-served kit BIN path is the cause.
3. Extend the PW_EXCEPTION32 dump to about 128 bytes before and after the eip
   and at `0x011E1062`, so the game function can be disassembled.

## Licence (corrected 2026-11-26)

Kitserver 13 is NOT GPL. The player's own copy (`kitserver/docs/license.txt`,
"Copyright (c) 2003-2013, Juce, Robbie, Jenkey1002") and the
pes-modding/kitserver-tools-2010-2013 copy both carry a BSD-style licence. It
permits redistribution in binary form **without modification** only, and
https://github.com/jesusminano/kitserver carries no licence file. The earlier
"GPL-3.0" text here was wrong.

So JKPro never distributes a modified kserv.dll or fserv.dll; all `*.dll` stay
gitignored. The repo holds only our own patch scripts and byte tables. The
shipped W7 fix (`launcher/src/jkpro_fservfix.c`) applies our 28 bytes in place
to the player's own fserv.dll, only when the player turns it on.
